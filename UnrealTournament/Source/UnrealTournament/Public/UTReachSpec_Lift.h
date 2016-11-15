// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTReachSpec.h"
#include "UTRecastNavMesh.h"
#include "UTLift.h"

#include "UTReachSpec_Lift.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTReachSpec_Lift : public UUTReachSpec
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
	/** lift jumps only - true if bot should avoid air control initially during jump (e.g. elevator shaft or similar where early air control can cause bot to bump head on an earlier floor than intended) */
	UPROPERTY()
	bool bSkipInitialAirControl;

	UUTReachSpec_Lift(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		PathColor = FLinearColor(0.0f, 0.0f, 1.0f);
	}

	virtual int32 CostFor(int32 DefaultCost, const FUTPathLink& OwnerLink, APawn* Asker, const FNavAgentProperties& AgentProps, const FUTReachParams& ReachParams, AController* RequestOwner, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh) override
	{
		// low skill bots avoid lift jumps unless required to get to an area at all
		if (OwnerLink.ReachFlags & R_JUMP)
		{
			AUTBot* B = Cast<AUTBot>(RequestOwner);
			if (B != NULL && B->Skill + B->Personality.MovementAbility < 2.0f)
			{
				DefaultCost += 100000;
			}
		}
		// TODO: check if lift is usable?
		// TODO: check lift position instead of static cost at build time?
		return Lift.IsValid() ? DefaultCost : BLOCKED_PATH_COST;
	}

	virtual bool WaitForMove(const FUTPathLink& OwnerLink, APawn* Asker, const FComponentBasedPosition& MovePos, const FRouteCacheItem& Target) const override
	{
		if (!Lift.IsValid())
		{
			// shouldn't happen, but just in case
			return false;
		}
		else
		{
			ACharacter* P = Cast<ACharacter>(Asker);
			if (!bEntryPath && P != NULL && P->GetCharacterMovement() != NULL && P->GetCharacterMovement()->MovementMode == MOVE_Falling &&
				P->GetActorLocation().Z + 0.5f * P->GetCharacterMovement()->Velocity.Z * FMath::Abs<float>(P->GetCharacterMovement()->Velocity.Z / P->GetCharacterMovement()->GetGravityZ()) > LiftExitLoc.Z)
			{
				// lift jumped and on the way to target
				return false;
			}
			else if (Asker->GetMovementBase() == Lift.Get()->GetEncroachComponent())
			{
				// make sure AI moves to center of lift
				if (MovePos.Base == Asker->GetMovementBase())
				{
					return false;
				}
				else
				{
					return (Asker->GetActorLocation() - LiftCenter).Size2D() < Lift.Get()->GetSimpleCollisionRadius() && FMath::Abs<float>(Asker->GetActorLocation().Z - LiftExitLoc.Z) > Asker->GetSimpleCollisionHalfHeight() * 1.1f;
				}
			}
			// check if we got off the lift successfully and can now finish the move
			else if (Asker->GetActorLocation().Z + Asker->GetSimpleCollisionHalfHeight() * 1.1f > LiftExitLoc.Z && !GetUTNavData(Asker->GetWorld())->RaycastWithZCheck(Asker->GetNavAgentLocation(), LiftExitLoc))
			{
				return false;
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

	virtual bool AllowWalkOffLedges(const FUTPathLink& OwnerLink, APawn* Asker, const FComponentBasedPosition& MovePos) const
	{
		// allow jumping onto lift center regardless of path
		// also handle case where pathing went directly from exit to exit (bug due to navmesh not working correctly for moving platforms)
		return (MovePos.Base == Lift.Get()->GetEncroachComponent() || (!bEntryPath && LiftCenter.Z > LiftExitLoc.Z + Asker->GetSimpleCollisionHalfHeight()));
	}

	virtual bool OverrideAirControl(const FUTPathLink& OwnerLink, APawn* Asker, const FComponentBasedPosition& MovePos, const FRouteCacheItem& Target) const
	{
		return (bSkipInitialAirControl && Asker->GetActorLocation().Z < LiftExitLoc.Z - Asker->GetSimpleCollisionHalfHeight() * 3.0f && (Asker->GetVelocity().GetSafeNormal2D() | (LiftExitLoc - Asker->GetActorLocation()).GetSafeNormal2D()) >= 0.0f);
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
				LiftLoc = Hit.Location;
			}
			if (AgentProps.bCanJump)
			{
				// we can just jump down to the lift
				TArray<NavNodeRef> PolyRoute;
				NavMesh->FindPolyPath(StartLoc, AgentProps, FRouteCacheItem(NavMesh->GetPolyCenter(OwnerLink.StartEdgePoly), OwnerLink.StartEdgePoly), PolyRoute, false);
				NavMesh->DoStringPulling(StartLoc, PolyRoute, AgentProps, MovePoints);
			}
			else
			{
				NavNodeRef LiftPoly = NavMesh->UTFindNearestPoly(LiftLoc, FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f + Lift->GetComponentsBoundingBox().GetExtent().Z)); // extra height because lift is partially in the way
				if (LiftPoly != INVALID_NAVNODEREF)
				{
					TArray<NavNodeRef> PolyRoute;
					NavMesh->FindPolyPath(StartLoc, AgentProps, FRouteCacheItem(LiftLoc, LiftPoly), PolyRoute, false);
					NavMesh->DoStringPulling(StartLoc, PolyRoute, AgentProps, MovePoints);
				}
			}
			MovePoints.Add(FComponentBasedPosition(Lift->GetEncroachComponent(), LiftLoc + FVector(0.0f, 0.0f, AgentProps.AgentHeight * 0.5f)));
			if (OwnerLink.ReachFlags & R_JUMP)
			{
				MovePoints.Add(FComponentBasedPosition(LiftExitLoc));
			}
			MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
		}
		else
		{
			// for lift jumps add exit loc so LD can precisely tune jump characteristics
			if (OwnerLink.ReachFlags & R_JUMP)
			{
				MovePoints.Add(FComponentBasedPosition(LiftExitLoc));
			}
			MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
		}
		return true;
	}
};