// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Enforcer.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringBurst.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTImpactEffect.h"

AUTWeap_Enforcer::AUTWeap_Enforcer(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	Ammo = 20;
	MaxAmmo = 40;
	LastFireTime = 0.f;
	SpreadResetInterval = 1.f;
	SpreadIncrease = 0.04f;
	MaxSpread = 0.15f;
}

void AUTWeap_Enforcer::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
	// burst mode takes care of spread variation itself
	if (!Cast<UUTWeaponStateFiringBurst>(FiringState[GetCurrentFireMode()]))
	{
		if (UTOwner->GetWorld()->GetTimeSeconds() - LastFireTime > SpreadResetInterval)
		{
			Spread[GetCurrentFireMode()] = 0.f;
		}
		else
		{
			Spread[GetCurrentFireMode()] = FMath::Min(MaxSpread, Spread[GetCurrentFireMode()] + SpreadIncrease);
		}
	}
	Super::FireInstantHit(bDealDamage, OutHit);
	if (UTOwner)
	{
		LastFireTime = UTOwner->GetWorld()->GetTimeSeconds();
	}
}

void AUTWeap_Enforcer::PlayFiringEffects()
{
	UUTWeaponStateFiringBurst* BurstFireMode = Cast<UUTWeaponStateFiringBurst>(FiringState[GetCurrentFireMode()]);
	if (!BurstFireMode || (BurstFireMode->CurrentShot == 0))
	{
		Super::PlayFiringEffects();
	}
	else if ((UTOwner != NULL) && (GetNetMode() != NM_DedicatedServer))
	{
		// muzzle flash
		if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL && MuzzleFlash[CurrentFireMode]->Template != NULL)
		{
			// if we detect a looping particle system, then don't reactivate it
			if (!MuzzleFlash[CurrentFireMode]->bIsActive || !IsLoopingParticleSystem(MuzzleFlash[CurrentFireMode]->Template))
			{
				MuzzleFlash[CurrentFireMode]->ActivateSystem();
			}
		}
	}
}