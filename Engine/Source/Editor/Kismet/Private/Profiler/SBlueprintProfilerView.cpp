// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SBlueprintProfilerView.h"

// Profiler View types
#include "SProfilerOverview.h"
#include "SLeastPerformantDisplay.h"
#include "SGraphExecutionStatDisplay.h"
#include "ScriptPerfData.h"

#define LOCTEXT_NAMESPACE "BlueprintProfilerView"

//////////////////////////////////////////////////////////////////////////
// SBlueprintProfilerView

SBlueprintProfilerView::~SBlueprintProfilerView()
{
	// Register for Profiler toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.RemoveAll(this);
}

void SBlueprintProfilerView::Construct(const FArguments& InArgs)
{
	CurrentViewType = InArgs._ProfileViewType;
	BlueprintEditor = InArgs._AssetEditor;
	// Register for Profiler toggle events
	FBlueprintCoreDelegates::OnToggleScriptProfiler.AddSP(this, &SBlueprintProfilerView::OnToggleProfiler);
	// Initialise the number format.
	FNumberFormattingOptions StatisticNumberFormat;
	StatisticNumberFormat.MinimumFractionalDigits = 4;
	StatisticNumberFormat.MaximumFractionalDigits = 4;
	StatisticNumberFormat.UseGrouping = false;
	FScriptPerfData::SetNumberFormattingForStats(StatisticNumberFormat);
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
	// Create the profiler view widgets
	UpdateActiveProfilerWidget();
}

void SBlueprintProfilerView::OnToggleProfiler(bool bEnabled)
{
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		StatusText = bEnabled ? LOCTEXT("ProfilerNoDataText", "The Blueprint Profiler is active but does not currently have any data to display") :
								LOCTEXT("ProfilerInactiveText", "The Blueprint Profiler is currently Inactive");
	}
	else
	{
		StatusText = LOCTEXT("DisabledProfilerText", "The Blueprint Profiler is experimental and is currently not enabled in the editor preferences");
	}
	UpdateActiveProfilerWidget();
}

void SBlueprintProfilerView::UpdateActiveProfilerWidget()
{
		FMenuBuilder ViewComboContent(true, NULL);
		ViewComboContent.AddMenuEntry(	LOCTEXT("OverviewViewType", "Profiler Overview"), 
										LOCTEXT("OverviewViewTypeDesc", "Displays High Level Profiling Information"), 
										FSlateIcon(), 
										FExecuteAction::CreateSP(this, &SBlueprintProfilerView::OnViewSelectionChanged, EBlueprintPerfViewType::Overview),
										NAME_None, 
										EUserInterfaceActionType::Button);
		ViewComboContent.AddMenuEntry(	LOCTEXT("ExecutionGraphViewType", "Execution Graph"), 
										LOCTEXT("ExecutionGraphViewTypeDesc", "Displays the Execution Graph with Statistics"), 
										FSlateIcon(), 
										FExecuteAction::CreateSP(this, &SBlueprintProfilerView::OnViewSelectionChanged, EBlueprintPerfViewType::ExecutionGraph),
										NAME_None, 
										EUserInterfaceActionType::Button);
		ViewComboContent.AddMenuEntry(	LOCTEXT("LeastPerformantViewType", "Least Performant List"), 
										LOCTEXT("LeastPerformantViewTypeDesc", "Displays a list of Least Performant Areas of the Blueprint"), 
										FSlateIcon(), 
										FExecuteAction::CreateSP(this, &SBlueprintProfilerView::OnViewSelectionChanged, EBlueprintPerfViewType::LeastPerformant),
										NAME_None, 
										EUserInterfaceActionType::Button);

		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("BlueprintProfiler.ViewToolBar"))
				.Padding(0)
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(5,0))
					[
						SAssignNew(ViewComboButton, SComboButton)
						.ForegroundColor(this, &SBlueprintProfilerView::GetViewButtonForegroundColor)
						.ToolTipText(LOCTEXT("BlueprintProfilerViewType", "View Type"))
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.ContentPadding(2)
						.MenuContent()
						[
							ViewComboContent.MakeWidget()
						]
						.ButtonContent()
						[
							CreateViewButton()
						]
					]
				]
			]
			+SVerticalBox::Slot()
			[
				SNew(SBorder)
				.Padding(FMargin(0,2,0,0))
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				[
					CreateActiveStatisticWidget()
				]
			]
		];
	}

FText SBlueprintProfilerView::GetCurrentViewText() const
{
	switch(CurrentViewType)
	{
		case EBlueprintPerfViewType::Overview:			return LOCTEXT("OverviewLabel", "Profiler Overview");
		case EBlueprintPerfViewType::ExecutionGraph:	return LOCTEXT("ExecutionGraphLabel", "Execution Graph");
		case EBlueprintPerfViewType::LeastPerformant:	return LOCTEXT("LeastPerformantLabel", "Least Performant");
	}
	return LOCTEXT("UnknownViewLabel", "Unknown ViewType");
}

TSharedRef<SWidget> SBlueprintProfilerView::CreateViewButton() const
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image( FEditorStyle::GetBrush("GenericViewButton") )
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2, 0, 0, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(GetCurrentViewText())
		];
}

FSlateColor SBlueprintProfilerView::GetViewButtonForegroundColor() const
{
	static const FName InvertedForegroundName("InvertedForeground");
	static const FName DefaultForegroundName("DefaultForeground");
	return ViewComboButton->IsHovered() ? FEditorStyle::GetSlateColor(InvertedForegroundName) : FEditorStyle::GetSlateColor(DefaultForegroundName);
}

void SBlueprintProfilerView::OnViewSelectionChanged(const EBlueprintPerfViewType::Type NewViewType)
{
	if (CurrentViewType != NewViewType)
	{
		CurrentViewType = NewViewType;
		UpdateActiveProfilerWidget();
	}
}

TSharedRef<SWidget> SBlueprintProfilerView::CreateActiveStatisticWidget()
{
	// Get profiler status
	EBlueprintPerfViewType::Type NewViewType = EBlueprintPerfViewType::None;
	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintPerformanceAnalysisTools)
	{
		IBlueprintProfilerInterface* ProfilerInterface = FModuleManager::GetModulePtr<IBlueprintProfilerInterface>("BlueprintProfiler");
		if (ProfilerInterface && ProfilerInterface->IsProfilerEnabled())
		{
			NewViewType = CurrentViewType;
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
				.AssetEditor(BlueprintEditor);
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
						.BorderImage(FEditorStyle::GetBrush("NoBorder"))
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
