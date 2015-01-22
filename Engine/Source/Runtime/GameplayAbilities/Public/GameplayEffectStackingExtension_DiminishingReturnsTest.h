// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectStackingExtension.h"
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "AttributeSet.h"
#include "GameplayEffectStackingExtension_DiminishingReturnsTest.generated.h"

UCLASS(BlueprintType)
class GAMEPLAYABILITIES_API UGameplayEffectStackingExtension_DiminishingReturnsTest : public UGameplayEffectStackingExtension
{
	GENERATED_UCLASS_BODY()

public:

	void CalculateStack(TArray<FActiveGameplayEffect*>& CustomGameplayEffects, FActiveGameplayEffectsContainer& Container, FActiveGameplayEffect& CurrentEffect) override;
};