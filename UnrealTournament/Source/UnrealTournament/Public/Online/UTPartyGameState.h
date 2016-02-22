// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PartyGameState.h"
#include "UTPartyGameState.generated.h"

class UUTParty;
class UUTPartyMemberState;

enum class EMatchmakingCompleteResult : uint8;
enum class EUTPartyMemberLocation : uint8;

UENUM(BlueprintType)
enum class EUTPartyState : uint8
{
	/** In the menus */
	Menus = 0,
	/** Actively matchmaking, no destination yet */
	Matchmaking,
	/** Destination found and beyond (attempting to join, lobby, game, etc) */
	PostMatchmaking
};

/**
 * Current state of the party
 */
USTRUCT()
struct FUTPartyRepState : public FPartyState
{
	GENERATED_USTRUCT_BODY();
	
	/** What the party is doing at the moment */
	UPROPERTY()
	EUTPartyState PartyProgression;

	/** Has the leader begun connecting to lobby */
	UPROPERTY()
	bool bLobbyConnectionStarted;

	/** Result of matchmaking by the leader */
	UPROPERTY()
	EMatchmakingCompleteResult MatchmakingResult;

	/** Session id to join when set */
	UPROPERTY()
	FString SessionId;

	FUTPartyRepState()
	{
		Reset();
		PartyType = EPartyType::Public;
		bLeaderFriendsOnly = false;
		bLeaderInvitesOnly = false;
	}

	/** Reset party back to defaults */
	virtual void Reset() override;
};

/**
 * Party game state that contains all information relevant to the communication within a party
 * Keeps all players in sync with the state of the party and its individual members
 */
UCLASS(config=Game, notplaceable)
class UNREALTOURNAMENT_API UUTPartyGameState : public UPartyGameState
{
	GENERATED_UCLASS_BODY()

	virtual void RegisterFrontendDelegates() override;
	virtual void UnregisterFrontendDelegates() override;

	/**
	 * Delegate fired when a party state changes
	 * 
	 * @param NewPartyState current state object containing the changes
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnClientPartyStateChanged, EUTPartyState /* NewPartyState */);

	/**
	 * Delegate fired when a matchmaking state changes
	 * 
	 * @param Result result of the last matchmaking attempt by the leader
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnClientMatchmakingComplete, EMatchmakingCompleteResult /* Result */);

private:
	/** Passenger view related delegates prior to joining a lobby/game */
	FOnClientPartyStateChanged ClientPartyStateChanged;
	FOnClientMatchmakingComplete ClientMatchmakingComplete;
	
	/**
	 * Cached data for the party, only modifiable by the party leader
	 */
	UPROPERTY()
	FUTPartyRepState PartyState;

	/** Delegates related to party member data changes */
	FOnPartyMemberPropertyChanged LocationChanged;

	/**
	 * Handle matchmaking started delegate
	 */
	void OnPartyMatchmakingStarted();
	
	/**
	 * Delegate triggered when matchmaking is complete
	 *
	 * @param EndResult in what state matchmaking ended
	 * @param SearchResult the result returned if successful
	 */
	void OnPartyMatchmakingComplete(EMatchmakingCompleteResult EndResult);

	/**
	 * Handle lobby connection started delegate
	 */
	void OnLobbyConnectionStarted();

	/**
	 * Handle lobby connection attempt failure
	 */
	void OnLobbyConnectionAttemptFailed();
	
	/**
	 * Handle lobby connect success and now waiting for players
	 */
	void OnLobbyWaitingForPlayers();

	/**
	 * Handle joining the game from the lobby
	 */
	void OnLobbyConnectingToGame();

	/**
	 * Handle disconnect from a lobby
	 */
	void OnLobbyDisconnected();

	/**
	 * Handle lobby connect request made by the party leader to members
	 *
	 * @param SearchResult destination of the party leader
	 */
	void OnConnectToLobby(const FOnlineSessionSearchResult& SearchResult, const FString& CriticalMissionSessionId);

	friend UUTParty;
	friend UUTPartyMemberState;

public:
	/**
	 * Tell other party members about the location of local players
	 * 
	 * @param NewLocation new "location" within the game (see EFortPartyMemberLocation)
	 */
	void SetLocation(EUTPartyMemberLocation NewLocation);
	
	/** @return delegate fired when the location of the player has changed */
	FOnPartyMemberPropertyChanged& OnLocationChanged() { return LocationChanged; }

	FOnClientPartyStateChanged& OnClientPartyStateChanged() { return ClientPartyStateChanged; }
	FOnClientMatchmakingComplete& OnClientMatchmakingComplete() { return ClientMatchmakingComplete; }
};