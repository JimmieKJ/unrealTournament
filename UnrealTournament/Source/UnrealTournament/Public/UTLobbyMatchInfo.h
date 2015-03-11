// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "OnlineSubsystemTypes.h"
#include "TAttributeProperty.h"
#include "UTGameMode.h"
#include "UTLobbyMatchInfo.generated.h"

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
	TWeakObjectPtr<AUTLobbyPlayerState> LocalPlayerState;

	// The current name of this player
	UPROPERTY()
	FString PlayerName;

	// The current score for this player
	UPROPERTY()
	float PlayerScore;
	
	TArray<TSharedPtr<FAllowedMapData>> AvailableMaps;

	FPlayerListInfo() {};

	FPlayerListInfo(TWeakObjectPtr<AUTLobbyPlayerState> inPlayerState)
	{
		if (inPlayerState.IsValid())
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

	// If true, please must be under the rank of the host
	UPROPERTY(Replicated)
	int32 RankCeiling;

	// Holds data about the match.  In matches that are not started yet, it holds the description of the match.  In matches in progress, it's 
	// replicated data from the instance about the state of the match.  NOTE: Player information is not replicated from the instance to the server here
	// it's replicated in the PlayersInMatchInstance array.

	UPROPERTY(Replicated, replicatedUsing = OnRep_MatchStats)
	FString MatchStats;

	UPROPERTY(Replicated, replicatedUsing = OnRep_MatchBadge)
	FString MatchBadge;

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
	UPROPERTY(Replicated)
	int32 MaxPlayers;

	// A list of players in this lobby
	UPROPERTY(Replicated, replicatedUsing = OnRep_Players)
	TArray<TWeakObjectPtr<AUTLobbyPlayerState>> Players;

	// This is the process handle of the game instance that is running.
	FProcHandle GameInstanceProcessHandle;

	// This is the lobby server generated instance id
	UPROPERTY()
	uint32 GameInstanceID;

	// The GUID for this game instance for spectating and join in progress
	UPROPERTY()
	FString GameInstanceGUID;

	// Holds a list of Unique IDs of players who are currently in the match.  When a player returns to lobby if their ID is in this list, they will be re-added to the match.
	UPROPERTY(Replicated, replicatedUsing = OnRep_PlayersInMatch)
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
		MaxPlayers = FMath::Clamp<int32>(NewMaxPlayers, 2 , 20);
		ServerSetMaxPlayers(MaxPlayers);
	}

	// Called when the Host wants to set the MAX # of players in this match
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetMaxPlayers(int32 NewMaxPlayers);


	virtual void SetAllowJoinInProgress(bool bAllow)
	{
		bJoinAnytime = bAllow;
		ServerSetAllowJoinInProgress(bAllow);
	}

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetAllowJoinInProgress(bool bAllow);

	virtual void SetAllowSpectating(bool bAllow)
	{
		bSpectatable = bAllow;
		ServerSetAllowSpectating(bAllow);
	}

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetAllowSpectating(bool bAllow);

	void SetRankCeiling(int32 NewRankCeiling)
	{
		ServerSetRankCeiling(NewRankCeiling);
	}

	
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetRankCeiling(int32 NewRankCeiling);


	// Allows the current panel to trigger when something has changed
	FOnMatchInfoGameModeChanged OnMatchGameModeChanged;
	FOnMatchInfoMapChanged OnMatchMapChanged;
	FOnMatchInfoOptionsChanged OnMatchOptionsChanged;


	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerManageUser(int32 CommandID, AUTLobbyPlayerState* Target);

	TArray<FUniqueNetIdRepl> BannedIDs;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerStartMatch();
	
	// Actually launch the map.  NOTE: This is used for QuickStart and doesn't check any of the "can I launch" metrics.
	virtual void LaunchMatch();

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

	// This will be true if this match is a dedicated match and shouldn't ever go down
	UPROPERTY(Replicated)
	bool bDedicatedMatch;

	FText GetDebugInfo();

	bool SkillTest(int32 Rank)
	{
		return RankCeiling < 1 || Rank <= RankCeiling + 400; // MAKE ME CONFIG
	}

	/**
	 *	Returns the Owner's UTLobbyPlayerState
	 **/
	TWeakObjectPtr<AUTLobbyPlayerState> GetOwnerPlayerState()
	{
		for (int32 i=0;i<Players.Num();i++)
		{
			if (Players[i].IsValid() && Players[i]->UniqueId == OwnerId)
			{
				return Players[i];
			}
		}

		return NULL;
	}

protected:

	// Only available on the server, this holds a cached reference to the GameState.
	UPROPERTY()
	TWeakObjectPtr<AUTLobbyGameState> LobbyGameState;

	FText MatchElapsedTime;

	// Called when Match Options change.  This should funnel the new options string to the UI and update everyone.
	UFUNCTION()
	virtual void OnRep_MatchOptions();
	
	UFUNCTION()
	virtual void OnRep_MatchGameMode();

	UFUNCTION()
	virtual void OnRep_MatchMap();

	UFUNCTION()
	virtual void OnRep_MatchStats();

	UFUNCTION()
	virtual void OnRep_Players();

	UFUNCTION()
	virtual void OnRep_PlayersInMatch();

	UFUNCTION()
	virtual void OnRep_MatchBadge();

	// The client has received the OwnerID so we are good to go
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerMatchIsReadyForPlayers();

	// This match info is done.  Kill it.
	void RecycleMatchInfo();

	bool CheckLobbyGameState();
	void UpdateBadgeForNewGameMode();
};



