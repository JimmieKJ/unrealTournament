// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectBlockedImmunity.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectExtension.h"

UAbilityTask_WaitGameplayEffectBlockedImmunity::UAbilityTask_WaitGameplayEffectBlockedImmunity(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UseExternalOwner = false;
	ExternalOwner = nullptr;
}

UAbilityTask_WaitGameplayEffectBlockedImmunity* UAbilityTask_WaitGameplayEffectBlockedImmunity::WaitGameplayEffectBlockedByImmunity(UObject* WorldContextObject, FGameplayTagRequirements InSourceTagRequirements, FGameplayTagRequirements InTargetTagRequirements, AActor* InOptionalExternalTarget, bool InTriggerOnce)
{
	auto MyObj = NewAbilityTask<UAbilityTask_WaitGameplayEffectBlockedImmunity>(WorldContextObject);
	MyObj->SourceTagRequirements = InSourceTagRequirements;
	MyObj->TargetTagRequirements = InTargetTagRequirements;
	MyObj->TriggerOnce = InTriggerOnce;
	MyObj->SetExternalActor(InOptionalExternalTarget);
	return MyObj;
}

void UAbilityTask_WaitGameplayEffectBlockedImmunity::Activate()
{
	if (GetASC())
	{
		RegisterDelegate();
	}
}

void UAbilityTask_WaitGameplayEffectBlockedImmunity::ImmunityCallback(const FGameplayEffectSpec& BlockedSpec, const FActiveGameplayEffect* ImmunityGE)
{
	bool PassedComparison = false;

	if (!SourceTagRequirements.RequirementsMet(*BlockedSpec.CapturedSourceTags.GetAggregatedTags()))
	{
		return;
	}
	if (!TargetTagRequirements.RequirementsMet(*BlockedSpec.CapturedTargetTags.GetAggregatedTags()))
	{
		return;
	}
	
	// We have to copy the spec, since the blocked spec is not ours
	FGameplayEffectSpecHandle SpecHandle(new FGameplayEffectSpec(BlockedSpec));

	Blocked.Broadcast(SpecHandle, ImmunityGE->Handle);
	
	if (TriggerOnce)
	{
		EndTask();
	}
}

void UAbilityTask_WaitGameplayEffectBlockedImmunity::OnDestroy(bool AbilityEnded)
{
	if (GetASC())
	{
		RemoveDelegate();
	}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitGameplayEffectBlockedImmunity::SetExternalActor(AActor* InActor)
{
	if (InActor)
	{
		UseExternalOwner = true;
		ExternalOwner  = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InActor);
	}
}

UAbilitySystemComponent* UAbilityTask_WaitGameplayEffectBlockedImmunity::GetASC()
{
	if (UseExternalOwner)
	{
		return ExternalOwner;
	}
	return AbilitySystemComponent;
}

void UAbilityTask_WaitGameplayEffectBlockedImmunity::RegisterDelegate()
{
	if (UAbilitySystemComponent* ASC =  GetASC())
	{
		// Only do this on the server. Clients could mispredict.
		if (ASC->IsNetSimulating() == false)
		{
			DelegateHandle = ASC->OnImmunityBlockGameplayEffectDelegate.AddUObject(this, &UAbilityTask_WaitGameplayEffectBlockedImmunity::ImmunityCallback);
		}
	}
}

void UAbilityTask_WaitGameplayEffectBlockedImmunity::RemoveDelegate()
{
	if (DelegateHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC =  GetASC())
		{
			ASC->OnImmunityBlockGameplayEffectDelegate.Remove(DelegateHandle);
			DelegateHandle.Reset();
		}
	}
}