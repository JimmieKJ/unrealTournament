// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGameInstance.h"
#include "UTPartyBeaconClient.h"
#include "UTLobbyBeaconClient.h"
#include "PartyGameState.h"
#include "UTMatchmakingPolicy.h"
#include "UTMatchmaking.generated.h"

class UQosEvaluator;

enum class EUTPartyState : uint8;
enum class EQosCompletionResult : uint8;

/** Time before actually reconnecting to the reservation beacon */
#define CONNECT_TO_RESERVATION_BEACON_DELAY 3.0f

/** Enum for possible types of matchmaking */
UENUM()
enum class EUTMatchmakingType : uint8
{
	Gathering,
};

/** Struct to represent params used for past matchmaking attempts */
USTRUCT()
struct FUTCachedMatchmakingSearchParams
{
	GENERATED_BODY()

public:
	/** Constructors */
	FUTCachedMatchmakingSearchParams();
	FUTCachedMatchmakingSearchParams(EUTMatchmakingType InType, const FMatchmakingParams& InParams);
	
	/** Simple accessor on validity of cached params */
	bool IsValid() const;

	/** Force invalidation of the cached params */
	void Invalidate();

	/** Simple accessor for the cached matchmaking type */
	EUTMatchmakingType GetMatchmakingType() const;

	/** Simple accessor for the cached matchmaking params */
	const FMatchmakingParams& GetMatchmakingParams() const;

private:

	/** Matchmaking type that was used */
	UPROPERTY()
	EUTMatchmakingType MatchmakingType;

	/** Matchmaking parameters that were used */
	UPROPERTY()
	FMatchmakingParams MatchmakingParams;

	/** Whether the data structure is valid or not */
	UPROPERTY()
	bool bValid;
};

UCLASS(config = Game, notplaceable)
class UNREALTOURNAMENT_API UUTMatchmaking : public UObject
{
	GENERATED_UCLASS_BODY()

private:

public:

	// UObject interface begin
	virtual UWorld* GetWorld() const override;
	virtual void BeginDestroy() override;
	// UObject interface end

	/**
	 * Initialization of the matchmaking code, sets up listening for related activity
	 */
	void Init();
	
	/**
	 * Register with the party interface to monitor party events and dispatch to specific party state
	 */
	void RegisterDelegates();

	/**
	 * Unregister with the party interface 
	 */
	void UnregisterDelegates();
	
	/**
	 * Start gathering players for a game
	 *
	 * @param FMatchmakingParams desired matchmaking parameters
	 *
	 * @return true if the operation started, false otherwise
	 */
	bool FindGatheringSession(const FMatchmakingParams& InParams);

	/**
	 * Get the lobby beacon client that is controlling the lobby for this client
	 *
	 * @return the lobby beacon client
	 */
	AUTLobbyBeaconClient* GetLobbyBeaconClient() const;

	/** Disconnect and leave the lobby */
	void DisconnectFromLobby();

	/** Generic delegate for lobby flow */
	DECLARE_MULTICAST_DELEGATE(FOnLobbyNotification);
	DECLARE_MULTICAST_DELEGATE(FOnMatchmakingStarted);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMatchmakingComplete, EMatchmakingCompleteResult /* Result */);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMatchmakingStateChange, EMatchmakingState::Type /*OldState*/, EMatchmakingState::Type /*NewState*/, const FMMAttemptState& /*MMState*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConnectToLobby, const FOnlineSessionSearchResult& /*SearchResult*/, const FString& /*CriticalMissionSessionId*/)

	/** @return the delegate fired when matchmaking starts */
	FOnMatchmakingStarted& OnMatchmakingStarted() { return MatchmakingStarted; }

	/** @return the delegate fired when matchmaking is complete for any reason */
	FOnMatchmakingComplete& OnMatchmakingComplete() { return MatchmakingComplete; }

	/** @return the delegate fired when matchmaking state changes */
	FOnMatchmakingStateChange& OnMatchmakingStateChange() { return MatchmakingStateChange; }

	/** @return the delegate triggered when the lobby connection attempt starts */
	FOnLobbyNotification& OnLobbyConnectionStarted() { return LobbyConnectionAttemptStarted; }

	/** @return the delegate triggered when the lobby connection attempt fails */
	FOnLobbyNotification& OnLobbyConnectionAttemptFailed() { return LobbyConnectionAttemptFailed; }

	/** @return the delegate triggered when the lobby connection attempt begins */
	FOnConnectToLobby& OnConnectToLobby() { return ConnectToLobbyDelegates; }

	/** @return the delegate triggered when the lobby is ready, but waiting for more players */
	FOnLobbyNotification& OnLobbyWaitingForPlayers() { return LobbyWaitingForPlayers; }

	/** @return the delegate triggered when beginning to join the game from the lobby */
	FOnLobbyNotification& OnLobbyConnectingToGame() { return LobbyConnectingToGame; }

	/** @return the delegate triggered when this client has been disconnected from the lobby */
	FOnLobbyNotification& OnLobbyDisconnecting() { return LobbyDisconnecting; }

	/** @return the delegate triggered when this client has been disconnected from the lobby */
	FOnLobbyNotification& OnLobbyDisconnected() { return LobbyDisconnected; }

private:

	/**
	 * QoS evaluation
	 */

	UPROPERTY()
	UQosEvaluator* QosEvaluator;

	/** Delegate triggered when matchmaking starts */
	FOnMatchmakingStarted MatchmakingStarted;
	
	/**
	 * Delegate triggered when matchmaking is complete
	 *
	 * @param Result end result of the matchmaking attempt
	 */
	FOnMatchmakingComplete MatchmakingComplete;
	
	/**
	 * Delegate triggered when matchmaking state changes
	 *
	 * @param OldState leaving state
	 * @param NewState entering state
	 * @param MMState structure containing extended information about matchmaking
	 */
	FOnMatchmakingStateChange MatchmakingStateChange;

	/** Delegate triggered when the lobby connection attempt starts */
	FOnLobbyNotification LobbyConnectionAttemptStarted;

	/** Delegate triggered when the lobby connection attempt fails */
	FOnLobbyNotification LobbyConnectionAttemptFailed;
	
	/** 
	 * Delegate triggered when a connection to a lobby begins 
	 * 
	 * @param SearchResult lobby result to connect to 
	 */
	FOnConnectToLobby ConnectToLobbyDelegates;

	/** Delegate triggered when the lobby is ready, but waiting for more players */
	FOnLobbyNotification LobbyWaitingForPlayers;

	/** Delegate triggered when the lobby has been disconnected */
	FOnLobbyNotification LobbyConnectingToGame;

	/** Delegate triggered when the lobby has started the async process of disconnecting */
	FOnLobbyNotification LobbyDisconnecting;

	/** Delegate triggered when the lobby has been disconnected */
	FOnLobbyNotification LobbyDisconnected;

	/**
	 * Lobby disconnect has completed (beacons cleared, game session deleted)
	 *
	 * @param SessionName name of session deleted
	 * @param bWasSuccessful was the delete successful
	 */
	void OnDisconnectFromLobbyComplete(FName SessionName, bool bWasSuccessful);
	
	/**
	 * Cleanup the reservation beacon
	 */
	void CleanupReservationBeacon();

	/**
	 * Cleanup a lobby connection
	 */
	void CleanupLobbyBeacons();

	/**
	 * Cancels matchmaking and restores the menu
	 */
	void CancelMatchmaking();
	
	/**
	 * Cleanup any data that was cached during matchmaking
	 */
	void ClearCachedMatchmakingData();
	
	/**
	 * Attempt to re-matchmake from any cached parameters, if possible
	 */
	void ReattemptMatchmakingFromCachedParameters();

	/**
	 * Disconnect from the reservation beacon; Tear down the beacon and trigger an attempt to re-matchmake if possible
	 */
	void DisconnectFromReservationBeacon();
	
	/**
	 * Reservation beacon disconnect has completed (beacon cleared, game session deleted)
	 *
	 * @param SessionName name of session deleted
	 * @param bWasSuccessful was the delete successful
	 */
	void OnDisconnectFromReservationBeaconComplete(FName SessionName, bool bWasSuccessful);
	
	/**
	 * Notification that the local players have joined a party,
	 * register with party state delegates
	 *
	 * @param InParty party that has been joined
	 */
	void OnPartyJoined(UPartyGameState* InParty);

	/**
	 * Notification that the local players have left a party, 
	 * unregister with party state delegates and cancels matchmaking
	 *
	 * @param InParty party that has been left
	 * @param Reason reason why the players left
	 */
	void OnPartyLeft(UPartyGameState* InParty, EMemberExitedReason Reason);

	/**
	 * Notification that a party member has left
	 *
	 * @param InParty party that has been affected
	 * @param InMemberId party member that left
	 * @param Reason reason why the player left
	 */
	void OnPartyMemberLeft(UPartyGameState* InParty, const FUniqueNetIdRepl& InMemberId, EMemberExitedReason Reason);

	/**
	 * Notification that a party member has been promoted
	 * Cancel matchmaking under these circumstances
	 *
	 * @param InParty party that has been affected
	 * @param InMemberId party member that has been promoted
	 */
	void OnPartyMemberPromoted(UPartyGameState* InParty, const FUniqueNetIdRepl& InMemberId);

	/**
	 * Notification that the state of the party has changed as it traverses "passenger view"
	 *
	 * @param NewPartyState new Fortnite specific flow progression
	 */
	void OnClientPartyStateChanged(EUTPartyState NewPartyState);

	/**
	 * Notification that the leader has finished matchmaking
	 *
	 * @param Result result of the matchmaking (complete, canceled, etc)
	 */
	void OnClientMatchmakingComplete(EMatchmakingCompleteResult Result);
	
	/**
	 * Progression through actual matchmaking after a datacenter id has been determined
	 *
	 * @param Result datacenter qos completion result
	 * @param DatacenterId datacenter id chosen by the evaluator
	 * @param InParams matchmaking parameters passed along from the original request
	 *
	 */
	void ContinueMatchmaking(EQosCompletionResult Result, const FString& DatacenterId, FMatchmakingParams InParams);

	/**
	 * Handle the end of matchmaking (reserved space and joined the session, now connect to the reservation / lobby beacons
	 *
	 * @param Result end result of the matchmaking attempt
	 * @param SearchResult the session of interest
	 */
	void OnGatherMatchmakingComplete(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult);

	/** Internal notification that matchmaking state has changed, routes externally */
	void OnGatherMatchmakingStateChangeInternal(EMatchmakingState::Type OldState, EMatchmakingState::Type NewState);

	/** Internal notification that the matchmaking has completed, routes externally */
	void OnMatchmakingCompleteInternal(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult);

	/** Reservation beacon class */
	UPROPERTY(Transient)
	TSubclassOf<APartyBeaconClient> ReservationBeaconClientClass;

	/** Reservation beacon client instance */
	UPROPERTY(Transient)
	AUTPartyBeaconClient* ReservationBeaconClient;

	/** Lobby beacon client instance */
	UPROPERTY(Transient)
	AUTLobbyBeaconClient* LobbyBeaconClient;
		
	/**
	 * Connect to a reservation beacon
	 *
	 * @param SearchResult session to connect to
	 */
	void ConnectToReservationBeacon(FOnlineSessionSearchResult SearchResult);
	
	/**
	 * Handle a failure to connect to the reservation beacon
	 */
	void OnReservationBeaconConnectionFailure();

	/**
	 * Handle a reservation count update from the reservation beacon
	 */
	void OnReservationCountUpdate(int32 NumRemaining);

	/** 
	 * Called via delegate when the reservation beacon hits its capacity for reservations 
	 */
	void OnReservationFull();

	/** 
	 * Called via delegate when the reservation beacon's reservations are allowed to proceed past the reservation phase 
	 */
	void OnAllowedToProceedFromReservation();

	/** 
	 * Called via delegate when the reservation beacon's reservations have timed out waiting for permission to proceed past the reservation phase 
	 */
	void OnAllowedToProceedFromReservationTimeout();
	
	/**
	 * Handle results after reconnection with the reservation beacon
	 *
	 * @param ReservationResponse response from the reservation beacon regarding an existing reservation
	 * @param SearchResult the session of interest
	 */
	void OnReconnectResponseReceived(EPartyReservationResult::Type ReservationResponse, FOnlineSessionSearchResult SearchResult);

	/**
	 * Helpers
	 */

	/** Active controller */
	UPROPERTY()
	int32 ControllerId;

	/** The search parameters used for the last matchmaking attempt */
	UPROPERTY()
	FUTCachedMatchmakingSearchParams CachedMatchmakingSearchParams;

	/** The cached search result from a successful reservation reconnect attempt; Used to then connect to a lobby */
	FOnlineSessionSearchResult CachedSearchResult;

	/** Active matchmaking policy, if null then matchmaking is idle */
	UPROPERTY()
	UUTMatchmakingPolicy* Matchmaking;

	/** @return the game instance */
	UUTGameInstance* GetUTGameInstance() const;

	/** @return the owning controller */
	AUTPlayerController* GetOwningController() const;

	/** Timer handle for waiting before attempting a lobby connection */
	FTimerHandle ConnectToLobbyTimerHandle;

	/** Timer handle for waiting before attempting to reconnect to the reservation beacon */
	FTimerHandle ConnectToReservationBeaconTimerHandle;

	/** Timer handle for waiting for join acknowledgment */
	FTimerHandle JoiningAckTimerHandle;
};