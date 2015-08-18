// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeap_Enforcer.h"
#include "UTWeaponStateFiringBurstEnforcer.h"


UUTWeaponStateFiringBurstEnforcer::UUTWeaponStateFiringBurstEnforcer(const FObjectInitializer& OI)
: Super(OI)
{
	BurstSize = 3;
	BurstInterval = 0.15f;
	SpreadIncrease = 0.06f;
	LastFiredTime = 0.0f;

	bFirstVolleyComplete = false;
	bSecondVolleyComplete = false;

}

void UUTWeaponStateFiringBurstEnforcer::BeginState(const UUTWeaponState* PrevState)
{
	if (GetRemainingCooldownTime() == 0.0f || (Cast<AUTWeap_Enforcer>(GetOuterAUTWeapon())->bDualEnforcerMode && !bSecondVolleyComplete))
	{
		if (GetRemainingCooldownTime() == 0.0f)
		{
			bFirstVolleyComplete = false;
			bSecondVolleyComplete = false;
		}

		CurrentShot = 0;
		GetOuterAUTWeapon()->Spread[GetOuterAUTWeapon()->GetCurrentFireMode()] = 0.f;
		RefireCheckTimer();
		GetOuterAUTWeapon()->OnStartedFiring();
	}
	else
	{
		//set the timer to the proper refire time
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(RefireCheckHandle, this, &UUTWeaponStateFiringBurstEnforcer::RefireCheckTimer, GetRemainingCooldownTime(), true);
	}
}

void UUTWeaponStateFiringBurstEnforcer::ResetTiming()
{
	//Reset the timer if the time remaining on it is greater than the new FireRate
	if (GetOuterAUTWeapon()->GetUTOwner()->GetPendingWeapon() == NULL && GetOuterAUTWeapon()->HasAmmo(GetOuterAUTWeapon()->GetCurrentFireMode()))
	{
		if (GetWorld()->GetTimeSeconds() - LastFiredTime >= BurstInterval / GetUTOwner()->GetFireRateMultiplier())
		{
			RefireCheckTimer();
		}
	}
}

void UUTWeaponStateFiringBurstEnforcer::EndState()
{
	GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(RefireCheckHandle);
	GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(PutDownHandle);
	Super::EndState();
}

void UUTWeaponStateFiringBurstEnforcer::IncrementShotTimer()
{
	if ((!Cast<AUTWeap_Enforcer>(GetOuterAUTWeapon())->bDualEnforcerMode && (CurrentShot == BurstSize)) 
		|| (Cast<AUTWeap_Enforcer>(GetOuterAUTWeapon())->bDualEnforcerMode && (CurrentShot == BurstSize) && bFirstVolleyComplete))
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(RefireCheckHandle, this, &UUTWeaponStateFiringBurstEnforcer::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()) - (BurstSize - 1)*BurstInterval / GetUTOwner()->GetFireRateMultiplier(), true);
	}
	else
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(RefireCheckHandle, this, &UUTWeaponStateFiringBurstEnforcer::RefireCheckTimer, BurstInterval / GetUTOwner()->GetFireRateMultiplier(), true);
	}
}

void UUTWeaponStateFiringBurstEnforcer::UpdateTiming()
{
	// TODO: we should really restart the timer at the percentage it currently is, but FTimerManager has no facility to do this
	GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(RefireCheckHandle, this, &UUTWeaponStateFiringBurstEnforcer::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()) - (BurstSize - 1)*BurstInterval / GetUTOwner()->GetFireRateMultiplier(), true);
}


void UUTWeaponStateFiringBurstEnforcer::RefireCheckTimer()
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
			if (!bFirstVolleyComplete)
			{
				bFirstVolleyComplete = true;
			}
			else
			{
				bSecondVolleyComplete = true;
			}

			CurrentShot = 0;
			if (GetOuterAUTWeapon()->Spread.IsValidIndex(CurrentFireMode))
			{
				GetOuterAUTWeapon()->Spread[CurrentFireMode] = 0.f;
			}
		}

		bool bDualMode = Cast<AUTWeap_Enforcer>(GetOuterAUTWeapon())->bDualEnforcerMode;

		if ((bDualMode && !bSecondVolleyComplete) || (!bDualMode && !bFirstVolleyComplete) || GetRemainingCooldownTime() == 0.0f)
		{
			GetOuterAUTWeapon()->OnContinuedFiring();

			FireShot();
			LastFiredTime = GetOuterAUTWeapon()->GetWorld()->GetTimeSeconds();
			if (GetUTOwner() != NULL) // shot might have resulted in owner dying!
			{
				CurrentShot++;
				if (GetOuterAUTWeapon()->Spread.IsValidIndex(CurrentFireMode))
				{
					GetOuterAUTWeapon()->Spread[CurrentFireMode] += SpreadIncrease;
				}
				IncrementShotTimer();

				if ((bDualMode && bSecondVolleyComplete) || (!bDualMode && bFirstVolleyComplete))
				{
					bFirstVolleyComplete = false;
					bSecondVolleyComplete = false;
				}
			}
		}
		else if (!GetOuterAUTWeapon()->GetUTOwner()->IsPendingFire(CurrentFireMode))
		{
			GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(RefireCheckHandle, this, &UUTWeaponStateFiringBurstEnforcer::RefireCheckTimer, GetRemainingCooldownTime(), true);
		}
		else
		{
			GetOuterAUTWeapon()->GotoActiveState();
		}
	}
	else
	{
		if (!bFirstVolleyComplete)
		{
			bFirstVolleyComplete = true;
		}
		else
		{
			bSecondVolleyComplete = true;
		}
		GetOuterAUTWeapon()->GotoActiveState();
	}
}

float UUTWeaponStateFiringBurstEnforcer::GetRemainingCooldownTime()
{
	float CooldownTime = GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()) - (BurstSize - 1)*BurstInterval / GetUTOwner()->GetFireRateMultiplier();
	return FMath::Max<float>(0.0f, (CooldownTime + LastFiredTime) - GetOuterAUTWeapon()->GetWorld()->GetTimeSeconds());
}

void UUTWeaponStateFiringBurstEnforcer::PutDown()
{
	HandleDelayedShot();
	if ((CurrentShot == BurstSize) && GetRemainingCooldownTime() == 0.0f)
	{
		GetOuterAUTWeapon()->UnEquip();
	}
}

