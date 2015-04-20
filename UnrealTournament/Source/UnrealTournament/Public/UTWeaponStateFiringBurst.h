// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"

#include "UTWeaponStateFiringBurst.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTWeaponStateFiringBurst : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

	/** Number of shots in a burst */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Burst)
	int32 BurstSize;

	/** Interval between shots in a burst */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Burst)
	float BurstInterval;

	/** How much spread increases for each shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Burst)
	float SpreadIncrease;

	/** current shot since we started firing */
	int32 CurrentShot;

	virtual void BeginState(const UUTWeaponState* PrevState) override;
	virtual void UpdateTiming() override;

	/** called after the refire delay to see what we should do next (generally, fire or go back to active state) */
	virtual void RefireCheckTimer();

	virtual void Tick(float DeltaTime) override;

	/** sets NextShotTime for the next shot, taking into whether in burst or not */
	virtual void IncrementShotTimer();

	/** Allow putdown if between bursts */
	virtual void PutDown() override;

protected:

	/** time until next shot will occur - we have to manually time via Tick() instead of a timer to properly handle overflow with the changing shot intervals
	* as we can't retrieve the overflow from the timer system and changing the interval will clobber that overflow
	*/
	float ShotTimeRemaining;
};