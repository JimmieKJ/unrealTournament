// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectStackingExtension.generated.h"

struct FGameplayEffectStackingCallbackData
{
	FGameplayEffectStackingCallbackData(const FGameplayEffectSpec& InEffectSpec, const FModifierSpec& InModifierSpec, FGameplayModifierEvaluatedData & InEvaluatedData, UAbilitySystemComponent & InTarget)
		: EffectSpec(InEffectSpec)
		, ModifierSpec(InModifierSpec)
		, EvaluatedData(InEvaluatedData)
		, Target(InTarget)
	{

	}

	const struct FGameplayEffectSpec &		EffectSpec;		// The spec that the mod came from
	const struct FModifierSpec &			ModifierSpec;	// The mod we are going to apply
	struct FGameplayModifierEvaluatedData & EvaluatedData;	// The 'flat'/computed data to be applied to the target

	class UAbilitySystemComponent &Target;		// Target we intend to apply to
};


USTRUCT(BlueprintType)
struct FGameplayEffectStackingHandle
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectStackingHandle()
	: Handle(INDEX_NONE)
	{
		// do nothing
	}

	FGameplayEffectStackingHandle(uint32 InHandle)
		: Handle(InHandle)
	{
		// do nothing
	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	bool operator==(const FGameplayEffectStackingHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FGameplayEffectStackingHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

private:

	UPROPERTY()
	uint32 Handle;
};

UCLASS(BlueprintType)
class GAMEPLAYABILITIES_API UGameplayEffectStackingExtension: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	static FGameplayEffectSpec* StackCustomEffects(TArray<FGameplayEffectSpec*>& CustomGameplayEffects);

	virtual void CalculateStack(TArray<FActiveGameplayEffect*>& CustomGameplayEffects, FActiveGameplayEffectsContainer& Container, FActiveGameplayEffect& CurrentEffect) {}

	FGameplayEffectStackingHandle Handle;
};
