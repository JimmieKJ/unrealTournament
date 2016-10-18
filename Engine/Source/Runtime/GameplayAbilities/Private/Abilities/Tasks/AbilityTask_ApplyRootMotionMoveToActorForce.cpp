// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToActorForce.h"
#include "Net/UnrealNetwork.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 DebugMoveToActorForce = 0;
static FAutoConsoleVariableRef CVarDebugMoveToActorForce(
TEXT("AbilitySystem.DebugMoveToActorForce"),
	DebugMoveToActorForce,
	TEXT("Show debug info for MoveToActorForce"),
	ECVF_Default
	);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

UAbilityTask_ApplyRootMotionMoveToActorForce::UAbilityTask_ApplyRootMotionMoveToActorForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;
	bDisableDestinationReachedInterrupt = false;
	bSetNewMovementMode = false;
	NewMovementMode = EMovementMode::MOVE_Walking;
	PreviousMovementMode = EMovementMode::MOVE_None;
	RootMotionSourceID = (uint16)ERootMotionSourceID::Invalid;
	TargetLocationOffset = FVector::ZeroVector;
	OffsetAlignment = ERootMotionMoveToActorTargetOffsetType::AlignFromTargetToSource;
	bIsFinished = false;
	MovementComponent = nullptr;
	bRestrictSpeedToExpected = false;
	PathOffsetCurve = nullptr;
	TimeMappingCurve = nullptr;
	TargetLerpSpeedHorizontalCurve = nullptr;
	TargetLerpSpeedVerticalCurve = nullptr;
	VelocityOnFinishMode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
	SetVelocityOnFinish = FVector::ZeroVector;
}

UAbilityTask_ApplyRootMotionMoveToActorForce* UAbilityTask_ApplyRootMotionMoveToActorForce::ApplyRootMotionMoveToActorForce(UObject* WorldContextObject, FName TaskInstanceName, AActor* TargetActor, FVector TargetLocationOffset, ERootMotionMoveToActorTargetOffsetType OffsetAlignment, float Duration, UCurveFloat* TargetLerpSpeedHorizontal, UCurveFloat* TargetLerpSpeedVertical, bool bSetNewMovementMode, EMovementMode MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, bool bDisableDestinationReachedInterrupt)
{
	auto MyTask = NewAbilityTask<UAbilityTask_ApplyRootMotionMoveToActorForce>(WorldContextObject, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->TargetActor = TargetActor;
	MyTask->TargetLocationOffset = TargetLocationOffset;
	MyTask->OffsetAlignment = OffsetAlignment;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // Avoid negative or divide-by-zero cases
	MyTask->bDisableDestinationReachedInterrupt = bDisableDestinationReachedInterrupt;
	MyTask->TargetLerpSpeedHorizontalCurve = TargetLerpSpeedHorizontal;
	MyTask->TargetLerpSpeedVerticalCurve = TargetLerpSpeedVertical;
	MyTask->bSetNewMovementMode = bSetNewMovementMode;
	MyTask->NewMovementMode = MovementMode;
	MyTask->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->TimeMappingCurve = TimeMappingCurve;
	MyTask->VelocityOnFinishMode = VelocityOnFinishMode;
	MyTask->SetVelocityOnFinish = SetVelocityOnFinish;
	if (MyTask->GetAvatarActor() != nullptr)
	{
		MyTask->StartLocation = MyTask->GetAvatarActor()->GetActorLocation();
	}
	else
	{
		checkf(false, TEXT("UAbilityTask_ApplyRootMotionMoveToActorForce called without valid avatar actor to get start location from."));
		MyTask->StartLocation = TargetActor ? TargetActor->GetActorLocation() : FVector(0.f);
	}
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UAbilityTask_ApplyRootMotionMoveToActorForce::Activate()
{
}

void UAbilityTask_ApplyRootMotionMoveToActorForce::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	Super::InitSimulatedTask(InGameplayTasksComponent);

	SharedInitAndApply();
}

void UAbilityTask_ApplyRootMotionMoveToActorForce::OnRep_TargetLocation()
{
	if (bIsSimulating)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Draw debug
		if (DebugMoveToActorForce > 0)
		{
			DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 10, FColor::Green, false, 15.f);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		SetRootMotionTargetLocation(TargetLocation);
	}
}

void UAbilityTask_ApplyRootMotionMoveToActorForce::SharedInitAndApply()
{
	if (AbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(AbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			if (bSetNewMovementMode)
			{
				PreviousMovementMode = MovementComponent->MovementMode;
				MovementComponent->SetMovementMode(NewMovementMode);
			}

			// Set initial target location
			if (TargetActor)
			{
				TargetLocation = CalculateTargetOffset();
			}

			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionMoveToActorForce") : ForceName;
			FRootMotionSource_MoveToDynamicForce* MoveToActorForce = new FRootMotionSource_MoveToDynamicForce();
			MoveToActorForce->InstanceName = ForceName;
			MoveToActorForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			MoveToActorForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
			MoveToActorForce->Priority = 900;
			MoveToActorForce->InitialTargetLocation = TargetLocation;
			MoveToActorForce->TargetLocation = TargetLocation;
			MoveToActorForce->StartLocation = StartLocation;
			MoveToActorForce->Duration = Duration;
			MoveToActorForce->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
			MoveToActorForce->PathOffsetCurve = PathOffsetCurve;
			MoveToActorForce->TimeMappingCurve = TimeMappingCurve;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(MoveToActorForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilityTask_ApplyRootMotionMoveToActorForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

FVector UAbilityTask_ApplyRootMotionMoveToActorForce::CalculateTargetOffset() const
{
	check(TargetActor != nullptr);

	const FVector TargetActorLocation = TargetActor->GetActorLocation();
	FVector CalculatedTargetLocation = TargetActorLocation;
	
	if (OffsetAlignment == ERootMotionMoveToActorTargetOffsetType::AlignFromTargetToSource)
	{
		if (MovementComponent)
		{
			FVector ToSource = MovementComponent->GetActorLocation() - TargetActorLocation;
			ToSource.Z = 0.f;
			CalculatedTargetLocation += ToSource.ToOrientationQuat().RotateVector(TargetLocationOffset);
		}

	}
	else if (OffsetAlignment == ERootMotionMoveToActorTargetOffsetType::AlignToTargetForward)
	{
		CalculatedTargetLocation += TargetActor->GetActorQuat().RotateVector(TargetLocationOffset);
	}
	else if (OffsetAlignment == ERootMotionMoveToActorTargetOffsetType::AlignToWorldSpace)
	{
		CalculatedTargetLocation += TargetLocationOffset;
	}
	
	return CalculatedTargetLocation;
}

bool UAbilityTask_ApplyRootMotionMoveToActorForce::UpdateTargetLocation(float DeltaTime)
{
	if (TargetActor && GetWorld())
	{
		const FVector PreviousTargetLocation = TargetLocation;
		FVector ExactTargetLocation = CalculateTargetOffset();

		const float CurrentTime = GetWorld()->GetTimeSeconds();
		const float CompletionPercent = (CurrentTime - StartTime) / Duration;

		const float TargetLerpSpeedHorizontal = TargetLerpSpeedHorizontalCurve ? TargetLerpSpeedHorizontalCurve->GetFloatValue(CompletionPercent) : 1000.f;
		const float TargetLerpSpeedVertical = TargetLerpSpeedVerticalCurve ? TargetLerpSpeedVerticalCurve->GetFloatValue(CompletionPercent) : 500.f;

		const float MaxHorizontalChange = FMath::Max(0.f, TargetLerpSpeedHorizontal * DeltaTime);
		const float MaxVerticalChange = FMath::Max(0.f, TargetLerpSpeedVertical * DeltaTime);

		FVector ToExactLocation = ExactTargetLocation - PreviousTargetLocation;
		FVector TargetLocationDelta = ToExactLocation;

		// Cap vertical lerp
		if (FMath::Abs(ToExactLocation.Z) > MaxVerticalChange)
		{
			if (ToExactLocation.Z >= 0.f)
			{
				TargetLocationDelta.Z = MaxVerticalChange;
			}
			else
			{
				TargetLocationDelta.Z = -MaxVerticalChange;
			}
		}

		// Cap horizontal lerp
		if (FMath::Abs(ToExactLocation.SizeSquared2D()) > MaxHorizontalChange*MaxHorizontalChange)
		{
			FVector ToExactLocationHorizontal(ToExactLocation.X, ToExactLocation.Y, 0.f);
			ToExactLocationHorizontal.Normalize();
			ToExactLocationHorizontal *= MaxHorizontalChange;

			TargetLocationDelta.X = ToExactLocationHorizontal.X;
			TargetLocationDelta.Y = ToExactLocationHorizontal.Y;
		}

		TargetLocation += TargetLocationDelta;

		return true;
	}

	return false;
}

void UAbilityTask_ApplyRootMotionMoveToActorForce::SetRootMotionTargetLocation(FVector NewTargetLocation)
{
	if (MovementComponent)
	{
		auto RMS = MovementComponent->GetRootMotionSourceByID(RootMotionSourceID);
		if (RMS.IsValid())
		{
			if (RMS->GetScriptStruct() == FRootMotionSource_MoveToDynamicForce::StaticStruct())
			{
				FRootMotionSource_MoveToDynamicForce* MoveToActorForce = static_cast<FRootMotionSource_MoveToDynamicForce*>(RMS.Get());
				if (MoveToActorForce)
				{
					MoveToActorForce->SetTargetLocation(TargetLocation);
				}
			}
		}
	}
}

void UAbilityTask_ApplyRootMotionMoveToActorForce::TickTask(float DeltaTime)
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
		const bool bTimedOut = CurrentTime >= EndTime;

		// Update target location
		{
			const FVector PreviousTargetLocation = TargetLocation;
			if (UpdateTargetLocation(DeltaTime))
			{
				SetRootMotionTargetLocation(TargetLocation);
			}
			else
			{
				// TargetLocation not updated - TargetActor not around anymore, continue on to last set TargetLocation
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Draw debug
		if (DebugMoveToActorForce > 0)
		{
			DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 10, FColor::Green, false, 15.f);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		const float ReachedDestinationDistanceSqr = 50.f * 50.f;
		const bool bReachedDestination = FVector::DistSquared(TargetLocation, MyActor->GetActorLocation()) < ReachedDestinationDistanceSqr;

		if (bTimedOut || (bReachedDestination && !bDisableDestinationReachedInterrupt))
		{
			// Task has finished
			bIsFinished = true;
			if (!bIsSimulating)
			{
				MyActor->ForceNetUpdate();
				OnFinished.Broadcast(bReachedDestination, bTimedOut, TargetLocation);
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

void UAbilityTask_ApplyRootMotionMoveToActorForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, ForceName);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, StartLocation);
	DOREPLIFETIME_CONDITION(UAbilityTask_ApplyRootMotionMoveToActorForce, TargetLocation, COND_SimulatedOnly); // Autonomous and server calculate target location independently
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, TargetActor);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, TargetLocationOffset);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, OffsetAlignment);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, Duration);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, bDisableDestinationReachedInterrupt);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, TargetLerpSpeedHorizontalCurve);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, TargetLerpSpeedVerticalCurve);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, bSetNewMovementMode);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, NewMovementMode);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, bRestrictSpeedToExpected);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, PathOffsetCurve);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, VelocityOnFinishMode);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToActorForce, SetVelocityOnFinish);
}

void UAbilityTask_ApplyRootMotionMoveToActorForce::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UAbilityTask_ApplyRootMotionMoveToActorForce::OnDestroy(bool AbilityIsEnding)
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);

		if (bSetNewMovementMode)
		{
			MovementComponent->SetMovementMode(NewMovementMode);
		}

		if (VelocityOnFinishMode == ERootMotionFinishVelocityMode::SetVelocity)
		{
			FVector EndVelocity;
			ACharacter* Character = MovementComponent->GetCharacterOwner();
			if (Character)
			{
				EndVelocity = Character->GetActorRotation().RotateVector(SetVelocityOnFinish);
			}
			else
			{
				EndVelocity = SetVelocityOnFinish;
			}

			// When we mean to SetVelocity when finishing a MoveTo, we apply a short-duration low-priority
			// root motion velocity override. This ensures that the velocity we set is replicated properly
			// and takes effect.
			{
				const FName OnFinishForceName = FName("AbilityTaskApplyRootMotionMoveToActorForce_EndForce");
				FRootMotionSource_ConstantForce* ConstantForce = new FRootMotionSource_ConstantForce();
				ConstantForce->InstanceName = OnFinishForceName;
				ConstantForce->AccumulateMode = ERootMotionAccumulateMode::Override;
				ConstantForce->Priority = 1; // Low priority so any other override root motion sources stomp it
				ConstantForce->Force = EndVelocity;
				ConstantForce->Duration = 0.001f;
				MovementComponent->ApplyRootMotionSource(ConstantForce);

				if (Ability)
				{
					Ability->SetMovementSyncPoint(OnFinishForceName);
				}
			}
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}
