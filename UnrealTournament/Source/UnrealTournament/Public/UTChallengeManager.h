// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTChallengeManager.generated.h"

static const FName NAME_ChallengeSlateBadgeName_DM(TEXT("UT.ChallengeBadges.DM"));
static const FName NAME_ChallengeSlateBadgeName_CTF(TEXT("UT.ChallengeBadges.CTF"));
static const FName NAME_PlayerTeamRoster(TEXT("PlayersTeam"));
static const FName NAME_ChallengeDM(TEXT("InitialDeathmatchChallenge"));
static const FName NAME_ChallengeCTF(TEXT("InitialCaptureTheFlagChallenge"));

static const FName NAME_EasyNecrisTeam(TEXT("EasyNecrisTeam"));
static const FName NAME_MediumNecrisTeam(TEXT("MediumNecrisTeam"));
static const FName NAME_HardNecrisTeam(TEXT("HardNecrisTeam"));

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

UCLASS(Abstract)
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
};