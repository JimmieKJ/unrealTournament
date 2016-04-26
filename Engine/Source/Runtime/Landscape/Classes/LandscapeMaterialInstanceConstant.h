// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LandscapeMaterialInstanceConstant.generated.h"

UCLASS(MinimalAPI)
class ULandscapeMaterialInstanceConstant : public UMaterialInstanceConstant
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	uint32 bIsLayerThumbnail:1;
};



