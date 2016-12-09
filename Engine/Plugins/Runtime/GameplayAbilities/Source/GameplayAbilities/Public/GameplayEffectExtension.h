// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AttributeSet.h"
#include "GameplayEffectExtension.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;
struct FGameplayModifierEvaluatedData;

struct FGameplayEffectModCallbackData
{
	FGameplayEffectModCallbackData(const FGameplayEffectSpec& InEffectSpec, FGameplayModifierEvaluatedData& InEvaluatedData, UAbilitySystemComponent& InTarget)
		: EffectSpec(InEffectSpec)
		, EvaluatedData(InEvaluatedData)
		, Target(InTarget)
	{

	}

	const struct FGameplayEffectSpec&		EffectSpec;		// The spec that the mod came from
	struct FGameplayModifierEvaluatedData&	EvaluatedData;	// The 'flat'/computed data to be applied to the target

	class UAbilitySystemComponent &Target;		// Target we intend to apply to
};

UCLASS(BlueprintType)
class GAMEPLAYABILITIES_API UGameplayEffectExtension: public UObject
{
	GENERATED_UCLASS_BODY()

	/** Attributes on the source instigator that are relevant to calculating modifications using this extension */
	UPROPERTY(EditDefaultsOnly, Category=Calculation)
	TArray<FGameplayAttribute> RelevantSourceAttributes;

	/** Attributes on the target that are relevant to calculating modifications using this extension */
	UPROPERTY(EditDefaultsOnly, Category=Calculation)
	TArray<FGameplayAttribute> RelevantTargetAttributes;

public:

	virtual void PreGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, FGameplayEffectModCallbackData &Data) const
	{

	}

	virtual void PostGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, const FGameplayEffectModCallbackData &Data) const
	{

	}
};
