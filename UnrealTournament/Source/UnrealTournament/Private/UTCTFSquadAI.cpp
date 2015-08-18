// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFSquadAI.h"
#include "UTCTFFlag.h"

AUTCTFSquadAI::AUTCTFSquadAI(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void AUTCTFSquadAI::Initialize(AUTTeamInfo* InTeam, FName InOrders)
{
	Super::Initialize(InTeam, InOrders);

	for (TActorIterator<AUTGameObjective> It(GetWorld()); It; ++It)
	{
		if (It->GetTeamNum() != GetTeamNum())
		{
			EnemyBase = *It;
		}
		else
		{
			FriendlyBase = *It;
		}
	}
	// initial objective - if attacking, enemy base; if defending, my base
	if (Orders == NAME_Attack)
	{
		SetObjective(EnemyBase);
	}
	else if (Orders == NAME_Defend)
	{
		SetObjective(FriendlyBase);
	}
}

bool AUTCTFSquadAI::MustKeepEnemy(APawn* TheEnemy)
{
	// must keep enemy flag holder
	AUTCharacter* UTC = Cast<AUTCharacter>(TheEnemy);
	return (UTC != NULL && UTC->GetCarriedObject() != NULL && UTC->GetCarriedObject()->GetTeamNum() == GetTeamNum());
}

bool AUTCTFSquadAI::ShouldUseTranslocator(AUTBot* B)
{
	if (Super::ShouldUseTranslocator(B))
	{
		return true;
	}
	else if (FriendlyBase == NULL || FriendlyBase->GetCarriedObject() == NULL || FriendlyBase->GetCarriedObjectState() == CarriedObjectState::Home)
	{
		return false;
	}
	else
	{
		// prioritize translocator when chasing enemy flag carrier
		return ( B->RouteCache.Num() > 0 && (B->RouteCache.Last().Actor == FriendlyBase->GetCarriedObject() || B->RouteCache.Last().Actor == FriendlyBase->GetCarriedObject()->HoldingPawn) &&
				(B->GetEnemy() != FriendlyBase->GetCarriedObject()->HoldingPawn || !B->IsEnemyVisible(B->GetEnemy()) || (B->CurrentAggression > 0.0f && (B->GetPawn()->GetActorLocation() - B->GetEnemy()->GetActorLocation()).Size() > 3000.0f)) );
	}
}

bool AUTCTFSquadAI::IsNearEnemyBase(const FVector& TestLoc)
{
	return EnemyBase != NULL && (TestLoc - EnemyBase->GetActorLocation()).Size() < 4500.0f;
}

float AUTCTFSquadAI::ModifyEnemyRating(float CurrentRating, const FBotEnemyInfo& EnemyInfo, AUTBot* B)
{
	if ( EnemyInfo.GetUTChar() != NULL && EnemyInfo.GetUTChar()->GetCarriedObject() != NULL && EnemyInfo.GetUTChar()->GetCarriedObject()->GetTeamNum() == GetTeamNum() &&
		 B->CanAttack(EnemyInfo.GetPawn(), EnemyInfo.LastKnownLoc, false) )
	{
		if ( (B->GetPawn()->GetActorLocation() - EnemyInfo.LastKnownLoc).Size() < 3500.0f || (B->GetUTChar() != NULL && B->GetUTChar()->GetWeapon() != NULL && B->GetUTChar()->GetWeapon()->bSniping) ||
			IsNearEnemyBase(EnemyInfo.LastKnownLoc) )
		{
			return CurrentRating + 6.0f;
		}
		else
		{
			return CurrentRating + 1.5f;
		}
	}
	else
	{
		return CurrentRating;
	}
}

void AUTCTFSquadAI::DrawDebugSquadRoute(AUTBot* B) const
{
	if (B->GetUTChar() != NULL && B->GetUTChar()->GetCarriedObject() != NULL)
	{
		if (CapRoutes.IsValidIndex(B->UsingSquadRouteIndex))
		{
			DrawDebugRoute(GetWorld(), B->GetPawn(), CapRoutes[B->UsingSquadRouteIndex].RouteCache);
		}
	}
	else
	{
		Super::DrawDebugSquadRoute(B);
	}
}

bool AUTCTFSquadAI::TryPathTowardObjective(AUTBot* B, AActor* Goal, bool bAllowDetours, const FString& SuccessGoalString)
{
	// maintain a separate list of alternate routes for capturing the taken enemy flag (i.e. from enemy base to home base)
	if (Goal == FriendlyBase && FollowAlternateRoute(B, Goal, CapRoutes, bAllowDetours, SuccessGoalString))
	{
		return true;
	}
	else
	{
		bool bResult = Super::TryPathTowardObjective(B, Goal, bAllowDetours, SuccessGoalString);
		if (bResult && EnemyBase != NULL && (Goal == EnemyBase || Goal == EnemyBase->GetCarriedObject()) && B->GetRouteDist() < 2500 && B->LineOfSightTo(Goal))
		{
			B->SendVoiceMessage(StatusMessage::IGotFlag);
		}
		return bResult;
	}
}

bool AUTCTFSquadAI::SetFlagCarrierAction(AUTBot* B)
{
	if (FriendlyBase != NULL && FriendlyBase->GetCarriedObjectState() != CarriedObjectState::Home)
	{
		if (FriendlyBase->GetCarriedObjectState() == CarriedObjectState::Dropped && (FriendlyBase->GetCarriedObject()->GetActorLocation() - B->GetPawn()->GetActorLocation()).Size() < 3000.0f && B->LineOfSightTo(FriendlyBase->GetCarriedObject()))
		{
			return RecoverFriendlyFlag(B);
		}
		else if (HideTarget.IsValid() || (B->GetPawn()->GetActorLocation() - FriendlyBase->GetActorLocation()).Size() < 3000.0f)
		{
			float EnemyStrength = B->RelativeStrength(B->GetEnemy());
			if (B->GetEnemy() != NULL && EnemyStrength < 0.0f)
			{
				// fight enemy
				return false;
			}
			else if (HideTarget.IsValid())
			{
				if (StartHideTime == 0.0f && !NavData->HasReachedTarget(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), HideTarget))
				{
					FSingleEndpointEval NodeEval(HideTarget.GetLocation(NULL));
					float Weight = 0.0f;
					if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), NodeEval, B->GetNavAgentLocation(), Weight, true, B->RouteCache))
					{
						B->GoalString = "Hide";
						B->SetMoveTarget(B->RouteCache[0]);
						B->StartWaitForMove();
						return true;
					}
				}
				else
				{
					// TODO: maybe don't fire and try to hide even if visible enemy if they don't seem to have seen us?
					if (B->GetEnemy() == NULL || (!B->IsEnemyVisible(B->GetEnemy()) && GetWorld()->TimeSeconds - B->GetEnemyInfo(B->GetEnemy(), false)->LastHitByTime > 2.0f))
					{
						if (StartHideTime == 0.0f)
						{
							StartHideTime = GetWorld()->TimeSeconds;
							UsedHidingSpots.AddUnique(HideTarget.Node);
							if (UsedHidingSpots.Num() > 5)
							{
								UsedHidingSpots.RemoveAt(0);
							}
						}
						if (CheckSuperPickups(B, 5000))
						{
							return true;
						}
						else if (B->FindInventoryGoal(0.0003f))
						{
							B->GoalString = "Pick up item while hiding";
							B->SetMoveTarget(B->RouteCache[0]);
							B->StartWaitForMove();
							return true;
						}
						else if (!NavData->HasReachedTarget(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), HideTarget))
						{
							FSingleEndpointEval NodeEval(HideTarget.GetLocation(NULL));
							float Weight = 0.0f;
							if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), NodeEval, B->GetNavAgentLocation(), Weight, true, B->RouteCache))
							{
								B->GoalString = "Hide";
								B->SetMoveTarget(B->RouteCache[0]);
								B->StartWaitForMove();
								return true;
							}
							// else intentional fallthrough, path to HideTarget became invalid for some reason
						}
						else
						{
							if (B->GetCharacter() != NULL)
							{
								B->GetCharacter()->GetCharacterMovement()->bWantsToCrouch = (B->Skill > 2.0f);
							}
							B->GoalString = "Hide here";
							// TODO: Camp action
							B->SetMoveTarget(HideTarget);
							B->StartWaitForMove();
							B->SendVoiceMessage(StatusMessage::GetFlagBack);
							return true;
						}
					}
					else
					{
						HidingSpotDiscoveredTime = GetWorld()->TimeSeconds;
						// note that we only record fails in the learning data because we don't know how much longer we could have hidden
						if (StartHideTime > 0.0f && HideTarget.Node != NULL)
						{
							HideTarget.Node->AvgHideDuration = ((HideTarget.Node->AvgHideDuration * HideTarget.Node->HideAttempts) + (GetWorld()->TimeSeconds - StartHideTime)) / float(HideTarget.Node->HideAttempts + 1);
							HideTarget.Node->HideAttempts++;
						}
						HideTarget.Clear();
					}
				}
			}
			if (B->GetEnemy() != NULL && GetWorld()->TimeSeconds - HidingSpotDiscoveredTime < 3.0f)
			{
				// fight off enemy before trying new hiding spot
				return false;
			}
			// new hide target
			StartHideTime = 0.0f;
			FHideLocEval NodeEval(FMath::FRand() < (0.07f * B->Skill + 0.5f * B->Personality.MapAwareness), FSphere(FriendlyBase->GetActorLocation(), (EnemyBase->GetActorLocation() - FriendlyBase->GetActorLocation()).Size()));
			NodeEval.RejectNodes = UsedHidingSpots;
			float Weight = 0.0f;
			if (NavData->FindBestPath(B->GetPawn(), B->GetPawn()->GetNavAgentPropertiesRef(), NodeEval, B->GetNavAgentLocation(), Weight, true, B->RouteCache))
			{
				B->GoalString = "Hide";
				HideTarget = B->RouteCache.Last();
				B->SetMoveTarget(B->RouteCache[0]);
				B->StartWaitForMove();
				return true;
			}
			else
			{
				HideTarget.Clear();
				return false;
			}
		}
	}
	
	HideTarget.Clear();
	StartHideTime = 0.0f;
	// return to base
	bool bOnFriendlySide = (B->GetPawn()->GetActorLocation() - FriendlyBase->GetActorLocation()).Size() < (B->GetPawn()->GetActorLocation() - EnemyBase->GetActorLocation()).Size();
	if (bOnFriendlySide && FriendlyBase != NULL && FriendlyBase->GetCarriedObjectState() == CarriedObjectState::Home)
	{
		B->SendVoiceMessage(StatusMessage::DefendFlag);
	}
	// TODO: check super pickups? (at low range)
	bool bAllowDetours = (FriendlyBase != NULL && FriendlyBase->GetCarriedObjectState() != CarriedObjectState::Home) || bOnFriendlySide;
	return TryPathTowardObjective(B, FriendlyBase, bAllowDetours, "Return to base with enemy flag");
}

bool AUTCTFSquadAI::RecoverFriendlyFlag(AUTBot* B)
{
	bool bEnemyFlagOut = (EnemyBase == NULL || EnemyBase->GetCarriedObjectState() == CarriedObjectState::Home);
	AUTCharacter* EnemyCarrier = FriendlyBase->GetCarriedObject()->HoldingPawn;

	if (EnemyCarrier != NULL)
	{
		if (EnemyCarrier == B->GetEnemy())
		{
			B->GoalString = "Fight flag carrier";
			// fight enemy
			return false;
		}
		else
		{
			// TODO: FindPathToIntercept()? Maybe adjust hunting logic to not require hunting target == enemy and use that?
			return B->TryPathToward(EnemyCarrier, bEnemyFlagOut, "Hunt flag carrier");
		}
	}
	else
	{
		// TODO: model of where flag might be, search around for it
		return B->TryPathToward(FriendlyBase->GetCarriedObject(), bEnemyFlagOut, "Find dropped flag");
	}
}

bool AUTCTFSquadAI::CheckSquadObjectives(AUTBot* B)
{
	FName CurrentOrders = GetCurrentOrders(B);

	// TODO: will need to redirect to vehicle for VCTF
	if (B->GetUTChar() != NULL && B->GetUTChar()->GetCarriedObject() != NULL)
	{
		return SetFlagCarrierAction(B);
	}
	else if (CurrentOrders == NAME_Defend)
	{
		if (B->NeedsWeapon() && (GameObjective == NULL || GameObjective->GetCarriedObject() == NULL || (B->GetPawn()->GetActorLocation() - GameObjective->GetCarriedObject()->GetActorLocation()).Size() > 3000.0f) && B->FindInventoryGoal(0.0f))
		{
			B->GoalString = FString::Printf(TEXT("Get inventory %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
			B->SetMoveTarget(B->RouteCache[0]);
			B->StartWaitForMove();
			return true;
		}
		else if (GameObjective != NULL && GameObjective->GetCarriedObject() != NULL && GameObjective->GetCarriedObjectState() != CarriedObjectState::Home)
		{
			return RecoverFriendlyFlag(B);
		}
		else if (B->GetEnemy() != NULL)
		{
			if (!B->LostContact(3.0f) || !CheckSuperPickups(B, 5000))
			{
				B->GoalString = "Fight attacker";
				return false;
			}
			else
			{
				return true;
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
		else if (Objective != NULL)
		{
			// TODO: find defense point
			return B->TryPathToward(Objective, true, "Defend objective");
		}
		else
		{
			return false;
		}
	}
	else if (CurrentOrders == NAME_Attack)
	{
		if (B->NeedsWeapon() && B->FindInventoryGoal(0.0f))
		{
			B->GoalString = FString::Printf(TEXT("Get inventory %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
			B->SetMoveTarget(B->RouteCache[0]);
			B->StartWaitForMove();
			return true;
		}
		else if (FriendlyBase != NULL && FriendlyBase->GetCarriedObjectState() != CarriedObjectState::Home && B->LineOfSightTo(FriendlyBase->GetCarriedObject()))
		{
			return RecoverFriendlyFlag(B);
		}
		else if (GameObjective != NULL && GameObjective->GetCarriedObjectState() != CarriedObjectState::Home)
		{
			if (GameObjective->GetCarriedObjectState() == CarriedObjectState::Held)
			{
				// defend flag carrier
				if (B->GetEnemy() != NULL && B->LineOfSightTo(GameObjective->GetCarriedObject()->HoldingPawn))
				{
					return false;
				}
				else
				{
					return B->TryPathToward(GameObjective->GetCarriedObject()->HoldingPawn, true, "Find friendly flag carrier");
				}
			}
			else
			{
				return B->TryPathToward(GameObjective->GetCarriedObject(), false, "Go pickup flag");
			}
		}
		// prioritize grabbing some powerups just after respawning, less likely to do so after engaging enemy base
		// bias towards powerups more if less aggressive personality
		// skip if flag is out (prioritize getting hands on enemy flag ASAP)
		// note that even if this check never fires, standard pathing detours will still result in bots picking up powerups near their chosen assault path
		else if ( (FriendlyBase == NULL || FriendlyBase->GetCarriedObjectState() == CarriedObjectState::Home) &&
					((B->GetEnemy() == NULL && B->Personality.Aggressiveness <= 0.0f) || GetWorld()->TimeSeconds - B->LastRespawnTime < 10.0f * (1.0f - B->Personality.Aggressiveness)) &&
					CheckSuperPickups(B, 5000) )
		{
			return true;
		}
		else
		{
			return TryPathTowardObjective(B, Objective, true, "Attack objective");
		}
	}
	else if (Cast<APlayerController>(Leader) != NULL && Leader->GetPawn() != NULL)
	{
		if (B->NeedsWeapon() && B->FindInventoryGoal(0.0f))
		{
			B->GoalString = FString::Printf(TEXT("Get inventory %s"), *GetNameSafe(B->RouteCache.Last().Actor.Get()));
			B->SetMoveTarget(B->RouteCache[0]);
			B->StartWaitForMove();
			return true;
		}
		else if (B->GetEnemy() != NULL && !B->LostContact(3.0f))
		{
			// fight!
			return false;
		}
		else
		{
			return B->TryPathToward(Leader->GetPawn(), true, "Find leader");
		}
	}
	else
	{
		// TODO: midfield - engage enemies, acquire powerups, attack when stacked
		return Super::CheckSquadObjectives(B);
	}
}

void AUTCTFSquadAI::NotifyObjectiveEvent(AActor* InObjective, AController* InstigatedBy, FName EventName)
{
	AUTGameObjective* InGameObjective = Cast<AUTGameObjective>(InObjective);
	if (InstigatedBy != NULL && InGameObjective != NULL && InGameObjective->GetCarriedObject() != NULL && InGameObjective->GetCarriedObject()->Holder == InstigatedBy->PlayerState && Members.Contains(InstigatedBy))
	{
		// re-enable alternate paths for flag carrier so it can consider them for planning its escape
		AUTBot* B = Cast<AUTBot>(InstigatedBy);
		if (B != NULL)
		{
			B->bDisableSquadRoutes = false;
			B->SquadRouteGoal.Clear();
			B->UsingSquadRouteIndex = INDEX_NONE;
		}
		SetLeader(InstigatedBy);
	}
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
			else if (B->GetMoveTarget().Actor != NULL && (B->GetMoveTarget().Actor == InGameObjective || B->GetMoveTarget().Actor == InGameObjective->GetCarriedObject()))
			{
				SetRetaskTimer(B);
			}
		}
	}

	Super::NotifyObjectiveEvent(InObjective, InstigatedBy, EventName);
}

bool AUTCTFSquadAI::HasHighPriorityObjective(AUTBot* B)
{
	// if our flag is out and enemy's is safe, everyone needs to try to rectify that in some way or another
	if (FriendlyBase != NULL && EnemyBase != NULL && FriendlyBase->GetCarriedObjectState() != CarriedObjectState::Home && EnemyBase->GetCarriedObjectState() == CarriedObjectState::Home)
	{
		return true;
	}
	else
	{
		// otherwise only high priority if the flag this squad cares about is out
		return (GameObjective != NULL && GameObjective->GetCarriedObjectState() != CarriedObjectState::Home && GameObjective->GetCarriedObjectHolder() != B->PlayerState);
	}
}