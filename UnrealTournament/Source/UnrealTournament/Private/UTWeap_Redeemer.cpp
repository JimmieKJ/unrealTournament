// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Redeemer.h"

AUTWeap_Redeemer::AUTWeap_Redeemer(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	BringUpTime = 1.8f;
	PutDownTime = 1.44f;
	Ammo = 1;
	MaxAmmo = 1;
	FiringViewKickback = -50.f;
	bMustBeHolstered = true;
}

AUTProjectile* AUTWeap_Redeemer::FireProjectile()
{
	if (GetUTOwner() == NULL)
	{
		UE_LOG(UT, Warning, TEXT("%s::FireProjectile(): Weapon is not owned (owner died during firing sequence)"));
		return NULL;
	}
	else if (Role == ROLE_Authority)
	{
		if (CurrentFireMode == 0)
		{
			return Super::FireProjectile();
		}
		else
		{
			// try and fire a projectile
			const FVector SpawnLocation = GetFireStartLoc();
			const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);

			//DrawDebugSphere(GetWorld(), SpawnLocation, 10, 10, FColor::Green, true);

			UTOwner->IncrementFlashCount(CurrentFireMode);

			// spawn the projectile at the muzzle
			FActorSpawnParameters Params;
			Params.Instigator = UTOwner;
			AUTRemoteRedeemer* RemoteRedeemer = GetWorld()->SpawnActor<AUTRemoteRedeemer>(RemoteRedeemerClass, SpawnLocation, SpawnRotation, Params);
			if (RemoteRedeemer)
			{
				if (UTOwner && UTOwner->Controller)
				{
					RemoteRedeemer->SetOwner(UTOwner->Controller);
					RemoteRedeemer->ForceReplication();
					RemoteRedeemer->TryToDrive(UTOwner);
				}

				RemoteRedeemer->CollisionComp->bGenerateOverlapEvents = true;
			}
			else
			{
				UE_LOG(UT, Warning, TEXT("Could not spawn remote redeemer"));
			}

			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}