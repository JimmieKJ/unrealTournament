// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Everything a player controller will use to manage online sessions.
 */
#pragma once

#include "UTMatchmakingPolicy.h"
#include "Private/Online/UTMatchmakingStats.h"
#include "UTMatchmakingSingleSession.generated.h"

class FOnlineSessionSearchResult;

/**
 * Current state of joining a single session by invite/presence
 */
USTRUCT()
struct FUTInviteSessionParams
{
	GENERATED_USTRUCT_BODY()

	/** State of session join attempt */
	UPROPERTY()
	TEnumAsByte<EMatchmakingState::Type> State;
	/** Recorded reason for the failure */
	UPROPERTY()
	FText FailureReason;
	/** Last response from the beacon during the join attempt */
	UPROPERTY()
	TEnumAsByte<EPartyReservationResult::Type> LastBeaconResponse;
	/** Matchmaking stats */
	TSharedPtr<FUTMatchmakingStats> MMStats;
};


UCLASS(config=Game)
class UNREALTOURNAMENT_API UUTMatchmakingSingleSession : public UUTMatchmakingPolicy
{
	GENERATED_UCLASS_BODY()

public:

	// UFortMatchmakingPolicy interface begin
	virtual void Init(const FMatchmakingParams& InParams) override;
	virtual void StartMatchmaking() override;
	virtual void CancelMatchmaking() override;
	virtual bool IsMatchmaking() const override;
	// UFortMatchmakingPolicy interface end

private:

	/** Joining session state */
	UPROPERTY(Transient)
	FUTInviteSessionParams CurrentSessionParams;

	/** Helper for reserving space and then joining a session */
	UPROPERTY(Transient)
	UUTSessionHelper* SessionHelper;

	/**
	 * Join a single session by id
	 *
	 * Attempts to find the session, reserve space in it, and then join it
	 *
	 * @param SessionName name of session to associate with the join
	 * @param SessionId id of session to find and join
	 */
	void JoinSessionById(FName SessionName, const FUniqueNetId& SessionId);
	
	/**
	 * Internal join session starting point, intersection of JoinSessionById and JoinSession by search result
	 *
	 * @param SessionName name of session to associate with the join procedure
	 * @param SearchResult the session where a reserve/join will be attempted
	 */
	void JoinSessionInternal(FName SessionName, const FOnlineSessionSearchResult& SearchResult);

	/**
	 * Delegate fired once the find session by id task has completed
	 * Session has not been joined at this point, and still requires a call to JoinSession()
	 *
	 * @param LocalUserNum the controller number of the accepting user
	 * @param bWasSuccessful the session was found and is joinable, false otherwise
	 * @param SearchResult the search/settings for the session we're attempting to join
	 */
	void OnFindSessionByIdComplete(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& SearchResult, FName SessionName);

private:

	/**
	 * State Machine
	 */

	/**
 	 * Handle all state change requests during session join
	 *
	 * @param NewState the join state to move to
	 */
	void ProcessMatchmakingStateChange(EMatchmakingState::Type NewState);

	/**
	 * Handle successful completion 
	 */
	void ProcessJoinSuccess();

	/**
	 * Cleanup that must occur if we get into an error state in between cleanup of the game we are leaving
	 * and the new game we are joining
	 *
	 * Limited choices at this point, can only really return to main menu
	 */
	void ProcessFailure();

	/**
	 * Initiate a no session found result ending with OnProcessNoMatchesAvailableComplete
	 */
	void ProcessNoMatchesAvailable();
	void OnProcessNoMatchesAvailableComplete();

private:

	/**
	 * Internal delegate when the reserve session portion of joining a game has completed
	 *
	 * @param ReserveResult result of the reserve session call
	 */
	void OnReserveSessionComplete(EUTSessionHelperJoinResult::Type ReserveResult);

	/**
	 * Internal delegate when the join session portion of joining a game has completed (still not traveling)
	 *
	 * @param JoinResult result of the join session call
	 */
	void OnJoinReservedSessionComplete(EUTSessionHelperJoinResult::Type JoinResult);

	/**
	 * Cleanup portion of the reserve -> cleanup -> join flow for normal game presence/invite handling
	 */
	void CleanupExistingSessionsAndContinue();

	/**
	 * Internal delegate called in between a successful reservation and the actual join
	 * to cleanup any existing game state / sessions before continuation of the flow
	 *
	 * @param SessionName name of session cleaned up
	 * @param bWasSuccessful was the cleanup of the session successful
	 */
	void OnCleanupForJoinComplete(FName SessionName, bool bWasSuccessful);

	/**
	 * Callback after the new game session cleanup has occurred
	 * waits one before continuing with OnCleanupAfterFailureCompleteContinued
	 * allows for delegates to unwind
	 *
	 * @param SessionName name of session cleaned up
	 * @param bWasSuccessful was the cleanup of the session successful
	 */
	void OnCleanupAfterFailureComplete(FName SessionName, bool bWasSuccesful);

	/**
	 * Continuation of failure cleanup
	 * Actually ReturnToMainMenu or failing that, do a hard travel back to the beginning
	 */
	void OnCleanupAfterFailureCompleteContinued();
};

