// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameplayPrediction.h"

UAbilityTask_WaitInputRelease::UAbilityTask_WaitInputRelease(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StartTime = 0.f;
	bTestInitialState = false;
}

void UAbilityTask_WaitInputRelease::OnReleaseCallback()
{
	float ElapsedTime = GetWorld()->GetTimeSeconds() - StartTime;

	if (!Ability || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()).Remove(DelegateHandle);

	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent, IsPredictingClient());

	if (IsPredictingClient())
	{
		// Tell the server about this
		AbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey(), AbilitySystemComponent->ScopedPredictionKey);
	}
	else
	{
		AbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey());
	}

	// We are done. Kill us so we don't keep getting broadcast messages
	OnRelease.Broadcast(ElapsedTime);
	EndTask();
}

UAbilityTask_WaitInputRelease* UAbilityTask_WaitInputRelease::WaitInputRelease(class UObject* WorldContextObject, bool bTestAlreadyReleased)
{
	UAbilityTask_WaitInputRelease* Task = NewAbilityTask<UAbilityTask_WaitInputRelease>(WorldContextObject);
	Task->bTestInitialState = bTestAlreadyReleased;
	return Task;
}

void UAbilityTask_WaitInputRelease::Activate()
{
	StartTime = GetWorld()->GetTimeSeconds();
	if (Ability)
	{
		if (bTestInitialState && IsLocallyControlled())
		{
			FGameplayAbilitySpec *Spec = Ability->GetCurrentAbilitySpec();
			if (Spec && !Spec->InputPressed)
			{
				OnReleaseCallback();
				return;
			}
		}

		DelegateHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()).AddUObject(this, &UAbilityTask_WaitInputRelease::OnReleaseCallback);
		if (IsForRemoteClient())
		{
			if (!AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::InputReleased, GetAbilitySpecHandle(), GetActivationPredictionKey()))
			{
				SetWaitingOnRemotePlayerData();
			}
		}
	}
}

void UAbilityTask_WaitInputRelease::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}
