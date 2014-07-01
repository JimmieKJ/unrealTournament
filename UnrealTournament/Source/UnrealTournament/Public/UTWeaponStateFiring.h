// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponState.h"

#include "UTWeaponStateFiring.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UUTWeaponStateFiring : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateFiring(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	// accessor for convenience
	inline uint8 GetFireMode()
	{
		return GetOuterAUTWeapon()->GetCurrentFireMode();
	}

	virtual bool IsFiring() const OVERRIDE
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

	/** called when the owner starts a pending fire for this mode (pressed button but may or may not move to this state yet)
	 * this function is called even if another fire mode is in use
	 * NOTE: the weapon is *not* in this state! This is intended for modes that can do something while another mode is active (e.g. zooming)
	 */
	virtual void PendingFireStarted()
	{}
	/** called when the owner stops a pending fire for this mode (released button regardless of whether they're actually firing this mode)
	* this function is called even if another fire mode is in use
	* NOTE: the weapon is *not* in this state! This is intended for modes that can do something while another mode is active (e.g. zooming)
	*/
	virtual void PendingFireStopped()
	{}
};