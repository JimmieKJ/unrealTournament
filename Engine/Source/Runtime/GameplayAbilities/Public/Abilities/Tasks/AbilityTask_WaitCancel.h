// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AbilityTask.h"
#include "GameplayAbility.h"
#include "AbilityTask_WaitCancel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitCancelDelegate);

UCLASS(MinimalAPI)
class UAbilityTask_WaitCancel : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitCancelDelegate	OnCancel;
	
	UFUNCTION()
	void OnCancelCallback();

	UFUNCTION()
	void OnLocalCancelCallback();

	UFUNCTION(BlueprintCallable, meta=(HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true", DisplayName="Wait for Cancel Input"), Category="Ability|Tasks")
	static UAbilityTask_WaitCancel* WaitCancel(UObject* WorldContextObject);	

	virtual void Activate() override;

protected:

	virtual void OnDestroy(bool AbilityEnding) override;

	bool RegisteredCallbacks;
	
};