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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		float ColumnHeaderPowerupXDuringReadyUp;

	UPROPERTY()
		FText PowerupText;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		FText CH_Powerup;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		FText DefendTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		FText AttackTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		TArray<FText> DefendLines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		TArray<FText> AttackLines;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText BlueTeamName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText RedTeamName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText TeamScorePrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText TeamScorePostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText TeamWinsPrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText TeamWinsPostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText StarText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText DefenseScorePrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText DefenseScorePostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText DeliveredPrefix;
	
	UPROPERTY()
		float EndIntermissionTime;

	UPROPERTY()
		int32 OldDisplayedParagraphs;

	UPROPERTY()
		bool bFullListPlayed;

	UPROPERTY()
		bool bNeedReplay;

	UPROPERTY()
		int32 PendingTiebreak;

	UPROPERTY()
		USoundBase* LineDisplaySound;

	UPROPERTY()
		USoundBase* StarPoundSound;

	UPROPERTY()
		USoundBase* StarWooshSound;

	UPROPERTY()
		AUTTeamInfo* ScoringTeam;
	
	UPROPERTY()
		APlayerState* ScoringPlayer;
	
	UPROPERTY()
		uint8 RoundBonus;
	
	UPROPERTY()
		uint8 Reason;

	UPROPERTY()
		float ScoreReceivedTime;
	
	UPROPERTY()
		float ScoreInfoDuration;

	UPROPERTY()
		bool bHasAnnouncedWin;

	virtual void AnnounceRoundScore(AUTTeamInfo* InScoringTeam, APlayerState* InScoringPlayer, uint8 InRoundBonus, uint8 InReason);

protected:
	virtual void DrawScoreHeaders(float RenderDelta, float& YOffset);
	virtual void DrawGamePanel(float RenderDelta, float& YOffset) override;
	virtual void DrawScorePanel(float RenderDelta, float& YOffset) override;
	virtual void DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor) override;
	virtual void DrawReadyText(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width);
	virtual void DrawTeamStats(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, const FStatsFontInfo& StatsFontInfo) override;
	virtual void DrawScoringPlayInfo(const struct FCTFScoringPlay& Play, float CurrentScoreHeight, float SmallYL, float MedYL, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, FFontRenderInfo TextRenderInfo, bool bIsSmallPlay) override;
	virtual void DrawStatsRight(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom) override;
	virtual void DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom) override;
	virtual bool ShouldDrawScoringStats() override;
	virtual void DrawMinimap(float RenderDelta) override;
	virtual void DrawTeamPanel(float RenderDelta, float& YOffset) override;

	virtual bool ShouldShowPowerupForPlayer(AUTPlayerState* PlayerState);

	virtual void DrawScoringSummary(float RenderDelta, float& YOffset, float XOffset, float ScoreWidth, float PageBottom);

	virtual bool IsBeforeFirstRound();
	
	virtual bool ShowScorePanel();

	virtual bool ShowScoringInfo();

	virtual void DrawScoreAnnouncement(float DeltaTime);

	virtual float DrawWinAnnouncement(float DeltaTime, UFont* InFont);

	virtual void DrawFramedBackground(float XOffset, float YOffset, float Width, float Height);

	virtual void DrawScoringPlays(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight) override;

};