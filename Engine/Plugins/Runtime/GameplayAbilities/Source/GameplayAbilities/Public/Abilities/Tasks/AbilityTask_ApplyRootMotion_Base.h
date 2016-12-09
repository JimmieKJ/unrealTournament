// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AbilityTask.h"
#include "AbilityTask_ApplyRootMotion_Base.generated.h"

class UCharacterMovementComponent;

UCLASS(MinimalAPI)
class UAbilityTask_ApplyRootMotion_Base : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;

protected:

	virtual void SharedInitAndApply() {};

	void SetFinishVelocity(FName RootMotionSourceName, FVector FinishVelocity);
	void ClampFinishVelocity(FName RootMotionSourceName, float VelocityClamp);

protected:

	UPROPERTY(Replicated)
	FName ForceName;

	UPROPERTY()
	UCharacterMovementComponent* MovementComponent; 
	
	uint16 RootMotionSourceID;

	bool bIsFinished;

	float StartTime;
	float EndTime;
};