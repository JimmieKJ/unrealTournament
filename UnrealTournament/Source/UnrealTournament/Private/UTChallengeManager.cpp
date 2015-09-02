// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChallengeManager.h"
#include "UTBotCharacter.h"

/*
[2015.09.01-21.14.33:684][ 67]UT:Warning: Riker Skill 4.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male03.TC_Male03_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Thannis Skill 4.500000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male02.TC_Male02_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Garog Skill 4.500000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Alanna Skill 5.000000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Torgr Skill 5.000000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
*/
/*
1/3.5/6

15 challenges, unlock new character every 5.  Challenges ramp up enemy skill along the way

1v1 
Kali 1.2
Harbinger 2.0
Othello 4.0
Harbinger 4.5
Jakob 6.0
Loque 7.5

FFA Easy 
Genghis 2.5
Kali 1.2
Acolyte 0.5
Judas 1.0
Guardian 2.0 ++

FFA Medium 
Genghis  2.5
Drekorig 3.2
Cadaver 3.0
Kryss 4.0
Othello  4.0

FFA Hard
Freylis 6.0
Jakob 6.0
Picard 5.0
Skakruk 7.0
Necroth 6.0

Medium TC/Skaarj
Genghis  2.5
Drekorig 3.2
Leeb     3.5
Othello  4.0
Gaargod  4.5


Hard Skaarj/TC
[2015.09.01-21.14.33:685][ 67]UT:Warning: Clanlord Skill 7.500000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Skakruk Skill 7.000000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Malcolm Skill 7.900000 character /Game/RestrictedAssets/Character/Malcom_New/Malcolm_New.Malcolm_New_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Jakob Skill 6.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male05.TC_Male05_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Picard Skill 5.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male02.TC_Male02_C

rename Kryss (too close to Cryss) - new Solace

remove Lauren
Get rid of UTBotConfig ini
*/

UUTChallengeManager::UUTChallengeManager(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// @TODO FIXMESTEVE build fnames for all bots in .h

	// player team roster, includes roster upgrades
	PlayerTeamRoster = FTeamRoster(NSLOCTEXT("Challenges","PlayersTeamRoster","Players Team"));
	PlayerTeamRoster.Roster.Add(NAME_Damian);
	PlayerTeamRoster.Roster.Add(NAME_Skirge);
	PlayerTeamRoster.Roster.Add(NAME_Arachne);
	PlayerTeamRoster.Roster.Add(NAME_Kraagesh);

	PlayerTeamRoster.Roster.Add(NAME_Samael);
	PlayerTeamRoster.Roster.Add(NAME_Taye);
	PlayerTeamRoster.Roster.Add(NAME_Malise);
	PlayerTeamRoster.Roster.Add(NAME_Gkoblok);

	PlayerTeamRoster.Roster.Add(NAME_Grail);
	PlayerTeamRoster.Roster.Add(NAME_Barktooth);
	PlayerTeamRoster.Roster.Add(NAME_Raven);
	PlayerTeamRoster.Roster.Add(NAME_Dominator);

	// enemy team rosters
	FTeamRoster EasyNecrisRoster = FTeamRoster(NSLOCTEXT("Challenges","EasyEnemyNecris","Necris Recruit Team"));
	EasyNecrisRoster.Roster.Add(NAME_Acolyte);
	EasyNecrisRoster.Roster.Add(NAME_Judas);
	EasyNecrisRoster.Roster.Add(NAME_Kali);
	EasyNecrisRoster.Roster.Add(NAME_Harbinger);
	EasyNecrisRoster.Roster.Add(NAME_Nocturne);
	EnemyTeamRosters.Add(NAME_EasyNecrisTeam, EasyNecrisRoster);

	FTeamRoster MediumNecrisRoster = FTeamRoster(NSLOCTEXT("Challenges", "MediumEnemyNecris", "Necris Adept Team"));
	MediumNecrisRoster.Roster.Add(NAME_Cadaver);
	MediumNecrisRoster.Roster.Add(NAME_Cryss);
	MediumNecrisRoster.Roster.Add(NAME_Kragoth);
	MediumNecrisRoster.Roster.Add(NAME_Kryss);
	MediumNecrisRoster.Roster.Add(NAME_Malakai);
	EnemyTeamRosters.Add(NAME_MediumNecrisTeam, MediumNecrisRoster);

	FTeamRoster HardNecrisRoster = FTeamRoster(NSLOCTEXT("Challenges", "HardEnemyNecris", "Necris Inhuman Team"));
	HardNecrisRoster.Roster.Add(NAME_Loque);
	HardNecrisRoster.Roster.Add(NAME_Freylis);
	HardNecrisRoster.Roster.Add(NAME_Necroth);
	HardNecrisRoster.Roster.Add(NAME_Visse);
	HardNecrisRoster.Roster.Add(NAME_Malakai);
	EnemyTeamRosters.Add(NAME_HardNecrisTeam, HardNecrisRoster);

	Challenges.Add(NAME_ChallengeDM, 
		FUTChallengeInfo(TEXT("Deathmatch Challenge"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("DM"),
		TEXT("This is a test of the challenge system.  Steve replace me please with the actual text.  Note you can use various rich text statements in here.\n\nThis is only a test."), 
		4, 5, NAME_EasyNecrisTeam, NAME_EasyNecrisTeam, NAME_EasyNecrisTeam, NAME_ChallengeSlateBadgeName_DM));
	Challenges.Add(NAME_ChallengeCTF, 
		FUTChallengeInfo(TEXT("CTF Challenge"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-Outside"),
		TEXT("CTF"),
		TEXT("This is a test of the challenge system part 2.  Steve replace me please with the actual text.  Note you can use various rich text statements in here."), 
		4, 5, NAME_EasyNecrisTeam, NAME_EasyNecrisTeam, NAME_EasyNecrisTeam, NAME_ChallengeSlateBadgeName_CTF));
}

UUTBotCharacter* UUTChallengeManager::ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum, int32 TotalStars) const
{
	if ( CurrentGame->ChallengeTag != NAME_None && Challenges.Contains(CurrentGame->ChallengeTag) )
	{
		const FUTChallengeInfo* Challenge = Challenges.Find(CurrentGame->ChallengeTag);

		int32 EnemyIndex = CurrentGame->NumBots;
		if (CurrentGame->bTeamGame)
		{
			// fill player team first
			int32 PlayerTeamSize = FMath::Min(PlayerTeamRoster.Roster.Num(), Challenge->PlayerTeamSize);
			int32 PlayerTeamIndex = CurrentGame->NumBots + FMath::Min(TotalStars / 5, 8);
			if (CurrentGame->NumBots < PlayerTeamSize)
			{
				TeamNum = 1;
				for (int32 i = 0; i < CurrentGame->EligibleBots.Num(); i++)
				{
					if (CurrentGame->EligibleBots[i]->GetFName() == PlayerTeamRoster.Roster[PlayerTeamIndex])
					{
						return CurrentGame->EligibleBots[i];
					}
				}
			}
			EnemyIndex -= PlayerTeamSize;
		}

		// fill enemy team
		TeamNum = 0;
		int32 RosterIndex = FMath::Clamp<int32>(CurrentGame->ChallengeDifficulty, 0, 2);
		if (EnemyTeamRosters.Contains(Challenge->EnemyTeamName[RosterIndex]))
		{
			const FTeamRoster* EnemyRoster = EnemyTeamRosters.Find(Challenge->EnemyTeamName[RosterIndex]);
			for (int32 i = 0; i < CurrentGame->EligibleBots.Num(); i++)
			{
				if (CurrentGame->EligibleBots[i]->GetFName() == EnemyRoster->Roster[EnemyIndex])
				{
					return CurrentGame->EligibleBots[i];
				}
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
		return 1 + Challenge->PlayerTeamSize + Challenge->EnemyTeamSize;
	}
	return 1;
}

bool UUTChallengeManager::IsValidChallenge(AUTGameMode* CurrentGame, const FString& MapName) const
{
	if (CurrentGame->ChallengeTag != NAME_None && Challenges.Contains(CurrentGame->ChallengeTag))
	{
		const FUTChallengeInfo* Challenge = Challenges.Find(CurrentGame->ChallengeTag);

		/** @TODO FIXMESTEVE
		// verify gametype and map matches challenge
		UE_LOG(UT, Warning, TEXT("Challenge in %s should be %s"), *MapName, *Challenge->Map);

		if (CurrentGame->GetClass()->GetFullName() != CurrentGame->StaticGetFullGameClassName(Challenge->GameMode))
		{
			UE_LOG(UT, Warning, TEXT("CHALLENGE FAILED - Challenge game %s should be %s"), *CurrentGame->GetClass()->GetFullName(), *CurrentGame->StaticGetFullGameClassName(Challenge->GameMode));
			return false;
		}*/
		return true;
	}
	UE_LOG(UT, Warning, TEXT("FAILED TO FIND MATCHING CHALLENGE"));
	return false;
}

