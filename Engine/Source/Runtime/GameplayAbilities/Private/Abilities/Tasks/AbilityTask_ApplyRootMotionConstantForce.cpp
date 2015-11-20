// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Net/UnrealNetwork.h"

UAbilityTask_ApplyRootMotionConstantForce::UAbilityTask_ApplyRootMotionConstantForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;
	bIsFinished = false;
	RootMotionSourceID = (uint16)ERootMotionSourceID::Invalid;
	MovementComponent = nullptr;
}

UAbilityTask_ApplyRootMotionConstantForce* UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(UObject* WorldContextObject, FName TaskInstanceName, FVector WorldDirection, float Strength, float Duration, bool bIsAdditive, bool bDisableImpartingVelocityOnRemoval)
{
	auto MyTask = NewAbilityTask<UAbilityTask_ApplyRootMotionConstantForce>(WorldContextObject, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->WorldDirection = WorldDirection.GetSafeNormal();
	MyTask->Strength = Strength;
	MyTask->Duration = FMath::Max(Duration, 0.001f);		// Avoid negative or divide-by-zero cases
	MyTask->bIsAdditive = bIsAdditive;
	MyTask->bDisableImpartingVelocityOnRemoval = bDisableImpartingVelocityOnRemoval;
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UAbilityTask_ApplyRootMotionConstantForce::Activate()
{
}

void UAbilityTask_ApplyRootMotionConstantForce::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	Super::InitSimulatedTask(InGameplayTasksComponent);

	SharedInitAndApply();
}

void UAbilityTask_ApplyRootMotionConstantForce::SharedInitAndApply()
{
	if (AbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(AbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionConstantForce"): ForceName;
			FRootMotionSource_ConstantForce* ConstantForce = new FRootMotionSource_ConstantForce();
			ConstantForce->InstanceName = ForceName;
			ConstantForce->AccumulateMode = bIsAdditive ? ERootMotionAccumulateMode::Additive : ERootMotionAccumulateMode::Override;
			if (bDisableImpartingVelocityOnRemoval)
			{
				ConstantForce->bImpartsVelocityOnRemoval = false;
			}
			ConstantForce->Priority = 5;
			ConstantForce->Force = WorldDirection * Strength;
			ConstantForce->Duration = Duration;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(ConstantForce);

			if (Ability.IsValid())
			{
				Ability.Get()->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilityTask_ApplyRootMotionConstantForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability.IsValid() ? *Ability.Get()->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

void UAbilityTask_ApplyRootMotionConstantForce::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);

	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime >= EndTime)
		{
			// Task has finished
			bIsFinished = true;
			if (!bIsSimulating)
			{
				MyActor->ForceNetUpdate();
				OnFinish.Broadcast();
				EndTask();
			}
		}
	}
	else
	{
		bIsFinished = true;
		EndTask();
	}
}

void UAbilityTask_ApplyRootMotionConstantForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionConstantForce, ForceName);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionConstantForce, WorldDirection);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionConstantForce, Strength);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionConstantForce, Duration);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionConstantForce, bIsAdditive);
}

void UAbilityTask_ApplyRootMotionConstantForce::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UAbilityTask_ApplyRootMotionConstantForce::OnDestroy(bool AbilityIsEnding)
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy(AbilityIsEnding);
}
