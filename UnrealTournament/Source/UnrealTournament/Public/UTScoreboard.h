// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget.h"
#include "UTScoreboard.generated.h"

UCLASS()
class UUTScoreboard : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	// This font is used for the game rules's values
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UFont* LargeFont;

	// This font is used for Game rules descriptions and player names
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UFont* MediumFont;

	// This font is used for Player Values, Server Name and Server Rules
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UFont* SmallFont;

	// This font is used for player headings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UFont* TinyFont;

	// The font for the clock
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UFont* ClockFont;

	// The main drawing stub
	virtual void Draw_Implementation(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderPlayerX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderScoreX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderDeathsX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderPingX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnHeaderY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	float ColumnY;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UTexture2D* TextureAtlas;

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return bShowScores;
	}

	virtual void DrawGamePanel(float RenderDelta, float& YOffset);
	virtual void DrawGameOptions(float RenderDelta, float& YOffset);

	virtual void DrawTeamPanel(float RenderDelta, float& YOffset);

	virtual void DrawScorePanel(float RenderDelta, float& YOffset);
	virtual void DrawScoreHeaders(float RenderDelta, float& DrawY);
	virtual void DrawPlayerScores(float RenderDelta, float& DrawY);
	virtual void DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset);


	virtual void DrawServerPanel(float RenderDelta, float& YOffset);
};

