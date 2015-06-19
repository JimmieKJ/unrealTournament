// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChange.h"

#include "AbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayEffectExtension.h"

UAbilityTask_WaitAttributeChange::UAbilityTask_WaitAttributeChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTriggerOnce = false;
}

UAbilityTask_WaitAttributeChange* UAbilityTask_WaitAttributeChange::WaitForAttributeChange(UObject* WorldContextObject, FGameplayAttribute InAttribute, FGameplayTag InWithTag, FGameplayTag InWithoutTag, bool TriggerOnce)
{
	auto MyObj = NewTask<UAbilityTask_WaitAttributeChange>(WorldContextObject);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->Attribute = InAttribute;
	MyObj->ComparisonType = EWaitAttributeChangeComparison::None;
	MyObj->bTriggerOnce = TriggerOnce;

	return MyObj;
}

UAbilityTask_WaitAttributeChange* UAbilityTask_WaitAttributeChange::WaitForAttributeChangeWithComparison(UObject* WorldContextObject, FGameplayAttribute InAttribute, FGameplayTag InWithTag, FGameplayTag InWithoutTag, TEnumAsByte<EWaitAttributeChangeComparison::Type> InComparisonType, float InComparisonValue, bool TriggerOnce)
{
	auto MyObj = NewTask<UAbilityTask_WaitAttributeChange>(WorldContextObject);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->Attribute = InAttribute;
	MyObj->ComparisonType = InComparisonType;
	MyObj->ComparisonValue = InComparisonValue;
	MyObj->bTriggerOnce = TriggerOnce;

	return MyObj;
}

void UAbilityTask_WaitAttributeChange::Activate()
{
	if (AbilitySystemComponent.IsValid())
	{
		OnAttributeChangeDelegateHandle = AbilitySystemComponent->RegisterGameplayAttributeEvent(Attribute).AddUObject(this, &UAbilityTask_WaitAttributeChange::OnAttributeChange);
	}
}

void UAbilityTask_WaitAttributeChange::OnAttributeChange(float NewValue, const FGameplayEffectModCallbackData* Data)
{
	if (Data == nullptr)
	{
		// There may be no execution data associated with this change, for example a GE being removed. 
		// In this case, we auto fail any WithTag requirement and auto pass any WithoutTag requirement
		if (WithTag.IsValid())
		{
			return;
		}
	}
	else
	{
		if ((WithTag.IsValid() && !Data->EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag(WithTag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit)) ||
			(WithoutTag.IsValid() && Data->EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag(WithoutTag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit)))
		{
			// Failed tag check
			return;
		}
	}	

	bool PassedComparison = true;
	switch (ComparisonType)
	{
	case EWaitAttributeChangeComparison::ExactlyEqualTo:
		PassedComparison = (NewValue == ComparisonValue);
		break;		
	case EWaitAttributeChangeComparison::GreaterThan:
		PassedComparison = (NewValue > ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::GreaterThanOrEqualTo:
		PassedComparison = (NewValue >= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThan:
		PassedComparison = (NewValue < ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThanOrEqualTo:
		PassedComparison = (NewValue <= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::NotEqualTo:
		PassedComparison = (NewValue != ComparisonValue);
		break;
	default:
		break;
	}
	if (PassedComparison)
	{
		OnChange.Broadcast();
		if (bTriggerOnce)
		{
			EndTask();
		}
	}
}

void UAbilityTask_WaitAttributeChange::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayAttributeEvent(Attribute).Remove(OnAttributeChangeDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}