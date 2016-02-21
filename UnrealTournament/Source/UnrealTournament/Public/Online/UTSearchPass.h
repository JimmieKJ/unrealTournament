// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealTournament/Private/Online/UTSessionHelper.h"
#include "UTOnlineSessionTypes.h"
#include "UnrealTournament/Private/Online/UTMatchmakingStats.h"
#include "UTSearchPass.generated.h"

class FUTOnlineSessionSearchBase;
class UUTSessionHelper;
class UUTGameInstance;
enum class EQosResponseType : uint8;
enum class EMatchmakingFlags : uint8;

/** Time between a successful reservation and the time the session join attempt is made */
#define JOIN_RESERVED_SESSION_DELAY 0.1f 
/** Time between a failed reservation attempt and the next search result is attempted */
#define CONTINUE_MATCHMAKING_DELAY 0.3f
/** Time between the cancellation complete delegate and notifying the higher level */
#define CANCEL_MATCHMAKING_DELAY 1.0f
/** Time between no search results found and notifying the higher level */
#define NO_MATCHES_AVAILABLE_DELAY 1.0f

/**
 * Structure for holding the state of a matchmaking attempt
 */
USTRUCT()
struct FMMAttemptState
{
	GENERATED_USTRUCT_BODY()

	FMMAttemptState() : 
		BestSessionIdx(0),
		NumSearchResults(0),
		State(EMatchmakingState::NotMatchmaking),
		LastBeaconResponse(EPartyReservationResult::NoResult)
	{}

	~FMMAttemptState() {}

	/** Current search result in process */
	UPROPERTY()
	int32 BestSessionIdx;
	/** Number of total search results in the query */
	UPROPERTY()
	int32 NumSearchResults;
	/** Matchmaking state */
	UPROPERTY()
	TEnumAsByte<EMatchmakingState::Type> State;
	/** Last beacon result */
	UPROPERTY()
	TEnumAsByte<EPartyReservationResult::Type> LastBeaconResponse;
};

USTRUCT()
struct FUTSearchPassParams
{
	GENERATED_USTRUCT_BODY()

	/** Id of the controller making the request */
	UPROPERTY()
	int32 ControllerId;
	/** Name of session settings are stored with */
	UPROPERTY()
	FName SessionName;
	/** Datacenter to search within */
	UPROPERTY()
	FString BestDatacenterId;
	/** Matchmaking flags */
	EMatchmakingFlags Flags;

	FUTSearchPassParams();
};

/**
 * Internal state for a given search pass
 */
USTRUCT()
struct FUTSearchPassState
{
	GENERATED_USTRUCT_BODY()

	/** Current search result choice to join */
	UPROPERTY()
	int32 BestSessionIdx;
	/** WUID of any lock acquisition attempt, if valid an unlock needs to occur on any "cancel/failure" case of matchmaking */
	UPROPERTY()
	FString WUIDLockSemaphore;
	/** Was this search attempt canceled */
	UPROPERTY()
	bool bWasCanceled;
	/** Was there a failure of some kind */
	UPROPERTY()
	bool bWasFailure;
	/** Matchmaking state */
	UPROPERTY()
	TEnumAsByte<EMatchmakingState::Type> MatchmakingState;
	/** Last beacon result */
	UPROPERTY()
	TEnumAsByte<EPartyReservationResult::Type> LastBeaconResponse;
	/** Matchmaking stats */
	TSharedPtr<FUTMatchmakingStats> MMStats;

	FUTSearchPassState() :
		BestSessionIdx(INDEX_NONE),
		bWasCanceled(false),
		bWasFailure(false),
		MatchmakingState(EMatchmakingState::NotMatchmaking),
		LastBeaconResponse(EPartyReservationResult::GeneralError),
		MMStats(nullptr)
	{}
};

UCLASS(config=Game, notplaceable)
class UNREALTOURNAMENT_API UUTSearchPass : public UObject
{
	GENERATED_UCLASS_BODY()

private:

	/*
	 * Delegate triggered when no search results are found
	 */
	DECLARE_DELEGATE(FOnNoSearchResultsFoundComplete);
	FOnNoSearchResultsFoundComplete NoSearchResultsFoundComplete;

	/* Delegate triggered internally when the search is canceled */
	DECLARE_DELEGATE(FOnCancelledSearchComplete);
	FOnCancelledSearchComplete CancelledSearchComplete;

	/* Delegate triggered internally when the search has failed */
	DECLARE_DELEGATE(FOnSearchFailure);
	FOnSearchFailure SearchFailure;

	/**
	 * Delegate triggered when a session is joined
	 *
	 * @param SessionName name of session that was joined
	 * @param bWasSuccessful was the join successful
	 */
	DECLARE_DELEGATE_TwoParams(FOnJoinSessionComplete, FName /*SessionName*/, bool /*bWasSuccessful*/);
	FOnJoinSessionComplete JoinSessionComplete;

	/**
	 * Delegate triggered when matchmaking state changes
	 *
	 * @param OldState leaving state
	 * @param NewState entering state
	 */
	DECLARE_DELEGATE_TwoParams(FOnMatchmakingStateChange, EMatchmakingState::Type /*OldState*/, EMatchmakingState::Type /*NewState*/);
	FOnMatchmakingStateChange MatchmakingStateChange;

public:

	/**
	 * Cancel matchmaking in progress
	 */
	void CancelMatchmaking();

	/**
	 * Is matchmaking canceling or canceled
	 *
	 * @return true if in any state that would indicate not matchmaking or otherwise canceling matchmaking
	 */
	bool IsMatchmaking() const;

	/**
	 * Get the search results found and the current search result being probed
	 *
	 * @return State of search result query
	 */
	FMMAttemptState GetSearchResultStatus() const;

	/** @return the delegate fired when matchmaking state changes */
	FOnMatchmakingStateChange& OnMatchmakingStateChange() { return MatchmakingStateChange; }

protected:

	/** Helper for reserving/joining sessions */
	UPROPERTY()
	UUTSessionHelper* SessionHelper;

	/** Delegate for canceling matchmaking */
	FOnCancelFindSessionsCompleteDelegate OnCancelSessionsCompleteDelegate;

	/** Search params given to the search pass */
	UPROPERTY(Transient)
	FUTSearchPassParams CurrentSearchParams;

	/** Search pass state during game creation/matchmaking */
	UPROPERTY(Transient)
	FUTSearchPassState CurrentSearchPassState;

	/** Current search filter */
	TSharedPtr<FUTOnlineSessionSearchBase> SearchFilter;

public:

	/**
	 * Helpers
	 */

	/**
	 * Get the search result currently being considered
	 *
	 * @return search result in use
	 */
	const FOnlineSessionSearchResult& GetCurrentSearchResult() const;

	/** Clear all bound internally related matchmaking delegates */
	void ClearMatchmakingDelegates();

private:

	/** Quick access to the current world */
	UWorld* GetWorld() const override;

	/** Quick access to the world timer manager */
	FTimerManager& GetWorldTimerManager() const;

	/** Quick access to game instance */
	UUTGameInstance* GetUTGameInstance() const;

	/** Client beacon cleanup */
	void DestroyClientBeacons();

private:
	
	/**
	 * State machine
	 */

	/**
	 * Entry point for matchmaking after search results are returned
	 * Used for existing sessions
	 */
	void StartTestingExistingSessions();

	/**
	 * Entry point for matchmaking after search results are returned
	 * Used for empty server resources
	 */
	void StartTestingEmptyServers();

	/**
	 * Return point after each failed attempt to join a search result
	 * Used for existing sessions
	 */
	void ContinueTestingExistingSessions();

	/**
	 * Return point after each failed attempt to join a search result
	 * Used for empty server resources
	 */
	void ContinueTestingEmptyServers();

	/**
	 * Choose the best session from a list of search results based on game criteria
	 */
	void ChooseBestSession();

	/**
	 * Reset the variables the are keeping track of session join attempts
	 */
	void ResetBestSessionVars();

	/**
	 * Handle all state change requests during matchmaking
	 *
	 * @param NewState the matchmaking state to move to
	 */
	void ProcessMatchmakingStateChange(EMatchmakingState::Type NewState);

	/**
	 * Handle the join success state
	 */
	void ProcessJoinSuccess();

	/**
	 * Handle a cancel matchmaking request
	 */
	void ProcessCancelMatchmaking();

	/**
	 * Delegate fired when the cancellation of a search for an online session has completed
	 *
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	void OnCancelFindSessionsComplete(bool bWasSuccessful);

	/**
	 * Begin final operations to cancel matchmaking
	 */
	void FinishCancelMatchmaking();

	/**
	 * Matchmaking cancellation is actually complete
	 */
	void OnCancelMatchmakingComplete();

	/** Handle no more search results are available */
	void ProcessNoMatchesAvailable();

	/** Delegate triggered after the no search result path is complete */
	void OnNoMatchesAvailableComplete();

	/**
	 * Handle cleanup after a matchmaking cancellation
	 */
	void ProcessNotMatchmaking();

	/**
	 * Delegate keeping tabs on the state of the lower level session reserve/join progress
	 *
	 * @param OldState previous state of the helper
	 * @param NewState current state of the helper
	 */
	void SessionHelperStateChanged(EUTSessionHelperJoinState::Type OldState, EUTSessionHelperJoinState::Type NewState);

public:

	/** @return the delegate fired when no search results are found in this pass */
	FOnNoSearchResultsFoundComplete& OnNoSearchResultsFound() { return NoSearchResultsFoundComplete; }

	/** @return the delegate fired when a cancel operation has completely finished */
	FOnCancelledSearchComplete& OnCancelledSearchComplete() { return CancelledSearchComplete; }

	/** @return the delegate fired when some internal failure has occurred */
	FOnSearchFailure& OnSearchFailure() { return SearchFailure; }

public:

	/**
	 * Finding sessions
	 */

	/**
	 * Find an existing online session
	 *
	 * @param ControllerId controller that initiated the request
	 * @param SessionName name of session this search will generate
	 * @param InSearchFilter search params to query for
	 */
	void FindSession(FUTSearchPassParams& InSearchParams, const TSharedRef<FUTOnlineSessionSearchBase>& InSearchFilter);

	/**
	 * Find an empty server to host a new world
	 *
	 * @param ControllerId controller that initiated the request
	 * @param SessionName name of session this search will generate
	 * @param InSearchFilter search params to query for
	 */
	void FindEmptySession(FUTSearchPassParams& InSearchParams, const TSharedRef<FUTOnlineSessionSearchBase>& InSearchFilter);

private:

	/** Delegate for searching for sessions */
	FOnFindSessionsCompleteDelegate OnFindSessionsCompleteDelegate;

	/**
	 * Common functionality for finding one or more advertised sessions
	 *
	 * @param InSearchParams search parameters specified initially
	 * @param InSearchFilter search settings for finding the session
	 */
	void FindInternal(const FUTSearchPassParams& InSearchParams, const TSharedRef<FUTOnlineSessionSearchBase>& InSearchFilter);

	/**
	 * Continue finding an empty server now that a datacenter query has ended and we know which datacenter to search within
	 *
	 * @param InSearchParams search parameters specified initially
	 * @param InSearchFilter search settings for finding the session
	 */
	void ContinueFindEmptySession(const FUTSearchPassParams InSearchParams, const TSharedRef<FUTOnlineSessionSearchBase> InSearchFilter);

	/**
	 * Delegate fired when a session search query has completed
	 *
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	void OnFindSessionsComplete(bool bWasSuccessful);

private:

	/**
	 * Reservation handling
	 */

	/** Request space in the current "best" session */
	void RequestReservation();

	/** Request space in the current "best" empty dedicated session */
	void RequestEmptyServerReservation();

	/**
	 * Delegate fired on client in response to a reservation request from the host beacon
	 *
	 * @param ReservationResult reservation response returned by host beacon
	 */
	void OnSessionReservationRequestComplete(EUTSessionHelperJoinResult::Type ReservationResult);

private:

	/**
	 * Joining sessions
	 */

	/** Join the session referenced by the current "best session" */
	void JoinReservedSession();

	/**
	 * Delegate fired when a session join request has completed
	 *
	 * @param JoinResult result of the join session attempt
	 */
	void OnJoinSessionCompleteInternal(EUTSessionHelperJoinResult::Type JoinResult);

public:

	/** @return the delegate fired when joining a session */
	FOnJoinSessionComplete& OnJoinSessionComplete() { return JoinSessionComplete; }

public:

	/**
	 * Analytics
	 */

	/**
	* Set the analytics module share pointer related with this search pass
	*/
	void SetMMStatsSharePtr(TSharedPtr<FUTMatchmakingStats> MMStatsSharePtr);

	/**
	* release the analytics module share pointer related with this search pass
	*/
	void ReleaseMMStatsSharePtr();

private:

	FTimerHandle RequestEmptyServerReservationTimerHandle;
	FTimerHandle RequestReservationTimerHandle;
	FTimerHandle JoinReservedSessionTimerHandle;
	FTimerHandle ContinueEmptyDedicatedMatchmakingTimerHandle;
	FTimerHandle ContinueMatchmakingTimerHandle;
	FTimerHandle FinishCancelMatchmakingTimerHandle;
	FTimerHandle OnNoMatchesAvailableCompleteTimerHandle;
	FTimerHandle OnCancelMatchmakingCompleteTimerHandle;

	FDelegateHandle OnFindSessionsCompleteDelegateHandle;
	FDelegateHandle OnCancelSessionsCompleteDelegateHandle;
};

