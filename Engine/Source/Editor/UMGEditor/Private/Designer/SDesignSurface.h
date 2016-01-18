// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"
#include "SNodePanel.h"

class SDesignSurface : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SDesignSurface )
		: _AllowContinousZoomInterpolation(false)
	{ }

		/** Slot for this designers content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_ATTRIBUTE(bool, AllowContinousZoomInterpolation)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	// End of Swidget interface

protected:
	void PaintBackgroundAsLines(const FSlateBrush* BackgroundImage, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const;

	/** Gets the current zoom factor. */
	float GetZoomAmount() const;

	void ChangeZoomLevel(int32 ZoomLevelDelta, const FVector2D& WidgetSpaceZoomOrigin, bool bOverrideZoomLimiting);
	
	void PostChangedZoom();

	bool ScrollToLocation(const FGeometry& MyGeometry, FVector2D DesiredCenterPosition, const float InDeltaTime);

	bool ZoomToLocation(const FVector2D& CurrentSizeWithoutZoom, const FVector2D& DesiredSize, bool bDoneScrolling);

	void ZoomToFit(bool bInstantZoom);

	FText GetZoomText() const;
	FSlateColor GetZoomTextColorAndOpacity() const;

	FVector2D GetViewOffset() const;

	FSlateRect ComputeSensibleBounds() const;

	FVector2D GraphCoordToPanelCoord(const FVector2D& GraphSpaceCoordinate) const;
	FVector2D PanelCoordToGraphCoord(const FVector2D& PanelSpaceCoordinate) const;

protected:
	virtual FSlateRect ComputeAreaBounds() const;
	virtual float GetGridScaleAmount() const;
	virtual int32 GetGraphRulePeriod() const;
	virtual int32 GetSnapGridSize() const = 0;

protected:
	/** The position within the graph at which the user is looking */
	FVector2D ViewOffset;

	/** Previous Zoom Level */
	int32 PreviousZoomLevel;

	/** How zoomed in/out we are. e.g. 0.25f results in quarter-sized nodes. */
	int32 ZoomLevel;

	/** Are we panning the view at the moment? */
	bool bIsPanning;

	/** Allow continuous zoom interpolation? */
	TAttribute<bool> AllowContinousZoomInterpolation;

	/** Fade on zoom for graph */
	FCurveSequence ZoomLevelGraphFade;

	/** Curve that handles fading the 'Zoom +X' text */
	FCurveSequence ZoomLevelFade;

	// The interface for mapping ZoomLevel values to actual node scaling values
	TScopedPointer<FZoomLevelsContainer> ZoomLevels;

	bool bAllowContinousZoomInterpolation;

	bool bTeleportInsteadOfScrollingWhenZoomingToFit;

	FVector2D ZoomTargetTopLeft;
	FVector2D ZoomTargetBottomRight;
	FVector2D ZoomToFitPadding;

	/** Cumulative magnify delta from trackpad gesture */
	float TotalGestureMagnify;

	/** Does the user need to press Control in order to over-zoom. */
	bool bRequireControlToOverZoom;

	/** Cached geometry for use within the active timer */
	FGeometry CachedGeometry;

private:
	/** Active timer that handles deferred zooming until the target zoom is reached */
	EActiveTimerReturnType HandleZoomToFit(double InCurrentTime, float InDeltaTime);

	/** The handle to the active timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	// A flag noting if we have a pending zoom to extents operation to perform next tick.
	bool bDeferredZoomToExtents;
};
