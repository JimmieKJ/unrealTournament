// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/SequencerWidgets/Public/ITimeSlider.h"

struct FVisualLoggerTimeSliderArgs : FTimeSliderArgs
{
	FVisualLoggerTimeSliderArgs()
		: CursorSize(0.1f)
	{
		ViewRange = TRange<float>(0.0f, 5.0f);
	}

	/** Cursor range for data like histogram graphs, etc. */
	TAttribute< float > CursorSize;
};

/**
 * A time slider controller for sequencer
 * Draws and manages time data for a Sequencer
 */
class FVisualLoggerTimeSliderController : public ITimeSliderController
{
public:
	FVisualLoggerTimeSliderController(const FVisualLoggerTimeSliderArgs& InArgs);
	void SetTimesliderArgs(const FVisualLoggerTimeSliderArgs& InArgs);

	/** ITimeSliderController Interface */
	virtual int32 OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	/**
	 * Draws major tick lines in the section view                                                              
	 */
	int32 OnPaintSectionView( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, bool bDisplayTickLines, bool bDisplayScrubPosition ) const;
	FVisualLoggerTimeSliderArgs& GetTimeSliderArgs() { return TimeSliderArgs; }
	/**
	 * Call this method when the user's interaction has changed the scrub position
	 *
	 * @param NewValue				Value resulting from the user's interaction
	 * @param bIsScrubbing			True if done via scrubbing, false if just releasing scrubbing
	 */
	void CommitScrubPosition( float NewValue, bool bIsScrubbing );

	void SetExternalScrollbar(TSharedRef<SScrollBar> Scrollbar);
	void SetTimeRange(float MinValue, float MaxValue);
	void SetClampRange(float MinValue, float MaxValue);
	bool IsPanning() { return bPanning; }

private:

	/**
	 * Draws time tick marks
	 *
	 * @param OutDrawElements	List to add draw elements to
	 * @param RangeToScreen		Time range to screen space converter
	 * @param InArgs			Parameters for drawing the tick lines
	 */
	void DrawTicks( FSlateWindowElementList& OutDrawElements, const struct FScrubRangeToScreen& RangeToScreen, struct FDrawTickArgs& InArgs ) const;
	void HorizontalScrollBar_OnUserScrolled(float ScrollOffset);
private:
	FVisualLoggerTimeSliderArgs TimeSliderArgs;
	/** Brush for drawing an upwards facing scrub handle */
	const FSlateBrush* ScrubHandleUp;
	/** Brush for drawing a downwards facing scrub handle */
	const FSlateBrush* ScrubHandleDown;
	/** Brush for drawing cursor background to visualize corsor size */
	const FSlateBrush* CursorBackground;
	/** Total mouse delta during dragging **/
	float DistanceDragged;
	/** If we are dragging the scrubber */
	bool bDraggingScrubber;
	/** If we are currently panning the panel */
	bool bPanning;
	/***/
	TSharedPtr<SScrollBar> Scrollbar;
	FVector2D SoftwareCursorPosition;
};
