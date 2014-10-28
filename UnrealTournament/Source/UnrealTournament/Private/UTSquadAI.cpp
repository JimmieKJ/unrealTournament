// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTSquadAI.h"

AUTSquadAI::AUTSquadAI(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
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

bool AUTSquadAI::CheckSquadObjectives(AUTBot* B)
{
	return false;
}

bool AUTSquadAI::PickRetreatDestination(AUTBot* B)
{
	if (B->FindInventoryGoal(0.0f))
	{
		B->SetMoveTarget(B->RouteCache[0]);
		return true;
	}
	// keep moving to previous retreat destination if possible (don't oscillate)
	else if (B->RouteCache.Num() > 1 && NavData->HasReachedTarget(B->GetPawn(), *B->GetPawn()->GetNavAgentProperties(), B->RouteCache[0]))
	{
		B->RouteCache.RemoveAt(0);
		B->SetMoveTarget(B->RouteCache[0]);
		return true;
	}
	else
	{
		FRandomDestEval NodeEval;
		float Weight = 0.0f;
		if (NavData->FindBestPath(B->GetPawn(), *B->GetPawn()->GetNavAgentProperties(), NodeEval, B->GetNavAgentLocation(), Weight, false, B->RouteCache))
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