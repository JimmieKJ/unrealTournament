// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAsymCTFSquadAI.h"
#include "UTCTFRoundGame.h"
#include "UTDefensePoint.h"

void AUTAsymCTFSquadAI::Initialize(AUTTeamInfo* InTeam, FName InOrders)
{
	Super::Initialize(InTeam, InOrders);

	AUTCTFRoundGame* Game = GetWorld()->GetAuthGameMode<AUTCTFRoundGame>();
	AUTCTFGameState* GS = GetWorld()->GetGameState<AUTCTFGameState>();
	if (GS != NULL && Game != NULL && GS->FlagBases.Num() >= 2 && GS->FlagBases[0] != NULL && GS->FlagBases[1] != NULL)
	{
		Objective = GS->FlagBases[Game->bRedToCap ? 1 : 0];
		Flag = GS->FlagBases[Game->bRedToCap ? 0 : 1]->GetCarriedObject();
	}
}

bool AUTAsymCTFSquadAI::IsAttackingTeam() const
{
	AUTCTFRoundGame* Game = GetWorld()->GetAuthGameMode<AUTCTFRoundGame>();
	return (Game != NULL && GetTeamNum() == (Game->bRedToCap ? 0 : 1));
}

bool AUTAsymCTFSquadAI::MustKeepEnemy(APawn* TheEnemy)
{
	// must keep enemy flag holder
	AUTCharacter* UTC = Cast<AUTCharacter>(TheEnemy);
	return (UTC != NULL && UTC->GetCarriedObject() != NULL);
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
	else if (Flag->HoldingPawn != NULL && IsAttackingTeam() && (Flag->HoldingPawn->LastHitBy == EnemyInfo.GetPawn()->GetController() || !GetWorld()->LineTraceTestByChannel(EnemyInfo.LastKnownLoc, Flag->HoldingPawn->GetActorLocation(), ECC_Pawn, FCollisionQueryParams::DefaultQueryParam, WorldResponseParams)))
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

bool AUTAsymCTFSquadAI::SetFlagCarrierAction(AUTBot* B)
{
	// TODO: wait for allies, maybe double back, etc
	return TryPathTowardObjective(B, Objective, false, "Head to enemy base with flag");
}

void AUTAsymCTFSquadAI::GetPossibleEnemyGoals(AUTBot* B, const FBotEnemyInfo* EnemyInfo, TArray<FVector>& Goals)
{
	Super::GetPossibleEnemyGoals(B, EnemyInfo, Goals);
	if (!IsAttackingTeam() && Flag != NULL && Objective != NULL)
	{
		if (Flag->HoldingPawn == NULL)
		{
			Goals.Add(Flag->GetActorLocation());
		}
		Goals.Add(Objective->GetActorLocation());
	}
}

bool AUTAsymCTFSquadAI::HuntEnemyFlag(AUTBot* B)
{
	AUTCharacter* EnemyCarrier = Flag->HoldingPawn;
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
			B->GoalString = "Hunt down enemy flag carrier";
			B->DoHunt(EnemyCarrier);
			return true;
		}
	}
	else if ((Flag->GetActorLocation() - B->GetPawn()->GetActorLocation()).Size() < 3000.0f && B->UTLineOfSightTo(Flag))
	{
		// fight/camp here
		return false;
	}
	else
	{
		return B->TryPathToward(Flag, true, "Camp dropped flag");
	}
}

bool AUTAsymCTFSquadAI::CheckSquadObjectives(AUTBot* B)
{
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
		else if (Flag->HoldingPawn == NULL) 
		{
			return B->TryPathToward(Flag, true, "Get flag");
		}
		else if (CurrentOrders == NAME_Defend)
		{
			if ((B->GetPawn()->GetActorLocation() - Flag->HoldingPawn->GetActorLocation()).Size() < 2000.0f)
			{
				return false; // fight enemies around FC
			}
			else
			{
				return B->TryPathToward(Flag->HoldingPawn, true, "Find flag carrier");
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
			else
			{
				return B->TryPathToward(Flag->HoldingPawn, true, "Find flag carrier");
			}
		}
	}
	else
	{
		if (CurrentOrders == NAME_Defend)
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
		else if (CurrentOrders == NAME_Defend)	
		{
			if (B->GetEnemy() != NULL)
			{
				if (!B->LostContact(3.0f) || MustKeepEnemy(B->GetEnemy()))
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
					return B->TryPathToward(B->GetDefensePoint(), true, "Go to defense point");
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
				return B->TryPathToward(B->GetDefensePoint(), true, "Go to defense point");
			}
			else if (Objective != NULL)
			{
				return B->TryPathToward(Objective, true, "Defend objective");
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (B->GetEnemy() != NULL && MustKeepEnemy(B->GetEnemy()) && !B->LostContact(2.0f))
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

void AUTAsymCTFSquadAI::NotifyObjectiveEvent(AActor* InObjective, AController* InstigatedBy, FName EventName)
{
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
	return ((B->GetUTChar() != NULL && B->GetUTChar()->GetCarriedObject() != NULL) || (B->GetEnemy() != NULL && MustKeepEnemy(B->GetEnemy())));
}