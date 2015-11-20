// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitAbilityCommit.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"


UAbilityTask_WaitAbilityCommit::UAbilityTask_WaitAbilityCommit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitAbilityCommit* UAbilityTask_WaitAbilityCommit::WaitForAbilityCommit(UObject* WorldContextObject, FGameplayTag InWithTag, FGameplayTag InWithoutTag, bool InTriggerOnce)
{
	auto MyObj = NewAbilityTask<UAbilityTask_WaitAbilityCommit>(WorldContextObject);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->TriggerOnce = InTriggerOnce;

	return MyObj;
}

void UAbilityTask_WaitAbilityCommit::Activate()
{
	if (AbilitySystemComponent.IsValid())	
	{		
		OnAbilityCommitDelegateHandle = AbilitySystemComponent->AbilityCommitedCallbacks.AddUObject(this, &UAbilityTask_WaitAbilityCommit::OnAbilityCommit);
	}
}

void UAbilityTask_WaitAbilityCommit::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->AbilityCommitedCallbacks.Remove(OnAbilityCommitDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitAbilityCommit::OnAbilityCommit(UGameplayAbility *ActivatedAbility)
{
	if ( (WithTag.IsValid() && !ActivatedAbility->AbilityTags.HasTag(WithTag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit)) ||
		 (WithoutTag.IsValid() && ActivatedAbility->AbilityTags.HasTag(WithoutTag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit)))
	{
		// Failed tag check
		return;
	}

	OnCommit.Broadcast(ActivatedAbility);

	if (TriggerOnce)
	{
		EndTask();
	}
}