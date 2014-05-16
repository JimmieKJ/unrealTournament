// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameState.generated.h"

UCLASS(minimalapi)
class AUTGameState : public AGameState
{
	GENERATED_UCLASS_BODY()

	/** If TRUE, then we weapon pick ups to stay on their base */
	UPROPERTY(Replicated)
	uint32 bWeaponStay:1;

	/** If TRUE, then the player has to signal ready before they can start */
	UPROPERTY(Replicated)
	uint32 bPlayerMustBeReady:1;

	/** If a single player's (or team's) score hits this limited, the game is over */
	UPROPERTY(Replicated)
	uint32 GoalScore;

	/** The maximum amount of time the game will be */
	UPROPERTY(Replicated)
	uint32 TimeLimit;

	// If true, we will stop counting down time
	UPROPERTY(Replicated)
	uint32 bStopCountdown:1;

	// Used to sync the time on clients to the server. -- See DefaultTimer()
	UPROPERTY(Replicated)
	uint32 RemainingMinute;

	/** How much time is remaining in this match. */
	UPROPERTY()
	uint32 RemainingTime;

	UPROPERTY(Replicated)
	AUTPlayerState* WinnerPlayerState;

	UFUNCTION()
	virtual void SetTimeLimit(float NewTimeLimit);

	UFUNCTION()
	virtual void SetGoalScore(uint32 NewGoalScore);


	/** Called once per second (or so depending on TimeDilation) after RemainingTime() has been replicated */
	virtual void DefaultTimer();

	/** Determines if a player is on the same team */
	virtual bool OnSameTeam(class APlayerState* Player1, class APlayerState* Player2);

};



