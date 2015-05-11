// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CustomBotPrivatePCH.h"
#include "UTCustomPathFollowingComponent.h"

//UUTPathFollowingComponent::UUTPathFollowingComponent(const FObjectInitializer& ObjectInitializer)
//	: Super(ObjectInitializer)
//{
//
//}

void UUTCustomPathFollowingComponent::FollowPathSegment(float DeltaTime)
{
	Super::FollowPathSegment(DeltaTime);

	if (CharacterMovementComp)
	{
		// this is needed as a hack to have movement component set some acceleration
		// otherwise the pawn won't animate properly. 
		// a proper fix has already been done on UE4's master branch
		//CharacterMovementComp->Acceleration =
		const FVector CurrentLocation = MovementComp->GetActorFeetLocation();
		const FVector CurrentTarget = GetCurrentTargetLocation();
		const FVector MoveVelocity = (CurrentTarget - CurrentLocation) / DeltaTime;
		CharacterMovementComp->AddInputVector(MoveVelocity);
	}
}

void UUTCustomPathFollowingComponent::SetMovementComponent(UNavMovementComponent* MoveComp)
{
	Super::SetMovementComponent(MoveComp);
	CharacterMovementComp = Cast<UCharacterMovementComponent>(MovementComp);
}