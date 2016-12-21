// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTHUD_CTF.h"
#include "SUTHUDWindow.h"
#include "UTCTFGameState.h"
#include "UTFlagRunHUD.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagRunHUD : public AUTHUD_CTF
{
	GENERATED_UCLASS_BODY()

	virtual bool ScoreboardIsUp() override;
	virtual void DrawHUD() override;
	virtual void NotifyMatchStateChange() override;
	virtual void BeginPlay() override;

	/** icon for player starts on the minimap (foreground) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	FCanvasIcon PlayerStartIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
		FCanvasIcon RedTeamIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
		FCanvasIcon BlueTeamIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	FCanvasIcon RedTeamOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	FCanvasIcon BlueTeamOverlay;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText DefendersMustStop;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText DefendersMustHold;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScore;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText UnhandledCondition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreWin;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreTimeWin;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreShort;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText BlueTeamText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText RedTeamText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText MustScoreText;

	UPROPERTY()
		bool bUseShortWinMessage;

	int32 RedPlayerCount;
	int32 BluePlayerCount;

	virtual float DrawWinConditions(UFont* InFont, float XPos, float YPos, float ScoreWidth, float RenderScale, bool bCenterMessage, bool bSkipDrawing=false) override;

	virtual void DrawPlayerIcon(AUTPlayerState* PlayerState, float LiveScaling, float XOffset, float YOffset, float IconSize, bool bDrawLives);
	// get sorted array of players for which we should draw icons at the top of the HUD
	virtual void GetPlayerListForIcons(TArray<AUTPlayerState*>& SortedPlayers);
};