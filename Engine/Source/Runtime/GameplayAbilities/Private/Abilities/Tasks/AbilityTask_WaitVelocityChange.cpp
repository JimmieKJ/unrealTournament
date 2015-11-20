// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "AbilityTask_WaitVelocityChange.h"

UAbilityTask_WaitVelocityChange::UAbilityTask_WaitVelocityChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

void UAbilityTask_WaitVelocityChange::TickTask(float DeltaTime)
{
	if (CachedMovementComponent)
	{
		float dot = FVector::DotProduct(Direction, CachedMovementComponent->Velocity);

		if (dot > MinimumMagnitude)
		{
			OnVelocityChage.Broadcast();
			EndTask();
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilityTask_WaitVelocityChange ticked without a valid movement component. ending."));
		EndTask();
	}
}

UAbilityTask_WaitVelocityChange* UAbilityTask_WaitVelocityChange::CreateWaitVelocityChange(UObject* WorldContextObject, FVector InDirection, float InMinimumMagnitude)
{
	auto MyObj = NewAbilityTask<UAbilityTask_WaitVelocityChange>(WorldContextObject);

	MyObj->MinimumMagnitude = InMinimumMagnitude;
	MyObj->Direction = InDirection.GetSafeNormal();
	

	return MyObj;
}

void UAbilityTask_WaitVelocityChange::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	CachedMovementComponent = ActorInfo->MovementComponent.Get();
}