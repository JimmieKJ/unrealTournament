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

protected:
	virtual void DrawGameOptions(float RenderDelta, float& YOffset);
	virtual void DrawScoreHeaders(float RenderDelta, float& YOffset);
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset);

	virtual void DrawScoringStats(float RenderDelta, float& YOffset) override;

	/** Draw the list of flag captures with details. */
	virtual void DrawScoringPlays(float RenderDelta, float& YOffset, float XOffset, float ScoreWidth, float MaxHeight);

	virtual void DrawScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight) override;
	virtual void DrawTeamScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight) override;
	virtual void PageChanged_Implementation() override;
};