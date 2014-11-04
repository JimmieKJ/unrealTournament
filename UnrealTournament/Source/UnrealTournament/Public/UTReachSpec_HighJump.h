// path that requires a higher than normal jump capability
// may also be traversed by advanced weapon use, e.g. impact jump, translocator
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTReachSpec.h"
#include "UTRecastNavMesh.h"
#include "UTLift.h"
#include "UTCharacterMovement.h"

#include "UTReachSpec_HighJump.generated.h"

UCLASS(CustomConstructor)
class UUTReachSpec_HighJump : public UUTReachSpec
{
	GENERATED_UCLASS_BODY()

	UUTReachSpec_HighJump(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	/** jump Z velocity required to reach the endpoint */
	UPROPERTY()
	float RequiredJumpZ;
	/** gravity at the time the path was generated (so we can adjust for lowgrav) */
	UPROPERTY()
	float OriginalGravityZ;
	/** physics volume that contains the current gravity (if NULL, world gravity) */
	UPROPERTY()
	TWeakObjectPtr<APhysicsVolume> GravityVolume;

	/** calculates JumpZ available to Asker through standard jump moves (no weapon assist)
	 * @param RepeatableJumpZ - JumpZ that the character can achieve limitlessly
	 * @param BestJumpZ - JumpZ that the character can achieve one or more times (jump boots, etc)
	 */
	float CalcAvailableSimpleJumpZ(APawn* Asker, float* RepeatableJumpZ = NULL) const
	{
		AUTCharacter* UTC = Cast<AUTCharacter>(Asker);
		if (UTC != NULL)
		{
			// Repeatable: what we can do by default
			if (RepeatableJumpZ != NULL)
			{
				*RepeatableJumpZ = UTC->GetClass()->GetDefaultObject<AUTCharacter>()->CharacterMovement->JumpZVelocity;
				if (UTC->GetClass()->GetDefaultObject<AUTCharacter>()->UTCharacterMovement->bAllowJumpMultijumps && UTC->GetClass()->GetDefaultObject<AUTCharacter>()->UTCharacterMovement->MaxMultiJumpCount > 1)
				{
					*RepeatableJumpZ += UTC->GetClass()->GetDefaultObject<AUTCharacter>()->UTCharacterMovement->MultiJumpImpulse * (UTC->GetClass()->GetDefaultObject<AUTCharacter>()->UTCharacterMovement->MaxMultiJumpCount - 1);
				}
			}

			// Best: what we can do now
			float BestJumpZ = UTC->CharacterMovement->JumpZVelocity;
			if (UTC->UTCharacterMovement->bAllowJumpMultijumps && UTC->UTCharacterMovement->MaxMultiJumpCount > 1)
			{
				BestJumpZ += UTC->UTCharacterMovement->MultiJumpImpulse * (UTC->UTCharacterMovement->MaxMultiJumpCount - 1);
			}
			return BestJumpZ;
		}
		else
		{
			ACharacter* C = Cast<ACharacter>(Asker);
			if (C == NULL || C->CharacterMovement == NULL)
			{
				if (RepeatableJumpZ != NULL)
				{
					*RepeatableJumpZ = 0.0f;
				}
				return 0.0f;
			}
			else
			{
				if (RepeatableJumpZ != NULL)
				{
					*RepeatableJumpZ = C->GetClass()->GetDefaultObject<ACharacter>()->CharacterMovement->JumpZVelocity;
				}
				return C->CharacterMovement->JumpZVelocity;
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
		return AdjustedRequiredJumpZ * 1.05f; // little bit of leeway to avoid broken close calls
	}

	virtual int32 CostFor(int32 DefaultCost, const FUTPathLink& OwnerLink, APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh) const override
	{
		if (Asker == NULL)
		{
			return BLOCKED_PATH_COST;
		}
		else
		{
			float AdjustedRequiredJumpZ = CalcRequiredJumpZ(Asker);
			float RepeatableJumpZ = 0.0f;
			float BestJumpZ = CalcAvailableSimpleJumpZ(Asker, &RepeatableJumpZ);;
			if (BestJumpZ >= AdjustedRequiredJumpZ)
			{
				// extra cost if limited availability movement required
				return DefaultCost + ((RepeatableJumpZ >= AdjustedRequiredJumpZ) ? 0 : 2000);
			}
			// don't try to translocate or impact jump out of a water volume
			else if (GravityVolume.IsValid() && GravityVolume->bWaterVolume)
			{
				return BLOCKED_PATH_COST;
			}
			else
			{
				AUTBot* B = Cast<AUTBot>(Asker->Controller);
				if (B == NULL)
				{
					return BLOCKED_PATH_COST;
				}
				else
				{
					const FVector JumpEnd = NavMesh->GetPolyCenter(OwnerLink.EndPoly);
					const FVector JumpStart = NavMesh->GetPolyCenter(OwnerLink.StartEdgePoly);
					int32 JumpDist = FMath::TruncToInt((JumpEnd - JumpStart).Size());
					// TODO: hardcoded numbers based on default translocator
					if (B->AllowTranslocator() && JumpDist < 2000 * (-2150.0f / FMath::Min<float>(-1.0f, GravityVolume.IsValid() ? GravityVolume->GetGravityZ() : NavMesh->GetWorld()->GetGravityZ())))
					{
						// the higher we need to throw the disc for a lower Z change, the more time the throw will take; adjust distance for that
						return FMath::Max<int32>(450, FMath::TruncToInt((AdjustedRequiredJumpZ - (JumpEnd.Z - JumpStart.Z)) / 1.5f)) + (DefaultCost - JumpDist) + (JumpDist / 2);
					}
					else if (B->AllowImpactJump())
					{
						BestJumpZ += B->ImpactJumpZ;
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

	virtual bool WaitForMove(APawn* Asker) const override
	{
		if (Asker == NULL)
		{
			return false;
		}
		else
		{
			float AdjustedRequiredJumpZ = CalcRequiredJumpZ(Asker);
			float BestJumpZ = CalcAvailableSimpleJumpZ(Asker);
			if (BestJumpZ >= AdjustedRequiredJumpZ)
			{
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

	virtual bool GetMovePoints(const FUTPathLink& OwnerLink, const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const override
	{
		// start bot considering to switch to needed traversal weapon, if applicable
		if (CalcAvailableSimpleJumpZ(Asker) < CalcRequiredJumpZ(Asker))
		{
			AUTBot* B = Cast<AUTBot>(Asker->Controller);
			if (B != NULL)
			{
				B->SwitchToBestWeapon();
			}
		}
		return OwnerLink.GetJumpMovePoints(StartLoc, Asker, AgentProps, Target, FullRoute, NavMesh, MovePoints);
	}
};
