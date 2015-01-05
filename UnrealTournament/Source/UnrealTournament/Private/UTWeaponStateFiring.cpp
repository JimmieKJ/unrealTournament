// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"

void UUTWeaponStateFiring::BeginState(const UUTWeaponState* PrevState)
{
	GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), true);
	ToggleLoopingEffects(true);
	PendingFireSequence = -1;
	GetOuterAUTWeapon()->OnStartedFiring();
	FireShot();
}
void UUTWeaponStateFiring::EndState()
{
	ToggleLoopingEffects(false);
	GetOuterAUTWeapon()->OnStoppedFiring();
	GetOuterAUTWeapon()->StopFiringEffects();
	GetOuterAUTWeapon()->GetUTOwner()->ClearFiringInfo();
	GetOuterAUTWeapon()->GetWorldTimerManager().ClearAllTimersForObject(this);
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

bool UUTWeaponStateFiring::WillSpawnShot(float DeltaTime)
{
	return (GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerRemaining(this, &UUTWeaponStateFiring::RefireCheckTimer) < DeltaTime);
}

void UUTWeaponStateFiring::RefireCheckTimer()
{
	// query bot to consider whether to still fire, switch modes, etc
	AUTBot* B = Cast<AUTBot>(GetUTOwner()->Controller);
	if (B != NULL)
	{
		B->CheckWeaponFiring();
	}
	if (PendingFireSequence >= 0)
	{
		GetUTOwner()->SetPendingFire(PendingFireSequence, true);
		PendingFireSequence = -1;
	}

	if (GetOuterAUTWeapon()->HandleContinuedFiring())
	{
		FireShot();
	}
}

void UUTWeaponStateFiring::FireShot()
{
	//float CurrentMoveTime = (GetUTOwner() && GetUTOwner()->UTCharacterMovement) ? GetUTOwner()->UTCharacterMovement->GetCurrentSynchTime() : GetWorld()->GetTimeSeconds();
	//UE_LOG(UT, Warning, TEXT("Fire SHOT at %f (world time %f)"), CurrentMoveTime, GetWorld()->GetTimeSeconds());
	GetOuterAUTWeapon()->FireShot();
}

void UUTWeaponStateFiring::PutDown()
{
	// by default, firing states delay put down until the weapon returns to active via player letting go of the trigger, out of ammo, etc
	// However, allow putdown time to overlap with reload time - start a timer to do an early check
	float TimeTillPutDown = GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerRemaining(this, &UUTWeaponStateFiring::RefireCheckTimer);
	if (TimeTillPutDown <= GetOuterAUTWeapon()->GetPutDownTime())
	{
		Super::PutDown();
	}
	else
	{
		TimeTillPutDown -= GetOuterAUTWeapon()->GetPutDownTime();
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::PutDown, TimeTillPutDown, false);
	}
}

bool UUTWeaponStateFiring::BeginFiringSequence(uint8 FireModeNum, bool bClientFired)
{
	// on server, might not be quite done reloading yet when client done, so queue firing
	if (bClientFired)
	{
		PendingFireSequence = FireModeNum;
		GetUTOwner()->NotifyPendingServerFire();
	}
	return false;
}


// FIX PUTDOWN FOR CHARGING modes
