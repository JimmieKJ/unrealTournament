// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* A list of filters currently applied to an asset view.
*/

class STimelineBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STimelineBar){}
		SLATE_EVENT(FOnItemSelectionChanged, OnItemSelectionChanged)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual bool SupportsKeyboardFocus() const override { return true;  }

	void Construct(const FArguments& InArgs, TSharedPtr<FVisualLoggerTimeSliderController> InTimeSliderController, TSharedPtr<class STimeline> TimelineOwner);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	FVector2D ComputeDesiredSize(float) const override;
	void SnapScrubPosition(float ScrubPosition);
	void SnapScrubPosition(int32 NewItemIndex);
	int32 GetClosestItem(float Time) const;

	void OnSelect();
	void OnDeselect();
	void GotoNextItem(int32 MoveDistance = 1);
	void GotoPreviousItem(int32 MoveDistance = 1);

protected:
	TSharedPtr<class FVisualLoggerTimeSliderController> TimeSliderController;
	TWeakPtr<class STimeline> TimelineOwner;
	mutable int32 CurrentItemIndex;
};
