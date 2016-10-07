// path that requires a higher than normal jump capability
// may also be traversed by advanced weapon use, e.g. impact jump, translocator
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTReachSpec.h"
#include "UTRecastNavMesh.h"
#include "UTLift.h"
#include "UTCharacterMovement.h"

#include "UTReachSpec_HighJump.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTReachSpec_HighJump : public UUTReachSpec
{
	GENERATED_UCLASS_BODY()

	UUTReachSpec_HighJump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		PathColor = FLinearColor(0.5f, 0.0f, 0.8f);
		DodgeJumpZMult = 1.0f;
	}

	/** jump Z velocity required to reach the endpoint */
	UPROPERTY()
	float RequiredJumpZ;
	/** if capable of dodge jump, can get easier jump of RequiredJumpZ * DodgeJumpZMult */
	UPROPERTY()
	float DodgeJumpZMult;
	/** center of jump source poly */
	UPROPERTY()
	FVector JumpStart;
	/** center of jump target poly */
	UPROPERTY()
	FVector JumpEnd;
	/** gravity at the time the path was generated (so we can adjust for lowgrav) */
	UPROPERTY()
	float OriginalGravityZ;
	/** physics volume that contains the current gravity (if NULL, world gravity) */
	UPROPERTY()
	TWeakObjectPtr<APhysicsVolume> GravityVolume;
	/** if true, AI should jump from poly center of the edge poly (StartEdgePoly of the owning FUTPathLink) instead of moving to the very edge of the walkable surface
	 * (generally used for jumps from open space up to a ledge or outcropping)
	 */
	UPROPERTY()
	bool bJumpFromEdgePolyCenter;

	/** cached result for translocator toss check */
	UPROPERTY(transient)
	TWeakObjectPtr<const AUTProjectile> CachedTransDiscTemplate;
	UPROPERTY(transient)
	bool bCachedTranslocatorResult;

	virtual void PostLoad() override
	{
		Super::PostLoad();

		// backwards compatibility
		if (JumpStart == FVector::ZeroVector)
		{
			bool bFound = false;
			AUTRecastNavMesh* NavData = GetTypedOuter<AUTRecastNavMesh>();
			for (const UUTPathNode* Node : NavData->GetAllNodes())
			{
				for (const FUTPathLink& Link : Node->Paths)
				{
					if (Link.Spec == this)
					{
						JumpStart = NavData->GetPolyCenter(Link.StartEdgePoly);
						JumpEnd = NavData->GetPolyCenter(Link.EndPoly);
						bFound = true;
						break;
					}
				}
				if (bFound)
				{
					break;
				}
			}
		}
	}

	float CalcRequiredJumpZ(APawn* Asker) const
	{
		float AdjustedRequiredJumpZ = RequiredJumpZ;
		const float GravityZ = (GravityVolume != NULL) ? GravityVolume->GetGravityZ() : Asker->GetWorld()->GetGravityZ();
		if (GravityZ != OriginalGravityZ)
		{
			AdjustedRequiredJumpZ -= (0.6f * RequiredJumpZ * (1.0f - GravityZ / OriginalGravityZ));
		}
		return AdjustedRequiredJumpZ;
	}

	/** returns if specified projectile can reach from JumpStart to JumpEnd */
	bool CheckTranslocatorArc(const AUTProjectile* ProjTemplate, float GravityZ)
	{
		if (ProjTemplate == NULL || ProjTemplate->ProjectileMovement == NULL)
		{
			return true;
		}
		else if (ProjTemplate == CachedTransDiscTemplate)
		{
			return bCachedTranslocatorResult;
		}
		else
		{
			GravityZ *= ProjTemplate->ProjectileMovement->ProjectileGravityScale;
			if (GravityZ >= 0.0f)
			{
				return true;
			}
			else
			{
				float TossSpeed = ProjTemplate->ProjectileMovement->InitialSpeed;
				// if firing upward, add minimum possible TossZ contribution to effective speed to improve toss prediction
				if (ProjTemplate->TossZ > 0.0f)
				{
					TossSpeed += FMath::Max<float>(0.0f, (JumpEnd - JumpStart).GetSafeNormal().Z * ProjTemplate->TossZ);
				}
				GravityZ = -GravityZ;
				const FVector FlightDelta = JumpEnd - JumpStart;
				const float DeltaXY = FlightDelta.Size2D();
				const float DeltaZ = FlightDelta.Z;
				const float TossSpeedSq = FMath::Square(TossSpeed);

				// v^4 - g*(g*x^2 + 2*y*v^2)
				bCachedTranslocatorResult = (FMath::Square(TossSpeedSq) - GravityZ * ((GravityZ * FMath::Square(DeltaXY)) + (2.f * DeltaZ * TossSpeedSq)) >= 0.0f);
				CachedTransDiscTemplate = ProjTemplate;
				return bCachedTranslocatorResult;
			}
		}
	}

	virtual int32 CostFor(int32 DefaultCost, const FUTPathLink& OwnerLink, APawn* Asker, const FNavAgentProperties& AgentProps, const FUTReachParams& ReachParams, AController* RequestOwner, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh) override
	{
		if (Asker == NULL)
		{
			return BLOCKED_PATH_COST;
		}
		else
		{
			float AdjustedRequiredJumpZ = CalcRequiredJumpZ(Asker);
			if (ReachParams.MaxSimpleJumpZ >= AdjustedRequiredJumpZ)
			{
				// extra cost if limited availability movement required
				return DefaultCost + ((ReachParams.MaxSimpleRepeatableJumpZ >= AdjustedRequiredJumpZ) ? 0 : 2000);
			}
			// don't try to translocate or impact jump out of a water volume
			else if (GravityVolume.IsValid() && GravityVolume->bWaterVolume)
			{
				return BLOCKED_PATH_COST;
			}
			else
			{
				AUTBot* B = Cast<AUTBot>(RequestOwner);
				if (B == NULL)
				{
					return BLOCKED_PATH_COST;
				}
				else
				{
					int32 JumpDist = FMath::TruncToInt((JumpEnd - JumpStart).Size());
					if (B->AllowTranslocator() && CheckTranslocatorArc(B->TransDiscTemplate, (GravityVolume != NULL) ? GravityVolume->GetGravityZ() : Asker->GetWorld()->GetGravityZ()))
					{
						// the higher we need to throw the disc for a lower Z change, the more time the throw will take; adjust distance for that
						return FMath::Max<int32>(450, FMath::TruncToInt((AdjustedRequiredJumpZ - (JumpEnd.Z - JumpStart.Z)) / 1.5f)) + (DefaultCost - JumpDist) + (JumpDist / 2);
					}
					else if (B->AllowImpactJump())
					{
						AUTCharacter* UTChar = Cast<AUTCharacter>(Asker);
						if (UTChar != NULL && UTChar->UTCharacterMovement->DodgeImpulseHorizontal > UTChar->UTCharacterMovement->MaxWalkSpeed)
						{
							AdjustedRequiredJumpZ *= DodgeJumpZMult;
						}
						const float SpecialJumpZ = ReachParams.MaxSimpleJumpZ - ReachParams.MaxSimpleRepeatableJumpZ;
						const float BaseImpactJumpZ = ReachParams.MaxSimpleRepeatableJumpZ + B->ImpactJumpZ;
						const float BestJumpZ = BaseImpactJumpZ * (BaseImpactJumpZ / (BaseImpactJumpZ + SpecialJumpZ)) + SpecialJumpZ;
						if (BestJumpZ >= AdjustedRequiredJumpZ)
						{
							return DefaultCost + 5000; // TODO: reduce cost if in a rush or have high health?
						}
						else
						{
							return BLOCKED_PATH_COST;
						}
					}
					else
					{
						return BLOCKED_PATH_COST;
					}
				}
			}
		}
	}

	virtual bool WaitForMove(const FUTPathLink& OwnerLink, APawn* Asker, const FComponentBasedPosition& MovePos, const FRouteCacheItem& Target) const override
	{
		if (Asker == NULL)
		{
			return false;
		}
		else
		{
			float AdjustedRequiredJumpZ = CalcRequiredJumpZ(Asker);
			float BestJumpZ = FUTReachParams::CalcAvailableSimpleJumpZ(Asker);
			if (BestJumpZ >= AdjustedRequiredJumpZ)
			{
				if ((MovePos.Get() - Target.GetLocation(Asker)).IsNearlyZero() && (Asker->GetVelocity().IsZero() || (Asker->GetVelocity().GetSafeNormal2D() | (MovePos.Get() - Asker->GetActorLocation()).GetSafeNormal2D()) > 0.9f))
				{
					bool bJumpBeforeEdge = bJumpFromEdgePolyCenter;
					if (!bJumpBeforeEdge && Asker->GetActorLocation().Z < MovePos.Get().Z)
					{
						// check for bot ending up on a further poly when expecting a ledge or wall; this can happen when it's a small obstruction and the string pulling got around it
						AUTRecastNavMesh* NavData = GetUTNavData(Asker->GetWorld());
						if (NavData->RaycastWithZCheck(Asker->GetNavAgentLocation(), Target.GetLocation(NULL)))
						{
							NavNodeRef CurrentPoly = NavData->UTFindNearestPoly(Asker->GetNavAgentLocation(), Asker->GetSimpleCollisionCylinderExtent());
							if (CurrentPoly != OwnerLink.StartEdgePoly && (MovePos.Get() - NavData->GetPolyCenter(CurrentPoly)).Size() < (MovePos.Get() - JumpStart).Size() * 0.9f)
							{
								bJumpBeforeEdge = true;
							}
						}
					}
					if (bJumpBeforeEdge)
					{
						ACharacter* C = Cast<ACharacter>(Asker);
						if (C != NULL && C->GetCharacterMovement() != NULL)
						{
							C->GetCharacterMovement()->DoJump(false);
						}
					}
				}
				return false; // use standard jump moves
			}
			else
			{
				AUTBot* B = Cast<AUTBot>(Asker->Controller);
				if (B == NULL)
				{
					return false;
				}
				else
				{
					// aren't forced to wait until final stretch
					if (B->GetMovePoint() != B->GetMoveTarget().GetLocation(Asker))
					{
						return false;
					}
					else if (B->GetUTChar() == NULL)
					{
						UE_LOG(UT, Warning, TEXT("Bot %s in Pawn %s attempting to use high jump path!"), *B->PlayerState->PlayerName, *GetNameSafe(B->GetPawn()));
						B->MoveTimer = -1.0f;
						return false;
					}
					else if (B->GetUTChar()->UTCharacterMovement->MovementMode == MOVE_Falling)
					{
						return false;
					}
					else if (B->GetUTChar()->GetWeapon() == NULL || !B->GetUTChar()->GetWeapon()->DoAssistedJump())
					{
						if (B->GetUTChar()->GetPendingWeapon() == NULL)
						{
							B->SwitchToBestWeapon();
						}
						// wait for weapon to be ready
						return true;
					}
					else
					{
						// special action has happened and we should be good to go
						B->ClearFocus(SCRIPTEDMOVE_FOCUS_PRIORITY);
						return false;
					}
				}
			}
		}
	}

	virtual bool OverrideAirControl(const FUTPathLink& OwnerLink, APawn* Asker, const FComponentBasedPosition& MovePos, const FRouteCacheItem& Target) const
	{
		// make sure AI won't clip level geometry on the way up, if so back up via air control
		float Dist = (MovePos.Get() - Asker->GetActorLocation()).Size();
		FHitResult Hit;
		if (Asker->GetWorld()->LineTraceSingleByChannel(Hit, Asker->GetActorLocation(), Asker->GetActorLocation() + Asker->GetVelocity().GetSafeNormal() * Dist, ECC_Pawn, FCollisionQueryParams::DefaultQueryParam, WorldResponseParams))
		{
			Asker->GetMovementComponent()->AddInputVector((Asker->GetActorLocation() - MovePos.Get()).GetSafeNormal2D());
			return true;
		}
		else
		{
			return false;
		}
	}

	virtual bool GetMovePoints(const FUTPathLink& OwnerLink, const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const override
	{
		// start bot considering to switch to needed traversal weapon, if applicable
		if (FUTReachParams::CalcAvailableSimpleJumpZ(Asker) < CalcRequiredJumpZ(Asker))
		{
			AUTBot* B = Cast<AUTBot>(Asker->Controller);
			if (B != NULL)
			{
				B->SwitchToBestWeapon();
			}
		}
		if (bJumpFromEdgePolyCenter)
		{
			TArray<NavNodeRef> PolyRoute;
			if (NavMesh->FindPolyPath(StartLoc, AgentProps, FRouteCacheItem(NavMesh->GetPolySurfaceCenter(OwnerLink.StartEdgePoly), OwnerLink.StartEdgePoly), PolyRoute, false) && PolyRoute.Num() > 0 && NavMesh->DoStringPulling(StartLoc, PolyRoute, AgentProps, MovePoints))
			{
				MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return OwnerLink.GetJumpMovePoints(StartLoc, Asker, AgentProps, Target, FullRoute, NavMesh, MovePoints);
		}
	}
};
