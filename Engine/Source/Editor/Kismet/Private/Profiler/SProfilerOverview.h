// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Developer/BlueprintProfiler/Public/BlueprintProfilerModule.h"

// Shared pointer to a performance stat
typedef TSharedPtr<class FBPProfilerStatWidget> FBPStatWidgetPtr;

//////////////////////////////////////////////////////////////////////////
// SProfilerOverview

class SProfilerOverview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProfilerOverview)
		{}

		SLATE_ARGUMENT(TWeakPtr<FBlueprintEditor>, AssetEditor)
	SLATE_END_ARGS()


public:

	~SProfilerOverview();

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End of SWidget interface

protected:

	/** Called to generate a stat row for the execution stat tree */
	TSharedRef<ITableRow> OnGenerateRow(FBPStatWidgetPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** On double click statistic */
	void OnDoubleClickStatistic(FBPStatWidgetPtr Item);

protected:

	/** Blueprint editor */
	TWeakPtr<class FBlueprintEditor> BlueprintEditor;

	/** Profiler status text attribute */
	TAttribute<FText> ProfilerStatusText;

	/** Sorted stat list for identifying expensive items */
	TSharedPtr<SListView<FBPStatWidgetPtr>> BlueprintStatList;

	/** Source item array for the treeview */
	TArray<FBPStatWidgetPtr> RootTreeItems;

	/** Current blueprint to display stats for */
	TWeakObjectPtr<UBlueprint> CurrentBlueprint;

};
