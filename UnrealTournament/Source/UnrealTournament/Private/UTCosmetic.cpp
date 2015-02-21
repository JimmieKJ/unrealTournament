// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCosmetic.h"


AUTCosmetic::AUTCosmetic(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRootComponent(ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneRootComp")));

	GetRootComponent()->Mobility = EComponentMobility::Movable;

	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	CosmeticName = TEXT("Unnamed Cosmetic");
	CosmeticAuthor = TEXT("Anonymous");
}

void AUTCosmetic::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	if (GetRootComponent())
	{
		bool bRootNotPrimitiveComponent = true;

		UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(GetRootComponent());
		if (PrimComponent)
		{
			PrimComponent->bReceivesDecals = false;
			PrimComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			bRootNotPrimitiveComponent = false;
			PrimComponent->bLightAttachmentsAsGroup = true;
		}

		TArray<USceneComponent*> Children;
		GetRootComponent()->GetChildrenComponents(true, Children);
		for (auto Child : Children)
		{
			PrimComponent = Cast<UPrimitiveComponent>(Child);
			if (PrimComponent)
			{
				if (bRootNotPrimitiveComponent)
				{
					SetRootComponent(PrimComponent);
					bRootNotPrimitiveComponent = false;
				}
				PrimComponent->bReceivesDecals = false;
				PrimComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				PrimComponent->bLightAttachmentsAsGroup = true;
			}
		}
	}
}

void AUTCosmetic::OnWearerHeadshot_Implementation()
{

}