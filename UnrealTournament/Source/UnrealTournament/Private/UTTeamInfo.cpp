// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamInfo.h"
#include "UnrealNetwork.h"
#include "UTBot.h"
#include "UTSquadAI.h"

AUTTeamInfo::AUTTeamInfo(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetReplicates(true);
	bAlwaysRelevant = true;
	NetUpdateFrequency = 1.0f;
	TeamIndex = 255; // invalid so we will always get ReceivedTeamIndex() on clients
	TeamColor = FLinearColor::White;
	DefaultOrderIndex = -1;
	DefaultOrders.Add(FName(TEXT("Attack")));
	DefaultOrders.Add(FName(TEXT("Defend")));
}

void AUTTeamInfo::AddToTeam(AController* C)
{
	if (C != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
		if (PS != NULL)
		{
			if (PS->Team != NULL)
			{
				RemoveFromTeam(C);
			}
			PS->Team = this;
			PS->NotifyTeamChanged();
			TeamMembers.Add(C);
		}
	}
}

void AUTTeamInfo::RemoveFromTeam(AController* C)
{
	if (C != NULL && TeamMembers.Contains(C))
	{
		TeamMembers.Remove(C);
		// remove from squad
		AUTBot* B = Cast<AUTBot>(C);
		if (B != NULL && B->GetSquad() != NULL)
		{
			B->GetSquad()->RemoveMember(B);
		}
		// TODO: human player squads
		AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
		if (PS != NULL)
		{
			PS->Team = NULL;
			PS->NotifyTeamChanged();
		}
	}
}

void AUTTeamInfo::ReceivedTeamIndex()
{
	if (!bFromPreviousLevel && TeamIndex != 255)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			if (GS->Teams.Num() <= TeamIndex)
			{
				GS->Teams.SetNum(TeamIndex + 1);
			}
			GS->Teams[TeamIndex] = this;
		}
	}
}

void AUTTeamInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTTeamInfo, TeamName, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTTeamInfo, TeamIndex, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTTeamInfo, TeamColor, COND_InitialOnly);
	DOREPLIFETIME(AUTTeamInfo, bFromPreviousLevel);
	DOREPLIFETIME(AUTTeamInfo, Score);
}

void AUTTeamInfo::UpdateEnemyInfo(APawn* NewEnemy, EAIEnemyUpdateType UpdateType)
{
	if (NewEnemy != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(NewEnemy, this))
		{
			bool bFound = false;
			for (int32 i = 0; i < EnemyList.Num(); i++)
			{
				if (!EnemyList[i].IsValid(this))
				{
					EnemyList.RemoveAt(i--, 1);
				}
				else if (EnemyList[i].GetPawn() == NewEnemy)
				{
					EnemyList[i].Update(UpdateType);
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				new(EnemyList) FBotEnemyInfo(NewEnemy, UpdateType);
				// tell bots on team to consider new enemy
				/* TODO: notify squads, let it decide if this enemy is worth disrupting bots for
				for (AController* Member : TeamMembers)
				{
					AUTBot* B = Cast<AUTBot>(Member);
					if (B != NULL)
					{
						B->PickNewEnemy();
					}
				}*/
			}
		}
	}
}

bool AUTTeamInfo::AssignToSquad(AController* C, FName Orders, AController* Leader)
{
	AUTSquadAI* NewSquad = NULL;
	for (int32 i = 0; i < Squads.Num(); i++)
	{
		if (Squads[i] == NULL || Squads[i]->bPendingKillPending)
		{
			Squads.RemoveAt(i--);
		}
		else if (Squads[i]->Orders == Orders && (Leader == NULL || Squads[i]->GetLeader() == Leader) && (Leader != NULL || Squads[i]->GetSize() < GetWorld()->GetAuthGameMode<AUTGameMode>()->MaxSquadSize))
		{
			NewSquad = Squads[i];
			break;
		}
	}
	if (NewSquad == NULL && (Leader == NULL || Leader == C))
	{
		NewSquad = GetWorld()->SpawnActor<AUTSquadAI>(GetWorld()->GetAuthGameMode<AUTGameMode>()->SquadType);
		Squads.Add(NewSquad);
		NewSquad->Initialize(this, Orders);
	}
	if (NewSquad == NULL)
	{
		return false;
	}
	else
	{
		// assign squad
		AUTBot* B = Cast<AUTBot>(C);
		if (B != NULL)
		{
			B->SetSquad(NewSquad);
		}
		else
		{
			// TODO: playercontroller
		}
		return true;
	}
}

void AUTTeamInfo::AssignDefaultSquadFor(AController* C)
{
	if (Cast<AUTBot>(C) != NULL)
	{
		if (DefaultOrders.Num() > 0)
		{
			DefaultOrderIndex = (DefaultOrderIndex + 1) % DefaultOrders.Num();
			AssignToSquad(C, DefaultOrders[DefaultOrderIndex]);
		}
		else
		{
			AssignToSquad(C, NAME_None);
		}
	}
	else
	{
		// TODO: playercontroller
	}
}

void AUTTeamInfo::NotifyObjectiveEvent(AActor* InObjective, AController* InstigatedBy, FName EventName)
{
	for (AUTSquadAI* Squad : Squads)
	{
		// by default just notify all squads and let them filter
		if (Squad != NULL && !Squad->bPendingKillPending)
		{
			Squad->NotifyObjectiveEvent(InObjective, InstigatedBy, EventName);
		}
	}
}

void AUTTeamInfo::ReinitSquads()
{
	for (AUTSquadAI* Squad : Squads)
	{
		Squad->Initialize(this, Squad->Orders);
	}
}
