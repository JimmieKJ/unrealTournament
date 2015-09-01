// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTChallengeManager.generated.h"

static const FName NAME_ChallengeSlateBadgeName_DM(TEXT("UT.ChallengeBadges.DM"));
static const FName NAME_ChallengeSlateBadgeName_CTF(TEXT("UT.ChallengeBadges.CTF"));
static const FName NAME_PlayerTeamRoster(TEXT("PlayersTeam"));
static const FName NAME_ChallengeDM(TEXT("InitialDeathmatchChallenge"));
static const FName NAME_ChallengeCTF(TEXT("InitialCaptureTheFlagChallenge"));

static const FName NAME_EnemyTeam_SkaarjDefault(TEXT("EnemyTeamSkaarjDefault"));


UCLASS(Abstract)
class UNREALTOURNAMENT_API UUTChallengeManager : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual UUTBotCharacter* ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum) const;

	UPROPERTY()
	FTeamRoster PlayerTeamRoster;

	UPROPERTY()
	TMap<FName, FUTChallengeInfo> Challenges;
};