// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	UUTWeaponStateFiringChargedRocket(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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

	virtual void EndState() override
	{
		Super::EndState();

		// the super will only fire if there are rockets charged up, so we need to make sure to clear everything in case the loading never got that far
		ChargeTime = 0.0f;
		bCharging = false;
		RocketLauncher->NumLoadedRockets = 0;
		GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiring::RefireCheckTimer);
		GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiringChargedRocket::GraceTimer);
		GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiringChargedRocket::LoadTimer);
	}

	virtual void LoadTimer()
	{
		RocketLauncher->EndLoadRocket();

		//If we are fully loaded or out of ammo start the grace timer
		if (RocketLauncher->NumLoadedRockets >= RocketLauncher->MaxLoadedRockets || !RocketLauncher->HasAmmo(GetFireMode()))
		{
			// since ammo consumption is not locally simulated the client won't know if it needs to stop loading rockets early and thus we need to tell it
			if (!GetUTOwner()->IsLocallyControlled() && GetWorld()->GetNetMode() != NM_Client)
			{
				RocketLauncher->ClientAbortLoad();
			}
			GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiringChargedRocket::GraceTimer, RocketLauncher->GracePeriod, false);
		}
		//Fire delay for shooting one alternate rocket
		else if (!bCharging)
		{
			EndFiringSequence(GetFireMode());
		}
		else
		{
			RocketLauncher->BeginLoadRocket();

			GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiringChargedRocket::LoadTimer, RocketLauncher->GetLoadTime(), false);
		}
	}

	virtual void RefireCheckTimer() override
	{
		// query bot to consider whether to still fire, switch modes, etc
		AUTBot* B = Cast<AUTBot>(GetUTOwner()->Controller);
		if (B != NULL)
		{
			B->CheckWeaponFiring();
		}

		if (GetOuterAUTWeapon()->HandleContinuedFiring())
		{
			bCharging = true;
			BeginState(this);
		}
	}
	virtual void GraceTimer()
	{
		EndFiringSequence(GetFireMode());
	}

	virtual void UpdateTiming() override
	{
		Super::UpdateTiming();
		if (GetOuterAUTWeapon()->GetWorldTimerManager().IsTimerActive(this, &UUTWeaponStateFiringChargedRocket::LoadTimer))
		{
			float RemainingPct = GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerRemaining(this, &UUTWeaponStateFiringChargedRocket::LoadTimer) / GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerRate(this, &UUTWeaponStateFiringChargedRocket::LoadTimer);
			GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiringChargedRocket::LoadTimer, RocketLauncher->GetLoadTime() * RemainingPct, false);
		}
	}

	virtual void EndFiringSequence(uint8 FireModeNum) override
	{
		if (FireModeNum == GetFireMode())
		{
			bCharging = false;
			if (RocketLauncher->NumLoadedRockets > 0)
			{
				FireLoadedRocket();
			}
		}
	}

	virtual void FireLoadedRocket()
	{
		if (RocketLauncher->NumLoadedRockets > 0)
		{
			FireShot();
		}
		if (RocketLauncher->NumLoadedRockets > 0)
		{
			GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiringChargedRocket::FireLoadedRocket, RocketLauncher->BurstInterval, false); 
		}
		else
		{
			ChargeTime = 0.0f;
			if (GetOuterAUTWeapon()->GetCurrentState() == this)
			{
				GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), false);
			}

			GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiringChargedRocket::GraceTimer);
			GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiringChargedRocket::LoadTimer);
		}
	}
};
