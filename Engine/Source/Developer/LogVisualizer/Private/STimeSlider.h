// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITimeSlider.h"
class STimeSlider : public ITimeSlider
{
public:

	SLATE_BEGIN_ARGS(STimeSlider)
		: _MirrorLabels( false )
	{}
		/* If we should mirror the labels on the timeline */
		SLATE_ARGUMENT( bool, MirrorLabels )
	SLATE_END_ARGS()


	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController );

protected:
	// SWidget interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
private:
	TSharedPtr<ITimeSliderController> TimeSliderController;
	bool bMirrorLabels;
};

