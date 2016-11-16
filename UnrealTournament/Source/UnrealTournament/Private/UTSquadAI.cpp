// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTSquadAI.h"
#include "UTPickupWeapon.h"
#include "UTDefensePoint.h"
#include "UTWeaponLocker.h"

FName NAME_Attack(TEXT("Attack"));
FName NAME_Defend(TEXT("Defend"));

bool FSuperPickupEval::AllowPickup(APawn* Asker, AController* RequestOwner, AActor* Pickup, float Desireability, float PickupDist)
{
	if (ClaimedPickups.Contains(Pickup))
	{
		return false;
	}
	else
	{
		AUTPickup* LevelPickup = Cast<AUTPickup>(Pickup);
		if ((LevelPickup != NULL) ? LevelPickup->IsSuperDesireable(RequestOwner, Desireability) : (Desireability >= MinDesireability))
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
						bResult = (Existing == NULL || !Existing->HasAnyAmmo());
					}
				}
			}
			return bResult;
		}
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
	else if (MustKeepEnemy(B, B->GetEnemy()))
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

int32 AUTSquadAI::GetDefensePointPriority(AUTBot* B, AUTDefensePoint* Point)
{
	return Point->GetPriorityFor(B);
}

void AUTSquadAI::SetDefensePointFor(AUTBot* B)
{
	if (GameObjective != NULL)
	{
		if ( B->GetDefensePoint() == NULL || GameObjective != B->GetDefensePoint()->Objective || GetDefensePointPriority(B, B->GetDefensePoint()) <= 0 /* || (B.DefensePoint.bOnlyOnFoot && Vehicle(B.Pawn) != None)*/ ||
			// don't change defensepoints if fighting, recently fought, or if haven't reached it yet
			(B->GetEnemy() == NULL && GetWorld()->TimeSeconds - B->GetLastAnyEnemySeenTime() >= 5.0 && NavData != NULL && NavData->HasReachedTarget(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), FRouteCacheItem(B->GetDefensePoint()))) )
		{ 
			TArray<AUTDefensePoint*> DefensePoints = GameObjective->DefensePoints;
			// remove points in use
			for (int32 i = DefensePoints.Num() - 1; i >= 0; i--)
			{
				if (DefensePoints[i] == NULL || (DefensePoints[i]->CurrentUser != NULL && DefensePoints[i]->CurrentUser != B))
				{
					DefensePoints.RemoveAt(i);
				}
			}

			if (DefensePoints.Num() == 0)
			{
				B->SetDefensePoint(NULL);
			}
			else
			{
				TArray<int32> Priorities;
				Priorities.Reserve(DefensePoints.Num());
				int32 TotalPriority = 0;
				for (AUTDefensePoint* Point : DefensePoints)
				{
					int32 Priority = FMath::Max<int32>(0, GetDefensePointPriority(B, Point));
					Priorities.Add(Priority);
					TotalPriority += Priority;
				}
				int32 Choice = FMath::RandHelper(TotalPriority);
				for (int32 i = 0; i < Priorities.Num(); i++)
				{
					Choice -= Priorities[i];
					if (Choice < 0)
					{
						B->SetDefensePoint(DefensePoints[i]);
						break;
					}
				}
			}
		}
	}
}

static bool NeedsPickupClaim(AActor* Pickup)
{
	// weapon lockers are always weapon stay so everyone can have one
	if (Cast<AUTWeaponLocker>(Pickup) != nullptr)
	{
		return false;
	}
	else
	{
		AUTGameState* GS = Pickup->GetWorld()->GetGameState<AUTGameState>();
		if (GS == nullptr || !GS->bWeaponStay)
		{
			return true;
		}
		else
		{
			AUTPickupInventory* IP = Cast<AUTPickupInventory>(Pickup);
			if (IP == nullptr)
			{
				return true;
			}
			else
			{
				TSubclassOf<AUTWeapon> WeaponType(*IP->GetInventoryType());
				return (WeaponType == nullptr || !WeaponType.GetDefaultObject()->bWeaponStay);
			}
		}
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
	else
	{
		// if I'm signficantly closer, let me take it
		float ClaimDist = (Claim.Pickup->GetActorLocation() - Claim.ClaimedBy->GetActorLocation()).Size();
		if ((Claim.Pickup->GetActorLocation() - B->GetPawn()->GetActorLocation()).Size() * 0.9f < ClaimDist && ClaimDist > 512.0f)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}

bool AUTSquadAI::CheckSuperPickups(AUTBot* B, int32 MaxDist)
{
	// TODO: check vehicle driver

	if (B->GetUTChar() == NULL || !B->GetUTChar()->bCanPickupItems)
	{
		return false;
	}

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
		// check for human teammates hovering near pickups and assume they have "claimed" it
		for (AController* C : Team->GetTeamMembers())
		{
			APlayerController* PC = Cast<APlayerController>(C);
			if (PC != NULL && PC->GetPawn() != NULL)
			{
				UUTPathNode* Node = NavData->FindNearestNode(PC->GetPawn()->GetNavAgentLocation(), PC->GetPawn()->GetSimpleCollisionCylinderExtent());
				if (Node != NULL)
				{
					for (TWeakObjectPtr<AActor> POI : Node->POIs)
					{
						AUTPickup* Item = Cast<AUTPickup>(POI.Get());
						if (Item != NULL && NeedsPickupClaim(Item) && (Item->GetActorLocation() - PC->GetPawn()->GetActorLocation()).Size() < 512.0f && !HasBetterPickupClaim(B, FPickupClaim(PC->GetPawn(), POI.Get(), false)))
						{
							ClaimedPickups.Add(POI.Get());
						}
					}
				}
			}
		}
	}

	AActor* PrevGoal = (B->RouteCache.Num() > 0) ? B->RouteCache.Last().Actor.Get() : NULL;
	FSuperPickupEval NodeEval(B->RespawnPredictionTime, (B->GetCharacter() != NULL) ? B->GetCharacter()->GetCharacterMovement()->MaxWalkSpeed : GetDefault<AUTCharacter>()->GetCharacterMovement()->MaxWalkSpeed, MaxDist, 1.0f, ClaimedPickups, PrevGoal);
	float Weight = 0.0f;
	TArray<FRouteCacheItem> PotentialRoute;
	if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), B, NodeEval, B->GetPawn()->GetNavAgentLocation(), Weight, true, PotentialRoute))
	{
		AActor* NewGoal = PotentialRoute.Last().Actor.Get();
		if (Team != NULL && NewGoal != NULL && NeedsPickupClaim(NewGoal))
		{
			Team->SetPickupClaim(B->GetPawn(), NewGoal);
		}
		B->RouteCache = PotentialRoute;
		if (PotentialRoute.Num() == 1 && NavData->HasReachedTarget(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), PotentialRoute[0]))
		{
			B->GoalString = FString::Printf(TEXT("Camp here for super pickup %s"), *GetNameSafe(NewGoal));
			B->DoCamp();
		}
		else
		{
			// announce to team
			if (NewGoal != NULL && NewGoal != PrevGoal && B->Skill >= 3.0f)
			{
				AUTPickup* Item = Cast<AUTPickup>(NewGoal);
				if (Item != NULL && !Item->State.bActive && Item->BaseDesireability >= 1.0f)
				{
					if (B->Skill + B->Personality.MapAwareness >= 6.0f)
					{
						// give exact time remaining
						B->Say(FText::Format(NSLOCTEXT("UTBot", "PickupTimingExact", "{0} up in {1}. On my way there."), Item->GetDisplayName(), FText::AsNumber(FMath::CeilToInt(Item->GetRespawnTimeOffset(B->GetPawn())))).ToString(), true);
					}
					else
					{
						B->Say(FText::Format(NSLOCTEXT("UTBot", "PickupTimingSoon", "{0} respawning soon. On my way there."), Item->GetDisplayName()).ToString(), true);
					}
				}
			}
			B->GoalString = FString::Printf(TEXT("Get super pickup %s"), *GetNameSafe(NewGoal));
			B->SetMoveTarget(PotentialRoute[0]);
			B->StartWaitForMove();
		}
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

bool AUTSquadAI::FollowAlternateRoute(AUTBot* B, AActor* Goal, TArray<FAlternateRoute>& Routes, bool bAllowDetours, bool bAllowPartial, const FString& SuccessGoalString)
{
	if (Goal == NULL || B->bDisableSquadRoutes)
	{
		return false;
	}

	if (!Routes.IsValidIndex(B->UsingSquadRouteIndex))
	{
		// initially allow squadmates to go on separate routes in the name of quickly calculating multiple routes
		if (B == Leader || Cast<APlayerController>(Leader) != NULL || Routes.Num() < FMath::Min<int32>(MaxSquadRoutes, 2))
		{
			if (Routes.Num() < MaxSquadRoutes)
			{
				// generate new route
				FSingleEndpointEvalWeighted NodeEval(Goal, bAllowPartial);
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
				if (!NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), B, NodeEval, B->GetPawn()->GetNavAgentLocation(), Weight, false, Routes.Last().RouteCache))
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
			B->UsingSquadRouteIndex = INDEX_NONE;
			B->SquadRouteGoal.Clear();
			B->bDisableSquadRoutes = true;
			return false;
		}
		else
		{
			// multi-endpoint search for the desired target and all further points on the alternate (in case bot is slightly ahead, make sure we never backtrack)
			FMultiPathNodeEval NodeEval;
			for (int32 i = SquadRouteGoalIndex; i < AlternatePath.RouteCache.Num(); i++)
			{
				if (AlternatePath.RouteCache[i].Node == nullptr || !AlternatePath.RouteCache[i].Node->bDestinationOnly)
				{
					NodeEval.Goals.Add(AlternatePath.RouteCache[i]);
				}
			}
			// sanity check the goal is in there
			{
				NavNodeRef Poly = NavData->UTFindNearestPoly(Goal->GetActorLocation(), NavData->GetPOIExtent(Goal));
				NodeEval.Goals.Add(FRouteCacheItem(Goal, Goal->GetActorLocation(), Poly, NavData->GetNodeFromPoly(Poly)));
			}
			float Weight = 0.0f;
			if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), B, NodeEval, B->GetPawn()->GetNavAgentLocation(), Weight, bAllowDetours, B->RouteCache))
			{
				// set SquadRouteGoal to the endpoint we actually found
				// note: there might be an extra route item at the end to reach the exact location of the squad route point, but we just want to match the PathNode
				B->SquadRouteGoal = (B->RouteCache.Last().Actor == nullptr && B->RouteCache.Last().Node == nullptr && B->RouteCache.Num() >= 2) ? B->RouteCache[B->RouteCache.Num() - 2] : B->RouteCache.Last();
				// fill bot route with the rest of the squad route for e.g. translocator planning
				for (int32 i = SquadRouteGoalIndex; i < AlternatePath.RouteCache.Num() - 1; i++)
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
	if (Goal == Objective && FollowAlternateRoute(B, Goal, SquadRoutes, bAllowDetours, false, SuccessGoalString))
	{
		return true;
	}
	else
	{
		return B->TryPathToward(Goal, bAllowDetours, false, SuccessGoalString);
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
		if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), B, NodeEval, B->GetNavAgentLocation(), Weight, false, B->RouteCache))
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

bool AUTSquadAI::ShouldUseTranslocator(AUTBot* B)
{
	// use only if no enemy to shoot at
	return (B->GetTarget() == NULL || !B->CanAttack(B->GetTarget(), (B->GetTarget() == B->GetEnemy()) ? B->GetEnemyLocation(B->GetEnemy(), true) : B->GetTarget()->GetActorLocation(), B->GetEnemy() == NULL || MustKeepEnemy(B, B->GetEnemy())));
}

void AUTSquadAI::GetPossibleEnemyGoals(AUTBot* B, const FBotEnemyInfo* EnemyInfo, TArray<FPredictedGoal>& Goals)
{
	if (B != nullptr && EnemyInfo != nullptr && EnemyInfo->GetPawn() != nullptr)
	{
		float MaxWalkSpeed = GetDefault<AUTCharacter>()->GetCharacterMovement()->MaxWalkSpeed;
		if (EnemyInfo->GetUTChar() != nullptr)
		{
			MaxWalkSpeed = EnemyInfo->GetUTChar()->GetCharacterMovement()->MaxWalkSpeed;
		}
		// assume enemy wants super pickups
		FSuperPickupEval NodeEval(B->RespawnPredictionTime, MaxWalkSpeed, 10000, 1.0f);
		float Weight = 0.0f;
		TArray<FRouteCacheItem> EnemyRouteCache;
		if (NavData->FindBestPath(EnemyInfo->GetPawn(), EnemyInfo->GetPawn()->GetNavAgentPropertiesRef(), B, NodeEval, EnemyInfo->LastKnownLoc, Weight, false, EnemyRouteCache))
		{
			Goals.Add(FPredictedGoal(EnemyRouteCache.Last().GetLocation(NULL), false));
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
					SetRetaskTimer(B);
				}
			}
		}
	}
}
