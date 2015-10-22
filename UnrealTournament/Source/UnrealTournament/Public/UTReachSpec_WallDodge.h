// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTReachSpec.h"

#include "UTReachSpec_WallDodge.generated.h"

UCLASS()
class UUTReachSpec_WallDodge : public UUTReachSpec
{
	GENERATED_BODY()
public:
	
	UUTReachSpec_WallDodge(const FObjectInitializer& OI)
		: Super(OI)
	{
		PathColor = FLinearColor(1.0f, 1.0f, 0.0f);
	}

	/** point on jump source poly that we should start at to move towards wall (generally a specific edge of the poly to get around outcroppings and such) */
	UPROPERTY()
	FVector JumpStart;
	/** target point on the surface of the wall */
	UPROPERTY()
	FVector WallPoint;
	/** wall normal/dodge direction */
	UPROPERTY()
	FVector WallNormal;

	virtual int32 CostFor(int32 DefaultCost, const FUTPathLink& OwnerLink, APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh) override
	{
		// low skill bots avoid wall dodge unless required to get to an area at all
		if (Asker != NULL)
		{
			AUTBot* B = Cast<AUTBot>(Asker->Controller);
			if (B != NULL && B->Skill + B->Personality.MovementAbility < 2.0f)
			{
				DefaultCost += 100000;
			}
		}
		// TODO: maybe some bots prioritize instead? possibly should consider actual difficulty of the jump and dodge instead of a flat number...
		return DefaultCost + 500; // extra for potential risk of fall
	}

	virtual bool WaitForMove(const FUTPathLink& OwnerLink, APawn* Asker, const FComponentBasedPosition& MovePos, const FRouteCacheItem& Target) const override
	{
		// check for wall dodge where the wall is actually on walkable space so we need to jump before going off any ledge
		AUTCharacter* UTC = Cast<AUTCharacter>(Asker);
		if ( UTC != NULL && UTC->UTCharacterMovement->MovementMode == MOVE_Walking &&
			 ( (MovePos.Get() - Target.GetLocation(Asker)).IsNearlyZero() && (UTC->GetActorLocation() - WallPoint).Size2D() < UTC->GetSimpleCollisionRadius() * 1.5f ||
			 (MovePos.Get() - WallPoint).Size2D() < UTC->GetSimpleCollisionRadius() * 1.5f ) )
		{
			UTC->UTCharacterMovement->Velocity = (WallPoint - UTC->GetActorLocation()).GetSafeNormal2D() * UTC->UTCharacterMovement->MaxWalkSpeed;
			UTC->UTCharacterMovement->DoJump(false);
		}
		return false;
	}

	virtual bool OverrideAirControl(const FUTPathLink& OwnerLink, APawn* Asker, const FComponentBasedPosition& MovePos, const FRouteCacheItem& Target) const override
	{
		AUTCharacter* UTC = Cast<AUTCharacter>(Asker);
		if (UTC != NULL && (MovePos.Get() - Target.GetLocation(Asker)).IsNearlyZero())
		{
			if (UTC->UTCharacterMovement->Velocity.Size2D() < UTC->UTCharacterMovement->MaxWalkSpeed + 1.0f && (UTC->GetActorLocation() - WallPoint).Size2D() < UTC->GetSimpleCollisionRadius() * 1.5f)
			{
				// check for the wall dodge
				// ideally wall dodge completely in desired movement direction, but try increments closer to wall 
				FVector DesiredDir = (MovePos.Get() - UTC->GetActorLocation()).GetSafeNormal();
				if (UTC->Dodge(DesiredDir, (DesiredDir ^ FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal()))
				{
					UTC->UTCharacterMovement->UpdateWallSlide(false);
					return false;
				}
				else
				{
					DesiredDir = (DesiredDir + WallNormal).GetSafeNormal();
					if (UTC->Dodge(DesiredDir, (DesiredDir ^ FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal()) || UTC->Dodge(WallNormal, (WallNormal ^ FVector(0.0f, 0.0f, 1.0f)).GetSafeNormal()))
					{
						UTC->UTCharacterMovement->UpdateWallSlide(false);
						return false;
					}
					else
					{
						// air control into wall
						FVector AirControlDir = (WallPoint - UTC->GetActorLocation()).GetSafeNormal2D();
						if (UTC->UTCharacterMovement->bIsAgainstWall)
						{
							// already against wall so slide along it
							AirControlDir -= FMath::Max<float>(0.f, (AirControlDir | UTC->UTCharacterMovement->WallSlideNormal)) * UTC->UTCharacterMovement->WallSlideNormal;
						}
						UTC->UTCharacterMovement->AddInputVector(AirControlDir);
						UTC->UTCharacterMovement->UpdateWallSlide(true);
						return true;
					}
				}
			}
			else
			{
				UTC->UTCharacterMovement->UpdateWallSlide(true);
				return false;
			}
		}
		else
		{
			UTC->UTCharacterMovement->UpdateWallSlide(true);
			return false;
		}
	}

	virtual bool GetMovePoints(const FUTPathLink& OwnerLink, const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const override
	{
		TArray<NavNodeRef> PolyRoute;
		if (NavMesh->FindPolyPath(StartLoc, AgentProps, FRouteCacheItem(NavMesh->GetPolySurfaceCenter(OwnerLink.StartEdgePoly), OwnerLink.StartEdgePoly), PolyRoute, false) && PolyRoute.Num() > 0 && NavMesh->DoStringPulling(StartLoc, PolyRoute, AgentProps, MovePoints))
		{
			MovePoints.Add(FComponentBasedPosition(JumpStart));
			AUTCharacter* UTC = Cast<AUTCharacter>(Asker);
			MovePoints.Add(FComponentBasedPosition(WallPoint + WallNormal * ((UTC != NULL) ? (UTC->UTCharacterMovement->WallDodgeTraceDist * 0.5f) : (Asker->GetSimpleCollisionRadius() + 1.0f))));
			MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
			return true;
		}
		else
		{
			return false;
		}
	}
};