// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTCTFBaseGame.h"
#include "UTCTFRoundGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFRoundGame : public AUTCTFBaseGame
{
	GENERATED_UCLASS_BODY()

	/**Alternate round victory condition - get this many kills. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CTF)
	int32 RoundLives;

	UPROPERTY()
		bool bNeedFiveKillsMessage;

	virtual void RestartPlayer(AController* aPlayer) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	void BuildServerResponseRules(FString& OutRules);
	virtual void HandleFlagCapture(AUTPlayerState* Holder) override;
	virtual void HandleExitingIntermission() override;
	virtual void InitGameState() override;
	virtual int32 IntermissionTeamToView(AUTPlayerController* PC) override;
	virtual void CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps);

	/** Score round ending due to team out of lives. */
	virtual void ScoreOutOfLives(int32 WinningTeamIndex);

	/** Initialize for new round. */
	virtual void InitRound();
};