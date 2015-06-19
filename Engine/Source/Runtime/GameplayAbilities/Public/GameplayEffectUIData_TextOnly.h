// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectUIData_TextOnly.generated.h"

/**
 * UI data that contains only text. This is mostly used as an example of a subclass of UGameplayEffectUIData.
 * If your game needs only text, this is a reasonable class to use. To include more data, make a custom subclass of UGameplayEffectUIData.
 */
UCLASS()
class UGameplayEffectUIData_TextOnly : public UGameplayEffectUIData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Data)
	FText Description;
};