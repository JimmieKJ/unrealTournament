// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AbilityTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilityTask_WaitDelay.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitDelayDelegate);

UCLASS(MinimalAPI)
class UAbilityTask_WaitDelay : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitDelayDelegate	OnFinish;

	virtual void Activate() override;

	/** Return debug string describing task */
	virtual FString GetDebugString() const override;

	/** Wait specified time. This is functionally the same as a standard Delay node. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitDelay* WaitDelay(UObject* WorldContextObject, float Time);

private:

	void OnTimeFinish();

	virtual void OnDestroy(bool AbilityEnded) override;
	float Time;
	float TimeStarted;
};