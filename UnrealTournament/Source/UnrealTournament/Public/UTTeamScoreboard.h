// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTScoreboard.h"

#include "UTTeamScoreboard.generated.h"

UCLASS()
class UUTTeamScoreboard : public UUTScoreboard
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UFont* HugeFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UFont* ScoreFont;


protected:
	virtual void DrawTeamPanel(float RenderDelta, float& YOffset);
	virtual void DrawPlayerScores(float RenderDelta, float& DrawY);

};

