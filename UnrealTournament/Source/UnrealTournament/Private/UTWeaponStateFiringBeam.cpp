// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringBeam.h"

UUTWeaponStateFiringBeam::UUTWeaponStateFiringBeam(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MinDamage = 5;
}

void UUTWeaponStateFiringBeam::FireShot()
{
	// consume ammo but don't fire from here
	GetOuterAUTWeapon()->PlayFiringEffects();
	GetOuterAUTWeapon()->ConsumeAmmo(GetOuterAUTWeapon()->GetCurrentFireMode());
	if (GetUTOwner() != NULL)
	{
		static FName NAME_FiredWeapon(TEXT("FiredWeapon"));
		GetUTOwner()->InventoryEvent(NAME_FiredWeapon);
	}
}

void UUTWeaponStateFiringBeam::Tick(float DeltaTime)
{
	HandleDelayedShot();
	if (!GetOuterAUTWeapon()->FireShotOverride() && GetOuterAUTWeapon()->InstantHitInfo.IsValidIndex(GetOuterAUTWeapon()->GetCurrentFireMode()))
	{
		const FInstantHitDamageInfo& DamageInfo = GetOuterAUTWeapon()->InstantHitInfo[GetOuterAUTWeapon()->GetCurrentFireMode()];
		FHitResult Hit;
		GetOuterAUTWeapon()->FireInstantHit(false, &Hit);
		if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged)
		{
			Accumulator += float(DamageInfo.Damage) / GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()) * DeltaTime;
			if (Accumulator >= MinDamage)
			{
				int32 AppliedDamage = FMath::TruncToInt(Accumulator);
				Accumulator -= AppliedDamage;
				FVector FireDir = (Hit.Location - Hit.TraceStart).GetSafeNormal();
				Hit.Actor->TakeDamage(AppliedDamage, FUTPointDamageEvent(AppliedDamage, Hit, FireDir, DamageInfo.DamageType, FireDir * (GetOuterAUTWeapon()->GetImpartedMomentumMag(Hit.Actor.Get()) * float(AppliedDamage) / float(DamageInfo.Damage))), GetOuterAUTWeapon()->GetUTOwner()->Controller, GetOuterAUTWeapon());
			}
		}
		// beams show a clientside beam target
		if (GetOuterAUTWeapon()->Role < ROLE_Authority && GetOuterAUTWeapon()->GetUTOwner() != NULL) // might have lost owner due to TakeDamage() call above!
		{
			GetOuterAUTWeapon()->GetUTOwner()->SetFlashLocation(Hit.Location, GetOuterAUTWeapon()->GetCurrentFireMode());
		}
	}
}