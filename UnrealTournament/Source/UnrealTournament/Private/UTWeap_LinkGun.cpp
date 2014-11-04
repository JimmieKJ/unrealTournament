// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_LinkGun.h"
#include "UTProj_BioShot.h"

AUTWeap_LinkGun::AUTWeap_LinkGun(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	LinkedBio = NULL;
}

void AUTWeap_LinkGun::PlayImpactEffects(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	FVector ModifiedTargetLoc = TargetLoc;

	if (LinkedBio && (LinkedBio->IsPendingKillPending() || (FireMode != 1) || ((TargetLoc - LinkedBio->GetActorLocation()).Size() > LinkedBio->MaxLinkDistance)))
	{
		LinkedBio = NULL;
	}
	if (LinkedBio)
	{
		ModifiedTargetLoc = LinkedBio->GetActorLocation();
	}
	Super::PlayImpactEffects(ModifiedTargetLoc, FireMode, SpawnLocation, SpawnRotation);
}

void AUTWeap_LinkGun::StopFire(uint8 FireModeNum)
{
	LinkedBio = NULL;
	Super::StopFire(FireModeNum);
}

void AUTWeap_LinkGun::ServerStopFire_Implementation(uint8 FireModeNum)
{
	LinkedBio = NULL;
	Super::ServerStopFire_Implementation(FireModeNum);
}

