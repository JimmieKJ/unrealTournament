// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChallengeManager.h"
#include "UTBotCharacter.h"

/*
15 challenges, unlock new character every 5.  Challenges ramp up enemy skill along the way

1v1 
Kali 1.2
Harbinger 2.0
Othello 4.0
Harbinger 4.5
Jakob 6.0
Loque 7.5

rename Kryss (too close to Cryss) - new Solace
remove Lauren
Get rid of UTBotConfig ini
*/

UUTChallengeManager::UUTChallengeManager(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// @TODO FIXMESTEVE make this a blueprint

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

	FTeamRoster MediumMixedRoster = FTeamRoster(NSLOCTEXT("Challenges", "MediumEnemyMixed", "Mixed Adept Team"));
	MediumMixedRoster.Roster.Add(NAME_Genghis);
	MediumMixedRoster.Roster.Add(NAME_Drekorig);
	MediumMixedRoster.Roster.Add(NAME_Leeb);
	MediumMixedRoster.Roster.Add(NAME_Othello);
	MediumMixedRoster.Roster.Add(NAME_Gaargod);
	EnemyTeamRosters.Add(NAME_MediumMixedTeam, MediumMixedRoster);

	FTeamRoster HardMixedRoster = FTeamRoster(NSLOCTEXT("Challenges", "HardEnemyMixed", "Mixed Inhuman Team"));
	HardMixedRoster.Roster.Add(NAME_Clanlord);
	HardMixedRoster.Roster.Add(NAME_Skakruk);
	HardMixedRoster.Roster.Add(NAME_Malcolm);
	HardMixedRoster.Roster.Add(NAME_Jakob);
	HardMixedRoster.Roster.Add(NAME_Picard);
	EnemyTeamRosters.Add(NAME_HardMixedTeam, HardMixedRoster);

	FTeamRoster EasyFFARoster = FTeamRoster(NSLOCTEXT("Challenges", "EasyFFAMixed", "Easy FFA"));
	EasyFFARoster.Roster.Add(NAME_Genghis);
	EasyFFARoster.Roster.Add(NAME_Kali);
	EasyFFARoster.Roster.Add(NAME_Acolyte);
	EasyFFARoster.Roster.Add(NAME_Judas);
	EasyFFARoster.Roster.Add(NAME_Guardian);
	EnemyTeamRosters.Add(NAME_EasyFFATeam, EasyFFARoster);

	FTeamRoster MediumFFARoster = FTeamRoster(NSLOCTEXT("Challenges", "MediumFFAMixed", "Medium FFA"));
	MediumFFARoster.Roster.Add(NAME_Genghis);
	MediumFFARoster.Roster.Add(NAME_Drekorig);
	MediumFFARoster.Roster.Add(NAME_Cadaver);
	MediumFFARoster.Roster.Add(NAME_Kryss);
	MediumFFARoster.Roster.Add(NAME_Othello);
	EnemyTeamRosters.Add(NAME_MediumFFATeam, MediumFFARoster);

	FTeamRoster HardFFARoster = FTeamRoster(NSLOCTEXT("Challenges", "HardFFAMixed", "Hard FFA"));
	HardFFARoster.Roster.Add(NAME_Freylis);
	HardFFARoster.Roster.Add(NAME_Jakob);
	HardFFARoster.Roster.Add(NAME_Picard);
	HardFFARoster.Roster.Add(NAME_Necroth);
	HardFFARoster.Roster.Add(NAME_Skakruk);
	EnemyTeamRosters.Add(NAME_HardFFATeam, HardFFARoster);

	Challenges.Add(NAME_ChallengeDMFFA,
		FUTChallengeInfo(TEXT("Deathmatch in Outpost 23"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("DM"),
		TEXT("Free for all Deathmatch in Outpost 23."), 
		4, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM));

	Challenges.Add(NAME_ChallengeCTF, 
		FUTChallengeInfo(TEXT("Capture the Flag in Titan Pass"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-Outside"),
		TEXT("CTF"),
		TEXT("CTF in the newest arena approved for the Liandri Grand Tournament."), 
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeam, NAME_ChallengeSlateBadgeName_CTF));

	Challenges.Add(NAME_ChallengeTDM,
		FUTChallengeInfo(TEXT("Team Deathmatch in Outpost 23"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("TDM"),
		TEXT("Team Deathmatch in Outpost 23."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_DM));

	Challenges.Add(NAME_ChallengeDMFFATwo,
		FUTChallengeInfo(TEXT("Deathmatch in Deck 17"), TEXT("/Game/RestrictedAssets/Maps/DM-DeckTest"),
		TEXT("DM"),
		TEXT("Free for all Deathmatch in Deck 17.  Liandri is renovating the legendary Deck 17 arena, but it remains a popular venue even in its unfinished state."),
		4, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM));

	Challenges.Add(NAME_ChallengeCTFTwo,
		FUTChallengeInfo(TEXT("Capture the Flag in Big Rock"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-BigRock"),
		TEXT("CTF"),
		TEXT("CTF in the impressive Big Rock asteroid arena, still under construction."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeam, NAME_ChallengeSlateBadgeName_CTF));

	Challenges.Add(NAME_ChallengeTDMTwo,
		FUTChallengeInfo(TEXT("Team Deathmatch in Spacer"), TEXT("/Game/RestrictedAssets/Maps/DM-Spacer"),
		TEXT("TDM"),
		TEXT("Team Deathmatch in Spacer, a challenging new arena under construction in a space station."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardMixedTeam, NAME_ChallengeSlateBadgeName_DM));

	Challenges.Add(NAME_ChallengeCTFThree,
		FUTChallengeInfo(TEXT("Capture the Flag in Facing Worlds"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-FaceTest"),
		TEXT("CTF"),
		TEXT("CTF in the legendary Facing Worlds arena."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardMixedTeam, NAME_ChallengeSlateBadgeName_CTF));

	Challenges.Add(NAME_ChallengeDMFFAThree,
		FUTChallengeInfo(TEXT("Deathmatch in Spaceer"), TEXT("/Game/RestrictedAssets/Maps/DM-Spacer"),
		TEXT("DM"),
		TEXT("Free for all Deathmatch in Spacer, a challenging new arena under construction in a space station."),
		4, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM));

	Challenges.Add(NAME_ChallengeTDMThree,
		FUTChallengeInfo(TEXT("Team Deathmatch in Temple"), TEXT("/Game/RestrictedAssets/Maps/DM-Temple"),
		TEXT("TDM"),
		TEXT("Team Deathmatch in Temple, an ancient temple ruin being restored for tournament play."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeam, NAME_ChallengeSlateBadgeName_DM));

	Challenges.Add(NAME_ChallengeCTFFour,
		FUTChallengeInfo(TEXT("Capture the Flag in Pistola"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-Pistola"),
		TEXT("CTF"),
		TEXT("CTF in the challenging Pistola arena."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_CTF));

	Challenges.Add(NAME_ChallengeCTFFive,
		FUTChallengeInfo(TEXT("Capture the Flag in Blank"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-Blank"),
		TEXT("CTF"),
		TEXT("This new arena is still not complete, and doesn't even have a working name.  That hasn't stopped some exhibition matches from being staged in the arena currently called 'Blank'."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_CTF));

}

/*
DM-Lea 1v1
DM-Lea 2v2
DM-Outpost23 1v3
DM-ASDF 1v1
*/

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
				EnemyIndex = FMath::Clamp<int32>(EnemyIndex, 0, EnemyRoster->Roster.Num()-1);
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

