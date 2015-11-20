// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "Sequencer.h"
#include "SSequencerTreeView.h"
#include "SSequencerTrackLane.h"
#include "SSequencerTrackArea.h"

static FName TrackAreaName = "TrackArea";

namespace Utils
{
	enum class ESearchState { Before, After, Found };

	template<typename T, typename F>
	const T* BinarySearch(const TArray<T>& InContainer, const F& InPredicate)
	{
		int32 Min = 0;
		int32 Max = InContainer.Num();

		ESearchState State = ESearchState::Before;

		for ( ; Min != Max ; )
		{
			int32 SearchIndex = Min + (Max - Min) / 2;

			auto& Item = InContainer[SearchIndex];
			State = InPredicate(Item);
			switch (State)
			{
				case ESearchState::Before:	Max = SearchIndex; break;
				case ESearchState::After:	Min = SearchIndex + 1; break;
				case ESearchState::Found: 	return &Item;
			}
		}

		return nullptr;
	}
}

SSequencerTreeViewRow::~SSequencerTreeViewRow()
{
	auto TreeView = StaticCastSharedPtr<SSequencerTreeView>(OwnerTablePtr.Pin());
	auto PinnedNode = Node.Pin();
	if (TreeView.IsValid() && PinnedNode.IsValid())
	{
		TreeView->OnChildRowRemoved(PinnedNode.ToSharedRef());
	}
}

/** Construct function for this widget */
void SSequencerTreeViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const FDisplayNodeRef& InNode)
{
	Node = InNode;
	OnGenerateWidgetForColumn = InArgs._OnGenerateWidgetForColumn;

	SMultiColumnTableRow::Construct(SMultiColumnTableRow::FArguments(), OwnerTableView);
}

TSharedRef<SWidget> SSequencerTreeViewRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	auto PinnedNode = Node.Pin();
	if (PinnedNode.IsValid())
	{
		return OnGenerateWidgetForColumn.Execute(PinnedNode.ToSharedRef(), ColumnId, SharedThis(this));
	}

	return SNullWidget::NullWidget;
}

TSharedPtr<FSequencerDisplayNode> SSequencerTreeViewRow::GetDisplayNode() const
{
	return Node.Pin();
}

void SSequencerTreeViewRow::AddTrackAreaReference(const TSharedPtr<SSequencerTrackLane>& Lane)
{
	TrackLaneReference = Lane;
}

void SSequencerTreeViewRow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	StaticCastSharedPtr<SSequencerTreeView>(OwnerTablePtr.Pin())->ReportChildRowGeometry(Node.Pin().ToSharedRef(), AllottedGeometry);
}

void SSequencerTreeView::Construct(const FArguments& InArgs, const TSharedRef<FSequencerNodeTree>& InNodeTree, const TSharedRef<SSequencerTrackArea>& InTrackArea)
{
	SequencerNodeTree = InNodeTree;
	TrackArea = InTrackArea;

	// We 'leak' these delegates (they'll get cleaned up automatically when the invocation list changes)
	// It's not safe to attempt their removal in ~SSequencerTreeView because SequencerNodeTree->GetSequencer() may not be valid
	FSequencer& Sequencer = InNodeTree->GetSequencer();
	Sequencer.GetSettings()->GetOnShowCurveEditorChanged().AddSP(this, &SSequencerTreeView::UpdateTrackArea);
	Sequencer.GetSelection().GetOnOutlinerNodeSelectionChanged().AddSP(this, &SSequencerTreeView::OnSequencerSelectionChangedExternally);

	HeaderRow = SNew(SHeaderRow).Visibility(EVisibility::Collapsed);

	SetupColumns(InArgs);

	STreeView::Construct
	(
		STreeView::FArguments()
		.TreeItemsSource(&RootNodes)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SSequencerTreeView::OnGenerateRow)
		.OnGetChildren(this, &SSequencerTreeView::OnGetChildren)
		.HeaderRow(HeaderRow)
		.ExternalScrollbar(InArgs._ExternalScrollbar)
		.OnExpansionChanged(this, &SSequencerTreeView::OnExpansionChanged)
		.AllowOverscroll(EAllowOverscroll::No)
	);
}

void SSequencerTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	
	PhysicalNodes.Reset();
	CachedRowGeometry.GenerateValueArray(PhysicalNodes);

	PhysicalNodes.Sort([](const FCachedGeometry& A, const FCachedGeometry& B){
		return A.PhysicalTop < B.PhysicalTop;
	});
}

TOptional<SSequencerTreeView::FCachedGeometry> SSequencerTreeView::GetPhysicalGeometryForNode(const FDisplayNodeRef& InNode) const
{
	if (const FCachedGeometry* FoundGeometry = CachedRowGeometry.Find(InNode))
	{
		return *FoundGeometry;
	}

	return TOptional<FCachedGeometry>();
}

void SSequencerTreeView::ReportChildRowGeometry(const FDisplayNodeRef& InNode, const FGeometry& InGeometry)
{
	float ChildOffset = TransformPoint(
		Concatenate(
			InGeometry.GetAccumulatedLayoutTransform(),
			CachedGeometry.GetAccumulatedLayoutTransform().Inverse()
		),
		FVector2D(0,0)
	).Y;

	CachedRowGeometry.Add(InNode, FCachedGeometry(InNode, ChildOffset, InGeometry.Size.Y));
}

void SSequencerTreeView::OnChildRowRemoved(const FDisplayNodeRef& InNode)
{
	CachedRowGeometry.Remove(InNode);
}

TSharedPtr<FSequencerDisplayNode> SSequencerTreeView::HitTestNode(float InPhysical) const
{
	auto* Found = Utils::BinarySearch<FCachedGeometry>(PhysicalNodes, [&](const FCachedGeometry& In){

		if (InPhysical < In.PhysicalTop)
		{
			return Utils::ESearchState::Before;
		}
		else if (InPhysical > In.PhysicalTop + In.PhysicalHeight)
		{
			return Utils::ESearchState::After;
		}

		return Utils::ESearchState::Found;

	});

	if (Found)
	{
		return Found->Node;
	}
	
	return nullptr;
}

float SSequencerTreeView::PhysicalToVirtual(float InPhysical) const
{
	int32 SearchIndex = PhysicalNodes.Num() / 2;

	auto* Found = Utils::BinarySearch<FCachedGeometry>(PhysicalNodes, [&](const FCachedGeometry& In){

		if (InPhysical < In.PhysicalTop)
		{
			return Utils::ESearchState::Before;
		}
		else if (InPhysical > In.PhysicalTop + In.PhysicalHeight)
		{
			return Utils::ESearchState::After;
		}

		return Utils::ESearchState::Found;

	});


	if (Found)
	{
		const float FractionalHeight = (InPhysical - Found->PhysicalTop) / Found->PhysicalHeight;
		return Found->Node->GetVirtualTop() + (Found->Node->GetVirtualBottom() - Found->Node->GetVirtualTop()) * FractionalHeight;
	}

	if (PhysicalNodes.Num())
	{
		auto& Last = PhysicalNodes.Last();
		return Last.Node->GetVirtualTop() + (InPhysical - Last.PhysicalTop);
	}

	return InPhysical;
}

float SSequencerTreeView::VirtualToPhysical(float InVirtual) const
{
	auto* Found = Utils::BinarySearch(PhysicalNodes, [&](const FCachedGeometry& In){

		if (InVirtual < In.Node->GetVirtualTop())
		{
			return Utils::ESearchState::Before;
		}
		else if (InVirtual > In.Node->GetVirtualBottom())
		{
			return Utils::ESearchState::After;
		}

		return Utils::ESearchState::Found;

	});

	if (Found)
	{
		const float FractionalHeight = (InVirtual - Found->Node->GetVirtualTop()) / (Found->Node->GetVirtualBottom() - Found->Node->GetVirtualTop());
		return Found->PhysicalTop + Found->PhysicalHeight * FractionalHeight;
	}
	
	if (PhysicalNodes.Num())
	{
		auto Last = PhysicalNodes.Last();
		return Last.PhysicalTop + (InVirtual - Last.Node->GetVirtualTop());
	}

	return InVirtual;
}

void SSequencerTreeView::SetupColumns(const FArguments& InArgs)
{
	FSequencer& Sequencer = SequencerNodeTree->GetSequencer();

	// Define a column for the Outliner
	auto GenerateOutliner = [=](const FDisplayNodeRef& InNode, const TSharedRef<SSequencerTreeViewRow>& InRow)
	{
		return InNode->GenerateContainerWidgetForOutliner(InRow);
	};

	Columns.Add("Outliner", FSequencerTreeViewColumn(GenerateOutliner, 1.f));

	// Now populate the header row with the columns
	for (auto& Pair : Columns)
	{
		if (Pair.Key != TrackAreaName || !Sequencer.GetSettings()->GetShowCurveEditor())
		{
			HeaderRow->AddColumn(
				SHeaderRow::Column(Pair.Key)
				.FillWidth(Pair.Value.Width)
			);
		}
	}
}

void SSequencerTreeView::UpdateTrackArea()
{
	FSequencer& Sequencer = SequencerNodeTree->GetSequencer();

	// Add or remove the column
	if (Sequencer.GetSettings()->GetShowCurveEditor())
	{
		HeaderRow->RemoveColumn(TrackAreaName);
	}
	else if (const auto* Column = Columns.Find(TrackAreaName))
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(TrackAreaName)
			.FillWidth(Column->Width)
		);
	}
}

void SSequencerTreeView::OnSequencerSelectionChangedExternally()
{
	Private_ClearSelection();

	FSequencer& Sequencer = SequencerNodeTree->GetSequencer();
	for (auto& Node : Sequencer.GetSelection().GetSelectedOutlinerNodes())
	{
		Private_SetItemSelection(Node, true, false);
	}

	Private_SignalSelectionChanged(ESelectInfo::Direct);
}

void SSequencerTreeView::Private_SignalSelectionChanged(ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		FSequencer& Sequencer = SequencerNodeTree->GetSequencer();
		FSequencerSelection& Selection = Sequencer.GetSelection();
		Selection.EmptySelectedOutlinerNodes();
		for (auto& Item : GetSelectedItems())
		{
			Selection.AddToSelection(Item);
		}
	}

	STreeView::Private_SignalSelectionChanged(SelectInfo);
}

void SSequencerTreeView::Refresh()
{
	RootNodes.Reset(SequencerNodeTree->GetRootNodes().Num());

	for (const auto& RootNode : SequencerNodeTree->GetRootNodes())
	{
		if (RootNode->IsExpanded())
		{
			SetItemExpansion(RootNode, true);
		}

		if (!RootNode->IsHidden())
		{
			RootNodes.Add(RootNode);
		}
	}

	RequestTreeRefresh();
}

void SSequencerTreeView::ScrollByDelta(float DeltaInSlateUnits)
{
	ScrollBy( CachedGeometry, DeltaInSlateUnits, EAllowOverscroll::No );
}

template<typename T>
bool ShouldExpand(const T& InContainer, ETreeRecursion Recursion)
{
	bool bAllExpanded = true;
	for (auto& Item : InContainer)
	{
		bAllExpanded &= Item->IsExpanded();
		if (Recursion == ETreeRecursion::Recursive)
		{
			Item->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& InNode){
				bAllExpanded &= InNode.IsExpanded();
				return true;
			});
		}
	}
	return !bAllExpanded;
}

void SSequencerTreeView::ToggleExpandCollapseNodes(ETreeRecursion Recursion)
{
	FSequencer& Sequencer = SequencerNodeTree->GetSequencer();

	const TSet< FDisplayNodeRef >& SelectedNodes = Sequencer.GetSelection().GetSelectedOutlinerNodes();

	bool bExpand = false;
	if (SelectedNodes.Num() != 0)
	{
		bExpand = ShouldExpand(SelectedNodes, Recursion);
	}
	else
	{
		bExpand = ShouldExpand(SequencerNodeTree->GetRootNodes(), Recursion);
	}

	ExpandOrCollapseNodes(Recursion, bExpand);
}

void SSequencerTreeView::ExpandNodes(ETreeRecursion Recursion)
{
	const bool bExpand = true;
	ExpandOrCollapseNodes(Recursion, bExpand);
}

void SSequencerTreeView::CollapseNodes(ETreeRecursion Recursion)
{
	const bool bExpand = false;
	ExpandOrCollapseNodes(Recursion, bExpand);
}

void SSequencerTreeView::ExpandOrCollapseNodes(ETreeRecursion Recursion, bool bExpand)
{
	FSequencer& Sequencer = SequencerNodeTree->GetSequencer();

	const TSet< FDisplayNodeRef >& SelectedNodes = Sequencer.GetSelection().GetSelectedOutlinerNodes();

	if (SelectedNodes.Num() != 0)
	{
		for (auto& Item : SelectedNodes)
		{
			ExpandCollapseNode(Item, bExpand, Recursion);
		}
	}
	else
	{
		for (auto& Item : SequencerNodeTree->GetRootNodes())
		{
			ExpandCollapseNode(Item, bExpand, Recursion);
		}
	}
}

void SSequencerTreeView::ExpandCollapseNode(const FDisplayNodeRef& InNode, bool bExpansionState, ETreeRecursion Recursion)
{
	SetItemExpansion(InNode, bExpansionState);

	if (Recursion == ETreeRecursion::Recursive)
	{
		for (auto& Node : InNode->GetChildNodes())
		{
			ExpandCollapseNode(Node, bExpansionState, ETreeRecursion::Recursive);
		}
	}
}

void SSequencerTreeView::OnExpansionChanged(FDisplayNodeRef InItem, bool bIsExpanded)
{
	InItem->SetExpansionState(bIsExpanded);
	
	// Expand any children that are also expanded
	for (auto& Child : InItem->GetChildNodes())
	{
		if (Child->IsExpanded())
		{
			SetItemExpansion(Child, true);
		}
	}
}

void SSequencerTreeView::OnGetChildren(FDisplayNodeRef InParent, TArray<FDisplayNodeRef>& OutChildren) const
{
	for (const auto& Node : InParent->GetChildNodes())
	{
		if (!Node->IsHidden())
		{
			OutChildren.Add(Node);
		}
	}
}

TSharedRef<ITableRow> SSequencerTreeView::OnGenerateRow(FDisplayNodeRef InDisplayNode, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SSequencerTreeViewRow> Row =
		SNew(SSequencerTreeViewRow, OwnerTable, InDisplayNode)
		.OnGenerateWidgetForColumn(this, &SSequencerTreeView::GenerateWidgetForColumn);

	// Ensure the track area is kept up to date with the virtualized scroll of the tree view
	TSharedPtr<FSequencerDisplayNode> SectionAuthority = InDisplayNode->GetSectionAreaAuthority();
	if (SectionAuthority.IsValid())
	{
		TSharedPtr<SSequencerTrackLane> TrackLane = TrackArea->FindTrackSlot(SectionAuthority.ToSharedRef());

		if (!TrackLane.IsValid())
		{
			// Add a track slot for the row
			TAttribute<TRange<float>> ViewRange = FAnimatedRange::WrapAttribute( TAttribute<FAnimatedRange>::Create(TAttribute<FAnimatedRange>::FGetter::CreateSP(&SequencerNodeTree->GetSequencer(), &FSequencer::GetViewRange)) );	

			TrackLane = SNew(SSequencerTrackLane, SectionAuthority.ToSharedRef(), SharedThis(this))
			[
				SectionAuthority->GenerateWidgetForSectionArea(ViewRange)
			];

			TrackArea->AddTrackSlot(SectionAuthority.ToSharedRef(), TrackLane);
		}

		if (ensure(TrackLane.IsValid()))
		{
			Row->AddTrackAreaReference(TrackLane);
		}
	}

	return Row;
}

TSharedRef<SWidget> SSequencerTreeView::GenerateWidgetForColumn(const FDisplayNodeRef& InNode, const FName& ColumnId, const TSharedRef<SSequencerTreeViewRow>& Row) const
{
	const auto* Definition = Columns.Find(ColumnId);

	if (ensureMsgf(Definition, TEXT("Invalid column name specified")))
	{
		return Definition->Generator(InNode, Row);
	}

	return SNullWidget::NullWidget;
}