// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PartyBeaconState.h"
#include "OnlineSessionSettings.h"
#include "OnlineSessionInterface.h"
#include "UTSessionHelper.generated.h"

class FUniqueNetId;
class AUTPartyBeaconClient;
class FOnlineSessionSearchResult;
struct FEmptyServerReservation;

/** Time between previous beacon destruction and next reservation request */
#define NEXT_RESERVATION_REQUEST_DELAY 0.1f

/**
 * Various results that may occur in the course of using the session helper to reserve/join a session
 */
UENUM()
namespace EUTSessionHelperJoinResult
{
	enum Type
	{
		/** Nothing started, mostly an init placeholder */
		NoResult,
		/** Reservation has been made/accepted, expectation is game will clean up and proceed to join */
		ReservationSuccess,
		/** Reservation attempt has failed, helper is finished */
		ReservationFailure,
		/** Joining the session has succeeded, expectation is game will actually travel to the session */
		JoinSessionSuccess,
		/** Joining the session has failed, helper is finished */
		JoinSessionFailure,
	};
}

namespace EUTSessionHelperJoinResult
{
	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EUTSessionHelperJoinResult::Type JoinSessionResult)
	{
		switch (JoinSessionResult)
		{
			case NoResult:
			{
				return TEXT("No Result");
			}
			case JoinSessionFailure:
			{
				return TEXT("Join Failure");
			}
			case ReservationSuccess:
			{
				return TEXT("Reservation Success");
			}
			case ReservationFailure:
			{
				return TEXT("Reservation Failure");
			}
			case JoinSessionSuccess:
			{
				return TEXT("Join Success");
			}
			default:
			{
				return TEXT("");
			}
		}
	}
}

/**
 * Current state of the helper while reserving / joining a session
 */
UENUM()
namespace EUTSessionHelperJoinState
{
	enum Type
	{
		/** Not in any attempt (not started) */
		NotJoining,
		/** Testing the search result for joinability */
		RequestingReservation,
		/** Failed the reservation request for some reason (end of state machine) */
		FailedReservation,
		/** Notified the game of the reservation result, waiting for cleanup and continue */
		WaitingOnGame,
		/** Attempting to join the session */
		AttemptingJoin,
		/** In process of joining a session (end of state machine) */
		JoiningSession,
		/** Failed to join a session for some reason (end of state machine) */
		FailedJoin
	};
}

namespace EUTSessionHelperJoinState
{
	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EUTSessionHelperJoinState::Type SessionJoinState)
	{
		switch (SessionJoinState)
		{
		case NotJoining:
		{
			return TEXT("Not Joining");
		}
		case RequestingReservation:
		{
			return TEXT("Requesting Reservation");
		}
		case FailedReservation:
		{
			return TEXT("Failed reservation");
		}
		case WaitingOnGame:
		{
			return TEXT("Waiting On Game");
		}
		case AttemptingJoin:
		{
			return TEXT("Attempting Join");
		}
		case JoiningSession:
		{
			return TEXT("Joining Session");
		}
		case FailedJoin:
		{
			return TEXT("Failed Join");
		}
		default:
		{
			return TEXT("");
		}
		}
	}
}


/** Delegate that fires when the reservation attempt completes */
DECLARE_DELEGATE_OneParam(FOnUTReserveSessionComplete, EUTSessionHelperJoinResult::Type /** JoinResult */);
/** Delegate that fires when the join attempt completes */
DECLARE_DELEGATE_OneParam(FOnUTJoinSessionComplete, EUTSessionHelperJoinResult::Type /** JoinResult */);

/**
 * Helper class to simplify / compartmentalize the an attempt to join an existing session
 *
 * Starts by reserving space in the session, success and failures are alerted via callback
 *
 * User is expected to clean up current / existing game state before proceeding.
 *
 * After cleanup, call JoinSession() to continue the flow and join the session,
 * success and failures are alerted via callback
 *
 * User is expected to end things with a server travel on success
 */
UCLASS()
class UNREALTOURNAMENT_API UUTSessionHelper : public UObject
{
	GENERATED_UCLASS_BODY()

private:

	/** Class of beacon to be used for managing reservations/setup */
	UPROPERTY(Transient)
	UClass* BeaconClientClass;
	/** Beacon for sending reservation requests when matchmaking */
	UPROPERTY(Transient)
	AUTPartyBeaconClient* PartyBeaconClient;

	/** Transient value containing the current userid involved in this join attempt */
	TSharedPtr<const FUniqueNetId> CurrentLocalUserId;

	/** Transient value containing the current search result involved in this join attempt */
	FOnlineSessionSearchResult CurrentSessionToJoin;

	/** Handle to JoinReservedSessionComplete delegate */
	FDelegateHandle JoinReservedSessionCompleteDelegateHandle;

	/** Current session the reserve/join will associate with */
	UPROPERTY(Transient)
	FName CurrentSessionName;
	/** Last response received from the reservation beacon */
	UPROPERTY(Transient)
	TEnumAsByte<EPartyReservationResult::Type> LastBeaconResponse;
	/** Current state of the helper */
	UPROPERTY(Transient)
	TEnumAsByte<EUTSessionHelperJoinState::Type> CurrentJoinState;
	/** Current result of the join, changes from reservation to join as state progresses */
	UPROPERTY(Transient)
	TEnumAsByte<EUTSessionHelperJoinResult::Type> CurrentJoinResult;
	
	/**
	 * Delegate triggered when matchmaking state changes
	 */
	DECLARE_DELEGATE_TwoParams(FOnSessionHelperStateChange, EUTSessionHelperJoinState::Type /** OldState */, EUTSessionHelperJoinState::Type /** NewState */);
	FOnSessionHelperStateChange SessionHelperStateChange;
	
	/**
	 * Delegate triggered when an attempt to reserve space in a session completes
	 */
	FOnUTReserveSessionComplete ReserveSessionCompleteDelegate;
	
	/**
	 * Delegate triggered when an attempt to join a reserved session completes (no travel occurs)
	 */
	FOnUTJoinSessionComplete JoinReservedSessionCompleteDelegate;

	/** Helper for returning the world from this UObject */
	virtual UWorld* GetWorld() const override;

	/**
	 * Destroy any client beacons related to making a reservation
	 */
	void DestroyClientBeacons();

	/**
	 * Common code for reserving space in an active session of some kind
	 *
	 * @param LocalUserId user initiating the request, typically the leader
	 * @param SessionName session name to associate the reservation with
	 * @param SessionToJoin valid search result to reserve space on
	 * @param InCompletionDelegate external delegate to trigger when the reservation is complete
	 * @param TimerDelegate actual work to do now or at the next possible opportunity
	 */
	void ReserveSessionCommon(TSharedPtr<const FUniqueNetId>& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& SessionToJoin, FOnUTReserveSessionComplete& InCompletionDelegate, FTimerDelegate& TimerDelegate);

	/**
	 * Internal delegate when a reservation attempt completes
	 */
	void OnReservationRequestComplete(EPartyReservationResult::Type ReservationResult);

	/**
	 * Internal delegate when a reservation update occurs
	 *
	 * @param NumRemaining number of remaining slots in the session
	 */
	void OnReservationCountUpdate(int32 NumRemaining);

	/**
	 * Internal delegate when a reservation attempt fails to connect to the host beacon
	 */
	void OnHostConnectionFailure();

	/**
	 * Attempt to establish a connection and then a reservation with a host beacon
	 */
	void ReserveSessionInternal();

	/**
	 * Attempt to establish a connection and then a reservation with a host beacon
	 * configuring the server to the demands of the reservation
	 */
	void ReserveEmptySessionInternal(FEmptyServerReservation ReservationData);

	/**
	 * Internal delegate when an attempt to join a reserved session has completed
	 *
	 * @param SessionName name of session 
	 * @param Result outcome of the join attempt
	 */
	void OnJoinReservedSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	/**
	 * Process all state change requests during a join attempt
	 *
	 * @param NewState the matchmaking state to move to
	 */
	void ProcessJoinStateChange(EUTSessionHelperJoinState::Type NewState);

	/**
	 * Handle moving into the "failed while reserving a session" state and finish
	 */
	void HandleFailedReservation();

	/**
	 * Handle moving into the "waiting on game" state
	 */
	void HandleWaitingOnGame();

	/**
	 * Handle moving into the "actively joining session" state
	 */
	void HandleJoiningSession();

	/**
	 * Handle moving into the "failed while attempting join" state
	 */
	void HandleFailedJoin();

public:

	/**
	 * Bind to notification of any state changes while reserving and joining a session
	 */
	FOnSessionHelperStateChange& OnSessionHelperStateChange() { return SessionHelperStateChange; }

	/**
	 * Reset the class for reuse (assumes it is not in use)
	 */
	void Reset();

	/** @return if the class is in the process of joining right now */
	bool IsJoining();

	/** @return the search result being operated on */
	const FOnlineSessionSearchResult& GetSearchResult() const { return CurrentSessionToJoin; }

	/** @return the last beacon response value */
	TEnumAsByte<EPartyReservationResult::Type>& GetLastBeaconResponse() { return LastBeaconResponse; }

	/**
	 * Attempt to reserve a spot in the given session, requires a call to JoinSession to complete
	 * 
	 * @param LocalUserId user initiating the request, typically the leader
	 * @param SessionName session name to associate the reservation with
	 * @param SessionToJoin valid search result to reserve space on
	 * @param InCompletionDelegate external delegate to trigger when the reservation is complete
	 *
	 */
	void ReserveSession(TSharedPtr<const FUniqueNetId>& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& SessionToJoin, FOnUTReserveSessionComplete& InCompletionDelegate);

	/**
	 * Attempt to reserve a spot in the given session, requires a call to JoinSession to complete
	 * additionally, configure the server to the properties specified in the reservation data structure
	 *
	 * @param LocalUserId user initiating the request, typically the leader
	 * @param SessionName session name to associate the reservation with
	 * @param SessionToJoin valid search result to reserve space on
	 * @param EmptyReservation configuration details for the empty server
	 * @param InCompletionDelegate external delegate to trigger when the reservation is complete
	 *
	 */
	void ReserveEmptySession(TSharedPtr<const FUniqueNetId>& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& SessionToJoin, const FEmptyServerReservation& EmptyReservation, FOnUTReserveSessionComplete& InCompletionDelegate);

	/**
	 * Skip reserving a spot in the given session, requires a call to JoinSession to complete
	 * (used by clients following party leaders and assumes a reservation has already been made for this client)
	 *
	 * @param LocalUserId user initiating the request, typically the leader
	 * @param SessionName session name to associate the reservation with
	 * @param SessionToJoin valid search result to reserve space on
	 * @param EmptyReservation configuration details for the empty server
	 * @param InCompletionDelegate external delegate to trigger when the reservation is complete
	 *
	 */
	void SkipReservation(TSharedPtr<const FUniqueNetId>& LocalUserId, FName SessionName, const FOnlineSessionSearchResult& SessionToJoin, FOnUTReserveSessionComplete& InCompletionDelegate);
	
	/**
	 * Cancel a reservation in flight, always triggers the reservation complete delegate specified in ReserveSession/ReserveEmptySession above
	 */
	void CancelReservation();

	/**
	 * Continue the join process by attempting to join the already specified session (assumes all cleanup has occurred)
	 */
	void JoinReservedSession(FOnUTJoinSessionComplete& InCompletionDelegate);
};

/**
 * Generic helper for building party members up for reservations and stats
 */
void BuildPartyMembers(UWorld* World, const FUniqueNetIdRepl& PartyLeader, TArray<FPlayerReservation>& PartyMembers);
