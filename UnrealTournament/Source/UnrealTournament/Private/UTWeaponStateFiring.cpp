// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"

void UUTWeaponStateFiring::BeginState(const UUTWeaponState* PrevState)
{
	GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), true);
	ToggleLoopingEffects(true);
	GetOuterAUTWeapon()->OnStartedFiring();
	FireShot();
}
void UUTWeaponStateFiring::EndState()
{
	ToggleLoopingEffects(false);
	GetOuterAUTWeapon()->OnStoppedFiring();
	GetOuterAUTWeapon()->StopFiringEffects();
	GetOuterAUTWeapon()->GetUTOwner()->ClearFiringInfo();
	GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiring::RefireCheckTimer);
	GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiring::PutDown);
}

void UUTWeaponStateFiring::ToggleLoopingEffects(bool bNowOn)
{
	if (GetOuterAUTWeapon()->FireLoopingSound.IsValidIndex(GetFireMode()) && GetOuterAUTWeapon()->FireLoopingSound[GetFireMode()] != NULL)
	{
		GetUTOwner()->SetAmbientSound(GetOuterAUTWeapon()->FireLoopingSound[GetFireMode()], !bNowOn);
	}
}

void UUTWeaponStateFiring::UpdateTiming()
{
	// TODO: we should really restart the timer at the percentage it currently is, but FTimerManager has no facility to do this
	GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), true);
}

void UUTWeaponStateFiring::RefireCheckTimer()
{
	if (GetOuterAUTWeapon()->GetUTOwner()->GetPendingWeapon() != NULL || !GetOuterAUTWeapon()->GetUTOwner()->IsPendingFire(GetOuterAUTWeapon()->GetCurrentFireMode()) || !GetOuterAUTWeapon()->HasAmmo(GetOuterAUTWeapon()->GetCurrentFireMode()))
	{
		GetOuterAUTWeapon()->GotoActiveState();
	}
	else
	{
		GetOuterAUTWeapon()->OnContinuedFiring();
		FireShot();
	}
}

void UUTWeaponStateFiring::FireShot()
{
	GetOuterAUTWeapon()->FireShot();
}

void UUTWeaponStateFiring::PutDown()
{
	// by default, firing states delay put down until the weapon returns to active via player letting go of the trigger, out of ammo, etc
	// However, allow putdown time to overlap with reload time - start a timer to do an early check
	float TimeTillPutDown = GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerRemaining(this, &UUTWeaponStateFiring::RefireCheckTimer);
	if (TimeTillPutDown <= GetOuterAUTWeapon()->PutDownTime)
	{
		Super::PutDown();
	}
	else
	{
		TimeTillPutDown -= GetOuterAUTWeapon()->PutDownTime;
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::PutDown, TimeTillPutDown, false);
	}
}

// FIX PUTDOWN FOR CHARGING modes
