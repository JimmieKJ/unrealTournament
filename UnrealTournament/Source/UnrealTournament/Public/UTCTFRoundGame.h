// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTCTFBaseGame.h"
#include "UTArmor.h"
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

	/** Respawn wait time for team with no life limit. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		float UnlimitedRespawnWaitTime;

	/** If true, round lives are per player. */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		bool bPerPlayerLives;

	UPROPERTY()
		bool bNeedFiveKillsMessage;

	UPROPERTY()
		bool bFirstRoundInitialized;

	UPROPERTY()
		int32 ExtraHealth;

	UPROPERTY()
		int32 FlagPickupDelay;

	UPROPERTY()
		int32 RemainingPickupDelay;

	virtual void InitFlags();

	virtual void FlagCountDown();

	virtual void AnnounceMatchStart() override;
	virtual void RestartPlayer(AController* aPlayer) override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	virtual bool CheckReachedGoalScore(AUTTeamInfo* ScoringTeam);
	void BuildServerResponseRules(FString& OutRules);
	virtual void HandleFlagCapture(AUTPlayerState* Holder) override;
	virtual void HandleExitingIntermission() override;
	virtual int32 IntermissionTeamToView(AUTPlayerController* PC) override;
	virtual void CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps);
	virtual void HandleMatchHasStarted() override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual bool AvoidPlayerStart(class AUTPlayerStart* P) override;
	virtual void DiscardInventory(APawn* Other, AController* Killer) override;
	virtual bool ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast) override;
	virtual void CheckGameTime() override;

	virtual void TossSkull(TSubclassOf<AUTSkullPickup> SkullPickupClass, const FVector& StartLocation, const FVector& TossVelocity, AUTCharacter* FormerInstigator);

	/** Score round ending due to team out of lives. */
	virtual void ScoreOutOfLives(int32 WinningTeamIndex);

	/** Initialize for new round. */
	virtual void InitRound();

	virtual void BroadcastVictoryConditions();

	TAssetSubclassOf<AUTArmor> ShieldBeltObject;
	TAssetSubclassOf<AUTArmor> ThighPadObject;

	UPROPERTY()
		TSubclassOf<AUTArmor> ShieldBeltClass;

	UPROPERTY()
		TSubclassOf<AUTArmor> ThighPadClass;

	virtual void GiveDefaultInventory(APawn* PlayerPawn) override;
};