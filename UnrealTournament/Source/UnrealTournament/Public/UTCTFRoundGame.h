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

	/*  If single flag in game */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bOneFlagGameMode;

	/*  If true, trying to deliver own flag to enemy base */ 
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bCarryOwnFlag;

	/*  If true, no auto return flag on touch */ 
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bNoFlagReturn;

	/*  If true, slow flag carrier */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bSlowFlagCarrier;

	/*  If true, delay rally teleport */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bDelayedRally;

	/** If true, red team is trying to cap with asymmetric conditions. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bRedToCap;

	/** Respawn wait time for team with no life limit. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		float UnlimitedRespawnWaitTime;

	/** Limited Respawn wait time. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		float LimitedRespawnWaitTime;

	/** If true, round lives are per player. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bPerPlayerLives;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bAttackerLivesLimited;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bDefenderLivesLimited;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		int32 NumRounds;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		int OffenseKillsNeededForPowerUp;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		int DefenseKillsNeededForPowerUp;

	UPROPERTY()
		bool bNeedFiveKillsMessage;

	UPROPERTY()
		bool bFirstRoundInitialized;

	/*  Victory due to secondary score (best total capture time) */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bSecondaryWin;

	virtual void PlayEndOfMatchMessage() override;

	UPROPERTY()
		int32 FlagPickupDelay;

	UPROPERTY()
		int32 MaxTimeScoreBonus;

	UPROPERTY()
		bool bRollingAttackerSpawns;

	UPROPERTY()
		float RollingAttackerRespawnDelay;

	UPROPERTY()
		float LastAttackerSpawnTime;

	UPROPERTY()
		float RollingSpawnStartTime;

	UPROPERTY()
		bool bLastManOccurred;
	
	UPROPERTY(config)
		bool bShouldAllowPowerupSelection;

	UPROPERTY()
		int32 GoldBonusTime;

	UPROPERTY()
		int32 SilverBonusTime;

	UPROPERTY()
		int32 GoldScore;

	UPROPERTY()
		int32 SilverScore;

	UPROPERTY()
		int32 BronzeScore;

	// Score for a successful defense
	UPROPERTY()
		int32 DefenseScore;

	UPROPERTY()
		AUTPlayerState* FlagScorer;

	virtual int32 GetFlagCapScore() override;

	virtual void InitFlags();

	virtual void FlagCountDown();

	/** Update tiebreaker value based on new round bonus. Tiebreaker is positive if Red is ahead, negative if blue is ahead. */
	virtual void UpdateTiebreak(int32 Bonus, int32 TeamIndex);

	virtual void RemoveLosers(int32 LoserTeam, int32 FlagTeam);

	virtual void ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason) override;
	virtual void BroadcastCTFScore(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam, int32 OldScore = 0) override;
	virtual void RestartPlayer(AController* aPlayer) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	virtual bool CheckForWinner(AUTTeamInfo* ScoringTeam);
	void BuildServerResponseRules(FString& OutRules);
	virtual void HandleFlagCapture(AUTCharacter* HolderPawn, AUTPlayerState* Holder) override;
	virtual void HandleExitingIntermission() override;
	virtual int32 IntermissionTeamToView(AUTPlayerController* PC) override;
	virtual void CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps);
	virtual void HandleMatchHasStarted() override;
	virtual bool AvoidPlayerStart(class AUTPlayerStart* P) override;
	virtual void DiscardInventory(APawn* Other, AController* Killer) override;
	virtual bool ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast) override;
	virtual void CheckGameTime() override;
	virtual void HandleMatchIntermission() override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual float AdjustNearbyPlayerStartScore(const AController* Player, const AController* OtherController, const ACharacter* OtherCharacter, const FVector& StartLoc, const APlayerStart* P) override;
	virtual int32 PickCheatWinTeam() override;
	virtual void AdjustLeaderHatFor(AUTCharacter* UTChar) override;
	virtual bool SkipPlacement(AUTCharacter* UTChar) override;
	virtual void HandleCountdownToBegin() override;

	virtual void EndTeamGame(AUTTeamInfo* Winner, FName Reason);

	virtual bool UTIsHandlingReplays() override { return false; }
	virtual void StopRCTFReplayRecording();

	virtual void ScoreRedAlternateWin();
	virtual void ScoreBlueAlternateWin();

	/** Score round ending due some other reason than capture. */
	virtual void ScoreAlternateWin(int32 WinningTeamIndex, uint8 Reason = 0);

	/** Initialize for new round. */
	virtual void InitRound();

	/** Initialize a player for the new round. */
	virtual void InitPlayerForRound(AUTPlayerState* PS);

	TAssetSubclassOf<class AUTInventory> ActivatedPowerupPlaceholderObject;
	TAssetSubclassOf<class AUTInventory> RepulsorObject;

	UPROPERTY()
		TSubclassOf<class AUTInventory> ActivatedPowerupPlaceholderClass;

	virtual TSubclassOf<class AUTInventory> GetActivatedPowerupPlaceholderClass() { return ActivatedPowerupPlaceholderClass; };

	UPROPERTY()
		TSubclassOf<class AUTInventory> RepulsorClass;

	virtual void GiveDefaultInventory(APawn* PlayerPawn) override;

	UPROPERTY()
		bool bAllowPrototypePowerups;

	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	virtual bool IsTeamOnOffense(int32 TeamNumber) const;
	virtual bool IsTeamOnDefense(int32 TeamNumber) const;
	virtual bool IsPlayerOnLifeLimitedTeam(AUTPlayerState* PlayerState) const;

	virtual void HandlePowerupUnlocks(APawn* Other, AController* Killer);
	virtual void UpdatePowerupUnlockProgress(AUTPlayerState* VictimPS, AUTPlayerState* KillerPS);
	virtual void GrantPowerupToTeam(int TeamIndex, AUTPlayerState* PlayerToHighlight);

	UPROPERTY()
	int32 InitialBoostCount;

	// If true, players who join during the round, or switch teams during the round will be forced to
	// sit out and wait for the next round/
	UPROPERTY()
	bool bSitOutDuringRound;

};