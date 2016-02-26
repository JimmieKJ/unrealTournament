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
	UPROPERTY(BlueprintReadWrite, Category = CTF)
	int32 RoundLives;

	/*  If true, one team wins round by cap, other team wins by kills */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bAsymmetricVictoryConditions;

	/*  If true, trying to deliver own flag to enemy base */ 
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bCarryOwnFlag;

	/*  If true, no auto return flag on touch */ 
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bNoFlagReturn;

	/** Q engages rechargeable through the air dash instead of having translocator. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bUseDash;

	/** If true, red team is trying to cap with asymmetric conditions. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bRedToCap;

	/** If true, round lives are per player. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bPerPlayerLives;

	UPROPERTY()
		bool bNeedFiveKillsMessage;

	UPROPERTY()
		bool bFirstRoundInitialized;

	virtual void RestartPlayer(AController* aPlayer) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	void BuildServerResponseRules(FString& OutRules);
	virtual void HandleFlagCapture(AUTPlayerState* Holder) override;
	virtual void HandleExitingIntermission() override;
	virtual int32 IntermissionTeamToView(AUTPlayerController* PC) override;
	virtual void CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps);
	virtual void StartMatch() override;

	/** Score round ending due to team out of lives. */
	virtual void ScoreOutOfLives(int32 WinningTeamIndex);

	/** Initialize for new round. */
	virtual void InitRound();
};