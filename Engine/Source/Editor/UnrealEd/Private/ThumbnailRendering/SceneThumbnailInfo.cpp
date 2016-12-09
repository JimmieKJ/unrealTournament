// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/SceneThumbnailInfo.h"

USceneThumbnailInfo::USceneThumbnailInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OrbitPitch = -11.25;
	OrbitYaw = -157.5;
	OrbitZoom = 0;
}
