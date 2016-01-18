// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateUnequipping.generated.h"

/** base class of states that fire the weapon and live in the weapon's FiringState array */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateUnequipping : public UUTWeaponState
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateUnequipping(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	// set to amount of equip time that elapsed when exiting early, i.e. to go back up
	float PartialEquipTime;

	/** unequip time so far; when this reaches UnequipTime, the weapon change happens
	 * manual timer so that we can track overflow and apply it to the weapon being switched to
	 */
	float UnequipTimeElapsed;
	/** total time to bring down the weapon */
	float UnequipTime;

	virtual void BeginState(const UUTWeaponState* PrevState) override;
	virtual void EndState() override
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().ClearTimer(PutDownFinishedHandle);
	}
	
	FTimerHandle PutDownFinishedHandle;

	void PutDownFinished()
	{
		float OverflowTime = UnequipTimeElapsed - UnequipTime;
		GetOuterAUTWeapon()->DetachFromOwner();
		GetOuterAUTWeapon()->GotoState(GetOuterAUTWeapon()->InactiveState);
		GetOuterAUTWeapon()->GetUTOwner()->WeaponChanged(OverflowTime);
	}

	virtual void BringUp(float OverflowTime) override
	{
		PartialEquipTime = FMath::Max<float>(0.001f, GetOuterAUTWeapon()->GetWorldTimerManager().GetTimerElapsed(PutDownFinishedHandle));
		GetOuterAUTWeapon()->GotoEquippingState(OverflowTime);
	}

	virtual void Tick(float DeltaTime) override
	{
		UnequipTimeElapsed += DeltaTime;
		if (UnequipTimeElapsed > UnequipTime)
		{
			PutDownFinished();
		}
	}
};