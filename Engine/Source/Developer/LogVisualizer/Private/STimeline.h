// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* A list of filters currently applied to an asset view.
*/
class STimeline : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STimeline){}
		SLATE_EVENT(FOnItemSelectionChanged, OnItemSelectionChanged)
		SLATE_EVENT(FOnGetContent, OnGetMenuContent)
	SLATE_END_ARGS();

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual void OnSelect();
	virtual void OnDeselect();

	/** Constructs this widget with InArgs */
	virtual ~STimeline();
	void Construct(const FArguments& InArgs, TSharedPtr<class SVisualLoggerView> VisualLoggerView, TSharedPtr<class FVisualLoggerTimeSliderController> InTimeSliderController, TSharedPtr<class STimelinesContainer> InContainer, const FVisualLogDevice::FVisualLogEntryItem& Entry);
	bool IsSelected() const;
	bool IsEntryHidden(const FVisualLogDevice::FVisualLogEntryItem&) const;
	const FSlateBrush* GetBorder() const;

	void OnFiltersChanged();
	void OnSearchChanged(const FText& Filter);
	void OnFiltersSearchChanged(const FText& Filter);
	void UpdateVisibility();
	void UpdateVisibilityForItems();

	void AddEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry);
	const TArray<FVisualLogDevice::FVisualLogEntryItem>& GetEntries() { return Entries; }
	FName GetName() { return Name; }
	FName GetOwnerClassName() { return OwnerClassName; }
	void HandleLogVisualizerSettingChanged(FName Name);
	void UpdateVisibilityForEntry(FVisualLogDevice::FVisualLogEntryItem& Entry, const FString& SearchString, bool SearchInsideLogs);
	TSharedPtr<class STimelinesContainer> GetOwner() { return Owner; }

	void Goto(float ScrubPosition);
	void GotoNextItem();
	void GotoPreviousItem();

protected:
	TArray<FVisualLogDevice::FVisualLogEntryItem> Entries;
	TArray<const FVisualLogDevice::FVisualLogEntryItem*> HiddenEntries;
	TSharedPtr<class STimelinesContainer> Owner;
	TSharedPtr<class STimelineBar> TimelineBar;
	TSharedPtr<SMenuAnchor> PopupAnchor;

	FName Name;
	FName OwnerClassName;
	FString SearchFilter;

	/** Delegate to execute to get the menu content of this timeline */
	FOnGetContent OnGetMenuContent;
};
