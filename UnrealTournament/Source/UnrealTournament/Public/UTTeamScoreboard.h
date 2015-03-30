// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

	virtual void SelectNext(int32 Offset, bool bDoNoWrap=false);
	virtual void SelectionLeft();
	virtual void SelectionRight();


protected:
	virtual void DrawTeamPanel(float RenderDelta, float& YOffset);
	virtual void DrawPlayerScores(float RenderDelta, float& DrawY);

};

