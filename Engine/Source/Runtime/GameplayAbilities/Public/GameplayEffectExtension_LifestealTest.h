// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectExtension.h"
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "AttributeSet.h"
#include "GameplayEffectExtension_LifestealTest.generated.h"

UCLASS(BlueprintType)
class GAMEPLAYABILITIES_API UGameplayEffectExtension_LifestealTest : public UGameplayEffectExtension
{
	GENERATED_UCLASS_BODY()

public:

	void PreGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, FGameplayEffectModCallbackData &Data) const override;
	void PostGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, const FGameplayEffectModCallbackData &Data) const override;

	/** The GameplayEffect to apply when restoring health to the instigator */
	UPROPERTY(EditDefaultsOnly, Category = Lifesteal)
	UGameplayEffect *	HealthRestoreGameplayEffect;
};