// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealTournament.h"
#include "UTWeaponStateFiring_Enforcer.h"

void UUTWeaponStateFiring_Enforcer::BeginState(const UUTWeaponState* PrevState)
{
	GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(RefireCheckHandle, this, &UUTWeaponStateFiring_Enforcer::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), true);
	ToggleLoopingEffects(true);
	GetOuterAUTWeapon()->OnStartedFiring();
	FireShot();
}
void UUTWeaponStateFiring_Enforcer::EndState()
{
	ToggleLoopingEffects(false);
	GetOuterAUTWeapon()->OnStoppedFiring();
	GetOuterAUTWeapon()->StopFiringEffects();
	GetOuterAUTWeapon()->GetUTOwner()->ClearFiringInfo();
	GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(RefireCheckHandle);
	GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(PutDownHandle);
}

void UUTWeaponStateFiring_Enforcer::UpdateTiming()
{
	// TODO: we should really restart the timer at the percentage it currently is, but FTimerManager has no facility to do this
	GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(RefireCheckHandle, this, &UUTWeaponStateFiring_Enforcer::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), true);
}

void UUTWeaponStateFiring_Enforcer::PutDown()
{
	HandleDelayedShot();

	// by default, firing states delay put down until the weapon returns to active via player letting go of the trigger, out of ammo, etc
	// However, allow putdown time to overlap with reload time - start a timer to do an early check
	float TimeTillPutDown = GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerRemaining(RefireCheckHandle);
	if (TimeTillPutDown <= GetOuterAUTWeapon()->GetPutDownTime())
	{
		Super::PutDown();
	}
	else
	{
		TimeTillPutDown -= GetOuterAUTWeapon()->GetPutDownTime();
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(PutDownHandle, this, &UUTWeaponStateFiring_Enforcer::PutDown, TimeTillPutDown, false);
	}
}