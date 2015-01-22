// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayEffectExtension_ShieldTest.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemTestAttributeSet.h"

UGameplayEffectExtension_ShieldTest::UGameplayEffectExtension_ShieldTest(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}


void UGameplayEffectExtension_ShieldTest::PreGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, FGameplayEffectModCallbackData &Data) const
{
	
}

void UGameplayEffectExtension_ShieldTest::PostGameplayEffectExecute(const FGameplayModifierEvaluatedData &SelfData, const FGameplayEffectModCallbackData &Data) const
{

}