// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_StartAbilityState.h"
#include "AbilitySystemComponent.h"

UAbilityTask_StartAbilityState::UAbilityTask_StartAbilityState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bEndCurrentState = true;
	bWasEnded = false;
	bWasInterrupted = false;
}

UAbilityTask_StartAbilityState* UAbilityTask_StartAbilityState::StartAbilityState(UObject* WorldContextObject, FName StateName, bool bEndCurrentState)
{
	auto Task = NewAbilityTask<UAbilityTask_StartAbilityState>(WorldContextObject, StateName);
	Task->bEndCurrentState = bEndCurrentState;
	return Task;
}

void UAbilityTask_StartAbilityState::Activate()
{
	if (Ability.IsValid())
	{
		if (bEndCurrentState && Ability->OnGameplayAbilityStateEnded.IsBound())
		{
			Ability->OnGameplayAbilityStateEnded.Broadcast(NAME_None);
		}

		EndStateHandle = Ability->OnGameplayAbilityStateEnded.AddUObject(this, &UAbilityTask_StartAbilityState::OnEndState);
		InterruptStateHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UAbilityTask_StartAbilityState::OnInterruptState);
	}
}

void UAbilityTask_StartAbilityState::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);

	if (bWasInterrupted && OnStateInterrupted.IsBound())
	{
		OnStateInterrupted.Broadcast();
	}
	else if ((bWasEnded || AbilityEnded) && OnStateEnded.IsBound())
	{
		OnStateEnded.Broadcast();
	}

	if (Ability.IsValid())
	{
		Ability->OnGameplayAbilityCancelled.Remove(InterruptStateHandle);
		Ability->OnGameplayAbilityStateEnded.Remove(EndStateHandle);
	}
}

void UAbilityTask_StartAbilityState::OnEndState(FName StateNameToEnd)
{
	// All states end if 'NAME_None' is passed to this delegate
	if (StateNameToEnd.IsNone() || StateNameToEnd == InstanceName)
	{
		bWasEnded = true;

		EndTask();
	}
}

void UAbilityTask_StartAbilityState::OnInterruptState()
{
	bWasInterrupted = true;
}

void UAbilityTask_StartAbilityState::ExternalCancel()
{
	bWasInterrupted = true;

	Super::ExternalCancel();
}

FString UAbilityTask_StartAbilityState::GetDebugString() const
{
	return FString::Printf(TEXT("%s (AbilityState)"), *InstanceName.ToString());
}
