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