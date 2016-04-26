// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BPProfilerStatisticWidgets.h"
#include "SGraphExecutionStatDisplay.h"
#include "BlueprintEditor.h"
#include "Public/Profiler/EventExecution.h"
#include "SDockTab.h"
#include "Developer/BlueprintProfiler/Public/ScriptInstrumentationPlayback.h"

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
	// Register for profiling toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.AddSP(this, &SGraphExecutionStatDisplay::OnToggleProfiler);
	// Remove delegate for graph structural changes
	if (IBlueprintProfilerInterface* Profiler = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler"))
	{
		Profiler->GetGraphLayoutChangedDelegate().AddSP(this, &SGraphExecutionStatDisplay::OnGraphLayoutChanged);
	}
	// Set Default States
	bDisplayInstances = false;
	bScopeToActiveDebugInstance = false;
	bAutoExpandStats = false;
	bFilterEventsToGraph = true;

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
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(5,0))
					[
						SNew(SCheckBox)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("FilterToGraph", "Filter Events to Current Graph"))
						]
						.IsChecked(this, &SGraphExecutionStatDisplay::GetGraphFilterCheck)
						.OnCheckStateChanged(this, &SGraphExecutionStatDisplay::OnGraphFilterCheck)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(5,0))
					[
						SNew(SCheckBox)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ShowInstancesCheck", "Display By Instance"))
						]
						.IsChecked(this, &SGraphExecutionStatDisplay::GetDisplayByInstanceCheck)
						.OnCheckStateChanged(this, &SGraphExecutionStatDisplay::OnDisplayByInstanceCheck)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(5,0))
					[
						SNew(SCheckBox)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("InstanceFilterCheck", "Scope To Debug Filter"))
						]
						.IsChecked(this, &SGraphExecutionStatDisplay::GetInstanceFilterCheck)
						.OnCheckStateChanged(this, &SGraphExecutionStatDisplay::OnInstanceFilterCheck)
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(5,0))
					[
						SNew(SCheckBox)
						.Content()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AutoItemExpansion", "Auto Expand Statistics"))
						]
						.IsChecked(this, &SGraphExecutionStatDisplay::GetAutoExpansionCheck)
						.OnCheckStateChanged(this, &SGraphExecutionStatDisplay::OnAutoExpansionCheck)
					]
				]
			]
		]
		+SVerticalBox::Slot()
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
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

void SGraphExecutionStatDisplay::OnGraphLayoutChanged(TWeakObjectPtr<UBlueprint> Blueprint)
{
	if (CurrentBlueprint == Blueprint)
	{
		// Force a stat update.
		CurrentBlueprint.Reset();
	}
}

void SGraphExecutionStatDisplay::OnToggleProfiler(bool bEnabled)
{
	// Force a stat update.
	CurrentBlueprint.Reset();
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

void SGraphExecutionStatDisplay::OnGraphFilterCheck(ECheckBoxState NewState)
{
	const bool bOldState = bFilterEventsToGraph;
	bFilterEventsToGraph = (NewState == ECheckBoxState::Checked) ? true : false;
	// Force Update.
	if (bFilterEventsToGraph != bOldState)
	{
		CurrentBlueprint.Reset();
	}
}

void SGraphExecutionStatDisplay::OnInstanceFilterCheck(ECheckBoxState NewState)
{
	const bool bOldState = bScopeToActiveDebugInstance;
	bScopeToActiveDebugInstance = (NewState == ECheckBoxState::Checked) ? true : false;
	// Force Update.
	if (bScopeToActiveDebugInstance != bOldState)
	{
		CurrentBlueprint.Reset();
	}
}

void SGraphExecutionStatDisplay::OnDisplayByInstanceCheck(ECheckBoxState NewState)
{
	const bool bOldState = bDisplayInstances;
	bDisplayInstances = (NewState == ECheckBoxState::Checked) ? true : false;
	// Force Update.
	if (bDisplayInstances != bOldState)
	{
		CurrentBlueprint.Reset();
	}
}

void SGraphExecutionStatDisplay::OnAutoExpansionCheck(ECheckBoxState NewState)
{
	const bool bOldState = bAutoExpandStats;
	bAutoExpandStats = (NewState == ECheckBoxState::Checked) ? true : false;
	// Force Update.
	if (bAutoExpandStats != bOldState)
	{
		if (bAutoExpandStats)
		{
			for (auto Iter : RootTreeItems)
			{
				Iter->ExpandWidgetState(ExecutionStatTree, bAutoExpandStats);
			}
		}
		else
		{
			for (auto Iter : RootTreeItems)
			{
				Iter->RestoreWidgetExpansionState(ExecutionStatTree);
			}
		}
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
	if (!bAutoExpandStats && Item.IsValid())
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
					TSharedPtr<FBlueprintExecutionContext> BlueprintExecContext = Profiler->GetBlueprintContext(BPGC->GetPathName());

					if (BlueprintExecContext.IsValid())
					{
						FName NewInstancePath = BlueprintExecContext->RemapInstancePath(FName(*NewInstance->GetPathName()));
						const bool bBlueprintChanged = NewBlueprint != CurrentBlueprint.Get();
						const bool bInstanceChanged = bScopeToActiveDebugInstance && NewInstancePath != CurrentInstancePath;
						// Find active graph to filter on
						bool bGraphFilterChanged = false;
						TSharedPtr<SDockTab> DockTab = BlueprintEditor.Pin()->DocumentManager->GetActiveTab();
						if (DockTab.IsValid())
						{
							TSharedRef<SGraphEditor> ActiveGraphEditor = StaticCastSharedRef<SGraphEditor>(DockTab->GetContent());
							bGraphFilterChanged = ActiveGraphEditor->GetCurrentGraph() != CurrentGraph.Get();
							CurrentGraph = ActiveGraphEditor->GetCurrentGraph();
						}

						if (bBlueprintChanged||bInstanceChanged||bGraphFilterChanged)
						{
							const FString BlueprintPath = NewBlueprint->GeneratedClass->GetPathName();
							TSharedPtr<FScriptExecutionBlueprint> BlueprintExecNode = BlueprintExecContext->GetBlueprintExecNode();
							RootTreeItems.Reset(0);

							if (BlueprintExecNode.IsValid())
							{
								// Cache Active blueprint and Instance
								CurrentBlueprint = NewBlueprint;
								CurrentInstancePath = NewInstancePath;
								// Build Instance widget execution trees
								FName GraphFilter = CurrentGraph.IsValid() && bFilterEventsToGraph ? CurrentGraph.Get()->GetFName() : NAME_None;

								if (bDisplayInstances && bScopeToActiveDebugInstance)
								{
									if (CurrentInstancePath != NAME_None)
									{
										TSharedPtr<FScriptExecutionNode> InstanceStat = BlueprintExecNode->GetInstanceByName(CurrentInstancePath);
										if (InstanceStat.IsValid())
										{
											FTracePath InstanceTracePath;
											FBPStatWidgetPtr InstanceWidget = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(InstanceStat, InstanceTracePath));
											InstanceWidget->GenerateExecNodeWidgets(InstanceStat->GetName(), GraphFilter);
											RootTreeItems.Add(InstanceWidget);
										}
									}
								}
								else if (bDisplayInstances)
								{
									for (int32 InstanceIdx = 0; InstanceIdx < BlueprintExecNode->GetInstanceCount(); ++InstanceIdx)
									{
										TSharedPtr<FScriptExecutionNode> InstanceStat = BlueprintExecNode->GetInstanceByIndex(InstanceIdx);
										FTracePath InstanceTracePath;
										FBPStatWidgetPtr InstanceWidget = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(InstanceStat, InstanceTracePath));
										InstanceWidget->GenerateExecNodeWidgets(InstanceStat->GetName(), GraphFilter);
										RootTreeItems.Add(InstanceWidget);
									}
								}
								else
								{
									FTracePath TracePath;
									FBPStatWidgetPtr BlueprintWidget = MakeShareable<FBPProfilerStatWidget>(new FBPProfilerStatWidget(BlueprintExecNode, TracePath));
									BlueprintWidget->GenerateExecNodeWidgets(NAME_None, GraphFilter);
									RootTreeItems.Add(BlueprintWidget);
								}
							}
							// Refresh Tree
							if (ExecutionStatTree.IsValid())
							{
								ExecutionStatTree->RequestTreeRefresh();
								if (bAutoExpandStats)
								{
									for (auto Iter : RootTreeItems)
									{
										Iter->ExpandWidgetState(ExecutionStatTree, bAutoExpandStats);
									}
								}
								else
								{
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
}

#undef LOCTEXT_NAMESPACE
