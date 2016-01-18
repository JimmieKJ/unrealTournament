// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiringSpinUp.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTWeaponStateFiringSpinUp : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

	/** interval between warmup shots before going to normal firing */
	UPROPERTY(EditDefaultsOnly, Category = Firing)
	TArray<float> WarmupShotIntervals;
	/** cooldown time when fully warmed up */
	UPROPERTY(EditDefaultsOnly, Category = Firing)
	float CoolDownTime;

	/** anim for spin up */
	UPROPERTY(EditDefaultsOnly, Category = Animation)
	UAnimMontage* WarmupAnim;
	/** anim for cool down */
	UPROPERTY(EditDefaultsOnly, Category = Animation)
	UAnimMontage* CooldownAnim;
	/** anim for fully warmed up firing */
	UPROPERTY(EditDefaultsOnly, Category = Animation)
	UAnimMontage* FiringLoopAnim;

protected:
	/** current shot since we started firing */
	int32 CurrentShot;
	/** time until next shot will occur - we have to manually time via Tick() instead of a timer to properly handle overflow with the changing shot intervals
	 * as we can't retrieve the overflow from the timer system and changing the interval will clobber that overflow
	 */
	float ShotTimeRemaining;

public:
	virtual void BeginState(const UUTWeaponState* PrevState) override;
	virtual void EndState() override;
	virtual void UpdateTiming() override;
	/** called after the refire delay to see what we should do next (generally, fire or go back to active state) */
	virtual void RefireCheckTimer();
	virtual void Tick(float DeltaTime) override;

	/** sets NextShotTime for the next shot, taking into account warmup if we're still in that phase */
	virtual void IncrementShotTimer();

	FTimerHandle CoolDownFinishedHandle;

	/** called on a timer to finish cooldown and return to the active state */
	UFUNCTION()
	virtual void CooldownFinished();
};