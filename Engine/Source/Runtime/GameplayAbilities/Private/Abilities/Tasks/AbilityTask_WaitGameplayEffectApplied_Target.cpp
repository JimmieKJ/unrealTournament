// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectApplied_Target.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayEffectExtension.h"

UAbilityTask_WaitGameplayEffectApplied_Target::UAbilityTask_WaitGameplayEffectApplied_Target(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitGameplayEffectApplied_Target* UAbilityTask_WaitGameplayEffectApplied_Target::WaitGameplayEffectAppliedToTarget(UObject* WorldContextObject, const FGameplayTargetDataFilterHandle InFilter, FGameplayTagRequirements InSourceTagRequirements, FGameplayTagRequirements InTargetTagRequirements, bool InTriggerOnce, AActor* OptionalExternalOwner)
{
	auto MyObj = NewTask<UAbilityTask_WaitGameplayEffectApplied_Target>(WorldContextObject);
	MyObj->Filter = InFilter;
	MyObj->SourceTagRequirements = InSourceTagRequirements;
	MyObj->TargetTagRequirements = InTargetTagRequirements;
	MyObj->TriggerOnce = InTriggerOnce;
	MyObj->SetExternalActor(OptionalExternalOwner);
	return MyObj;
}

void UAbilityTask_WaitGameplayEffectApplied_Target::BroadcastDelegate(AActor* Avatar, FGameplayEffectSpecHandle SpecHandle, FActiveGameplayEffectHandle ActiveHandle)
{
	OnApplied.Broadcast(Avatar, SpecHandle, ActiveHandle);
}

void UAbilityTask_WaitGameplayEffectApplied_Target::RegisterDelegate()
{
	OnApplyGameplayEffectCallbackDelegateHandle = GetASC()->OnGameplayEffectAppliedDelegateToTarget.AddUObject(this, &UAbilityTask_WaitGameplayEffectApplied::OnApplyGameplayEffectCallback);
}

void UAbilityTask_WaitGameplayEffectApplied_Target::RemoveDelegate()
{
	GetASC()->OnGameplayEffectAppliedDelegateToTarget.Remove(OnApplyGameplayEffectCallbackDelegateHandle);
}