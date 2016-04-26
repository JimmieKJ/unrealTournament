// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask.h"

UAbilityTask::UAbilityTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaitStateBitMask = (uint8)EAbilityTaskWaitState::WaitingOnGame;
}

FGameplayAbilitySpecHandle UAbilityTask::GetAbilitySpecHandle() const
{
	return Ability ? Ability->GetCurrentAbilitySpecHandle() : FGameplayAbilitySpecHandle();
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
	return Ability ? Ability->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
}

bool UAbilityTask::IsPredictingClient() const
{
	return Ability && Ability->IsPredictingClient();
}

bool UAbilityTask::IsForRemoteClient() const
{
	return Ability && Ability->IsForRemoteClient();
}

bool UAbilityTask::IsLocallyControlled() const
{
	return Ability && Ability->IsLocallyControlled();
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
	if (Ability && IsPendingKill() == false && AbilitySystemComponent)
	{
		WaitStateBitMask |= (uint8)EAbilityTaskWaitState::WaitingOnUser;
		Ability->NotifyAbilityTaskWaitingOnPlayerData(this);
	}
}

void UAbilityTask::ClearWaitingOnRemotePlayerData()
{
	WaitStateBitMask &= ~((uint8)EAbilityTaskWaitState::WaitingOnUser);
}

bool UAbilityTask::IsWaitingOnRemotePlayerdata() const
{
	return (WaitStateBitMask & (uint8)EAbilityTaskWaitState::WaitingOnUser) != 0;
}

void UAbilityTask::SetWaitingOnAvatar()
{
	if (Ability && IsPendingKill() == false && AbilitySystemComponent)
	{
		WaitStateBitMask |= (uint8)EAbilityTaskWaitState::WaitingOnAvatar;
		Ability->NotifyAbilityTaskWaitingOnAvatar(this);
	}
}

void UAbilityTask::ClearWaitingOnAvatar()
{
	WaitStateBitMask &= ~((uint8)EAbilityTaskWaitState::WaitingOnAvatar);
}

bool UAbilityTask::IsWaitingOnAvatar() const
{
	return (WaitStateBitMask & (uint8)EAbilityTaskWaitState::WaitingOnAvatar) != 0;
}