// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"

class SZoomPan : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SZoomPan) {}
		/** Slot for this designers content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)
		
		SLATE_ATTRIBUTE(FVector2D, ViewOffset)
		
		SLATE_ATTRIBUTE(float, ZoomAmount)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	/**
	 * Sets the content for this border
	 *
	 * @param	InContent	The widget to use as content for the border
	 */
	void SetContent( const TSharedRef< SWidget >& InContent );

protected:
	void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;

	virtual float GetRelativeLayoutScale( const FSlotBase& Child ) const override;

	/** The position within the panel at which the user is looking */
	TAttribute<FVector2D> ViewOffset;

	/** How zoomed in/out we are. e.g. 0.25f results in quarter-sized widgets. */
	TAttribute<float> ZoomAmount;
};
