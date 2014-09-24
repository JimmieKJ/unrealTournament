// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "UTLobbyMatchInfo.generated.h"

UCLASS(notplaceable)
class UNREALTOURNAMENT_API AUTLobbyMatchInfo : public AInfo
{
	GENERATED_UCLASS_BODY()

	// The PlayerState of the player that currently owns this match info
	UPROPERTY(Replicated)
	AUTLobbyPlayerState* OwnersPlayerState;

	// if true, the owner will have to accept people joining this lobby
	UPROPERTY(Replicated)
	uint32 bPrivateMatch;

	// The name of this lobby.
	UPROPERTY(Replicated)
	FString MatchDescription;

	// The options for this match
	UPROPERTY(Replicated, replicatedUsing = OnRep_MatchOptions)
	FString MatchOptions;

	// Number of players in this Match Lobby
	UPROPERTY(Replicated)
	int32 NumPlayers;

	// Cache some data
	virtual void PreInitializeComponents() override;

	// Sends a text message to everyone currently in this match
	virtual void BroadcastMatchMessage(AUTLobbyPlayerState* SenderPS, const FString& Message);

protected:
	
	// Only available on the server, this holds a cached reference to the GameState.
	AUTLobbyGameState* LobbyGameState;

	// Called when Match Options change.  This should funnel the new options string to the UI and update everyone.
	UFUNCTION()
	virtual void OnRep_MatchOptions();
};



