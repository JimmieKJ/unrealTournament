// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTChallengeManager.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API UUTChallengeManager : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual UUTBotCharacter* ChooseBotCharacter(AUTGameMode* CurrentGame, uint8& TeamNum) const;

	UPROPERTY()
		TArray<FName> PlayerTeamRoster;
};