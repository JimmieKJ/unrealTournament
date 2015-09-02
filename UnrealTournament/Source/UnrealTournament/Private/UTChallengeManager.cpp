// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChallengeManager.h"
#include "UTBotCharacter.h"

/*
[2015.09.01-21.14.33:684][ 67]UT:Warning: Trollface Skill 0.000000 character
[2015.09.01-21.14.33:684][ 67]UT:Warning: Judas Skill 1.000000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale03.NecrisMale03_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Skirge Skill 1.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male01.TC_Male01_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Damian Skill 1.000000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale04.NecrisMale04_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Arachne Skill 1.200000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Kraagesh Skill 1.500000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Guardian Skill 2.000000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Harbinger Skill 2.000000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale05.NecrisMale05_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Nocturne Skill 2.000000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Deslok Skill 2.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male04.TC_Male04_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Karag Skill 2.000000 character
[2015.09.01-21.14.33:684][ 67]UT:Warning: Genghis Skill 2.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male03.TC_Male03_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Samael Skill 2.500000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale01.NecrisMale01_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Cadaver Skill 3.000000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale04.NecrisMale04_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Cryss Skill 3.000000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Taye Skill 3.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male01.TC_Male01_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Malise Skill 3.000000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Drekorig Skill 3.200000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Leeb Skill 3.500000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male02.TC_Male02_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Gkoblok Skill 4.000000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Othello Skill 4.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male05.TC_Male05_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Kryss Skill 4.000000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Riker Skill 4.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male03.TC_Male03_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Kragoth Skill 4.000000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale02.NecrisMale02_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Grail Skill 4.000000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale03.NecrisMale03_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Gaargod Skill 4.500000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Thannis Skill 4.500000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male02.TC_Male02_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Garog Skill 4.500000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Alanna Skill 5.000000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Malakai Skill 5.000000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale01.NecrisMale01_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Barktooth Skill 5.000000 character
[2015.09.01-21.14.33:684][ 67]UT:Warning: Picard Skill 5.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male02.TC_Male02_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Torgr Skill 5.000000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:684][ 67]UT:Warning: Raven Skill 5.300000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Jakob Skill 6.000000 character /Game/EpicInternal/Teams/ThunderCrash/TC_Male05.TC_Male05_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Freylis Skill 6.000000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Dominator Skill 6.000000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Necroth Skill 6.000000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale03.NecrisMale03_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Lauren Skill 6.500000 character
[2015.09.01-21.14.33:685][ 67]UT:Warning: Visse Skill 6.800000 character /Game/RestrictedAssets/Character/Human/Female/FemaleNecrisCharacter.FemaleNecrisCharacter_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Skakruk Skill 7.000000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Loque Skill 7.500000 character /Game/EpicInternal/Teams/BlackLegion/NecrisMale01.NecrisMale01_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Clanlord Skill 7.500000 character /Game/RestrictedAssets/Character/Skaarji/SkaarjiCharacterContent.SkaarjiCharacterContent_C
[2015.09.01-21.14.33:685][ 67]UT:Warning: Malcolm Skill 7.900000 character /Game/RestrictedAssets/Character/Malcom_New/Malcolm_New.Malcolm_New_C
*/
/*
1/3.5/6

15 challenges, unlock new character every 5.  Challenges ramp up enemy skill along the way

Damian    1.0   NecrisM  (become 0.5)
Skirge    1.0   TC
Arachne   1.2   NecrisF
Kraagesh  1.5   Skaarj

Samael    2.5   NecrisM
Taye      3.0   TC
Malisse   3.0   NecrisF
Gkoblok   4.0   Skaarj

Grail     4.0   NecrisM  (Become 4.3)
Barktooth 5.0
Raven     5.3            (Become 5.5)
Dominator 6.0
*/

UUTChallengeManager::UUTChallengeManager(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// @TODO FIXMESTEVE build fnames for all bots in .h

	PlayerTeamRoster = FTeamRoster(NSLOCTEXT("Challenges","PlayersTeamRoster","Players Team"));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Damian")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Skirge")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Arachne")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Kraagesh")));

	PlayerTeamRoster.Roster.Add(FName(TEXT("Samael")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Taye")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Malise")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Gkoblok")));

	PlayerTeamRoster.Roster.Add(FName(TEXT("Grail")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Barktooth")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Raven")));
	PlayerTeamRoster.Roster.Add(FName(TEXT("Dominator")));

	FTeamRoster EnemyRoster;

	// easy enemy team
	EnemyRoster = FTeamRoster(NSLOCTEXT("Challenges","EnemyTeam_Skaarj_Default","Default Skaarj Team"));
	EnemyRoster.Roster.Add(FName(TEXT("Cadaver")));
	EnemyRoster.Roster.Add(FName(TEXT("Leeb")));
	EnemyRoster.Roster.Add(FName(TEXT("Genghis")));
	EnemyRoster.Roster.Add(FName(TEXT("Trollface")));
	EnemyRoster.Roster.Add(FName(TEXT("Malakai")));
	EnemyTeamRosters.Add(NAME_EnemyTeam_SkaarjDefault, EnemyRoster);

	Challenges.Add(NAME_ChallengeDM, 
		FUTChallengeInfo(TEXT("Deathmatch Challenge"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("DM"),
		TEXT("This is a test of the challenge system.  Steve replace me please with the actual text.  Note you can use various rich text statements in here.\n\nThis is only a test."), 
		4, 5, NAME_EnemyTeam_SkaarjDefault, NAME_EnemyTeam_SkaarjDefault, NAME_EnemyTeam_SkaarjDefault, NAME_ChallengeSlateBadgeName_DM));
	Challenges.Add(NAME_ChallengeCTF, 
		FUTChallengeInfo(TEXT("CTF Challenge"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-Outside"),
		TEXT("CTF"),
		TEXT("This is a test of the challenge system part 2.  Steve replace me please with the actual text.  Note you can use various rich text statements in here."), 
		4, 5, NAME_EnemyTeam_SkaarjDefault, NAME_EnemyTeam_SkaarjDefault, NAME_EnemyTeam_SkaarjDefault, NAME_ChallengeSlateBadgeName_CTF));
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

