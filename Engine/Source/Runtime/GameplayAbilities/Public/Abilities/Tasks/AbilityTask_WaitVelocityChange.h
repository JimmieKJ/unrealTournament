// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AbilityTask.h"
#include "AbilityTask_WaitVelocityChange.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitVelocityChangeDelegate);

UCLASS(MinimalAPI)
class UAbilityTask_WaitVelocityChange: public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	/** Delegate called when velocity requirements are met */
	UPROPERTY(BlueprintAssignable)
	FWaitVelocityChangeDelegate OnVelocityChage;

	virtual void TickTask(float DeltaTime) override;

	/** Wait for the actor's movement component velocity to be of minimum magnitude when projected along given direction */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName="WaitVelocityChange",HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitVelocityChange* CreateWaitVelocityChange(UObject* WorldContextObject, FVector Direction, float MinimumMagnitude);
		
	virtual void Activate() override;

protected:

	UPROPERTY()
	UMovementComponent*	CachedMovementComponent;

	float	MinimumMagnitude;
	FVector Direction;
};