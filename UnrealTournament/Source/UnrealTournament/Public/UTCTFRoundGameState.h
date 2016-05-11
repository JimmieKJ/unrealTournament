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
		int32 GoldBonusThreshold;

	UPROPERTY(Replicated)
		int32 SilverBonusThreshold;

	UPROPERTY(Replicated)
		int32 RemainingPickupDelay;

	UPROPERTY(ReplicatedUsing=OnBonusLevelChanged)
		uint8 BonusLevel;

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

protected:
	virtual void UpdateTimeMessage();
		

};
