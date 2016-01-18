// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTTeamScoreboard.h"
#include "UTDuelScoreboard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTDuelScoreboard : public UUTTeamScoreboard
{
	GENERATED_UCLASS_BODY()

protected:
	virtual void DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo) override;
};

