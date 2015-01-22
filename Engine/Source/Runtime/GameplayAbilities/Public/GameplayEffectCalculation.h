// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectTypes.h"
#include "GameplayEffectCalculation.generated.h"

struct FGameplayEffectSpec;
class UAbilitySystemComponent;

/** Abstract base for specialized gameplay effect calculations; Capable of specifing attribute captures */
UCLASS(BlueprintType, Blueprintable, Abstract)
class GAMEPLAYABILITIES_API UGameplayEffectCalculation : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Simple accessor to capture definitions for attributes */
	virtual const TArray<FGameplayEffectAttributeCaptureDefinition>& GetAttributeCaptureDefinitions() const;

protected:

	/** Attributes to capture that are relevant to the calculation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Attributes)
	TArray<FGameplayEffectAttributeCaptureDefinition> RelevantAttributesToCapture;
};
