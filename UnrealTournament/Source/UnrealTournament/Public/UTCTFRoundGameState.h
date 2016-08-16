// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFGameState.h"
#include "UTCTFRoundGameState.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFRoundGameState : public AUTCTFGameState
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Replicated)
	int32 IntermissionTime;

	UPROPERTY(Replicated)
		int32 OffenseKills;

	UPROPERTY(Replicated)
		int32 DefenseKills;

	UPROPERTY(Replicated)
		int32 OffenseKillsNeededForPowerup;

	UPROPERTY(Replicated)
		int32 DefenseKillsNeededForPowerup;

	UPROPERTY(Replicated)
		bool bIsDefenseAbleToGainPowerup;

	UPROPERTY(Replicated)
		bool bIsOffenseAbleToGainPowerup;

	UPROPERTY(Replicated)
		bool bUsePrototypePowerupSelect;

	UPROPERTY(Replicated)
		int32 GoldBonusThreshold;

	UPROPERTY(Replicated)
		int32 SilverBonusThreshold;

	UPROPERTY(Replicated)
		int32 RemainingPickupDelay;

	UPROPERTY(ReplicatedUsing=OnBonusLevelChanged)
		uint8 BonusLevel;

	UPROPERTY(Replicated)
		int32 FlagRunMessageSwitch;

	UPROPERTY(Replicated)
		int32 TiebreakValue;

	UPROPERTY(Replicated)
		class AUTTeamInfo* FlagRunMessageTeam;

	UPROPERTY()
		FText GoldBonusText;

	UPROPERTY()
		FText SilverBonusText;

	UPROPERTY()
		FText BronzeBonusText;

	UPROPERTY()
		FText GoldBonusTimedText;

	UPROPERTY()
		FText SilverBonusTimedText;

	UFUNCTION()
	void OnBonusLevelChanged();

	virtual FText GetGameStatusText(bool bForScoreboard) override;
	virtual float GetIntermissionTime() override;
	virtual void DefaultTimer() override;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual bool IsTeamOnOffense(int32 TeamNumber) const;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual bool IsTeamOnDefense(int32 TeamNumber) const;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual int GetKillsNeededForPowerup(int32 TeamNumber) const;
	
	UFUNCTION(BlueprintCallable, Category = Team)
	virtual bool IsTeamAbleToEarnPowerup(int32 TeamNumber) const;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual bool IsTeamOnDefenseNextRound(int32 TeamNumber) const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual TSubclassOf<class AUTInventory> GetSelectableBoostByIndex(AUTPlayerState* PlayerState, int Index) const override;

	//Handles precaching all game announcement sounds for the local player
	virtual void PrecacheAllPowerupAnnouncements(class UUTAnnouncer* Announcer) const;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = GameState)
	FString GetPowerupSelectWidgetPath(int32 TeamNumber);

	virtual bool InOrder(class AUTPlayerState* P1, class AUTPlayerState* P2) override;

protected:
	virtual void UpdateTimeMessage();

	virtual void UpdateSelectablePowerups();
	virtual void AddModeSpecificOverlays();

	virtual void CachePowerupAnnouncement(class UUTAnnouncer* Announcer, TSubclassOf<AUTInventory> PowerupClass) const;

	UPROPERTY()
	TArray<TSubclassOf<class AUTInventory>> DefenseSelectablePowerups;

	UPROPERTY()
	TArray<TSubclassOf<class AUTInventory>> OffenseSelectablePowerups;

};
