// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChallengeManager.h"
#include "UTBotCharacter.h"

UUTChallengeManager::UUTChallengeManager(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PlayerTeamRoster.Add(FName(TEXT("Taye")));
	PlayerTeamRoster.Add(FName(TEXT("Kragoth")));
	PlayerTeamRoster.Add(FName(TEXT("Othello")));
	PlayerTeamRoster.Add(FName(TEXT("Samael")));
}

UUTBotCharacter* UUTChallengeManager::ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum) const
{
	if (CurrentGame->bTeamGame)
	{
		// fill player team first
		if (CurrentGame->NumBots < 4)
		{
			TeamNum = 1;
			for (int32 i = 0; i < CurrentGame->EligibleBots.Num(); i++)
			{
				if (CurrentGame->EligibleBots[i]->GetFName() == PlayerTeamRoster[CurrentGame->NumBots])
				{
					return CurrentGame->EligibleBots[i];
				}
			}
		}
	}
	return NULL;
}

