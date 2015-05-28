// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** A widget that displays a spinning image.*/
class SLATE_API SSpinningImage : public SImage
{
public:
	SLATE_BEGIN_ARGS(SSpinningImage)
		: _Image( FCoreStyle::Get().GetDefaultBrush() )
		, _ColorAndOpacity( FLinearColor::White )
		, _OnMouseButtonDown()
		, _Period( 1.0f )
		{}

		/** Image resource */
		SLATE_ATTRIBUTE( const FSlateBrush*, Image )

		/** Color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Invoked when the mouse is pressed in the widget. */
		SLATE_EVENT( FPointerEventHandler, OnMouseButtonDown )

		/** The amount of time in seconds for a full rotation */
		SLATE_ARGUMENT( float, Period )

	SLATE_END_ARGS()

	/* Construct this widget */
	void Construct( const FArguments& InArgs );

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

private:
	/** The sequence to drive the spinning animation */
	FCurveSequence SpinAnimationSequence;
};