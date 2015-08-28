// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChallengeManager.h"
#include "UTBotCharacter.h"

/*
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Trollface
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Judas
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Damian
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Skirge
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Deslok
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Karag
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Genghis
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Cadaver
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Taye
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Leeb
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Othello
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Kragoth
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Grail
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Riker
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Thannis
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Barktooth
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Malakai
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Picard
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Jakob
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Necroth
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Lauren
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Loque
[2015.08.28-20.10.44:299][  0]UT:Warning: Found Malcolm
*/

UUTChallengeManager::UUTChallengeManager(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// @TODO FIXMESTEVE build fnames for all bots in .h
	PlayerTeamRoster.Add(FName(TEXT("Taye")));
	PlayerTeamRoster.Add(FName(TEXT("Kragoth")));
	PlayerTeamRoster.Add(FName(TEXT("Othello")));
	PlayerTeamRoster.Add(FName(TEXT("Samael")));

	Challenges.Add(FChallengeInfo());
	Challenges[0].EnemyRoster.Add(FName(TEXT("Cadaver")));
	Challenges[0].EnemyRoster.Add(FName(TEXT("Leeb")));
	Challenges[0].EnemyRoster.Add(FName(TEXT("Genghis")));
	Challenges[0].EnemyRoster.Add(FName(TEXT("Trollface")));
	Challenges[0].EnemyRoster.Add(FName(TEXT("Malakai")));
}

UUTBotCharacter* UUTChallengeManager::ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum) const
{
	if (CurrentGame->ChallengeIndex > Challenges.Num() - 1)
	{
		UE_LOG(UT, Warning, TEXT("INVALID CHALLENGE INDEX"));
		return NULL;
	}
	int32 EnemyIndex = CurrentGame->NumBots;
	if (CurrentGame->bTeamGame)
	{
		// fill player team first
		int32 PlayerTeamSize = FMath::Min(PlayerTeamRoster.Num(), Challenges[CurrentGame->ChallengeIndex].PlayerTeamSize);
		if (CurrentGame->NumBots < PlayerTeamSize)
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
		EnemyIndex -= PlayerTeamSize;
	}

	// fill enemy team
	TeamNum = 0;
	for (int32 i = 0; i < CurrentGame->EligibleBots.Num(); i++)
	{
		if (CurrentGame->EligibleBots[i]->GetFName() == Challenges[CurrentGame->ChallengeIndex].EnemyRoster[EnemyIndex])
		{
			return CurrentGame->EligibleBots[i];
		}
	}

	return NULL;
}

