// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask.h"

UAbilityTask::UAbilityTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = false;
	bSimulatedTask = false;
	bIsSimulating = false;
}

void UAbilityTask::Activate()
{

}

void UAbilityTask::InitTask(UGameplayAbility* InAbility)
{
	Ability = InAbility;
	AbilitySystemComponent = InAbility->GetCurrentActorInfo()->AbilitySystemComponent;
	InAbility->TaskStarted(this);

	// If this task requires ticking, register it with AbilitySystemComponent
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->TaskStarted(this);
	}
}

void UAbilityTask::InitSimulatedTask(UAbilitySystemComponent* InAbilitySystemComponent)
{
	check(InAbilitySystemComponent);
	AbilitySystemComponent = InAbilitySystemComponent;
	bIsSimulating = true;
}

UWorld* UAbilityTask::GetWorld() const
{
	if (AbilitySystemComponent.IsValid())
	{
		return AbilitySystemComponent.Get()->GetWorld();
	}

	return nullptr;
}

AActor* UAbilityTask::GetOwnerActor() const
{
	if (Ability.IsValid())
	{
		const FGameplayAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
		if (Info)
		{
			return Info->OwnerActor.Get();
		}
	}
	else if (AbilitySystemComponent.IsValid())
	{
		return AbilitySystemComponent->AbilityActorInfo->OwnerActor.Get();
	}

	return nullptr;	
}

AActor* UAbilityTask::GetAvatarActor() const
{
	if (Ability.IsValid())
	{
		const FGameplayAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
		if (Info)
		{
			return Info->AvatarActor.Get();
		}
	}
	else if (AbilitySystemComponent.IsValid())
	{
		return AbilitySystemComponent->AbilityActorInfo->AvatarActor.Get();
	}

	return nullptr;
}

void UAbilityTask::AbilityEnded()
{
	if (!IsPendingKill())
	{
		OnDestroy(true);
	}
}

void UAbilityTask::EndTask()
{
	if (!IsPendingKill())
	{
		OnDestroy(false);
	}
}

void UAbilityTask::ExternalConfirm(bool bEndTask)
{
	if (bEndTask)
	{
		EndTask();
	}
}

void UAbilityTask::ExternalCancel()
{
	EndTask();
}

void UAbilityTask::OnDestroy(bool AbilityIsEnding)
{
	ensure(!IsPendingKill());

	// If this task required ticking, unregister it with AbilitySystemComponent
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->TaskEnded(this);
	}

	// Remove ourselves from the owning Ability's task list, if the ability isn't ending
	if (!AbilityIsEnding && Ability.IsValid())
	{
		Ability->TaskEnded(this);
	}

	MarkPendingKill();
}

FString UAbilityTask::GetDebugString() const
{
	return FString::Printf(TEXT("Generic %s"), *GetName());
}

FGameplayAbilitySpecHandle UAbilityTask::GetAbilitySpecHandle() const
{
	UGameplayAbility* MyAbility = Ability.Get();
	return MyAbility ? MyAbility->GetCurrentAbilitySpecHandle() : FGameplayAbilitySpecHandle();
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
	return AbilitySystemComponent->CallOrAddReplicatedDelegate(Event, GetAbilitySpecHandle(), GetActivationPredictionKey(), Delegate);
}
