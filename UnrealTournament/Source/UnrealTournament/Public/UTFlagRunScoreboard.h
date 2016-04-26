// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFScoreboard.h"
#include "UTFlagRunScoreboard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTFlagRunScoreboard : public UUTCTFScoreboard
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ColumnHeaderPowerupX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ColumnHeaderPowerupEndX;

	UPROPERTY()
		FText PowerupText;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		FText CH_Powerup;
protected:
	virtual void DrawScoreHeaders(float RenderDelta, float& YOffset);
	virtual void DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor) override;

};