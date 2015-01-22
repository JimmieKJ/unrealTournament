// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotImagePopup.cpp: Implements the SScreenImagePopup class.
=============================================================================*/

#include "ScreenShotComparisonPrivatePCH.h"

void SScreenShotImagePopup::Construct( const FArguments& InArgs )
{
	// Load the image
	DynamicImageBrush = MakeShareable(new FSlateDynamicImageBrush( InArgs._ImageAssetName, InArgs._ImageSize ) );

	// Create the screen shot popup widget.
	ChildSlot
	[
		SNew(SBorder)
		. BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		. Padding(10)
		[
			SNew(SBox)
			.WidthOverride(InArgs._ImageSize.X)
			.HeightOverride(InArgs._ImageSize.Y)
			[
				SNew(SImage)
				.Image( DynamicImageBrush.Get() )
			]
		]
	];
}