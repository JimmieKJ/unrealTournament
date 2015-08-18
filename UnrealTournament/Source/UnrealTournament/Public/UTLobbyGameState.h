// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLobbyPlayerState.h"
#include "UTServerBeaconLobbyHostListener.h"
#include "UTServerBeaconLobbyHostObject.h"
#include "UTReplicatedGameRuleset.h"
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

	FGameInstanceData()
		: MatchInfo(NULL)
		, InstancePort(0)
	{};

	FGameInstanceData(AUTLobbyMatchInfo* inMatchInfo, int32 inInstancePort)
		: MatchInfo(inMatchInfo)
		, InstancePort(inInstancePort)
	{};

};


class AUTGameMode;

UCLASS(notplaceable, Config = Game)
class UNREALTOURNAMENT_API AUTLobbyGameState : public AUTGameState
{
	GENERATED_UCLASS_BODY()

	virtual void PostInitializeComponents() override;

	// Holds a list of running Game Instances.
	TArray<FGameInstanceData> GameInstances;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Lobby)
	TArray<AUTLobbyMatchInfo*> AvailableMatches;

	TArray<FProcHandle> ProcessesToGetReturnCode;

	// These Game Options will be forced on ALL games instanced regardless of their GameMdoe
	UPROPERTY(Config)
	FString ForcedInstanceGameOptions;

	// These additional commandline parameters will be added to all instances that run.
	UPROPERTY(Config)
	FString AdditionalInstanceCommandLine;

	// Holds a list of keys used for dedicated instances to connect to this hub.
	UPROPERTY(Config)
	TArray<FString> AccessKeys;

	/** maintains a reference to gametypes that have been requested as the UI refs are weak pointers and won't prevent GC on their own */
	UPROPERTY(Transient)
	TArray<UClass*> LoadedGametypes;

	virtual void BroadcastChat(AUTLobbyPlayerState* SenderPS, FName Destination, const FString& Message);

	/**
	 *	Creates a new match and sets it's host.
	 **/
	virtual AUTLobbyMatchInfo* AddNewMatch(AUTLobbyPlayerState* HostOwner, AUTLobbyMatchInfo* MatchToCopy = NULL);

	/**
	 *	Create a quick match for a quick start player then get them in to it without the typical setup
	 **/
	virtual AUTLobbyMatchInfo* QuickStartMatch(AUTLobbyPlayerState* Host, bool bIsCTFMatch);

	/**
	 *	Sets someone as the host of a match and replicates all of the relevant match information to them
	 **/
	virtual void HostMatch(AUTLobbyMatchInfo* MatchInfo, AUTLobbyPlayerState* MatchOwner, AUTLobbyMatchInfo* MatchToCopy = NULL);

	/**
	 *	Joins an existing match.
	 **/
	virtual void JoinMatch(AUTLobbyMatchInfo* MatchInfo, AUTLobbyPlayerState* NewPlayer, bool bAsSpectator=false);

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
	void LaunchGameInstance(AUTLobbyMatchInfo* MatchOwner, FString GameURL);

	/**
	 *	Create the default "MATCH" for the server.
	 **/
	void CreateAutoMatch(FString MatchGameMode, FString MatchOptions, FString MatchMap);

	/**
	 *	Terminate an existing game instance
	 **/
	void TerminateGameInstance(AUTLobbyMatchInfo* MatchOwner, bool bAborting = false);

	/**
	 *	Called when a Game Instance is up and ready for players to join.
	 **/
	void GameInstance_Ready(uint32 GameInstanceID, FGuid GameInstanceGUID);

	/**
	 *	Called when an instance needs to update it's match information
	 **/
	void GameInstance_MatchUpdate(uint32 GameInstanceID, const FString& NewDescription);

	/**
	 *	Called when an instance needs to update it's badge information
	 **/
	void GameInstance_MatchBadgeUpdate(uint32 GameInstanceID, const FString& NewDescription);


	/**
	 *	Called when an instance needs to update a player in a match's info
	 **/
	void GameInstance_PlayerUpdate(uint32 GameInstanceID, FUniqueNetIdRepl PlayerID, const FString& PlayerName, int32 PlayerScore, bool bSpectator, uint8 TeamNum, bool bLastUpdate, int32 PlayerRank, FName Avatar);

	/**
	 *	Called when an instance's game is over.  It this called via GameEnded and doesn't mean any of the
	 *  players have started to transition back.  But the Panel should no longer allow spectators to join
	 **/
	void GameInstance_EndGame(uint32 GameInstanceID, const FString& FinalDescription);

	/**
	 *	Called when the instance server is ready.  When it is called, the Lobby will kill the server instance.
	 **/
	void GameInstance_Empty(uint32 GameInstanceID);

	void CheckForExistingMatch(AUTLobbyPlayerState* NewPlayer, bool bReturnedFromMatch);

	bool IsMatchStillValid(AUTLobbyMatchInfo* TestMatch);

	// Called when a new player enters the game.  This causes all of the allowed gametypes and maps to be replicated to that player
	void InitializeNewPlayer(AUTLobbyPlayerState* NewPlayer);

	// returns true if a match can start
	bool CanLaunch();

	void BeginPlay();

protected:

	// The instance ID of the game running in this lobby.  This ID will be used to identify any incoming data from the game server instance.  It's incremented each time a 
	// game server instance is launched.  NOTE: this id should never be 0.
	uint32 GameInstanceID;

	uint32 GameInstanceListenPort;

	AUTServerBeaconLobbyHostListener* LobbyBeacon_Listener;
	AUTServerBeaconLobbyHostObject* LobbyBeacon_Object;

	void CheckInstanceHealth();

	AUTLobbyMatchInfo* FindMatchPlayerIsIn(FString PlayerID);

	// We replicate the count seperately so that the client can check and test their array to see if replication has been completed.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Lobby)
	int32 AvailabelGameRulesetCount;

	void ScanAssetRegistry(TArray<FAssetData>& MapAssets);

public:
	// The actual cached copy of all of the game rulesets
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Lobby)
	TArray<TWeakObjectPtr<AUTReplicatedGameRuleset>> AvailableGameRulesets;

	virtual TWeakObjectPtr<AUTReplicatedGameRuleset> FindRuleset(FString TagToFind);

public:
	// This array holds all of the data that a client can configure in his custom dialog.  When this object is created, this array will be filled out.
	UPROPERTY(Config)
	TArray<FAllowedData> AllowedGameData;

	// This will be true when all of the entries in AllowedGameData have been replicated and assigned on the client.
	bool bGameDataReplicationCompleted;

	// Hold sorted allowed data types.  NOTE: these are only valid on the client as they are filled out as part of the 
	// data pump system.  

	TArray<FAllowedData> AllowedGameModes;
	TArray<FAllowedData> AllowedMutators;
	TArray<FAllowedData> AllowedMaps;

	void ClientAssignGameData(FAllowedData Data);

	virtual void GetAvailableGameData(TArray<UClass*>& GameModes, TArray<UClass*>& MutatorList);

	virtual AUTReplicatedMapInfo* GetMapInfo(FString MapPackageName);

	/**
	 *	Find all possible playable maps.  This should only be called on the server.
	 **/
	virtual void FindAllPlayableMaps(TArray<FAssetData>& MapAssets);

	/**
	 *	Returns the Replicated Map List.  This should only be called from the client
	 **/
	virtual void GetMapList(const TArray<FString>& AllowedMapPrefixes, TArray<AUTReplicatedMapInfo*>& MapList, bool bUseCache=false);

	virtual void AuthorizeDedicatedInstance(AUTServerBeaconLobbyClient* Beacon, FGuid InstanceGUID, const FString& HubKey, const FString& ServerName);

	AUTLobbyMatchInfo* FindMatch(FGuid MatchID);

	// Holds a list of all maps available on this server.
	UPROPERTY(Replicated)
	TArray<AUTReplicatedMapInfo*> AllMapsOnServer;

protected:
	virtual bool AddDedicatedInstance(FGuid InstanceGUID, const FString& AccessKey, const FString& ServerName);
public:
	virtual void HandleQuickplayRequest(AUTServerBeaconClient* Beacon, const FString& MatchType, int32 ELORank);

	// Sets a limit on the # of spectators allowed in an instance
	UPROPERTY(Config)
	int32 MaxSpectatorsInInstance;

	virtual AUTReplicatedMapInfo* CreateMapInfo(const FAssetData& MapAsset) override;

	// An external client wants to join to an instance.. see if they can
	virtual void RequestInstanceJoin(AUTServerBeaconClient* Beacon, const FString& InstanceId, bool bSpectator, int32 Rank);

};



