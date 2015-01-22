// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitConfirmCancel.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"

UAbilityTask_WaitConfirmCancel::UAbilityTask_WaitConfirmCancel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegisteredCallbacks = false;

}

void UAbilityTask_WaitConfirmCancel::OnConfirmCallback()
{
	OnConfirm.Broadcast();
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->ConsumeAbilityConfirmCancel();
		EndTask();
	}
}

void UAbilityTask_WaitConfirmCancel::OnCancelCallback()
{
	OnCancel.Broadcast();
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->ConsumeAbilityConfirmCancel();
		EndTask();
	}
}

UAbilityTask_WaitConfirmCancel* UAbilityTask_WaitConfirmCancel::WaitConfirmCancel(UObject* WorldContextObject)
{
	return NewTask<UAbilityTask_WaitConfirmCancel>(WorldContextObject);
}

void UAbilityTask_WaitConfirmCancel::Activate()
{
	if (AbilitySystemComponent.IsValid())
	{
		const FGameplayAbilityActorInfo* Info = Ability.Get()->GetCurrentActorInfo();

		// If not locally controlled (E.g., we are the server and this is an ability running on the client), check to see if they already sent us
		// the Confirm/Cancel. If so, immediately end this task.
		if (!Info->IsLocallyControlled())
		{
			if (AbilitySystemComponent->ReplicatedConfirmAbility)
			{
				OnConfirmCallback();
				return;
			}
			else if (AbilitySystemComponent->ReplicatedCancelAbility)
			{
				OnCancelCallback();
				return;
			}
		}
		
		// We have to wait for the callback from the AbilitySystemComponent. Which may be instigated locally or through network replication

		AbilitySystemComponent->ConfirmCallbacks.AddDynamic(this, &UAbilityTask_WaitConfirmCancel::OnConfirmCallback);	// Tell me if the confirm input is pressed
		AbilitySystemComponent->CancelCallbacks.AddDynamic(this, &UAbilityTask_WaitConfirmCancel::OnCancelCallback);	// Tell me if the cancel input is pressed

		RegisteredCallbacks = true;
		
		// Tell me if something else tells me to 'force' my target data (an ability may allow targeting during animation/limited time)
		// Tell me if something else tells me to cancel my targeting
	}
}

void UAbilityTask_WaitConfirmCancel::OnDestroy(bool AbilityEnding)
{
	if (RegisteredCallbacks && AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->ConfirmCallbacks.RemoveDynamic(this, &UAbilityTask_WaitConfirmCancel::OnConfirmCallback);
		AbilitySystemComponent->CancelCallbacks.RemoveDynamic(this, &UAbilityTask_WaitConfirmCancel::OnCancelCallback);
	}

	Super::OnDestroy(AbilityEnding);
}