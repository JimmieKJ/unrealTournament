// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AttributeSet.h"
#include "AbilityTask_WaitAttributeChange.h"
#include "AbilityTask_WaitAttributeChangeRatioThreshold.generated.h"

struct FGameplayEffectModCallbackData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaitAttributeChangeRatioThresholdDelegate, bool, bMatchesComparison, float, CurrentRatio);

/**
 *	Waits for the ratio between two attributes to match a threshold
 */
UCLASS(MinimalAPI)
class UAbilityTask_WaitAttributeChangeRatioThreshold : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAttributeChangeRatioThresholdDelegate OnChange;

	virtual void Activate() override;

	void OnNumeratorAttributeChange(float NewValue, const FGameplayEffectModCallbackData* Data);
	void OnDenominatorAttributeChange(float NewValue, const FGameplayEffectModCallbackData* Data);

	/** Wait on attribute ratio change meeting a comparison threshold. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitAttributeChangeRatioThreshold* WaitForAttributeChangeRatioThreshold(UObject* WorldContextObject, FGameplayAttribute AttributeNumerator, FGameplayAttribute AttributeDenominator, TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType, float ComparisonValue, bool bTriggerOnce);

	FGameplayAttribute AttributeNumerator;
	FGameplayAttribute AttributeDenominator;
	TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType;
	float ComparisonValue;
	bool bTriggerOnce;
	FDelegateHandle OnNumeratorAttributeChangeDelegateHandle;
	FDelegateHandle OnDenominatorAttributeChangeDelegateHandle;

protected:

	float LastAttributeNumeratorValue;
	float LastAttributeDenominatorValue;
	bool bMatchedComparisonLastAttributeChange;

	/** Timer used when either numerator or denominator attribute is changed to delay checking of ratio to avoid false positives (MaxHealth changed before Health updates accordingly) */
	FTimerHandle CheckAttributeTimer;

	void OnAttributeChange();
	void OnRatioChange();

	virtual void OnDestroy(bool AbilityEnded) override;

	bool DoesValuePassComparison(float ValueNumerator, float ValueDenominator) const;
};
