// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilityTask_WaitConfirm.generated.h"

UCLASS(MinimalAPI)
class UAbilityTask_WaitConfirm : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate	OnConfirm;

	UFUNCTION()
	void OnConfirmCallback(UGameplayAbility* Ability);

	virtual void Activate() override;

	/** Wait until the server confirms the use of this ability. This is used to gate predictive portions of the ability */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitConfirm* WaitConfirm(UObject* WorldContextObject);

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;

	FDelegateHandle OnConfirmCallbackDelegateHandle;
};