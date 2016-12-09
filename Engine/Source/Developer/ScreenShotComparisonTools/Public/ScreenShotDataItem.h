// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageComparer.h"

struct FScreenShotDataItem
{
	// Constructors
	FScreenShotDataItem( const FString& InImageFilePath, const FString& InViewName, const FString& InDeviceName, const FImageComparisonResult& InComparison)
		: ImageFilePath(InImageFilePath)
		, AssetName( InImageFilePath )
		, ChangeListNumber( 0 )
		, DeviceName( InDeviceName )
		, ViewName( InViewName )
		, Comparison( InComparison )
	{}

public:

	// The asset name associated with the view
	FString ImageFilePath;

	// The asset name associated with the view
	FString AssetName;

	// The change list the view came from
	int32 ChangeListNumber;

	// The device the screen shot came from
	FString DeviceName;

	// The name of the view
	FString ViewName;

	FImageComparisonResult Comparison;
};

