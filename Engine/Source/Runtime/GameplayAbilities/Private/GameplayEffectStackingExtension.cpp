// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayEffectStackingExtension.h"

UGameplayEffectStackingExtension::UGameplayEffectStackingExtension(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	
}

FGameplayEffectSpec* UGameplayEffectStackingExtension::StackCustomEffects(TArray<FGameplayEffectSpec*>& CustomGameplayEffects)
{
	return NULL;
}