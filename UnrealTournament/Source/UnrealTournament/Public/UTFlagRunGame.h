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
		int32 GoldScore;

	UPROPERTY()
		int32 SilverScore;

	UPROPERTY()
		int32 BronzeScore;

	// Score for a successful defense
	UPROPERTY()
		int32 DefenseScore;

	UPROPERTY()
		bool bAllowPrototypePowerups;

	UPROPERTY()
		bool bAllowBoosts;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		int OffenseKillsNeededForPowerUp;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		int DefenseKillsNeededForPowerUp;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		class AUTCarriedObject* ActiveFlag;

	TAssetSubclassOf<class AUTInventory> ActivatedPowerupPlaceholderObject;
	TAssetSubclassOf<class AUTInventory> RepulsorObject;

	UPROPERTY()
		TSubclassOf<class AUTInventory> ActivatedPowerupPlaceholderClass;

	virtual TSubclassOf<class AUTInventory> GetActivatedPowerupPlaceholderClass() { return ActivatedPowerupPlaceholderClass; };

	UPROPERTY()
		TSubclassOf<class AUTInventory> RepulsorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		USoundBase* RallyFailedSound;

	UPROPERTY()
		FName LastAttackerSpawnGroup;

	UPROPERTY()
		FName LastDefenderSpawnGroup;

	virtual class AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName = TEXT("")) override;
	virtual void AnnounceWin(AUTTeamInfo* WinningTeam, APlayerState* ScoringPlayer, uint8 Reason) override;
	virtual void NotifyFirstPickup(AUTCarriedObject* Flag) override;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual float OverrideRespawnTime(AUTPickupInventory* Pickup, TSubclassOf<AUTInventory> InventoryType) override;
	virtual bool HandleRallyRequest(AController* C) override;
	virtual void CompleteRallyRequest(AController* C) override;
	virtual bool CheckForWinner(AUTTeamInfo* ScoringTeam) override;
	virtual int32 PickCheatWinTeam() override;
	virtual bool AvoidPlayerStart(class AUTPlayerStart* P) override;
	virtual void InitDelayedFlag(class AUTCarriedObject* Flag) override;
	virtual void InitFlagForRound(class AUTCarriedObject* Flag) override;
	virtual void IntermissionSwapSides() override;
	virtual int32 GetFlagCapScore() override;
	virtual int32 GetDefenseScore() override;
	virtual void BroadcastCTFScore(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam, int32 OldScore = 0) override;
	virtual void CheckRoundTimeVictory() override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual void HandleTeamChange(AUTPlayerState* PS, AUTTeamInfo* OldTeam) override;
	virtual void InitGameStateForRound() override;
	virtual bool IsTeamOnOffense(int32 TeamNumber) const override;
	virtual AActor* SetIntermissionCameras(uint32 TeamToWatch) override;
	virtual void SendRestartNotifications(AUTPlayerState* PS, AUTPlayerController* PC) override;

	virtual int32 GetComSwitch(FName CommandTag, AActor* ContextActor, AUTPlayerController* Instigator, UWorld* World);
	virtual void InitFlags() override;
	virtual void HandleMatchIntermission() override;
	virtual void CheatScore() override;
	virtual void DefaultTimer() override;
	virtual void PlayEndOfMatchMessage() override;

	virtual void UpdateSkillRating() override;

	virtual FString GetRankedLeagueName() override;

	virtual float RatePlayerStart(APlayerStart* P, AController* Player) override;

	virtual uint8 GetNumMatchesFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual int32 GetEloFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual void SetEloFor(AUTPlayerState* PS, bool bRankedSession, int32 NewELoValue, bool bIncrementMatchCount) override;

	virtual void ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason) override;
	virtual void ScoreAlternateWin(int32 WinningTeamIndex, uint8 Reason = 0);

	virtual bool SupportsInstantReplay() const override;
	virtual void FindAndMarkHighScorer() override;
	virtual void HandleRollingAttackerRespawn(AUTPlayerState* OtherPS) override;

protected:

	virtual void HandlePowerupUnlocks(APawn* Other, AController* Killer);
	virtual void UpdatePowerupUnlockProgress(AUTPlayerState* VictimPS, AUTPlayerState* KillerPS);
	virtual void GrantPowerupToTeam(int TeamIndex, AUTPlayerState* PlayerToHighlight);
};

