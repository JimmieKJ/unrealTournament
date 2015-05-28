// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

// ----------------------------------------------------------------

UAbilityTask_WaitGameplayTagAdded::UAbilityTask_WaitGameplayTagAdded(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

UAbilityTask_WaitGameplayTagAdded* UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(UObject* WorldContextObject, FGameplayTag Tag, AActor* OptionalExternalTarget, bool OnlyTriggerOnce)
{
	auto MyObj = NewTask<UAbilityTask_WaitGameplayTagAdded>(WorldContextObject);
	MyObj->Tag = Tag;
	MyObj->SetExternalTarget(OptionalExternalTarget);
	MyObj->OnlyTriggerOnce = OnlyTriggerOnce;

	return MyObj;
}

void UAbilityTask_WaitGameplayTagAdded::Activate()
{
	UAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC && ASC->HasMatchingGameplayTag(Tag))
	{			
		Added.Broadcast();
		if(OnlyTriggerOnce)
		{
			EndTask();
			return;
		}
	}

	Super::Activate();
}

void UAbilityTask_WaitGameplayTagAdded::GameplayTagCallback(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount==1)
	{
		Added.Broadcast();
		if(OnlyTriggerOnce)
		{
			EndTask();
		}
	}
}

// ----------------------------------------------------------------

UAbilityTask_WaitGameplayTagRemoved::UAbilityTask_WaitGameplayTagRemoved(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

UAbilityTask_WaitGameplayTagRemoved* UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(UObject* WorldContextObject, FGameplayTag Tag, AActor* OptionalExternalTarget, bool OnlyTriggerOnce)
{
	auto MyObj = NewTask<UAbilityTask_WaitGameplayTagRemoved>(WorldContextObject);
	MyObj->Tag = Tag;
	MyObj->SetExternalTarget(OptionalExternalTarget);
	MyObj->OnlyTriggerOnce = OnlyTriggerOnce;

	return MyObj;
}

void UAbilityTask_WaitGameplayTagRemoved::Activate()
{
	UAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC && ASC->HasMatchingGameplayTag(Tag) == false)
	{			
		Removed.Broadcast();
		if(OnlyTriggerOnce)
		{
			EndTask();
			return;
		}
	}

	Super::Activate();
}

void UAbilityTask_WaitGameplayTagRemoved::GameplayTagCallback(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount==0)
	{
		Removed.Broadcast();
		if(OnlyTriggerOnce)
		{
			EndTask();
		}
	}
}

// ----------------------------------------------------------------