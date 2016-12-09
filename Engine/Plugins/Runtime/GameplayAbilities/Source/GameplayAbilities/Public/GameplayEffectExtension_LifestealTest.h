// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectExtension_LifestealTest.generated.h"

class UGameplayEffect;
struct FGameplayModifierEvaluatedData;

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
