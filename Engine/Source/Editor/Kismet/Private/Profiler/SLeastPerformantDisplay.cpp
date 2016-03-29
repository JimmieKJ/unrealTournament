// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SLeastPerformantDisplay.h"
#include "BPProfilerStatisticWidgets.h"

//////////////////////////////////////////////////////////////////////////
// SLeastPerformantDisplay

SLeastPerformantDisplay::~SLeastPerformantDisplay()
{
}

void SLeastPerformantDisplay::Construct(const FArguments& InArgs)
{	
	BlueprintEditor = InArgs._AssetEditor;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(FMargin(0, 0, 0, 2))
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SBorder)
			.Padding(4)
			.BorderImage(FEditorStyle::GetBrush("BlueprintProfiler.ViewToolBar"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoHeight()
				[
					SNew(SHorizontalBox)
				]
			]
		]
		+SVerticalBox::Slot()
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			[
				SAssignNew(BlueprintStatList, SListView<FBPStatWidgetPtr>)
				.ListItemsSource(&RootTreeItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SLeastPerformantDisplay::OnGenerateRow)
	//			.OnSelectionChanged(this, &SMessagingTypes::HandleTypeListSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Name))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Name))
					.ManualWidth(450)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Time))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Time))
					.ManualWidth(70)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::PureTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::PureTime))
					.ManualWidth(100)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::InclusiveTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::InclusiveTime))
					.ManualWidth(120)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::MaxTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::MaxTime))
					.ManualWidth(90)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::MinTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::MinTime))
					.ManualWidth(90)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::TotalTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::TotalTime))
					.ManualWidth(80)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Samples))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Samples))
					.ManualWidth(60)
				)
			]
		]
	];
}

TSharedRef<ITableRow> SLeastPerformantDisplay::OnGenerateRow(FBPStatWidgetPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SProfilerStatRow, OwnerTable, InItem);
}

void SLeastPerformantDisplay::OnDoubleClickStatistic(FBPStatWidgetPtr Item)
{
	if (Item.IsValid())
	{
//		Item->ExpandWidgetState(ExecutionStatTree, !Item->GetExpansionState());
	}
}

void SLeastPerformantDisplay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		if (IBlueprintProfilerInterface* Profiler = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler"))
		{
			// Find and process execution path timing data.
			if (BlueprintEditor.IsValid())
			{
			}
		}
	}
}
