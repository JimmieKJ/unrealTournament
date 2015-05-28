// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a widget that displays an image with a desired width and height.
 */
class SLATE_API SImage
	: public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS( SImage )
		: _Image( FCoreStyle::Get().GetDefaultBrush() )
		, _ColorAndOpacity( FLinearColor::White )
		, _OnMouseButtonDown()
		{}

		/** Image resource */
		SLATE_ATTRIBUTE( const FSlateBrush*, Image )

		/** Color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Invoked when the mouse is pressed in the widget. */
		SLATE_EVENT( FPointerEventHandler, OnMouseButtonDown )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

public:

	/** See the ColorAndOpacity attribute */
	void SetColorAndOpacity( const TAttribute<FSlateColor>& InColorAndOpacity );
	
	/** See the ColorAndOpacity attribute */
	void SetColorAndOpacity( FLinearColor InColorAndOpacity );

	/** See the Image attribute */
	void SetImage(TAttribute<const FSlateBrush*> InImage);
	
	/** See OnMouseButtonDown event */
	void SetOnMouseButtonDown(FPointerEventHandler EventHandler);

public:

	// SWidget overrides

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FVector2D ComputeDesiredSize(float) const override;

protected:

	/** The FName of the image resource to show */
	TAttribute< const FSlateBrush* > Image;

	/** Color and opacity scale for this image */
	TAttribute<FSlateColor> ColorAndOpacity;

	/** Invoked when the mouse is pressed in the image */
	FPointerEventHandler OnMouseButtonDownHandler;
};
