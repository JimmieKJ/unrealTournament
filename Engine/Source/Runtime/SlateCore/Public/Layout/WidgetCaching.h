// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Used by the implementation of ILayoutCache to store additional information about every widget drawn,
 * so that the hit test grid can be replicated during subsequent frames without the widgets actually doing 
 * any work.
 */
class SLATECORE_API FCachedWidgetNode
{
public:
	void Initialize(const FPaintArgs& Args, TSharedRef<SWidget> InWidget, const FGeometry& InGeometry, const FSlateRect& InClippingRect);

	void RecordHittestGeometry(FHittestGrid& Grid, int32 LastHittestIndex);

public:

	TArray< FCachedWidgetNode*, TInlineAllocator<4> > Children;

	TWeakPtr<SWidget> Widget;
	FGeometry Geometry;
	FSlateRect ClippingRect;
	FVector2D WindowOffset;

	EVisibility RecordedVisibility;
	int32 LastRecordedHittestIndex;

	// TODO Do we want to cache information about the elements drawn by each widget?
	//TArray< FSlateWindowElementList > Elements;
};
