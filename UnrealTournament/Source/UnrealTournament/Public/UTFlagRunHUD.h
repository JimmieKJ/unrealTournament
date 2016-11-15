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

	UPROPERTY()
		float WinConditionMessageTime;

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
		FText BlueTeamText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText RedTeamText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText MustScoreText;

	int32 RedPlayerCount;
	int32 BluePlayerCount;

	virtual void DrawWinConditions(UFont* InFont, float XPos, float YPos, float ScoreWidth, float RenderScale, bool bCenterMessage) override;

	virtual void DrawPlayerIcon(FCanvasIcon PlayerIcon, FLinearColor DrawColor, float LiveScaling, float XOffset, float YOffset, float IconSize);
};