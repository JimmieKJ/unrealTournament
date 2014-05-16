// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameMode.generated.h"

/** Defines the current state of the game. */

UCLASS(minimalapi, dependson=AUTGameState)
class AUTGameMode : public AGameMode
{
	GENERATED_UCLASS_BODY()

	/** Currently not used, but will be needed later*/
	UPROPERTY(globalconfig)
	float GameDifficulty;		

	/** How long to wait after the end of a match before the transition to the new level start */
	UPROPERTY(globalconfig)
	float EndTimeDelay;			

	/** If TRUE, force dead players to respawn immediately */
	UPROPERTY(globalconfig)
	uint32 bForceRespawn:1;		

	/** Score needed to win the match.  Can be overridden with GOALSCORE=x on the url */
	UPROPERTY(config)
	int32 GoalScore;    

	/** How long should the match be.  Can be overridden with TIMELIMIT=x on the url */
	UPROPERTY(config)
	int32 TimeLimit;    

	/** Will be TRUE if the game has ended */
	UPROPERTY()
	uint32 bGameEnded:1;    

	/** Will be TRUE if this is a test game */
	UPROPERTY()
	uint32 bTeamGame:1;

	/** TRUE if we have started the count down to the match starting */
	UPROPERTY()
	uint32 bStartedCountDown:1;

	/** # of seconds before the match begins */
	UPROPERTY()
	int32 CountDown;

	/** Holds the last place any player started from */
	UPROPERTY()
	class AActor* LastStartSpot;    // last place any player started from

	/** Timestamp of when this game ended */
	UPROPERTY()
	float EndTime;

	/** Which actor in the game should all other actors focus on after the game is over */
	UPROPERTY()
	class AActor* EndGameFocus;

	UPROPERTY()
	TSubclassOf<class UUTLocalMessage>  DeathMessageClass;

	UPROPERTY()
	TSubclassOf<class UUTLocalMessage>  GameMessageClass;
	
public:

protected:
};




