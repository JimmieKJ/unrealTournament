// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "OnlineSubsystemTypes.h"
#include "UTLobbyMatchInfo.generated.h"

namespace ELobbyMatchState
{
	extern const FName Dead;						// this match is dead and watching to be cleaned up.
	extern const FName Setup;						// We are entering this map, actors are not yet ticking
	extern const FName WaitingForPlayers;			// The game is entering overtime
	extern const FName CountdownToGo;				// The game is in overtime
	extern const FName Launching;					// The game is in the process of launching the instanced server
	extern const FName InProgress;					// The game is in progress, the instanced server has ack'd the lobby server and everything is good to go.
	extern const FName Returning;					// the instance server has said the game is over and players should be returning to this server
}


UCLASS(notplaceable)
class UNREALTOURNAMENT_API AUTLobbyMatchInfo : public AInfo
{
	GENERATED_UCLASS_BODY()

	// We use  the FUniqueNetID of the owner to be the Anchor point for this object.  This way we can reassociated the MatchInfo with the player when they reenter a server from travel.
	UPROPERTY(Replicated)
	FUniqueNetIdRepl OwnerID;

	// The current state of this match.  
	UPROPERTY(Replicated)
	FName CurrentState;

	// The PlayerState of the player that currently owns this match info
	UPROPERTY(Replicated)
	AUTLobbyPlayerState* OwnersPlayerState;

	// if true, the owner will have to accept people joining this lobby
	UPROPERTY(Replicated)
	uint32 bPrivateMatch:1;

	// if true (defaults to true) then this match can be joined as a spectator.
	UPROPERTY(Replicated)
	uint32 bSpectatable:1;

	// The name of this lobby.
	UPROPERTY(Replicated)
	FString MatchDescription;

	// The options for this match
	UPROPERTY(Replicated, replicatedUsing = OnRep_MatchOptions)
	FString MatchOptions;

	// Number of players in this Match Lobby
	UPROPERTY(Replicated)
	int32 MaxPlayers;

	// A list of players in this lobby
	UPROPERTY(Replicated)
	TArray<AUTLobbyPlayerState*> Players;

	// Cache some data
	virtual void PreInitializeComponents() override;

	// Sends a text message to everyone currently in this match
	virtual void BroadcastMatchMessage(AUTLobbyPlayerState* SenderPS, const FString& Message);

	virtual void AddPlayer(AUTLobbyPlayerState* PlayerToAdd, bool bIsOwner = false);
	virtual void RemovePlayer(AUTLobbyPlayerState* PlayerToRemove);

protected:
	
	void SetLobbyMatchState(FName NewMatchState);

	// Only available on the server, this holds a cached reference to the GameState.
	AUTLobbyGameState* LobbyGameState;

	// Called when Match Options change.  This should funnel the new options string to the UI and update everyone.
	UFUNCTION()
	virtual void OnRep_MatchOptions();
};



