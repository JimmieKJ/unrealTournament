// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeThreshold.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayEffectExtension.h"

UAbilityTask_WaitAttributeChangeThreshold::UAbilityTask_WaitAttributeChangeThreshold(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTriggerOnce = false;
	bMatchedComparisonLastAttributeChange = false;
}

UAbilityTask_WaitAttributeChangeThreshold* UAbilityTask_WaitAttributeChangeThreshold::WaitForAttributeChangeThreshold(UObject* WorldContextObject, FGameplayAttribute Attribute, TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType, float ComparisonValue, bool bTriggerOnce)
{
	auto MyTask = NewAbilityTask<UAbilityTask_WaitAttributeChangeThreshold>(WorldContextObject);
	MyTask->Attribute = Attribute;
	MyTask->ComparisonType = ComparisonType;
	MyTask->ComparisonValue = ComparisonValue;
	MyTask->bTriggerOnce = bTriggerOnce;

	return MyTask;
}

void UAbilityTask_WaitAttributeChangeThreshold::Activate()
{
	if (AbilitySystemComponent)
	{
		const float CurrentValue = AbilitySystemComponent->GetNumericAttribute(Attribute);
		bMatchedComparisonLastAttributeChange = DoesValuePassComparison(CurrentValue);

		// Broadcast OnChange immediately with current value
		OnChange.Broadcast(bMatchedComparisonLastAttributeChange, CurrentValue);

		OnAttributeChangeDelegateHandle = AbilitySystemComponent->RegisterGameplayAttributeEvent(Attribute).AddUObject(this, &UAbilityTask_WaitAttributeChangeThreshold::OnAttributeChange);
	}
}

void UAbilityTask_WaitAttributeChangeThreshold::OnAttributeChange(float NewValue, const FGameplayEffectModCallbackData* Data)
{
	bool bPassedComparison = DoesValuePassComparison(NewValue);
	if (bPassedComparison != bMatchedComparisonLastAttributeChange)
	{
		bMatchedComparisonLastAttributeChange = bPassedComparison;
		OnChange.Broadcast(bPassedComparison, NewValue);
		if (bTriggerOnce)
		{
			EndTask();
		}
	}
}

bool UAbilityTask_WaitAttributeChangeThreshold::DoesValuePassComparison(float Value) const
{
	bool bPassedComparison = true;
	switch (ComparisonType)
	{
	case EWaitAttributeChangeComparison::ExactlyEqualTo:
		bPassedComparison = (Value == ComparisonValue);
		break;		
	case EWaitAttributeChangeComparison::GreaterThan:
		bPassedComparison = (Value > ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::GreaterThanOrEqualTo:
		bPassedComparison = (Value >= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThan:
		bPassedComparison = (Value < ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThanOrEqualTo:
		bPassedComparison = (Value <= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::NotEqualTo:
		bPassedComparison = (Value != ComparisonValue);
		break;
	default:
		break;
	}
	return bPassedComparison;
}

void UAbilityTask_WaitAttributeChangeThreshold::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RegisterGameplayAttributeEvent(Attribute).Remove(OnAttributeChangeDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}