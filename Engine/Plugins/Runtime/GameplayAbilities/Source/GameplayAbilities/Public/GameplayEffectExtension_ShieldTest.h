// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectExtension_ShieldTest.generated.h"

class UGameplayEffect;
struct FGameplayModifierEvaluatedData;

UCLASS(BlueprintType)
class GAMEPLAYABILITIES_API UGameplayEffectExtension_ShieldTest : public UGameplayEffectExtension
{
	GENERATED_UCLASS_BODY()

public:

	void PreGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, FGameplayEffectModCallbackData &Data) const override;
	void PostGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, const FGameplayEffectModCallbackData &Data) const override;

	UPROPERTY(EditDefaultsOnly, Category = Lifesteal)
	UGameplayEffect * ShieldRemoveGameplayEffect;
};
