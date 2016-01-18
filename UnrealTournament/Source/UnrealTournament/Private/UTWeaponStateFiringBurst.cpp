// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringBurst.h"

UUTWeaponStateFiringBurst::UUTWeaponStateFiringBurst(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	BurstSize = 3;
	BurstInterval = 0.15f;
	SpreadIncrease = 0.06f;
}

void UUTWeaponStateFiringBurst::BeginState(const UUTWeaponState* PrevState)
{
	CurrentShot = 0;
	if (GetOuterAUTWeapon()->Spread.IsValidIndex(GetOuterAUTWeapon()->GetCurrentFireMode()))
	{
		GetOuterAUTWeapon()->Spread[GetOuterAUTWeapon()->GetCurrentFireMode()] = 0.f;
	}
	ShotTimeRemaining = -0.001f;
	RefireCheckTimer();
	GetOuterAUTWeapon()->OnStartedFiring();
}

void UUTWeaponStateFiringBurst::IncrementShotTimer()
{
	ShotTimeRemaining += (CurrentShot < BurstSize) ? BurstInterval : FMath::Max(0.01f, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()) - (BurstSize-1)*BurstInterval);
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

void UUTWeaponStateFiringBurst::PutDown()
{
	if ((CurrentShot == BurstSize) && (ShotTimeRemaining < FMath::Max(0.01f, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()) - BurstSize*BurstInterval)))
	{
		GetOuterAUTWeapon()->UnEquip();
	}
}

