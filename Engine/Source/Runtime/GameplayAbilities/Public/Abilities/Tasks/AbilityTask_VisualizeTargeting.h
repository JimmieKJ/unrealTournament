// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TimerManager.h"
#include "AbilityTask.h"
#include "AbilityTask_VisualizeTargeting.generated.h"

class AGameplayAbilityTargetActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVisualizeTargetingDelegate);

UCLASS(MinimalAPI)
class UAbilityTask_VisualizeTargeting: public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FVisualizeTargetingDelegate TimeElapsed;

	void OnTimeElapsed();

	/** Spawns target actor and uses it for visualization. */
	UFUNCTION(BlueprintCallable, meta=(HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true", HideSpawnParms="Instigator"), Category="Ability|Tasks")
	static UAbilityTask_VisualizeTargeting* VisualizeTargeting(UObject* WorldContextObject, TSubclassOf<AGameplayAbilityTargetActor> Class, FName TaskInstanceName, float Duration = -1.0f);

	/** Visualize target using a specified target actor. */
	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "Ability|Tasks")
	static UAbilityTask_VisualizeTargeting* VisualizeTargetingUsingActor(UObject* WorldContextObject, AGameplayAbilityTargetActor* TargetActor, FName TaskInstanceName, float Duration = -1.0f);

	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	bool BeginSpawningActor(UObject* WorldContextObject, TSubclassOf<AGameplayAbilityTargetActor> Class, AGameplayAbilityTargetActor*& SpawnedActor);

	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	void FinishSpawningActor(UObject* WorldContextObject, AGameplayAbilityTargetActor* SpawnedActor);

protected:

	void SetDuration(const float Duration);

	bool ShouldSpawnTargetActor() const;
	void InitializeTargetActor(AGameplayAbilityTargetActor* SpawnedActor) const;
	void FinalizeTargetActor(AGameplayAbilityTargetActor* SpawnedActor) const;

	virtual void OnDestroy(bool AbilityEnded) override;

protected:

	TSubclassOf<AGameplayAbilityTargetActor> TargetClass;

	/** The TargetActor that we spawned */
	TWeakObjectPtr<AGameplayAbilityTargetActor>	TargetActor;

	/** Handle for efficient management of OnTimeElapsed timer */
	FTimerHandle TimerHandle_OnTimeElapsed;
};


/**
*	Requirements for using Begin/Finish SpawningActor functionality:
*		-Have a parameters named 'Class' in your Proxy factor function (E.g., WaitTargetdata)
*		-Have a function named BeginSpawningActor w/ the same Class parameter
*			-This function should spawn the actor with SpawnActorDeferred and return true/false if it spawned something.
*		-Have a function named FinishSpawningActor w/ an AActor* of the class you spawned
*			-This function *must* call ExecuteConstruction + PostActorConstruction
*/