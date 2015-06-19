// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "GameFramework/Volume.h"
#include "ProceduralFoliageBlockingVolume.generated.h"

class AProceduralFoliageVolume;

/** An invisible volume used to block ProceduralFoliage instances from being spawned. */
UCLASS(MinimalAPI)
class AProceduralFoliageBlockingVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = ProceduralFoliage, EditAnywhere)
	AProceduralFoliageVolume* ProceduralFoliageVolume;
};



