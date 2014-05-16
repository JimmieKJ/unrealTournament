// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateUnequipping.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UUTWeaponStateUnequipping : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateUnequipping(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	// set to amount of equip time that elapsed when exiting early, i.e. to go back up
	float PartialEquipTime;

	virtual void BeginState(const UUTWeaponState* PrevState) OVERRIDE;
	virtual void EndState() OVERRIDE
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateUnequipping::PutDownFinished);
	}

	void PutDownFinished()
	{
		GetOuterAUTWeapon()->GotoState(GetOuterAUTWeapon()->InactiveState);
		GetOuterAUTWeapon()->GetUTOwner()->WeaponChanged();
	}

	virtual void BringUp() OVERRIDE
	{
		PartialEquipTime = GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerElapsed(this, &UUTWeaponStateUnequipping::PutDownFinished);
		GetOuterAUTWeapon()->GotoState(GetOuterAUTWeapon()->EquippingState);
	}
};