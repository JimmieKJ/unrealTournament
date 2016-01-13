// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTTeamScoreboard.h"
#include "UTShowdownScoreboard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTShowdownScoreboard : public UUTTeamScoreboard
{
	GENERATED_UCLASS_BODY()

	public:
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
			float ColumnHeaderKillsX;
		
		virtual void DrawScoreHeaders(float RenderDelta, float& YOffset) override;
		virtual void DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor) override;
		virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset) override;
	virtual void Draw_Implementation(float RenderDelta) override;
};

