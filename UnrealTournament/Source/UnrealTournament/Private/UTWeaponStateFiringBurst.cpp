// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringBurst.h"

UUTWeaponStateFiringBurst::UUTWeaponStateFiringBurst(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	BurstSize = 3;
	BurstInterval = 0.15f;
	SpreadIncrease = 0.06f;
}

void UUTWeaponStateFiringBurst::BeginState(const UUTWeaponState* PrevState)
{
	CurrentShot = 0;
	ShotTimeRemaining = -0.001f;
	RefireCheckTimer();
	IncrementShotTimer();
	GetOuterAUTWeapon()->OnStartedFiring();
}

void UUTWeaponStateFiringBurst::IncrementShotTimer()
{
	ShotTimeRemaining += (CurrentShot < BurstSize) ? BurstInterval : GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode());
}

void UUTWeaponStateFiringBurst::UpdateTiming()
{
	// unnecessary since we're manually incrementing
}

void UUTWeaponStateFiringBurst::RefireCheckTimer()
{
	uint8 CurrentFireMode = GetOuterAUTWeapon()->GetCurrentFireMode();
	if (GetOuterAUTWeapon()->GetUTOwner()->GetPendingWeapon() != NULL || !GetOuterAUTWeapon()->HasAmmo(CurrentFireMode))
	{
		GetOuterAUTWeapon()->GotoActiveState();
	}
	else if ((CurrentShot < BurstSize) || GetOuterAUTWeapon()->GetUTOwner()->IsPendingFire(CurrentFireMode))
	{
		if (CurrentShot == BurstSize)
		{
			CurrentShot = 0;
			if (GetOuterAUTWeapon()->Spread.IsValidIndex(CurrentFireMode))
			{
				GetOuterAUTWeapon()->Spread[CurrentFireMode] = 0.f;
			}
		}
		GetOuterAUTWeapon()->OnContinuedFiring();
		FireShot();
		CurrentShot++;
		if (GetOuterAUTWeapon()->Spread.IsValidIndex(CurrentFireMode))
		{
			GetOuterAUTWeapon()->Spread[CurrentFireMode] += SpreadIncrease;
		}
		IncrementShotTimer();
	}
	else
	{
		GetOuterAUTWeapon()->GotoActiveState();
	}
}

void UUTWeaponStateFiringBurst::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ShotTimeRemaining -= DeltaTime * GetUTOwner()->GetFireRateMultiplier();
	if (ShotTimeRemaining <= 0.0f)
	{
		RefireCheckTimer();
	}
}
