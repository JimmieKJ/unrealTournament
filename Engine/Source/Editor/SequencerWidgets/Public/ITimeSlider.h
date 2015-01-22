// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_DELEGATE_TwoParams( FOnScrubPositionChanged, float, bool )
DECLARE_DELEGATE_OneParam( FOnViewRangeChanged, TRange<float> )

struct FTimeSliderArgs
{
	FTimeSliderArgs()
		: ScrubPosition(0)
		, ViewRange( TRange<float>(0.0f, 5.0f) )
		, ClampMin(-70000.0f)
		, ClampMax(70000.0f)
		, OnScrubPositionChanged()
		, OnBeginScrubberMovement()
		, OnEndScrubberMovement()
		, OnViewRangeChanged()
		, AllowZoom(true)
	{}

	/** The scrub position */
	TAttribute<float> ScrubPosition;
	/** View time range */
	TAttribute< TRange<float> > ViewRange;
	/** Optional min output to clamp to */
	TAttribute<TOptional<float> > ClampMin;
	/** Optional max output to clamp to */
	TAttribute<TOptional<float> > ClampMax;
	/** Called when the scrub position changes */
	FOnScrubPositionChanged OnScrubPositionChanged;
	/** Called right before the scrubber begins to move */
	FSimpleDelegate OnBeginScrubberMovement;
	/** Called right after the scrubber handle is released by the user */
	FSimpleDelegate OnEndScrubberMovement;
	/** Called when the view range changes */
	FOnViewRangeChanged OnViewRangeChanged;
	/** If we are allowed to zoom */
	bool AllowZoom;
};

class ITimeSliderController
{
public:
	virtual ~ITimeSliderController(){}
	virtual int32 OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const = 0;
	virtual FReply OnMouseButtonDown( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) = 0;
	virtual FReply OnMouseButtonUp( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) = 0;
	virtual FReply OnMouseMove( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) = 0;
	virtual FReply OnMouseWheel( TSharedRef<SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) = 0;
};

/**
 * Base class for a widget that scrubs time or frames
 */
class ITimeSlider : public SCompoundWidget
{
public:
};