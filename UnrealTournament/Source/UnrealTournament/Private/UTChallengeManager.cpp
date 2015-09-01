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

	PlayerTeamRoster = FTeamRoster(NAME_PlayerTeamRoster, NSLOCTEXT("Challenges","PlayersTeamRoster","Players Team"));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Taye")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Kragoth")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Othello")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Samael")));

	TArray<FTeamRoster> EnemyRosters;
	FTeamRoster EnemyRoster;

	// easy enemy team
	EnemyRoster = FTeamRoster(NAME_EnemyTeam_SkaarjDefault, NSLOCTEXT("Challenges","EnemyTeam_Skaarj_Default","Default Skaarj Team"));
	EnemyRoster.Roster.Add(FName(TEXT("Cadaver")));
	EnemyRoster.Roster.Add(FName(TEXT("Leeb")));
	EnemyRoster.Roster.Add(FName(TEXT("Genghis")));
	EnemyRoster.Roster.Add(FName(TEXT("Trollface")));
	EnemyRoster.Roster.Add(FName(TEXT("Malakai")));
	EnemyRosters.Add(EnemyRoster);

	Challenges.Add(NAME_ChallengeDM, FUTChallengeInfo(TEXT("Deathmatch Challenge"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),TEXT("DM"),TEXT("This is a test of the challenge system.  Steve replace me please with the actual text.  Note you can use various rich text statements in here.\n\nThis is only a test."), 4, EnemyRosters, NAME_ChallengeSlateBadgeName_DM));
	Challenges.Add(NAME_ChallengeCTF, FUTChallengeInfo(TEXT("CTF Challenge"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-Outside"),TEXT("CTF"),TEXT("This is a test of the challenge system part 2.  Steve replace me please with the actual text.  Note you can use various rich text statements in here."), 4, EnemyRosters, NAME_ChallengeSlateBadgeName_CTF));
}

UUTBotCharacter* UUTChallengeManager::ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum) const
{
	if ( CurrentGame->ChallengeTag != NAME_None && Challenges.Contains(CurrentGame->ChallengeTag) )
	{
		const FUTChallengeInfo* Challenge = Challenges.Find(CurrentGame->ChallengeTag);

		int32 EnemyIndex = CurrentGame->NumBots;
		if (CurrentGame->bTeamGame)
		{
			// fill player team first
			int32 PlayerTeamSize = FMath::Min(PlayerTeamRoster.Roster.Num(), Challenge->PlayerTeamSize);
			if (CurrentGame->NumBots < PlayerTeamSize)
			{
				TeamNum = 1;
				for (int32 i = 0; i < CurrentGame->EligibleBots.Num(); i++)
				{
					if (CurrentGame->EligibleBots[i]->GetFName() == PlayerTeamRoster.Roster[CurrentGame->NumBots])
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
			int32 RosterIndex = FMath::Clamp<int32>(CurrentGame->ChallengeDifficulty,0,Challenge->EnemyRosters.Num());
			if (RosterIndex < Challenge->EnemyRosters.Num() && CurrentGame->EligibleBots[i]->GetFName() == Challenge->EnemyRosters[RosterIndex].Roster[EnemyIndex])
			{
				return CurrentGame->EligibleBots[i];
			}
		}
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Challenge %s is Invalid"), *CurrentGame->ChallengeTag.ToString());
	}
	return NULL;
}

int32 UUTChallengeManager::GetNumPlayers(AUTGameMode* CurrentGame) const
{
	if (CurrentGame->ChallengeTag != NAME_None && Challenges.Contains(CurrentGame->ChallengeTag))
	{
		const FUTChallengeInfo* Challenge = Challenges.Find(CurrentGame->ChallengeTag);
		int32 RosterIndex = FMath::Clamp<int32>(CurrentGame->ChallengeDifficulty, 0, Challenge->EnemyRosters.Num());
		int32 EnemyTeamSize = (RosterIndex < Challenge->EnemyRosters.Num()) ? Challenge->EnemyRosters[RosterIndex].Roster.Num() : 0;
		int32 PlayerTeamSize = FMath::Min(PlayerTeamRoster.Roster.Num(), Challenge->PlayerTeamSize);
		return 1 + PlayerTeamSize + EnemyTeamSize;
	}
	return 1;
}

