// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AbilityTask.h"
#include "GameplayAbilityTargetTypes.h"
#include "AbilityTask_WaitTargetData.generated.h"

class AGameplayAbilityTargetActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitTargetDataDelegate, FGameplayAbilityTargetDataHandle, Data);

/** Wait for targeting actor (spawned from parameter) to provide data. Can be set not to end upon outputting data. Can be ended by task name. */
UCLASS(notplaceable)
class GAMEPLAYABILITIES_API UAbilityTask_WaitTargetData: public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate	ValidData;

	UPROPERTY(BlueprintAssignable)
	FWaitTargetDataDelegate	Cancelled;

	UFUNCTION()
	void OnTargetDataReplicatedCallback(FGameplayAbilityTargetDataHandle Data, FGameplayTag ActivationTag);

	UFUNCTION()
	void OnTargetDataReplicatedCancelledCallback();

	UFUNCTION()
	void OnTargetDataReadyCallback(FGameplayAbilityTargetDataHandle Data);

	UFUNCTION()
	void OnTargetDataCancelledCallback(FGameplayAbilityTargetDataHandle Data);

	/** Spawns target actor and waits for it to return valid data or to be canceled. */
	UFUNCTION(BlueprintCallable, meta=(HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true", HideSpawnParms="Instigator"), Category="Ability|Tasks")
	static UAbilityTask_WaitTargetData* WaitTargetData(UObject* WorldContextObject, FName TaskInstanceName, TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType, TSubclassOf<AGameplayAbilityTargetActor> Class);

	/** Uses specified target actor and waits for it to return valid data or to be canceled. */
	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true", HideSpawnParms = "Instigator"), Category = "Ability|Tasks")
	static UAbilityTask_WaitTargetData* WaitTargetDataUsingActor(UObject* WorldContextObject, FName TaskInstanceName, TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType, AGameplayAbilityTargetActor* TargetActor);

	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	bool BeginSpawningActor(UObject* WorldContextObject, TSubclassOf<AGameplayAbilityTargetActor> Class, AGameplayAbilityTargetActor*& SpawnedActor);

	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	void FinishSpawningActor(UObject* WorldContextObject, AGameplayAbilityTargetActor* SpawnedActor);

	/** Called when the ability is asked to confirm from an outside node. What this means depends on the individual task. By default, this does nothing other than ending if bEndTask is true. */
	virtual void ExternalConfirm(bool bEndTask) override;

	/** Called when the ability is asked to cancel from an outside node. What this means depends on the individual task. By default, this does nothing other than ending the task. */
	virtual void ExternalCancel() override;

protected:

	bool ShouldSpawnTargetActor() const;
	void InitializeTargetActor(AGameplayAbilityTargetActor* SpawnedActor) const;
	void FinalizeTargetActor(AGameplayAbilityTargetActor* SpawnedActor) const;

	void RegisterTargetDataCallbacks();

	virtual void OnDestroy(bool AbilityEnded) override;

	bool ShouldReplicateDataToServer() const;

protected:

	TSubclassOf<AGameplayAbilityTargetActor> TargetClass;

	/** The TargetActor that we spawned */
	TWeakObjectPtr<AGameplayAbilityTargetActor>	TargetActor;

	TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType;

	FDelegateHandle OnTargetDataReplicatedCallbackDelegateHandle;
};


/**
*	Requirements for using Begin/Finish SpawningActor functionality:
*		-Have a parameters named 'Class' in your Proxy factor function (E.g., WaitTargetdata)
*		-Have a function named BeginSpawningActor w/ the same Class parameter
*			-This function should spawn the actor with SpawnActorDeferred and return true/false if it spawned something.
*		-Have a function named FinishSpawningActor w/ an AActor* of the class you spawned
*			-This function *must* call ExecuteConstruction + PostActorConstruction
*/
