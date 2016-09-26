// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateActive.h"
#include "UTWeaponStateFiring.h"

void UUTWeaponStateActive::BeginState(const UUTWeaponState* PrevState)
{
	// see if we need to process a pending putdown
	AUTCharacter* UTOwner = GetOuterAUTWeapon()->GetUTOwner();
	if (UTOwner->GetPendingWeapon() == NULL || !GetOuterAUTWeapon()->PutDown())
	{
		// check for any firemode already pending
		for (uint8 i = 0; i < GetOuterAUTWeapon()->GetNumFireModes(); i++)
		{
			if (UTOwner->IsPendingFire(i) && GetOuterAUTWeapon()->HasAmmo(i))
			{
				GetOuterAUTWeapon()->CurrentFireMode = i;
				GetOuterAUTWeapon()->GotoState(GetOuterAUTWeapon()->FiringState[i]);
				return;
			}
		}
	}
	if (UTOwner->PendingAutoSwitchWeapon && !UTOwner->PendingAutoSwitchWeapon->IsPendingKillPending() && UTOwner->IsInInventory(UTOwner->PendingAutoSwitchWeapon) && (UTOwner->PendingAutoSwitchWeapon != GetOuterAUTWeapon()) && UTOwner->PendingAutoSwitchWeapon->HasAnyAmmo())
	{
		UTOwner->SwitchWeapon(UTOwner->PendingAutoSwitchWeapon);
	}
	UTOwner->PendingAutoSwitchWeapon = nullptr;
}

bool UUTWeaponStateActive::BeginFiringSequence(uint8 FireModeNum, bool bClientFired)
{
	if (GetOuterAUTWeapon()->FiringState.IsValidIndex(FireModeNum) && GetOuterAUTWeapon()->HasAmmo(FireModeNum))
	{
		GetOuterAUTWeapon()->CurrentFireMode = FireModeNum;
		GetOuterAUTWeapon()->GotoState(GetOuterAUTWeapon()->FiringState[FireModeNum]);
		return true;
	}
	return false;
}

bool UUTWeaponStateActive::WillSpawnShot(float DeltaTime)
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetOuterAUTWeapon()->GetUTOwner()->GetController());
	return UTPC && UTPC->HasDeferredFireInputs();
}