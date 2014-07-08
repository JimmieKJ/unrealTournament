// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeap_BioRifle.h"
#include "UTProj_BioShot.h"
#include "UTProj_BioGlob.h"
#include "UTWeaponStateFiringCharged.h"


AUTWeap_BioRifle::AUTWeap_BioRifle(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP.SetDefaultSubobjectClass<UUTWeaponStateFiringCharged>(TEXT("FiringState1")))
{
	if (FiringState.Num() > 1)
	{
#if WITH_EDITORONLY_DATA
		FiringStateType[1] = UUTWeaponStateFiringCharged::StaticClass();
#endif
	}

	FireInterval[0] = 0.33f;
	FireInterval[1] = 0.66f;

	GlobConsumeTime = 0.33f;

	MaxGlobStrength = 10;
	GlobStrength = 0;
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
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
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
	GetWorldTimerManager().SetTimer(this, &AUTWeap_BioRifle::IncreaseGlobStrength, GlobConsumeTime, false);
}

void AUTWeap_BioRifle::ClearGlobStrength()
{
	GlobStrength = 0;
	GetWorldTimerManager().ClearTimer(this, &AUTWeap_BioRifle::IncreaseGlobStrength);
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
			AUTProj_BioGlob* Glob = Cast<AUTProj_BioGlob>(FireProjectile());

			if (Glob != NULL)
			{
				Glob->SetGlobStrength(GlobStrength);
			}
		}
		OnChargeShot();
		PlayFiringEffects();
		ClearGlobStrength();
	}
	else
	{
		Super::FireShot();
	}
}

