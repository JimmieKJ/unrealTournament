// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LandscapeMaterialInstanceConstant.generated.h"

UCLASS(MinimalAPI)
class ULandscapeMaterialInstanceConstant : public UMaterialInstanceConstant
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	uint32 bIsLayerThumbnail:1;

	UPROPERTY()
	int32 DataWeightmapIndex;

	UPROPERTY()
	int32 DataWeightmapSize;
};



