// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask.h"

UAbilityTask::UAbilityTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaitState = EAbilityTaskWaitState::WaitingOnGame;
}

FGameplayAbilitySpecHandle UAbilityTask::GetAbilitySpecHandle() const
{
	UGameplayAbility* MyAbility = Ability.Get();
	return MyAbility ? MyAbility->GetCurrentAbilitySpecHandle() : FGameplayAbilitySpecHandle();
}

void UAbilityTask::SetAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent)
{
	AbilitySystemComponent = InAbilitySystemComponent;
}

void UAbilityTask::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	UGameplayTask::InitSimulatedTask(InGameplayTasksComponent);

	SetAbilitySystemComponent(Cast<UAbilitySystemComponent>(TasksComponent.Get()));
}

FPredictionKey UAbilityTask::GetActivationPredictionKey() const
{
	UGameplayAbility* MyAbility = Ability.Get();
	return MyAbility ? MyAbility->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
}

bool UAbilityTask::IsPredictingClient() const
{
	UGameplayAbility* MyAbility = Ability.Get();
	return MyAbility && MyAbility->IsPredictingClient();
}

bool UAbilityTask::IsForRemoteClient() const
{
	UGameplayAbility* MyAbility = Ability.Get();
	return MyAbility && MyAbility->IsForRemoteClient();
}

bool UAbilityTask::IsLocallyControlled() const
{
	UGameplayAbility* MyAbility = Ability.Get();
	return MyAbility && MyAbility->IsLocallyControlled();
}

bool UAbilityTask::CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::Type Event, FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!AbilitySystemComponent->CallOrAddReplicatedDelegate(Event, GetAbilitySpecHandle(), GetActivationPredictionKey(), Delegate))
	{
		SetWaitingOnRemotePlayerData();
		return false;
	}
	return true;
}

void UAbilityTask::SetWaitingOnRemotePlayerData()
{
	UGameplayAbility* MyAbility = Ability.Get();
	if (MyAbility && IsPendingKill() == false && AbilitySystemComponent.IsValid())
	{
		WaitState = EAbilityTaskWaitState::WaitingOnUser;
		MyAbility->NotifyAbilityTaskWaitingOnPlayerData(this);
	}
}

void UAbilityTask::ClearWaitingOnRemotePlayerData()
{
	WaitState = EAbilityTaskWaitState::WaitingOnGame;
}

bool UAbilityTask::IsWaitingOnRemotePlayerdata() const
{
	return (WaitState == EAbilityTaskWaitState::WaitingOnUser);
}