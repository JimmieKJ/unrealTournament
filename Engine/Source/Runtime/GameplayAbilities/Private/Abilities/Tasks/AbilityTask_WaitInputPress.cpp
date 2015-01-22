// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "GameplayPrediction.h"

UAbilityTask_WaitInputPress::UAbilityTask_WaitInputPress(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTime = 0.f;
	RegisteredCallback= false;
}

void UAbilityTask_WaitInputPress::OnPressCallback(int32 InputID)
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
	OnPress.Broadcast(ElapsedTime);
	EndTask();
}

UAbilityTask_WaitInputPress* UAbilityTask_WaitInputPress::WaitInputPress(class UObject* WorldContextObject)
{
	return NewTask<UAbilityTask_WaitInputPress>(WorldContextObject);
}

void UAbilityTask_WaitInputPress::Activate()
{
	if (Ability.IsValid())
	{
		FGameplayAbilitySpec* Spec = Ability->GetCurrentAbilitySpec();
		if (Spec)
		{
			if (Spec->InputPressed)
			{
				// Input was already pressed before we got here, we are done.
				OnPress.Broadcast(0.f);
				EndTask();
			}
			else
			{
				StartTime = GetWorld()->GetTimeSeconds();
				AbilitySystemComponent->AbilityKeyPressCallbacks.AddDynamic(this, &UAbilityTask_WaitInputPress::OnPressCallback);
				RegisteredCallback = true;
			}
		}
	}
}

void UAbilityTask_WaitInputPress::OnDestroy(bool AbilityEnded)
{
	if (RegisteredCallback && Ability.IsValid())
	{
		AbilitySystemComponent->AbilityKeyPressCallbacks.RemoveDynamic(this, &UAbilityTask_WaitInputPress::OnPressCallback);
	}

	Super::OnDestroy(AbilityEnded);
}
