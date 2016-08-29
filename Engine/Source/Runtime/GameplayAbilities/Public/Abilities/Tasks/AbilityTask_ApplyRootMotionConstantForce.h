// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "AbilityTask_ApplyRootMotionConstantForce.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionConstantForceDelegate);

class AActor;

/**
 *	Applies force to character's movement
 */
UCLASS(MinimalAPI)
class UAbilityTask_ApplyRootMotionConstantForce : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionConstantForceDelegate OnFinish;

	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_ApplyRootMotionConstantForce* ApplyRootMotionConstantForce(UObject* WorldContextObject, FName TaskInstanceName, FVector WorldDirection, float Strength, float Duration, bool bIsAdditive, bool bDisableImpartingVelocityOnRemoval, UCurveFloat* StrengthOverTime);

	virtual void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

protected:

	UPROPERTY(Replicated)
	FName ForceName;

	UPROPERTY(Replicated)
	FVector WorldDirection;

	UPROPERTY(Replicated)
	float Strength;

	UPROPERTY(Replicated)
	float Duration;

	UPROPERTY(Replicated)
	bool bIsAdditive;

	UPROPERTY(Replicated)
	bool bDisableImpartingVelocityOnRemoval;

	/** 
	 *  Strength of the force over time
	 *  Curve Y is 0 to 1 which is percent of full Strength parameter to apply
	 *  Curve X is 0 to 1 normalized time if this force has a limited duration (Duration > 0), or
	 *          is in units of seconds if this force has unlimited duration (Duration < 0)
	 */
	UPROPERTY(Replicated)
	UCurveFloat* StrengthOverTime;

	uint16 RootMotionSourceID;
	bool bIsFinished;
	float StartTime;
	float EndTime;

	void SharedInitAndApply();

	UPROPERTY()
	UCharacterMovementComponent* MovementComponent;

};