// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "AbilityTask_ApplyRootMotionMoveToForce.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionMoveToForceDelegate);

class AActor;

/**
 *	Applies force to character's movement
 */
UCLASS(MinimalAPI)
class UAbilityTask_ApplyRootMotionMoveToForce : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionMoveToForceDelegate OnTimedOut;

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionMoveToForceDelegate OnTimedOutAndDestinationReached;

	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_ApplyRootMotionMoveToForce* ApplyRootMotionMoveToForce(UObject* WorldContextObject, FName TaskInstanceName, FVector TargetLocation, float Duration, bool bSetNewMovementMode, EMovementMode MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve);

	virtual void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

	void PreReplicatedRemove(const struct FOrionCardArray& InArray);

protected:

	UPROPERTY(Replicated)
	FName ForceName;

	UPROPERTY(Replicated)
	FVector StartLocation;

	UPROPERTY(Replicated)
	FVector TargetLocation;

	UPROPERTY(Replicated)
	float Duration;

	UPROPERTY(Replicated)
	bool bSetNewMovementMode;

	UPROPERTY(Replicated)
	TEnumAsByte<EMovementMode> NewMovementMode;

	UPROPERTY(Replicated)
	bool bRestrictSpeedToExpected;

	UPROPERTY(Replicated)
	UCurveVector* PathOffsetCurve;

	uint16 RootMotionSourceID;
	EMovementMode PreviousMovementMode;

	bool bIsFinished;
	float StartTime;
	float EndTime;

	void SharedInitAndApply();

	UPROPERTY()
	UCharacterMovementComponent* MovementComponent;

};