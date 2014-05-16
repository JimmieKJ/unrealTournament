// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameMode.generated.h"

/** Defines the current state of the game. */

UCLASS(minimalapi, dependson=UTGameState)
class AUTGameMode : public AGameMode
{
	GENERATED_UCLASS_BODY()

public:
	/** Cached reference to our game state for quick access. */
	UPROPERTY()
	AUTGameState* UTGameState;		

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

	UPROPERTY()
	TSubclassOf<class UUTLocalMessage>  VictoryMessageClass;

	//UFUNCTION()
	//virtual void SetGameStage(EGameStage NewGameStage);

	virtual void Reset() OVERRIDE;
	virtual void StartNewPlayer(class APlayerController* NewPlayer);
	virtual bool IsEnemy(class AController* First, class AController* Second);
	virtual void Killed( class AController* Killer, class AController* KilledPlayer, class APawn* KilledPawn, const class UDamageType* DamageType );
	virtual void ScoreKill(AController* Killer, AController* Other);
	virtual bool CheckScore(AUTPlayerState* Scorer);
	virtual bool AUTGameMode::IsAWinner(AUTPlayerController* PC);
	virtual void EndGame(AUTPlayerState* Winner, const FString& Reason);
	virtual void EndMatch();

	virtual void BroadcastDeathMessage(AController* Killer, AController* Other, const UDamageType* DamageType);
	virtual void PlayEndOfMatchMessage();

protected:
};




