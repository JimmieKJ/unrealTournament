// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiringCharged.generated.h"

UCLASS(CustomConstructor)
class UUTWeaponStateFiringCharged : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateFiringCharged(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	bool bCharging;
	float ChargeTime;

	virtual void BeginState(const UUTWeaponState* PrevState) OVERRIDE
	{
		GetOuterAUTWeapon()->OnStartedFiring();
		bCharging = true;
	}
	virtual void UpdateTiming() OVERRIDE
	{
		if (!bCharging)
		{
			Super::UpdateTiming();
		}
	}
	virtual void RefireCheckTimer() OVERRIDE
	{
		if (GetOuterAUTWeapon()->GetUTOwner()->GetPendingWeapon() != NULL || !GetOuterAUTWeapon()->GetUTOwner()->IsPendingFire(GetOuterAUTWeapon()->GetCurrentFireMode()) || !GetOuterAUTWeapon()->HasAmmo(GetOuterAUTWeapon()->GetCurrentFireMode()))
		{
			GetOuterAUTWeapon()->GotoActiveState();
		}
		else
		{
			bCharging = true;
		}
	}
	virtual void EndFiringSequence(uint8 FireModeNum)
	{
		FireShot();
		bCharging = false;
		ChargeTime = 0.0f;
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), true);
	}
	virtual void Tick(float DeltaTime) OVERRIDE
	{
		if (bCharging)
		{
			ChargeTime += DeltaTime * GetOuterAUTWeapon()->GetUTOwner()->GetFireRateMultiplier();
		}
	}
};