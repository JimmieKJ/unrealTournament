// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		FText RedTeamText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		FText BlueTeamText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		bool bUseRoundKills;

	UPROPERTY()
		int32 PendingScore;

	UPROPERTY()
		float RedScoreScaling;

	UPROPERTY()
		float BlueScoreScaling;

protected:
	virtual void DrawTeamPanel(float RenderDelta, float& YOffset);
	virtual void DrawPlayerScores(float RenderDelta, float& DrawY);

	UPROPERTY()
		FText TeamScoringHeader;

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

	/** Return player with best score for Team. */
	virtual AUTPlayerState* FindTopTeamScoreFor(uint8 TeamNum);

	virtual void DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom) override;
};

