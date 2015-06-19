// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* A list of filters currently applied to an asset view.
*/
class STimelinesContainer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STimelinesContainer){}
	SLATE_END_ARGS()

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedRef<class SVisualLoggerView>, TSharedRef<FVisualLoggerTimeSliderController> TimeSliderController);
	TSharedRef<SWidget> MakeTimeline(TSharedPtr<class SVisualLoggerView> VisualLoggerView, TSharedPtr<class FVisualLoggerTimeSliderController> TimeSliderController, const FVisualLogDevice::FVisualLogEntryItem& Entry);
	TSharedRef<SWidget> GetRightClickMenuContent();

	void OnTimelineSelected(TSharedPtr<class STimelinesBar> Widget);
	void ChangeSelection(class TSharedPtr<class STimeline>, const FPointerEvent& MouseEvent);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	void OnNewLogEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry);
	void OnFiltersChanged();
	void OnSearchChanged(const FText& Filter);
	void OnFiltersSearchChanged(const FText& Filter);

	void ResetData();

	void GenerateReport();

	/**
	* Selects a node in the tree
	*
	* @param AffectedNode			The node to select
	* @param bSelect				Whether or not to select
	* @param bDeselectOtherNodes	Whether or not to deselect all other nodes
	*/
	void SetSelectionState(TSharedPtr<class STimeline> AffectedNode, bool bSelect, bool bDeselectOtherNodes = true);

	/**
	* @return All currently selected nodes
	*/
	const TArray< TSharedPtr<class STimeline> >& GetSelectedNodes() const { return SelectedNodes; }

	const TArray< TSharedPtr<class STimeline> >& GetAllNodes() const { return TimelineItems; }

	/**
	* Returns whether or not a node is selected
	*
	* @param Node	The node to check for selection
	* @return true if the node is selected
	*/
	bool IsNodeSelected(TSharedPtr<class STimeline> Node) const;

protected:
	TSharedPtr<class FVisualLoggerTimeSliderController> TimeSliderController;
	TSharedPtr<class SVisualLoggerView> VisualLoggerView;
	TArray<TSharedPtr<class STimeline> > TimelineItems;
	TArray< TSharedPtr<class STimeline> > SelectedNodes;
	TSharedPtr<SVerticalBox> ContainingBorder;
	float CachedMinTime, CachedMaxTime;
};
