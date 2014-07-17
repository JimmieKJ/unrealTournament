// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"

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

	virtual void BeginState(const UUTWeaponState* PrevState) override
	{
		if (GetOuterAUTWeapon()->FireLoopingSound.IsValidIndex(GetFireMode()) && GetOuterAUTWeapon()->FireLoopingSound[GetFireMode()] != NULL)
		{
			GetUTOwner()->SetAmbientSound(GetOuterAUTWeapon()->FireLoopingSound[GetFireMode()]);
		}
		GetOuterAUTWeapon()->OnStartedFiring();
		bCharging = true;
	}
	virtual void EndState() override
	{
		if (bCharging)
		{
			EndFiringSequence(GetFireMode());
		}
		Super::EndState();
	}
	virtual void UpdateTiming() override
	{
		if (!bCharging)
		{
			Super::UpdateTiming();
		}
	}
	virtual void RefireCheckTimer() override
	{
		if (GetOuterAUTWeapon()->GetUTOwner()->GetPendingWeapon() != NULL || !GetOuterAUTWeapon()->GetUTOwner()->IsPendingFire(GetOuterAUTWeapon()->GetCurrentFireMode()) || !GetOuterAUTWeapon()->HasAmmo(GetOuterAUTWeapon()->GetCurrentFireMode()))
		{
			GetOuterAUTWeapon()->GotoActiveState();
		}
		else
		{
			GetOuterAUTWeapon()->OnContinuedFiring();
			bCharging = true;
		}
	}
	virtual void EndFiringSequence(uint8 FireModeNum)
	{
		if (bCharging && FireModeNum == GetFireMode())
		{
			FireShot();
			bCharging = false;
			ChargeTime = 0.0f;
			if (GetOuterAUTWeapon()->GetCurrentState() == this)
			{
				GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), false);
			}
		}
	}
	virtual void Tick(float DeltaTime) override
	{
		if (bCharging)
		{
			ChargeTime += DeltaTime * GetOuterAUTWeapon()->GetUTOwner()->GetFireRateMultiplier();
		}
	}
};