// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"

void UUTWeaponStateFiring::BeginState(const UUTWeaponState* PrevState)
{
	GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), true);
	FireShot();
}
void UUTWeaponStateFiring::EndState()
{
	GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateFiring::RefireCheckTimer);
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
		FireShot();
	}
}

void UUTWeaponStateFiring::FireShot()
{
	GetOuterAUTWeapon()->FireShot();
}