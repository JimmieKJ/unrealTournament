// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitAbilityActivate.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"

UAbilityTask_WaitAbilityActivate::UAbilityTask_WaitAbilityActivate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IncludeTriggeredAbilities = false;
}

UAbilityTask_WaitAbilityActivate* UAbilityTask_WaitAbilityActivate::WaitForAbilityActivate(UObject* WorldContextObject, FGameplayTag InWithTag, FGameplayTag InWithoutTag, bool InIncludeTriggeredAbilities, bool InTriggerOnce)
{
	auto MyObj = NewTask<UAbilityTask_WaitAbilityActivate>(WorldContextObject);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->IncludeTriggeredAbilities = InIncludeTriggeredAbilities;
	MyObj->TriggerOnce = InTriggerOnce;
	return MyObj;
}

void UAbilityTask_WaitAbilityActivate::Activate()
{
	if (AbilitySystemComponent.IsValid())
	{
		OnAbilityActivateDelegateHandle = AbilitySystemComponent->AbilityActivatedCallbacks.AddUObject(this, &UAbilityTask_WaitAbilityActivate::OnAbilityActivate);
	}
}

void UAbilityTask_WaitAbilityActivate::OnAbilityActivate(UGameplayAbility* ActivatedAbility)
{
	if (!IncludeTriggeredAbilities && ActivatedAbility->IsTriggered())
	{
		return;
	}

	if ((WithTag.IsValid() && !ActivatedAbility->AbilityTags.HasTag(WithTag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit)) ||
		(WithoutTag.IsValid() && ActivatedAbility->AbilityTags.HasTag(WithoutTag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit)))
	{
		// Failed tag check
		return;
	}

	OnActivate.Broadcast(ActivatedAbility);

	if (TriggerOnce)
	{
		EndTask();
	}
}

void UAbilityTask_WaitAbilityActivate::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->AbilityActivatedCallbacks.Remove(OnAbilityActivateDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}