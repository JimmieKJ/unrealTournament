// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTChallengeManager.generated.h"

static const FName NAME_ChallengeSlateBadgeName_DM(TEXT("UT.ChallengeBadges.DM"));
static const FName NAME_ChallengeSlateBadgeName_DM_OP23(TEXT("UT.ChallengeBadges.DM_OP23"));
static const FName NAME_ChallengeSlateBadgeName_DM_Lea(TEXT("UT.ChallengeBadges.DM_Lea"));
static const FName NAME_ChallengeSlateBadgeName_CTF(TEXT("UT.ChallengeBadges.CTF"));
static const FName NAME_ChallengeSlateBadgeName_CTF_Face(TEXT("UT.ChallengeBadges.CTF_Face"));
static const FName NAME_ChallengeSlateBadgeName_CTF_Pistola(TEXT("UT.ChallengeBadges.CTF_Pistola"));
static const FName NAME_ChallengeSlateBadgeName_CTF_Titan(TEXT("UT.ChallengeBadges.CTF_Titan"));
static const FName NAME_PlayerTeamRoster(TEXT("PlayersTeam"));

static const FName NAME_ChallengeDMFFA(TEXT("ChallengeDMFFA1"));
static const FName NAME_ChallengeDMFFATwo(TEXT("ChallengeDMFFA2"));
static const FName NAME_ChallengeDMFFAThree(TEXT("ChallengeDMFFA3"));
static const FName NAME_ChallengeCTF(TEXT("ChallengeCTF1"));
static const FName NAME_ChallengeCTFTwo(TEXT("ChallengeCTF2"));
static const FName NAME_ChallengeCTFThree(TEXT("ChallengeCTF3"));
static const FName NAME_ChallengeCTFFour(TEXT("ChallengeCTF4"));
static const FName NAME_ChallengeCTFFive(TEXT("ChallengeCTF5"));
static const FName NAME_ChallengeTDM(TEXT("ChallengeTDM"));
static const FName NAME_ChallengeTDMTwo(TEXT("ChallengeTDM2"));
static const FName NAME_ChallengeTDMThree(TEXT("ChallengeTDM3"));
static const FName NAME_ChallengeTDMFour(TEXT("ChallengeTDM4"));
static const FName NAME_ChallengeTDMFive(TEXT("ChallengeTDM5"));
static const FName NAME_ChallengeDuel(TEXT("ChallengeDuel"));
static const FName NAME_ChallengeDuelTwo(TEXT("ChallengeDuel2"));

static const FName NAME_EasyNecrisTeam(TEXT("EasyNecrisTeam"));
static const FName NAME_MediumNecrisTeam(TEXT("MediumNecrisTeam"));
static const FName NAME_HardNecrisTeam(TEXT("HardNecrisTeam"));
static const FName NAME_HardMixedTeamA(TEXT("HardMixedTeamA"));
static const FName NAME_HardMixedTeamB(TEXT("HardMixedTeamB"));
static const FName NAME_MediumMixedTeam(TEXT("MediumMixedTeam"));
static const FName NAME_EasyFFATeam(TEXT("EasyFFATeam"));
static const FName NAME_MediumFFATeam(TEXT("MediumFFATeam"));
static const FName NAME_HardFFATeam(TEXT("HardFFATeam"));

static const FName NAME_Damian(TEXT("Damian"));
static const FName NAME_Skirge(TEXT("Skirge"));
static const FName NAME_Arachne(TEXT("Arachne"));
static const FName NAME_Kraagesh(TEXT("Kraagesh"));
static const FName NAME_Samael(TEXT("Samael"));
static const FName NAME_Taye(TEXT("Taye"));
static const FName NAME_Malise(TEXT("Malise"));
static const FName NAME_Gkoblok(TEXT("Gkoblok"));
static const FName NAME_Grail(TEXT("Grail"));
static const FName NAME_Barktooth(TEXT("Barktooth"));
static const FName NAME_Raven(TEXT("Raven"));
static const FName NAME_Dominator(TEXT("Dominator"));

static const FName NAME_Acolyte(TEXT("Acolyte"));
static const FName NAME_Judas(TEXT("Judas"));
static const FName NAME_Kali(TEXT("Kali"));
static const FName NAME_Harbinger(TEXT("Harbinger"));
static const FName NAME_Nocturne(TEXT("Nocturne"));

static const FName NAME_Cadaver(TEXT("Cadaver"));
static const FName NAME_Cryss(TEXT("Cryss"));
static const FName NAME_Kragoth(TEXT("Kragoth"));
static const FName NAME_Kryss(TEXT("Kryss"));
static const FName NAME_Malakai(TEXT("Malakai"));

static const FName NAME_Loque(TEXT("Loque"));
static const FName NAME_Freylis(TEXT("Freylis"));
static const FName NAME_Necroth(TEXT("Necroth"));
static const FName NAME_Visse(TEXT("Visse"));

static const FName NAME_Genghis(TEXT("Genghis"));
static const FName NAME_Drekorig(TEXT("Drekorig"));
static const FName NAME_Leeb(TEXT("Leeb"));
static const FName NAME_Othello(TEXT("Othello"));
static const FName NAME_Gaargod(TEXT("Gaargod"));

static const FName NAME_Clanlord(TEXT("Clanlord"));
static const FName NAME_Skakruk(TEXT("Skakruk"));
static const FName NAME_Malcolm(TEXT("Malcolm"));
static const FName NAME_Jakob(TEXT("Jakob"));
static const FName NAME_Picard(TEXT("Picard"));

static const FName NAME_Guardian(TEXT("Guardian"));

static const FName NAME_REWARD_GoldStars(TEXT("REWARD_GoldStars"));
static const FName NAME_REWARD_HalloweenStars(TEXT("REWARD_HalloweenStars"));
static const FName NAME_REWARD_DailyStars(TEXT("REWARD_DailyStars"));

static const FName NAME_REWARDSTYLE_STAR(TEXT("UT.Star.Outline"));
static const FName NAME_REWARDSTYLE_STAR_COMPLETED(TEXT("UT.Star"));
static const FName NAME_REWARDSTYLE_SCARY(TEXT("UT.ScaryStar"));
static const FName NAME_REWARDSTYLE_SCARY_COMPLETED(TEXT("UT.ScaryStar.Completed"));

static const int32 DAILY_STALE_TIME_HOURS = 48;
static const int32 MAX_ACTIVE_DAILY_CHALLENGES = 1;

class UUTProgressionStorage;

UCLASS(config = Game)
class UNREALTOURNAMENT_API UUTChallengeManager : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTBotCharacter* ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum, int32 TotalStars) const;

	int32 GetNumPlayers(AUTGameMode* CurrentGame) const;

	/** Validate map and gametype match challenge parameters. */
	bool IsValidChallenge(AUTGameMode* CurrentGame, const FString& MapName) const;

	UPROPERTY()
	FTeamRoster PlayerTeamRoster;

	UPROPERTY()
	TMap<FName,	FTeamRoster> EnemyTeamRosters;

	UPROPERTY()
	TMap<FName, FUTChallengeInfo> Challenges;

	/** XP bonus per star earned on a challenge. */
	UPROPERTY()
	float XPBonus;

	// Holds a list of possible reward tags in sorted order.  In this case however, higher in this list means display first in 
	// the menu.  This is because we want the original challenge to be at the bottom of the list and since they lack RewardTags they
	// will get the value of -1.
	UPROPERTY()
	TArray<FName> RewardTags;

	UPROPERTY()
	TMap<FName, FText> RewardCaptions;

	// For testing, not valid in release builds
	UPROPERTY(globalconfig)
		bool bTestDailyChallenges;

	TMap<FName, FUTRewardInfo> RewardInfo;

	// update the challenge information from the MCP
	void UpdateChallengeFromMCP(const FMCPPulledData& MCPData);

	// The version number of this challegne set.
	int32 RevisionNumber;

	// Returns a given challenge or NULL if that channge does not exist.
	const FUTChallengeInfo* GetChallenge(FName Challenge)
	{
		if (Challenges.Contains(Challenge))
		{
			return &Challenges[Challenge];
		}

		return NULL;
	}

	bool bNewDailyUnlocked;
	bool CheckDailyChallenge(UUTProgressionStorage* Storage);

	void GetChallenges(TArray<const FUTChallengeInfo*>& outChallengeList, EChallengeFilterType::Type Filter, UUTProgressionStorage* Storage);
	//returns the time in hours until a challenge expires.  It returns 0 if expired or not unlocked.
	int32 TimeUntilExpiration(FName DailyChallengeName, UUTProgressionStorage* Storage );

};