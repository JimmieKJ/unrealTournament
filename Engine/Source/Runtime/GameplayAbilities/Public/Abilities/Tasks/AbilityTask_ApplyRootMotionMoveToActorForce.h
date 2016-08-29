// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "AbilityTask_ApplyRootMotionMoveToActorForce.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FApplyRootMotionMoveToActorForceDelegate, bool, DestinationReached, bool, TimedOut, FVector, FinalTargetLocation);

class AActor;

UENUM()
enum class ERootMotionMoveToActorTargetOffsetType : uint8
{
	// Align target offset vector from target to source, ignoring height difference
	AlignFromTargetToSource = 0,
	// Align from target actor location to target actor forward
	AlignToTargetForward,
	// Align in world space
	AlignToWorldSpace
};

/**
 *	Applies force to character's movement
 */
UCLASS(MinimalAPI)
class UAbilityTask_ApplyRootMotionMoveToActorForce : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionMoveToActorForceDelegate OnFinished;

	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_ApplyRootMotionMoveToActorForce* ApplyRootMotionMoveToActorForce(UObject* WorldContextObject, FName TaskInstanceName, AActor* TargetActor, FVector TargetLocationOffset, ERootMotionMoveToActorTargetOffsetType OffsetAlignment, float Duration, UCurveFloat* TargetLerpSpeedHorizontal, UCurveFloat* TargetLerpSpeedVertical, bool bSetNewMovementMode, EMovementMode MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, bool bDisableDestinationReachedInterrupt);

	virtual void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

protected:

	bool UpdateTargetLocation(float DeltaTime);

	void SetRootMotionTargetLocation(FVector NewTargetLocation);

	FVector CalculateTargetOffset() const;

	UPROPERTY(Replicated)
	FName ForceName;

	UPROPERTY(Replicated)
	FVector StartLocation;

	UPROPERTY(ReplicatedUsing=OnRep_TargetLocation)
	FVector TargetLocation;

	UFUNCTION()
	void OnRep_TargetLocation();

	UPROPERTY(Replicated)
	AActor* TargetActor;

	UPROPERTY(Replicated)
	FVector TargetLocationOffset;

	UPROPERTY(Replicated)
	ERootMotionMoveToActorTargetOffsetType OffsetAlignment;

	UPROPERTY(Replicated)
	float Duration;

	/** By default, this force ends when the destination is reached. Using this parameter you can disable it so it will not 
	 *  "early out" and get interrupted by reaching the destination and instead go to its full duration. */
	UPROPERTY(Replicated)
	bool bDisableDestinationReachedInterrupt;

	UPROPERTY(Replicated)
	bool bSetNewMovementMode;

	UPROPERTY(Replicated)
	TEnumAsByte<EMovementMode> NewMovementMode;

	/** If enabled, we limit velocity to the initial expected velocity to go distance to the target over Duration.
	 *  This prevents cases of getting really high velocity the last few frames of the root motion if you were being blocked by
	 *  collision. Disabled means we do everything we can to velocity during the move to get to the TargetLocation. */
	UPROPERTY(Replicated)
	bool bRestrictSpeedToExpected;

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

	UPROPERTY(Replicated)
	UCurveFloat* TargetLerpSpeedHorizontalCurve;

	UPROPERTY(Replicated)
	UCurveFloat* TargetLerpSpeedVerticalCurve;

	/** What to do with character's Velocity when root motion finishes */
	UPROPERTY(Replicated)
	ERootMotionFinishVelocityMode VelocityOnFinishMode;

	/** If VelocityOnFinish mode is "SetVelocity", character velocity is set to this value when root motion finishes */
	UPROPERTY(Replicated)
	FVector SetVelocityOnFinish;

	uint16 RootMotionSourceID;
	EMovementMode PreviousMovementMode;

	bool bIsFinished;
	float StartTime;
	float EndTime;

	void SharedInitAndApply();

	UPROPERTY()
	UCharacterMovementComponent* MovementComponent;

};