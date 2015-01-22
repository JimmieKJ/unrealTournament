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
		SLATE_ATTRIBUTE(TSharedPtr<IVisualLoggerInterface>, VisualLoggerInterface)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual bool SupportsKeyboardFocus() const { return true;  }

	void Construct(const FArguments& InArgs, TSharedPtr<FSequencerTimeSliderController> InTimeSliderController, TSharedPtr<class STimeline> TimelineOwner);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	FVector2D ComputeDesiredSize() const;
	void SnapScrubPosition(float ScrubPosition);
	void SnapScrubPosition(int32 NewItemIndex);
	uint32 GetClosestItem(float Time) const;

	void OnSelect();
	void OnDeselect();

protected:
	TSharedPtr<IVisualLoggerInterface> VisualLoggerInterface;
	TSharedPtr<class FSequencerTimeSliderController> TimeSliderController;
	TWeakPtr<class STimeline> TimelineOwner;
	mutable int32 CurrentItemIndex;
};
