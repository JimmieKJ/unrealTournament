// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitAbilityCommit.h"

#include "AbilitySystemComponent.h"



UAbilityTask_WaitAbilityCommit::UAbilityTask_WaitAbilityCommit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitAbilityCommit* UAbilityTask_WaitAbilityCommit::WaitForAbilityCommit(UGameplayAbility* OwningAbility, FGameplayTag InWithTag, FGameplayTag InWithoutTag, bool InTriggerOnce)
{
	auto MyObj = NewAbilityTask<UAbilityTask_WaitAbilityCommit>(OwningAbility);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->TriggerOnce = InTriggerOnce;

	return MyObj;
}

void UAbilityTask_WaitAbilityCommit::Activate()
{
	if (AbilitySystemComponent)	
	{		
		OnAbilityCommitDelegateHandle = AbilitySystemComponent->AbilityCommitedCallbacks.AddUObject(this, &UAbilityTask_WaitAbilityCommit::OnAbilityCommit);
	}
}

void UAbilityTask_WaitAbilityCommit::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityCommitedCallbacks.Remove(OnAbilityCommitDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitAbilityCommit::OnAbilityCommit(UGameplayAbility *ActivatedAbility)
{
	if ( (WithTag.IsValid() && !ActivatedAbility->AbilityTags.HasTag(WithTag)) ||
		 (WithoutTag.IsValid() && ActivatedAbility->AbilityTags.HasTag(WithoutTag)))
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
