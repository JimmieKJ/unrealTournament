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

	virtual void OpenScoringPlaysPage() override;

	UPROPERTY()
		float TimeLineOffset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoreboard")
		USoundBase* ScoreUpdateSound;

	virtual void SetScoringPlaysTimer(bool bEnableTimer) override;

	virtual void DrawPlayerStats(AUTPlayerState* PS, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo) override;

protected:
	virtual void DrawGameOptions(float RenderDelta, float& YOffset);
	virtual void DrawScoreHeaders(float RenderDelta, float& YOffset);
	virtual void DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor) override;

	virtual void DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom) override;
	virtual void DrawStatsRight(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom) override;

	/** Draw the list of flag captures with details. */
	virtual void DrawScoringPlays(float RenderDelta, float& YOffset, float XOffset, float ScoreWidth, float PageBottom);

	virtual void DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo) override;
	virtual void PageChanged_Implementation() override;
};