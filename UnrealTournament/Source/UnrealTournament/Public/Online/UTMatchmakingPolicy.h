// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTSearchPass.h"
#include "UTMatchmakingPolicy.generated.h"

/**
 * Possible completion states at the end of a matchmaking request
 */
UENUM(BlueprintType)
enum class EMatchmakingCompleteResult : uint8
{
	/** matchmaking not started */
	NotStarted,
	/** matchmaking was canceled */
	Cancelled,
	/** matchmaking returned no results */
	NoResults,
	/** matchmaking failed internally */
	Failure,
	/** matchmaking failed creating a new session */
	CreateFailure,
	/** matchmaking is complete */
	Success
};

/**
 * Possible start locations within matchmaking flow
 */
UENUM()
enum class EMatchmakingStartLocation : uint8
{
	/** Start with a lobby search */
	Lobby,
	/** Start with an existing game search */
	Game,
	/** Start by creating a new game */
	CreateNew,
	/** Find a single session */
	FindSingle
};

UENUM()
enum class EMatchmakingFlags : uint8
{
	None = 0x0,				// No flags
	CreateNewOnly = 0x1,	// Matchmaking should only search for new sessions
	NoReservation = 0x2,	// No reservation beacon connection is required (party leader already made one)
	Private = 0x4,			// Make a private game
	UseWorldDataOwner = 0x8,// World data owner is important
};

ENUM_CLASS_FLAGS(EMatchmakingFlags)

/**
 * Common matchmaking parameters fed into the system
 */
USTRUCT()
struct FMatchmakingParams
{
	GENERATED_USTRUCT_BODY()
	
	FMatchmakingParams() :
		ControllerId(INVALID_CONTROLLERID),
		PartySize(1),
		PlaylistId(INDEX_NONE),
		Flags(EMatchmakingFlags::None),
		StartWith(EMatchmakingStartLocation::Lobby),
		ChanceToHostOverride(0.f)
	{}

	~FMatchmakingParams() {}
	
	/** Id of the controller making the request */
	UPROPERTY()
	int32 ControllerId;
	/** Number of players in the party */
	UPROPERTY()
	int32 PartySize;
	/** Datacenter to use */
	UPROPERTY()
	FString DatacenterId;
	/** Game mode to play */
	UPROPERTY()
	int32 PlaylistId;
	/** Matchmaking flags */
	UPROPERTY()
	EMatchmakingFlags Flags;
	/** Specific session to join */
	UPROPERTY()
	FString SessionId;
	/** Where to begin matchmaking */
	UPROPERTY()
	EMatchmakingStartLocation StartWith;
	/** If > 0.f, acts as an override to the chance to host during matchmaking */
	UPROPERTY()
	float ChanceToHostOverride;
};

/**
 * Base class for implementing matchmaking flow,  
 */
UCLASS(config = Game)
class UNREALTOURNAMENT_API UUTMatchmakingPolicy : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Is matchmaking canceling or canceled
	 *
	 * @return true if in any state that would indicate not matchmaking or otherwise canceling matchmaking
	 */
	virtual bool IsMatchmaking() const;
	
	/**
	 * Start matchmaking
	 */
	virtual void StartMatchmaking();

	/**
	 * Cancel matchmaking in progress
	 */
	virtual void CancelMatchmaking();

private:
	
	/**
	 * Delegate triggered when matchmaking is complete
	 *
	 * @param Result state matchmaking ended with
	 * @param SearchResult valid search result if matchmaking was successful, invalid otherwise
	 */
	DECLARE_DELEGATE_TwoParams(FOnMatchmakingComplete, EMatchmakingCompleteResult /** Result */, const FOnlineSessionSearchResult& /** SearchResult */);
	FOnMatchmakingComplete MatchmakingComplete;
	
	/**
	 * Delegate triggered when matchmaking state changes
	 *
	 * @param OldState leaving state
	 * @param NewState entering state
	 */
	DECLARE_DELEGATE_TwoParams(FOnMatchmakingStateChange, EMatchmakingState::Type /*OldState*/, EMatchmakingState::Type /*NewState*/);
	FOnMatchmakingStateChange MatchmakingStateChange;
	
protected:
	
	/**
	 * Generic function to cleanup after a join failure initiated at the policy level
	 * Cleans up the game session and signals matchmaking complete with a failure indication
	 */
	void CleanupJoinFailure();
	
	/**
	 * Delegate called when everything is cleaned up after a failure
	 *
	 * @param SessionName session that was cleaned up
	 * @param bWasSuccessful true if cleanup was successful, false otherwise
	 */
	void OnJoinFailureCleanupComplete(FName SessionName, bool bWasSuccessful);

	/** 
	 * Notify the caller that matchmaking has completed 
	 *
	 * @param Result result of the matchmaking
	 * @param SearchResult search result, if applicable, invalid otherwise
	 */
	void SignalMatchmakingComplete(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult);

	/** Name of the session acted upon */
	UPROPERTY(Transient)
	FName SessionName;

	/** Has matchmaking started */
	UPROPERTY(Transient)
	bool bMatchmakingInProgress;
	
	/** Transient properties during game creation/matchmaking */
	UPROPERTY(Transient)
	FMatchmakingParams CurrentParams;

	/** Helper to facilitate a single matchmaking pass */
	UPROPERTY(Transient)
	UUTSearchPass* MMPass;

	virtual UWorld* GetWorld() const override;

	/** Quick access to the world timer manager */
	FTimerManager& GetWorldTimerManager() const;

	/** Quick access to game instance */
	UUTGameInstance* GetUTGameInstance() const;

public:
	
	/**
	 * Initialize the matchmaking with the parameters desired for matchmaking
	 *
	 * @param InParams valid parameters for a new matchmaking attempt
	 */
	virtual void Init(const FMatchmakingParams& InParams);

	/**
	 * Get the search results found and the current search result being probed
	 *
	 * @return State of search result query
	 */
	FMMAttemptState GetMatchmakingState() const;

	/** @return the delegate fired when matchmaking state changes */
	FOnMatchmakingStateChange& OnMatchmakingStateChange() { return MatchmakingStateChange; }

	/** @return the delegate fired when matchmaking is complete for any reason */
	FOnMatchmakingComplete& OnMatchmakingComplete() { return MatchmakingComplete; }

};