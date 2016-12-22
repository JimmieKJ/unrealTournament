// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SSlider.h"
#include "../SUTStyle.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTSlider : public SSlider
{
	SLATE_BEGIN_ARGS(SUTSlider)
		: _IndentHandle(true)
		, _Locked(false)
		, _Orientation(EOrientation::Orient_Horizontal)
		, _SliderBarColor(FLinearColor::White)
		, _SliderHandleColor(FLinearColor::White)
		, _Style(&FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider"))
		, _StepSize(0.01f)
		, _SnapCount(0)
		, _Value(1.f)
		, _IsFocusable(true)
		, _OnMouseCaptureBegin()
		, _OnMouseCaptureEnd()
		, _OnValueChanged()
		, _InitialSnap(0)
		{ }

		/** Whether the slidable area should be indented to fit the handle. */
		SLATE_ATTRIBUTE( bool, IndentHandle )

		/** Whether the handle is interactive or fixed. */
		SLATE_ATTRIBUTE( bool, Locked )

		/** The slider's orientation. */
		SLATE_ARGUMENT( EOrientation, Orientation)

		/** The color to draw the slider bar in. */
		SLATE_ATTRIBUTE( FSlateColor, SliderBarColor )

		/** The color to draw the slider handle in. */
		SLATE_ATTRIBUTE( FSlateColor, SliderHandleColor )

		/** The style used to draw the slider. */
		SLATE_STYLE_ARGUMENT( FSliderStyle, Style )

		/** The input mode while using the controller. */
		SLATE_ATTRIBUTE(float, StepSize)

		/** The # of Snap points this slider has.  Use 0 for no snaps */
		SLATE_ATTRIBUTE(int32, SnapCount)

		/** A value that drives where the slider handle appears. Value is normalized between 0 and 1. */
		SLATE_ATTRIBUTE( float, Value )

		/** Sometimes a slider should only be mouse-clickable and never keyboard focusable. */
		SLATE_ARGUMENT(bool, IsFocusable)

		/** Invoked when the mouse is pressed and a capture begins. */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

		/** Invoked when the mouse is released and a capture ends. */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureEnd)

		/** Invoked when the Controller is pressed and capture begins. */
		SLATE_EVENT(FSimpleDelegate, OnControllerCaptureBegin)

		/** Invoked when the controller capture is released.  */
		SLATE_EVENT(FSimpleDelegate, OnControllerCaptureEnd)

		/** Called when the value is changed by the slider. */
		SLATE_EVENT( FOnFloatValueChanged, OnValueChanged )

		/** will attempt to set the initial value to a given snap */
		SLATE_ARGUMENT(int32, InitialSnap)


	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	/** Snaps to a given index */
	virtual void SnapTo(int32 SnapIndex);

	/** returns the current snap value */
	virtual int32 GetSnapValue();

protected:
	TAttribute<int32> SnapCount;
	virtual void CommitValue(float NewValue) override;
};

#endif