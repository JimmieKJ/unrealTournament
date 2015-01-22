// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotDataItem.h: Implements the ScreenShotDataItem class.
=============================================================================*/

#pragma once

// Dummy data used to test the Screen Shot Comparison UI
struct FScreenShotDataItem
{
	// Constructors
	FScreenShotDataItem( const FString& InViewName, const FString& InDeviceName, const FString& InAssetName, const int32 InChangeListNumber )
		: AssetName( InAssetName )
		, ChangeListNumber( InChangeListNumber )
		, DeviceName( InDeviceName )
		, ViewName( InViewName )
	{}

public:

	// The asset name associated with the view
	FString AssetName;

	// The change list the view came from
	int32 ChangeListNumber;

	// The device the screen shot came from
	FString DeviceName;

	// The name of the view
	FString ViewName;
};

