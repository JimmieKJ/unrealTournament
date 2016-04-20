// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFGameState.h"
#include "UTCTFRoundGameState.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFRoundGameState : public AUTCTFGameState
{
	GENERATED_UCLASS_BODY()

		UPROPERTY(Replicated)
		uint32 IntermissionTime;

		UPROPERTY(Replicated)
		int OffenseKills;

		UPROPERTY(Replicated)
		int DefenseKills;

		UPROPERTY(Replicated)
		int OffenseKillsNeededForPowerup;

		UPROPERTY(Replicated)
		int DefenseKillsNeededForPowerup;

	virtual float GetIntermissionTime() override;
	virtual void DefaultTimer() override;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual bool IsTeamOnOffense(int32 TeamNumber) const;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual bool IsTeamOnDefense(int32 TeamNumber) const;

	// return % that represense the # of kills needed to get a bonus.  1.0 = Bonus is available
	virtual float GetKillsNeededForPowerup(bool bOnOffense);

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual bool IsTeamOnDefenseNextRound(int32 TeamNumber) const;
};
