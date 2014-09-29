// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTTeamDMGameMode.h"
#include "UTHUD_TeamDM.h"

AUTTeamDMGameMode::AUTTeamDMGameMode(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bScoreSuicides = true;
	bScoreTeamKills = true;
	GoalScore = 50;
	HUDClass = AUTHUD_TeamDM::StaticClass();
	FriendlyGameName = TEXT("TDM");
}

void AUTTeamDMGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bOnlyTheStrongSurvive = false;
}


void AUTTeamDMGameMode::ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType)
{
	AUTPlayerState* KillerState = (Killer != NULL) ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
	AUTPlayerState* VictimState = (Other != NULL) ? Cast<AUTPlayerState>(Other->PlayerState) : NULL;
	if (VictimState != NULL && VictimState->Team != NULL)
	{
		int32 ScoreChange = 0;
		if (Killer == NULL || Killer == Other)
		{
			if (bScoreSuicides)
			{
				ScoreChange = -1;
			}
		}
		else if (Cast<AUTGameState>(GameState)->OnSameTeam(Killer, Other))
		{
			if (bScoreTeamKills)
			{
				ScoreChange = -1;
			}
		}
		else
		{
			ScoreChange = 1;
		}

		if (KillerState != NULL && KillerState->Team != NULL)
		{
			KillerState->Team->Score += ScoreChange;
			KillerState->Team->ForceNetUpdate();
		}
		else
		{
			VictimState->Team->Score += ScoreChange;
			VictimState->Team->ForceNetUpdate();
		}
	}

	Super::ScoreKill(Killer, Other, DamageType);
}

AUTPlayerState* AUTTeamDMGameMode::IsThereAWinner(uint32& bTied)
{
	AUTTeamInfo* BestTeam = NULL;
	bTied = false;
	for (int32 i=0; i < UTGameState->Teams.Num(); i++)
	{
		if (UTGameState->Teams[i] != NULL)
		{
			if (BestTeam == NULL || UTGameState->Teams[i]->Score > BestTeam->Score)
			{
				BestTeam = UTGameState->Teams[i];
				bTied = false;
			}
			else if (UTGameState->Teams[i]->Score == BestTeam->Score)
			{
				bTied = true;
			}
		}
	}

	AUTPlayerState* BestPlayer = NULL;
	if (!bTied)
	{
		float BestScore = 0.0;

		for (int PlayerIdx=0; PlayerIdx < UTGameState->PlayerArray.Num();PlayerIdx++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
			if (PS != NULL && PS->Team == BestTeam)
			{
				if (BestPlayer == NULL || PS->Score > BestScore)
				{
					BestPlayer = PS;
					BestScore = BestPlayer->Score;
				}
			}
		}
	}

	return BestPlayer;

}
