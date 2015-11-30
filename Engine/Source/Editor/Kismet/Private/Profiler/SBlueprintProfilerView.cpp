// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintEditorPrivatePCH.h"
#include "BPProfilerStatisticWidgets.h"
#include "SBlueprintProfilerView.h"

//////////////////////////////////////////////////////////////////////////
// SBlueprintProfilerView

FNumberFormattingOptions SBlueprintProfilerView::NumberFormat;

SBlueprintProfilerView::~SBlueprintProfilerView()
{
	// Register for Profiler toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.RemoveAll(this);
}

void SBlueprintProfilerView::Construct(const FArguments& InArgs)
{	
	CurrentViewType = InArgs._ProfileViewType;
	// Register for Profiler toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.AddRaw(this, &SBlueprintProfilerView::OnToggleProfiler);
	// Initialise the number format.
	NumberFormat.MinimumFractionalDigits = 4;
	NumberFormat.MaximumFractionalDigits = 4;
	NumberFormat.UseGrouping = false;
	// Update the average sample retention count.
	FBPPerformanceData::StatSampleSize = GetDefault<UEditorExperimentalSettings>()->BlueprintProfilerAverageSampleCount;
	// Create the profiler view widgets
	bTreeViewIsDirty = false;
	CreateProfilerWidgets();
}

TSharedRef<ITableRow> SBlueprintProfilerView::OnGenerateRowForProfiler(FBPProfilerStatPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SProfilerStatRow, OwnerTable, InItem);
}

void SBlueprintProfilerView::OnGetChildrenForProfiler(FBPProfilerStatPtr InParent, TArray<FBPProfilerStatPtr>& OutChildren)
{
	if (InParent.IsValid())
	{
		InParent->GatherChildren(OutChildren);
	}
}

void SBlueprintProfilerView::OnToggleProfiler(bool bEnabled)
{
	if (!bEnabled)
	{
		// Clear profiler data, this requires work as I think the stats are currently leaking.
		for (auto Iter : RootTreeItems)
		{
			Iter->ReleaseData();
			Iter.Reset();
		}
		RootTreeItems.Reset();
		ClassContexts.Reset();
		if (ExecutionStatTree.IsValid())
		{
			bTreeViewIsDirty = true;
		}
	}
}

void SBlueprintProfilerView::CreateProfilerWidgets()
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(0.2f)
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Visibility(this, &SBlueprintProfilerView::IsProfilerHidden)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("InactiveProfilerWidget", "InactiveText", "The Blueprint Profiler is currently Inactive"))
			]
		]
#if 0 // To do stat ranking list
		+SVerticalBox::Slot()
		[
			SAssignNew(BlueprintStatList, SListView<FBPProfilerStatPtr>)
			.ListItemsSource(&RootTreeItems)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SBlueprintProfilerView::OnGenerateRowForProfiler)
//			.OnSelectionChanged(this, &SMessagingTypes::HandleTypeListSelectionChanged)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Name))
				.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Name))
				.ManualWidth(450)
				+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::InclusiveTime))
				.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::InclusiveTime))
				.ManualWidth(120)
				+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::ExclusiveTime))
				.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::ExclusiveTime))
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
#endif
		+SVerticalBox::Slot()
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Visibility(this, &SBlueprintProfilerView::IsProfilerVisible)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("BueprintProfilerView", "StatisticTreeTitle", "Execution Tree Performance Statistics"))
				]
				+SVerticalBox::Slot()
				.FillHeight(0.8f)
				[
					SAssignNew(ExecutionStatTree, STreeView<FBPProfilerStatPtr>)
					.TreeItemsSource(&RootTreeItems)
					.SelectionMode(ESelectionMode::Single)
					.OnGetChildren(this, &SBlueprintProfilerView::OnGetChildrenForProfiler)
					.OnGenerateRow(this, &SBlueprintProfilerView::OnGenerateRowForProfiler)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Name))
						.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Name))
						.ManualWidth(450)
						+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::InclusiveTime))
						.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::InclusiveTime))
						.ManualWidth(120)
						+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::ExclusiveTime))
						.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::ExclusiveTime))
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
		]
	];
}

void SBlueprintProfilerView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		IBlueprintProfilerInterface* ProfilerInterface = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler");
		if (ProfilerInterface && ProfilerInterface->IsProfilerEnabled())
		{
			// Find and process execution path timing data.
			bool bRequestRefresh = false;
			TArray<FBlueprintInstrumentedEvent>& ProfilerData = ProfilerInterface->GetProfilingData();
			int32 ExecStart = 0;

			for (int32 EventIdx = 0; EventIdx < ProfilerData.Num(); ++EventIdx)
			{
				switch(ProfilerData[EventIdx].GetType())
				{
					case EScriptInstrumentation::Class:
					{
						CurrentClassPath = ProfilerData[EventIdx].GetObjectPath();
						ExecStart = EventIdx;
						break;
					}
					case EScriptInstrumentation::Stop:
					{
						check(!CurrentClassPath.IsEmpty());
						FProfilerClassContext* ActiveClass = ClassContexts.Find(CurrentClassPath);
						const bool bNewClassContext = !ActiveClass;
						if (!ActiveClass)
						{
							ActiveClass = &ClassContexts.Add(CurrentClassPath);
						}
						ActiveClass->ProcessExecutionPath(ProfilerData, ExecStart, EventIdx);
						if (bNewClassContext)
						{
							RootTreeItems.Add(ActiveClass->GetRootStat());
							bRequestRefresh = true;
						}
						ExecStart = EventIdx+1;
						break;
					}
				}
			}
			// Reset profiler data
			ProfilerData.Reset();
		}
	}
	// Refresh Tree
	if (bTreeViewIsDirty && ExecutionStatTree.IsValid())
	{
		ExecutionStatTree->RequestTreeRefresh();
	}
}
