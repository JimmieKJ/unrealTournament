// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamInfo.h"

AUTTeamInfo::AUTTeamInfo(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	SetReplicates(true);
	bAlwaysRelevant = true;
	NetUpdateFrequency = 1.0f;
	TeamIndex = 255; // invalid so we will always get ReceivedTeamIndex() on clients
	TeamColor = FLinearColor::White;
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

	DOREPLIFETIME_CONDITION(AUTTeamInfo, TeamIndex, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTTeamInfo, TeamColor, COND_InitialOnly);
	DOREPLIFETIME(AUTTeamInfo, bFromPreviousLevel);
	DOREPLIFETIME(AUTTeamInfo, Score);
}