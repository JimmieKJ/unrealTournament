// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/SequencerWidgets/Public/ITimeSlider.h"

struct FPaintPlaybackRangeArgs
{
	FPaintPlaybackRangeArgs()
		: StartBrush(nullptr), EndBrush(nullptr), BrushWidth(0.f)
	{}

	FPaintPlaybackRangeArgs(const FSlateBrush* InStartBrush, const FSlateBrush* InEndBrush, float InBrushWidth)
		: StartBrush(InStartBrush), EndBrush(InEndBrush), BrushWidth(InBrushWidth)
	{}
	/** Brush to use for the start bound */
	const FSlateBrush* StartBrush;
	/** Brush to use for the end bound */
	const FSlateBrush* EndBrush;
	/** The width of the above brushes, in slate units */
	float BrushWidth;
};

struct FPaintSectionAreaViewArgs
{
	FPaintSectionAreaViewArgs()
		: bDisplayTickLines(false), bDisplayScrubPosition(false)
	{}

	/** Whether to display tick lines */
	bool bDisplayTickLines;
	/** Whether to display the scrub position */
	bool bDisplayScrubPosition;
	/** Optional Paint args for the playback range*/
	TOptional<FPaintPlaybackRangeArgs> PlaybackRangeArgs;
};

/**
 * A time slider controller for sequencer
 * Draws and manages time data for a Sequencer
 */
class FSequencerTimeSliderController : public ITimeSliderController
{
public:
	FSequencerTimeSliderController( const FTimeSliderArgs& InArgs );

	/** ITimeSliderController Interface */
	virtual int32 OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FCursorReply OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	/** End ITimeSliderController Interface */

	/** Get the current view range for this controller */
	virtual FAnimatedRange GetViewRange() const override { return TimeSliderArgs.ViewRange.Get(); }

	/** Get the current clamp range for this controller */
	virtual FAnimatedRange GetClampRange() const override { return TimeSliderArgs.ClampRange.Get(); }

	/** Convert time to frame */
	virtual int32 TimeToFrame(float Time) const override;

	/** Convert time to frame */
	virtual float FrameToTime(int32 Frame) const override;

	/**
	 * Clamp the given range to the clamp range 
	 *
	 * @param NewRangeMin		The new lower bound of the range
	 * @param NewRangeMax		The new upper bound of the range
	 */	
	void ClampViewRange(float& NewRangeMin, float& NewRangeMax);

	/**
	 * Set a new range based on a min, max and an interpolation mode
	 * 
	 * @param NewRangeMin		The new lower bound of the range
	 * @param NewRangeMax		The new upper bound of the range
	 * @param Interpolation		How to set the new range (either immediately, or animated)
	 */
	virtual void SetViewRange( float NewRangeMin, float NewRangeMax, EViewRangeInterpolation Interpolation ) override;

	/**
	 * Set a new clamp range based on a min, max
	 * 
	 * @param NewRangeMin		The new lower bound of the clamp range
	 * @param NewRangeMax		The new upper bound of the clamp range
	 */
	virtual void SetClampRange( float NewRangeMin, float NewRangeMax ) override;

	/**
	 * Zoom the range by a given delta.
	 * 
	 * @param InDelta		The total amount to zoom by (+ve = zoom out, -ve = zoom in)
	 * @param ZoomBias		Bias to apply to lower/upper extents of the range. (0 = lower, 0.5 = equal, 1 = upper)
	 */
	bool ZoomByDelta( float InDelta, float ZoomBias = 0.5f );

	/**
	 * Pan the range by a given delta
	 * 
	 * @param InDelta		The total amount to pan by (+ve = pan forwards in time, -ve = pan backwards in time)
	 */
	void PanByDelta( float InDelta );

	/**
	 * Draws major tick lines in the section view                                                              
	 */
	int32 OnPaintSectionView( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintSectionAreaViewArgs& Args ) const;

private:
	/**
	 * Call this method when the user's interaction has changed the scrub position
	 *
	 * @param NewValue				Value resulting from the user's interaction
	 * @param bIsScrubbing			True if done via scrubbing, false if just releasing scrubbing
	 */
	void CommitScrubPosition( float NewValue, bool bIsScrubbing );

	/**
	 * Draws time tick marks
	 *
	 * @param OutDrawElements	List to add draw elements to
	 * @param RangeToScreen		Time range to screen space converter
	 * @param InArgs			Parameters for drawing the tick lines
	 */
	void DrawTicks( FSlateWindowElementList& OutDrawElements, const struct FScrubRangeToScreen& RangeToScreen, struct FDrawTickArgs& InArgs ) const;

	/**
	 * Draws the playback range
	 * @return the new layer ID
	 */
	int32 DrawPlaybackRange(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const;

private:
	/**
	 * Hit test the lower bound of the playback range
	 */
	bool HitTestPlaybackStart(const FScrubRangeToScreen& RangeToScreen, const TRange<float>& PlaybackRange, float LocalHitPositionX, float ScrubPosition) const;

	/**
	 * Hit test the upper bound of the playback range
	 */
	bool HitTestPlaybackEnd(const FScrubRangeToScreen& RangeToScreen, const TRange<float>& PlaybackRange, float LocalHitPositionX, float ScrubPosition) const;

	void SetPlaybackRangeStart(float NewStart);
	void SetPlaybackRangeEnd(float NewEnd);

private:
	FTimeSliderArgs TimeSliderArgs;
	/** The size of the scrub handle */
	float ScrubHandleSize;
	/** Brush for drawing an upwards facing scrub handle */
	const FSlateBrush* ScrubHandleUp;
	/** Brush for drawing a downwards facing scrub handle */
	const FSlateBrush* ScrubHandleDown;
	/** Total mouse delta during dragging **/
	float DistanceDragged;
	/** If we are dragging the scrubber or dragging to set the time range */
	enum DragType
	{
		DRAG_SCRUBBING_TIME,
		DRAG_SETTING_RANGE,
		DRAG_START_RANGE,
		DRAG_END_RANGE,
		DRAG_NONE
	};
	DragType MouseDragType;
	/** If we are currently panning the panel */
	bool bPanning;
	/** Mouse down time range */
	FVector2D MouseDownRange;
	/** Range stack */
	TArray<FVector2D> RangeStack;
};
