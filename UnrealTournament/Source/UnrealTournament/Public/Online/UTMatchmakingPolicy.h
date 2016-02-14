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
		Flags(EMatchmakingFlags::None)
	{}

	~FMatchmakingParams() {}
	
	/** Id of the controller making the request */
	UPROPERTY()
	int32 ControllerId;
	/** Number of players in the party */
	UPROPERTY()
	int32 PartySize;
	/** Game mode to play */
	UPROPERTY()
	int32 PlaylistId;
	/** Matchmaking flags */
	UPROPERTY()
	EMatchmakingFlags Flags;
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
	 * Notify the caller that matchmaking has completed 
	 *
	 * @param Result result of the matchmaking
	 * @param SearchResult search result, if applicable, invalid otherwise
	 */
	void SignalMatchmakingComplete(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult);

protected:

	/** Has matchmaking started */
	UPROPERTY(Transient)
	bool bMatchmakingInProgress;
	
	/** Helper to facilitate a single matchmaking pass */
	UPROPERTY(Transient)
	UUTSearchPass* MMPass;
	
public:

	/** @return the delegate fired when matchmaking is complete for any reason */
	FOnMatchmakingComplete& OnMatchmakingComplete() { return MatchmakingComplete; }

};