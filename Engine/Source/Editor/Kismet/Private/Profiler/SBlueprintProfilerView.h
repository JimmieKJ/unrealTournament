// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Developer/BlueprintProfiler/Public/BlueprintProfilerModule.h"
#include "ScriptEventExecution.h"

// Shared pointer to a performance stat
typedef TSharedPtr<class FBPProfilerStat> FBPProfilerStatPtr;

/** Enum to describe type of performance stats view */
enum EPerformanceViewType
{
	PVT_LiveView = 0,
	PVT_Blueprint,
	PVT_BlueprintInstance,
	PVT_BlueprintTrace
};

//////////////////////////////////////////////////////////////////////////
// SBlueprintProfilerView

typedef TSharedPtr<class FBPProfilerStat> FBPProfilerStatPtr;

class SBlueprintProfilerView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SBlueprintProfilerView )
		: _ProfileViewType(PVT_Blueprint)
		{}

		SLATE_ARGUMENT( EPerformanceViewType, ProfileViewType )
	SLATE_END_ARGS()


public:

	~SBlueprintProfilerView();

	void Construct( const FArguments& InArgs );

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

	/** Returns FText number format specifier */
	static const FNumberFormattingOptions& GetNumberFormat() { return NumberFormat; }

protected:

	/** Called when the profiler state is toggled */
	void OnToggleProfiler(bool bEnabled);

	/** Called to generate a stat row for the execution stat tree */
	TSharedRef<ITableRow> OnGenerateRowForProfiler(FBPProfilerStatPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Called to generate children for given profiler stat */
	void OnGetChildrenForProfiler(FBPProfilerStatPtr InParent, TArray<FBPProfilerStatPtr>& OutChildren);

	/** Returns is the profiler widgets are currently visible */
	EVisibility IsProfilerVisible() const { return RootTreeItems.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; }

	/** Returns is the profiler widgets are currently not visible, used to display the inactive message */
	EVisibility IsProfilerHidden() const { return RootTreeItems.Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed; }

	/** Called to create the child profiler widgets */
	void CreateProfilerWidgets();

protected:

	/** Number formatting options */
	static FNumberFormattingOptions NumberFormat;

	/** Current profiler view type */
	EPerformanceViewType CurrentViewType;

	/** Sorted stat list for identifying expensive items */
	TSharedPtr<SListView<FBPProfilerStatPtr>> BlueprintStatList;

	/** Sorted stat list for identifying expensive items */
	TSharedPtr<STreeView<FBPProfilerStatPtr>> ExecutionStatTree;

	/** Source item array for the treeview */
	TArray<FBPProfilerStatPtr> RootTreeItems;

	/** Active Instance Contexts */
	TMap<FString, FProfilerClassContext> ClassContexts;

	/** Cached active class statistic path */
	FString CurrentClassPath;

	/** Flag to control treeview refresh */
	bool bTreeViewIsDirty;

};
