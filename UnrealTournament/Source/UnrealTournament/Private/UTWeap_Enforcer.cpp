// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Enforcer.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringBurst.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTImpactEffect.h"

AUTWeap_Enforcer::AUTWeap_Enforcer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Ammo = 20;
	MaxAmmo = 40;
	LastFireTime = 0.f;
	SpreadResetInterval = 1.f;
	SpreadIncrease = 0.03f;
	MaxSpread = 0.12f;
	BringUpTime = 0.3f;
	PutDownTime = 0.2f;
	StoppingPower = 30000.f;
	BaseAISelectRating = 0.4f;
}

float AUTWeap_Enforcer::GetImpartedMomentumMag(AActor* HitActor)
{
	AUTCharacter* HitChar = Cast<AUTCharacter>(HitActor);
	return (HitChar && HitChar->GetWeapon() && HitChar->GetWeapon()->bAffectedByStoppingPower)
		? StoppingPower
		: InstantHitInfo[CurrentFireMode].Momentum;
}

void AUTWeap_Enforcer::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
	// burst mode takes care of spread variation itself
	if (!Cast<UUTWeaponStateFiringBurst>(FiringState[GetCurrentFireMode()]))
	{
		float TimeSinceFired = UTOwner->GetWorld()->GetTimeSeconds() - LastFireTime;
		float SpreadScalingOverTime = FMath::Max(0.f, 1.f - (TimeSinceFired - FireInterval[GetCurrentFireMode()]) / (SpreadResetInterval - FireInterval[GetCurrentFireMode()]));
		Spread[GetCurrentFireMode()] = FMath::Min(MaxSpread, Spread[GetCurrentFireMode()] + SpreadIncrease) * SpreadScalingOverTime;
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
		UTOwner->TargetEyeOffset.X = FiringViewKickback;
		// @TODO FIXMESTEVE is this causing first person muzzle flash for bots in standalone?
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