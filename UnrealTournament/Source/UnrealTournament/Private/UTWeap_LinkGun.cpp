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

	if (LinkedBio) 
	{
		if (LinkedBio->IsPendingKillPending() || (FireMode != 1) || !UTOwner || ((UTOwner->GetActorLocation() - LinkedBio->GetActorLocation()).Size() > LinkedBio->MaxLinkDistance + InstantHitInfo[1].TraceRange))
		{
			LinkedBio = NULL;
		}
		else
		{
			// verify line of sight
			FHitResult Hit;
			static FName NAME_BioLinkTrace(TEXT("BioLinkTrace"));
			bool bBlockingHit = GetWorld()->LineTraceSingle(Hit, SpawnLocation, LinkedBio->GetActorLocation(), COLLISION_TRACE_WEAPON, FCollisionQueryParams(NAME_BioLinkTrace, false, UTOwner));
			if ((bBlockingHit || (Hit.Actor != NULL)) && !Cast<AUTProj_BioShot>(Hit.Actor.Get()))
			{
				LinkedBio = NULL;
			}
		}
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

