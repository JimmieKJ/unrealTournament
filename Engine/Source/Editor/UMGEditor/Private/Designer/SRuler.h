// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SRuler : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SRuler)
		: _Orientation(Orient_Horizontal)
		, _OnMouseButtonDown()
	{}

	SLATE_ARGUMENT(EOrientation, Orientation)

	SLATE_EVENT(FPointerEventHandler, OnMouseButtonDown)

	SLATE_END_ARGS()


	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Sets the rules to use when rendering the ruler.
	 *
	 * @param AbsoluteOrigin The origin of the ruler in absolute window space.
	 * @param SlateToUnitScale The scaler applied to slate units to get the same number 
	 * of units currently being displayed by the rule marks.
	 */
	void SetRuling(FVector2D AbsoluteOrigin, float SlateToUnitScale);

	/**
	 * Sets the cursor position in window absolute space.
	 *
	 * @param AbsoluteCursor The position of the cursor in absolute window space.
	 */
	void SetCursor(TOptional<FVector2D> AbsoluteCursor);

protected:
	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:
	/** Draws the ticks on the track */
	int32 DrawTicks(FSlateWindowElementList& OutDrawElements, const struct FScrubRangeToScreen& RangeToScreen, struct FDrawTickArgs& InArgs) const;

private:
	/** The orientation of the ruler */
	EOrientation Orientation;

	/** The absolute origin of the document being measured.  The 0 on the ruler will start here. */
	FVector2D AbsoluteOrigin;

	/** The absolute position of the cursor so that little lines can be drawn showing the cursors position on the ruler. */
	TOptional<FVector2D> AbsoluteCursor;

	/** The current conversion from Slate Pixel To Unit Size. */
	float SlateToUnitScale;

	/** The public event we expose when the mouse button is pressed onto the ruler. */
	FPointerEventHandler MouseButtonDownHandler;
};

