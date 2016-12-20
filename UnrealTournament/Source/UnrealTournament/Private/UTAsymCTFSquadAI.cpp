// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAsymCTFSquadAI.h"
#include "UTDefensePoint.h"
#include "UTFlagRunGame.h"
#include "UTFlagRunGameState.h"
#include "UTRallyPoint.h"
#include "UTFlagRunGameState.h"
#include "UTPickup.h"
#include "UTDroppedPickup.h"

void AUTAsymCTFSquadAI::Initialize(AUTTeamInfo* InTeam, FName InOrders)
{
	Super::Initialize(InTeam, InOrders);

	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (GS != NULL && GS->FlagBases.Num() >= 2 && GS->FlagBases[0] != NULL && GS->FlagBases[1] != NULL)
	{
		GameObjective = GS->FlagBases[GS->bRedToCap ? 1 : 0];
		Objective = GameObjective;
		Flag = GS->FlagBases[GS->bRedToCap ? 0 : 1]->GetCarriedObject();
		TotalFlagRunDistance = (Flag == nullptr) ? FLT_MAX : (Objective->GetActorLocation() - Flag->GetActorLocation()).Size();
		if (GameObjective->DefensePoints.Num() == 0)
		{
			FurthestDefensePointDistance = 10000000.0f;
		}
		else
		{
			FurthestDefensePointDistance = 0.0f;
			for (AUTDefensePoint* DP : GameObjective->DefensePoints)
			{
				FurthestDefensePointDistance = FMath::Max<float>(FurthestDefensePointDistance, (DP->GetActorLocation() - Objective->GetActorLocation()).Size());
			}
		}
	}
	LastFlagNode = nullptr;
	SquadRoutes.Empty();
}

bool AUTAsymCTFSquadAI::IsAttackingTeam() const
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	return (GS != NULL && GetTeamNum() == (GS->bRedToCap ? 0 : 1));
}

bool AUTAsymCTFSquadAI::MustKeepEnemy(AUTBot* B, APawn* TheEnemy)
{
	// generally must keep enemy flag holder
	// but allow losing them in cases where they are farther from objective and not in sight, since they have to come to us to win
	AUTCharacter* UTC = Cast<AUTCharacter>(TheEnemy);
	return (UTC != NULL && UTC->GetCarriedObject() != NULL && (B->IsEnemyVisible(TheEnemy) || (B->GetPawn()->GetActorLocation() - B->GetEnemyLocation(TheEnemy, true)).Size() < 3000.0f + (B->GetPawn()->GetActorLocation() - Objective->GetActorLocation()).Size()));
}

void AUTAsymCTFSquadAI::ModifyAggression(AUTBot* B, float& Aggressiveness)
{
	// reduce aggression against enemies that are much farther away from objective, no need to pursue as they need to come to us
	if ((B->GetEnemyLocation(B->GetEnemy(), true) - B->GetPawn()->GetActorLocation()).Size() > 4000.0f + (B->GetPawn()->GetActorLocation() - Objective->GetActorLocation()).Size())
	{
		Aggressiveness -= 0.5f;
	}
	else
	{
		Super::ModifyAggression(B, Aggressiveness);
	}
}

bool AUTAsymCTFSquadAI::ShouldUseTranslocator(AUTBot* B)
{
	if (Super::ShouldUseTranslocator(B))
	{
		return true;
	}
	else if (B->RouteCache.Num() < 3)
	{
		return false;
	}
	else
	{
		// prioritize translocator when chasing enemy flag carrier
		AUTCharacter* CharGoal = Cast<AUTCharacter>(B->RouteCache.Last().Actor.Get());
		AUTCharacter* CharHuntTarget = Cast<AUTCharacter>(B->HuntingTarget);
		return ((CharGoal != NULL && CharGoal->GetCarriedObject() != NULL) || (CharHuntTarget != NULL && CharHuntTarget->GetCarriedObject() != NULL)) &&
			(B->GetEnemy() != CharGoal || !B->IsEnemyVisible(B->GetEnemy()) || (B->CurrentAggression > 0.0f && (B->GetPawn()->GetActorLocation() - B->GetEnemy()->GetActorLocation()).Size() > 3000.0f));
	}
}

float AUTAsymCTFSquadAI::ModifyEnemyRating(float CurrentRating, const FBotEnemyInfo& EnemyInfo, AUTBot* B)
{
	if (EnemyInfo.GetUTChar() != NULL && EnemyInfo.GetUTChar()->GetCarriedObject() != NULL && B->CanAttack(EnemyInfo.GetPawn(), EnemyInfo.LastKnownLoc, false))
	{
		if ((B->GetPawn()->GetActorLocation() - EnemyInfo.LastKnownLoc).Size() < 3500.0f || (B->GetUTChar() != NULL && B->GetUTChar()->GetWeapon() != NULL && B->GetUTChar()->GetWeapon()->bSniping) ||
			(EnemyInfo.LastKnownLoc - Objective->GetActorLocation()).Size() < 4500.0f )
		{
			return CurrentRating + 6.0f;
		}
		else
		{
			return CurrentRating + 1.5f;
		}
	}
	// prioritize enemies that target friendly flag carrier
	else if (Flag && Flag->HoldingPawn != NULL && IsAttackingTeam() && (Flag->HoldingPawn->LastHitBy == EnemyInfo.GetPawn()->GetController() || !GetWorld()->LineTraceTestByChannel(EnemyInfo.LastKnownLoc, Flag->HoldingPawn->GetActorLocation(), ECC_Pawn, FCollisionQueryParams::DefaultQueryParam, WorldResponseParams)))
	{
		return CurrentRating + 1.5f;
	}
	else
	{
		return CurrentRating;
	}
}

bool AUTAsymCTFSquadAI::TryPathTowardObjective(AUTBot* B, AActor* Goal, bool bAllowDetours, const FString& SuccessGoalString)
{
	bool bResult = Super::TryPathTowardObjective(B, Goal, bAllowDetours, SuccessGoalString);
	if (bResult && Cast<AUTCarriedObject>(Goal) != NULL && B->GetRouteDist() < 2500 && B->LineOfSightTo(Goal))
	{
		B->SendVoiceMessage(StatusMessage::IGotFlag);
	}
	return bResult;
}

bool AUTAsymCTFSquadAI::ShouldStartRally(AUTBot* B)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (GS != nullptr && (B->GetEnemy() == nullptr || GetWorld()->TimeSeconds - B->GetEnemyInfo(B->GetEnemy(), false)->LastFullUpdateTime > 2.0f) && GetWorld()->TimeSeconds - LastRallyTime > 15.0f)
	{
		float ClosestAllyDist = FLT_MAX;
		for (AController* C : Team->GetTeamMembers())
		{
			if (C != B && C->GetPawn() != nullptr)
			{
				ClosestAllyDist = FMath::Min<float>(ClosestAllyDist, (C->GetPawn()->GetActorLocation() - B->GetPawn()->GetActorLocation()).Size());
			}
		}
		if (ClosestAllyDist > 2500.0f)
		{
			// check if we beat the defense and shouldn't hold up
			if (B->Skill + B->Personality.Tactics >= 4.0f)
			{
				const TArray<const FBotEnemyInfo>& EnemyList = Team->GetEnemyList();
				for (const FBotEnemyInfo& TestEnemy : EnemyList)
				{
					if ((TestEnemy.LastKnownLoc - Objective->GetActorLocation()).Size() < (B->GetPawn()->GetActorLocation() - Objective->GetActorLocation()).Size())
					{
						return true;
					}
				}
				// check if there are unknown respawned enemies
				// this isn't cheating because the HUD shows respawn times
				AUTTeamInfo* EnemyTeam = GS->Teams[Team->TeamIndex == 0 ? 1 : 0];
				if (EnemyList.Num() < EnemyTeam->GetTeamMembers().Num())
				{
					for (AController* C : EnemyTeam->GetTeamMembers())
					{
						if (C->GetPawn() != nullptr && !EnemyList.ContainsByPredicate([C](const FBotEnemyInfo& TestItem) { return TestItem.GetPawn() == C->GetPawn(); }))
						{
							return true;
						}
					}
				}
			}
			else
			{
				return true;
			}
		}
	}
	return false;
}

bool AUTAsymCTFSquadAI::SetFlagCarrierAction(AUTBot* B)
{
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (GS != nullptr && GS->CurrentRallyPoint != nullptr && GS->CurrentRallyPoint->RallyPointState == RallyPointStates::Powered)
	{
		// if not aggressive bot, maybe wait here for allies to rally
		if (GS->CurrentRallyPoint->RallyTimeRemaining >= 3.0f && FMath::FRand() < 0.5f - 0.5f * B->Personality.Aggressiveness)
		{
			B->GoalString = TEXT("Wait for teammates to rally");
			B->DoCamp();
			return true;
		}
		bWantRally = false;
		LastRallyTime = GetWorld()->TimeSeconds;
	}
	else
	{
		bWantRally = bWantRally || ShouldStartRally(B);
		if (bWantRally)
		{
			// path looking for rally points and the goal (so head to closest one or ignore and go for the win if objective is closer)
			TArray<FRouteCacheItem> Goals;
			for (TActorIterator<AUTRallyPoint> It(GetWorld()); It; ++It)
			{
				NavNodeRef Poly = NavData->UTFindNearestPoly(It->GetActorLocation(), NavData->GetPOIExtent(*It));
				if (Poly != INVALID_NAVNODEREF)
				{
					const FRouteCacheItem* RallyPt = new(Goals) FRouteCacheItem(*It, It->GetActorLocation(), Poly, NavData->GetNodeFromPoly(Poly));
					if (NavData->HasReachedTarget(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), *RallyPt))
					{
						B->GoalString = FString::Printf(TEXT("Wait for rally point %s to activate"), *It->GetName());
						B->DoCamp();
						return true;
					}
				}
			}
			{
				NavNodeRef Poly = NavData->UTFindNearestPoly(Objective->GetActorLocation(), NavData->GetPOIExtent(Objective));
				if (Poly != INVALID_NAVNODEREF)
				{
					new(Goals) FRouteCacheItem(Objective, Objective->GetActorLocation(), Poly, NavData->GetNodeFromPoly(Poly));
				}
			}
			FMultiPathNodeEval Eval(Goals);
			float Weight = 0.0f;
			if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), B, Eval, B->GetPawn()->GetNavAgentLocation(), Weight, true, B->RouteCache) && B->RouteCache.Last().Actor != Objective)
			{
				B->GoalString = FString::Printf(TEXT("Go to rally point %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
				B->SetMoveTarget(B->RouteCache[0]);
				B->StartWaitForMove();
				return true;
			}
		}
	}
	// TODO: wait for allies, maybe double back, etc
	return TryPathTowardObjective(B, Objective, false, "Head to enemy base with flag");
}

bool AUTAsymCTFSquadAI::TryPathToFlag(AUTBot* B, TArray<FAlternateRoute>& Routes, const FString& SuccessGoalString)
{
	const UUTPathNode* FlagNode = NavData->FindNearestNode(Flag->GetActorLocation(), NavData->GetPOIExtent(Flag));
	if (FlagNode != LastFlagNode)
	{
		Routes.Reset();
		LastFlagNode = FlagNode;
	}
	return FollowAlternateRoute(B, Flag, Routes, true, true, SuccessGoalString) || B->TryPathToward(Flag, true, true, SuccessGoalString);
}

void AUTAsymCTFSquadAI::GetPossibleEnemyGoals(AUTBot* B, const FBotEnemyInfo* EnemyInfo, TArray<FPredictedGoal>& Goals)
{
	Super::GetPossibleEnemyGoals(B, EnemyInfo, Goals);
	if (!IsAttackingTeam() && Flag != NULL && Objective != NULL)
	{
		if (Flag->HoldingPawn == NULL)
		{
			Goals.Add(FPredictedGoal(Flag->GetActorLocation() - FVector(0.0f, 0.0f, Flag->GetSimpleCollisionHalfHeight()), true));
		}
		else
		{
			Goals.Add(FPredictedGoal(Objective->GetActorLocation(), true));
		}
	}
}

bool AUTAsymCTFSquadAI::HuntEnemyFlag(AUTBot* B)
{
	if (Flag->HoldingPawn != nullptr)
	{
		// if the enemy FC has never been seen, use the alternate path logic
		// this prevents the AI from abandoning the alternate approach routes prematurely when the flag is picked up
		const FBotEnemyInfo* CarrierInfo = B->GetEnemyInfo(Flag->HoldingPawn, true);
		if (CarrierInfo == nullptr || CarrierInfo->LastSeenTime <= 0.0f)
		{
			if ((B->GetPawn()->GetActorLocation() - Flag->HoldingPawn->GetActorLocation()).Size() < TotalFlagRunDistance * (0.33f - 0.1f * B->Personality.Aggressiveness))
			{
				B->GoalString = TEXT("Wait here for enemy assault to begin");
				B->DoCamp();
				return true;
			}
			else if (FollowAlternateRoute(B, Flag->HoldingPawn, SquadRoutes, true, true, "Continue prior route to flag carrier"))
			{
				return true;
			}
		}
		
		if (Flag->HoldingPawn == B->GetEnemy())
		{
			B->GoalString = "Fight flag carrier";
			// fight enemy
			return false;
		}
		else
		{
			B->GoalString = "Hunt down enemy flag carrier";
			B->DoHunt(Flag->HoldingPawn);
			return true;
		}
	}
	else if ( (B->RouteCache.Num() > 0 && (Cast<AUTPickup>(B->RouteCache.Last().Actor.Get()) != nullptr || Cast<AUTDroppedPickup>(B->RouteCache.Last().Actor.Get()) != nullptr) && !NavData->HasReachedTarget(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), B->RouteCache.Last())) ||
				((Flag->GetActorLocation() - B->GetPawn()->GetActorLocation()).Size() < FMath::Min<float>(3000.0f, (Flag->GetActorLocation() - Objective->GetActorLocation()).Size()) && B->UTLineOfSightTo(Flag)) )
	{
		// fight/camp here
		return false;
	}
	else
	{
		// use alternate route to reach flag so that defenders approach from multiple angles
		return TryPathToFlag(B, SquadRoutes, "Camp dropped flag");
	}
}

bool AUTAsymCTFSquadAI::CheckSquadObjectives(AUTBot* B)
{
	AUTFlagRunGame* Game = GetWorld()->GetAuthGameMode<AUTFlagRunGame>();
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();

	// make bot with the flag Leader if possible
	if (B->GetUTChar() != nullptr && B->GetUTChar()->GetCarriedObject() != nullptr && Cast<APlayerController>(Leader) == nullptr)
	{
		SetLeader(B);
	}

	FName CurrentOrders = GetCurrentOrders(B);

	if (Flag == NULL || Objective == NULL)
	{
		return Super::CheckSquadObjectives(B);
	}
	else if (IsAttackingTeam())
	{
		B->SetDefensePoint(NULL);
		if (B->GetUTChar() != NULL && B->GetUTChar()->GetCarriedObject() != NULL)
		{
			return SetFlagCarrierAction(B);
		}
		else if (B->NeedsWeapon() && (GameObjective == NULL || GameObjective->GetCarriedObject() == NULL || (B->GetPawn()->GetActorLocation() - GameObjective->GetCarriedObject()->GetActorLocation()).Size() > 3000.0f) && B->FindInventoryGoal(0.0f))
		{
			B->GoalString = FString::Printf(TEXT("Get inventory %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
			B->SetMoveTarget(B->RouteCache[0]);
			B->StartWaitForMove();
			return true;
		}
		else if ( Game != nullptr && GS != nullptr && GS->CurrentRallyPoint != nullptr &&
				(GS->CurrentRallyPoint->GetActorLocation() - Objective->GetActorLocation()).Size() < (B->GetPawn()->GetActorLocation() - Objective->GetActorLocation()).Size() - 1500.0f &&
				(B->GetEnemy() == nullptr || !B->IsEnemyVisible(B->GetEnemy())) && Game->HandleRallyRequest(B) )
		{
			B->DoCamp();
			return true;
		}
		else if (CurrentOrders == NAME_Defend)
		{
			if (Flag->HoldingPawn == NULL)
			{
				// amortize generation of alternate routes during delay before flag can be picked up
				if (!Flag->bFriendlyCanPickup && SquadRoutes.Num() < MaxSquadRoutes && (GetLeader() == B || Cast<APlayerController>(GetLeader()) != nullptr))
				{
					FollowAlternateRoute(B, Objective, SquadRoutes, false, false, TEXT(""));
					// clear cached values since we're not actually following the route right now
					B->UsingSquadRouteIndex = INDEX_NONE;
					B->bDisableSquadRoutes = false;
					CurrentSquadRouteIndex = INDEX_NONE;
				}
				if (!Flag->bFriendlyCanPickup)
				{
					// don't go for flag if someone else already on it
					for (AController* C : Team->GetTeamMembers())
					{
						if (C != B && C->GetPawn() != nullptr && C->GetPawn()->IsOverlappingActor(Flag))
						{
							if (B->FindInventoryGoal(0.0f))
							{
								B->GoalString = FString::Printf(TEXT("Initial rush: Head to inventory %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
								B->SetMoveTarget(B->RouteCache[0]);
								B->StartWaitForMove();
								return true;
							}
							else
							{
								return false;
							}
						}
					}
				}
				// use alternate route to flag when respawned, not if it dropped while we were alive
				if (B->UsingSquadRouteIndex != INDEX_NONE || GetWorld()->TimeSeconds - B->GetPawn()->CreationTime < 10.0f)
				{
					bool bResult = TryPathToFlag(B, FlagRetrievalRoutes, "Get flag");
					// amortize generation of alternate routes during the first couple nodes while the bot is in spawn
					if (FlagRetrievalRoutes.Num() < 3)
					{
						B->UsingSquadRouteIndex = INDEX_NONE;
						B->bDisableSquadRoutes = false;
						CurrentSquadRouteIndex = INDEX_NONE;
					}
					return bResult;
				}
				else
				{
					return B->TryPathToward(Flag, true, false, "Get flag");
				}
			}
			else if ((B->GetPawn()->GetActorLocation() - Flag->HoldingPawn->GetActorLocation()).Size() < 2000.0f)
			{
				return false; // fight enemies around FC
			}
			else
			{
				return B->TryPathToward(Flag->HoldingPawn, true, false, "Find flag carrier");
			}
		}
		else
		{
			// priorize fighting and powerups
			if (B->GetEnemy() != NULL && !B->LostContact(2.0f + 1.5f * B->Personality.Aggressiveness))
			{
				return false;
			}
			else if (CheckSuperPickups(B, 10000))
			{
				return true;
			}
			else if (Team->GetEnemyList().Num() == 0 && B->FindInventoryGoal(0.0f))
			{
				B->GoalString = FString::Printf(TEXT("Initial rush: Head to inventory %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
				B->SetMoveTarget(B->RouteCache[0]);
				B->StartWaitForMove();
				return true;
			}
			else if (Flag->HoldingPawn == nullptr && (B->UTLineOfSightTo(Flag) || (Flag->GetActorLocation() - B->GetPawn()->GetActorLocation()).Size() < 2500.0f))
			{
				return B->TryPathToward(Flag, true, false, "Get flag because near");
			}
			else if (!B->bDisableSquadRoutes)
			{
				return TryPathTowardObjective(B, Objective, true, TEXT("Attack enemy base"));
			}
			else if (Flag->HoldingPawn == nullptr)
			{
				return B->TryPathToward(Flag, true, false, "Get flag because nothing else to do");
			}
			else
			{
				return B->TryPathToward(Flag->HoldingPawn, true, false, TEXT("Find flag carrier"));
			}
		}
	}
	else
	{
		// during initial setup all defending bots use defense points (if available)
		if (CurrentOrders == NAME_Defend || !Flag->bFriendlyCanPickup)
		{
			SetDefensePointFor(B);
		}
		else
		{
			B->SetDefensePoint(NULL);
		}

		if (B->NeedsWeapon() && B->FindInventoryGoal(0.0f))
		{
			B->GoalString = FString::Printf(TEXT("Get inventory %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
			B->SetMoveTarget(B->RouteCache[0]);
			B->StartWaitForMove();
			return true;
		}
		else if (CurrentOrders == NAME_Defend || B->GetDefensePoint() != nullptr)	
		{
			if (B->GetEnemy() != NULL)
			{
				// prioritize defense point if haven't actually encountered enemy since respawning
				bool bPrioritizeEnemy = B->GetEnemy() != nullptr && (B->GetDefensePoint() == nullptr || B->GetEnemyInfo(B->GetEnemy(), false)->LastFullUpdateTime > B->LastRespawnTime);
				if (bPrioritizeEnemy && B->GetEnemy() == Flag->HoldingPawn && (B->GetDefensePoint() == nullptr || !B->GetDefensePoint()->bSniperSpot || (!B->IsEnemyVisible(B->GetEnemy()) && MustKeepEnemy(B, B->GetEnemy()))))
				{
					return HuntEnemyFlag(B);
				}
				else if (bPrioritizeEnemy && (!B->LostContact(3.0f) || MustKeepEnemy(B, B->GetEnemy())))
				{
					B->GoalString = "Fight attacker";
					return false;
				}
				else if (CheckSuperPickups(B, 5000))
				{
					return true;
				}
				else if (B->GetDefensePoint() != NULL)
				{
					return B->TryPathToward(B->GetDefensePoint(), true, false, "Go to defense point");
				}
				else
				{
					B->GoalString = "Fight attacker";
					return false;
				}
			}
			else if (Super::CheckSquadObjectives(B))
			{
				return true;
			}
			else if (B->FindInventoryGoal(0.0003f))
			{
				B->GoalString = FString::Printf(TEXT("Get inventory %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
				B->SetMoveTarget(B->RouteCache[0]);
				B->StartWaitForMove();
				return true;
			}
			else if (B->GetDefensePoint() != NULL)
			{
				return B->TryPathToward(B->GetDefensePoint(), true, false, "Go to defense point");
			}
			// TODO: decide between defending flag or defending goal based on situation
			else if (Flag != NULL)
			{
				return HuntEnemyFlag(B);
			}
			else if (Objective != NULL)
			{
				return B->TryPathToward(Objective, true, true, "Defend objective");
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (B->GetEnemy() != NULL && MustKeepEnemy(B, B->GetEnemy()) && !B->LostContact(2.0f))
			{
				return HuntEnemyFlag(B);
			}
			else
			{
				const FBotEnemyInfo* EnemyFCTeamData = (Flag->HoldingPawn != nullptr) ? Team->GetEnemyList().FindByPredicate([this](const FBotEnemyInfo& TestItem) { return TestItem.GetPawn() == Flag->HoldingPawn; }) : nullptr;
				// prioritize finding FC if no eyes on it for a while
				if (EnemyFCTeamData != nullptr && GetWorld()->TimeSeconds - EnemyFCTeamData->LastFullUpdateTime > 3.0f)
				{
					return HuntEnemyFlag(B);
				}
				else if (CurrentOrders == NAME_Attack)
				{
					if (CheckSuperPickups(B, 8000))
					{
						return true;
					}
					else
					{
						return HuntEnemyFlag(B);
					}
				}
				else
				{
					// freelance, just fight whoever's around
					return false;
				}
			}
		}
	}
}

int32 AUTAsymCTFSquadAI::GetDefensePointPriority(AUTBot* B, AUTDefensePoint* Point)
{
	// only flag defense bots use the sniper spots since the offensive bots will switch to powerup control and pursuing enemies
	if (Point->bSniperSpot && GetCurrentOrders(B) != NAME_Defend)
	{
		return 0;
	}
	else
	{
		// prioritize defense points closer to start of map
		// but outright reject those that the enemy FC has passed
		const float PointDist = (Point->GetActorLocation() - Objective->GetActorLocation()).Size();
		const FVector FlagLoc = (Flag->HoldingPawn != nullptr) ? B->GetEnemyLocation(Flag->HoldingPawn, true) : Flag->GetActorLocation();
		const float FlagDist = (FlagLoc - Objective->GetActorLocation()).Size();
		// hard reject if flag has passed the point or it's too far away from the action
		if ((FMath::Min<float>(FlagDist, FurthestDefensePointDistance) - 4000.0f > PointDist && !Point->bSniperSpot) || FlagDist + 2000.0f < PointDist || (FlagDist < PointDist && GetWorld()->LineTraceTestByChannel(Point->GetActorLocation(), FlagLoc, ECC_Visibility)))
		{
			return 0;
		}
		else
		{
			return Super::GetDefensePointPriority(B, Point) + FMath::Min<int32>(33, FMath::TruncToInt(33.0f * (PointDist / TotalFlagRunDistance)));
		}
	}
}

void AUTAsymCTFSquadAI::NotifyObjectiveEvent(AActor* InObjective, AController* InstigatedBy, FName EventName)
{
	bWantRally = false;
	AUTGameObjective* InGameObjective = Cast<AUTGameObjective>(InObjective);
	for (AController* C : Members)
	{
		AUTBot* B = Cast<AUTBot>(C);
		if (B != NULL)
		{
			if (B->GetUTChar() != NULL && B->GetUTChar()->GetCarriedObject() != NULL)
			{
				// retask flag carrier immediately
				B->WhatToDoNext();
			}
			else if (B->GetMoveTarget().Actor != NULL && (B->GetMoveTarget().Actor == InObjective || (InGameObjective != NULL && B->GetMoveTarget().Actor == InGameObjective->GetCarriedObject())))
			{
				SetRetaskTimer(B);
			}
		}
	}

	Super::NotifyObjectiveEvent(InObjective, InstigatedBy, EventName);
}

bool AUTAsymCTFSquadAI::HasHighPriorityObjective(AUTBot* B)
{
	return ((B->GetUTChar() != NULL && B->GetUTChar()->GetCarriedObject() != NULL) || (B->GetEnemy() != NULL && MustKeepEnemy(B, B->GetEnemy())));
}