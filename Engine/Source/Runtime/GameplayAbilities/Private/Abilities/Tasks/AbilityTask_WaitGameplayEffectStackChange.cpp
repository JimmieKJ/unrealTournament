// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectStackChange.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"

UAbilityTask_WaitGameplayEffectStackChange::UAbilityTask_WaitGameplayEffectStackChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Registered = false;
}

UAbilityTask_WaitGameplayEffectStackChange* UAbilityTask_WaitGameplayEffectStackChange::WaitForGameplayEffectStackChange(UObject* WorldContextObject, FActiveGameplayEffectHandle InHandle)
{
	auto MyObj = NewAbilityTask<UAbilityTask_WaitGameplayEffectStackChange>(WorldContextObject);
	MyObj->Handle = InHandle;
	return MyObj;
}

void UAbilityTask_WaitGameplayEffectStackChange::Activate()
{
	if (Handle.IsValid() == false)
	{
		InvalidHandle.Broadcast(Handle, 0, 0);
		EndTask();
		return;;
	}

	UAbilitySystemComponent* EffectOwningAbilitySystemComponent = Handle.GetOwningAbilitySystemComponent();

	if (EffectOwningAbilitySystemComponent)
	{
		FOnActiveGameplayEffectStackChange* DelPtr = EffectOwningAbilitySystemComponent->OnGameplayEffectStackChangeDelegate(Handle);
		if (DelPtr)
		{
			OnGameplayEffectStackChangeDelegateHandle = DelPtr->AddUObject(this, &UAbilityTask_WaitGameplayEffectStackChange::OnGameplayEffectStackChange);
			Registered = true;
		}
	}
}

void UAbilityTask_WaitGameplayEffectStackChange::OnDestroy(bool AbilityIsEnding)
{
	UAbilitySystemComponent* EffectOwningAbilitySystemComponent = Handle.GetOwningAbilitySystemComponent();
	if (EffectOwningAbilitySystemComponent && OnGameplayEffectStackChangeDelegateHandle.IsValid())
	{
		FOnActiveGameplayEffectRemoved* DelPtr = EffectOwningAbilitySystemComponent->OnGameplayEffectRemovedDelegate(Handle);
		if (DelPtr)
		{
			DelPtr->Remove(OnGameplayEffectStackChangeDelegateHandle);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}

void UAbilityTask_WaitGameplayEffectStackChange::OnGameplayEffectStackChange(FActiveGameplayEffectHandle InHandle, int32 NewCount, int32 OldCount)
{
	OnChange.Broadcast(InHandle, NewCount, OldCount);
}