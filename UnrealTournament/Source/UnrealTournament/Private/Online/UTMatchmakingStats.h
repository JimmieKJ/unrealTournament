// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PartyBeaconHost.h"

/** Types of matchmaking searches possible */
namespace EMatchmakingSearchType
{
	enum Type
	{
		/** Initialization value */
		None,
		/** Searching for an empty server to host a session */
		EmptyServer,
		/** Searching for an existing session */
		ExistingSession,
		/** Searching for a single session */
		JoinPresence,
		/** Joining an invite */
		JoinInvite
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EMatchmakingSearchType::Type MMSearchType)
	{
		switch (MMSearchType)
		{
		case None:
			{
				return TEXT("Undefined search");
			}
		case EmptyServer:
			{
				return TEXT("Empty server search");
			}
		case ExistingSession:
			{
				return TEXT("Existing session search");
			}
		case JoinPresence:
			{
				return TEXT("Join presence");
			}
		case JoinInvite:
			{
				return TEXT("Join invite");
			}
		}
		return TEXT("");
	}
}

/** Types of possible results from contacting a reservation beacon */
namespace EMatchmakingJoinAction
{
	enum Type
	{
		/** Initialization value */
		NoAttempt,
		/** Beacon accepted reservation request */
		JoinSucceeded,
		/** Beacon denied reservation as the server is full */
		JoinFailed_Full,
		/** Beacon connection timed out */
		JoinFailed_Timeout,
		/** Beacon denied reservation request, may be in server travel */
		JoinFailed_Denied,
		/** Beacon denied duplicate reservation request */
		JoinFailed_Duplicate,
		/** Beacon denied due to party member banned state */
		JoinFailed_Banned,
		/** Beacon request was canceled before it was sent */
		JoinFailed_Canceled,
		/** Beacon denied reservation for misc reason */
		JoinFailed_Other
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EMatchmakingJoinAction::Type MMJoinActionType)
	{
		switch (MMJoinActionType)
		{
		case NoAttempt:
			{
				return TEXT("No attempt");
			}
		case JoinSucceeded:
			{
				return TEXT("Reservation Accepted");
			}
		case JoinFailed_Full:
			{
				return TEXT("Reservation failed: full");
			}
		case JoinFailed_Timeout:
			{
				return TEXT("Reservation failed: timeout");
			}
		case JoinFailed_Denied:
			{
				return TEXT("Reservation failed: denied");
			}
		case JoinFailed_Duplicate:
			{
				return TEXT("Reservation failed: duplicate");
			}
		case JoinFailed_Banned:
			{
				return TEXT("Reservation failed: banned");
			}
		case JoinFailed_Canceled:
			{
				return TEXT("Reservation failed: canceled");
			}
		case JoinFailed_Other:
			{
				return TEXT("Reservation failed: other");
			}
		}
		return TEXT("");
	}
}

/** Types of possible endings to a matchmaking attempt */
namespace EMatchmakingAttempt
{
	enum Type
	{
		/** Initialization value */
		None,
		/** Successful join */
		SuccessJoin,
		/** Locally canceled matchmaking */
		LocalCancel,
		/** Ended in error */
		EndedInError,
		/** Matchmaking completed with no join */
		EndedNoResults
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EMatchmakingAttempt::Type MMAtemptType)
	{
		switch (MMAtemptType)
		{
		case None:
			{
				return TEXT("No attempt");
			}
		case SuccessJoin:
			{
				return TEXT("Successful join");
			}
		case LocalCancel:
			{
				return TEXT("Canceled locally");
			}
		case EndedInError:
			{
				return TEXT("Ending in error");
			}
		case EndedNoResults:
			{
				return TEXT("Ended with nothing to join");
			}
		}
		return TEXT("");
	}
}

/**
 * Matchmaking stats
 */
class FUTMatchmakingStats 
{
private:

	struct MMStats_Timer
	{
		/** Time in ms captured */
		double MSecs;
		/** Is this timer running */
		bool bInProgress;

		MMStats_Timer() :
			MSecs(0.0),
			bInProgress(false)
		{}
	};

	/** Stats representation of a party member */
	struct MMStats_PartyMember
	{
		/** Unique id of the party member */
		TSharedPtr<const FUniqueNetId> UniqueId;

		MMStats_PartyMember(const TSharedPtr<const FUniqueNetId>& InUniqueId) :
			UniqueId(InUniqueId)
		{}
	};

	/** Stats representation of a single search result */
	struct MMStats_SearchResult
	{
		/** Time in ms it took to contact the reservation beacon */
		MMStats_Timer JoinActionTime;
		/** Owner of the session */
		TSharedPtr<const FUniqueNetId> OwnerId;
		/** Is the session a dedicated server */
		bool bIsDedicated;
		/** Result of the join attempt */
		EMatchmakingJoinAction::Type JoinResult;
		/** Is this result valid */
		bool bIsValid;

		MMStats_SearchResult() :
			OwnerId(NULL),
			bIsDedicated(false),
			JoinResult(EMatchmakingJoinAction::NoAttempt),
			bIsValid(false)
		{}
	};

	/** Stats representation of a single search pass */
	struct MMStats_SearchPass
	{
		/** Time in ms it took to find the search results (exclusive) */
		MMStats_Timer SearchTime;
		/** Array of search result details found this pass */
		TArray<MMStats_SearchResult> SearchResults;
	};

	/** Stats representation of a single Qos search result */
	struct MMStats_QosSearchResult
	{
		/** Owner of the session */
		TSharedPtr<const FUniqueNetId> OwnerId;
		/** Datacenter Id */
		FString DatacenterId;
		/** Ping time */
		int32 PingInMs;
		/** Is this result valid */
		bool bIsValid;

		MMStats_QosSearchResult() :
			OwnerId(NULL),
			PingInMs(0),
			bIsValid(false)
		{}
	};

	/** Stats representation of a single Qos search pass */
	struct MMStats_QosSearchPass
	{
		/** Id of the datacenter chosen */
		FString BestDatacenterId;
		/** Time in ms it took to find the search results (exclusive) */
		MMStats_Timer SearchTime;
		/** Array of search result details found this pass */
		TArray<MMStats_QosSearchResult> SearchResults;
	};

	/** Stats representation of a complete matchmaking attempt */
	struct MMStats_Attempt
	{
		/** Info pertaining to the single Qos search run each attempt */
		MMStats_QosSearchPass QosPass;
		/** Info pertaining a successive search pass */
		MMStats_SearchPass SearchPass;
		/** Type of initial matchmaking search */
		EMatchmakingSearchType::Type SearchType;
		/** Time in ms of the entire matchmaking attempt (inclusive) */
		MMStats_Timer AttemptTime;
		/** Result of the matchmaking attempt */
		EMatchmakingAttempt::Type AttemptResult;

		MMStats_Attempt() :
			SearchType(EMatchmakingSearchType::None),
			AttemptResult(EMatchmakingAttempt::None)
		{}
	};

	struct MMStats_CompleteSearchPass
	{
		/** Time of the search */
		FString Timestamp;
		/** Party members involved in the search */
		TArray<MMStats_PartyMember> Members;
		/** Time in ms of the entire matchmaking procedure (inclusive) */
		MMStats_Timer SearchTime;
		/** Info pertaining to each search attempt */
		TArray<MMStats_Attempt> SearchAttempts;
		/** Counting of total search attempts in this complete search pass */
		int32 TotalSearchAttempts;
		/** Counting of total search results in this complete search pass */
		int32 TotalSearchResults;
		/** Type of initial matchmaking search */
		EMatchmakingSearchType::Type IniSearchType;
		/** Result of the matchmaking */
		EMatchmakingAttempt::Type EndResult;
		/** Counting of different search types of search attempts */
		TArray<int32> SearchTypeCount;

		MMStats_CompleteSearchPass() :
			TotalSearchAttempts(0),
			TotalSearchResults(0),
			IniSearchType(EMatchmakingSearchType::None),
			EndResult(EMatchmakingAttempt::None)
		{
			SearchTypeCount.AddZeroed(EMatchmakingSearchType::JoinInvite + 1);
		}
	};


	// Common attribution
	static const FString MMStatsSessionGuid;
	static const FString MMStatsVersion;

	// Events
	static const FString MMStatsCompleteSearchEvent;
	static const FString MMStatsSingleSearchEvent;

	// Matchmaking total stats
	static const FString MMStatsTotalSearchTime;
	static const FString MMStatsSearchTimestamp;
	static const FString MMStatsSearchUserId;
	static const FString MMStatsSearchAttemptCount;
	static const FString MMStatsTotalSearchResults;
	static const FString MMStatsIniSearchType;
	static const FString MMStatsEndSearchType;
	static const FString MMStatsEndSearchResult;
	static const FString MMStatsEndQosDatacenterId;
	static const FString MMStatsPartyMember;
	static const FString MMStatsSearchTypeNoneCount;
	static const FString MMStatsSearchTypeEmptyServerCount;
	static const FString MMStatsSearchTypeExistingSessionCount;
	static const FString MMStatsSearchTypeJoinPresenceCount;
	static const FString MMStatsSearchTypeJoinInviteCount;

	// Matchmaking single attempt stats
	static const FString MMStatsSearchType;
	static const FString MMStatsSearchAttemptTime;
	static const FString MMStatsSearchAttemptEndResult;
	static const FString MMStatsAttemptSearchResultCount;

	// Qos stats
	static const FString MMStatsQosDatacenterId;
	static const FString MMStatsQosTotalTime;
	static const FString MMStatsQosNumResults;
	static const FString MMStatsQosSearchResultDetails;

	// Single search pass stats
	static const FString MMStatsSearchPassTime;
	static const FString MMStatsTotalDedicatedCount;
	static const FString MMStatsSearchPassNumResults;
	static const FString MMStatsTotalJoinAttemptTime;

	// Single search join attempt stats
	static const FString MMStatsSearchResultDetails;
	static const FString MMStatsSearchJoinOwnerId;
	static const FString MMStatsSearchJoinIsDedicated;
	static const FString MMStatsSearchJoinAttemptTime;
	static const FString MMStatsSearchJoinAttemptResult;

	static const FString MMStatsJoinAttemptNone_Count;
	static const FString MMStatsJoinAttemptSucceeded_Count;
	static const FString MMStatsJoinAttemptFailedFull_Count;
	static const FString MMStatsJoinAttemptFailedTimeout_Count;
	static const FString MMStatsJoinAttemptFailedDenied_Count;
	static const FString MMStatsJoinAttemptFailedDuplicate_Count;
	static const FString MMStatsJoinAttemptFailedOther_Count;


	/** Version of the stats for separation */
	int32 StatsVersion;

	/** Container of an entire search process */
	MMStats_CompleteSearchPass CompleteSearch;

	/** Matchmaking analytics in progress */
	bool bAnalyticsInProgress;

	/** The localplayer who start the matchmaking */
	int32 ControllerId;

	/**
	 * Start the timer for a given stats container 
	 *
	 * @param Timer Timer to start
	 */
	void StartTimer(MMStats_Timer& Timer);

	/**
	 * Stops the timer for a given stats container 
	 *
	 * @param Timer Timer to stop
	 */
	void StopTimer(MMStats_Timer& Timer);

	/**
	 * Get the container for the current search pass,
	 * returning a new container if one is not started
	 *
	 * @return Stats representation of a single search pass
	 */
	MMStats_SearchPass& GetCurrentSearchPass();

	/**
	 * Finalize all the data, stopping timers, etc
	 */
	void Finalize();

	/**
	 * Finalize all the data for a single search pass,
	 * stopping timers, etc
	 *
	 * @param SearchPass pass to finalize
	 */
	void FinalizeSearchPass(MMStats_SearchPass& SearchPass);

	/**
	* Finalize all the data for a single search attempt,
	* stopping timers, etc
	*
	* @param SearchAttempt attempt to finalize
	*/
	void FinalizeSearchAttempt(MMStats_Attempt& SearchAttempt);

	/**
	* Parse a single search attempt, adding its data to the recorded event
	*
	* @param AnalyticsProvider provider recording the event
	* @param SessionId unique id for the matchmaking event
	* @param SearchAttempt search attempt data to add to the event
	*/
	void ParseSearchAttempt(TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, FGuid& SessionId, MMStats_Attempt& SearchAttempt);

	/**
	 * Parse an entire search, adding its data to the recorded event
	 *
	 * @param AnalyticsProvider provider recording the event
	 * @param SessionId unique id for the matchmaking event
	 * @param MatchmakingUserId unique id for the user initiating the attempt
	 */
	void ParseMatchmakingResult(TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, FGuid& SessionId, const FUniqueNetId& MatchmakingUserId);

public:

	FUTMatchmakingStats();
	virtual ~FUTMatchmakingStats() {}

	/**
	 * Start recording an entire matchmaking search process
	 *
	 * @param InSearchType type of matchmaking attempt
	 * @param PartyMembers players involved in the attempt
	 * @param LocalControllerId id of the player who start matchmaking
	 */
	void StartMatchmaking(EMatchmakingSearchType::Type InSearchType, TArray<FPlayerReservation>& PartyMembers, int32 LocalControllerId);

	/**
	* Start recording an single search attempt;
	*/
	void StartSearchAttempt(EMatchmakingSearchType::Type CurrentSearchType);

	/**
	 * Start a Qos search pass
	 */
	void StartQosPass();

	/**
	 * End recording of a Qos search pass within the matchmaking attempt
	 *
	 * @param SearchSettings results of the search pass
	 * @param DatacenterId resulting datacenter choice
	 */
	void EndQosPass(const class FUTOnlineSessionSearchBase& SearchSettings, const FString& DatacenterId);

	/**
	 * Start recording a new search pass within the matchmaking attempt
	 */
	void StartSearchPass();

	/**
	 * End recording of a search pass within the matchmaking attempt
	 * 
	 * @param SearchSettings results of the search pass
	 */
	void EndSearchPass(const class FUTOnlineSessionSearchBase& SearchSettings);

	/**
	 * End recording of a search pass within the matchmaking attempt (friend search version)
	 * 
	 * @param SearchResult results of the search pass
	 */
	void EndSearchPass(const class FOnlineSessionSearchResult& SearchResult);

	/**
	 * Start recording a new attempt to join a single search result
	 *
	 * @param SessionIndex index of the search result
	 */
	void StartJoinAttempt(int32 SessionIndex);

	/**
	 * End recording a search result join attempt
	 *
	 * @param SessionIndex index of the search result
	 * @param Result result of the join attempt
	 */
	void EndJoinAttempt(int32 SessionIndex, EPartyReservationResult::Type Result);

	/**
	* End recording of an entire matchmaking attempt
	*
	* @param Result result of the matchmaking attempt
	*/
	void EndMatchmakingAttempt(EMatchmakingAttempt::Type Result);

	/**
	 * Record previously saved matchmaking stats to an analytics provider
	 *
	 * @param AnalyticsProvider provider to record stats to
	 * @param MatchmakingUserId unique id for the user initiating the attempt
	 */
	void EndMatchmakingAndUpload(TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FUniqueNetId& MatchmakingUserId);

	/**
	* Get the controller id of the player who start matchmaking
	*
	*/
	int32 GetMMLocalControllerId();

};
