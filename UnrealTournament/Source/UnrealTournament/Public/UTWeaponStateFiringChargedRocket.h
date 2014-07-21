// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiringCharged.h"
#include "UTWeaponStateFiringChargedRocket.generated.h"

/**
 * 
 */
UCLASS(CustomConstructor)
class UUTWeaponStateFiringChargedRocket : public UUTWeaponStateFiringCharged
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	AUTWeap_RocketLauncher* RocketLauncher;

	UUTWeaponStateFiringChargedRocket(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	virtual void BeginState(const UUTWeaponState* PrevState) override
	{
		RocketLauncher = Cast<AUTWeap_RocketLauncher>(GetOuterAUTWeapon());
		bCharging = true;

		//Only used for the rocket launcher
		if (RocketLauncher == NULL)
		{
			UE_LOG(UT, Warning, TEXT("%s::BeginState(): Weapon is not AUTWeap_RocketLauncher"));
			GetOuterAUTWeapon()->GotoActiveState();
			return;
		}

		RocketLauncher->BeginLoadRocket();
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiringChargedRocket::LoadTimer, RocketLauncher->GetLoadTime(), false);
	}

	virtual void LoadTimer()
	{
		RocketLauncher->EndLoadRocket();

		//If we are fully loaded or out of ammo start the grace timer
		if (RocketLauncher->NumLoadedRockets >= RocketLauncher->MaxLoadedRockets || !RocketLauncher->HasAmmo(GetFireMode()))
		{
			GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiringChargedRocket::GraceTimer, RocketLauncher->GracePeriod, false);
			return;
		}

		//Fire delay for shooting one alternate rocket
		if (!bCharging)
		{
			EndFiringSequence(GetFireMode());
			return;
		}

		RocketLauncher->BeginLoadRocket();

		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiringChargedRocket::LoadTimer, RocketLauncher->GetLoadTime(), false);
	}

	virtual void RefireCheckTimer() override
	{
		if (GetOuterAUTWeapon()->GetUTOwner()->GetPendingWeapon() != NULL || !GetOuterAUTWeapon()->GetUTOwner()->IsPendingFire(GetOuterAUTWeapon()->GetCurrentFireMode()) || !GetOuterAUTWeapon()->HasAmmo(GetOuterAUTWeapon()->GetCurrentFireMode()))
		{
			GetOuterAUTWeapon()->GotoActiveState();
			return;
		}
		else
		{
			GetOuterAUTWeapon()->OnContinuedFiring();
			bCharging = true;
			BeginState(this);
		}
	}
	virtual void GraceTimer()
	{
		EndFiringSequence(GetFireMode());
	}

	virtual void EndFiringSequence(uint8 FireModeNum) override
	{
		if (FireModeNum == GetFireMode())
		{
			bCharging = false;

			if (RocketLauncher->NumLoadedRockets > 0)
			{
				FireShot();
				ChargeTime = 0.0f;
				if (GetOuterAUTWeapon()->GetCurrentState() == this)
				{
					GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), false);
				}

				GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiringChargedRocket::GraceTimer);
				GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiringChargedRocket::LoadTimer);
			}
		}
	}
};
