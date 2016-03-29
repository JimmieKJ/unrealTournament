// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTags.h"
#include "GameplayAbilityTypes.h"
#include "AbilityTask_WaitGameplayEvent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitGameplayEventDelegate, FGameplayEventData, Payload);

UCLASS(MinimalAPI)
class UAbilityTask_WaitGameplayEvent : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEventDelegate	EventReceived;

	/**
	 * 	Wait until the specified gameplay tag event is triggered. By default this will look at the owner of this ability. OptionalExternalTarget can be set to make this look at another actor's tags for changes. 
	 *  It will keep listening as long as OnlyTriggerOnce = false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayEvent* WaitGameplayEvent(UObject* WorldContextObject, FGameplayTag EventTag, AActor* OptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	void SetExternalTarget(AActor* Actor);

	UAbilitySystemComponent* GetTargetASC();

	virtual void Activate() override;

	virtual void GameplayEventCallback(const FGameplayEventData* Payload);

	void OnDestroy(bool AbilityEnding) override;

	FGameplayTag Tag;

	UPROPERTY()
	UAbilitySystemComponent* OptionalExternalTarget;

	bool UseExternalTarget;	
	bool OnlyTriggerOnce;

	FDelegateHandle MyHandle;
};