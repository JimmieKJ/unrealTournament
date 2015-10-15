// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChallengeManager.h"
#include "UTBotCharacter.h"

/*
rename Kryss (too close to Cryss) - new Solace
remove Lauren
Get rid of UTBotConfig ini
*/

UUTChallengeManager::UUTChallengeManager(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// @TODO FIXMESTEVE make this a blueprint
	XPBonus = 250;

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
	EasyNecrisRoster.Roster.Add(NAME_Kali);
	EasyNecrisRoster.Roster.Add(NAME_Acolyte);
	EasyNecrisRoster.Roster.Add(NAME_Judas);
	EasyNecrisRoster.Roster.Add(NAME_Harbinger);
	EasyNecrisRoster.Roster.Add(NAME_Nocturne);
	EnemyTeamRosters.Add(NAME_EasyNecrisTeam, EasyNecrisRoster);

	FTeamRoster MediumNecrisRoster = FTeamRoster(NSLOCTEXT("Challenges", "MediumEnemyNecris", "Necris Adept Team"));
	MediumNecrisRoster.Roster.Add(NAME_Malakai);
	MediumNecrisRoster.Roster.Add(NAME_Cadaver);
	MediumNecrisRoster.Roster.Add(NAME_Cryss);
	MediumNecrisRoster.Roster.Add(NAME_Kragoth);
	MediumNecrisRoster.Roster.Add(NAME_Kryss);
	EnemyTeamRosters.Add(NAME_MediumNecrisTeam, MediumNecrisRoster);

	FTeamRoster HardNecrisRoster = FTeamRoster(NSLOCTEXT("Challenges", "HardEnemyNecris", "Necris Inhuman Team"));
	HardNecrisRoster.Roster.Add(NAME_Loque);
	HardNecrisRoster.Roster.Add(NAME_Freylis);
	HardNecrisRoster.Roster.Add(NAME_Necroth);
	HardNecrisRoster.Roster.Add(NAME_Visse);
	HardNecrisRoster.Roster.Add(NAME_Malakai);
	EnemyTeamRosters.Add(NAME_HardNecrisTeam, HardNecrisRoster);

	FTeamRoster MediumMixedRoster = FTeamRoster(NSLOCTEXT("Challenges", "MediumEnemyMixed", "Mixed Adept Team"));
	MediumMixedRoster.Roster.Add(NAME_Othello);
	MediumMixedRoster.Roster.Add(NAME_Genghis);
	MediumMixedRoster.Roster.Add(NAME_Drekorig);
	MediumMixedRoster.Roster.Add(NAME_Leeb);
	MediumMixedRoster.Roster.Add(NAME_Gaargod);
	EnemyTeamRosters.Add(NAME_MediumMixedTeam, MediumMixedRoster);

	FTeamRoster HardMixedRosterA = FTeamRoster(NSLOCTEXT("Challenges", "HardEnemyMixed", "Mixed Inhuman Team"));
	HardMixedRosterA.Roster.Add(NAME_Jakob);
	HardMixedRosterA.Roster.Add(NAME_Necroth);
	HardMixedRosterA.Roster.Add(NAME_Skakruk);
	HardMixedRosterA.Roster.Add(NAME_Malcolm);
	HardMixedRosterA.Roster.Add(NAME_Picard);
	EnemyTeamRosters.Add(NAME_HardMixedTeamA, HardMixedRosterA);

	FTeamRoster HardMixedRosterB = FTeamRoster(NSLOCTEXT("Challenges", "HardEnemyMixed", "Mixed Inhuman Team"));
	HardMixedRosterB.Roster.Add(NAME_Jakob);
	HardMixedRosterB.Roster.Add(NAME_Clanlord);
	HardMixedRosterB.Roster.Add(NAME_Skakruk);
	HardMixedRosterB.Roster.Add(NAME_Freylis);
	HardMixedRosterB.Roster.Add(NAME_Picard);
	EnemyTeamRosters.Add(NAME_HardMixedTeamB, HardMixedRosterB);

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
		TEXT("?Game=DM"),
		TEXT("Free for all Deathmatch in Outpost 23."), 
		0, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM_OP23, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTF, 
		FUTChallengeInfo(TEXT("Capture the Flag in Titan Pass"), TEXT("/Game/RestrictedAssets/Maps/CTF-TitanPass"),
		TEXT("?Game=CTF"),
		TEXT("CTF in the newest arena approved for the Liandri Grand Tournament."), 
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamB, NAME_ChallengeSlateBadgeName_CTF_Titan, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeDuel,
		FUTChallengeInfo(TEXT("Duel in Lea"), TEXT("/Game/EpicInternal/Lea/DM-Lea"),
		TEXT("?Game=Duel"),
		TEXT("1v1 Duel in Lea. Be sure to visit the UT Marketplace to gain access to this map."),
		0, 1, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamB, NAME_ChallengeSlateBadgeName_DM_Lea, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDM,
		FUTChallengeInfo(TEXT("Team Deathmatch in Outpost 23"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("?Game=TDM"),
		TEXT("Team Deathmatch in Outpost 23."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_DM_OP23, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTFThree,
		FUTChallengeInfo(TEXT("Capture the Flag in Facing Worlds"), TEXT("/Game/RestrictedAssets/Maps/CTF-Face"),
		TEXT("?Game=CTF"),
		TEXT("CTF in the legendary Facing Worlds arena."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardMixedTeamA, NAME_ChallengeSlateBadgeName_CTF_Face, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeDMFFATwo,
		FUTChallengeInfo(TEXT("Deathmatch in Deck 16"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-DeckTest"),
		TEXT("?Game=DM"),
		TEXT("Free for all Deathmatch in Deck 16.  Liandri is renovating the legendary Deck 16 arena, but it remains a popular venue even in its unfinished state."),
		0, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTFFour,
		FUTChallengeInfo(TEXT("Capture the Flag in Pistola"), TEXT("/Game/EpicInternal/Pistola/CTF-Pistola"),
		TEXT("?Game=CTF"),
		TEXT("CTF in the challenging Pistola arena.  Be sure to visit the UT Marketplace to gain access to this map."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_CTF_Pistola, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDMTwo,
		FUTChallengeInfo(TEXT("Team Deathmatch in Spacer"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-Spacer"),
		TEXT("?Game=TDM"),
		TEXT("Team Deathmatch in Spacer, a challenging new arena under construction in a space station."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardMixedTeamA, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTFTwo,
		FUTChallengeInfo(TEXT("Capture the Flag in Big Rock"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-BigRock"),
		TEXT("?Game=CTF"),
		TEXT("CTF in the impressive Big Rock asteroid arena, still under construction."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamA, NAME_ChallengeSlateBadgeName_CTF, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeDMFFAThree,
		FUTChallengeInfo(TEXT("Deathmatch in Spacer"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-Spacer"),
		TEXT("?Game=DM"),
		TEXT("Free for all Deathmatch in Spacer, a challenging new arena under construction in a space station."),
		0, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDMThree,
		FUTChallengeInfo(TEXT("Team Deathmatch in Temple"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-Temple"),
		TEXT("?Game=TDM"),
		TEXT("Team Deathmatch in Temple, an ancient temple ruin being restored for tournament play."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamB, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDMFour,
		FUTChallengeInfo(TEXT("2v2 Team Deathmatch in Lea"), TEXT("/Game/EpicInternal/Lea/DM-Lea"),
		TEXT("?Game=TDM"),
		TEXT("2v2 Team Deathmatch in Lea.  Be sure to visit the UT Marketplace to gain access to this map."),
		1, 2, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardMixedTeamB, NAME_ChallengeSlateBadgeName_DM_Lea, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTFFive,
		FUTChallengeInfo(TEXT("Capture the Flag in Blank"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-Blank"),
		TEXT("?Game=CTF"),
		TEXT("This new arena is still not complete, and doesn't even have a working name.  That hasn't stopped some exhibition matches from being staged in the arena currently called 'Blank'."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_CTF, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeDuelTwo,
		FUTChallengeInfo(TEXT("Duel in ASDF"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-ASDF"),
		TEXT("?Game=Duel"),
		TEXT("1v1 Duel in ASDF."),
		0, 1, NAME_EasyFFATeam, NAME_MediumNecrisTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDMFive,
		FUTChallengeInfo(TEXT("1v5 Team Deathmatch in Outpost 23"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("?Game=TDM"),
		TEXT("Prove your worth in a 1v5 Team Deathmatch in Outpost 23."),
		0, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamA, NAME_ChallengeSlateBadgeName_DM_OP23, NAME_REWARD_GoldStars));

	RewardCaptions.Add(NAME_REWARD_HalloweenStars, NSLOCTEXT("ChallengeManage","HalloweenStarsCaption","You have earned {0} spooky pumpkins!"));
	RewardCaptions.Add(NAME_REWARD_GoldStars, NSLOCTEXT("ChallengeManage","GoldStarsCaption","You have earned {0} gold stars!"));

	RewardInfo.Add(NAME_REWARD_HalloweenStars, FUTRewardInfo(FLinearColor(0.98,0.76,0.23,1.0), NAME_REWARDSTYLE_SCARY, NAME_REWARDSTYLE_SCARY_COMPLETED));
	RewardInfo.Add(NAME_REWARD_GoldStars, FUTRewardInfo(FLinearColor(0.9,0.9,0.0,1.0), NAME_REWARDSTYLE_STAR, NAME_REWARDSTYLE_STAR_COMPLETED));

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
		UE_LOG(UT, Warning, TEXT("%s Challenge %s is Invalid"), *GetName(), *CurrentGame->ChallengeTag.ToString());
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

		FString GameMode = CurrentGame->ParseOption(Challenge->GameURL, TEXT("Game"));

		if (MapName != Challenge->Map.Right(MapName.Len()))
		{
			UE_LOG(UT, Warning, TEXT("CHALLENGE FAILED - Challenge in %s should be %s"), *MapName, *Challenge->Map);
			return false;
		}
		else if (CurrentGame->GetClass()->GetPathName() != CurrentGame->StaticGetFullGameClassName(GameMode))
		{
			UE_LOG(UT, Warning, TEXT("CHALLENGE FAILED - Challenge game %s should be %s [%s]"), *CurrentGame->GetClass()->GetFullName(), *CurrentGame->StaticGetFullGameClassName(GameMode), *GameMode);
			return false;
		}
		else if (!GetModPakFilenameFromPkg(CurrentGame->GetOutermost()->GetName()).IsEmpty())
		{
			UE_LOG(UT, Warning, TEXT("CHALLENGE FAILED - Map is overridden by mod pak file %s"), *GetModPakFilenameFromPkg(CurrentGame->GetOutermost()->GetName()));
			return false;
		}
		else
		{
			return true;
		}
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("FAILED TO FIND MATCHING CHALLENGE"));
		return false;
	}
}

void UUTChallengeManager::UpdateChallengeFromMCP(const FMCPPulledData& MCPData)
{
	if (MCPData.bValid)
	{
		for (int32 i = 0; i < MCPData.Challenges.Num(); i++)
		{
			Challenges.Add(MCPData.Challenges[i].ChallengeName, MCPData.Challenges[i].Challenge );
		}

		RewardTags.Empty();
		RewardTags = MCPData.RewardTags;
	}

}