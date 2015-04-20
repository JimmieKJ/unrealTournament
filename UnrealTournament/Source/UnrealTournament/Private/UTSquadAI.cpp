// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTSquadAI.h"
#include "UTPickupWeapon.h"

FName NAME_Attack(TEXT("Attack"));
FName NAME_Defend(TEXT("Defend"));

bool FSuperPickupEval::AllowPickup(APawn* Asker, AActor* Pickup, float Desireability, float PickupDist)
{
	if (ClaimedPickups.Contains(Pickup))
	{
		return false;
	}
	else if (Desireability >= MinDesireability)
	{
		return true;
	}
	else
	{
		bool bResult = false;
		// ignore desireability constraint for bot's favorite weapon if it doesn't have or it's out of ammo
		if (Asker != NULL)
		{
			AUTBot* B = Cast<AUTBot>(Asker->Controller);
			if (B != NULL && B->Personality.FavoriteWeapon != NAME_None)
			{
				AUTPickupWeapon* WeaponPickup = Cast<AUTPickupWeapon>(Pickup);
				if (WeaponPickup != NULL && B->IsFavoriteWeapon(WeaponPickup->WeaponType))
				{
					AUTWeapon* Existing = NULL;
					if (Cast<AUTCharacter>(Asker) != NULL)
					{
						Existing = ((AUTCharacter*)Asker)->FindInventoryType<AUTWeapon>(WeaponPickup->WeaponType, true);
					}
					bResult = (Existing != NULL && Existing->HasAnyAmmo());
				}
			}
		}
		return bResult;
	}
}

AUTSquadAI::AUTSquadAI(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	CurrentSquadRouteIndex = INDEX_NONE;
	MaxSquadRoutes = 5;
}

void AUTSquadAI::AddMember(AController* C)
{
	Members.Add(C);
	if (Leader == NULL)
	{
		SetLeader(C);
	}
}
void AUTSquadAI::RemoveMember(AController* C)
{
	Members.Remove(C);
	if (Members.Num() == 0)
	{
		Destroy();
	}
	else if (Leader == C)
	{
		Leader = NULL;
		SetLeader(Members[0]);
	}
}
void AUTSquadAI::SetLeader(AController* NewLeader)
{
	if (Members.Contains(NewLeader))
	{
		Leader = NewLeader;
	}
}

bool AUTSquadAI::LostEnemy(AUTBot* B)
{
	if (B->GetEnemy() == NULL || B->GetEnemy()->Controller == NULL)
	{
		B->SetEnemy(NULL);
		B->PickNewEnemy();
		return true;
	}
	else if (MustKeepEnemy(B->GetEnemy()))
	{
		return false;
	}
	else if (Team == NULL)
	{
		B->RemoveEnemy(B->GetEnemy());
		B->PickNewEnemy();
		return true;
	}
	else
	{
		// if teammates have detected enemy recently then don't let bot fully lose it, but still let it check for other higher priority enemies
		APawn* PrevEnemy = B->GetEnemy();
		if (Team != NULL)
		{
			const FBotEnemyInfo* TeamEnemyInfo = B->GetEnemyInfo(B->GetEnemy(), true);
			if (TeamEnemyInfo != NULL && GetWorld()->TimeSeconds - TeamEnemyInfo->LastFullUpdateTime > 5.0f)
			{
				B->RemoveEnemy(B->GetEnemy());
			}
		}
		B->PickNewEnemy();
		return PrevEnemy != B->GetEnemy();
	}
}

bool AUTSquadAI::HasBetterPickupClaim(AUTBot* B, const FPickupClaim& Claim)
{
	if (Claim.bHardClaim)
	{
		return false;
	}
	// priority to flag carriers
	else if (B->GetUTChar() != NULL && B->GetUTChar()->GetCarriedObject() != NULL)
	{
		return true;
	}
	else if (Cast<AUTCharacter>(Claim.ClaimedBy) != NULL && ((AUTCharacter*)Claim.ClaimedBy)->GetCarriedObject() != NULL)
	{
		return false;
	}
	// if I'm signficantly closer, let me take it
	else if ((Claim.Pickup->GetActorLocation() - B->GetPawn()->GetActorLocation()).Size() * 0.9f < (Claim.Pickup->GetActorLocation() - Claim.ClaimedBy->GetActorLocation()).Size())
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool AUTSquadAI::CheckSuperPickups(AUTBot* B, int32 MaxDist)
{
	// TODO: check vehicle driver

	TArray<AActor*> ClaimedPickups;
	if (Team != NULL)
	{
		for (const FPickupClaim& Claim : Team->PickupClaims)
		{
			if (Claim.IsValid() && Claim.ClaimedBy != B->GetPawn() && !HasBetterPickupClaim(B, Claim))
			{
				ClaimedPickups.Add(Claim.Pickup);
			}
		}
	}

	FSuperPickupEval NodeEval(B->RespawnPredictionTime, (B->GetCharacter() != NULL) ? B->GetCharacter()->GetCharacterMovement()->MaxWalkSpeed : GetDefault<AUTCharacter>()->GetCharacterMovement()->MaxWalkSpeed, MaxDist, 1.0f, ClaimedPickups);
	float Weight = 0.0f;
	TArray<FRouteCacheItem> PotentialRoute;
	if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), NodeEval, B->GetPawn()->GetNavAgentLocation(), Weight, true, PotentialRoute))
	{
		if (Team != NULL && PotentialRoute.Last().Actor != NULL)
		{
			Team->SetPickupClaim(B->GetPawn(), PotentialRoute.Last().Actor.Get());
		}
		B->GoalString = FString::Printf(TEXT("Get super pickup %s"), *GetNameSafe(PotentialRoute.Last().Actor.Get()));
		B->RouteCache = PotentialRoute;
		B->SetMoveTarget(PotentialRoute[0]);
		B->StartWaitForMove();
		return true;
	}
	else
	{
		return false;
	}
}

bool AUTSquadAI::CheckSquadObjectives(AUTBot* B)
{
	// search specifically for super pickups
	// TODO: maybe get distracted depending on enemy, skill, personality (e.g. grudge)
	if (B->Skill < 1.5f || B->NeedsWeapon())
	{
		// low skill bots don't do 
		return false;
	}
	else
	{
		int32 SuperSearchRange = 2000 * (B->Skill + B->Personality.Tactics + B->Personality.MapAwareness);
		// prefer to engage enemy if we can get a clear shot and are on equal/stronger footing
		if (B->GetEnemy() != NULL && B->IsEnemyVisible(B->GetEnemy()) && B->RelativeStrength(B->GetEnemy()) <= B->Personality.Aggressiveness)
		{
			SuperSearchRange = FMath::Min<int32>(SuperSearchRange, 3000);
		}
		return CheckSuperPickups(B, SuperSearchRange);
	}
}

bool AUTSquadAI::FollowAlternateRoute(AUTBot* B, AActor* Goal, TArray<FAlternateRoute>& Routes, bool bAllowDetours, const FString& SuccessGoalString)
{
	if (B->bDisableSquadRoutes)
	{
		return false;
	}

	if (!Routes.IsValidIndex(B->UsingSquadRouteIndex))
	{
		if (B == Leader || Cast<APlayerController>(Leader) != NULL)
		{
			if (Routes.Num() < MaxSquadRoutes)
			{
				// generate new route
				FSingleEndpointEvalWeighted NodeEval(Goal);
				for (const FAlternateRoute& Route : Routes)
				{
					int32 RouteLength = Route.RouteCache.Num();
					for (int32 j = 0; j < RouteLength; j++)
					{
						const FRouteCacheItem& Point = Route.RouteCache[j];
						if (Point.Node != NULL)
						{
							uint32* ExtraCost = NodeEval.ExtraCosts.Find(Point.Node);
							if (ExtraCost == NULL)
							{
								ExtraCost = &NodeEval.ExtraCosts.Add(Point.Node);
							}

							float CostFactor = (j <= RouteLength / 2) ? j : (RouteLength - j);
							*ExtraCost += uint32(FMath::TruncToInt(CostFactor * 7000.f / RouteLength));
						}
					}
				}

				Routes.AddZeroed(1);
				float Weight = 0.0f;
				if (!NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), NodeEval, B->GetPawn()->GetNavAgentLocation(), Weight, bAllowDetours, Routes.Last().RouteCache))
				{
					Routes.RemoveAt(Routes.Num() - 1);
				}
			}
			CurrentSquadRouteIndex = (Routes.Num() > 0) ? FMath::RandHelper(Routes.Num()) : INDEX_NONE; // need to handle initial route failing!
		}
		B->UsingSquadRouteIndex = CurrentSquadRouteIndex;
		B->SquadRouteGoal.Clear();
	}
	if (!Routes.IsValidIndex(B->UsingSquadRouteIndex))
	{
		// failed to find an alternate path, give up and go direct
		B->bDisableSquadRoutes = true;
		return false;
	}
	else
	{
		// figure out our desired position along the squad route
		// if we've reached it, jump ahead some spaces to the next position (done to minimize congestion amongst squadmates, confusion due to temporary path blockers, etc)
		const FAlternateRoute& AlternatePath = Routes[B->UsingSquadRouteIndex];
		UUTPathNode* Anchor = NavData->GetNodeFromPoly(NavData->FindAnchorPoly(B->GetPawn()->GetNavAgentLocation(), B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef()));
		int32 AnchorIndex = (Anchor != NULL) ? AlternatePath.RouteCache.IndexOfByPredicate([Anchor](const FRouteCacheItem& TestItem){ return TestItem.Node == Anchor; }) : INDEX_NONE;
		// TODO: SquadRouteGoal.Actor depends on ReachSpec chosen, which sometimes changes from when the squad route was generated, so we're ignoring that discrepancy... need more testing to determine if this is correct
		int32 SquadRouteGoalIndex = B->SquadRouteGoal.IsValid() ? AlternatePath.RouteCache.IndexOfByPredicate([B](const FRouteCacheItem& TestItem){ return TestItem.Node == B->SquadRouteGoal.Node;/* && TestItem.Actor == B->SquadRouteGoal.Actor;*/ }) : INDEX_NONE;
		if (SquadRouteGoalIndex == INDEX_NONE || SquadRouteGoalIndex <= AnchorIndex)
		{
			SquadRouteGoalIndex = (AnchorIndex != INDEX_NONE) ? FMath::Min<int32>(AlternatePath.RouteCache.Num() - 1, AnchorIndex + 3) : 0;
		}
		if (SquadRouteGoalIndex == AlternatePath.RouteCache.Num() - 1)
		{
			// done following alternate path
			return false;
		}
		else
		{
			// multi-endpoint search for the desired target and all further points on the alternate (in case bot is slightly ahead, make sure we never backtrack)
			FMultiPathNodeEval NodeEval;
			for (int32 i = SquadRouteGoalIndex; i < AlternatePath.RouteCache.Num(); i++)
			{
				NodeEval.Goals.Add(AlternatePath.RouteCache[i].Node.Get());
			}
			// sanity check the goal is in there
			NodeEval.Goals.Add(NavData->FindNearestNode(Goal->GetActorLocation(), NavData->GetPOIExtent(Goal)));
			float Weight = 0.0f;
			if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), NodeEval, B->GetPawn()->GetNavAgentLocation(), Weight, bAllowDetours, B->RouteCache))
			{
				// set SquadRouteGoal to the endpoint we actually found
				B->SquadRouteGoal = B->RouteCache.Last();
				// fill bot route with the rest of the squad route for e.g. translocator planning
				for (int32 i = SquadRouteGoalIndex; i < AlternatePath.RouteCache.Num(); i++)
				{
					if (AlternatePath.RouteCache[i].Node == B->SquadRouteGoal.Node)
					{
						for (int32 j = i + 1; j < AlternatePath.RouteCache.Num(); j++)
						{
							B->RouteCache.Add(AlternatePath.RouteCache[j]);
						}
						break;
					}
				}
				B->GoalString = SuccessGoalString + TEXT(" (via squad alternate route)");
				B->SetMoveTarget(B->RouteCache[0]);
				B->StartWaitForMove();
				return true;
			}
			else
			{
				// couldn't get to alternate route, give up on it
				B->UsingSquadRouteIndex = INDEX_NONE;
				B->SquadRouteGoal.Clear();
				B->bDisableSquadRoutes = true;
				return false;
			}
		}
	}
}

bool AUTSquadAI::TryPathTowardObjective(AUTBot* B, AActor* Goal, bool bAllowDetours, const FString& SuccessGoalString)
{
	// if not the squad's objective, then don't do any alternate paths
	if (Goal == Objective && FollowAlternateRoute(B, Goal, SquadRoutes, bAllowDetours, SuccessGoalString))
	{
		return true;
	}
	else
	{
		return B->TryPathToward(Goal, bAllowDetours, SuccessGoalString);
	}
}

bool AUTSquadAI::PickRetreatDestination(AUTBot* B)
{
	if (B->FindInventoryGoal(0.0f))
	{
		B->SetMoveTarget(B->RouteCache[0]);
		return true;
	}
	// keep moving to previous retreat destination if possible (don't oscillate)
	else if (B->RouteCache.Num() > 1 && NavData->HasReachedTarget(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), B->RouteCache[0]))
	{
		B->RouteCache.RemoveAt(0);
		B->SetMoveTarget(B->RouteCache[0]);
		return true;
	}
	else
	{
		FRandomDestEval NodeEval;
		float Weight = 0.0f;
		if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), NodeEval, B->GetNavAgentLocation(), Weight, false, B->RouteCache))
		{
			B->SetMoveTarget(B->RouteCache[0]);
			return true;
		}
		else
		{
			return false;
		}
	}
}

void AUTSquadAI::NotifyObjectiveEvent(AActor* InObjective, AController* InstigatedBy, FName EventName)
{
	if (InObjective == Objective)
	{
		for (AController* C : Members)
		{
			AUTBot* B = Cast<AUTBot>(C);
			if (B != NULL)
			{
				if (B == InstigatedBy)
				{
					B->WhatToDoNext();
				}
				else
				{
					// set timer to retask bot, partially just to stagger updates and partially to account for their reaction time
					SetTimerUFunc(B, TEXT("WhatToDoNext"), 0.1f + 0.15f * FMath::FRand() + (0.5f - 0.5f * B->Personality.ReactionTime) * FMath::FRand(), false);
				}
			}
		}
	}
}
