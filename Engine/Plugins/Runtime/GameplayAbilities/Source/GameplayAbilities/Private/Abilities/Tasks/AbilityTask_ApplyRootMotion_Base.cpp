// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_ApplyRootMotion_Base.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

UAbilityTask_ApplyRootMotion_Base::UAbilityTask_ApplyRootMotion_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
	bSimulatedTask = true;

	ForceName = NAME_None;
	MovementComponent = nullptr;
	RootMotionSourceID = (uint16)ERootMotionSourceID::Invalid;
	bIsFinished = false;
	StartTime = 0.0f;
	EndTime = 0.0f;
}

void UAbilityTask_ApplyRootMotion_Base::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME(UAbilityTask_ApplyRootMotion_Base, ForceName);
}

void UAbilityTask_ApplyRootMotion_Base::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	Super::InitSimulatedTask(InGameplayTasksComponent);

	SharedInitAndApply();
}

void UAbilityTask_ApplyRootMotion_Base::SetFinishVelocity(FName RootMotionSourceName, FVector FinishVelocity)
{
	check(MovementComponent);

	const ACharacter* Character = MovementComponent->GetCharacterOwner();
	const FVector EndVelocity = (Character) ? Character->GetActorRotation().RotateVector(FinishVelocity) : FinishVelocity;

	// When we mean to SetVelocity when finishing a root motion move, we apply a short-duration low-priority
	// root motion velocity override. This ensures that the velocity we set is replicated properly
	// and takes effect.
	FRootMotionSource_ConstantForce* ConstantForce = new FRootMotionSource_ConstantForce();
	ConstantForce->InstanceName = RootMotionSourceName;
	ConstantForce->AccumulateMode = ERootMotionAccumulateMode::Override;
	ConstantForce->Priority = 1; // Low priority so any other override root motion sources stomp it
	ConstantForce->Force = EndVelocity;
	ConstantForce->Duration = 0.001f;
	ConstantForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);

	MovementComponent->ApplyRootMotionSource(ConstantForce);

	if (Ability)
	{
		Ability->SetMovementSyncPoint(RootMotionSourceName);
	}
}

void UAbilityTask_ApplyRootMotion_Base::ClampFinishVelocity(FName RootMotionSourceName, float VelocityClamp)
{
	check(MovementComponent);

	const FVector EndVelocity = MovementComponent->Velocity.GetClampedToMaxSize(VelocityClamp);

	FRootMotionSource_ConstantForce* ConstantForce = new FRootMotionSource_ConstantForce();
	ConstantForce->InstanceName = RootMotionSourceName;
	ConstantForce->AccumulateMode = ERootMotionAccumulateMode::Override;
	ConstantForce->Priority = 1; // Low priority so any other override root motion sources stomp it
	ConstantForce->Force = EndVelocity;
	ConstantForce->Duration = 0.001f;
	ConstantForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);

	MovementComponent->ApplyRootMotionSource(ConstantForce);

	if (Ability)
	{
		Ability->SetMovementSyncPoint(RootMotionSourceName);
	}
}
