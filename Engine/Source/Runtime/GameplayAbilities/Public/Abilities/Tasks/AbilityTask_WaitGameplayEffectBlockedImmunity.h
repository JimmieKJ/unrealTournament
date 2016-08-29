// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilityTask_WaitGameplayEffectBlockedImmunity.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGameplayEffectBlockedDelegate, FGameplayEffectSpecHandle, BlockedSpec, FActiveGameplayEffectHandle, ImmunityGameplayEffectHandle);

UCLASS(MinimalAPI)
class UAbilityTask_WaitGameplayEffectBlockedImmunity : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FGameplayEffectBlockedDelegate Blocked;

	/** Listens for GE immunity. By default this means "this hero blocked a GE due to immunity". Setting OptionalExternalTarget will listen for a GE being blocked on an external target. Note this only works on the server. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitGameplayEffectBlockedImmunity* WaitGameplayEffectBlockedByImmunity(UObject* WorldContextObject, FGameplayTagRequirements SourceTagRequirements, FGameplayTagRequirements TargetTagRequirements, AActor* OptionalExternalTarget=nullptr, bool OnlyTriggerOnce=false);

	virtual void Activate() override;

	virtual void ImmunityCallback(const FGameplayEffectSpec& BlockedSpec, const FActiveGameplayEffect* ImmunityGE);
	
	FGameplayTagRequirements SourceTagRequirements;
	FGameplayTagRequirements TargetTagRequirements;

	bool TriggerOnce;
	bool ListenForPeriodicEffects;

	void SetExternalActor(AActor* InActor);

protected:

	UAbilitySystemComponent* GetASC();
		
	void RegisterDelegate();
	void RemoveDelegate();

	virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;
	bool UseExternalOwner;

	UPROPERTY()
	UAbilitySystemComponent* ExternalOwner;

	FDelegateHandle DelegateHandle;
};