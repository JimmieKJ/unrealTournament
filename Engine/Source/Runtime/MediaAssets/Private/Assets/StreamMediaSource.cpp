// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "StreamMediaSource.h"


FString UStreamMediaSource::GetUrl() const
{
	return StreamUrl;
}


bool UStreamMediaSource::Validate() const
{
	return StreamUrl.Contains(TEXT("://"));
}
