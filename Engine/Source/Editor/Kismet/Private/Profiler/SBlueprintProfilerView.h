// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SBlueprintProfilerToolbar.h"

//////////////////////////////////////////////////////////////////////////
// SBlueprintProfilerView

class SBlueprintProfilerView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintProfilerView)
		: _ProfileViewType(EBlueprintPerfViewType::ExecutionGraph)
		{}

		SLATE_ARGUMENT(TWeakPtr<FBlueprintEditor>, AssetEditor)
		SLATE_ARGUMENT(EBlueprintPerfViewType::Type, ProfileViewType)
	SLATE_END_ARGS()


public:

	~SBlueprintProfilerView();

	void Construct(const FArguments& InArgs);

protected:

	/** Called when the profiler state is toggled */
	void OnToggleProfiler(bool bEnabled);

	/** Called to update the display when the stat layouts change or the blueprint is compiled. */
	void OnGraphLayoutChanged(TWeakObjectPtr<UBlueprint> Blueprint);

	/** Returns the profiler current status message */
	FText GetProfilerStatusText() const { return StatusText; }

	/** Called to create the child profiler widgets */
	void UpdateActiveProfilerWidget();

	/** Called when the profiler view type is changed */
	void OnViewChanged();

	/** Create active statistic display widget */
	TSharedRef<SWidget> CreateActiveStatisticWidget();

private:

	/** Update status message */
	void UpdateStatusMessage();

protected:

	/** Profiler status Text */
	FText StatusText;

	/** Blueprint editor */
	TWeakPtr<class FBlueprintEditor> BlueprintEditor;

	/** Display options */
	TSharedPtr<FBlueprintProfilerStatOptions> DisplayOptions;

	/** Profiler Toolbar */
	TSharedPtr<class SBlueprintProfilerToolbar> ProfilerToolbar;
};
