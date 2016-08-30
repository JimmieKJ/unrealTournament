// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTCTFRoundGame.h"
#include "UTATypes.h"
#include "UTFlagRunGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagRunGame : public AUTCTFRoundGame
{
	GENERATED_UCLASS_BODY()

public:

	/** optional class spawned at source location after translocating that continues to receive damage for a short duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TSubclassOf<class AUTWeaponRedirector> AfterImageType;

	UPROPERTY()
		float RallyRequestTime;

	UPROPERTY()
		float LastEntryDefenseWarningTime;

	UPROPERTY()
		float LastEnemyRallyWarning;

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

	FTimerHandle EnemyRallyWarningHandle;

	virtual void WarnEnemyRally();

	virtual void AnnounceWin(AUTTeamInfo* WinningTeam, uint8 Reason) override;

	virtual float OverrideRespawnTime(TSubclassOf<AUTInventory> InventoryType) override;
	virtual void HandleRallyRequest(AUTPlayerController* PC) override;
	virtual void CompleteRallyRequest(AUTPlayerController* PC) override;
	virtual bool CheckForWinner(AUTTeamInfo* ScoringTeam) override;
	virtual int32 PickCheatWinTeam() override;
	virtual bool AvoidPlayerStart(class AUTPlayerStart* P) override;
	virtual void InitDelayedFlag(class AUTCarriedObject* Flag) override;
	virtual void InitFlagForRound(class AUTCarriedObject* Flag) override;
	virtual void IntermissionSwapSides() override;
	virtual int32 GetFlagCapScore() override;
	virtual int32 GetDefenseScore() override;
	virtual void BroadcastCTFScore(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam, int32 OldScore = 0) override;
	virtual void InitGameState() override;
	virtual void CheckRoundTimeVictory() override;

	virtual int32 GetComSwitch(FName CommandTag, AActor* ContextActor, AUTPlayerController* Instigator, UWorld* World);
	virtual void InitFlags() override;
	virtual void HandleMatchIntermission() override;
	virtual void CheatScore() override;
	virtual void DefaultTimer() override;

	virtual void UpdateSkillRating() override;

	virtual uint8 GetNumMatchesFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual int32 GetEloFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual void SetEloFor(AUTPlayerState* PS, bool bRankedSession, int32 NewELoValue, bool bIncrementMatchCount) override;
};

