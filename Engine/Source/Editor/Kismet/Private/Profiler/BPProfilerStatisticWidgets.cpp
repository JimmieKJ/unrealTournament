// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SBlueprintProfilerView.h"
#include "BPProfilerStatisticWidgets.h"
#include "EventExecution.h"
#include "SProfilerStatExpander.h"
#include "Developer/BlueprintProfiler/Public/BlueprintProfilerModule.h"

#define LOCTEXT_NAMESPACE "BlueprintProfilerViewTypesUI"

//////////////////////////////////////////////////////////////////////////
// BlueprintProfilerStatText

namespace BlueprintProfilerStatText
{
	const FName ColumnId_Name("Name");
	const FName ColumnId_AverageTime("AverageTime");
	const FName ColumnId_InclusiveTime("InclusiveTime");
	const FName ColumnId_MaxTime("MaxTime");
	const FName ColumnId_MinTime("MinTime");
	const FName ColumnId_Samples("Samples");
	const FName ColumnId_TotalTime("TotalTime");

	const FText ColumnText_Name(LOCTEXT("Name", "Name") );
	const FText ColumnText_AverageTime(LOCTEXT("AverageTime", "Avg Time (ms)"));
	const FText ColumnText_InclusiveTime(LOCTEXT("InclusiveTime", "Inclusive Time (ms)"));
	const FText ColumnText_MaxTime(LOCTEXT("MaxTime", "Max Time (ms)"));
	const FText ColumnText_MinTime(LOCTEXT("MinTime", "Min Time (ms)"));
	const FText ColumnText_Samples(LOCTEXT("Samples", "Samples"));
	const FText ColumnText_TotalTime(LOCTEXT("TotalTime", "Total Time (ms)"));
};

//////////////////////////////////////////////////////////////////////////
// SProfilerStatRow

TSharedRef<SWidget> SProfilerStatRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == BlueprintProfilerStatText::ColumnId_Name)
	{
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SProfilerStatExpander, SharedThis(this))
				.IndentAmount(15.f)
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				ItemToEdit->GenerateColumnWidget(ColumnName)
			];
	}
	else
	{
		return ItemToEdit->GenerateColumnWidget(ColumnName);
	}
}

void SProfilerStatRow::Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView, FBPStatWidgetPtr InItemToEdit)
{
	check(InItemToEdit.IsValid());
	ItemToEdit = InItemToEdit;
	SMultiColumnTableRow<FBPStatWidgetPtr>::Construct(FSuperRowType::FArguments().Style(FEditorStyle::Get(), "BlueprintProfiler.TableView.Row"), OwnerTableView);
}

const FName SProfilerStatRow::GetStatName(const EBlueprintProfilerStat::Type StatId)
{
	switch(StatId)
	{
		case EBlueprintProfilerStat::Name:			return BlueprintProfilerStatText::ColumnId_Name;
		case EBlueprintProfilerStat::TotalTime:		return BlueprintProfilerStatText::ColumnId_TotalTime;
		case EBlueprintProfilerStat::AverageTime:	return BlueprintProfilerStatText::ColumnId_AverageTime;
		case EBlueprintProfilerStat::InclusiveTime:	return BlueprintProfilerStatText::ColumnId_InclusiveTime;
		case EBlueprintProfilerStat::MaxTime:		return BlueprintProfilerStatText::ColumnId_MaxTime;
		case EBlueprintProfilerStat::MinTime:		return BlueprintProfilerStatText::ColumnId_MinTime;
		case EBlueprintProfilerStat::Samples:		return BlueprintProfilerStatText::ColumnId_Samples;
		default:									return NAME_None;
	}
}

const FText SProfilerStatRow::GetStatText(const EBlueprintProfilerStat::Type StatId)
{
	switch(StatId)
	{
		case EBlueprintProfilerStat::Name:			return BlueprintProfilerStatText::ColumnText_Name;
		case EBlueprintProfilerStat::TotalTime:		return BlueprintProfilerStatText::ColumnText_TotalTime;
		case EBlueprintProfilerStat::AverageTime:	return BlueprintProfilerStatText::ColumnText_AverageTime;
		case EBlueprintProfilerStat::InclusiveTime:	return BlueprintProfilerStatText::ColumnText_InclusiveTime;
		case EBlueprintProfilerStat::MaxTime:		return BlueprintProfilerStatText::ColumnText_MaxTime;
		case EBlueprintProfilerStat::MinTime:		return BlueprintProfilerStatText::ColumnText_MinTime;
		case EBlueprintProfilerStat::Samples:		return BlueprintProfilerStatText::ColumnText_Samples;
		default:									return FText::GetEmpty();
	}
}

//////////////////////////////////////////////////////////////////////////
// FBPProfilerStatWidget

FBPProfilerStatWidget::FBPProfilerStatWidget(TSharedPtr<class FScriptExecutionNode> InExecNode, const FTracePath& WidgetTracePathIn)
	: WidgetTracePath(WidgetTracePathIn)
	, ExecNode(InExecNode)
{
	if (ExecNode->HasFlags(EScriptExecutionNodeFlags::TunnelInstance))
	{
		WidgetTracePath = FTracePath(WidgetTracePathIn, StaticCastSharedPtr<FScriptExecutionTunnelEntry>(InExecNode));
	}
}

TSharedRef<SWidget> FBPProfilerStatWidget::GenerateColumnWidget(FName ColumnName)
{
	if (ExecNode.IsValid())
	{
		if (ColumnName == BlueprintProfilerStatText::ColumnId_Name)
		{
			return SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					ExecNode->GetIconWidget(WidgetTracePath)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5,0))
				[
					ExecNode->GetHyperlinkWidget(WidgetTracePath)
		#if TRACEPATH_DEBUG
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5,0))
				[
					SNew(STextBlock)
					.Text(FText::FromString(*WidgetTracePath.GetPathString()))
		#endif
				];
		}
		else
		{
			TAttribute<FText> TextAttr(LOCTEXT("NonApplicableStat", ""));
			TAttribute<FSlateColor> ColorAttr;
			const uint32 NonNodeStats = EScriptExecutionNodeFlags::Container|EScriptExecutionNodeFlags::CallSite|EScriptExecutionNodeFlags::BranchNode|EScriptExecutionNodeFlags::ExecPin;

			if (ColumnName == BlueprintProfilerStatText::ColumnId_TotalTime)
			{
				TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetTotalTimingText);
				ColorAttr = TAttribute<FSlateColor>(FLinearColor::White);
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_InclusiveTime)
			{
				TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetInclusiveTimingText);
				ColorAttr = TAttribute<FSlateColor>(this, &FBPProfilerStatWidget::GetInclusiveHeatColor);
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_AverageTime)
			{
				TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetAverageTimingText);
				ColorAttr = TAttribute<FSlateColor>(this, &FBPProfilerStatWidget::GetAverageHeatColor);
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_MaxTime)
			{
				TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetMaxTimingText);
				//ColorAttr = TAttribute<FSlateColor>(this, &FBPProfilerStatWidget::GetMaxTimeHeatColor);
				ColorAttr = TAttribute<FSlateColor>(FLinearColor::White);
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_MinTime)
			{
				TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetMinTimingText);
				ColorAttr = TAttribute<FSlateColor>(FLinearColor::White);
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_Samples)
			{
				TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetSamplesText);
				ColorAttr = TAttribute<FSlateColor>(FLinearColor::White);
			}
			// Create the actual widget
			return SNew(STextBlock)
				.Text(TextAttr)
				.ColorAndOpacity(ColorAttr);
		}
	}
	return SNullWidget::NullWidget;
}

void FBPProfilerStatWidget::NavigateTo() const
{
	if (ExecNode.IsValid())
	{
		ExecNode->NavigateToObject();
	}
}

FSlateColor FBPProfilerStatWidget::GetAverageHeatColor() const
{
	const float Value = 1.f - PerformanceStats->GetAverageHeatLevel();
	return FLinearColor(1.f, Value, Value);
}

FSlateColor FBPProfilerStatWidget::GetInclusiveHeatColor() const
{
	const float Value = 1.f - PerformanceStats->GetInclusiveHeatLevel();
	return FLinearColor(1.f, Value, Value);
}

FSlateColor FBPProfilerStatWidget::GetMaxTimeHeatColor() const
{
	const float Value = 1.f - PerformanceStats->GetMaxTimeHeatLevel();
	return FLinearColor(1.f, Value, Value);
}

void FBPProfilerStatWidget::GenerateExecNodeWidgets(const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions)
{
	if (ExecNode.IsValid())
	{
		// Create perf stats entry
		PerformanceStats = ExecNode->GetOrAddPerfDataByInstanceAndTracePath(DisplayOptions->GetActiveInstance(), WidgetTracePath);
		CachedChildren.Reset(0);
		// Check for specialised variants
		if (ExecNode->HasFlags(EScriptExecutionNodeFlags::PureStats))
		{
			if (ExecNode->IsPureChain())
			{
				// Generate the specialised pure chain widget.
				GeneratePureNodeWidgets(DisplayOptions);
			}
		}
		else if (ExecNode->IsTunnelEntry())
		{
			TSharedPtr<FScriptExecutionTunnelEntry> TunnelEntryNode = StaticCastSharedPtr<FScriptExecutionTunnelEntry>(ExecNode);
			// Generate specialised tunnel node widget.
			if (TunnelEntryNode->IsComplexTunnel())
			{
				// Create a widget that houses the exit sites tracepaths as children.
				GenerateComplexTunnelWidgets(TunnelEntryNode, DisplayOptions);
			}
			else
			{
				// Create a widget that houses only tunnel exec nodes as children.
				GenerateSimpleTunnelWidgets(TunnelEntryNode, DisplayOptions);
			}
		}
		else	
		{
			// Create a standard set of widgets that serves for most exec nodes.
			GenerateStandardNodeWidgets(DisplayOptions);
		}
	}
}

void FBPProfilerStatWidget::GenerateStandardNodeWidgets(const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions)
{
	// Add any child nodes that are not filtered out.
	for (auto Iter : ExecNode->GetChildNodes())
	{
		// Filter out events based on graph
		if (!DisplayOptions->IsFiltered(Iter))
		{
			TArray<FScriptNodeExecLinkage::FLinearExecPath> LinearExecNodes;
			FTracePath ChildTracePath(WidgetTracePath);
			Iter->GetLinearExecutionPath(LinearExecNodes, ChildTracePath, false);
			if (LinearExecNodes.Num() > 1)
			{
				TSharedPtr<FBPProfilerStatWidget> ChildContainer = AsShared();
				for (auto LinearPathIter : LinearExecNodes)
				{
					if (!DisplayOptions->IsFiltered(LinearPathIter.LinkedNode))
					{
						TSharedPtr<FBPProfilerStatWidget> NewLinkedNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(LinearPathIter.LinkedNode, LinearPathIter.TracePath));
						NewLinkedNode->GenerateExecNodeWidgets(DisplayOptions);
						ChildContainer->CachedChildren.Add(NewLinkedNode);
						if (LinearPathIter.LinkedNode->HasFlags(EScriptExecutionNodeFlags::Container))
						{
							ChildContainer = NewLinkedNode;
						}
					}
				}
			}
			else
			{
				TSharedPtr<FBPProfilerStatWidget> NewChildNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(Iter, ChildTracePath));
				NewChildNode->GenerateExecNodeWidgets(DisplayOptions);
				CachedChildren.Add(NewChildNode);
			}
		}
	}
	// Follow execution links and generate node widgets
	const bool bShouldBuildLinks = ExecNode->IsBranch()||ExecNode->HasFlags(EScriptExecutionNodeFlags::TunnelEntryPin);
	if (bShouldBuildLinks)
	{
		for (auto LinkIter : ExecNode->GetLinkedNodes())
		{
			if (!DisplayOptions->IsFiltered(LinkIter.Value))
			{
				TArray<FScriptNodeExecLinkage::FLinearExecPath> LinearExecNodes;
				FTracePath LinkPath(WidgetTracePath);
				if (LinkIter.Value->IsEventPin())
				{
					LinkPath.ResetPath();
				}
				if (!LinkIter.Value->HasFlags(EScriptExecutionNodeFlags::InvalidTrace))
				{
					LinkPath.AddExitPin(LinkIter.Key);
				}
				LinkIter.Value->GetLinearExecutionPath(LinearExecNodes, LinkPath);
				for (auto LinearPathIter : LinearExecNodes)
				{
					TSharedPtr<FBPProfilerStatWidget> NewLinkedNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(LinearPathIter.LinkedNode, LinearPathIter.TracePath));
					NewLinkedNode->GenerateExecNodeWidgets(DisplayOptions);
					CachedChildren.Add(NewLinkedNode);
				}
			}
		}
	}
}

void FBPProfilerStatWidget::GeneratePureNodeWidgets(const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions)
{
	// Get the full pure node chain associated with this exec node.
	TMap<int32, TSharedPtr<FScriptExecutionNode>> AllPureNodes;
	ExecNode->GetAllPureNodes(AllPureNodes);

	// Build trace path, tree view node widget and register perf stats for tracking.
	FTracePath PureTracePath(WidgetTracePath);
	for (auto Iter : AllPureNodes)
	{
		PureTracePath.AddExitPin(Iter.Key);
		TSharedPtr<FBPProfilerStatWidget> NewPureChildNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(Iter.Value, PureTracePath));
		NewPureChildNode->GenerateExecNodeWidgets(DisplayOptions);

		// Pure nodes are shown in reverse execution order.
		CachedChildren.Insert(NewPureChildNode, 0);
	}
}

void FBPProfilerStatWidget::GenerateSimpleTunnelWidgets(TSharedPtr<FScriptExecutionTunnelEntry> TunnelEntryNode, const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions)
{
	// Map entry points
	for (auto EntryPoint : TunnelEntryNode->GetChildNodes())
	{
		// Filter out events based on graph
		if (!DisplayOptions->IsFiltered(EntryPoint) && !EntryPoint->IsPureChain())
		{
			TArray<FScriptNodeExecLinkage::FLinearExecPath> LinearExecNodes;
			EntryPoint->GetLinearExecutionPath(LinearExecNodes, WidgetTracePath, true);
			TSharedPtr<FBPProfilerStatWidget> ChildContainer = AsShared();
			for (auto LinearPathIter : LinearExecNodes)
			{
				const bool bInternalTunnelBoundary = TunnelEntryNode->IsInternalBoundary(LinearPathIter.LinkedNode);
				const bool bPureChain = LinearPathIter.LinkedNode->IsPureChain();
				if (!DisplayOptions->IsFiltered(LinearPathIter.LinkedNode) && !bPureChain && !bInternalTunnelBoundary)
				{
					TSharedPtr<FBPProfilerStatWidget> NewLinkedNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(LinearPathIter.LinkedNode, LinearPathIter.TracePath));
					NewLinkedNode->GenerateExecNodeWidgets(DisplayOptions);
					CachedChildren.Add(NewLinkedNode);
					if (LinearPathIter.LinkedNode->HasFlags(EScriptExecutionNodeFlags::Container))
					{
						ChildContainer = NewLinkedNode;
					}
				}
			}
		}
	}
}

void FBPProfilerStatWidget::GenerateComplexTunnelWidgets(TSharedPtr<FScriptExecutionTunnelEntry> TunnelEntryNode, const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions)
{
	// Map entry points
	for (auto EntryPoint : TunnelEntryNode->GetChildNodes())
	{
		// Filter out events based on graph
		if (!DisplayOptions->IsFiltered(EntryPoint) && !EntryPoint->IsPureChain())
		{
			TSharedPtr<FBPProfilerStatWidget> EntryPointNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(EntryPoint, WidgetTracePath));
			EntryPointNode->GenerateExecNodeWidgets(DisplayOptions);
			CachedChildren.Add(EntryPointNode);
		}
	}
	// Create exit links as child nodes
	for (auto ExitSite : TunnelEntryNode->GetLinkedNodes())
	{
		TSharedPtr<FBPProfilerStatWidget> ExitSiteNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(ExitSite.Value, WidgetTracePath));
		ExitSiteNode->GenerateTunnelLinkWidgets(DisplayOptions, ExitSite.Key);
		CachedChildren.Add(ExitSiteNode);
		if (ExitSite.Value->IsTunnelExit())
		{
			TSharedPtr<FScriptExecutionTunnelExit> ExitSiteTypedNode = StaticCastSharedPtr<FScriptExecutionTunnelExit>(ExitSite.Value);
			// Allow exit sites to jump to external pins using the correct widget path.
			ExitSiteTypedNode->AddInstanceExitSite(WidgetTracePath, ExitSiteTypedNode->GetExternalPin());
		}
	}
}

void FBPProfilerStatWidget::GenerateTunnelLinkWidgets(const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions, const int32 ScriptOffset)
{
	// Create perf stats entry
	PerformanceStats = ExecNode->GetOrAddPerfDataByInstanceAndTracePath(DisplayOptions->GetActiveInstance(), WidgetTracePath);
	CachedChildren.Reset(0);
	// Create child tracepath
	FTracePath ChildTrace(WidgetTracePath);
	if (ScriptOffset != INDEX_NONE)
	{
		ChildTrace.AddExitPin(ScriptOffset);
	}
	// Build linked widgets
	for (auto LinkIter : ExecNode->GetLinkedNodes())
	{
		if (!DisplayOptions->IsFiltered(LinkIter.Value))
		{
			TArray<FScriptNodeExecLinkage::FLinearExecPath> LinearExecNodes;
			LinkIter.Value->GetLinearExecutionPath(LinearExecNodes, ChildTrace);
			for (auto LinearPathIter : LinearExecNodes)
			{
				TSharedPtr<FBPProfilerStatWidget> NewLinkedNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(LinearPathIter.LinkedNode, LinearPathIter.TracePath));
				NewLinkedNode->GenerateExecNodeWidgets(DisplayOptions);
				CachedChildren.Add(NewLinkedNode);
			}
		}
	}
}

void FBPProfilerStatWidget::GatherChildren(TArray<TSharedPtr<FBPProfilerStatWidget>>& OutChildren)
{
	if (CachedChildren.Num())
	{
		OutChildren.Append(CachedChildren.GetData(), CachedChildren.Num());
	}
}

bool FBPProfilerStatWidget::GetExpansionState() const 
{ 
	return ExecNode.IsValid() ? ExecNode->IsExpanded() : false;
}

void FBPProfilerStatWidget::SetExpansionState(bool bExpansionStateIn)
{
	if(ExecNode.IsValid())
	{
		ExecNode->SetExpanded(bExpansionStateIn);
	}
}

void FBPProfilerStatWidget::ExpandWidgetState(TSharedPtr<STreeView<FBPStatWidgetPtr>> TreeView, bool bStateIn)
{
	if (TreeView.IsValid())
	{
		TreeView->SetItemExpansion(AsShared(), bStateIn);
	}
	for (auto Iter : CachedChildren)
	{
		Iter->ExpandWidgetState(TreeView, bStateIn);
	}
}

void FBPProfilerStatWidget::RestoreWidgetExpansionState(TSharedPtr<STreeView<FBPStatWidgetPtr>> TreeView)
{
	if (TreeView.IsValid() && ExecNode.IsValid())
	{
		TreeView->SetItemExpansion(AsShared(), ExecNode->IsExpanded());
	}
	for (auto Iter : CachedChildren)
	{
		Iter->RestoreWidgetExpansionState(TreeView);
	}
}

bool FBPProfilerStatWidget::ProbeChildWidgetExpansionStates()
{
	bool bIsExpanded = false;
	if (ExecNode.IsValid() && ExecNode->IsExpanded())
	{
		bIsExpanded = true;
	}
	else
	{
		for (auto Iter : CachedChildren)
		{
			if (Iter->ProbeChildWidgetExpansionStates())
			{
				bIsExpanded = true;
				break;
			}
		}
	}
	if (bIsExpanded)
	{
		ExecNode->SetExpanded(true);
	}
	return bIsExpanded;
}

#undef LOCTEXT_NAMESPACE
