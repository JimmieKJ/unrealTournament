// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"

USceneThumbnailInfoWithPrimitive::USceneThumbnailInfoWithPrimitive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimitiveType = TPT_Sphere;
	OrbitPitch = -35;
	OrbitYaw = -180.f;
	OrbitZoom = 0.f;
}

