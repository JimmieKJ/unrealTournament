// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SProfilerOverview.h"
#include "BPProfilerStatisticWidgets.h"

//////////////////////////////////////////////////////////////////////////
// SProfilerOverview

SProfilerOverview::~SProfilerOverview()
{
}

void SProfilerOverview::Construct(const FArguments& InArgs)
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
			.BorderImage(FEditorStyle::GetBrush("BlueprintProfiler.ViewContent"))
			[
				SAssignNew(BlueprintStatList, SListView<FBPStatWidgetPtr>)
				.ListItemsSource(&RootTreeItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SProfilerOverview::OnGenerateRow)
	//			.OnSelectionChanged(this, &SMessagingTypes::HandleTypeListSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Name))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Name))
					.FillWidth(0.5f)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::AverageTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::AverageTime))
					.FixedWidth(90)
					.HAlignHeader(HAlign_Right)
					.HAlignCell(HAlign_Right)
					.VAlignCell(VAlign_Center)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::InclusiveTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::InclusiveTime))
					.FixedWidth(120)
					.HAlignHeader(HAlign_Right)
					.HAlignCell(HAlign_Right)
					.VAlignCell(VAlign_Center)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::MaxTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::MaxTime))
					.FixedWidth(90)
					.HAlignHeader(HAlign_Right)
					.HAlignCell(HAlign_Right)
					.VAlignCell(VAlign_Center)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::MinTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::MinTime))
					.FixedWidth(90)
					.HAlignHeader(HAlign_Right)
					.HAlignCell(HAlign_Right)
					.VAlignCell(VAlign_Center)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::TotalTime))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::TotalTime))
					.FixedWidth(100)
					.HAlignHeader(HAlign_Right)
					.HAlignCell(HAlign_Right)
					.VAlignCell(VAlign_Center)
					+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Samples))
					.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Samples))
					.FixedWidth(100)
					.HAlignHeader(HAlign_Center)
					.HAlignCell(HAlign_Center)
					.VAlignCell(VAlign_Center)
				)
			]
		]
	];
}

TSharedRef<ITableRow> SProfilerOverview::OnGenerateRow(FBPStatWidgetPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SProfilerStatRow, OwnerTable, InItem);
}

void SProfilerOverview::OnDoubleClickStatistic(FBPStatWidgetPtr Item)
{
	if (Item.IsValid())
	{
//		Item->ExpandWidgetState(ExecutionStatTree, !Item->GetExpansionState());
	}
}

void SProfilerOverview::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		if (IBlueprintProfilerInterface* ProfilerInterface = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler"))
		{
		}
	}
}
