// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "OnlineSubsystemTypes.h"
#include "TAttributeProperty.h"
#include "UTGameMode.h"
#include "UTLobbyMatchInfo.generated.h"

DECLARE_DELEGATE(FOnMatchInfoUpdated);
DECLARE_DELEGATE(FOnRulesetUpdated);

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

	UPROPERTY()
	bool bIsSpectator;

	// The current name of this player
	UPROPERTY()
	FString PlayerName;

	// The current score for this player
	UPROPERTY()
	int32 PlayerScore;
	
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

	FPlayerListInfo(FUniqueNetIdRepl inPlayerID, FString inPlayerName, float inPlayerScore, bool inbIsSpectator)
		: PlayerID(inPlayerID)
		, PlayerName(inPlayerName)
		, PlayerScore(inPlayerScore)
		, bIsSpectator(inbIsSpectator)
	{
	}

};

UCLASS(notplaceable)
class UNREALTOURNAMENT_API AUTLobbyMatchInfo : public AInfo
{
	GENERATED_UCLASS_BODY()
public:
	// We use  the FUniqueNetID of the owner to be the Anchor point for this object.  This way we can reassociated the MatchInfo with the player when they reenter a server from travel.
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_Update)
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

	// -1 means no bots.
	UPROPERTY(Replicated)
	int32 BotSkillLevel;

	// Holds data about the match.  In matches that are not started yet, it holds the description of the match.  In matches in progress, it's 
	// replicated data from the instance about the state of the match.  NOTE: Player information is not replicated from the instance to the server here
	// it's replicated in the PlayersInMatchInstance array.

	UPROPERTY(Replicated, ReplicatedUsing = OnRep_MatchStats)
	FString MatchStats;

	UPROPERTY(Replicated)
	FString MatchBadge;

	UPROPERTY(Replicated, ReplicatedUsing = OnRep_InitialMap)
	FString InitialMap;

	// This will be looked up when the inital map is set.
	TSharedPtr<FMapListItem> InitialMapInfo;

	// Set by OnRep_InitialMap
	bool bMapChanged;

	// The current ruleset the governs this match
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_CurrentRuleset)
	TWeakObjectPtr<AUTReplicatedGameRuleset> CurrentRuleset;

	// A list of players in this lobby
	UPROPERTY(Replicated)
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
	UPROPERTY(Replicated)
	TArray<FPlayerListInfo> PlayersInMatchInstance;
	
	// Cache some data
	virtual void PreInitializeComponents() override;

	virtual void AddPlayer(AUTLobbyPlayerState* PlayerToAdd, bool bIsOwner = false);
	virtual bool RemovePlayer(AUTLobbyPlayerState* PlayerToRemove);
	virtual FText GetActionText();

	// The GameState needs to tell this MatchInfo what settings should be made available
	virtual void SetSettings(AUTLobbyGameState* GameState, AUTLobbyMatchInfo* MatchToCopy = NULL);

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

	FOnMatchInfoUpdated OnMatchInfoUpdatedDelegate;
	FOnRulesetUpdated OnRulesetUpdatedDelegate;

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
	 *	Removes the player from 
	 **/
	void RemoveFromMatchInstance(AUTLobbyPlayerState* PlayerState);


	/**
	 *	returns true if this match is in progress already.
	 **/
	virtual bool IsInProgress();
	virtual bool ShouldShowInDock();

	// Called from clients.  This will check to make sure all of the needed replicated information has arrived and that the player is ready to join.
	virtual bool MatchIsReadyToJoin(AUTLobbyPlayerState* Joiner);

	// This will be true if this match is a dedicated match and shouldn't ever go down
	UPROPERTY(Replicated)
	bool bDedicatedMatch;

	// The Key used to associated this match with a dedicated instance
	FString AccessKey;

	// The name for this server
	UPROPERTY(Replicated)
	FString DedicatedServerName;

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

	UFUNCTION()
	virtual void OnRep_CurrentRuleset();


	UFUNCTION()
	virtual void OnRep_Update();

	UFUNCTION()
	virtual void OnRep_InitialMap();

	// This match info is done.  Kill it.
	void RecycleMatchInfo();

	bool CheckLobbyGameState();

	UFUNCTION()
	virtual void OnRep_MatchStats();

public:
	int32 MatchGameTime;

	// This is called by the host when he has received his owner id and the default ruleset
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerMatchIsReadyForPlayers();

	// Returns true if the match has room for a new player to join it
	bool MatchHasRoom() { return true; }

	virtual void SetRules(TWeakObjectPtr<AUTReplicatedGameRuleset> NewRuleset, const FString& StartingMap);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetRules(const FString& RulesetTag, const FString& StartingMap, int32 NewBotSkillLevel);

	virtual void SetMatchStats(FString Update);

	bool GetNeededPackagesForCurrentRuleset(TArray<FString>& NeededPackageURLs);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerCreateCustomRule(const FString& GameMode, const FString& StartingMap, const FString& Description, const TArray<FString>& GameOptions, int32 DesiredSkillLevel, int32 DesiredPlayerCount);

	bool IsBanned(FUniqueNetIdRepl Who);
	TSharedPtr<FMapListItem> GetMapInformation(FString MapPackage);

	void LoadInitialMapInfo();

public:
	// Unique for each match on this hub.  This will be used to lookup data regarding a given instance.
	FGuid UniqueMatchID;

	int32 NumPlayersInMatch();

};



