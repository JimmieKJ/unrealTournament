// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeaponStateUnequipping.h"

void UUTWeaponStateUnequipping::BeginState(const UUTWeaponState* PrevState)
{
	const UUTWeaponStateEquipping* PrevEquip = Cast<UUTWeaponStateEquipping>(PrevState);
	float EquipTime = (PrevEquip != NULL) ? PrevEquip->PartialEquipTime : GetOuterAUTWeapon()->PutDownTime;
	if (EquipTime <= 0.0f)
	{
		PutDownFinished();
	}
	else
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateUnequipping::PutDownFinished, EquipTime);
		// TODO: anim
	}
}

void UUTWeaponStateEquipping::BeginState(const UUTWeaponState* PrevState)
{
	const UUTWeaponStateUnequipping* PrevEquip = Cast<UUTWeaponStateUnequipping>(PrevState);
	float EquipTime = (PrevEquip != NULL) ? PrevEquip->PartialEquipTime : GetOuterAUTWeapon()->PutDownTime;
	if (EquipTime <= 0.0f)
	{
		BringUpFinished();
	}
	else
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateEquipping::BringUpFinished, EquipTime);
		// TODO: anim
	}
}