// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintProfilerSettings.h"

/** Blueprint performance view type */
namespace EBlueprintPerfViewType
{
	enum Type
	{
		None = 0,
		Overview,
		ExecutionGraph,
		LeastPerformant
	};
}

/** Heatmap Control Id */
namespace EHeatmapControlId
{
	enum Type
	{
		None = 0,
		ViewType,
		GeneralOptions,
		NodeHeatmap,
		WireHeatmap,
		HeatThresholds
	};
}

//////////////////////////////////////////////////////////////////////////
// FBlueprintProfilerStatOptions

struct FBlueprintProfilerStatOptions : public TSharedFromThis<FBlueprintProfilerStatOptions>
{
public:

	/** Display flags controlling widget appearance */
	enum EDisplayFlags
	{
		DisplayByInstance			= 0x00000001,	// Show instance stats
		ScopeToDebugInstance		= 0x00000002,	// Scope stats to current debug instance
		GraphFilter					= 0x00000004,	// Filter to current graph
		DisplayPure					= 0x00000008,	// Display pure stats
		DisplayInheritedEvents		= 0x00000010,	// Display inherited events
		AverageBlueprintStats		= 0x00000020,	// Average blueprint stats ( Rather than sum )
		Modified					= 0x10000000,	// Display state was changed
	};

	/** Default constructor */
	FBlueprintProfilerStatOptions();

	/** Marks display state as modified */
	void SetStateModified() { Flags |= Modified; }

	/** Returns if the filter state options are modified */
	bool IsStateModified() const { return (Flags & Modified) != 0U; }

	/** Add flags */
	void AddFlags(const uint32 FlagsIn) { Flags |= FlagsIn; }

	/** Clear flags */
	void ClearFlags(const uint32 FlagsIn) { Flags &= ~FlagsIn; }

	/** Set flags */
	void SetFlags(const uint32 FlagsIn) { Flags = FlagsIn; }

	/** Returns true if any flags match */
	bool HasFlags(const uint32 FlagsIn) const { return (Flags & FlagsIn) != 0U; }

	/** Returns true if all flags match */
	bool HasAllFlags(const uint32 FlagsIn) const { return (Flags & FlagsIn) == FlagsIn; }

	/** Set active instance name */
	FName GetActiveInstance() const { return HasFlags(DisplayByInstance) ? ActiveInstance : NAME_None; }

	/** Set active instance name */
	void SetActiveInstance(const FName InstanceName);

	/** Set active graph name */
	void SetActiveGraph(const FName GraphName);

	/** Create combo content */
	TSharedRef<SWidget> CreateComboContent();

	/** Returns if the UI should be checked for flags */
	ECheckBoxState GetChecked(const uint32 FlagsIn) const;

	/** Handles UI check states */
	void OnChecked(ECheckBoxState NewState, const uint32 FlagsIn);

	/** Returns control visibility */
	EVisibility GetVisibility(const uint32 FlagsIn) const;

	/** Returns true if the exec node passes current filters */
	bool IsFiltered(TSharedPtr<class FScriptExecutionNode> Node) const;

private:

	/** The active instance name */
	FName ActiveInstance;
	/** The active graph name */
	FName ActiveGraph;
	/** The display flags */
	uint32 Flags;
};

//////////////////////////////////////////////////////////////////////////
// SBlueprintProfilerToolbar

class SBlueprintProfilerToolbar : public SCompoundWidget
{
public:
		DECLARE_DELEGATE(FOnProfilerViewChanged);

		SLATE_BEGIN_ARGS(SBlueprintProfilerToolbar)
		: _ProfileViewType(EBlueprintPerfViewType::ExecutionGraph)
		{}

		SLATE_ARGUMENT(EBlueprintPerfViewType::Type, ProfileViewType)
		SLATE_ARGUMENT(TSharedPtr<struct FBlueprintProfilerStatOptions>, DisplayOptions)
		SLATE_EVENT(FOnProfilerViewChanged, OnViewChanged)
	SLATE_END_ARGS()


public:

	void Construct(const FArguments& InArgs);

	/** Returns the currently active view */
	EBlueprintPerfViewType::Type GetActiveViewType() const { return CurrentViewType; }

protected:

	/** Constructs and returns the view button widget */
	TSharedRef<SWidget> CreateViewButton() const;

	/** Returns the current view label */
	FText GetCurrentViewText() const;

	/** Returns the active view button foreground color */
	FSlateColor GetButtonForegroundColor(const EHeatmapControlId::Type ControlId) const;

	/** Called when the profiler view type is changed */
	void OnViewSelectionChanged(const EBlueprintPerfViewType::Type NewViewType);

	/** Returns the current heat map display mode label */
	FText GetCurrentHeatMapDisplayModeText(const EHeatmapControlId::Type ControlId) const;

	/** Constructs and returns the heat map display mode button widget */
	TSharedRef<SWidget> CreateHeatMapDisplayModeButton(const EHeatmapControlId::Type ControlId) const;

	/** Returns the active heat map display mode button foreground color */
	FSlateColor GetHeatMapDisplayModeButtonForegroundColor(const EHeatmapControlId::Type ControlId) const;

	/** Returns whether or not the given heat level metrics type is selected */
	ECheckBoxState IsHeatLevelMetricsTypeSelected(const EBlueprintProfilerHeatLevelMetricsType InHeatLevelMetricsType) const;

	/** Called when the heat level metrics type checkbox state is changed */
	void OnHeatLevelMetricsTypeChanged(ECheckBoxState InCheckedState, const EBlueprintProfilerHeatLevelMetricsType NewHeatLevelMetricsType);

	/** Returns text label color for heat level metrics type toggle buttons */
	FSlateColor GetHeatLevelMetricsTypeTextColor(const EBlueprintProfilerHeatLevelMetricsType InHeatLevelMetricsType) const;

	/** Returns whether or not the given heat map display mode is selected */
	bool IsHeatMapDisplayModeSelected(const EHeatmapControlId::Type ControlId, const EBlueprintProfilerHeatMapDisplayMode InHeatMapDisplayMode) const;

	/** Called when the heat map display mode is changed */
	void OnHeatMapDisplayModeChanged(const EHeatmapControlId::Type ControlId, const EBlueprintProfilerHeatMapDisplayMode NewHeatMapDisplayMode);

	/** Returns whether or not the custom heat thresholds combo button should be enabled */
	bool IsCustomHeatThresholdsMenuEnabled() const;

	/** Create the custom heat thresholds combo menu content */
	TSharedRef<SWidget> CreateCustomHeatThresholdsMenuContent();

private:
	/** Custom performance thresholds */
	enum ECustomPerformanceThreshold
	{
		Event,
		Average,
		Inclusive,
		Max
	};

	/** Get/Set custom performance threshold helpers */
	TOptional<float> GetCustomPerformanceThreshold(ECustomPerformanceThreshold InType) const;
	void SetCustomPerformanceThreshold(const float NewValue, ECustomPerformanceThreshold InType);
	EVisibility IsCustomPerformanceThresholdDefaultButtonVisible(ECustomPerformanceThreshold InType) const;
	FReply ResetCustomPerformanceThreshold(ECustomPerformanceThreshold InType);

protected:

	/** Current profiler view type */
	EBlueprintPerfViewType::Type CurrentViewType;

	/** Stat display options */
	TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions;

	/** Delegate to call when the view changes */
	FOnProfilerViewChanged ViewChangeDelegate;

	/** General options combo button widget */
	TSharedPtr<SComboButton> GeneralOptionsComboButton;

	/** View combo button widget */
	TSharedPtr<SComboButton> ViewComboButton;

	/** Heat map display mode combo button widget */
	TSharedPtr<SComboButton> HeatMapDisplayModeComboButton;

	/** Heat map display mode combo button widget */
	TSharedPtr<SComboButton> WireHeatMapDisplayModeComboButton;

	/** Custom heat threshold combo button widget */
	TSharedPtr<SComboButton> CustomHeatThresholdComboButton;
};
