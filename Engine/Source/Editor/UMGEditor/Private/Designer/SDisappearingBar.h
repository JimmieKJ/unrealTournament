// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"

/**
 * A widget that shows another widget as long as the mouse isn't hovering over it.
 */
class SDisappearingBar : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDisappearingBar) {}
		/** Slot for this designers content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FLinearColor GetFadeColorAndOpacity() const;

	bool bDisappear;
	FCurveSequence FadeCurve;
};
