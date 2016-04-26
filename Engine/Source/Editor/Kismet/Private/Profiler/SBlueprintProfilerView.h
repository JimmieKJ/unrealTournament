// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


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

	/** Returns the profiler current status message */
	FText GetProfilerStatusText() const { return StatusText; }

	/** Called to create the child profiler widgets */
	void UpdateActiveProfilerWidget();

	/** Returns the current view label */
	FText GetCurrentViewText() const;

	/** Constructs and returns the view button widget */
	TSharedRef<SWidget> CreateViewButton() const;

	/** Returns the active view button foreground color */
	FSlateColor GetViewButtonForegroundColor() const;

	/** Called when the profiler view type is changed */
	void OnViewSelectionChanged(const EBlueprintPerfViewType::Type NewViewType);

	/** Create active statistic display widget */
	TSharedRef<SWidget> CreateActiveStatisticWidget();

protected:

	/** Profiler status Text */
	FText StatusText;

	/** Blueprint editor */
	TWeakPtr<class FBlueprintEditor> BlueprintEditor;

	/** Current profiler view type */
	EBlueprintPerfViewType::Type CurrentViewType;

	/** View combo button widget */
	TSharedPtr<SComboButton> ViewComboButton;

};
