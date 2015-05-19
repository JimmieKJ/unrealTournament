// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTScoreboard.h"

#include "UTTeamScoreboard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTTeamScoreboard : public UUTScoreboard
{
	GENERATED_UCLASS_BODY()

public:
	virtual void SelectNext(int32 Offset, bool bDoNoWrap=false);
	virtual void SelectionLeft();
	virtual void SelectionRight();

protected:
	virtual void DrawTeamPanel(float RenderDelta, float& YOffset);
	virtual void DrawPlayerScores(float RenderDelta, float& DrawY);

	/** 5coring breakdown for Teams. */
	virtual void DrawTeamScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight);

	/** 5coring breakdown for an individual player. */
	virtual void DrawScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight);
	
	/** Draw gametype specific stat lines for player score breakdown. */
	virtual void DrawPlayerStats(AUTPlayerState* PS, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, FFontRenderInfo TextRenderInfo, float SmallYL, float MedYL);

	/** Draw one line of scoring breakdown where values are clock stats. */
	virtual void DrawClockTeamStatsLine(FText StatsName, FName StatsID, float DeltaTime, float XOffset, float& YPos, const FFontRenderInfo& TextRenderInfo, float ScoreWidth, float SmallYL, bool bSkipEmpty);

	virtual AUTPlayerState* FindTopTeamKillerFor(uint8 TeamNum);

	virtual AUTPlayerState* FindTopTeamKDFor(uint8 TeamNum);

	virtual void SetScoringPlaysTimer(bool bEnableTimer) override;
	virtual void OpenScoringPlaysPage() override;
	virtual void DrawScoringStats(float RenderDelta, float& YOffset) override;
};

