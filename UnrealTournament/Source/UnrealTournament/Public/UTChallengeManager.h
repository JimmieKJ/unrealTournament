// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTChallengeManager.generated.h"

USTRUCT()
struct FChallengeInfo
{
	GENERATED_USTRUCT_BODY()

	FChallengeInfo() : PlayerTeamSize(4), EnemyRoster() {};

	UPROPERTY()
		int32 PlayerTeamSize;

	UPROPERTY()
		TArray<FName> EnemyRoster;
};

UCLASS(Abstract)
class UNREALTOURNAMENT_API UUTChallengeManager : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual UUTBotCharacter* ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum) const;

	UPROPERTY()
		TArray<FName> PlayerTeamRoster;

	UPROPERTY()
		TArray<FChallengeInfo> Challenges;
};