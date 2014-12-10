// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLobbyPlayerState.h"
#include "UTServerBeaconLobbyHostListener.h"
#include "UTServerBeaconLobbyHostObject.h"
#include "UTLobbyGameState.generated.h"

USTRUCT()
struct FGameInstanceData
{
	GENERATED_USTRUCT_BODY()

	// The match info this insance is there for
	UPROPERTY() 
	AUTLobbyMatchInfo* MatchInfo;

	// The port that this game instance is running on
	UPROPERTY()
	int32 InstancePort;

	// The connection string to travel clients to.  NOTE: It includes the port in the format xxx.xxx.xxx.xxx:yyyy
	UPROPERTY()
	FString ConnectionString;

	FGameInstanceData()
		: MatchInfo(NULL)
		, InstancePort(0)
		, ConnectionString(TEXT(""))
	{};

	FGameInstanceData(AUTLobbyMatchInfo* inMatchInfo, int32 inInstancePort, FString inConnectionString)
		: MatchInfo(inMatchInfo)
		, InstancePort(inInstancePort)
		, ConnectionString(inConnectionString)
	{};

};

UCLASS(notplaceable, Config = Game)
class UNREALTOURNAMENT_API AUTLobbyGameState : public AUTGameState
{
	GENERATED_UCLASS_BODY()

	// Holds a list of running Game Instances.
	TArray<FGameInstanceData> GameInstances;

	/** Holds a list of GameMode classes that can be configured for this lobby.  In BeginPlay() these classes will be loaded and  */
	UPROPERTY(Config)
	TArray<FString> AllowedGameModeClasses;

	/** server settings */
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo)
	FString LobbyName;
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo)
	FString LobbyMOTD;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Lobby)
	TArray<AUTLobbyMatchInfo*> AvailableMatches;

	// A list of all maps that are allow the be chosen from.  We might want to consider auto-filling this but I want the
	// admin to be able to select which maps he wants to offer.  The Client will then filter the list by what maps they have installed.
	// We can at some point add a button or some mechanism  to allow players to pull missing maps from the marketplace.
	UPROPERTY(Config)
	TArray<FString> AllowedMaps;

	virtual void BroadcastChat(AUTLobbyPlayerState* SenderPS, FName Destination, const FString& Message);


	/**
	 *	Creates a new match and sets it's host.
	 **/
	virtual AUTLobbyMatchInfo* AddNewMatch(AUTLobbyPlayerState* HostOwner, AUTLobbyMatchInfo* MatchToCopy = NULL);

	/**
	 *	Sets someone as the host of a match and replicates all of the relevant match information to them
	 **/
	virtual void HostMatch(AUTLobbyMatchInfo* MatchInfo, AUTLobbyPlayerState* MatchOwner, AUTLobbyMatchInfo* MatchToCopy = NULL);

	/**
	 *	Joins an existing match.
	 **/
	virtual void JoinMatch(AUTLobbyMatchInfo* MatchInfo, AUTLobbyPlayerState* NewPlayer);

	/**
	 *	Removes a player from a match.
	 **/
	virtual void RemoveFromAMatch(AUTLobbyPlayerState* PlayerOwner);

	/**
	 *	Removes a match from the available matches array. 
	 **/
	virtual void RemoveMatch(AUTLobbyMatchInfo* MatchToRemove);

	virtual void SortPRIArray();

	/**
	 *	Sets up the BeaconLister and HostObjects needed to listen for updates from the 
	 **/
	void SetupLobbyBeacons();

	/**
	 *	Launches an instance of a game that was created via the lobby interface.  MatchOwner is the MI of the match that is being created and ServerURLOptions is a string
	 *  that contains the game options.  
	 **/
	void LaunchGameInstance(AUTLobbyMatchInfo* MatchOwner, FString ServerURLOptions);

	/**
	 *	Terminate an existing game instance
	 **/
	void TerminateGameInstance(AUTLobbyMatchInfo* MatchOwner);

	/**
	 *	Called when a Game Instance is up and ready for players to join.
	 **/
	void GameInstance_Ready(uint32 GameInstanceID, FGuid GameInstanceGUID);

	/**
	 *	Called when an instance needs to update it's match information
	 **/
	void GameInstance_MatchUpdate(uint32 GameInstanceID, const FString& NewDescription);

	/**
	 *	Called when an instance needs to update a player in a match's info
	 **/
	void GameInstance_PlayerUpdate(uint32 GameInstanceID, FUniqueNetIdRepl PlayerID, const FString& PlayerName, int32 PlayerScore);

	/**
	 *	Called when an instance's game is over.  It this called via GameEnded and doesn't mean any of the
	 *  players have started to transition back.  But the Panel should no longer allow spectators to join
	 **/
	void GameInstance_EndGame(uint32 GameInstanceID, const FString& FinalDescription);

	/**
	 *	Called when the instance server is ready.  When it is called, the Lobby will kill the server instance.
	 **/
	void GameInstance_Empty(uint32 GameInstanceID);

	void CheckForExistingMatch(AUTLobbyPlayerState* NewPlayer);

	bool IsMatchStillValid(AUTLobbyMatchInfo* TestMatch);

protected:

	// The instance ID of the game running in this lobby.  This ID will be used to identify any incoming data from the game server instance.  It's incremented each time a 
	// game server instance is launched.  NOTE: this id should never be 0.
	uint32 GameInstanceID;

	AUTServerBeaconLobbyHostListener* LobbyBeacon_Listener;
	AUTServerBeaconLobbyHostObject* LobbyBeacon_Object;

};



