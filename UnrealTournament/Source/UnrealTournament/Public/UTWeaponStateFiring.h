// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponState.h"

#include "UTWeaponStateFiring.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateFiring : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateFiring(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		PendingFireSequence = -1;
		bDelayShot = false;
	}

	FTimerHandle RefireCheckHandle;

	// accessor for convenience
	inline uint8 GetFireMode()
	{
		return GetOuterAUTWeapon()->GetCurrentFireMode();
	}

	virtual bool IsFiring() const override
	{
		// default is we're firing if we're in this state
		return true;
	}

	/** Returns true if weapon will fire a shot this frame - used for network synchronization */
	virtual bool WillSpawnShot(float DeltaTime);

	/** called to fire the shot and consume ammo */
	virtual void FireShot();

	virtual void BeginState(const UUTWeaponState* PrevState) override;
	virtual void EndState() override;
	virtual void UpdateTiming() override;
	/** called after the refire delay to see what we should do next (generally, fire or go back to active state) */
	virtual void RefireCheckTimer();

	virtual void PutDown() override;
	virtual void Tick(float DeltaTime) override;

	/** If bDelayedShot, fire. */
	virtual void HandleDelayedShot();

	/** Pending fire mode on server when equip completes. */
	int32 PendingFireSequence;

	/** Delay shot one frame to synch with client */
	bool bDelayShot;

	virtual bool BeginFiringSequence(uint8 FireModeNum, bool bClientFired) override;

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

	/** called immediately after the weapon goes inactive (user switches away, drops it, etc)
	 * called on all fire modes of the weapon regardless of whether they were in use
	 */
	virtual void WeaponBecameInactive()
	{}

	/** draw additional HUD displays; called for all the active weapon's firemodes regardless of firing or not 
	 * return whether the weapon's standard crosshair should still be drawn
	 */
	virtual bool DrawHUD(class UUTHUDWidget* WeaponHudWidget)
	{
		return true;
	}

	/** activates or deactivates looping firing effects (sound/anim/etc) that are played for duration of firing state (if weapon has any set) */
	virtual void ToggleLoopingEffects(bool bNowOn);
};