// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitGameplayTagBase.generated.h"

class UAbilitySystemComponent;

UCLASS(MinimalAPI)
class UAbilityTask_WaitGameplayTag : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	virtual void Activate() override;

	UFUNCTION()
	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount);
	
	void SetExternalTarget(AActor* Actor);

	FGameplayTag	Tag;

protected:

	UAbilitySystemComponent* GetTargetASC();

	virtual void OnDestroy(bool AbilityIsEnding) override;
	
	bool RegisteredCallback;
	
	UPROPERTY()
	UAbilitySystemComponent* OptionalExternalTarget;

	bool UseExternalTarget;	
	bool OnlyTriggerOnce;

	FDelegateHandle DelegateHandle;
};
