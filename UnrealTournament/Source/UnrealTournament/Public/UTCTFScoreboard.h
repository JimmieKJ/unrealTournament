// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTeamScoreboard.h"
#include "UTCTFScoreboard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTCTFScoreboard : public UUTTeamScoreboard
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FText ScoringPlaysHeader;
	UPROPERTY()
	FText AssistedByText;
	UPROPERTY()
	FText UnassistedText;
	UPROPERTY()
	FText CaptureText;
	UPROPERTY()
	FText ScoreText;
	UPROPERTY()
	FText NoScoringText;
	UPROPERTY()
	FText PeriodText[3];

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderCapsX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderAssistsX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderReturnsX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ReadyX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ValueColumn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ScoreColumn;

	virtual void Draw_Implementation(float DeltaTime) override;

	FTimerHandle OpenScoringPlaysHandle;

	/** no-params accessor for timers */
	UFUNCTION()
	void OpenScoringPlaysPage();

	UPROPERTY()
		float TimeLineOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoreboard")
		USoundBase* ScoreUpdateSound;

protected:
	virtual void DrawGameOptions(float RenderDelta, float& YOffset);
	virtual void DrawScoreHeaders(float RenderDelta, float& YOffset);
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset);

	/** Show current 2 pages of scoring stats breakdowns. */
	virtual void DrawScoringStats(float RenderDelta, float& YOffset);

	/** Draw the list of flag captures with details. */
	virtual void DrawScoringPlays(float RenderDelta, float& YOffset, float XOffset, float ScoreWidth, float MaxHeight);

	/** 5coring breakdown for an individual player. */
	virtual void DrawScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight);

	/** 5coring breakdown for Teams. */
	virtual void DrawTeamScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight);

	/** Draw one line of scoring breakdown. */
	virtual void DrawStatsLine(FText StatsName, int32 StatValue, int32 ScoreValue, float DeltaTime, float XOffset, float& YPos, const FFontRenderInfo& TextRenderInfo, float ScoreWidth, float SmallYL);

	/** Draw one line of scoring breakdown where values are string instead of int32. */
	virtual void DrawTextStatsLine(FText StatsName, FString StatValue, FString ScoreValue, float DeltaTime, float XOffset, float& YPos, const FFontRenderInfo& TextRenderInfo, float ScoreWidth, float SmallYL, int32 HighlightIndex);

	UPROPERTY()
		bool bHighlightStatsLineTopValue;

	virtual void PageChanged_Implementation() override;

	virtual AUTPlayerState* FindTopTeamKillerFor(uint8 TeamNum);

	virtual AUTPlayerState* FindTopTeamKDFor(uint8 TeamNum);
};