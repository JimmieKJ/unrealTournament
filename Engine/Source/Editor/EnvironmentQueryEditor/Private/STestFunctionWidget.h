// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Debug console widget, designed to be summoned on top of a viewport or window
*/
class STestFunctionWidget : public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(STestFunctionWidget)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize() const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// normalized test scores for 11 normalized test values (11 samples: y(0), y(0.1), ..., y(1.0))
	// clamp lines will be fixed at samples x=ClampMinX and x=ClampMaxX
	TArray<float> ScoreValues;
	float ClampMinX;
	float ClampMaxX;
	float FilterLowX;
	float FilterHiX;

	// show clamp min line
	uint32 bShowClampMin : 1;
	uint32 bShowClampMax : 1;
	uint32 bShowLowPassFilter : 1;
	uint32 bShowHiPassFilter : 1;

private:

	FVector2D GetWidgetPosition(float X, float Y, const FGeometry& Geom) const;
};
