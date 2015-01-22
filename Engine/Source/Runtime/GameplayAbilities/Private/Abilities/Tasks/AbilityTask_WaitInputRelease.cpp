// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayPrediction.h"

UAbilityTask_WaitInputRelease::UAbilityTask_WaitInputRelease(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTime = 0.f;
	RegisteredCallback= false;
}

void UAbilityTask_WaitInputRelease::OnReleaseCallback(int32 InputID)
{
	float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;

	if (!Ability.IsValid())
	{
		return;
	}
	UGameplayAbility* MyAbility = Ability.Get();
	check(MyAbility->IsInstantiated());
	if (MyAbility->GetCurrentAbilitySpec()->InputID != InputID)
	{
		//This key is for some other ability
		return;
	}

	// We are done. Kill us so we don't keep getting broadcast messages
	OnRelease.Broadcast(ElapsedTime);
	EndTask();
}

UAbilityTask_WaitInputRelease* UAbilityTask_WaitInputRelease::WaitInputRelease(class UObject* WorldContextObject)
{
	return NewTask<UAbilityTask_WaitInputRelease>(WorldContextObject);
}

void UAbilityTask_WaitInputRelease::Activate()
{
	if (Ability.IsValid())
	{
		FGameplayAbilitySpec* Spec = Ability->GetCurrentAbilitySpec();
		if (Spec)
		{
			if (!Spec->InputPressed)
			{
				// Input was already released before we got here, we are done.
				OnRelease.Broadcast(0.f);
				EndTask();
			}
			else
			{
				StartTime = GetWorld()->GetTimeSeconds();
				AbilitySystemComponent->AbilityKeyReleaseCallbacks.AddDynamic(this, &UAbilityTask_WaitInputRelease::OnReleaseCallback);
				RegisteredCallback = true;
			}
		}
	}
}

void UAbilityTask_WaitInputRelease::OnDestroy(bool AbilityEnded)
{
	if (RegisteredCallback && Ability.IsValid())
	{
		AbilitySystemComponent->AbilityKeyReleaseCallbacks.RemoveDynamic(this, &UAbilityTask_WaitInputRelease::OnReleaseCallback);
	}

	Super::OnDestroy(AbilityEnded);
}
