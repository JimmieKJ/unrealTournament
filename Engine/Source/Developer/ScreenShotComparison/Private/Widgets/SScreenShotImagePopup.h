// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenImagePopup.h: Declares the SScreenShotImagePopup class.
=============================================================================*/

#pragma once

/**
 * A widget to show a screen shot image as a pop up window
 */
class SScreenShotImagePopup : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SScreenShotImagePopup ) {}
	
	/** The assest name for the screen shot */
	SLATE_ARGUMENT( FName, ImageAssetName )

	/** The size of the image */
	SLATE_ARGUMENT( FIntPoint, ImageSize )

	SLATE_END_ARGS()

	/**
	 * Construct the widget.
	 *
	 * @param InArgs   A declaration from which to construct the widget.
 	 * @param InOwnerTableView   The owning table data.
	 */
	void Construct( const FArguments& InArgs );

private:

	// Holds the brush for this screenshot
	TSharedPtr<FSlateDynamicImageBrush> DynamicImageBrush;
};

