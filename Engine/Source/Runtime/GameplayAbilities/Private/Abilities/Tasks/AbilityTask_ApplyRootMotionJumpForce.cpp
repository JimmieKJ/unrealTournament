// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionJumpForce.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"

UAbilityTask_ApplyRootMotionJumpForce::UAbilityTask_ApplyRootMotionJumpForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;
	bIsFinished = false;
	RootMotionSourceID = (uint16)ERootMotionSourceID::Invalid;
	MovementComponent = nullptr;
	PathOffsetCurve = nullptr;
	TimeMappingCurve = nullptr;
	bHasLanded = false;
}

UAbilityTask_ApplyRootMotionJumpForce* UAbilityTask_ApplyRootMotionJumpForce::ApplyRootMotionJumpForce(UObject* WorldContextObject, FName TaskInstanceName, FRotator Rotation, float Distance, float Height, float Duration, float MinimumLandedTriggerTime, bool bFinishOnLanded, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve)
{
	auto MyTask = NewAbilityTask<UAbilityTask_ApplyRootMotionJumpForce>(WorldContextObject, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->Rotation = Rotation;
	MyTask->Distance = Distance;
	MyTask->Height = Height;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // No zero duration
	MyTask->MinimumLandedTriggerTime = MinimumLandedTriggerTime * Duration; // MinimumLandedTriggerTime is normalized
	MyTask->bFinishOnLanded = bFinishOnLanded;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->TimeMappingCurve = TimeMappingCurve;
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UAbilityTask_ApplyRootMotionJumpForce::Activate()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (Character)
	{
		Character->LandedDelegate.AddDynamic(this, &UAbilityTask_ApplyRootMotionJumpForce::OnLandedCallback);
	}
	SetWaitingOnAvatar();
}

void UAbilityTask_ApplyRootMotionJumpForce::OnLandedCallback(const FHitResult& Hit)
{
	bHasLanded = true;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (Character && Character->bClientUpdating)
	{
		// If in a move replay, we just mark that we landed so that next tick we trigger landed
	}
	else
	{
		// TriggerLanded immediately if we're past time allowed, otherwise it'll get caught next valid tick
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime >= (StartTime+MinimumLandedTriggerTime))
		{
			TriggerLanded();
		}
	}
}

void UAbilityTask_ApplyRootMotionJumpForce::TriggerLanded()
{
	OnLanded.Broadcast();

	if (bFinishOnLanded)
	{
		Finish();
	}
}

void UAbilityTask_ApplyRootMotionJumpForce::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	Super::InitSimulatedTask(InGameplayTasksComponent);

	SharedInitAndApply();
}

void UAbilityTask_ApplyRootMotionJumpForce::SharedInitAndApply()
{
	if (AbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(AbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionJumpForce") : ForceName;
			FRootMotionSource_JumpForce* JumpForce = new FRootMotionSource_JumpForce();
			JumpForce->InstanceName = ForceName;
			JumpForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			JumpForce->Priority = 500;
			JumpForce->Duration = Duration;
			JumpForce->Rotation = Rotation;
			JumpForce->Distance = Distance;
			JumpForce->Height = Height;
			JumpForce->Duration = Duration;
			JumpForce->bDisableTimeout = bFinishOnLanded; // If we finish on landed, we need to disable force's timeout
			JumpForce->PathOffsetCurve = PathOffsetCurve;
			JumpForce->TimeMappingCurve = TimeMappingCurve;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(JumpForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilityTask_ApplyRootMotionJumpForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

void UAbilityTask_ApplyRootMotionJumpForce::Finish()
{
	bIsFinished = true;

	if (!bIsSimulating)
	{
		AActor* MyActor = GetAvatarActor();
		if (MyActor)
		{
			MyActor->ForceNetUpdate();
			OnFinish.Broadcast();
		}
	}

	EndTask();
}

void UAbilityTask_ApplyRootMotionJumpForce::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	if (bHasLanded && CurrentTime >= (StartTime+MinimumLandedTriggerTime))
	{
		TriggerLanded();
		return;
	}

	Super::TickTask(DeltaTime);

	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		if (!bFinishOnLanded && CurrentTime >= EndTime)
		{
			// Task has finished
			Finish();
		}
	}
	else
	{
		Finish();
	}
}

void UAbilityTask_ApplyRootMotionJumpForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, ForceName);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, Rotation);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, Distance);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, Height);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, Duration);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, MinimumLandedTriggerTime);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, bFinishOnLanded);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, PathOffsetCurve);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionJumpForce, TimeMappingCurve);
}

void UAbilityTask_ApplyRootMotionJumpForce::PreDestroyFromReplication()
{
	Finish();
}

void UAbilityTask_ApplyRootMotionJumpForce::OnDestroy(bool AbilityIsEnding)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (Character)
	{
		Character->LandedDelegate.RemoveDynamic(this, &UAbilityTask_ApplyRootMotionJumpForce::OnLandedCallback);
	}

	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy(AbilityIsEnding);
}
