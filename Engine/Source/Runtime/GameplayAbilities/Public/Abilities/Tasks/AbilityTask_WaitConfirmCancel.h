// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AbilityTask.h"
#include "GameplayAbility.h"
#include "AbilityTask_WaitConfirmCancel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitConfirmCancelDelegate);

// Fixme: this name is conflicting with AbilityTask_WaitConfirm
// UAbilityTask_WaitConfirmCancel = Wait for Targeting confirm/cancel
// UAbilityTask_WaitConfirm = Wait for server to confirm ability activation

UCLASS(MinimalAPI)
class UAbilityTask_WaitConfirmCancel : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitConfirmCancelDelegate	OnConfirm;

	UPROPERTY(BlueprintAssignable)
	FWaitConfirmCancelDelegate	OnCancel;
	
	UFUNCTION()
	void OnConfirmCallback();

	UFUNCTION()
	void OnCancelCallback();

	UFUNCTION()
	void OnLocalConfirmCallback();

	UFUNCTION()
	void OnLocalCancelCallback();

	UFUNCTION(BlueprintCallable, meta=(HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "true", DisplayName="Wait for Confirm Input"), Category="Ability|Tasks")
	static UAbilityTask_WaitConfirmCancel* WaitConfirmCancel(UObject* WorldContextObject);	

	virtual void Activate() override;

protected:

	virtual void OnDestroy(bool AbilityEnding) override;

	bool RegisteredCallbacks;
	
};