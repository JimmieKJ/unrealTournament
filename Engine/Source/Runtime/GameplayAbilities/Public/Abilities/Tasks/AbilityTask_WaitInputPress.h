// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilityTask_WaitInputPress.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputPressDelegate, float, TimeWaited);

/**
 *	Waits until the input is pressed from activating an ability. This should be true immediately upon starting the ability, since the key was pressed to activate it.
 *	We expect server to execute this task in parallel and keep its own time. We do not keep track of 
 */
UCLASS(MinimalAPI)
class UAbilityTask_WaitInputPress : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FInputPressDelegate		OnPress;

	UFUNCTION()
	void OnPressCallback();

	virtual void Activate() override;

	/** Wait until the user presses the input button for this ability's activation. Returns time this node spent waiting for the press. Will return 0 if input was already down. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitInputPress* WaitInputPress(UObject* WorldContextObject, bool bTestAlreadyPressed=false);

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	float StartTime;
	bool bTestInitialState;
	FDelegateHandle DelegateHandle;
};