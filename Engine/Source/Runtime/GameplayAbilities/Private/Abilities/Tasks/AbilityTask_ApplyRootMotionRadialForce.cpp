// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionRadialForce.h"
#include "Net/UnrealNetwork.h"

UAbilityTask_ApplyRootMotionRadialForce::UAbilityTask_ApplyRootMotionRadialForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;
	bIsFinished = false;
	RootMotionSourceID = (uint16)ERootMotionSourceID::Invalid;
	MovementComponent = nullptr;
	StrengthDistanceFalloff = nullptr;
	StrengthOverTime = nullptr;
	bUseFixedWorldDirection = false;
}

UAbilityTask_ApplyRootMotionRadialForce* UAbilityTask_ApplyRootMotionRadialForce::ApplyRootMotionRadialForce(UObject* WorldContextObject, FName TaskInstanceName, FVector Location, AActor* LocationActor, float Strength, float Duration, float Radius, bool bIsPush, bool bIsAdditive, bool bNoZForce, UCurveFloat* StrengthDistanceFalloff, UCurveFloat* StrengthOverTime, bool bUseFixedWorldDirection, FRotator FixedWorldDirection)
{
	auto MyTask = NewAbilityTask<UAbilityTask_ApplyRootMotionRadialForce>(WorldContextObject, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->Location = Location;
	MyTask->LocationActor = LocationActor;
	MyTask->Strength = Strength;
	MyTask->Radius = FMath::Max(Radius, SMALL_NUMBER); // No zero radius
	MyTask->Duration = Duration;
	MyTask->bIsPush = bIsPush;
	MyTask->bIsAdditive = bIsAdditive;
	MyTask->bNoZForce = bNoZForce;
	MyTask->StrengthDistanceFalloff = StrengthDistanceFalloff;
	MyTask->StrengthOverTime = StrengthOverTime;
	MyTask->bUseFixedWorldDirection = bUseFixedWorldDirection;
	MyTask->FixedWorldDirection = FixedWorldDirection;
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UAbilityTask_ApplyRootMotionRadialForce::Activate()
{
}

void UAbilityTask_ApplyRootMotionRadialForce::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	Super::InitSimulatedTask(InGameplayTasksComponent);

	SharedInitAndApply();
}

void UAbilityTask_ApplyRootMotionRadialForce::SharedInitAndApply()
{
	if (AbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(AbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionRadialForce") : ForceName;
			FRootMotionSource_RadialForce* RadialForce = new FRootMotionSource_RadialForce();
			RadialForce->InstanceName = ForceName;
			RadialForce->AccumulateMode = bIsAdditive ? ERootMotionAccumulateMode::Additive : ERootMotionAccumulateMode::Override;
			RadialForce->Priority = 5;
			RadialForce->Location = Location;
			RadialForce->LocationActor = LocationActor;
			RadialForce->Duration = Duration;
			RadialForce->Radius = Radius;
			RadialForce->Strength = Strength;
			RadialForce->bIsPush = bIsPush;
			RadialForce->bNoZForce = bNoZForce;
			RadialForce->StrengthDistanceFalloff = StrengthDistanceFalloff;
			RadialForce->StrengthOverTime = StrengthOverTime;
			RadialForce->bUseFixedWorldDirection = bUseFixedWorldDirection;
			RadialForce->FixedWorldDirection = FixedWorldDirection;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(RadialForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilityTask_ApplyRootMotionRadialForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

void UAbilityTask_ApplyRootMotionRadialForce::TickTask(float DeltaTime)
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
		const bool bIsInfiniteDuration = Duration < 0.f;
		if (!bIsInfiniteDuration && CurrentTime >= EndTime)
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

void UAbilityTask_ApplyRootMotionRadialForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, ForceName);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, Location);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, LocationActor);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, Radius);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, Strength);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, Duration);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, bIsPush);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, bIsAdditive);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, bNoZForce);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, StrengthDistanceFalloff);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, StrengthOverTime);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, bUseFixedWorldDirection);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionRadialForce, FixedWorldDirection);
}

void UAbilityTask_ApplyRootMotionRadialForce::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UAbilityTask_ApplyRootMotionRadialForce::OnDestroy(bool AbilityIsEnding)
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy(AbilityIsEnding);
}
