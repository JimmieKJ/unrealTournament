// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UTWeaponStateFiringBurst.h"
#include "UTWeaponStateFiringBurstEnforcer.generated.h"

/**
 * 
 */
UCLASS()
class UNREALTOURNAMENT_API UUTWeaponStateFiringBurstEnforcer : public UUTWeaponStateFiringBurst
{
	GENERATED_UCLASS_BODY()

	virtual void BeginState(const UUTWeaponState* PrevState) override;
	virtual void EndState() override; 
	virtual void IncrementShotTimer() override;
	virtual void UpdateTiming() override;
	virtual void RefireCheckTimer() override;
	virtual void PutDown() override;
	virtual float GetRemainingCooldownTime();
	virtual void Tick(float DeltaTime) override{};
	/** Reset the timer if the time remaining on it is greater than the new FireRate */
	virtual void ResetTiming();

	bool bFirstVolleyComplete;
	bool bSecondVolleyComplete;

private:

	float LastFiredTime;


};
