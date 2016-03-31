// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFRoundGame.h"

#include "UTCTFRoundGameTommy.generated.h"


UCLASS()
class UNREALTOURNAMENT_API AUTCTFRoundGameTommy : public AUTCTFRoundGame
{
	GENERATED_UCLASS_BODY()

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	
	virtual void InitRound() override;

	virtual void RestartPlayer(AController* aPlayer) override;

	/** Score round ending due to team out of lives. */
	virtual void ScoreOutOfLives(int32 WinningTeamIndex) override;

	virtual void DiscardInventory(APawn* Other, AController* Killer) override;

	UPROPERTY()
	TSubclassOf<class AUTArmor> ChestArmorClass;
	TAssetSubclassOf<class AUTArmor> ChestArmorObject;

	virtual void GiveDefaultInventory(APawn* PlayerPawn) override;

	virtual void ToggleSpecialFor(AUTCharacter* C) override;

protected:
	virtual bool IsPlayerOnLifeLimitedTeam(AUTPlayerState* PlayerState) const override;

	int OffenseKills;
	int OffenseKillsNeededForPowerUp;

	int DefenseKills;
	int DefenseKillsNeededForPowerUp;
};