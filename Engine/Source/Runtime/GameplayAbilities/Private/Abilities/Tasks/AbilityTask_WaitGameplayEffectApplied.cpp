// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectApplied.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectExtension.h"

UAbilityTask_WaitGameplayEffectApplied::UAbilityTask_WaitGameplayEffectApplied(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAbilityTask_WaitGameplayEffectApplied::Activate()
{
	if (GetASC())
	{
		RegisterDelegate();
	}
}

void UAbilityTask_WaitGameplayEffectApplied::OnApplyGameplayEffectCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	bool PassedComparison = false;

	AActor* AvatarActor = Target ? Target->AvatarActor : nullptr;

	if (!Filter.FilterPassesForActor(AvatarActor))
	{
		return;
	}
	if (!SourceTagRequirements.RequirementsMet(*SpecApplied.CapturedSourceTags.GetAggregatedTags()))
	{
		return;
	}
	if (!TargetTagRequirements.RequirementsMet(*SpecApplied.CapturedTargetTags.GetAggregatedTags()))
	{
		return;
	}
	
	FGameplayEffectSpecHandle	SpecHandle(new FGameplayEffectSpec(SpecApplied));
	BroadcastDelegate(AvatarActor, SpecHandle, ActiveHandle);
	if (TriggerOnce)
	{
		EndTask();
	}
}

void UAbilityTask_WaitGameplayEffectApplied::OnDestroy(bool AbilityEnded)
{
	if (GetASC())
	{
		RemoveDelegate();
	}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitGameplayEffectApplied::SetExternalActor(AActor* InActor)
{
	if (InActor)
	{
		UseExternalOwner = true;
		ExternalOwner  = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InActor);
	}
}

UAbilitySystemComponent* UAbilityTask_WaitGameplayEffectApplied::GetASC()
{
	if (UseExternalOwner)
	{
		return ExternalOwner.Get();
	}
	return AbilitySystemComponent.Get();
}