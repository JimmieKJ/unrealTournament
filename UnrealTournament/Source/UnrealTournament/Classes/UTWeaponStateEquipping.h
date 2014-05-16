// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateEquipping.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UUTWeaponStateEquipping : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateEquipping(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	// set to amount of equip time that elapsed when exiting early, i.e. to go back down
	float PartialEquipTime;

	virtual void BeginState(const UUTWeaponState* PrevState) OVERRIDE;
	virtual void EndState() OVERRIDE
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(this, &UUTWeaponStateEquipping::BringUpFinished);
	}

	void BringUpFinished()
	{
		GetOuterAUTWeapon()->GotoActiveState();
	}

	virtual void PutDown() OVERRIDE
	{
		PartialEquipTime = GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerElapsed(this, &UUTWeaponStateEquipping::BringUpFinished);
		GetOuterAUTWeapon()->GotoState(GetOuterAUTWeapon()->UnequippingState);
	}
};