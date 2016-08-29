// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScriptPerfData.h"
#include "TracePath.h"

//////////////////////////////////////////////////////////////////////////
// SProfilerStatRow

/** Enum for each column type in the execution tree widget */
namespace EBlueprintProfilerStat
{
	enum Type
	{
		Name = 0,
		TotalTime,
		AverageTime,
		InclusiveTime,
		PureTime,
		MaxTime,
		MinTime,
		Samples
	};
};

// Shared pointer to a performance stat
typedef TSharedPtr<class FBPProfilerStatWidget> FBPStatWidgetPtr;

class SProfilerStatRow : public SMultiColumnTableRow<FBPStatWidgetPtr>
{
protected:

	/** The profiler statistic associated with this stat row */
	FBPStatWidgetPtr ItemToEdit;

public:

	SLATE_BEGIN_ARGS(SProfilerStatRow){}
	SLATE_END_ARGS()

	/** Constructs the statistic row item widget */
	void Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView, FBPStatWidgetPtr InItemToEdit);

	/** Generates and returns a widget for the specified column name and the associated statistic */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	/** Converts the profiler stat enum into the associated column FName */
	static const FName GetStatName(const EBlueprintProfilerStat::Type StatId);

	/** Converts the profiler stat enum into the associated column FText */
	static const FText GetStatText(const EBlueprintProfilerStat::Type StatId);

};

//////////////////////////////////////////////////////////////////////////
// FBPProfilerStatWidget

class FBPProfilerStatWidget : public TSharedFromThis<FBPProfilerStatWidget>
{
public:

	FBPProfilerStatWidget(TSharedPtr<class FScriptExecutionNode> InExecNode, const FTracePath& WidgetTracePathIn);

	virtual ~FBPProfilerStatWidget() {}

	/** Generate exec node widgets */
	void GenerateExecNodeWidgets(const TSharedPtr<struct FBlueprintProfilerStatOptions> DisplayOptions);

	/** Gather children for list/tree widget */
	void GatherChildren(TArray<TSharedPtr<FBPProfilerStatWidget>>& OutChildren);

	/** Generate column widget */
	TSharedRef<SWidget> GenerateColumnWidget(FName ColumnName);

	/** Returns the exec path object context for this statistic */
	TSharedPtr<class FScriptExecutionNode> GetExecNode() { return ExecNode; }

	/** Returns the performance data object for this statistic */
	TSharedPtr<FScriptPerfData> GetPerformanceData() { return PerformanceStats; }

	/** Get expansion state */
	bool GetExpansionState() const;

	/** Set expansion state */
	void SetExpansionState(bool bExpansionStateIn);

	/** Expand widget state */
	void ExpandWidgetState(TSharedPtr<STreeView<FBPStatWidgetPtr>> TreeView, bool bStateIn);

	/** Restore widget Expansion state */
	void RestoreWidgetExpansionState(TSharedPtr<STreeView<FBPStatWidgetPtr>> TreeView);

	/** Returns if any children expect to be expanded */
	bool ProbeChildWidgetExpansionStates();

protected:

	/** Generate standard node widgets */
	void GenerateStandardNodeWidgets(const TSharedPtr<struct FBlueprintProfilerStatOptions> DisplayOptions);

	/** Generate pure node widgets */
	void GeneratePureNodeWidgets(const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions);

	/** Generate simple tunnel widgets */
	void GenerateSimpleTunnelWidgets(TSharedPtr<FScriptExecutionTunnelEntry> TunnelEntryNode, const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions);

	/** Generate complex tunnel widgets */
	void GenerateComplexTunnelWidgets(TSharedPtr<FScriptExecutionTunnelEntry> TunnelEntryNode, const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions);

	/** Generate tunnel exit site linked widgets */
	void GenerateTunnelLinkWidgets(const TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions, const int32 ScriptOffset = INDEX_NONE);

	/** Navigate to referenced node */
	void NavigateTo() const;

	/** Calculate and return the heat color to use for average timing stats. */
	FSlateColor GetAverageHeatColor() const;

	/** Calculate and return the heat color to use for inclusive timing stats. */
	FSlateColor GetInclusiveHeatColor() const;

	/** Calculate and return the heat color to use for max time stats. */
	FSlateColor GetMaxTimeHeatColor() const;

protected:

	/** Widget script tracepath */
	FTracePath WidgetTracePath;
	/** Script execution event node */
	TSharedPtr<FScriptExecutionNode> ExecNode;
	/** Script performance data */
	TSharedPtr<FScriptPerfData> PerformanceStats;
	/** Child statistics */
	TArray<TSharedPtr<FBPProfilerStatWidget>> CachedChildren;
};
