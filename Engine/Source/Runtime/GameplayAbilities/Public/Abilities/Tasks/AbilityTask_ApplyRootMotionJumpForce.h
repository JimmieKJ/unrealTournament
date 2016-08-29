// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "AbilityTask_ApplyRootMotionJumpForce.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionJumpForceDelegate);

class AActor;

/**
 *	Applies force to character's movement
 */
UCLASS(MinimalAPI)
class UAbilityTask_ApplyRootMotionJumpForce : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionJumpForceDelegate OnFinish;

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionJumpForceDelegate OnLanded;

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks")
	void Finish();

	UFUNCTION()
	void OnLandedCallback(const FHitResult& Hit);

	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_ApplyRootMotionJumpForce* ApplyRootMotionJumpForce(UObject* WorldContextObject, FName TaskInstanceName, FRotator Rotation, float Distance, float Height, float Duration, float MinimumLandedTriggerTime, bool bFinishOnLanded, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve);

	virtual void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

protected:

	UPROPERTY(Replicated)
	FName ForceName;

	UPROPERTY(Replicated)
	FRotator Rotation;

	UPROPERTY(Replicated)
	float Distance;

	UPROPERTY(Replicated)
	float Height;

	UPROPERTY(Replicated)
	float Duration;

	UPROPERTY(Replicated)
	float MinimumLandedTriggerTime;

	UPROPERTY(Replicated)
	bool bFinishOnLanded;

	UPROPERTY(Replicated)
	UCurveVector* PathOffsetCurve;

	/** 
	 *  Maps real time to movement fraction curve to affect the speed of the
	 *  movement through the path
	 *  Curve X is 0 to 1 normalized real time (a fraction of the duration)
	 *  Curve Y is 0 to 1 is what percent of the move should be at a given X
	 *  Default if unset is a 1:1 correspondence
	 */
	UPROPERTY(Replicated)
	UCurveFloat* TimeMappingCurve;

	uint16 RootMotionSourceID;
	bool bIsFinished;
	float StartTime;
	float EndTime;

	/**
	 * Work-around for OnLanded being called during bClientUpdating in movement replay code
	 * Don't want to trigger our Landed logic during a replay, so we wait until next frame
	 * If we don't, we end up removing root motion from a replay root motion set instead
	 * of the real one
	 */
	void TriggerLanded();
	bool bHasLanded;

	void SharedInitAndApply();

	UPROPERTY()
	UCharacterMovementComponent* MovementComponent;

};