// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTReachSpec.h"
#include "UTRecastNavMesh.h"
#include "UTLift.h"

#include "UTReachSpec_Lift.generated.h"

UCLASS(CustomConstructor)
class UUTReachSpec_Lift : public UUTReachSpec
{
	GENERATED_UCLASS_BODY()

	/** the lift the bot should ride*/
	UPROPERTY()
	TWeakObjectPtr<AUTLift> Lift;
	/** the lift exit corresponding to this path */
	UPROPERTY()
	FVector LiftExitLoc;
	/** the location where the bot should stand on the lift when it is in position to use this path */
	UPROPERTY()
	FVector LiftCenter;
	/** true if this is the path onto the lift, false if it's the path off the lift */
	UPROPERTY()
	bool bEntryPath;

	UUTReachSpec_Lift(const FPostConstructInitializeProperties& PCIP)
		: Super(PCIP)
	{
		PathColor = FLinearColor(0.0f, 0.0f, 1.0f);
	}

	virtual int32 CostFor(int32 DefaultCost, const FUTPathLink& OwnerLink, APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh) const override
	{
		// TODO: check if lift is usable?
		// TODO: check lift position instead of static cost at build time?
		return Lift.IsValid() ? DefaultCost : BLOCKED_PATH_COST;
	}

	virtual bool WaitForMove(APawn* Asker) const override
	{
		if (!Lift.IsValid())
		{
			// shouldn't happen, but just in case
			return false;
		}
		else
		{
			ACharacter* P = Cast<ACharacter>(Asker);
			if ( !bEntryPath && P != NULL && P->CharacterMovement != NULL && P->CharacterMovement->MovementMode == MOVE_Falling &&
				P->GetActorLocation().Z + 0.5f * P->CharacterMovement->Velocity.Z * FMath::Abs<float>(P->CharacterMovement->Velocity.Z / P->CharacterMovement->GetGravityZ()) > LiftExitLoc.Z )
			{
				// lift jumped and on the way to target
				return false;
			}
			else if (Asker->GetMovementBase() == Lift.Get()->GetEncroachComponent())
			{
				return (Asker->GetActorLocation() - LiftCenter).Size2D() < Asker->GetSimpleCollisionRadius() && FMath::Abs<float>(Asker->GetActorLocation().Z - LiftCenter.Z) > Asker->GetSimpleCollisionHalfHeight();
			}
			else
			{
				FHitResult Hit;
				FVector LiftLoc = Lift.Get()->GetComponentsBoundingBox().GetCenter();
				return ( !Lift.Get()->ActorLineTraceSingle(Hit, LiftLoc + FVector(0.0f, 0.0f, 10000.0f), LiftLoc - FVector(0.0f, 0.0f, 10000.0f), ECC_Pawn, FCollisionQueryParams()) ||
						Hit.Location.Z - LiftCenter.Z > Asker->GetSimpleCollisionHalfHeight() || (Hit.Location - LiftCenter).Size2D() > 10.0f );
			}
		}
	}

	virtual bool GetMovePoints(const FUTPathLink& OwnerLink, const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const
	{
		if (bEntryPath)
		{
			TArray<NavNodeRef> PolyRoute;
			NavMesh->FindPolyPath(StartLoc, AgentProps, FRouteCacheItem(NavMesh->GetPolyCenter(OwnerLink.StartEdgePoly), OwnerLink.StartEdgePoly), PolyRoute, false);
			NavMesh->DoStringPulling(StartLoc, PolyRoute, AgentProps, MovePoints);
			MovePoints.Add(FComponentBasedPosition(Lift.IsValid() ? Lift->GetEncroachComponent() : NULL, LiftCenter));
		}
		else if (Asker != NULL && Lift.IsValid() && Asker->GetMovementBase() != Lift.Get()->GetEncroachComponent())
		{
			// this can happen currently because the navmesh doesn't support moving objects and will generate polys for the lift in its starting position
			// that connect to the standard walkable surface
			// try to figure out how to get the AI to the lift anyway
			FVector LiftLoc = Lift.Get()->GetComponentsBoundingBox().GetCenter();
			FHitResult Hit;
			if (Lift.Get()->ActorLineTraceSingle(Hit, LiftLoc + FVector(0.0f, 0.0f, 10000.0f), LiftLoc - FVector(0.0f, 0.0f, 10000.0f), ECC_Pawn, FCollisionQueryParams()))
			{
				LiftLoc = Hit.Location - NavMesh->AgentHeight * 0.5f;
			}
			NavNodeRef LiftPoly = NavMesh->FindNearestPoly(LiftLoc, FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f + Lift->GetComponentsBoundingBox().GetExtent().Z)); // extra height because lift is partially in the way
			if (LiftPoly != INVALID_NAVNODEREF)
			{
				TArray<NavNodeRef> PolyRoute;
				NavMesh->FindPolyPath(StartLoc, AgentProps, FRouteCacheItem(LiftLoc, LiftPoly), PolyRoute, false);
				NavMesh->DoStringPulling(StartLoc, PolyRoute, AgentProps, MovePoints);
			}
			MovePoints.Add(FComponentBasedPosition(Lift->GetEncroachComponent(), LiftLoc));
			MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
		}
		else
		{
			MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
		}
		return true;
	}
};