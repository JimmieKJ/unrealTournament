// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTTeamDMGameMode.h"
#include "UTHUD_TeamDM.h"
#include "StatNames.h"
#include "UTCTFGameMessage.h"
#include "UTMutator.h"
#include "UTTeamScoreboard.h"

AUTTeamDMGameMode::AUTTeamDMGameMode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bScoreSuicides = true;
	bScoreTeamKills = true;
	bForceRespawn = true;
	GoalScore = 50;
	HUDClass = AUTHUD_TeamDM::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "TDM", "Team Deathmatch");
	XPMultiplier = 3.0f;
}

void AUTTeamDMGameMode::ScoreTeamKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	AUTPlayerState* KillerState = (Killer != NULL) ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
	AUTPlayerState* VictimState = (Other != NULL) ? Cast<AUTPlayerState>(Other->PlayerState) : NULL;
	if (VictimState && VictimState->Team && KillerState && KillerState->Team)
	{
		if (bScoreTeamKills)
		{
			int32 ScoreChange = -1;
			KillerState->AdjustScore(ScoreChange); // @TODO FIXMESTEVE track team kills
			KillerState->Team->Score += ScoreChange;
			KillerState->Team->ForceNetUpdate();
		}
	}

	AddKillEventToReplay(Killer, Other, DamageType);

	if (BaseMutator != NULL)
	{
		BaseMutator->ScoreKill(Killer, Other, DamageType);
	}
}

void AUTTeamDMGameMode::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
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
			if (!bHasBroadcastDominating)
			{
				int32 BestScore = 0;
				for (int32 i = 0; i < Teams.Num(); i++)
				{
					if ((Teams[i] != KillerState->Team) && (Teams[i]->Score >= BestScore))
					{
						BestScore = Teams[i]->Score;
					}
				}
				if (KillerState->Team->Score >= BestScore + 20)
				{
					bHasBroadcastDominating = true;
					BroadcastLocalized(this, UUTCTFGameMessage::StaticClass(), 10, KillerState, NULL, KillerState->Team);
				}
			}
		}
		else
		{
			VictimState->Team->Score += ScoreChange;
			VictimState->Team->ForceNetUpdate();
		}
	}

	Super::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);
}

AUTPlayerState* AUTTeamDMGameMode::IsThereAWinner_Implementation(bool& bTied)
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

		for (int32 PlayerIdx=0; PlayerIdx < UTGameState->PlayerArray.Num();PlayerIdx++)
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

void AUTTeamDMGameMode::UpdateSkillRating()
{
	for (int32 PlayerIdx = 0; PlayerIdx < UTGameState->PlayerArray.Num(); PlayerIdx++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
		if (PS && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(NAME_TDMSkillRating, PS->Team == UTGameState->WinningTeam, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}

	for (int32 PlayerIdx = 0; PlayerIdx < InactivePlayerArray.Num(); PlayerIdx++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(InactivePlayerArray[PlayerIdx]);
		if (PS && !PS->bOnlySpectator)
		{
			PS->UpdateTeamSkillRating(NAME_TDMSkillRating, PS->Team == UTGameState->WinningTeam, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}
}

int32 AUTTeamDMGameMode::GetEloFor(AUTPlayerState* PS, bool& bEloIsValid) const
{
	bEloIsValid = PS ? PS->bTDMEloValid : false;
	return PS ? PS->TDMRank : Super::GetEloFor(PS, bEloIsValid);
}

void AUTTeamDMGameMode::SetEloFor(AUTPlayerState* PS, int32 NewEloValue)
{
	if (PS)
	{
		PS->TDMRank = NewEloValue;
	}
}