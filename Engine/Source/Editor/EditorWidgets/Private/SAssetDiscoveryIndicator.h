// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** An indicator for the progress of the asset registry background search */
class SAssetDiscoveryIndicator : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SAssetDiscoveryIndicator )
		: _ScaleMode(EAssetDiscoveryIndicatorScaleMode::Scale_None)
		, _FadeIn(true)
		{}

		/** The way the indicator will scale out when done displaying progress */
		SLATE_ARGUMENT( EAssetDiscoveryIndicatorScaleMode::Type, ScaleMode )

		/** The padding to apply to the background of the indicator */
		SLATE_ARGUMENT( FMargin, Padding )

		/** If true, this widget will fade in after a short delay */
		SLATE_ARGUMENT( bool, FadeIn )

	SLATE_END_ARGS()

	/** Destructor */
	virtual ~SAssetDiscoveryIndicator();

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	/** Handles updating the progress from the asset registry */
	void OnAssetRegistryFileLoadProgress(int32 NumDiscoveredAssets, int32 TotalAssets);

	/** Gets the progress bar fraction */
	TOptional<float> GetProgress() const;

	/** Gets the background's opacity */
	FSlateColor GetBorderBackgroundColor() const;

	/** Gets the whole widget's opacity */
	FLinearColor GetIndicatorColorAndOpacity() const;

	/** Gets the whole widget's opacity */
	FVector2D GetIndicatorDesiredSizeScale() const;

	/** Gets the whole widget's visibility */
	EVisibility GetIndicatorVisibility() const;

private:
	/** The asset registry's asset discovery progress as a percentage */
	float Progress;

	/** The way the indicator will scale in/out before/after displaying progress */
	EAssetDiscoveryIndicatorScaleMode::Type ScaleMode;

	/** The fade in/out animation */
	FCurveSequence FadeAnimation;
	FCurveHandle FadeCurve;
	FCurveHandle ScaleCurve;
};