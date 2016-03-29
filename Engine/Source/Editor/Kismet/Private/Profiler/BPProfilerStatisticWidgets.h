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
		InclusiveTime,
		Time,
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

	FBPProfilerStatWidget(TSharedPtr<class FScriptExecutionNode> InExecNode, const FTracePath& WidgetTracePathIn)
		: WidgetTracePath(WidgetTracePathIn)
		, ExecNode(InExecNode)
	{
	}

	virtual ~FBPProfilerStatWidget() {}

	/** Generate exec node widgets */
	virtual void GenerateExecNodeWidgets(const FName InstanceName, const FName FilterGraph);

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

	/** Returns if the exec node representing an event is filtered from view */
	bool IsEventFiltered(TSharedPtr<FScriptExecutionNode> InEventNode, FName GraphFilter) const;

	/** Navigate to referenced node */
	void NavigateTo() const;

protected:

	/** Widget script tracepath */
	FTracePath WidgetTracePath;
	/** Script execution event node */
	TSharedPtr<FScriptExecutionNode> ExecNode;
	/** Script performance data */
	TSharedPtr<FScriptPerfData> PerformanceStats;
	/** Script code offset */
	int32 ScriptCodeOffset;
	/** Child statistics */
	TArray<TSharedPtr<FBPProfilerStatWidget>> CachedChildren;

};
