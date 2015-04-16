// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponState.generated.h"

UCLASS(DefaultToInstanced, EditInlineNew, CustomConstructor, Within=UTWeapon)
class UNREALTOURNAMENT_API UUTWeaponState : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTWeaponState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	// accessor for convenience
	inline AUTCharacter* GetUTOwner()
	{
		return GetOuterAUTWeapon()->GetUTOwner();
	}

	virtual UWorld* GetWorld() const override
	{
		return GetOuterAUTWeapon()->GetWorld();
	}

	virtual void BeginState(const UUTWeaponState* PrevState)
	{}
	virtual void EndState()
	{}
	virtual bool BeginFiringSequence(uint8 FireModeNum, bool bClientFired)
	{
		return false;
	}
	virtual void EndFiringSequence(uint8 FireModeNum)
	{}

	/** Returns true if weapon will fire a shot this frame - used for network synchronization */
	virtual bool WillSpawnShot(float DeltaTime)
	{
		return false;
	}

	/** handle request to bring up
	 * only inactive and equipping states should ever receive this function
	 */
	virtual void BringUp(float OverflowTime)
	{
		// note that this can happen legitimately if the player sets a pending weapon while firing then reverts back to this weapon before that firing sequence completes
		//UE_LOG(UT, Warning, TEXT("BringUp() called in state %s"), *GetName());
	}
	/** handle request to put down weapon
	 * note that the state can't outright reject putdown (weapon has that authority)
	 * but it can delay it until it returns to active by overriding this
	 */
	FTimerHandle PutDownHandle;
	virtual void PutDown()
	{
		GetOuterAUTWeapon()->UnEquip();
	}
	virtual bool IsFiring() const
	{
		return false;
	}
	/** update any timers and such due to a weapon timing change; for example, a powerup that changes firing speed */
	virtual void UpdateTiming()
	{}
	virtual void Tick(float DeltaTime)
	{}
};