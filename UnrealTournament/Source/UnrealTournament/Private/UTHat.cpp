// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHat.h"


AUTHat::AUTHat(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRootComponent(ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneRootComp")));

	GetRootComponent()->Mobility = EComponentMobility::Movable;

	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

}

void AUTHat::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	if (GetRootComponent())
	{
		UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(GetRootComponent());
		if (PrimComponent)
		{
			PrimComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		}

		TArray<USceneComponent*> Children;
		GetRootComponent()->GetChildrenComponents(true, Children);
		for (auto Child : Children)
		{
			PrimComponent = Cast<UPrimitiveComponent>(Child);
			if (PrimComponent)
			{
				PrimComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			}
		}
	}
}
