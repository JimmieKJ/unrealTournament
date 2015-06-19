// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"

UAbilityTask_PlayMontageAndWait::UAbilityTask_PlayMontageAndWait(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Rate = 1.f;
}

void UAbilityTask_PlayMontageAndWait::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Ability.IsValid() && Ability->GetCurrentMontage() == MontageToPlay)
	{
		if (Montage == MontageToPlay)
		{
			AbilitySystemComponent->ClearAnimatingAbility(Ability.Get());
		}
	}

	if (bInterrupted)
	{
		OnInterrupted.Broadcast();
	}
	else
	{
		OnComplete.Broadcast();
	}

	EndTask();
}

void UAbilityTask_PlayMontageAndWait::OnMontageInterrupted()
{
	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop the montage
	if (AbilitySystemComponent.IsValid() && Ability.IsValid())
	{
		if (AbilitySystemComponent->IsAnimatingAbility(Ability.Get())
			&& AbilitySystemComponent->GetCurrentMontage() == MontageToPlay)
		{
			// Unbind the delegate in this case so OnMontageEnded does not get called as well
			BlendingOutDelegate.Unbind();

			AbilitySystemComponent->CurrentMontageStop();

			// Let the BP handle the interrupt as well
			OnInterrupted.Broadcast();
		}
	}
}

UAbilityTask_PlayMontageAndWait* UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(UObject* WorldContextObject,
	FName TaskInstanceName, UAnimMontage *MontageToPlay, float Rate, FName StartSection)
{
	UAbilityTask_PlayMontageAndWait* MyObj = NewTask<UAbilityTask_PlayMontageAndWait>(WorldContextObject, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->Rate = Rate;
	MyObj->StartSection = StartSection;

	return MyObj;
}

void UAbilityTask_PlayMontageAndWait::Activate()
{
	UGameplayAbility* MyAbility = Ability.Get();
	if (MyAbility == nullptr)
	{
		return;
	}

	bool bPlayedMontage = false;

	if (AbilitySystemComponent.IsValid())
	{
		const FGameplayAbilityActorInfo* ActorInfo = MyAbility->GetCurrentActorInfo();
		if (ActorInfo->AnimInstance.IsValid())
		{
			if (AbilitySystemComponent->PlayMontage(MyAbility, MyAbility->GetCurrentActivationInfo(), MontageToPlay, Rate, StartSection) > 0.f)
			{
				// Playing a montage could potentially fire off a callback into game code which could kill this ability! Early out if we are  pending kill.
				if (IsPendingKill())
				{
					OnCancelled.Broadcast();
					return;
				}

				InterruptedHandle = MyAbility->OnGameplayAbilityCancelled.AddUObject(this, &UAbilityTask_PlayMontageAndWait::OnMontageInterrupted);

				BlendingOutDelegate.BindUObject(this, &UAbilityTask_PlayMontageAndWait::OnMontageBlendingOut);
				ActorInfo->AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate);

				bPlayedMontage = true;
			}
		}
	}

	if (!bPlayedMontage)
	{
		ABILITY_LOG(Warning, TEXT("UAbilityTask_PlayMontageAndWait called in Ability %s failed to play montage; Task Instance Name %s."), *MyAbility->GetName(), *InstanceName.ToString());
		OnCancelled.Broadcast();
	}
}

void UAbilityTask_PlayMontageAndWait::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());
	OnCancelled.Broadcast();
	Super::ExternalCancel();
}

void UAbilityTask_PlayMontageAndWait::OnDestroy(bool AbilityEnded)
{
	// Note: Clearing montage end delegate isn't necessary since its not a multicast and will be cleared when the next montage plays.
	// (If we are destroyed, it will detect this and not do anything)

	// This delegate, however, should be cleared as it is a multicast
	if (Ability.IsValid())
	{
		Ability->OnGameplayAbilityCancelled.Remove(InterruptedHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

FString UAbilityTask_PlayMontageAndWait::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability.IsValid())
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		if (ActorInfo->AnimInstance.IsValid())
		{
			PlayingMontage = ActorInfo->AnimInstance->GetCurrentActiveMontage();
		}
	}

	return FString::Printf(TEXT("PlayMontageAndWait. MontageToPlay: %s  (Currently Playing): %s"), *GetNameSafe(MontageToPlay), *GetNameSafe(PlayingMontage));
}