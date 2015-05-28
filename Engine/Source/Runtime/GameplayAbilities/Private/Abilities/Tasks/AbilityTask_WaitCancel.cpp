// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitCancel.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"

UAbilityTask_WaitCancel::UAbilityTask_WaitCancel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegisteredCallbacks = false;

}

void UAbilityTask_WaitCancel::OnCancelCallback()
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::GenericCancel, GetAbilitySpecHandle(), GetActivationPredictionKey());
		OnCancel.Broadcast();
		EndTask();
	}
}

void UAbilityTask_WaitCancel::OnLocalCancelCallback()
{
	if (AbilitySystemComponent.IsValid() && IsPredictingClient())
	{
		AbilitySystemComponent->ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::GenericCancel, GetAbilitySpecHandle(), GetActivationPredictionKey() ,AbilitySystemComponent->ScopedPredictionKey);
	}
	OnCancelCallback();
}

UAbilityTask_WaitCancel* UAbilityTask_WaitCancel::WaitCancel(UObject* WorldContextObject)
{
	return NewTask<UAbilityTask_WaitCancel>(WorldContextObject);
}

void UAbilityTask_WaitCancel::Activate()
{
	if (AbilitySystemComponent.IsValid())
	{
		const FGameplayAbilityActorInfo* Info = Ability.Get()->GetCurrentActorInfo();

		
		if (Info->IsLocallyControlled())
		{
			// We have to wait for the callback from the AbilitySystemComponent.
			AbilitySystemComponent->GenericLocalCancelCallbacks.AddDynamic(this, &UAbilityTask_WaitCancel::OnLocalCancelCallback);	// Tell me if the cancel input is pressed

			RegisteredCallbacks = true;
		}
		else
		{
			if (CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::GenericCancel,  FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UAbilityTask_WaitCancel::OnCancelCallback)) )
			{
				// GenericCancel was already received from the client and we just called OnCancelCallback. The task is done.
				return;
			}
		}
	}
}

void UAbilityTask_WaitCancel::OnDestroy(bool AbilityEnding)
{
	if (RegisteredCallbacks && AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->GenericLocalCancelCallbacks.RemoveDynamic(this, &UAbilityTask_WaitCancel::OnLocalCancelCallback);
	}

	Super::OnDestroy(AbilityEnding);
}
