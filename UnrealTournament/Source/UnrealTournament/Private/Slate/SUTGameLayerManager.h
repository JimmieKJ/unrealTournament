// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTGameLayerManager : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTGameLayerManager)
	{}

	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	
	// Begin SWidget overrides
	virtual bool OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent) override;
	// End SWidget overrides

public:

	void AddLayer(TSharedRef<class SWidget> ViewportContent, const int32 ZOrder = 0);
	void RemoveLayer(TSharedRef<class SWidget> ViewportContent);

	void AddLayer_NoAspect(TSharedRef<class SWidget> ViewportContent, const int32 ZOrder = 0);
	void RemoveLayer_NoAspect(TSharedRef<class SWidget> ViewportContent);

protected:
	TSharedPtr<class SOverlay> GameLayers;
	TSharedPtr<class SOverlay> GameLayers_NoAspect;
	TSharedPtr<class STooltipPresenter> TooltipPresenter;
};

#endif