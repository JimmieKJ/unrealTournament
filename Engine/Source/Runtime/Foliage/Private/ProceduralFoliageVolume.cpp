// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FoliagePrivate.h"
#include "ProceduralFoliageVolume.h"
#include "ProceduralFoliageComponent.h"
#include "ProceduralFoliageSpawner.h"

AProceduralFoliageVolume::AProceduralFoliageVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ProceduralComponent = ObjectInitializer.CreateDefaultSubobject<UProceduralFoliageComponent>(this, TEXT("ProceduralFoliageComponent"));
	ProceduralComponent->SetSpawningVolume(this);

	if (UBrushComponent* BrushComponent = GetBrushComponent())
	{
		BrushComponent->SetCollisionObjectType(ECC_WorldStatic);
		BrushComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

		// This is important because the volume overlaps with all procedural foliage
		// That means during streaming we'll get a huge hitch for UpdateOverlaps
		BrushComponent->bGenerateOverlapEvents = false;
	}
}

#if WITH_EDITOR

void AProceduralFoliageVolume::PostEditImport()
{
	// Make sure that this is the component's spawning volume
	ProceduralComponent->SetSpawningVolume(this);
}

bool AProceduralFoliageVolume::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (ProceduralComponent && ProceduralComponent->FoliageSpawner)
	{
		Objects.Add(ProceduralComponent->FoliageSpawner);
	}
	return true;
}

#endif