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
	virtual void DrawTeamScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom);

	virtual void DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo);

	/** Draw one line of scoring breakdown where values are clock stats. */
	virtual void DrawClockTeamStatsLine(FText StatsName, FName StatsID, float DeltaTime, float XOffset, float& YPos, const FStatsFontInfo& StatsFontInfo, float ScoreWidth, bool bSkipEmpty);

	/** Return player with most kills for Team. */
	virtual AUTPlayerState* FindTopTeamKillerFor(uint8 TeamNum);

	/** Return player with best kill/death ratio for Team. */
	virtual AUTPlayerState* FindTopTeamKDFor(uint8 TeamNum);

	/** Return player with best score per minute for Team. */
	virtual AUTPlayerState* FindTopTeamSPMFor(uint8 TeamNum);

	virtual void SetScoringPlaysTimer(bool bEnableTimer) override;
	virtual void OpenScoringPlaysPage() override;
	virtual void DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom) override;
};

