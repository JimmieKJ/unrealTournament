// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTeamScoreboard.h"
#include "UTCTFScoreboard.generated.h"

UCLASS()
class UUTCTFScoreboard : public UUTTeamScoreboard
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderCapsX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderAssistsX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderReturnsX;


protected:
	virtual void DrawGameOptions(float RenderDelta, float& YOffset);
	virtual void DrawScoreHeaders(float RenderDelta, float& YOffset);
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset);

};