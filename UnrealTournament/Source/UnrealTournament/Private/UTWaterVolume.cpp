// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWaterVolume.h"
#include "UTPainVolume.h"

AUTWaterVolume::AUTWaterVolume(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bWaterVolume = true;
	FluidFriction = 1.1f;
}

AUTPainVolume::AUTPainVolume(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bWaterVolume = true;
	FluidFriction = 1.5f;
}

void AUTWaterVolume::ActorEnteredVolume(class AActor* Other)
{
	if (Other && EntrySound)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), EntrySound, Other, SRT_None);
	}
	Super::ActorEnteredVolume(Other);
}

void AUTWaterVolume::ActorLeavingVolume(class AActor* Other)
{
	if (Other && ExitSound)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), ExitSound, Other, SRT_None);
	}
	Super::ActorLeavingVolume(Other);
}

void AUTPainVolume::ActorEnteredVolume(class AActor* Other)
{
	if (Other && EntrySound)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), EntrySound, Other, SRT_None);
	}
	Super::ActorEnteredVolume(Other);
}

void AUTPainVolume::ActorLeavingVolume(class AActor* Other)
{
	if (Other && ExitSound)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), ExitSound, Other, SRT_None);
	}
	Super::ActorLeavingVolume(Other);
}


