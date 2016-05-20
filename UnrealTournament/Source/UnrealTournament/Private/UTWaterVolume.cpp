// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWaterVolume.h"
#include "UTNoCameraVolume.h"
#include "UTCarriedObject.h"

AUTWaterVolume::AUTWaterVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bWaterVolume = true;
	FluidFriction = 0.3f;
	PawnEntryVelZScaling = 0.4f;
	BrakingDecelerationSwimming = 300.f;
	TerminalVelocity = 3000.f;
	WaterCurrentDirection = FVector(0.f);
	MaxRelativeSwimSpeed = 1000.f;
	WaterCurrentSpeed = 0.f;
	VolumeName = NSLOCTEXT("Volume", "WaterVolume", "Water");
}

AUTNoCameraVolume::AUTNoCameraVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	VolumeName = NSLOCTEXT("Volume", "NoCameraVolume", "Out of Bounds");
}

void AUTWaterVolume::ActorEnteredVolume(class AActor* Other)
{
	if (Other)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P)
		{
			P->EnteredWater(this);
		}
		else if (EntrySound)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), EntrySound, Other, SRT_None);
		}
		Super::ActorEnteredVolume(Other);
	}
}

void AUTWaterVolume::ActorLeavingVolume(class AActor* Other)
{
	if (Other)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P)
		{
			P->PlayWaterSound(ExitSound ? ExitSound : P->CharacterData.GetDefaultObject()->WaterExitSound);
		}
		Super::ActorLeavingVolume(Other);
	}
}

FVector AUTWaterVolume::GetCurrentFor_Implementation(AActor* Actor) const
{
	return WaterCurrentDirection.GetSafeNormal() * WaterCurrentSpeed;
}



