// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Developer/BlueprintProfiler/Public/BlueprintProfilerModule.h"

// Shared pointer to a performance stat
typedef TSharedPtr<class FBPProfilerStatWidget> FBPStatWidgetPtr;

//////////////////////////////////////////////////////////////////////////
// SGraphExecutionStatDisplay

class SGraphExecutionStatDisplay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGraphExecutionStatDisplay)
		{}

		SLATE_ARGUMENT(TWeakPtr<FBlueprintEditor>, AssetEditor)
		SLATE_ARGUMENT(TSharedPtr<struct FBlueprintProfilerStatOptions>, DisplayOptions)
	SLATE_END_ARGS()


public:

	~SGraphExecutionStatDisplay();

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End of SWidget interface

protected:

	/** Called when blueprint graph layout changes occur */
	void OnGraphLayoutChanged(TWeakObjectPtr<UBlueprint> Blueprint);

	/** Called when the blueprint profiler is enabled/disabled */
	void OnToggleProfiler(bool bEnabled);

	/** Returns is the profiler widgets are currently visible */
	EVisibility GetNoDataWidgetVisibility() const { return RootTreeItems.Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed; }

	/** Returns if the profiler widgets are currently not visible, used to display the inactive message */
	EVisibility GetWidgetVisibility() const { return RootTreeItems.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; }

	/** Called to generate a stat row for the execution stat tree */
	TSharedRef<ITableRow> OnGenerateRow(FBPStatWidgetPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Called to generate children for given profiler stat */
	void OnGetChildren(FBPStatWidgetPtr InParent, TArray<FBPStatWidgetPtr>& OutChildren);

	/** On double click statistic */
	void OnDoubleClickStatistic(FBPStatWidgetPtr Item);

	/** On statistic expansion state changed */
	void OnStatisticExpansionChanged(FBPStatWidgetPtr Item, bool bExpanded);

protected:

	/** Blueprint editor */
	TWeakPtr<class FBlueprintEditor> BlueprintEditor;

	/** Profiler status text attribute */
	TAttribute<FText> ProfilerStatusText;

	/** Sorted stat list for identifying expensive items */
	TSharedPtr<STreeView<FBPStatWidgetPtr>> ExecutionStatTree;

	/** Source item array for the treeview */
	TArray<FBPStatWidgetPtr> RootTreeItems;

	/** Current blueprint to display stats for */
	TWeakObjectPtr<UBlueprint> CurrentBlueprint;

	/** Current display options */
	TSharedPtr<struct FBlueprintProfilerStatOptions> DisplayOptions;
};
