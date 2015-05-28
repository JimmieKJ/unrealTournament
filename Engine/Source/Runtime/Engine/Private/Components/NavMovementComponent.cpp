// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "GameFramework/NavMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"

//----------------------------------------------------------------------//
// FNavAgentProperties
//----------------------------------------------------------------------//
const FNavAgentProperties FNavAgentProperties::DefaultProperties;

void FNavAgentProperties::UpdateWithCollisionComponent(UShapeComponent* CollisionComponent)
{
	check( CollisionComponent != NULL);
	AgentRadius = CollisionComponent->Bounds.SphereRadius;
}

//----------------------------------------------------------------------//
// UMovementComponent
//----------------------------------------------------------------------//
UNavMovementComponent::UNavMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUpdateNavAgentWithOwnersCollision(true)
	, bStopMovementAbortPaths(true)
{
}

FBasedPosition UNavMovementComponent::GetActorFeetLocationBased() const
{
	return FBasedPosition(NULL, GetActorFeetLocation());
}

void UNavMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	Velocity = MoveVelocity;
}

bool UNavMovementComponent::CanStopPathFollowing() const
{
	return true;
}

void UNavMovementComponent::StopActiveMovement()
{
	if (PathFollowingComp.IsValid() && bStopMovementAbortPaths)
	{
		PathFollowingComp->AbortMove("StopActiveMovement");
	}
}

void UNavMovementComponent::UpdateNavAgent(const AActor& Owner)
{
	ensure(&Owner == GetOwner());
	if (ShouldUpdateNavAgentWithOwnersCollision() == false)
	{
		return;
	}

	// initialize properties from navigation system
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys != nullptr)
	{
		NavAgentProps.NavWalkingSearchHeightScale = NavSys->GetDefaultSupportedAgentConfig().NavWalkingSearchHeightScale;
	}

	// Can't call GetSimpleCollisionCylinder(), because no components will be registered.
	float BoundRadius, BoundHalfHeight;	
	Owner.GetSimpleCollisionCylinder(BoundRadius, BoundHalfHeight);
	NavAgentProps.AgentRadius = BoundRadius;
	NavAgentProps.AgentHeight = BoundHalfHeight * 2.f;
}

void UNavMovementComponent::UpdateNavAgent(const UCapsuleComponent& CapsuleComponent)
{
	if (ShouldUpdateNavAgentWithOwnersCollision() == false)
	{
		return;
	}

	// initialize properties from navigation system
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys != nullptr)
	{
		NavAgentProps.NavWalkingSearchHeightScale = NavSys->GetDefaultSupportedAgentConfig().NavWalkingSearchHeightScale;
	}

	NavAgentProps.AgentRadius = CapsuleComponent.GetScaledCapsuleRadius();
	NavAgentProps.AgentHeight = CapsuleComponent.GetScaledCapsuleHalfHeight() * 2.f;
}

void UNavMovementComponent::SetUpdateNavAgentWithOwnersCollisions(bool bUpdateWithOwner)
{
	bUpdateNavAgentWithOwnersCollision = bUpdateWithOwner;
}

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//
const FNavAgentProperties* UNavMovementComponent::GetNavAgentProperties() const
{
	return &GetNavAgentPropertiesRef();
}
FNavAgentProperties* UNavMovementComponent::GetNavAgentProperties()
{
	return &GetNavAgentPropertiesRef();
}
void UNavMovementComponent::UpdateNavAgent(AActor* Owner)
{
	if (Owner)
	{
		UpdateNavAgent(*Owner);
	}
}
void UNavMovementComponent::UpdateNavAgent(UCapsuleComponent* CapsuleComponent)
{
	if (CapsuleComponent)
	{
		UpdateNavAgent(*CapsuleComponent);
	}
}