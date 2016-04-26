// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "GameplayEffectCustomApplicationRequirement.generated.h"

// Forward declarations
struct FGameplayEffectSpec;
class UAbilitySystemComponent;
class UGameplayEffect;

/** Class used to perform custom gameplay effect modifier calculations, either via blueprint or native code */ 
UCLASS(BlueprintType, Blueprintable, Abstract)
class GAMEPLAYABILITIES_API UGameplayEffectCustomApplicationRequirement : public UObject
{

public:
	GENERATED_UCLASS_BODY()
	
	/** Return whether the gameplay effect should be applied or not */
	UFUNCTION(BlueprintNativeEvent, Category="Calculation")
	bool CanApplyGameplayEffect(const UGameplayEffect* GameplayEffect, const FGameplayEffectSpec& Spec, UAbilitySystemComponent* ASC) const;
};