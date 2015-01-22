// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotItem.h: Declares the SScreenShotItem class.
=============================================================================*/

#pragma once

/**
 * The widget containing the screen shot image.
 */

class SScreenShotItem : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SScreenShotItem )
		: _ScreenShotData()
	{}

	SLATE_ARGUMENT( TSharedPtr<IScreenShotData>, ScreenShotData )

	SLATE_END_ARGS()

	/**
	 * Construct this widget.
	 *
	 * @param InArgs - The declaration data for this widget.
	 */
	void Construct( const FArguments& InArgs );

private:

	/**
	 * Load the dynamic brush.
	 *
	 */
	void LoadBrush();

	/**
	 * Handles the user clicking on the image
	 */
	FReply OnImageClicked(const FGeometry& InGeometry, const FPointerEvent& InEvent);

	/** 
	 * Gets the actual size of the image
	 */
	FIntPoint GetActualImageSize();
private:

	//The cached actual size of the screenshot
	FIntPoint CachedActualImageSize;

	//Holds the dynamic brush.
	TSharedPtr<FSlateDynamicImageBrush> DynamicBrush;

	//Holds the screen shot info.
	TSharedPtr<IScreenShotData> ScreenShotData;
};

