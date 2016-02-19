// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTMatchmakingPolicy.h"
#include "UTMatchmakingGather.generated.h"

/** Time delay between not finding a session to join and the time to start a new gathering session */
#define START_GATHERING_DELAY 1.0f
/** Time delay between exhausting all options in one flow pass and starting all over again */
#define FIND_GATHERING_RESTART_DELAY 2.0f

UCLASS(config=Game)
class UNREALTOURNAMENT_API UUTMatchmakingGather : public UUTMatchmakingPolicy
{
	GENERATED_UCLASS_BODY()

public:

	// UUTMatchmakingPolicy interface begin
	virtual void StartMatchmaking() override;
	virtual void CancelMatchmaking() override;
	// UUTMatchmakingPolicy interface end

protected:
	
	/**
	 * Start a brand new gathering session for other players to join
	 */
	void StartGatheringSession();

	/**
	 * Find an existing gathering session waiting for new players
	 */
	void FindGatheringSession();
	
	/**
	 * Used internally to go back to the beginning
	 */
	void RestartMatchmaking();
	
private:
		
	/**
	 * Handle an attempt to join an empty server configured to gather players
	 *
	 * @param SessionName name of session joined
	 * @param bWasSuccessful was the join successful
	 */
	void OnJoinEmptyGatheringServer(FName SessionName, bool bWasSuccessful);

	/**
	 * Handle an attempt to join an existing gathering server
	 *
	 * @param SessionName name of session joined
	 * @param bWasSuccessful was the join successful
	 */
	void OnJoinGatheringSessionComplete(FName SessionName, bool bWasSuccessful);

	/**
	 * Handle no empty gathering servers found
	 */
	void OnNoEmptyGatheringServersFound();

	/**
	 * Handle no existing gathering servers found
	 */
	void OnNoGatheringSessionsFound();
	
	/**
	 * Handle cancellation of a gathering server search
	 */
	void OnGatheringSearchCancelled();

	/**
	 * Handle a general search failure
	 */
	void OnSearchFailure();

	/**
	 * Handle successful matchmaking completion
	 */
	void OnMatchmakingSuccess();

	/**
	 * Get the probability that this player will host when finding no alternatives
	 * 
	 * @return Chance to host for the player
	 */
	float GetChanceToHost() const;

	/**
	 * Timer handles for various matchmaking state
	 */
	FTimerHandle StartGatherTimerHandle;
	FTimerHandle FindGatherTimerHandle;
};

