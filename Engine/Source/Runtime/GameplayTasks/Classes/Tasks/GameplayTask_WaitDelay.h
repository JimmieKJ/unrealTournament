// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameplayTask.h"
#include "GameplayTask_WaitDelay.generated.h"

UCLASS(MinimalAPI)
class UGameplayTask_WaitDelay : public UGameplayTask
{
	GENERATED_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTaskDelayDelegate);

public:
	UGameplayTask_WaitDelay(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FTaskDelayDelegate OnFinish;

	virtual void Activate() override;

	/** Return debug string describing task */
	virtual FString GetDebugString() const override;

	/** Wait specified time. This is functionally the same as a standard Delay node. */
	UFUNCTION(BlueprintCallable, Category = "GameplayTasks", meta = (AdvancedDisplay = "TaskOwner", DefaultToSelf = "TaskOwner", BlueprintInternalUseOnly = "TRUE"))
	static UGameplayTask_WaitDelay* TaskWaitDelay(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, float Time);

private:

	void OnTimeFinish();
	
	float Time;
	float TimeStarted;
};