// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AttributeSet.h"
#include "AbilityTask_WaitAttributeChange.h"
#include "AbilityTask_WaitAttributeChangeThreshold.generated.h"

struct FGameplayEffectModCallbackData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaitAttributeChangeThresholdDelegate, bool, bMatchesComparison, float, CurrentValue);

/**
 *	Waits for an attribute to match a threshold
 */
UCLASS(MinimalAPI)
class UAbilityTask_WaitAttributeChangeThreshold : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAttributeChangeThresholdDelegate OnChange;

	virtual void Activate() override;

	void OnAttributeChange(float NewValue, const FGameplayEffectModCallbackData* Data);

	/** Wait on attribute change meeting a comparison threshold. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitAttributeChangeThreshold* WaitForAttributeChangeThreshold(UObject* WorldContextObject, FGameplayAttribute Attribute, TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType, float ComparisonValue, bool bTriggerOnce);

	FGameplayAttribute Attribute;
	TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType;
	float ComparisonValue;
	bool bTriggerOnce;
	FDelegateHandle OnAttributeChangeDelegateHandle;

protected:

	bool bMatchedComparisonLastAttributeChange;

	virtual void OnDestroy(bool AbilityEnded) override;

	bool DoesValuePassComparison(float Value) const;
};
