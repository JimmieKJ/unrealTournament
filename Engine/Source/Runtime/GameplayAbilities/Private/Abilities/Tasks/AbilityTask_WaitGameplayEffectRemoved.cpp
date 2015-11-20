// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectRemoved.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"

UAbilityTask_WaitGameplayEffectRemoved::UAbilityTask_WaitGameplayEffectRemoved(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Registered = false;
}

UAbilityTask_WaitGameplayEffectRemoved* UAbilityTask_WaitGameplayEffectRemoved::WaitForGameplayEffectRemoved(UObject* WorldContextObject, FActiveGameplayEffectHandle InHandle)
{
	auto MyObj = NewAbilityTask<UAbilityTask_WaitGameplayEffectRemoved>(WorldContextObject);
	MyObj->Handle = InHandle;

	return MyObj;
}

void UAbilityTask_WaitGameplayEffectRemoved::Activate()
{
	if (Handle.IsValid() == false)
	{
		InvalidHandle.Broadcast();
		EndTask();
		return;;
	}

	UAbilitySystemComponent* EffectOwningAbilitySystemComponent = Handle.GetOwningAbilitySystemComponent();

	if (EffectOwningAbilitySystemComponent)
	{
		FOnActiveGameplayEffectRemoved* DelPtr = EffectOwningAbilitySystemComponent->OnGameplayEffectRemovedDelegate(Handle);
		if (DelPtr)
		{
			OnGameplayEffectRemovedDelegateHandle = DelPtr->AddUObject(this, &UAbilityTask_WaitGameplayEffectRemoved::OnGameplayEffectRemoved);
			Registered = true;
		}
	}

	if (!Registered)
	{
		// GameplayEffect was already removed, treat this as a warning? Could be cases of immunity or chained gameplay rules that would instant remove something
		OnGameplayEffectRemoved();
	}
}

void UAbilityTask_WaitGameplayEffectRemoved::OnDestroy(bool AbilityIsEnding)
{
	UAbilitySystemComponent* EffectOwningAbilitySystemComponent = Handle.GetOwningAbilitySystemComponent();
	if (EffectOwningAbilitySystemComponent)
	{
		FOnActiveGameplayEffectRemoved* DelPtr = EffectOwningAbilitySystemComponent->OnGameplayEffectRemovedDelegate(Handle);
		if (DelPtr)
		{
			DelPtr->Remove(OnGameplayEffectRemovedDelegateHandle);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}

void UAbilityTask_WaitGameplayEffectRemoved::OnGameplayEffectRemoved()
{
	OnRemoved.Broadcast();
	EndTask();
}