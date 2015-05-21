// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeap_BioRifle.h"
#include "UTProj_BioShot.h"
#include "UTWeaponStateFiringCharged.h"
#include "StatNames.h"


AUTWeap_BioRifle::AUTWeap_BioRifle(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTWeaponStateFiringCharged>(TEXT("FiringState1")))
{
	ClassicGroup = 3;
	Ammo = 40;
	MaxAmmo = 100;
	AmmoCost[0] = 2;
	AmmoCost[1] = 2;
	FOVOffset = FVector(1.7f, 1.f, 1.f);

	FireInterval[0] = 0.4f;
	FireInterval[1] = 0.66f;

	GlobConsumeTime = 0.33f;

	MaxGlobStrength = 10;
	GlobStrength = 0;

	SqueezeFireInterval = 0.2f;
	SqueezeFireSpread = 0.3f;
	SqueezeAmmoCost = 1;

	if (FiringState.IsValidIndex(1) && Cast<UUTWeaponStateFiringCharged>(FiringState[1]) != NULL)
	{
		((UUTWeaponStateFiringCharged*)FiringState[1])->bChargeFlashCount = true;
	}
	KillStatsName = NAME_BioRifleKills;
	DeathStatsName = NAME_BioRifleDeaths;
}

void AUTWeap_BioRifle::UpdateSqueeze()
{
	if (GetUTOwner() && GetUTOwner()->IsPendingFire(1))
	{
		FireInterval[0] = SqueezeFireInterval;
		Spread[0] = SqueezeFireSpread;
		ProjClass[0] = SqueezeProjClass;
		AmmoCost[0] = SqueezeAmmoCost;
	}
	else
	{
		FireInterval[0] = GetClass()->GetDefaultObject<AUTWeapon>()->FireInterval[0];
		Spread[0] = GetClass()->GetDefaultObject<AUTWeapon>()->Spread[0];
		ProjClass[0] = GetClass()->GetDefaultObject<AUTWeapon>()->ProjClass[0];
		AmmoCost[0] = GetClass()->GetDefaultObject<AUTWeapon>()->AmmoCost[0];
	}
}

void AUTWeap_BioRifle::GotoFireMode(uint8 NewFireMode)
{
	if (NewFireMode == 0)
	{
		UpdateSqueeze();
	}
	Super::GotoFireMode(NewFireMode);
}

bool AUTWeap_BioRifle::HandleContinuedFiring()
{
	if (GetUTOwner())
	{
		// possible switch to squirt mode
		if (GetCurrentFireMode() == 0)
		{
			float OldFireInterval = FireInterval[0];
			// if currently in firemode 0, and 1 pressed, just switch mode
			UpdateSqueeze();

			if (FireInterval[0] != OldFireInterval)
			{
				UpdateTiming();
			}
		}
	}

	return Super::HandleContinuedFiring();
}

void AUTWeap_BioRifle::OnStartedFiring_Implementation()
{
	if (CurrentFireMode == 1)
	{
		StartCharge();
	}
}

void AUTWeap_BioRifle::OnContinuedFiring_Implementation()
{
	if (CurrentFireMode == 1)
	{
		StartCharge();
	}
}

void AUTWeap_BioRifle::StartCharge()
{
	ClearGlobStrength();
	IncreaseGlobStrength();
	OnStartCharging();

	//Play the charge animation
	if (GetNetMode() != NM_DedicatedServer && UTOwner != NULL)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != NULL && ChargeAnimation != NULL)
		{
			AnimInstance->Montage_Play(ChargeAnimation, 1.f);
		}
	}
}

void AUTWeap_BioRifle::OnStartCharging_Implementation()
{
}

void AUTWeap_BioRifle::OnChargeShot_Implementation()
{
}

void AUTWeap_BioRifle::IncreaseGlobStrength()
{
	if (GlobStrength < MaxGlobStrength && HasAmmo(CurrentFireMode))
	{
		GlobStrength++;
		ConsumeAmmo(CurrentFireMode);
	}
	GetWorldTimerManager().SetTimer(IncreaseGlobStrengthHandle, this, &AUTWeap_BioRifle::IncreaseGlobStrength, GlobConsumeTime / ((UTOwner != NULL) ? UTOwner->GetFireRateMultiplier() : 1.0f), false);

	// AI decision to release fire
	if (UTOwner != NULL)
	{
		AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
		if (B != NULL && CanAttack(B->GetTarget(), B->GetFocalPoint(), true))
		{
			if (GlobStrength >= MaxGlobStrength)
			{
				UTOwner->StopFiring();
			}
			// stop if bot thinks its charge amount is enough to kill target
			// TODO: maybe also if predicting target to go behind wall?
			else if (B->GetTarget() == B->GetEnemy() && ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL)
			{
				AUTProj_BioShot* DefaultGlob = Cast<AUTProj_BioShot>(ProjClass[CurrentFireMode]->GetDefaultObject());
				if (DefaultGlob != NULL)
				{
					AUTCharacter* P = Cast<AUTCharacter>(B->GetEnemy());
					if (P != NULL && P->HealthMax * B->GetEnemyInfo(B->GetEnemy(), true)->EffectiveHealthPct < DefaultGlob->DamageParams.BaseDamage * GlobStrength)
					{
						UTOwner->StopFiring();
					}
				}
			}
		}
	}
}

void AUTWeap_BioRifle::ClearGlobStrength()
{
	GlobStrength = 0;
	GetWorldTimerManager().ClearTimer(IncreaseGlobStrengthHandle);
}

void AUTWeap_BioRifle::UpdateTiming()
{
	Super::UpdateTiming();
	if (GetWorldTimerManager().IsTimerActive(IncreaseGlobStrengthHandle))
	{
		float RemainingPct = GetWorldTimerManager().GetTimerRemaining(IncreaseGlobStrengthHandle) / GetWorldTimerManager().GetTimerRate(IncreaseGlobStrengthHandle);
		GetWorldTimerManager().SetTimer(IncreaseGlobStrengthHandle, this, &AUTWeap_BioRifle::IncreaseGlobStrength, GlobConsumeTime / ((UTOwner != NULL) ? UTOwner->GetFireRateMultiplier() : 1.0f), false);
	}
}

void AUTWeap_BioRifle::OnStoppedFiring_Implementation()
{
	if (CurrentFireMode == 1)
	{
		ClearGlobStrength();
	}
}

void AUTWeap_BioRifle::FireShot()
{
	if (!FireShotOverride() && CurrentFireMode == 1)
	{
		if (ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL)
		{
			AUTProj_BioShot* Glob = Cast<AUTProj_BioShot>(FireProjectile());

			if (Glob != NULL)
			{
				Glob->SetGlobStrength(GlobStrength);
			}
		}
		OnChargeShot();
		PlayFiringEffects();
		ClearGlobStrength();
		if (GetUTOwner() != NULL)
		{
			static FName NAME_FiredWeapon(TEXT("FiredWeapon"));
			GetUTOwner()->InventoryEvent(NAME_FiredWeapon);
		}
	}
	else
	{
		Super::FireShot();
	}
}

