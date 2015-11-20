// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilityTask_MoveToLocation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMoveToLocationDelegate);

class AActor;
class UPrimitiveComponent;
class UGameplayTasksComponent;

/**
 *	TODO:
 *		-Implement replicated time so that this can work as a simulated task for Join In Prgorss clients.
 */


/**
 *	Move to a location, ignoring clipping, over a given length of time. Ends when the TargetLocation is reached.
 *	This will RESET your character's current movement mode! If you wish to maintain PHYS_Flying or PHYS_Custom, you must
 *	reset it on completion.!
 */
UCLASS(MinimalAPI)
class UAbilityTask_MoveToLocation : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FMoveToLocationDelegate		OnTargetLocationReached;

	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;

	/** Move to the specified location, using the vector curve (range 0 - 1) if specified, otherwise the float curve (range 0 - 1) or fallback to linear interpolation */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_MoveToLocation* MoveToLocation(UObject* WorldContextObject, FName TaskInstanceName, FVector Location, float Duration, UCurveFloat* OptionalInterpolationCurve, UCurveVector* OptionalVectorInterpolationCurve);

	virtual void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void OnDestroy(bool AbilityIsEnding) override;

protected:

	bool bIsFinished;

	UPROPERTY(Replicated)
	FVector StartLocation;

	//FVector 
	
	UPROPERTY(Replicated)
	FVector TargetLocation;

	UPROPERTY(Replicated)
	float DurationOfMovement;


	float TimeMoveStarted;

	float TimeMoveWillEnd;

	UPROPERTY(Replicated)
	UCurveFloat* LerpCurve;

	UPROPERTY(Replicated)
	UCurveVector* LerpCurveVector;

	
};