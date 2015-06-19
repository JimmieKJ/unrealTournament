// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilityTask_WaitGameplayEffectApplied.generated.h"



UCLASS(MinimalAPI)
class UAbilityTask_WaitGameplayEffectApplied : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	void OnApplyGameplayEffectCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);

	virtual void Activate() override;

	FGameplayTargetDataFilterHandle Filter;
	FGameplayTagRequirements SourceTagRequirements;
	FGameplayTagRequirements TargetTagRequirements;
	bool TriggerOnce;

	void SetExternalActor(AActor* InActor);

protected:

	UAbilitySystemComponent* GetASC();

	virtual void BroadcastDelegate(AActor* Avatar, FGameplayEffectSpecHandle SpecHandle, FActiveGameplayEffectHandle ActiveHandle) { }
	virtual void RegisterDelegate() { }
	virtual void RemoveDelegate() { }

	virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;
	bool UseExternalOwner;

	TWeakObjectPtr<UAbilitySystemComponent> ExternalOwner;
};