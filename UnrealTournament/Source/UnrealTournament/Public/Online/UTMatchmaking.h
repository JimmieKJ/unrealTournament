// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGameInstance.h"
#include "UTPartyBeaconClient.h"
#include "PartyGameState.h"
#include "UTMatchmakingPolicy.h"
#include "UTMatchmaking.generated.h"

class UQosEvaluator;
class AUTPlayerController;
enum class EUTPartyState : uint8;
enum class EQosCompletionResult : uint8;

/** Time before actually reconnecting to the reservation beacon */
#define CONNECT_TO_RESERVATION_BEACON_DELAY 3.0f

/** Enum for possible types of matchmaking */
UENUM()
enum class EUTMatchmakingType : uint8
{
	Gathering,
	Session
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
	 * Cancels matchmaking and restores the menu
	 */
	void CancelMatchmaking();

	/**
	 * Find a session given to the client by a party leader
	 *
	 * @param FMatchmakingParams desired matchmaking parameters
	 *
	 * @return true if the operation started, false otherwise
	 */
	bool FindSessionAsClient(const FMatchmakingParams& InParams);

	/** Generic delegate for lobby flow */
	DECLARE_MULTICAST_DELEGATE(FOnMatchmakingStarted);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMatchmakingComplete, EMatchmakingCompleteResult /* Result */);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMatchmakingStateChange, EMatchmakingState::Type /*OldState*/, EMatchmakingState::Type /*NewState*/, const FMMAttemptState& /*MMState*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnConnectToLobby, const FOnlineSessionSearchResult& /*SearchResult*/)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartyStateChange, EUTPartyState)

	/** @return the delegate fired when matchmaking starts */
	FOnMatchmakingStarted& OnMatchmakingStarted() { return MatchmakingStarted; }

	/** @return the delegate fired when matchmaking is complete for any reason */
	FOnMatchmakingComplete& OnMatchmakingComplete() { return MatchmakingComplete; }

	/** @return the delegate fired when matchmaking state changes */
	FOnMatchmakingStateChange& OnMatchmakingStateChange() { return MatchmakingStateChange; }

	/** @return the delegate triggered when the lobby connection attempt begins */
	FOnConnectToLobby& OnConnectToLobby() { return ConnectToLobbyDelegates; }

	FOnPartyStateChange& OnPartyStateChange() { return PartyStateChange; }

private:

	/**
	 * QoS evaluation
	 */

	UPROPERTY()
	UQosEvaluator* QosEvaluator;

	FOnPartyStateChange PartyStateChange;

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
		
	/**
	 * Delegate triggered when a connection to a lobby begins
	 *
	 * @param SearchResult lobby result to connect to
	 */
	FOnConnectToLobby ConnectToLobbyDelegates;

	/**
	 * Cleanup the reservation beacon
	 */
	void CleanupReservationBeacon();
		
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

	void OnLeaderPartyStateChanged(EUTPartyState NewPartyState);

	/**
	 * Notification that the leader has finished matchmaking
	 *
	 * @param Result result of the matchmaking (complete, canceled, etc)
	 */
	void OnClientMatchmakingComplete(EMatchmakingCompleteResult Result);
	
	/**
	 * Notification that the leader has found the session id that the party will be joining
	 * passengers are expected to follow the leader into the given session, failure to do so will exit the party
	 * 
	 * @param SessionId session to search for and join
	 */
	void OnClientSessionIdChanged(const FString& SessionId);

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
	void OnSingleSessionMatchmakingComplete(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult);

	/** Internal notification that matchmaking state has changed, routes externally */
	void OnGatherMatchmakingStateChangeInternal(EMatchmakingState::Type OldState, EMatchmakingState::Type NewState);
	void OnSingleSessionMatchmakingStateChangeInternal(EMatchmakingState::Type OldState, EMatchmakingState::Type NewState);

	/** Internal notification that the matchmaking has completed, routes externally */
	void OnMatchmakingCompleteInternal(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult);

	void TravelToServer();

	/** Reservation beacon class */
	UPROPERTY(Transient)
	TSubclassOf<APartyBeaconClient> ReservationBeaconClientClass;

	/** Reservation beacon client instance */
	UPROPERTY(Transient)
	AUTPartyBeaconClient* ReservationBeaconClient;
			
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

	void DisconnectFromLobby();
	void OnDisconnectFromLobbyComplete(FName SessionName, bool bWasSuccessful);

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
	
	/** Timer handle for waiting before attempting to reconnect to the reservation beacon */
	FTimerHandle ConnectToReservationBeaconTimerHandle;

	/** Timer handle for waiting for join acknowledgment */
	FTimerHandle JoiningAckTimerHandle;
};