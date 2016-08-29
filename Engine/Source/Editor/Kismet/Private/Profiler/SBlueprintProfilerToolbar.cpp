// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintProfilerModule.h"
#include "SBlueprintProfilerToolbar.h"
#include "EventExecution.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "BlueprintProfilerToolbar"

//////////////////////////////////////////////////////////////////////////
// FBPProfilerStatOptions

FBlueprintProfilerStatOptions::FBlueprintProfilerStatOptions()
	: ActiveInstance(NAME_None)
	, ActiveGraph(NAME_None)
	, Flags(Modified)
{
	const UBlueprintProfilerSettings* ProfilerSettings = GetDefault<UBlueprintProfilerSettings>();

	if (ProfilerSettings->bDisplayByInstance)
	{
		Flags |= DisplayByInstance;
	}

	if (ProfilerSettings->bScopeToDebugInstance)
	{
		Flags |= ScopeToDebugInstance;
	}

	if (ProfilerSettings->bGraphFilter)
	{
		Flags |= GraphFilter;
	}

	if (ProfilerSettings->bDisplayPure)
	{
		Flags |= DisplayPure;
	}

	if (ProfilerSettings->bDisplayInheritedEvents)
	{
		Flags |= DisplayInheritedEvents;
	}

	if (ProfilerSettings->bAverageBlueprintStats)
	{
		Flags |= AverageBlueprintStats;
	}
}

void FBlueprintProfilerStatOptions::SetActiveInstance(const FName InstanceName)
{
	if (ActiveInstance != InstanceName && HasFlags(ScopeToDebugInstance))
	{
		Flags |= Modified;
	}
	ActiveInstance = InstanceName;
}

void FBlueprintProfilerStatOptions::SetActiveGraph(const FName GraphName)
{
	if (ActiveGraph != GraphName && HasFlags(GraphFilter))
	{
		Flags |= Modified;
	}
	ActiveGraph = GraphName;
}

TSharedRef<SWidget> FBlueprintProfilerStatOptions::CreateComboContent()
{
	return	
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5,2))
			[
				SNew(SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FilterToGraph_Label", "Filter to Graph"))
					.ToolTipText(LOCTEXT("FilterToGraph_Tooltip", "Only Display events that are applicable to the current graph."))
				]
				.IsChecked<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::GetChecked, GraphFilter)
				.OnCheckStateChanged<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::OnChecked, GraphFilter)
			]
		]
		+SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5,2))
			[
				SNew(SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AverageBlueprintStats_Label", "Use Averaged Blueprint Stats"))
					.ToolTipText(LOCTEXT("AverageBlueprintStats_Tooltip", "Blueprint Statistics are Averaged or Summed By Instance."))
				]
				.IsChecked<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::GetChecked, AverageBlueprintStats)
				.OnCheckStateChanged<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::OnChecked, AverageBlueprintStats)
			]
		]
#if 0 // Removing pure timings until we have an advanced display options.
		+SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5,2))
			[
				SNew(SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DisplayPureStats_Label", "Pure Timings"))
					.ToolTipText(LOCTEXT("DisplayPureStats_Tooltip", "Display timings from pure nodes."))
				]
				.IsChecked<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::GetChecked, DisplayPure)
				.OnCheckStateChanged<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::OnChecked, DisplayPure)
			]
		]
#endif
		+SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5,2))
			[
				SNew(SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DisplayInheritedEvents_Label", "Show Inherited Blueprint Events"))
					.ToolTipText(LOCTEXT("DisplayInheritedEvents_Tooltip", "Display events for parent blueprints."))
				]
				.IsChecked<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::GetChecked, DisplayInheritedEvents)
				.OnCheckStateChanged<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::OnChecked, DisplayInheritedEvents)
			]
		]
		+SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5,2))
			[
				SNew(SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowInstancesCheck_Label", "Show Instances"))
					.ToolTipText(LOCTEXT("ShowInstancesCheck_Tooltip", "Display statistics by instance rather than by blueprint."))
				]
				.IsChecked<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::GetChecked, DisplayByInstance)
				.OnCheckStateChanged<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::OnChecked, DisplayByInstance)
			]
		]
		+SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.Visibility<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::GetVisibility, ScopeToDebugInstance)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5,2))
			[
				SNew(SCheckBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InstanceFilterCheck_Label", "Debug Filter Scope"))
					.ToolTipText(LOCTEXT("InstanceFilterCheck_Tooltip", "Restrict the displayed instances to match the blueprint debug filter."))
				]
				.IsChecked<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::GetChecked, ScopeToDebugInstance)
				.OnCheckStateChanged<FBlueprintProfilerStatOptions, uint32>(this, &FBlueprintProfilerStatOptions::OnChecked, ScopeToDebugInstance)
			]
		];
}

ECheckBoxState FBlueprintProfilerStatOptions::GetChecked(const uint32 FlagsIn) const
{
	ECheckBoxState CheckedState;
	if (FlagsIn & ScopeToDebugInstance)
	{
		if (HasFlags(DisplayByInstance))
		{
			CheckedState = HasFlags(ScopeToDebugInstance) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		else
		{
			CheckedState = ECheckBoxState::Undetermined;
		}
	}
	else
	{
		CheckedState = HasAllFlags(FlagsIn) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return CheckedState;
}

void FBlueprintProfilerStatOptions::OnChecked(ECheckBoxState NewState, const uint32 FlagsIn)
{
	if (NewState == ECheckBoxState::Checked)
	{
		Flags |= FlagsIn;
	}
	else
	{
		Flags &= ~FlagsIn;
	}

	if (FlagsIn & AverageBlueprintStats)
	{
		FScriptPerfData::EnableBlueprintStatAverage(NewState == ECheckBoxState::Checked);
	}

	Flags |= Modified;

	UBlueprintProfilerSettings* ProfilerSettings = GetMutableDefault<UBlueprintProfilerSettings>();

	if (FlagsIn & DisplayByInstance)
	{
		ProfilerSettings->bDisplayByInstance = !!(Flags & DisplayByInstance);
	}

	if (FlagsIn & ScopeToDebugInstance)
	{
		ProfilerSettings->bScopeToDebugInstance = !!(Flags & ScopeToDebugInstance);
	}

	if (FlagsIn & GraphFilter)
	{
		ProfilerSettings->bGraphFilter = !!(Flags & GraphFilter);
	}

	if (FlagsIn & DisplayPure)
	{
		ProfilerSettings->bDisplayPure = !!(Flags & DisplayPure);
	}

	if (FlagsIn & DisplayInheritedEvents)
	{
		ProfilerSettings->bDisplayInheritedEvents = !!(Flags & DisplayInheritedEvents);
	}

	if (FlagsIn & AverageBlueprintStats)
	{
		ProfilerSettings->bAverageBlueprintStats = !!(Flags & AverageBlueprintStats);
	}

	ProfilerSettings->SaveConfig();
}

EVisibility FBlueprintProfilerStatOptions::GetVisibility(const uint32 FlagsIn) const
{
	EVisibility Result = EVisibility::Hidden;
	if (FlagsIn & ScopeToDebugInstance)
	{
		Result = (Flags & DisplayByInstance) != 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return Result;
}

bool FBlueprintProfilerStatOptions::IsFiltered(TSharedPtr<FScriptExecutionNode> Node) const
{
	bool bFilteredOut = !HasFlags(EDisplayFlags::DisplayPure) && Node->HasFlags(EScriptExecutionNodeFlags::PureStats);
	if (Node->IsEvent())
	{
		if (HasFlags(EDisplayFlags::GraphFilter))
		{
			if (Node->GetGraphName() == UEdGraphSchema_K2::FN_UserConstructionScript)
			{
				bFilteredOut = ActiveGraph != UEdGraphSchema_K2::FN_UserConstructionScript;
			}
			else
			{
				bFilteredOut = ActiveGraph == UEdGraphSchema_K2::FN_UserConstructionScript;
			}
		}
		if (!HasFlags(EDisplayFlags::DisplayInheritedEvents) && Node->HasFlags(EScriptExecutionNodeFlags::InheritedEvent))
		{
			bFilteredOut = true;
		}
	}
	return bFilteredOut;
}

//////////////////////////////////////////////////////////////////////////
// SBlueprintProfilerToolbar

void SBlueprintProfilerToolbar::Construct(const FArguments& InArgs)
{
	CurrentViewType = InArgs._ProfileViewType;
	DisplayOptions = InArgs._DisplayOptions;
	
	ViewChangeDelegate = InArgs._OnViewChanged;

	FMenuBuilder ViewComboContent(true, NULL);
	ViewComboContent.AddMenuEntry(	LOCTEXT("OverviewViewType", "Profiler Overview"), 
									LOCTEXT("OverviewViewTypeDesc", "Displays High Level Profiling Information"), 
									FSlateIcon(), 
									FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnViewSelectionChanged, EBlueprintPerfViewType::Overview),
									NAME_None, 
									EUserInterfaceActionType::Button);
	ViewComboContent.AddMenuEntry(	LOCTEXT("ExecutionGraphViewType", "Execution Graph"), 
									LOCTEXT("ExecutionGraphViewTypeDesc", "Displays the Execution Graph with Statistics"), 
									FSlateIcon(), 
									FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnViewSelectionChanged, EBlueprintPerfViewType::ExecutionGraph),
									NAME_None, 
									EUserInterfaceActionType::Button);
	ViewComboContent.AddMenuEntry(	LOCTEXT("LeastPerformantViewType", "Least Performant List"), 
									LOCTEXT("LeastPerformantViewTypeDesc", "Displays a list of Least Performant Areas of the Blueprint"), 
									FSlateIcon(), 
									FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnViewSelectionChanged, EBlueprintPerfViewType::LeastPerformant),
									NAME_None, 
									EUserInterfaceActionType::Button);

	FMenuBuilder HeatMapDisplayModeComboContent(true, nullptr);
	HeatMapDisplayModeComboContent.AddMenuEntry(LOCTEXT("HeatMapDisplayMode_None", "Off"),
		LOCTEXT("HeatMapDisplayMode_None_Desc", "Normal Display (No Heat Map)"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::None),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::None)
		),
		NAME_None,
		EUserInterfaceActionType::Check);
	HeatMapDisplayModeComboContent.AddMenuEntry(LOCTEXT("HeatMapDisplayMode_Average", "Average"),
		LOCTEXT("HeatMapDisplayMode_Average_Desc", "Heat Map - Average Timings"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::Average),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::Average)
		),
		NAME_None,
		EUserInterfaceActionType::Check);
	HeatMapDisplayModeComboContent.AddMenuEntry(LOCTEXT("HeatMapDisplayMode_Inclusive", "Inclusive"),
		LOCTEXT("HeatMapDisplayMode_Inclusive_Desc", "Heat Map - Inclusive Timings"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::Inclusive),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::Inclusive)
		),
		NAME_None,
		EUserInterfaceActionType::Check);
	HeatMapDisplayModeComboContent.AddMenuEntry(LOCTEXT("HeatMapDisplayMode_MaxTiming", "Max Time"),
		LOCTEXT("HeatMapDisplayMode_MaxTiming_Desc", "Heat Map - Max Time"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::MaxTiming),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::MaxTiming)
		),
		NAME_None,
		EUserInterfaceActionType::Check);
	HeatMapDisplayModeComboContent.AddMenuEntry(LOCTEXT("HeatMapDisplayMode_TotalTiming", "Total Time"),
		LOCTEXT("HeatMapDisplayMode_TotalTiming_Desc", "Heat Map - Max Time"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::Total),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected, EHeatmapControlId::NodeHeatmap, EBlueprintProfilerHeatMapDisplayMode::Total)
		),
		NAME_None,
		EUserInterfaceActionType::Check);

	FMenuBuilder WireHeatMapDisplayModeComboContent(true, nullptr);
	WireHeatMapDisplayModeComboContent.AddMenuEntry(LOCTEXT("WireHeatMapDisplayMode_None", "Off"),
		LOCTEXT("HeatMapDisplayMode_None_Desc", "Normal Display (No Heat Map)"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged, EHeatmapControlId::WireHeatmap, EBlueprintProfilerHeatMapDisplayMode::None),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected, EHeatmapControlId::WireHeatmap, EBlueprintProfilerHeatMapDisplayMode::None)
		),
		NAME_None,
		EUserInterfaceActionType::Check);
	WireHeatMapDisplayModeComboContent.AddMenuEntry(LOCTEXT("WireHeatMapDisplayMode_HottestPath", "Hottest Path"),
		LOCTEXT("WireHeatMapDisplayMode_HottestPath_Desc", "Heat Map - Hottest Path"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged, EHeatmapControlId::WireHeatmap, EBlueprintProfilerHeatMapDisplayMode::HottestPath),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected, EHeatmapControlId::WireHeatmap, EBlueprintProfilerHeatMapDisplayMode::HottestPath)
		),
		NAME_None,
		EUserInterfaceActionType::Check);
	WireHeatMapDisplayModeComboContent.AddMenuEntry(LOCTEXT("WireHeatMapDisplayMode_PinToPin", "Pin To Pin Heatmap"),
		LOCTEXT("WireHeatMapDisplayMode_PinToPin_Desc", "Heat Map - Pin To Pin"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged, EHeatmapControlId::WireHeatmap, EBlueprintProfilerHeatMapDisplayMode::PinToPin),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected, EHeatmapControlId::WireHeatmap, EBlueprintProfilerHeatMapDisplayMode::PinToPin)
		),
		NAME_None,
		EUserInterfaceActionType::Check);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("BlueprintProfiler.ViewToolBar"))
		.Padding(0)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			.Padding(FMargin(5, 0))
			[
				SAssignNew(GeneralOptionsComboButton, SComboButton)
				.ForegroundColor(this, &SBlueprintProfilerToolbar::GetButtonForegroundColor, EHeatmapControlId::GeneralOptions)
				.ToolTipText(LOCTEXT("GeneralOptions_Tooltip", "General Options"))
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.ContentPadding(2)
				.OnGetMenuContent(DisplayOptions.Get(), &FBlueprintProfilerStatOptions::CreateComboContent)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GeneralOptions_Label", "General Options"))
					.ColorAndOpacity(FLinearColor::White)
				]
			]
			+SHorizontalBox::Slot()
			.Padding(FMargin(5, 0))
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5, 0))
			[
				SAssignNew(HeatMapDisplayModeComboButton, SComboButton)
				.ForegroundColor(this, &SBlueprintProfilerToolbar::GetButtonForegroundColor, EHeatmapControlId::NodeHeatmap)
				.ToolTipText(LOCTEXT("HeatMapDisplayMode_Tooltip", "Heat Map Display Mode"))
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.ContentPadding(2)
				.MenuContent()
				[
					HeatMapDisplayModeComboContent.MakeWidget()
				]
				.ButtonContent()
				[
					CreateHeatMapDisplayModeButton(EHeatmapControlId::NodeHeatmap)
				]
			]
			+SHorizontalBox::Slot()
			.Padding(FMargin(5, 0))
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5, 0))
			[
				SAssignNew(WireHeatMapDisplayModeComboButton, SComboButton)
				.ForegroundColor(this, &SBlueprintProfilerToolbar::GetButtonForegroundColor, EHeatmapControlId::WireHeatmap)
				.ToolTipText(LOCTEXT("WireHeatMapDisplayMode_Tooltip", "Wire Heat Map Display Mode"))
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.ContentPadding(2)
				.MenuContent()
				[
					WireHeatMapDisplayModeComboContent.MakeWidget()
				]
				.ButtonContent()
				[
					CreateHeatMapDisplayModeButton(EHeatmapControlId::WireHeatmap)
				]
			]
			+SHorizontalBox::Slot()
			.Padding(FMargin(5, 0))
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			.Padding(FMargin(5, 0))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HeatmapThresholds_Label", "Heatmap Thresholds:"))
				.ColorAndOpacity(FLinearColor::White)
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			.Padding(FMargin(5, 2))
			[
				SNew(SUniformGridPanel)
				+SUniformGridPanel::Slot(0, 0)
				[
					SNew(SCheckBox)
					.Style(FEditorStyle::Get(), "BlueprintProfiler.ToggleButton.Start")
					.IsChecked(this, &SBlueprintProfilerToolbar::IsHeatLevelMetricsTypeSelected, EBlueprintProfilerHeatLevelMetricsType::ClassRelative)
					.OnCheckStateChanged(this, &SBlueprintProfilerToolbar::OnHeatLevelMetricsTypeChanged, EBlueprintProfilerHeatLevelMetricsType::ClassRelative)
					.ToolTipText(LOCTEXT("HeatLevelMetricsType_Default_Tooltip", "Heat map thresholds are automatically calculated relative to overall Blueprint execution time."))
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Padding(6, 2)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Default", "Default"))
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.ColorAndOpacity(this, &SBlueprintProfilerToolbar::GetHeatLevelMetricsTypeTextColor, EBlueprintProfilerHeatLevelMetricsType::ClassRelative)
						]
					]
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SCheckBox)
					.Style(FEditorStyle::Get(), "BlueprintProfiler.ToggleButton.End")
					.IsChecked(this, &SBlueprintProfilerToolbar::IsHeatLevelMetricsTypeSelected, EBlueprintProfilerHeatLevelMetricsType::CustomThresholds)
					.OnCheckStateChanged(this, &SBlueprintProfilerToolbar::OnHeatLevelMetricsTypeChanged, EBlueprintProfilerHeatLevelMetricsType::CustomThresholds)
					.ToolTipText(LOCTEXT("HeatLevelMetricsType_Custom_Tooltip", "Heat map thresholds are explicitly defined."))
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Padding(6, 2)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Custom", "Custom"))
							.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.ColorAndOpacity(this, &SBlueprintProfilerToolbar::GetHeatLevelMetricsTypeTextColor, EBlueprintProfilerHeatLevelMetricsType::CustomThresholds)
						]
					]
				]
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			.Padding(FMargin(1, 0))
			[
				SAssignNew(CustomHeatThresholdComboButton, SComboButton)
				.IsEnabled(this, &SBlueprintProfilerToolbar::IsCustomHeatThresholdsMenuEnabled)
				.ForegroundColor(this, &SBlueprintProfilerToolbar::GetButtonForegroundColor, EHeatmapControlId::HeatThresholds)
				.ToolTipText(LOCTEXT("HeatMapCustomThresholdsButton_Tooltip", "Custom Thresholds"))
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.ContentPadding(2)
				.OnGetMenuContent(this, &SBlueprintProfilerToolbar::CreateCustomHeatThresholdsMenuContent)
			]
#if 0
			+SHorizontalBox::Slot()
			.Padding(FMargin(5, 0))
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5,0))
			[
				SAssignNew(ViewComboButton, SComboButton)
				.ForegroundColor(this, &SBlueprintProfilerToolbar::GetButtonForegroundColor, EHeatmapControlId::ViewType)
				.ToolTipText(LOCTEXT("ViewType", "View Type"))
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
#endif
		]
	];
}

void SBlueprintProfilerToolbar::OnViewSelectionChanged(const EBlueprintPerfViewType::Type NewViewType)
{
	if (CurrentViewType != NewViewType)
	{
		CurrentViewType = NewViewType;
		ViewChangeDelegate.ExecuteIfBound();
	}
}

TSharedRef<SWidget> SBlueprintProfilerToolbar::CreateViewButton() const
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(0)
		[
			SNew(SImage)
			.Image( FEditorStyle::GetBrush("GenericViewButton") )
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2,0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ProfilerViewLabel", "Current View: "))
			.ColorAndOpacity(FLinearColor::White)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2,0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(GetCurrentViewText())
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.4f))
		];
}

FText SBlueprintProfilerToolbar::GetCurrentViewText() const
{
	switch(CurrentViewType)
	{
		case EBlueprintPerfViewType::Overview:			return LOCTEXT("OverviewLabel", "Profiler Overview");
		case EBlueprintPerfViewType::ExecutionGraph:	return LOCTEXT("ExecutionGraphLabel", "Execution Graph");
		case EBlueprintPerfViewType::LeastPerformant:	return LOCTEXT("LeastPerformantLabel", "Least Performant");
	}
	return LOCTEXT("UnknownViewLabel", "Unknown ViewType");
}

FText SBlueprintProfilerToolbar::GetCurrentHeatMapDisplayModeText(const EHeatmapControlId::Type ControlId) const
{
	FText Result = LOCTEXT("HeatMapDisplayModeLabel_Unknown", "<unknown>");
	IBlueprintProfilerInterface& ProfilerModule = FModuleManager::LoadModuleChecked<IBlueprintProfilerInterface>("BlueprintProfiler");
	EBlueprintProfilerHeatMapDisplayMode DisplayMode = ControlId == EHeatmapControlId::NodeHeatmap ? GetDefault<UBlueprintProfilerSettings>()->GraphNodeHeatMapDisplayMode : GetDefault<UBlueprintProfilerSettings>()->WireHeatMapDisplayMode;
	switch (DisplayMode)
	{
		case EBlueprintProfilerHeatMapDisplayMode::None:			Result = LOCTEXT("HeatMapDisplayModeLabel_None", "Off"); break;
		case EBlueprintProfilerHeatMapDisplayMode::Average:			Result = LOCTEXT("HeatMapDisplayModeLabel_Average", "Average"); break;
		case EBlueprintProfilerHeatMapDisplayMode::Inclusive:		Result = LOCTEXT("HeatMapDisplayModeLabel_Inclusive", "Inclusive"); break;
		case EBlueprintProfilerHeatMapDisplayMode::MaxTiming:		Result = LOCTEXT("HeatMapDisplayModeLabel_MaxTiming", "Max Timing"); break;
		case EBlueprintProfilerHeatMapDisplayMode::Total:			Result = LOCTEXT("HeatMapDisplayModeLabel_TotalTiming", "Total Timing"); break;
		case EBlueprintProfilerHeatMapDisplayMode::HottestPath:		Result = LOCTEXT("HeatMapDisplayModeLabel_HottestPath", "Hottest Path"); break;
		case EBlueprintProfilerHeatMapDisplayMode::PinToPin:		Result = LOCTEXT("HeatMapDisplayModeLabel_PinToPin", "Pin to Pin Heatmap"); break;
	}
	return Result;
}

TSharedRef<SWidget> SBlueprintProfilerToolbar::CreateHeatMapDisplayModeButton(const EHeatmapControlId::Type ControlId) const
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(0,0)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(ControlId == EHeatmapControlId::NodeHeatmap ? LOCTEXT("NodeHeatMapLabel_None", "Node Heatmap: ") : LOCTEXT("WireHeatMapLabel_None", "Wire Heatmap: "))
			.ColorAndOpacity(FLinearColor::White)
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(2,0)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(this, &SBlueprintProfilerToolbar::GetCurrentHeatMapDisplayModeText, ControlId)
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.4f))
		];
}

FSlateColor SBlueprintProfilerToolbar::GetButtonForegroundColor(const EHeatmapControlId::Type ControlId) const
{
	static const FName InvertedForegroundName("InvertedForeground");
	static const FName DefaultForegroundName("DefaultForeground");
	bool bButtonHovered = false;
	switch (ControlId)
	{
		case EHeatmapControlId::ViewType:			bButtonHovered = ViewComboButton->IsHovered(); break;
		case EHeatmapControlId::GeneralOptions:		bButtonHovered = GeneralOptionsComboButton->IsHovered(); break;
		case EHeatmapControlId::NodeHeatmap:		bButtonHovered = HeatMapDisplayModeComboButton->IsHovered(); break;
		case EHeatmapControlId::WireHeatmap:		bButtonHovered = WireHeatMapDisplayModeComboButton->IsHovered(); break;
		case EHeatmapControlId::HeatThresholds:		bButtonHovered = CustomHeatThresholdComboButton->IsHovered();	break;
	}
	return bButtonHovered ? FEditorStyle::GetSlateColor(InvertedForegroundName) : FEditorStyle::GetSlateColor(DefaultForegroundName);
}

ECheckBoxState SBlueprintProfilerToolbar::IsHeatLevelMetricsTypeSelected(EBlueprintProfilerHeatLevelMetricsType InHeatLevelMetricsType) const
{
	return GetDefault<UBlueprintProfilerSettings>()->HeatLevelMetricsType == InHeatLevelMetricsType ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SBlueprintProfilerToolbar::OnHeatLevelMetricsTypeChanged(ECheckBoxState InCheckedState, const EBlueprintProfilerHeatLevelMetricsType NewHeatLevelMetricsType)
{
	if (InCheckedState == ECheckBoxState::Checked)
	{
		UBlueprintProfilerSettings* Settings = GetMutableDefault<UBlueprintProfilerSettings>();
		Settings->HeatLevelMetricsType = NewHeatLevelMetricsType;

		Settings->SaveConfig();
	}
}

FSlateColor SBlueprintProfilerToolbar::GetHeatLevelMetricsTypeTextColor(EBlueprintProfilerHeatLevelMetricsType InHeatLevelMetricsType) const
{
	const UBlueprintProfilerSettings* Settings = GetDefault<UBlueprintProfilerSettings>();
	if (Settings->HeatLevelMetricsType == InHeatLevelMetricsType)
	{
		return FSlateColor(FLinearColor::Black);
	}
	else
	{
		return GetForegroundColor();
	}
}

bool SBlueprintProfilerToolbar::IsHeatMapDisplayModeSelected(const EHeatmapControlId::Type ControlId, const EBlueprintProfilerHeatMapDisplayMode InHeatMapDisplayMode) const
{
	const UBlueprintProfilerSettings* Settings = GetDefault<UBlueprintProfilerSettings>();
	return ControlId == EHeatmapControlId::NodeHeatmap ? (Settings->GraphNodeHeatMapDisplayMode == InHeatMapDisplayMode) :
														 (Settings->WireHeatMapDisplayMode == InHeatMapDisplayMode);
}

void SBlueprintProfilerToolbar::OnHeatMapDisplayModeChanged(const EHeatmapControlId::Type ControlId, const EBlueprintProfilerHeatMapDisplayMode NewHeatMapDisplayMode)
{
	UBlueprintProfilerSettings* Settings = GetMutableDefault<UBlueprintProfilerSettings>();
	if (ControlId == EHeatmapControlId::NodeHeatmap)
	{
		if (Settings->GraphNodeHeatMapDisplayMode != NewHeatMapDisplayMode)
		{
			Settings->GraphNodeHeatMapDisplayMode = NewHeatMapDisplayMode;

			// @TODO - should we save this setting?
			//Settings->SaveConfig();
		}
	}
	else 
	{
		if (Settings->WireHeatMapDisplayMode != NewHeatMapDisplayMode)
		{
			Settings->WireHeatMapDisplayMode = NewHeatMapDisplayMode;

			// @TODO - should we save this setting?
			//Settings->SaveConfig();
		}
	}
}

bool SBlueprintProfilerToolbar::IsCustomHeatThresholdsMenuEnabled() const
{
	return GetDefault<UBlueprintProfilerSettings>()->HeatLevelMetricsType == EBlueprintProfilerHeatLevelMetricsType::CustomThresholds;
}

TSharedRef<SWidget> SBlueprintProfilerToolbar::CreateCustomHeatThresholdsMenuContent()
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0,2))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4,1,4,1))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EventPerformanceThreshold_Label", "Event Heat Threshold"))
				.ToolTipText(LOCTEXT("EventPerformanceThreshold_Tooltip", "The Hot Threshold for Overall Event Timings"))
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(FMargin(4,1,1,1))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.01f)
					.MaxValue(5.f)
					.MinSliderValue(0.01f)
					.MaxSliderValue(5.f)
					.Value(this, &SBlueprintProfilerToolbar::GetCustomPerformanceThreshold, ECustomPerformanceThreshold::Event)
					.OnValueChanged(this, &SBlueprintProfilerToolbar::SetCustomPerformanceThreshold, ECustomPerformanceThreshold::Event)
					.MinDesiredValueWidth(100)
					.ToolTipText(LOCTEXT("EventPerformanceThreshold_Tooltip", "The Hot Threshold for Overall Event Timings"))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(4)
					.Visibility(this, &SBlueprintProfilerToolbar::IsCustomPerformanceThresholdDefaultButtonVisible, ECustomPerformanceThreshold::Event)
					.OnClicked(this, &SBlueprintProfilerToolbar::ResetCustomPerformanceThreshold, ECustomPerformanceThreshold::Event)
					.Content()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0,2))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4,1,4,1))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NodeAveragePerformanceThreshold_Label", "Node Average Heat Threshold"))
				.ToolTipText(LOCTEXT("NodeAveragePerformanceThreshold_Tooltip", "The Hot Threshold for Node Average Timings"))
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(FMargin(4,1,1,1))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.001f)
					.MaxValue(0.5f)
					.MinSliderValue(0.001f)
					.MaxSliderValue(0.5f)
					.Value(this, &SBlueprintProfilerToolbar::GetCustomPerformanceThreshold, ECustomPerformanceThreshold::Average)
					.OnValueChanged(this, &SBlueprintProfilerToolbar::SetCustomPerformanceThreshold, ECustomPerformanceThreshold::Average)
					.MinDesiredValueWidth(100)
					.ToolTipText(LOCTEXT("NodeAveragePerformanceThreshold_Tooltip", "The Hot Threshold for Node Average Timings"))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(4)
					.Visibility(this, &SBlueprintProfilerToolbar::IsCustomPerformanceThresholdDefaultButtonVisible, ECustomPerformanceThreshold::Average)
					.OnClicked(this, &SBlueprintProfilerToolbar::ResetCustomPerformanceThreshold, ECustomPerformanceThreshold::Average)
					.Content()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0,2))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4,1,4,1))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NodeInclusivePerformanceThreshold_Label", "Node Inclusive Heat Threshold"))
				.ToolTipText(LOCTEXT("NodeInclusivePerformanceThreshold_Tooltip", "The Hot Threshold for Node Inclusive Timings"))
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(1)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.001f)
					.MaxValue(0.5f)
					.MinSliderValue(0.001f)
					.MaxSliderValue(0.5f)
					.Value(this, &SBlueprintProfilerToolbar::GetCustomPerformanceThreshold, ECustomPerformanceThreshold::Inclusive)
					.OnValueChanged(this, &SBlueprintProfilerToolbar::SetCustomPerformanceThreshold, ECustomPerformanceThreshold::Inclusive)
					.MinDesiredValueWidth(100)
					.ToolTipText(LOCTEXT("NodeInclusivePerformanceThreshold_Tooltip", "The Hot Threshold for Node Inclusive Timings"))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(4)
					.Visibility(this, &SBlueprintProfilerToolbar::IsCustomPerformanceThresholdDefaultButtonVisible, ECustomPerformanceThreshold::Inclusive)
					.OnClicked(this, &SBlueprintProfilerToolbar::ResetCustomPerformanceThreshold, ECustomPerformanceThreshold::Inclusive)
					.Content()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0,2))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4,1,4,1))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NodeMaxPerformanceThreshold_Label", "Node Max Heat Threshold"))
				.ToolTipText(LOCTEXT("NodeMaxPerformanceThreshold_Tooltip", "The Hot Threshold for Max Node Timings"))
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.Padding(FMargin(4,1,1,1))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.01f)
					.MaxValue(2.f)
					.MinSliderValue(0.01f)
					.MaxSliderValue(2.f)
					.Value(this, &SBlueprintProfilerToolbar::GetCustomPerformanceThreshold, ECustomPerformanceThreshold::Max)
					.OnValueChanged(this, &SBlueprintProfilerToolbar::SetCustomPerformanceThreshold, ECustomPerformanceThreshold::Max)
					.MinDesiredValueWidth(100)
					.ToolTipText(LOCTEXT("NodeMaxPerformanceThreshold_Tooltip", "The Hot Threshold for Max Node Timings"))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(4)
					.Visibility(this, &SBlueprintProfilerToolbar::IsCustomPerformanceThresholdDefaultButtonVisible, ECustomPerformanceThreshold::Max)
					.OnClicked(this, &SBlueprintProfilerToolbar::ResetCustomPerformanceThreshold, ECustomPerformanceThreshold::Max)
					.Content()
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				]
			]
		];
}

TOptional<float> SBlueprintProfilerToolbar::GetCustomPerformanceThreshold(ECustomPerformanceThreshold InType) const
{
	TOptional<float> Result;
	const UBlueprintProfilerSettings* ProfilerSettings = GetDefault<UBlueprintProfilerSettings>();

	switch (InType)
	{
	case ECustomPerformanceThreshold::Event:
		Result = ProfilerSettings->CustomEventPerformanceThreshold;
		break;

	case ECustomPerformanceThreshold::Average:
		Result = ProfilerSettings->CustomAveragePerformanceThreshold;
		break;

	case ECustomPerformanceThreshold::Inclusive:
		Result = ProfilerSettings->CustomInclusivePerformanceThreshold;
		break;

	case ECustomPerformanceThreshold::Max:
		Result = ProfilerSettings->CustomMaxPerformanceThreshold;
		break;
	}

	return Result;
}

void SBlueprintProfilerToolbar::SetCustomPerformanceThreshold(float NewValue, ECustomPerformanceThreshold InType)
{
	float OldValue = 0.0f;
	UBlueprintProfilerSettings* ProfilerSettings = GetMutableDefault<UBlueprintProfilerSettings>();
	
	switch (InType)
	{
	case ECustomPerformanceThreshold::Event:
		OldValue = ProfilerSettings->CustomEventPerformanceThreshold;
		ProfilerSettings->CustomEventPerformanceThreshold = NewValue;
		break;

	case ECustomPerformanceThreshold::Average:
		OldValue = ProfilerSettings->CustomAveragePerformanceThreshold;
		ProfilerSettings->CustomAveragePerformanceThreshold = NewValue;
		break;

	case ECustomPerformanceThreshold::Inclusive:
		OldValue = ProfilerSettings->CustomInclusivePerformanceThreshold;
		ProfilerSettings->CustomInclusivePerformanceThreshold = NewValue;
		break;

	case ECustomPerformanceThreshold::Max:
		OldValue = ProfilerSettings->CustomMaxPerformanceThreshold;
		ProfilerSettings->CustomMaxPerformanceThreshold = NewValue;
		break;
	}

	if (OldValue != NewValue)
	{
		ProfilerSettings->SaveConfig();
	}
}

EVisibility SBlueprintProfilerToolbar::IsCustomPerformanceThresholdDefaultButtonVisible(ECustomPerformanceThreshold InType) const
{
	float CurValue = 0.f;
	float DefValue = 0.f;
	const UBlueprintProfilerSettings* ProfilerSettings = GetDefault<UBlueprintProfilerSettings>();

	switch (InType)
	{
	case ECustomPerformanceThreshold::Event:
		CurValue = ProfilerSettings->CustomEventPerformanceThreshold;
		DefValue = UBlueprintProfilerSettings::CustomEventPerformanceThresholdDefaultValue;
		break;

	case ECustomPerformanceThreshold::Average:
		CurValue = ProfilerSettings->CustomAveragePerformanceThreshold;
		DefValue = UBlueprintProfilerSettings::CustomAveragePerformanceThresholdDefaultValue;
		break;

	case ECustomPerformanceThreshold::Inclusive:
		CurValue = ProfilerSettings->CustomInclusivePerformanceThreshold;
		DefValue = UBlueprintProfilerSettings::CustomInclusivePerformanceThresholdDefaultValue;
		break;

	case ECustomPerformanceThreshold::Max:
		CurValue = ProfilerSettings->CustomMaxPerformanceThreshold;
		DefValue = UBlueprintProfilerSettings::CustomMaxPerformanceThresholdDefaultValue;
		break;
	}

	return CurValue == DefValue ? EVisibility::Hidden : EVisibility::Visible;
}

FReply SBlueprintProfilerToolbar::ResetCustomPerformanceThreshold(ECustomPerformanceThreshold InType)
{
	float OldValue = 0.0f;
	UBlueprintProfilerSettings* ProfilerSettings = GetMutableDefault<UBlueprintProfilerSettings>();
	
	switch (InType)
	{
	case ECustomPerformanceThreshold::Event:
		OldValue = ProfilerSettings->CustomEventPerformanceThreshold;
		ProfilerSettings->CustomEventPerformanceThreshold = UBlueprintProfilerSettings::CustomEventPerformanceThresholdDefaultValue;
		break;

	case ECustomPerformanceThreshold::Average:
		OldValue = ProfilerSettings->CustomAveragePerformanceThreshold;
		ProfilerSettings->CustomAveragePerformanceThreshold = UBlueprintProfilerSettings::CustomAveragePerformanceThresholdDefaultValue;
		break;

	case ECustomPerformanceThreshold::Inclusive:
		OldValue = ProfilerSettings->CustomInclusivePerformanceThreshold;
		ProfilerSettings->CustomInclusivePerformanceThreshold = UBlueprintProfilerSettings::CustomInclusivePerformanceThresholdDefaultValue;
		break;

	case ECustomPerformanceThreshold::Max:
		OldValue = ProfilerSettings->CustomMaxPerformanceThreshold;
		ProfilerSettings->CustomMaxPerformanceThreshold = UBlueprintProfilerSettings::CustomMaxPerformanceThresholdDefaultValue;
		break;
	}

	ProfilerSettings->SaveConfig();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
