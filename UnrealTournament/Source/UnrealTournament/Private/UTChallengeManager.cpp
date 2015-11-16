// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChallengeManager.h"
#include "UTBotCharacter.h"
#include "UTProfileSettings.h"
/*
rename Kryss (too close to Cryss) - new Solace
remove Lauren
Get rid of UTBotConfig ini
*/

UUTChallengeManager::UUTChallengeManager(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// @TODO FIXMESTEVE make this a blueprint
	XPBonus = 100;

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
	MediumNecrisRoster.Roster.Add(NAME_Leeb);
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
		FUTChallengeInfo(NAME_ChallengeDMFFA,TEXT("Deathmatch in Outpost 23"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("?Game=DM"),
		TEXT("Free for all Deathmatch in Outpost 23."), 
		0, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM_OP23, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTF, 
		FUTChallengeInfo(NAME_ChallengeCTF,TEXT("Capture the Flag in Titan Pass"), TEXT("/Game/RestrictedAssets/Maps/CTF-TitanPass"),
		TEXT("?Game=CTF"),
		TEXT("CTF in the newest arena approved for the Liandri Grand Tournament."), 
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamB, NAME_ChallengeSlateBadgeName_CTF_Titan, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeDuel,
		FUTChallengeInfo(NAME_ChallengeDuel,TEXT("Duel in Lea"), TEXT("/Game/EpicInternal/Lea/DM-Lea"),
		TEXT("?Game=Duel"),
		TEXT("1v1 Duel in Lea. Be sure to visit the UT Marketplace to gain access to this map."),
		0, 1, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamB, NAME_ChallengeSlateBadgeName_DM_Lea, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDM,
		FUTChallengeInfo(NAME_ChallengeTDM,TEXT("Team Deathmatch in Outpost 23"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("?Game=TDM"),
		TEXT("Team Deathmatch in Outpost 23."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_DM_OP23, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTFThree,
		FUTChallengeInfo(NAME_ChallengeCTFThree,TEXT("Capture the Flag in Facing Worlds"), TEXT("/Game/RestrictedAssets/Maps/CTF-Face"),
		TEXT("?Game=CTF"),
		TEXT("CTF in the legendary Facing Worlds arena."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardMixedTeamA, NAME_ChallengeSlateBadgeName_CTF_Face, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeDMFFATwo,
		FUTChallengeInfo(NAME_ChallengeDMFFATwo,TEXT("Deathmatch in Deck 16"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-DeckTest"),
		TEXT("?Game=DM"),
		TEXT("Free for all Deathmatch in Deck 16.  Liandri is renovating the legendary Deck 16 arena, but it remains a popular venue even in its unfinished state."),
		0, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTFFour,
		FUTChallengeInfo(NAME_ChallengeCTFFour,TEXT("Capture the Flag in Pistola"), TEXT("/Game/EpicInternal/Pistola/CTF-Pistola"),
		TEXT("?Game=CTF"),
		TEXT("CTF in the challenging Pistola arena.  Be sure to visit the UT Marketplace to gain access to this map."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_CTF_Pistola, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDMTwo,
		FUTChallengeInfo(NAME_ChallengeTDMTwo,TEXT("Team Deathmatch in Spacer"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-Spacer"),
		TEXT("?Game=TDM"),
		TEXT("Team Deathmatch in Spacer, a challenging new arena under construction in a space station."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardMixedTeamA, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTFTwo,
		FUTChallengeInfo(NAME_ChallengeCTFTwo,TEXT("Capture the Flag in Big Rock"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-BigRock"),
		TEXT("?Game=CTF"),
		TEXT("CTF in the impressive Big Rock asteroid arena, still under construction."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamA, NAME_ChallengeSlateBadgeName_CTF, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeDMFFAThree,
		FUTChallengeInfo(NAME_ChallengeDMFFAThree,TEXT("Deathmatch in Spacer"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-Spacer"),
		TEXT("?Game=DM"),
		TEXT("Free for all Deathmatch in Spacer, a challenging new arena under construction in a space station."),
		0, 5, NAME_EasyFFATeam, NAME_MediumFFATeam, NAME_HardFFATeam, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDMThree,
		FUTChallengeInfo(NAME_ChallengeTDMThree,TEXT("Team Deathmatch in Temple"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-Temple"),
		TEXT("?Game=TDM"),
		TEXT("Team Deathmatch in Temple, an ancient temple ruin being restored for tournament play."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamB, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDMFour,
		FUTChallengeInfo(NAME_ChallengeTDMFour,TEXT("2v2 Team Deathmatch in Lea"), TEXT("/Game/EpicInternal/Lea/DM-Lea"),
		TEXT("?Game=TDM"),
		TEXT("2v2 Team Deathmatch in Lea.  Be sure to visit the UT Marketplace to gain access to this map."),
		1, 2, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardMixedTeamB, NAME_ChallengeSlateBadgeName_DM_Lea, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeCTFFive,
		FUTChallengeInfo(NAME_ChallengeCTFFive,TEXT("Capture the Flag in Blank"), TEXT("/Game/RestrictedAssets/Maps/WIP/CTF-Blank"),
		TEXT("?Game=CTF"),
		TEXT("This new arena is still not complete, and doesn't even have a working name.  That hasn't stopped some exhibition matches from being staged in the arena currently called 'Blank'."),
		4, 5, NAME_EasyNecrisTeam, NAME_MediumNecrisTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_CTF, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeDuelTwo,
		FUTChallengeInfo(NAME_ChallengeDuelTwo,TEXT("Duel in ASDF"), TEXT("/Game/RestrictedAssets/Maps/WIP/DM-ASDF"),
		TEXT("?Game=Duel"),
		TEXT("1v1 Duel in ASDF."),
		0, 1, NAME_EasyFFATeam, NAME_MediumNecrisTeam, NAME_HardNecrisTeam, NAME_ChallengeSlateBadgeName_DM, NAME_REWARD_GoldStars));

	Challenges.Add(NAME_ChallengeTDMFive,
		FUTChallengeInfo(NAME_ChallengeTDMFive,TEXT("1v5 Team Deathmatch in Outpost 23"), TEXT("/Game/RestrictedAssets/Maps/DM-Outpost23"),
		TEXT("?Game=TDM"),
		TEXT("Prove your worth in a 1v5 Team Deathmatch in Outpost 23."),
		0, 5, NAME_EasyNecrisTeam, NAME_MediumMixedTeam, NAME_HardMixedTeamA, NAME_ChallengeSlateBadgeName_DM_OP23, NAME_REWARD_GoldStars));

	RewardCaptions.Add(NAME_REWARD_HalloweenStars, NSLOCTEXT("ChallengeManage","HalloweenStarsCaption","You have earned {0} spooky stars!"));
	RewardCaptions.Add(NAME_REWARD_GoldStars, NSLOCTEXT("ChallengeManage","GoldStarsCaption","You have earned {0} gold stars!"));
	RewardCaptions.Add(NAME_REWARD_DailyStars, NSLOCTEXT("ChallengeManage", "DailyStarsCaption", "You have earned {0} daily stars!"));

	RewardInfo.Add(NAME_REWARD_HalloweenStars, FUTRewardInfo(FLinearColor(0.98,0.76,0.23,1.0), NAME_REWARDSTYLE_SCARY, NAME_REWARDSTYLE_SCARY_COMPLETED));
	RewardInfo.Add(NAME_REWARD_GoldStars, FUTRewardInfo(FLinearColor(0.9,0.9,0.0,1.0), NAME_REWARDSTYLE_STAR, NAME_REWARDSTYLE_STAR_COMPLETED));
	RewardInfo.Add(NAME_REWARD_DailyStars, FUTRewardInfo(FLinearColor(0.9, 0.9, 0.0, 1.0), NAME_REWARDSTYLE_STAR, NAME_REWARDSTYLE_STAR_COMPLETED));

	bNewDailyUnlocked = false;
	bTestDailyChallenges = false;
}

UUTBotCharacter* UUTChallengeManager::ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum, int32 TotalStars) const
{
	if ( CurrentGame->ChallengeTag != NAME_None && Challenges.Contains(CurrentGame->ChallengeTag) )
	{
		const FUTChallengeInfo* Challenge = Challenges.Find(CurrentGame->ChallengeTag);

		// daily challenges have fixed player teams regardless of stars earned
		if (Challenge->bDailyChallenge)
		{
			TotalStars = CurrentGame->ChallengeDifficulty * 20;
		}
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
			// Fix up the tag referece
			FName ChallengeTag = MCPData.Challenges[i].ChallengeName;
			Challenges.Add(ChallengeTag, FUTChallengeInfo(ChallengeTag, MCPData.Challenges[i].Challenge));
		}
		RevisionNumber = MCPData.ChallengeRevisionNumber;
		RewardTags.Empty();
		RewardTags = MCPData.RewardTags;

		// Look at the Daily Challenge and see if we need to unlock one.
	}
}

// Returns true if we have unlocked a new Daily Challenge.
bool UUTChallengeManager::CheckDailyChallenge(UUTProfileSettings* ProfileSettings)
{
	if (ProfileSettings == nullptr) return false;

	TArray<const FUTChallengeInfo*> ActiveDailyChallenges;
	GetChallenges(ActiveDailyChallenges, EChallengeFilterType::DailyUnlocked, ProfileSettings);

	if (ActiveDailyChallenges.Num() < MAX_ACTIVE_DAILY_CHALLENGES) 
	{
		// Active the next challenge.
		TArray<const FUTChallengeInfo*> LockedDailyChallenges;
		GetChallenges(LockedDailyChallenges, EChallengeFilterType::DailyLocked, ProfileSettings);

		if (LockedDailyChallenges.Num() > 0)
		{
			// Unlock the first challenge.
			ProfileSettings->UnlockedDailyChallenges.Add( FUTDailyChallengeUnlock(LockedDailyChallenges[0]->Tag));
			return true;
		}
	}

	return false;
}

void UUTChallengeManager::GetChallenges(TArray<const FUTChallengeInfo*>& outChallengeList, EChallengeFilterType::Type Filter, UUTProfileSettings* ProfileSettings)
{
	// Iterate over all of the challenges and filter out those we do not want
	for (auto It = Challenges.CreateConstIterator(); It; ++It)
	{
		FName ChallengeTag = It.Key();
		const FUTChallengeInfo* Challenge = &It.Value();

		// Quickly reject daily challenges that haven't been unlocked yet.
		FUTDailyChallengeUnlock* UnlockFound = nullptr;
		if (Challenge->bDailyChallenge)
		{
			bool bLimitDailyChallenges = true;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			bLimitDailyChallenges = !bTestDailyChallenges;
#endif
			if (bLimitDailyChallenges)
			{
				if (ProfileSettings)
				{
					// Look to see if this challenge is unlocked.  
					UnlockFound = ProfileSettings->UnlockedDailyChallenges.FindByPredicate([ChallengeTag](const FUTDailyChallengeUnlock& Unlock)
					{
						return Unlock.Tag == ChallengeTag;
					});
				}

				// If we didn't find it, it hasn't been unlocked yet so just reject it
				if (UnlockFound == nullptr)
				{
					// If we are filtering for locked daily challenges, return it here.
					if (Filter == EChallengeFilterType::DailyLocked)
					{
						outChallengeList.Add(Challenge);
					}

					continue;
				}
			}
		}
		else if (Filter == EChallengeFilterType::DailyLocked)
		{
			continue;
		}

		if (Challenge->bExpiredChallenge && (Filter != EChallengeFilterType::Expired && Filter != EChallengeFilterType::All))
		{
			continue;
		}

		if (Filter != EChallengeFilterType::All)
		{
			if (Filter == EChallengeFilterType::Active)
			{
				if (Challenge->bDailyChallenge)
				{
					// Look to see if the unlock time has expired
					if (UnlockFound && (FDateTime::Now() - UnlockFound->UnlockTime).GetHours() >= DAILY_STALE_TIME_HOURS)
					{
						continue;
					}
				}

				if (ProfileSettings)
				{
					// If this challenge has a result, reject it.
					const FUTChallengeResult* ChallengeResult = ProfileSettings->ChallengeResults.FindByPredicate([ChallengeTag](const FUTChallengeResult& Result) {return Result.Tag == ChallengeTag;});
					if (ChallengeResult != nullptr)
					{
						continue;
					}
				}
			}
			else if (Filter == EChallengeFilterType::Completed)
			{
				if (ProfileSettings)
				{
					// If this challenge has a result, reject it.
					const FUTChallengeResult* ChallengeResult = ProfileSettings->ChallengeResults.FindByPredicate([ChallengeTag](const FUTChallengeResult& Result) { return Result.Tag == ChallengeTag; });
					if (ChallengeResult == nullptr)
					{
						continue;
					}
				}
			}
			else if (Filter == EChallengeFilterType::Expired)
			{
				if (Challenge->bDailyChallenge)
				{
					// Look to see if the unlock time has expired
					if (UnlockFound && (FDateTime::Now() - UnlockFound->UnlockTime).GetHours() < DAILY_STALE_TIME_HOURS && !Challenge->bExpiredChallenge)
					{
						continue;
					}
				}
				else if (!Challenge->bExpiredChallenge)
				{
					continue;
				}
			}
			else if (Filter == EChallengeFilterType::DailyUnlocked)
			{
				if (!Challenge->bDailyChallenge || (UnlockFound && (FDateTime::Now() - UnlockFound->UnlockTime).GetHours() >= DAILY_STALE_TIME_HOURS))
				{
					continue;
				}
			}
		}

		// It's possible to get here before gotten the profile or the MCP data.. so we have to manage it
		if (ProfileSettings)
		{
			// Insert Sort the challenge in to the list.

			bool bAdded = false;

			int32 RewardIndex = RewardTags.Find(Challenge->RewardTag);
			for (int32 i = 0; i < outChallengeList.Num(); i++)
			{
				int32 TestIndex = RewardTags.Find(outChallengeList[i]->RewardTag);
				if (TestIndex < RewardIndex)
				{
					outChallengeList.Insert(Challenge,i);
					bAdded = true;
					break;
				}
			}

			if (!bAdded) outChallengeList.Add(Challenge);
		}
		else
		{
			outChallengeList.Add(Challenge);
		}
	}
}

int32 UUTChallengeManager::TimeUntilExpiration(FName DailyChallengeName, UUTProfileSettings* ProfileSettings )
{
	if (ProfileSettings)
	{
		const FUTChallengeInfo Challenge = Challenges[DailyChallengeName];
		if (Challenge.bDailyChallenge)
		{
			FUTDailyChallengeUnlock* UnlockFound = ProfileSettings->UnlockedDailyChallenges.FindByPredicate([DailyChallengeName](const FUTDailyChallengeUnlock& Unlock)
			{
				return Unlock.Tag == DailyChallengeName;
			});

			if (UnlockFound != nullptr)
			{
				return DAILY_STALE_TIME_HOURS - (FDateTime::Now() - UnlockFound->UnlockTime).GetHours();
			}
		}
	}
	return 0;
}
