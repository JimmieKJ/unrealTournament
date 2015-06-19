// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "TimerManager.h"
#include "GameState.generated.h"

//=============================================================================
// GameState is replicated and is valid on servers and clients.
//=============================================================================

UCLASS(config=Game, notplaceable, BlueprintType, Blueprintable)
class ENGINE_API AGameState : public AInfo
{
	GENERATED_UCLASS_BODY()

	/** Class of the server's game mode, assigned by GameMode. */
	UPROPERTY(replicatedUsing=OnRep_GameModeClass)
	TSubclassOf<class AGameMode>  GameModeClass;

	/** Instance of the current game mode, exists only on the server. For non-authority clients, this will be NULL. */
	UPROPERTY(Transient, BlueprintReadOnly, Category=GameState)
	class AGameMode* AuthorityGameMode;

	/** Class used by spectators, assigned by GameMode. */
	UPROPERTY(replicatedUsing=OnRep_SpectatorClass)
	TSubclassOf<class ASpectatorPawn> SpectatorClass;

	// Code to deal with the match state machine

	/** Returns the current match state, this is an accessor to protect the state machine flow */
	FName GetMatchState() const { return MatchState; }

	/** Returns true if the match state is InProgress or later */
	virtual bool HasMatchStarted() const;

	/** Returns true if we're in progress */
	virtual bool IsMatchInProgress() const;

	/** Returns true if match is WaitingPostMatch or later */
	virtual bool HasMatchEnded() const;

	/** Updates the match state and calls the appropriate transition functions, only valid on server */
	void SetMatchState(FName NewState);

protected:

	/** What match state we are currently in */
	UPROPERTY(replicatedUsing=OnRep_MatchState)
	FName MatchState;

	/** Previous map state, used to handle if multiple transitions happen per frame */
	UPROPERTY()
	FName PreviousMatchState;

	/** Called when the state transitions to WaitingToStart */
	virtual void HandleMatchIsWaitingToStart();

	/** Called when the state transitions to InProgress */
	virtual void HandleMatchHasStarted();

	/** Called when the map transitions to WaitingPostMatch */
	virtual void HandleMatchHasEnded();

	/** Called when the match transitions to LeavingMap */
	virtual void HandleLeavingMap();

public:

	/** Elapsed game time since match has started. */
	UPROPERTY(replicatedUsing=OnRep_ElapsedTime, BlueprintReadOnly, Category = GameState)
	int32 ElapsedTime;

	/** Array of all PlayerStates, maintained on both server and clients (PlayerStates are always relevant) */
	UPROPERTY(BlueprintReadOnly, Category=GameState)
	TArray<class APlayerState*> PlayerArray;

	/** This list mirrors the GameMode's list of inactive PlayerState objects. */
	UPROPERTY()
	TArray<class APlayerState*> InactivePlayerArray;

	/** GameMode class notification callback. */
	UFUNCTION()
	virtual void OnRep_GameModeClass();

	/** Callback when we receive the spectator class */
	UFUNCTION()
	virtual void OnRep_SpectatorClass();

	/** Match state has changed */
	UFUNCTION()
	virtual void OnRep_MatchState();

	/** Gives clients the chance to do something when time gets updates */
	UFUNCTION()
	virtual void OnRep_ElapsedTime();

	// Begin AActor interface
	virtual void PostInitializeComponents() override;
	// End AActor interface

	/** Helper to return the default object of the GameMode class corresponding to this GameState */
	AGameMode* GetDefaultGameMode() const;
	
	/** Called when the GameClass property is set (at startup for the server, after the variable has been replicated on clients) */
	virtual void ReceivedGameModeClass();

	/** Called when the SpectatorClass property is set (at startup for the server, after the variable has been replicated on clients) */
	virtual void ReceivedSpectatorClass();

	/** Called during seamless travel transition twice (once when the transition map is loaded, once when destination map is loaded) */
	virtual void SeamlessTravelTransitionCheckpoint(bool bToTransitionMap);

	/** Add PlayerState to the PlayerArray */
	virtual void AddPlayerState(class APlayerState* PlayerState);

	/** Remove PlayerState from the PlayerArray. */
	virtual void RemovePlayerState(class APlayerState* PlayerState);

	/** Called periodically, overridden by subclasses */
	virtual void DefaultTimer();

	/** Should players show gore? */
	virtual bool ShouldShowGore() const;

protected:

	/** Handle for efficient management of DefaultTimer timer */
	FTimerHandle TimerHandle_DefaultTimer;

private:
	// Hidden functions that don't make sense to use on this class.
	HIDE_ACTOR_TRANSFORM_FUNCTIONS();
};



