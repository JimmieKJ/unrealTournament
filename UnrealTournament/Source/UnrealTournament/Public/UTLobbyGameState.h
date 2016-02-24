// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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

	virtual void AdminKillMatch(AUTLobbyMatchInfo* MatchToRemove);

	virtual void SortPRIArray();

	/**
	 *	Sets up the BeaconLister and HostObjects needed to listen for updates from the 
	 **/
	void SetupLobbyBeacons();

	/**
	 *	Launches an instance of a game that was created via the lobby interface.  MatchOwner is the MI of the match that is being created and ServerURLOptions is a string
	 *  that contains the game options.  
	 **/
	void LaunchGameInstance(AUTLobbyMatchInfo* MatchOwner, FString GameURL, int32 DebugCode);

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
	void GameInstance_Ready(uint32 GameInstanceID, FGuid GameInstanceGUID, const FString& MapName, AUTServerBeaconLobbyClient* InstanceBeacon);

	/**
	 *	Called when an instance needs to update it's match information
	 **/
	void GameInstance_MatchUpdate(uint32 GameInstanceID, const FMatchUpdate& MatchUpdate);

	/**
	 *	Called when an instance needs to update a player in a match's info
	 **/
	void GameInstance_PlayerUpdate(uint32 GameInstanceID, const FRemotePlayerInfo& PlayerInfo, bool bLastUpdate);

	/**
	 *	Called when an instance's game is over.  It this called via GameEnded and doesn't mean any of the
	 *  players have started to transition back.  But the Panel should no longer allow spectators to join
	 **/
	void GameInstance_EndGame(uint32 GameInstanceID, const FMatchUpdate& FinalMatchUpdate);

	/**
	 *	Called when the instance server is ready.  When it is called, the Lobby will kill the server instance.
	 **/
	void GameInstance_Empty(uint32 GameInstanceID);

	// Look to see if we should automatically place this player in a match
	void CheckForAutoPlacement(AUTLobbyPlayerState* NewPlayer);

	bool IsMatchStillValid(AUTLobbyMatchInfo* TestMatch);

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
	TArray< AUTReplicatedGameRuleset* > AvailableGameRulesets;

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

	virtual void AuthorizeDedicatedInstance(AUTServerBeaconLobbyClient* Beacon, FGuid InstanceGUID, const FString& HubKey, const FString& ServerName, const FString& ServerGameMode, const FString& ServerDescription, int32 MaxPlayers, bool bTeamGame);

	AUTLobbyMatchInfo* FindMatch(FGuid MatchID);

	// Holds a list of all maps available on this server.
	UPROPERTY(Replicated)
	TArray<AUTReplicatedMapInfo*> AllMapsOnServer;

protected:
	virtual bool AddDedicatedInstance(FGuid InstanceGUID, const FString& AccessKey, const FString& ServerName, const FString& ServerGameMode, const FString& ServerDescription, int32 MaxPlayers, bool bTeamGame);
	void FillOutRconPlayerList(TArray<FRconPlayerData>& PlayerList);
public:
	virtual void HandleQuickplayRequest(AUTServerBeaconClient* Beacon, const FString& MatchType, int32 RankCheck, bool bBeginner);

	// Sets a limit on the # of spectators allowed in an instance
	UPROPERTY(Config)
	int32 MaxSpectatorsInInstance;

	virtual AUTReplicatedMapInfo* CreateMapInfo(const FAssetData& MapAsset) override;

	// An external client wants to join to an instance.. see if they can
	virtual void RequestInstanceJoin(AUTServerBeaconClient* Beacon, const FString& InstanceId, bool bSpectator, int32 Rank);

	UPROPERTY()
	bool bTrainingGround;

	// Holds the # of game instances currently running
	UPROPERTY(Replicated)
	int32 NumGameInstances;

	bool SendSayToInstance(const FString& User, const FString& PlayerName, const FString& Message);

	UPROPERTY(replicated)
	bool bCustomContentAvailable;

	UPROPERTY(replicated)
	bool bAllowInstancesToStartWithBots;

	// Returns the # of matches available.  Not it ignores matches that are in the non-setup/dying state
	int32 NumMatchesInProgress();

	virtual void AttemptDirectJoin(AUTLobbyPlayerState* PlayerState, const FString& SessionID, bool bSpectator = false);

	virtual void MakeJsonReport(TSharedPtr<FJsonObject> JsonObject);

	virtual void GetMatchBans(int32 GameInstanceId, TArray<FUniqueNetIdRepl> &BanList);

};



