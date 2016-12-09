// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Profiler/SGraphExecutionStatDisplay.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "Profiler/BlueprintProfilerSettings.h"
#include "EditorStyleSet.h"
#include "Settings/EditorExperimentalSettings.h"
#include "BlueprintProfilerModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "ScriptInstrumentationPlayback.h"

#define LOCTEXT_NAMESPACE "BlueprintProfilerGraphExecView"

//////////////////////////////////////////////////////////////////////////
// SGraphExecutionStatDisplay

SGraphExecutionStatDisplay::~SGraphExecutionStatDisplay()
{
	// Remove delegate for profiling toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.RemoveAll(this);
	// Remove delegate for graph structural changes
	if (IBlueprintProfilerInterface* Profiler = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler"))
	{
		Profiler->GetGraphLayoutChangedDelegate().RemoveAll(this);
	}
}

void SGraphExecutionStatDisplay::Construct(const FArguments& InArgs)
{	
	BlueprintEditor = InArgs._AssetEditor;
	DisplayOptions = InArgs._DisplayOptions;
	// Register for profiling toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.AddSP(this, &SGraphExecutionStatDisplay::OnToggleProfiler);
	// Register delegate for graph structural changes
	if (IBlueprintProfilerInterface* Profiler = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler"))
	{
		Profiler->GetGraphLayoutChangedDelegate().AddSP(this, &SGraphExecutionStatDisplay::OnGraphLayoutChanged);
	}
	// Create the tree view
	ConstructTreeView();
}

void SGraphExecutionStatDisplay::ConstructTreeView()
{
	ChildSlot
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			.Visibility(this, &SGraphExecutionStatDisplay::GetWidgetVisibility)
			+SVerticalBox::Slot()
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FEditorStyle::GetBrush("BlueprintProfiler.ViewContent"))
				[
					SAssignNew(ExecutionStatTree, STreeView<FBPStatWidgetPtr>)
					.TreeItemsSource(&RootTreeItems)
					.SelectionMode(ESelectionMode::Single)
					.OnGetChildren(this, &SGraphExecutionStatDisplay::OnGetChildren)
					.OnGenerateRow(this, &SGraphExecutionStatDisplay::OnGenerateRow)
					.OnMouseButtonDoubleClick(this, &SGraphExecutionStatDisplay::OnDoubleClickStatistic)
					.OnExpansionChanged(this, &SGraphExecutionStatDisplay::OnStatisticExpansionChanged)
					.HeaderRow
					(
						GenerateHeaderRow()
					)
				]
			]
		]
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			.Visibility(this, &SGraphExecutionStatDisplay::GetNoDataWidgetVisibility)
			+SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage(FEditorStyle::GetBrush("BlueprintProfiler.ViewContent"))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoInstancesMessage", "There are currently no valid instances of this blueprint in the current level to gather profiling data on."))
					]
				]
			]
		]
	];
}

TSharedPtr<SHeaderRow> SGraphExecutionStatDisplay::GenerateHeaderRow() const
{
	// Create the header.
	TSharedPtr<SHeaderRow> HeaderRow;
	SAssignNew(HeaderRow, SHeaderRow)
		+SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Name))
		.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Name))
		.FillWidth(0.5f);
	// Add conditional columns
	const UBlueprintProfilerSettings* ProfilerSettings = GetDefault<UBlueprintProfilerSettings>();
	if (ProfilerSettings->bShowHottestPathStats)
	{
		HeaderRow->AddColumn(SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::HottestPath))
			.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::HottestPath))
			.FixedWidth(90)
			.HAlignHeader(HAlign_Right)
			.HAlignCell(HAlign_Right)
			.VAlignCell(VAlign_Center));
	}	
	if (ProfilerSettings->bShowHeatLevelStats)
	{
		HeaderRow->AddColumn(SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::HeatLevel))
			.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::HeatLevel))
			.FixedWidth(90)
			.HAlignHeader(HAlign_Right)
			.HAlignCell(HAlign_Right)
			.VAlignCell(VAlign_Center));
	}
	// Add Standard columns
	HeaderRow->AddColumn(SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::AverageTime))
		.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::AverageTime))
		.FixedWidth(90)
		.HAlignHeader(HAlign_Right)
		.HAlignCell(HAlign_Right)
		.VAlignCell(VAlign_Center)
		.OnSort(this, &SGraphExecutionStatDisplay::SetAverageHeatDisplay));
	HeaderRow->AddColumn(SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::InclusiveTime))
		.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::InclusiveTime))
		.FixedWidth(120)
		.HAlignHeader(HAlign_Right)
		.HAlignCell(HAlign_Right)
		.VAlignCell(VAlign_Center)
		.OnSort(this, &SGraphExecutionStatDisplay::SetInclusiveHeatDisplay));
	HeaderRow->AddColumn(SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::MaxTime))
		.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::MaxTime))
		.FixedWidth(90)
		.HAlignHeader(HAlign_Right)
		.HAlignCell(HAlign_Right)
		.VAlignCell(VAlign_Center)
		.OnSort(this, &SGraphExecutionStatDisplay::SetMaxHeatDisplay));
	HeaderRow->AddColumn(SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::MinTime))
		.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::MinTime))
		.FixedWidth(90)
		.HAlignHeader(HAlign_Right)
		.HAlignCell(HAlign_Right)
		.VAlignCell(VAlign_Center));
	HeaderRow->AddColumn(SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::TotalTime))
		.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::TotalTime))
		.FixedWidth(100)
		.HAlignHeader(HAlign_Right)
		.HAlignCell(HAlign_Right)
		.VAlignCell(VAlign_Center)
		.OnSort(this, &SGraphExecutionStatDisplay::SetTotalTimeHeatDisplay));
	HeaderRow->AddColumn(SHeaderRow::Column(SProfilerStatRow::GetStatName(EBlueprintProfilerStat::Samples))
		.DefaultLabel(SProfilerStatRow::GetStatText(EBlueprintProfilerStat::Samples))
		.FixedWidth(100)
		.HAlignHeader(HAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center));
	// Return the header row.
	return HeaderRow;
}

void SGraphExecutionStatDisplay::SetAverageHeatDisplay(EColumnSortPriority::Type /*NotUsed*/,const FName& /*NotUsed*/, EColumnSortMode::Type /*NotUsed*/) const
{
	UBlueprintProfilerSettings* ProfilerSettings = GetMutableDefault<UBlueprintProfilerSettings>();
	const bool bDisable = ProfilerSettings->GraphNodeHeatMapDisplayMode == EBlueprintProfilerHeatMapDisplayMode::Average;
	ProfilerSettings->GraphNodeHeatMapDisplayMode = bDisable ? EBlueprintProfilerHeatMapDisplayMode::None : EBlueprintProfilerHeatMapDisplayMode::Average;
}

void SGraphExecutionStatDisplay::SetInclusiveHeatDisplay(EColumnSortPriority::Type /*NotUsed*/,const FName& /*NotUsed*/, EColumnSortMode::Type /*NotUsed*/) const
{
	UBlueprintProfilerSettings* ProfilerSettings = GetMutableDefault<UBlueprintProfilerSettings>();
	const bool bDisable = ProfilerSettings->GraphNodeHeatMapDisplayMode == EBlueprintProfilerHeatMapDisplayMode::Inclusive;
	ProfilerSettings->GraphNodeHeatMapDisplayMode = bDisable ? EBlueprintProfilerHeatMapDisplayMode::None : EBlueprintProfilerHeatMapDisplayMode::Inclusive;
}

void SGraphExecutionStatDisplay::SetMaxHeatDisplay(EColumnSortPriority::Type /*NotUsed*/,const FName& /*NotUsed*/, EColumnSortMode::Type /*NotUsed*/) const
{
	UBlueprintProfilerSettings* ProfilerSettings = GetMutableDefault<UBlueprintProfilerSettings>();
	const bool bDisable = ProfilerSettings->GraphNodeHeatMapDisplayMode == EBlueprintProfilerHeatMapDisplayMode::MaxTiming;
	ProfilerSettings->GraphNodeHeatMapDisplayMode = bDisable ? EBlueprintProfilerHeatMapDisplayMode::None : EBlueprintProfilerHeatMapDisplayMode::MaxTiming;
}

void SGraphExecutionStatDisplay::SetTotalTimeHeatDisplay(EColumnSortPriority::Type /*NotUsed*/,const FName& /*NotUsed*/, EColumnSortMode::Type /*NotUsed*/) const
{
	UBlueprintProfilerSettings* ProfilerSettings = GetMutableDefault<UBlueprintProfilerSettings>();
	const bool bDisable = ProfilerSettings->GraphNodeHeatMapDisplayMode == EBlueprintProfilerHeatMapDisplayMode::Total;
	ProfilerSettings->GraphNodeHeatMapDisplayMode = bDisable ? EBlueprintProfilerHeatMapDisplayMode::None : EBlueprintProfilerHeatMapDisplayMode::Total;
}

void SGraphExecutionStatDisplay::OnGraphLayoutChanged(TWeakObjectPtr<UBlueprint> Blueprint)
{
	if (CurrentBlueprint == Blueprint)
	{
		// Force a stat update.
		DisplayOptions->SetStateModified();
	}
}

void SGraphExecutionStatDisplay::OnToggleProfiler(bool bEnabled)
{
	// Force a stat update.
	DisplayOptions->SetStateModified();
}

TSharedRef<ITableRow> SGraphExecutionStatDisplay::OnGenerateRow(FBPStatWidgetPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SProfilerStatRow, OwnerTable, InItem);
}

void SGraphExecutionStatDisplay::OnGetChildren(FBPStatWidgetPtr InParent, TArray<FBPStatWidgetPtr>& OutChildren)
{
	if (InParent.IsValid())
	{
		InParent->GatherChildren(OutChildren);
	}
}

void SGraphExecutionStatDisplay::OnDoubleClickStatistic(FBPStatWidgetPtr Item)
{
	if (Item.IsValid())
	{
		Item->ExpandWidgetState(ExecutionStatTree, !Item->GetExpansionState());
	}
}

void SGraphExecutionStatDisplay::OnStatisticExpansionChanged(FBPStatWidgetPtr Item, bool bExpanded)
{
	if (Item.IsValid())
	{
		Item->SetExpansionState(bExpanded);
	}
}

void SGraphExecutionStatDisplay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		if (IBlueprintProfilerInterface* Profiler = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler"))
		{
			// Find and process execution path timing data.
			if (BlueprintEditor.IsValid())
			{
				const TArray<UObject*>* Blueprints = BlueprintEditor.Pin()->GetObjectsCurrentlyBeingEdited();

				if (Blueprints->Num() == 1)
				{
					UBlueprint* NewBlueprint = Cast<UBlueprint>((*Blueprints)[0]);
					UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(NewBlueprint->GeneratedClass);
					TWeakObjectPtr<const UObject> NewInstance = NewBlueprint->GetObjectBeingDebugged();
					TSharedPtr<FBlueprintExecutionContext> BlueprintExecContext = Profiler->FindBlueprintContext(BPGC->GetPathName());

					if (BlueprintExecContext.IsValid())
					{
						// Check blueprint
						if (CurrentBlueprint.Get() != NewBlueprint)
						{
							CurrentBlueprint = NewBlueprint;
							DisplayOptions->SetStateModified();
						}
						// Find Current instance
						FName NewInstancePath = BlueprintExecContext->RemapInstancePath(FName(*NewInstance->GetPathName()));
						DisplayOptions->SetActiveInstance(NewInstancePath);
						// Find active graph to filter on
						TSharedPtr<SDockTab> DockTab = BlueprintEditor.Pin()->DocumentManager->GetActiveTab();
						if (DockTab.IsValid())
						{
							TSharedRef<SGraphEditor> ActiveGraphEditor = StaticCastSharedRef<SGraphEditor>(DockTab->GetContent());
							const UEdGraph* CurrentGraph = ActiveGraphEditor->GetCurrentGraph();
							DisplayOptions->SetActiveGraph(CurrentGraph ? CurrentGraph->GetFName() : NAME_None);
						}
						if (DisplayOptions->IsStateModified())
						{
							ConstructTreeView();
							TSharedPtr<FScriptExecutionBlueprint> BlueprintExecNode = BlueprintExecContext->GetBlueprintExecNode();
							RootTreeItems.Reset(0);

							if (BlueprintExecNode.IsValid())
							{
								// Build Instance widget execution trees
								DisplayOptions->ClearFlags(FBlueprintProfilerStatOptions::Modified);
								// Cache Active blueprint and Instance
								const FName CurrentInstancePath = DisplayOptions->GetActiveInstance();

								if (DisplayOptions->HasFlags(FBlueprintProfilerStatOptions::DisplayByInstance))
								{
									if (DisplayOptions->HasFlags(FBlueprintProfilerStatOptions::ScopeToDebugInstance))
									{
										if (CurrentInstancePath != SPDN_Blueprint)
										{
											TSharedPtr<FScriptExecutionNode> InstanceStat = BlueprintExecNode->GetInstanceByName(CurrentInstancePath);
											if (InstanceStat.IsValid())
											{
												FTracePath InstanceTracePath;
												DisplayOptions->SetActiveInstance(InstanceStat->GetName());
												FBPStatWidgetPtr InstanceWidget = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(InstanceStat, InstanceTracePath));
												InstanceWidget->GenerateExecNodeWidgets(DisplayOptions);
												RootTreeItems.Add(InstanceWidget);
											}
										}
									}
									else
									{
										for (int32 InstanceIdx = 0; InstanceIdx < BlueprintExecNode->GetInstanceCount(); ++InstanceIdx)
										{
											TSharedPtr<FScriptExecutionNode> InstanceStat = BlueprintExecNode->GetInstanceByIndex(InstanceIdx);
											FTracePath InstanceTracePath;
											DisplayOptions->SetActiveInstance(InstanceStat->GetName());
											FBPStatWidgetPtr InstanceWidget = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(InstanceStat, InstanceTracePath));
											InstanceWidget->GenerateExecNodeWidgets(DisplayOptions);
											RootTreeItems.Add(InstanceWidget);
										}
									}
								}
								else
								{
									FTracePath TracePath;
									FBPStatWidgetPtr BlueprintWidget = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(BlueprintExecNode, TracePath));
									BlueprintWidget->GenerateExecNodeWidgets(DisplayOptions);
									RootTreeItems.Add(BlueprintWidget);
								}
							}
							// Refresh Tree
							if (ExecutionStatTree.IsValid())
							{
								ExecutionStatTree->RequestTreeRefresh();
								for (auto Iter : RootTreeItems)
								{
									Iter->ProbeChildWidgetExpansionStates();
									Iter->RestoreWidgetExpansionState(ExecutionStatTree);
								}
							}
						}
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
