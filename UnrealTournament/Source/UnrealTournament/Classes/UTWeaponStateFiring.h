// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UUTWeaponStateFiring : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateFiring(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	virtual bool IsFiring() OVERRIDE
	{
		// default is we're firing if we're in this state
		return true;
	}

	/** called to fire the shot and consume ammo */
	virtual void FireShot();

	virtual void BeginState(const UUTWeaponState* PrevState) OVERRIDE;
	virtual void EndState() OVERRIDE;
	virtual void UpdateTiming() OVERRIDE;
	/** called after the refire delay to see what we should do next (generally, fire or go back to active state) */
	virtual void RefireCheckTimer();

	virtual void PutDown() OVERRIDE
	{
		// by default, firing states delay put down until the weapon returns to active via player letting go of the trigger, out of ammo, etc
	}
};