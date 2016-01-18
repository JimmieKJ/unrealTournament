// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponState.h"

#include "UTWeaponStateEquipping.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateEquipping : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateEquipping(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		PendingFireSequence = -1;
	}

	// set to amount of equip time that elapsed when exiting early, i.e. to go back down
	float PartialEquipTime;

	/** total time to bring up the weapon */
	float EquipTime;

	/** Pending fire mode on server when equip completes. */
	int32 PendingFireSequence;

	virtual void BeginState(const UUTWeaponState* PrevState) override;

	virtual bool BeginFiringSequence(uint8 FireModeNum, bool bClientFired) override;

	/** called to start the equip timer/anim; this isn't done automatically in BeginState() because we need to pass in any overflow time from the previous weapon's PutDown() */
	virtual void StartEquip(float OverflowTime);
	virtual void EndState() override
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().ClearAllTimersForObject(this);
	}

	virtual void Tick(float DeltaTime)
	{
		// failsafe
		if (!GetOuterAUTWeapon()->GetWorldTimerManager().IsTimerActive(BringUpFinishedHandle))
		{
			UE_LOG(UT, Warning, TEXT("%s in state Equipping with no equip timer!"), *GetOuterAUTWeapon()->GetName());
			BringUpFinished();
		}
	}

	FTimerHandle BringUpFinishedHandle;

	virtual void BringUpFinished();

	virtual void PutDown() override
	{
		PartialEquipTime = FMath::Max(0.001f, GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerElapsed(BringUpFinishedHandle));
		GetOuterAUTWeapon()->GotoState(GetOuterAUTWeapon()->UnequippingState);
	}
};