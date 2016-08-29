// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerSectionLayoutBuilder.h"
#include "ISequencerSection.h"
#include "Sequencer.h"
#include "MovieScene.h"
#include "SSequencer.h"
#include "SSequencerSectionAreaView.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "MovieSceneTrackEditor.h"
#include "CommonMovieSceneTools.h"
#include "IKeyArea.h"
#include "GroupedKeyArea.h"
#include "ObjectEditorUtils.h"
#include "GenericCommands.h"


#define LOCTEXT_NAMESPACE "SequencerDisplayNode"


/** When 0, regeneration of dynamic key groups is enabled, when non-zero, such behaviour is disabled */
FThreadSafeCounter KeyGroupRegenerationLock;

namespace SequencerNodeConstants
{
	extern const float CommonPadding;
	const float CommonPadding = 4.f;
}


class SSequencerObjectTrack
	: public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SSequencerObjectTrack) {}
		/** The view range of the section area */
		SLATE_ATTRIBUTE( TRange<float>, ViewRange )
	SLATE_END_ARGS()

	/** SLeafWidget Interface */
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	
	void Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> InRootNode )
	{
		RootNode = InRootNode;
		
		ViewRange = InArgs._ViewRange;

		check(RootNode->GetType() == ESequencerNode::Object);
	}

private:

	/** Collects all key times from the root node */
	void CollectAllKeyTimes(TArray<float>& OutKeyTimes) const;

	/** Adds a key time uniquely to an array of key times */
	void AddKeyTime(const float& NewTime, TArray<float>& OutKeyTimes) const;

private:

	/** Root node of this track view panel */
	TSharedPtr<FSequencerDisplayNode> RootNode;

	/** The current view range */
	TAttribute< TRange<float> > ViewRange;
};


int32 SSequencerObjectTrack::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (RootNode->GetSequencer().GetSettings()->GetShowCombinedKeyframes())
	{
		TArray<float> OutKeyTimes;
		CollectAllKeyTimes(OutKeyTimes);
	
		FTimeToPixel TimeToPixelConverter(AllottedGeometry, ViewRange.Get());

		for (int32 i = 0; i < OutKeyTimes.Num(); ++i)
		{
			float KeyPosition = TimeToPixelConverter.TimeToPixel(OutKeyTimes[i]);
			static const FVector2D KeyMarkSize = FVector2D(3.f, 21.f);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry(FVector2D(KeyPosition - FMath::CeilToFloat(KeyMarkSize.X/2.f), FMath::CeilToFloat(AllottedGeometry.Size.Y/2.f - KeyMarkSize.Y/2.f)), KeyMarkSize),
				FEditorStyle::GetBrush("Sequencer.KeyMark"),
				MyClippingRect,
				ESlateDrawEffect::None,
				FLinearColor(1.f, 1.f, 1.f, 1.f)
			);
		}
		return LayerId+1;
	}

	return LayerId;
}


FVector2D SSequencerObjectTrack::ComputeDesiredSize( float ) const
{
	// Note: X Size is not used
	return FVector2D( 100.0f, RootNode->GetNodeHeight() );
}


void SSequencerObjectTrack::CollectAllKeyTimes(TArray<float>& OutKeyTimes) const
{
	TArray<TSharedRef<FSequencerSectionKeyAreaNode>> OutNodes;
	RootNode->GetChildKeyAreaNodesRecursively(OutNodes);

for (int32 i = 0; i < OutNodes.Num(); ++i)
	{
		TArray< TSharedRef<IKeyArea> > KeyAreas = OutNodes[i]->GetAllKeyAreas();
		for (int32 j = 0; j < KeyAreas.Num(); ++j)
		{
			TArray<FKeyHandle> KeyHandles = KeyAreas[j]->GetUnsortedKeyHandles();
			for (int32 k = 0; k < KeyHandles.Num(); ++k)
			{
				AddKeyTime(KeyAreas[j]->GetKeyTime(KeyHandles[k]), OutKeyTimes);
			}
		}
	}
}


void SSequencerObjectTrack::AddKeyTime(const float& NewTime, TArray<float>& OutKeyTimes) const
{
	// @todo Sequencer It might be more efficient to add each key and do the pruning at the end
	for (float& KeyTime : OutKeyTimes)
	{
		if (FMath::IsNearlyEqual(KeyTime, NewTime))
		{
			return;
		}
	}

	OutKeyTimes.Add(NewTime);
}


FSequencerDisplayNode::FSequencerDisplayNode( FName InNodeName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
	: VirtualTop( 0.f )
	, VirtualBottom( 0.f )
	, ParentNode( InParentNode )
	, ParentTree( InParentTree )
	, NodeName( InNodeName )
	, bExpanded( false )
{
}


void FSequencerDisplayNode::Initialize(float InVirtualTop, float InVirtualBottom)
{
	bExpanded = ParentTree.GetSavedExpansionState( *this );

	VirtualTop = InVirtualTop;
	VirtualBottom = InVirtualBottom;
}


void FSequencerDisplayNode::AddObjectBindingNode(TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode)
{
	AddChildAndSetParent( ObjectBindingNode );
}


bool FSequencerDisplayNode::Traverse_ChildFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode)
{
	for (auto& Child : GetChildNodes())
	{
		if (!Child->Traverse_ChildFirst(InPredicate, true))
		{
			return false;
		}
	}

	return bIncludeThisNode ? InPredicate(*this) : true;
}


bool FSequencerDisplayNode::Traverse_ParentFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode)
{
	if (bIncludeThisNode && !InPredicate(*this))
	{
		return false;
	}

	for (auto& Child : GetChildNodes())
	{
		if (!Child->Traverse_ParentFirst(InPredicate, true))
		{
			return false;
		}
	}

	return true;
}


bool FSequencerDisplayNode::TraverseVisible_ChildFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode)
{
	// If the item is not expanded, its children ain't visible
	if (IsExpanded())
	{
		for (auto& Child : GetChildNodes())
		{
			if (!Child->IsHidden() && !Child->TraverseVisible_ChildFirst(InPredicate, true))
			{
				return false;
			}
		}
	}

	if (bIncludeThisNode && !IsHidden())
	{
		return InPredicate(*this);
	}

	// Continue iterating regardless of visibility
	return true;
}


bool FSequencerDisplayNode::TraverseVisible_ParentFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode)
{
	if (bIncludeThisNode && !IsHidden() && !InPredicate(*this))
	{
		return false;
	}

	// If the item is not expanded, its children ain't visible
	if (IsExpanded())
	{
		for (auto& Child : GetChildNodes())
		{
			if (!Child->IsHidden() && !Child->TraverseVisible_ParentFirst(InPredicate, true))
			{
				return false;
			}
		}
	}

	return true;
}

TSharedRef<FSequencerSectionCategoryNode> FSequencerDisplayNode::AddCategoryNode( FName CategoryName, const FText& DisplayLabel )
{
	TSharedPtr<FSequencerSectionCategoryNode> CategoryNode;

	// See if there is an already existing category node to use
	for (const TSharedRef<FSequencerDisplayNode>& Node : ChildNodes)
	{
		if ((Node->GetNodeName() == CategoryName) && (Node->GetType() == ESequencerNode::Category))
		{
			CategoryNode = StaticCastSharedRef<FSequencerSectionCategoryNode>(Node);
		}
	}

	if (!CategoryNode.IsValid())
	{
		// No existing category found, make a new one
		CategoryNode = MakeShareable(new FSequencerSectionCategoryNode(CategoryName, DisplayLabel, SharedThis(this), ParentTree));
		ChildNodes.Add(CategoryNode.ToSharedRef());
	}

	return CategoryNode.ToSharedRef();
}


TSharedRef<FSequencerTrackNode> FSequencerDisplayNode::AddSectionAreaNode(UMovieSceneTrack& AssociatedTrack, ISequencerTrackEditor& AssociatedEditor)
{
	TSharedPtr<FSequencerTrackNode> SectionNode;

	// see if there is an already existing section node to use
	for (const TSharedRef<FSequencerDisplayNode>& Node : ChildNodes)
	{
		if (Node->GetType() != ESequencerNode::Track)
		{
			continue;
		}

		SectionNode = StaticCastSharedRef<FSequencerTrackNode>(Node);

		if (SectionNode->GetTrack() != &AssociatedTrack)
		{
			SectionNode.Reset();
		}
		else
		{
			break;
		}
	}

	if (!SectionNode.IsValid())
	{
		// No existing node found make a new one
		SectionNode = MakeShareable( new FSequencerTrackNode( AssociatedTrack, AssociatedEditor, false, SharedThis(this), ParentTree ) );
		ChildNodes.Add( SectionNode.ToSharedRef() );
	}

	// The section node type has to match
	check(SectionNode->GetTrack() == &AssociatedTrack);

	return SectionNode.ToSharedRef();
}


void FSequencerDisplayNode::AddKeyAreaNode(FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea)
{
	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode;

	// See if there is an already existing key area node to use
	for (const TSharedRef<FSequencerDisplayNode>& Node : ChildNodes)
	{
		if ((Node->GetNodeName() == KeyAreaName) && (Node->GetType() == ESequencerNode::KeyArea))
		{
			KeyAreaNode = StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node);
		}
	}

	if (!KeyAreaNode.IsValid())
	{
		// No existing node found make a new one
		KeyAreaNode = MakeShareable( new FSequencerSectionKeyAreaNode( KeyAreaName, DisplayName, SharedThis( this ), ParentTree ) );
		ChildNodes.Add(KeyAreaNode.ToSharedRef());
	}

	KeyAreaNode->AddKeyArea(KeyArea);
}

FLinearColor FSequencerDisplayNode::GetDisplayNameColor() const
{
	return FLinearColor( 1.f, 1.f, 1.f, 1.f );
}

FText FSequencerDisplayNode::GetDisplayNameToolTipText() const
{
	return FText();
}

TSharedRef<SWidget> FSequencerDisplayNode::GenerateContainerWidgetForOutliner(const TSharedRef<SSequencerTreeViewRow>& InRow)
{
	auto NewWidget = SNew(SAnimationOutlinerTreeNode, SharedThis(this), InRow)
	.IconBrush(this, &FSequencerDisplayNode::GetIconBrush)
	.IconColor(this, &FSequencerDisplayNode::GetIconColor)
	.IconOverlayBrush(this, &FSequencerDisplayNode::GetIconOverlayBrush)
	.IconToolTipText(this, &FSequencerDisplayNode::GetIconToolTipText)
	.CustomContent()
	[
		GetCustomOutlinerContent()
	];

	return NewWidget;
}

TSharedRef<SWidget> FSequencerDisplayNode::GetCustomOutlinerContent()
{
	return SNew(SSpacer);
}

const FSlateBrush* FSequencerDisplayNode::GetIconBrush() const
{
	return nullptr;
}

const FSlateBrush* FSequencerDisplayNode::GetIconOverlayBrush() const
{
	return nullptr;
}

FSlateColor FSequencerDisplayNode::GetIconColor() const
{
	return FSlateColor( FLinearColor::White );
}

FText FSequencerDisplayNode::GetIconToolTipText() const
{
	return FText();
}

TSharedRef<SWidget> FSequencerDisplayNode::GenerateWidgetForSectionArea(const TAttribute< TRange<float> >& ViewRange)
{
	if (GetType() == ESequencerNode::Track)
	{
		return SNew(SSequencerSectionAreaView, SharedThis(this))
			.ViewRange(ViewRange);
	}
	
	if (GetType() == ESequencerNode::Object)
	{
		return SNew(SSequencerObjectTrack, SharedThis(this))
			.ViewRange(ViewRange);
	}

	// currently only section areas display widgets
	return SNullWidget::NullWidget;
}


TSharedPtr<FSequencerDisplayNode> FSequencerDisplayNode::GetSectionAreaAuthority() const
{
	TSharedPtr<FSequencerDisplayNode> Authority = SharedThis(const_cast<FSequencerDisplayNode*>(this));

	while (Authority.IsValid())
	{
		if (Authority->GetType() == ESequencerNode::Object || Authority->GetType() == ESequencerNode::Track)
		{
			return Authority;
		}
		else
		{
			Authority = Authority->GetParent();
		}
	}

	return Authority;
}


FString FSequencerDisplayNode::GetPathName() const
{
	// First get our parent's path
	FString PathName;

	if (ParentNode.IsValid())
	{
		PathName = ParentNode.Pin()->GetPathName() + TEXT(".");
	}

	//then append our path
	PathName += GetNodeName().ToString();

	return PathName;
}


TSharedPtr<SWidget> FSequencerDisplayNode::OnSummonContextMenu()
{
	// @todo sequencer replace with UI Commands instead of faking it
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetSequencer().GetCommandBindings());

	BuildContextMenu(MenuBuilder);

	return MenuBuilder.MakeWidget();
}


void FSequencerDisplayNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	TSharedRef<FSequencerDisplayNode> ThisNode = SharedThis(this);

	MenuBuilder.BeginSection("Edit", LOCTEXT("EditContextMenuSectionName", "Edit"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleNodeActive", "Active"),
			LOCTEXT("ToggleNodeActiveTooltip", "Set this track or selected tracks active/inactive"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(&GetSequencer(), &FSequencer::ToggleNodeActive),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(&GetSequencer(), &FSequencer::IsNodeActive)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleNodeLock", "Locked"),
			LOCTEXT("ToggleNodeLockTooltip", "Lock or unlock this node or selected tracks"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(&GetSequencer(), &FSequencer::ToggleNodeLocked),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(&GetSequencer(), &FSequencer::IsNodeLocked)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);


		// Add cut, copy and paste functions to the tracks
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Cut);

		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteNode", "Delete"),
			LOCTEXT("DeleteNodeTooltip", "Delete this or selected tracks"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(&GetSequencer(), &FSequencer::DeleteNode, ThisNode))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenameNode", "Rename"),
			LOCTEXT("RenameNodeTooltip", "Rename this track"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FSequencerDisplayNode::HandleContextMenuRenameNodeExecute),
				FCanExecuteAction::CreateSP(this, &FSequencerDisplayNode::HandleContextMenuRenameNodeCanExecute)
			)
		);
	}
	MenuBuilder.EndSection();
}


void FSequencerDisplayNode::GetChildKeyAreaNodesRecursively(TArray< TSharedRef<FSequencerSectionKeyAreaNode> >& OutNodes) const
{
	for (const TSharedRef<FSequencerDisplayNode>& Node : ChildNodes)
	{
		if (Node->GetType() == ESequencerNode::KeyArea)
		{
			OutNodes.Add(StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node));
		}

		Node->GetChildKeyAreaNodesRecursively(OutNodes);
	}
}


void FSequencerDisplayNode::SetExpansionState(bool bInExpanded)
{
	bExpanded = bInExpanded;

	// Expansion state has changed, save it to the movie scene now
	ParentTree.SaveExpansionState(*this, bExpanded);
}


bool FSequencerDisplayNode::IsExpanded() const
{
	return bExpanded;
}


bool FSequencerDisplayNode::IsHidden() const
{
	return ParentTree.HasActiveFilter() && !ParentTree.IsNodeFiltered(AsShared());
}


bool FSequencerDisplayNode::IsHovered() const
{
	return ParentTree.GetHoveredNode().Get() == this;
}

TSharedRef<FGroupedKeyArea> FSequencerDisplayNode::GetKeyGrouping(int32 InSectionIndex)
{
	if (!KeyGroupings.IsValidIndex(InSectionIndex))
	{
		KeyGroupings.SetNum(InSectionIndex + 1);
	}

	auto& KeyGroup = KeyGroupings[InSectionIndex];

	if (!KeyGroup.IsValid())
	{
		KeyGroup = MakeShareable(new FGroupedKeyArea(*this, InSectionIndex));
	}

	return KeyGroup.ToSharedRef();
}


TSharedRef<FGroupedKeyArea> FSequencerDisplayNode::UpdateKeyGrouping(int32 InSectionIndex)
{
	if (!KeyGroupings.IsValidIndex(InSectionIndex))
	{
		KeyGroupings.SetNum(InSectionIndex + 1);
	}

	auto& KeyGroup = KeyGroupings[InSectionIndex];

	if (!KeyGroup.IsValid())
	{
		KeyGroup = MakeShareable(new FGroupedKeyArea(*this, InSectionIndex));
	}
	else if (KeyGroupRegenerationLock.GetValue() == 0)
	{
		*KeyGroup = FGroupedKeyArea(*this, InSectionIndex);
	}

	return KeyGroup.ToSharedRef();
}


void FSequencerDisplayNode::HandleContextMenuRenameNodeExecute()
{
	RenameRequestedEvent.Broadcast();
}


bool FSequencerDisplayNode::HandleContextMenuRenameNodeCanExecute() const
{
	return CanRenameNode();
}


void FSequencerDisplayNode::DisableKeyGoupingRegeneration()
{
	KeyGroupRegenerationLock.Increment();
}


void FSequencerDisplayNode::EnableKeyGoupingRegeneration()
{
	KeyGroupRegenerationLock.Decrement();
}


void FSequencerDisplayNode::AddChildAndSetParent( TSharedRef<FSequencerDisplayNode> InChild )
{
	ChildNodes.Add( InChild );
	InChild->ParentNode = SharedThis( this );
}


#undef LOCTEXT_NAMESPACE
