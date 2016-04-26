// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FoliagePrivate.h"
#include "Components/BrushComponent.h"
#include "ProceduralFoliageBlockingVolume.h"

static FName ProceduralFoliageBlocking_NAME(TEXT("ProceduralFoliageBlockingVolume"));

AProceduralFoliageBlockingVolume::AProceduralFoliageBlockingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (UBrushComponent* BrushComponent = GetBrushComponent())
	{
		BrushComponent->SetCollisionObjectType(ECC_WorldStatic);
		BrushComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
}
