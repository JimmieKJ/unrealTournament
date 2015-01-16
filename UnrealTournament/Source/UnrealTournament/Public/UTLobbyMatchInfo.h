// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "OnlineSubsystemTypes.h"
#include "UTGameMode.h"
#include "UTLobbyMatchInfo.generated.h"

namespace ELobbyMatchState
{
	extern const FName Dead;						// this match is dead and watching to be cleaned up.
	extern const FName Initializing;				// this match info is being initialized and waiting for a host to be assigned
	extern const FName Setup;						// the host has been assigned and the match is replicating all relevant data
	extern const FName WaitingForPlayers;			// The match is waiting for players to join and for the match to begin
	extern const FName Launching;					// The match is in the process of launching the instanced server
	extern const FName Aborting;					// The match is aborting the setup of a server...
	extern const FName InProgress;					// The match is in progress, the instanced server has ack'd the lobby server and everything is good to go.
	extern const FName Completed;					// The match is over and the instance has signaled it's been completed.  Spectators cant join at this point
	extern const FName Recycling;					// The match is over and the host has returned and create a new match.  It's sitting here waiting other others to return or for it to timeout.
	extern const FName Returning;					// the instance server has said the game is over and players should be returning to this server
}

DECLARE_DELEGATE(FOnMatchInfoGameModeChanged);
DECLARE_DELEGATE(FOnMatchInfoMapChanged);
DECLARE_DELEGATE(FOnMatchInfoOptionsChanged);

USTRUCT()
struct FPlayerListInfo
{
	GENERATED_USTRUCT_BODY()

	// The unique ID of this player.  This will be used to associate any incoming updates for a player
	UPROPERTY()
	FUniqueNetIdRepl PlayerID;

	// If this player is in the match and in this lobby server, then LocalPlayerState will be valid
	UPROPERTY()
	AUTLobbyPlayerState* LocalPlayerState;

	// The current name of this player
	UPROPERTY()
	FString PlayerName;

	// The current score for this player
	UPROPERTY()
	float PlayerScore;
	
	TArray<TSharedPtr<FAllowedMapData>> AvailableMaps;

	FPlayerListInfo() {};

	FPlayerListInfo(AUTLobbyPlayerState* inPlayerState)
	{
		if (inPlayerState)
		{
			PlayerID = inPlayerState->UniqueId;
			LocalPlayerState = inPlayerState;
			PlayerName = inPlayerState->PlayerName;
			PlayerScore = 0;
		}
	}

};

UCLASS(notplaceable)
class UNREALTOURNAMENT_API AUTLobbyMatchInfo : public AInfo
{
	GENERATED_UCLASS_BODY()
public:
	// We use  the FUniqueNetID of the owner to be the Anchor point for this object.  This way we can reassociated the MatchInfo with the player when they reenter a server from travel.
	UPROPERTY(Replicated)
	FUniqueNetIdRepl OwnerId;

	// The current state of this match.  
	UPROPERTY(Replicated)
	FName CurrentState;

	// if true, the owner will have to accept people joining this lobby
	UPROPERTY(Replicated)
	uint32 bPrivateMatch:1;

	// if true (defaults to true) then this match can be joined as a spectator.
	UPROPERTY(Replicated)
	uint32 bSpectatable:1;

	// if true (defaults to true) then people can join this match at any time
	UPROPERTY(Replicated)
	uint32 bJoinAnytime:1;

	// Holds data about the match.  In matches that are not started yet, it holds the description of the match.  In matches in progress, it's 
	// replicated data from the instance about the state of the match.  NOTE: Player information is not replicated from the instance to the server here
	// it's replicated in the PlayersInMatchInstance array.

	UPROPERTY(Replicated)
	FString MatchStats;

	// The options for this match
	UPROPERTY(Replicated, replicatedUsing = OnRep_MatchGameMode)
	FString MatchGameMode;

	// The options for this match
	UPROPERTY(Replicated, replicatedUsing = OnRep_MatchOptions)
	FString MatchOptions;

 	// The options for this match
	UPROPERTY(Replicated, replicatedUsing = OnRep_MatchMap)
	FString MatchMap;

	// The minimum number of players needed to start the match
	UPROPERTY(Replicated)
	int32 MinPlayers;

	// Number of players in this Match Lobby.  This valie us set on the Host and replicated to the server
	// via a function call.  It is NOT replicated to clients.
	int32 MaxPlayers;

	// A list of players in this lobby
	UPROPERTY(Replicated)
	TArray<AUTLobbyPlayerState*> Players;

	// This is the process handle of the game instance that is running.
	FProcHandle GameInstanceProcessHandle;

	// This is the lobby server generated instance id
	UPROPERTY()
	uint32 GameInstanceID;

	// The GUID for this game instance for spectating and join in progress
	UPROPERTY()
	FString GameInstanceGUID;

	// Holds a list of Unique IDs of players who are currently in the match.  When a player returns to lobby if their ID is in this list, they will be re-added to the match.
	UPROPERTY(Replicated)
	TArray<FPlayerListInfo> PlayersInMatchInstance;
	
	// Cache some data
	virtual void PreInitializeComponents() override;

	virtual void AddPlayer(AUTLobbyPlayerState* PlayerToAdd, bool bIsOwner = false);
	virtual bool RemovePlayer(AUTLobbyPlayerState* PlayerToRemove);
	virtual FText GetActionText();

	// The GameState needs to tell this MatchInfo what settings should be made available
	virtual void SetSettings(AUTLobbyGameState* GameState, AUTLobbyMatchInfo* MatchToCopy = NULL);

	// Called when the Host needs to tell the server that a GameMode has changed.
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerMatchGameModeChanged(const FString& NewMatchGameMode);

	// Called when the Host needs to tell the server that the selected map has changed
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerMatchMapChanged(const FString& NewMatchMap);

	// Called when the Host changes an option.  
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerMatchOptionsChanged(const FString& NewMatchOptions);

	virtual void SetMaxPlayers(int32 NewMaxPlayers)
	{
		MaxPlayers = FMath::Clamp<int32>(NewMaxPlayers, 2 , 32);
		ServerSetMaxPlayers(MaxPlayers);
	}

	// Called when the Host wants to set the MAX # of players in this match
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetMaxPlayers(int32 NewMaxPlayers);

	// Allows the current panel to trigger when something has changed
	FOnMatchInfoGameModeChanged OnMatchGameModeChanged;
	FOnMatchInfoMapChanged OnMatchMapChanged;
	FOnMatchInfoOptionsChanged OnMatchOptionsChanged;


	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerManageUser(int32 CommandID, AUTLobbyPlayerState* Target);

	TArray<FUniqueNetIdRepl> BannedIDs;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerStartMatch();
	
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerAbortMatch();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetLobbyMatchState(FName NewMatchState);

	virtual void SetLobbyMatchState(FName NewMatchState);
	virtual void GameInstanceReady(FGuid inGameInstanceGUID);

	/**
	 *	returns true if this player belongs in this match.  When a player joins a lobby server,
	 *  the server will see if that player belongs in any of the active matches.
	 **/
	bool WasInMatchInstance(AUTLobbyPlayerState* PlayerState);

	/**
	 *	Removes the player from 
	 **/
	void RemoveFromMatchInstance(AUTLobbyPlayerState* PlayerState);


	/**
	 *	returns true if this match is in progress already.
	 **/
	virtual bool IsInProgress();
	virtual bool ShouldShowInDock();

	// This will hold a referece to the CurrentGameModeData in the GameState or be invalid if not yet set
	TSharedPtr<FAllowedGameModeData> CurrentGameModeData;

	virtual void UpdateGameMode();

	virtual void ClientGetDefaultGameOptions();

	// Called from clients.  This will check to make sure all of the needed replicated information has arrived and that the player is ready to join.
	virtual bool MatchIsReadyToJoin(AUTLobbyPlayerState* Joiner);

	// Builds a list of maps based on the current game
	virtual void BuildAllowedMapsList();
	
	TArray<TSharedPtr<FAllowedMapData>> AvailableMaps;

protected:

	// Only available on the server, this holds a cached reference to the GameState.
	UPROPERTY()
	AUTLobbyGameState* LobbyGameState;


	// Called when Match Options change.  This should funnel the new options string to the UI and update everyone.
	UFUNCTION()
	virtual void OnRep_MatchOptions();
	
	UFUNCTION()
	virtual void OnRep_MatchGameMode();

	UFUNCTION()
	virtual void OnRep_MatchMap();

	// The client has received the OwnerID so we are good to go
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerMatchIsReadyForPlayers();

	/**
	 *	Returns the Owner's UTLobbyPlayerState
	 **/
	AUTLobbyPlayerState* GetOwnerPlayerState()
	{
		for (int32 i=0;i<Players.Num();i++)
		{
			if (Players[i] && Players[i]->UniqueId == OwnerId)
			{
				return Players[i];
			}
		}

		return NULL;
	}

	// This match info is done.  Kill it.
	void RecycleMatchInfo();

};



