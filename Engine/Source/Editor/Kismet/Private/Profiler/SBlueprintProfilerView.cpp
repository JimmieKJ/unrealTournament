// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SBlueprintProfilerView.h"
#include "SBlueprintProfilerToolbar.h"
#include "BPProfilerStatisticWidgets.h"
#include "BlueprintEditor.h"

// Profiler View types
#include "SProfilerOverview.h"
#include "SLeastPerformantDisplay.h"
#include "SGraphExecutionStatDisplay.h"

#include "ScriptPerfData.h"
#include "SNumericEntryBox.h"
#include "Editor/UnrealEd/Classes/Settings/EditorExperimentalSettings.h"

#define LOCTEXT_NAMESPACE "BlueprintProfilerView"

//////////////////////////////////////////////////////////////////////////
// SBlueprintProfilerView

SBlueprintProfilerView::~SBlueprintProfilerView()
{
	// Register for Profiler toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.RemoveAll(this);
	// Remove delegate for graph structural changes
	if (IBlueprintProfilerInterface* Profiler = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler"))
	{
		Profiler->GetGraphLayoutChangedDelegate().RemoveAll(this);
	}
}

void SBlueprintProfilerView::Construct(const FArguments& InArgs)
{
	BlueprintEditor = InArgs._AssetEditor;
	// Register for Profiler toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.AddSP(this, &SBlueprintProfilerView::OnToggleProfiler);
	// Remove delegate for graph structural changes
	IBlueprintProfilerInterface& ProfilerModule = FModuleManager::LoadModuleChecked<IBlueprintProfilerInterface>("BlueprintProfiler");
	ProfilerModule.GetGraphLayoutChangedDelegate().AddSP(this, &SBlueprintProfilerView::OnGraphLayoutChanged);
	// Initialise the number format.
	FNumberFormattingOptions StatisticNumberFormat;
	StatisticNumberFormat.MinimumFractionalDigits = 4;
	StatisticNumberFormat.MaximumFractionalDigits = 4;
	StatisticNumberFormat.UseGrouping = false;
	FNumberFormattingOptions TimeNumberFormat;
	TimeNumberFormat.MinimumFractionalDigits = 1;
	TimeNumberFormat.MaximumFractionalDigits = 1;
	TimeNumberFormat.UseGrouping = false;
	FScriptPerfData::SetNumberFormattingForStats(StatisticNumberFormat, TimeNumberFormat);
	// Create the display text for the user
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		IBlueprintProfilerInterface* ProfilerInterface = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler");
		const bool bProfilerEnabled	= ProfilerInterface && ProfilerInterface->IsProfilerEnabled();
		StatusText = bProfilerEnabled ? LOCTEXT("ProfilerNoDataText", "The Blueprint Profiler is active but does not currently have any data to display") :
										LOCTEXT("ProfilerInactiveText", "The Blueprint Profiler is currently Inactive");
	}
	else
	{
		StatusText = LOCTEXT("DisabledProfilerText", "The Blueprint Profiler is experimental and is currently not enabled in the editor preferences");
	}
	// Create Display Options
	if (!DisplayOptions.IsValid())
	{
		DisplayOptions = MakeShareable(new FBlueprintProfilerStatOptions);
	}
	// Create the toolbar
	SAssignNew(ProfilerToolbar, SBlueprintProfilerToolbar)
		.ProfileViewType(InArgs._ProfileViewType)
		.DisplayOptions(DisplayOptions)
		.OnViewChanged(this, &SBlueprintProfilerView::OnViewChanged);
	// Create the profiler view widgets
	UpdateActiveProfilerWidget();
}

void SBlueprintProfilerView::OnToggleProfiler(bool bEnabled)
{
	UpdateStatusMessage();
	UpdateActiveProfilerWidget();
}

void SBlueprintProfilerView::OnGraphLayoutChanged(TWeakObjectPtr<UBlueprint> Blueprint)
{
	UBlueprint* CurrentBP = BlueprintEditor.IsValid() ? BlueprintEditor.Pin()->GetBlueprintObj() : nullptr;
	if (CurrentBP == Blueprint)
	{
		UpdateStatusMessage();
		UpdateActiveProfilerWidget();
	}
}

void SBlueprintProfilerView::UpdateStatusMessage()
{
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		IBlueprintProfilerInterface* ProfilerInterface = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler");
		if (ProfilerInterface && ProfilerInterface->IsProfilerEnabled())
		{
			UBlueprint* CurrentBP = BlueprintEditor.IsValid() ? BlueprintEditor.Pin()->GetBlueprintObj() : nullptr;
			if (CurrentBP && CurrentBP->GeneratedClass->HasInstrumentation())
			{
				StatusText = LOCTEXT("ProfilerNoDataText", "The Blueprint Profiler is active but does not currently have any data to display");
			}
			else
			{
				StatusText = LOCTEXT("ProfilerNoInstrumentationText", "The Blueprint Profiler is active but the current blueprint does not have any instrumentation");
			}
		}
		else
		{
			StatusText = LOCTEXT("ProfilerInactiveText", "The Blueprint Profiler is currently Inactive");
		}								
	}
	else
	{
		StatusText = LOCTEXT("DisabledProfilerText", "The Blueprint Profiler is experimental and is currently not enabled in the editor preferences");
	}
}

void SBlueprintProfilerView::UpdateActiveProfilerWidget()
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0)
		[
			ProfilerToolbar.ToSharedRef()
		]
		+SVerticalBox::Slot()
		[
			SNew(SBorder)
			.Padding(FMargin(0,2,0,0))
			.BorderImage(FEditorStyle::GetBrush("BlueprintProfiler.ViewContent"))
			[
				CreateActiveStatisticWidget()
			]
		]
	];
}

void SBlueprintProfilerView::OnViewChanged()
{
	UpdateActiveProfilerWidget();
}

TSharedRef<SWidget> SBlueprintProfilerView::CreateActiveStatisticWidget()
{
	// Get profiler status
	EBlueprintPerfViewType::Type NewViewType = EBlueprintPerfViewType::None;
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		IBlueprintProfilerInterface& ProfilerModule = FModuleManager::LoadModuleChecked<IBlueprintProfilerInterface>("BlueprintProfiler");
		if (ProfilerModule.IsProfilerEnabled())
		{
			UBlueprint* ActiveBlueprint = BlueprintEditor.IsValid() ? BlueprintEditor.Pin()->GetBlueprintObj() : nullptr;
			if (ActiveBlueprint && ActiveBlueprint->GeneratedClass->HasInstrumentation() && ProfilerToolbar.IsValid())
			{
				NewViewType = ProfilerToolbar->GetActiveViewType();
			}
		}
	}
	switch(NewViewType)
	{
		case EBlueprintPerfViewType::Overview:
		{
			return SNew(SProfilerOverview)
				.AssetEditor(BlueprintEditor);
		}
		case EBlueprintPerfViewType::ExecutionGraph:
		{
			return SNew(SGraphExecutionStatDisplay)
				.AssetEditor(BlueprintEditor)
				.DisplayOptions(DisplayOptions);
		}
		case EBlueprintPerfViewType::LeastPerformant:
		{
			return SNew(SLeastPerformantDisplay)
				.AssetEditor(BlueprintEditor);
		}
		default:
		{
			// Default place holder
			return SNew(SVerticalBox)
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
							.Text(this, &SBlueprintProfilerView::GetProfilerStatusText)
						]
					]
				];
		}
	}
}

#undef LOCTEXT_NAMESPACE
