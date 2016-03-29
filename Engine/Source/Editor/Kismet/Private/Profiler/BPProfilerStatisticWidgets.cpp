// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SBlueprintProfilerView.h"
#include "BPProfilerStatisticWidgets.h"
#include "SHyperlink.h"
#include "EventExecution.h"
#include "Developer/BlueprintProfiler/Public/BlueprintProfilerModule.h"

#define LOCTEXT_NAMESPACE "BlueprintProfilerViewTypesUI"

//////////////////////////////////////////////////////////////////////////
// BlueprintProfilerStatText

namespace BlueprintProfilerStatText
{
	const FName ColumnId_Name("Name");
	const FName ColumnId_InclusiveTime("InclusiveTime");
	const FName ColumnId_Time("Time");
	const FName ColumnId_PureTime("PureTime");
	const FName ColumnId_MaxTime("MaxTime");
	const FName ColumnId_MinTime("MinTime");
	const FName ColumnId_Samples("Samples");
	const FName ColumnId_TotalTime("TotalTime");

	const FText ColumnText_Name(LOCTEXT("Name", "Name") );
	const FText ColumnText_InclusiveTime(LOCTEXT("InclusiveTime", "Inclusive Time (ms)"));
	const FText ColumnText_Time(LOCTEXT("Time", "Time (ms)"));
	const FText ColumnText_PureTime(LOCTEXT("PureTime", "Pure Time (ms)"));
	const FText ColumnText_MaxTime(LOCTEXT("MaxTime", "Max Time (ms)"));
	const FText ColumnText_MinTime(LOCTEXT("MinTime", "Min Time (ms)"));
	const FText ColumnText_Samples(LOCTEXT("Samples", "Samples"));
	const FText ColumnText_TotalTime(LOCTEXT("TotalTime", "Total Time (s)"));
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
				SNew(SExpanderArrow, SharedThis(this))
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
	SMultiColumnTableRow<FBPStatWidgetPtr>::Construct( FSuperRowType::FArguments(), OwnerTableView );	
}

const FName SProfilerStatRow::GetStatName(const EBlueprintProfilerStat::Type StatId)
{
	switch(StatId)
	{
		case EBlueprintProfilerStat::Name:			return BlueprintProfilerStatText::ColumnId_Name;
		case EBlueprintProfilerStat::TotalTime:		return BlueprintProfilerStatText::ColumnId_TotalTime;
		case EBlueprintProfilerStat::InclusiveTime:	return BlueprintProfilerStatText::ColumnId_InclusiveTime;
		case EBlueprintProfilerStat::Time:			return BlueprintProfilerStatText::ColumnId_Time;
		case EBlueprintProfilerStat::PureTime:		return BlueprintProfilerStatText::ColumnId_PureTime;
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
		case EBlueprintProfilerStat::InclusiveTime:	return BlueprintProfilerStatText::ColumnText_InclusiveTime;
		case EBlueprintProfilerStat::Time:			return BlueprintProfilerStatText::ColumnText_Time;
		case EBlueprintProfilerStat::PureTime:		return BlueprintProfilerStatText::ColumnText_PureTime;
		case EBlueprintProfilerStat::MaxTime:		return BlueprintProfilerStatText::ColumnText_MaxTime;
		case EBlueprintProfilerStat::MinTime:		return BlueprintProfilerStatText::ColumnText_MinTime;
		case EBlueprintProfilerStat::Samples:		return BlueprintProfilerStatText::ColumnText_Samples;
		default:									return FText::GetEmpty();
	}
}

//////////////////////////////////////////////////////////////////////////
// FBPProfilerStatWidget

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
					SNew(SImage)
					.Image(ExecNode->GetIcon())
					.ColorAndOpacity(ExecNode->GetIconColor())
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5,0))
				[
					SNew(SHyperlink)
					.Text(ExecNode->GetDisplayName())
					.Style(FEditorStyle::Get(), "HoverOnlyHyperlink")
					.ToolTipText(ExecNode->GetToolTipText())
					.OnNavigate(this, &FBPProfilerStatWidget::NavigateTo)
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
			const uint32 NonNodeStats = EScriptExecutionNodeFlags::Class|EScriptExecutionNodeFlags::Instance|EScriptExecutionNodeFlags::Event|
										EScriptExecutionNodeFlags::BranchNode|EScriptExecutionNodeFlags::CallSite|EScriptExecutionNodeFlags::ExecPin;

			if (ColumnName == BlueprintProfilerStatText::ColumnId_TotalTime)
			{
				if (!ExecNode->HasFlags(EScriptExecutionNodeFlags::ExecPin))
				{
					TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetTotalTimingText);
					ColorAttr = TAttribute<FSlateColor>(FLinearColor::White);
				}
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_InclusiveTime)
			{
				if (!ExecNode->HasFlags(NonNodeStats))
				{
					TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetInclusiveTimingText);
					ColorAttr = TAttribute<FSlateColor>(PerformanceStats.Get(), &FScriptPerfData::GetInclusiveHeatColor);
				}
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_Time)
			{
				if (!ExecNode->HasFlags(EScriptExecutionNodeFlags::ExecPin))
				{
					TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetNodeTimingText);
					ColorAttr = TAttribute<FSlateColor>(PerformanceStats.Get(), &FScriptPerfData::GetNodeHeatColor);
				}
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_PureTime)
			{
				if (!ExecNode->HasFlags(NonNodeStats))
				{
					TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetPureNodeTimingText);
					ColorAttr = TAttribute<FSlateColor>(PerformanceStats.Get(), &FScriptPerfData::GetInclusiveHeatColor);
				}
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_MaxTime)
			{
				if (!ExecNode->HasFlags(EScriptExecutionNodeFlags::ExecPin))
				{
					TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetMaxTimingText);
					ColorAttr = TAttribute<FSlateColor>(PerformanceStats.Get(), &FScriptPerfData::GetMaxTimeHeatColor);
				}
			}
			else if (ColumnName == BlueprintProfilerStatText::ColumnId_MinTime)
			{
				if (!ExecNode->HasFlags(EScriptExecutionNodeFlags::ExecPin))
				{
					TextAttr = TAttribute<FText>(PerformanceStats.Get(), &FScriptPerfData::GetMinTimingText);
					ColorAttr = TAttribute<FSlateColor>(FLinearColor::White);
				}
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

void FBPProfilerStatWidget::GenerateExecNodeWidgets(const FName InstanceName, const FName FilterGraph)
{
	if (ExecNode.IsValid())
	{
		// Grab Performance Stats
		PerformanceStats = ExecNode->GetPerfDataByInstanceAndTracePath(InstanceName, WidgetTracePath);
		// Handle branch creation
		CachedChildren.Reset(0);
		if (ExecNode->IsBranch())
		{
			for (auto LinkIter : ExecNode->GetLinkedNodes())
			{
				TArray<FScriptNodeExecLinkage::FLinearExecPath> LinearExecNodes;
				FTracePath LinkPath(WidgetTracePath);
				LinkPath.AddExitPin(LinkIter.Key);
				LinkIter.Value->GetLinearExecutionPath(LinearExecNodes, LinkPath);
				for (auto LinearPathIter : LinearExecNodes)
				{
					TSharedPtr<FBPProfilerStatWidget> NewLinkedNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(LinearPathIter.LinkedNode, LinearPathIter.TracePath));
					NewLinkedNode->GenerateExecNodeWidgets(InstanceName, FilterGraph);
					CachedChildren.Add(NewLinkedNode);
				}
			}
		}
		else
		{
			// @TODO - filter out pure nodes?
			if (!ExecNode->IsPureNode())
			{
				// Get the full pure node chain associated with this exec node.
				TMap<int32, TSharedPtr<FScriptExecutionNode>> AllPureNodes;
				ExecNode->GetAllPureNodes(AllPureNodes);

				// Sort pure nodes by script offset (execution order).
				AllPureNodes.KeySort(TLess<int32>());

				// Build trace path, tree view node widget and register perf stats for tracking.
				FTracePath PureTracePath(WidgetTracePath);
				for (auto Iter : AllPureNodes)
				{
					PureTracePath.AddExitPin(Iter.Key);
					TSharedPtr<FBPProfilerStatWidget> NewPureChildNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(Iter.Value, PureTracePath));
					NewPureChildNode->GenerateExecNodeWidgets(InstanceName, FilterGraph);
					CachedChildren.Add(NewPureChildNode);
				}
			}

			for (auto Iter : ExecNode->GetChildNodes())
			{
				// Filter out events based on graph
				bool bFiltered = (Iter->IsEvent() && FilterGraph != NAME_None) ? IsEventFiltered(Iter, FilterGraph) : false;
				if (!bFiltered)
				{
					TArray<FScriptNodeExecLinkage::FLinearExecPath> LinearExecNodes;
					FTracePath ChildTracePath(WidgetTracePath);
					Iter->GetLinearExecutionPath(LinearExecNodes, ChildTracePath);
					if (LinearExecNodes.Num() > 1)
					{
						for (auto LinearPathIter : LinearExecNodes)
						{
							TSharedPtr<FBPProfilerStatWidget> NewLinkedNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(LinearPathIter.LinkedNode, LinearPathIter.TracePath));
							NewLinkedNode->GenerateExecNodeWidgets(InstanceName, FilterGraph);
							CachedChildren.Add(NewLinkedNode);
						}
					}
					else
					{
						TSharedPtr<FBPProfilerStatWidget> NewChildNode = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(Iter, ChildTracePath));
						NewChildNode->GenerateExecNodeWidgets(InstanceName, FilterGraph);
						CachedChildren.Add(NewChildNode);
					}
				}
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

bool FBPProfilerStatWidget::IsEventFiltered(TSharedPtr<FScriptExecutionNode> InEventNode, FName GraphFilter) const
{
	bool bEventFiltered = false;
	if (InEventNode.IsValid() && InEventNode->IsEvent())
	{
		if (InEventNode->GetGraphName() == UEdGraphSchema_K2::FN_UserConstructionScript)
		{
			bEventFiltered = GraphFilter != UEdGraphSchema_K2::FN_UserConstructionScript;
		}
		else
		{
			bEventFiltered = GraphFilter == UEdGraphSchema_K2::FN_UserConstructionScript;
		}
	}
	return bEventFiltered;
}

#undef LOCTEXT_NAMESPACE
