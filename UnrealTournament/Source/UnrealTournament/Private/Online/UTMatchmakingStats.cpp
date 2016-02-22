// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/AnalyticsEventAttribute.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "UTMatchmakingStats.h"
#include "UTOnlineGameSettings.h"

#define STATS_VERSION 5
#define ATTEMPT_RECORDS_CONSTRAINT 100
#define ATTEMPT_SEND_CONSTRAINT 5

// Common attribution
const FString FUTMatchmakingStats::MMStatsSessionGuid = TEXT("MMStats_SessionId");
const FString FUTMatchmakingStats::MMStatsVersion = TEXT("MMStats_Version");

// Events
const FString FUTMatchmakingStats::MMStatsCompleteSearchEvent = TEXT("MMStats_CompleteSearchEvent");
const FString FUTMatchmakingStats::MMStatsSingleSearchEvent = TEXT("MMStats_SingleSearchEvent");

// Matchmaking total stats
const FString FUTMatchmakingStats::MMStatsSearchTimestamp = TEXT("MMStats_Timestamp");
const FString FUTMatchmakingStats::MMStatsSearchUserId = TEXT("MMStats_UserId");
const FString FUTMatchmakingStats::MMStatsTotalSearchTime = TEXT("MMStats_TotalSearchTime");
const FString FUTMatchmakingStats::MMStatsSearchAttemptCount = TEXT("MMStats_TotalAttemptCount");
const FString FUTMatchmakingStats::MMStatsTotalSearchResults = TEXT("MMStats_TotalSearchResultsCount");
const FString FUTMatchmakingStats::MMStatsIniSearchType = TEXT("MMStats_InitialSearchType");
const FString FUTMatchmakingStats::MMStatsEndSearchType = TEXT("MMStats_EndSearchType");
const FString FUTMatchmakingStats::MMStatsEndSearchResult = TEXT("MMStats_EndSearchResult");
const FString FUTMatchmakingStats::MMStatsEndQosDatacenterId = TEXT("MMStats_EndQosDatacenterId");
const FString FUTMatchmakingStats::MMStatsPartyMember = TEXT("MMStats_PartyMember");
const FString FUTMatchmakingStats::MMStatsSearchTypeNoneCount = TEXT("MMStats_SearchTypeNone_Count");
const FString FUTMatchmakingStats::MMStatsSearchTypeEmptyServerCount = TEXT("MMStats_EmptyServer_Count");
const FString FUTMatchmakingStats::MMStatsSearchTypeExistingSessionCount = TEXT("MMStats_SearchTypeExistingSession_Count");
const FString FUTMatchmakingStats::MMStatsSearchTypeJoinPresenceCount = TEXT("MMStats_SearchTypeJoinPresence_Count");
const FString FUTMatchmakingStats::MMStatsSearchTypeJoinInviteCount = TEXT("MMStats_SearchTypeJoinInvite_Count");

// Matchmaking attempt stats
const FString FUTMatchmakingStats::MMStatsSearchType = TEXT("MMStats_SearchType");
const FString FUTMatchmakingStats::MMStatsSearchAttemptTime = TEXT("MMStats_AttemptTime");
const FString FUTMatchmakingStats::MMStatsSearchAttemptEndResult = TEXT("MMStats_AttemptEndResult");
const FString FUTMatchmakingStats::MMStatsAttemptSearchResultCount = TEXT("MMStats_AttemptSearchResultCount");

// Qos stats
const FString FUTMatchmakingStats::MMStatsQosDatacenterId = TEXT("MMStats_QosDatacenterId");
const FString FUTMatchmakingStats::MMStatsQosTotalTime = TEXT("MMStats_QosTotalTime");
const FString FUTMatchmakingStats::MMStatsQosNumResults = TEXT("MMStats_QosNumResults");
const FString FUTMatchmakingStats::MMStatsQosSearchResultDetails = TEXT("MMStats_QosSearchDetails");

// Single search pass stats
const FString FUTMatchmakingStats::MMStatsSearchPassTime = TEXT("MMStats_SearchPassTime");
const FString FUTMatchmakingStats::MMStatsTotalDedicatedCount = TEXT("MMStats_NumDedicatedResults");
const FString FUTMatchmakingStats::MMStatsSearchPassNumResults = TEXT("MMStats_NumSearchPassResults");
const FString FUTMatchmakingStats::MMStatsTotalJoinAttemptTime = TEXT("MMStats_TotalJoinAttemptTime");

// Single search join attempt stats
const FString FUTMatchmakingStats::MMStatsSearchResultDetails = TEXT("MMStats_SearchResultDetails");
const FString FUTMatchmakingStats::MMStatsSearchJoinOwnerId = TEXT("MMStats_JoinOwnerId");
const FString FUTMatchmakingStats::MMStatsSearchJoinIsDedicated = TEXT("MMStats_JoinIsDedicated");
const FString FUTMatchmakingStats::MMStatsSearchJoinAttemptTime = TEXT("MMStats_JoinAttemptTime");
const FString FUTMatchmakingStats::MMStatsSearchJoinAttemptResult = TEXT("MMStats_JoinAttemptResult");

const FString FUTMatchmakingStats::MMStatsJoinAttemptNone_Count = TEXT("MMStats_JoinAttemptNone_Count");
const FString FUTMatchmakingStats::MMStatsJoinAttemptSucceeded_Count = TEXT("MMStats_JoinAttemptSucceeded_Count");
const FString FUTMatchmakingStats::MMStatsJoinAttemptFailedFull_Count = TEXT("MMStats_JoinAttemptFailedFull_Count");
const FString FUTMatchmakingStats::MMStatsJoinAttemptFailedTimeout_Count = TEXT("MMStats_JoinAttemptFailedTimeout_Count");
const FString FUTMatchmakingStats::MMStatsJoinAttemptFailedDenied_Count = TEXT("MMStats_JoinAttemptFailedDenied_Count");
const FString FUTMatchmakingStats::MMStatsJoinAttemptFailedDuplicate_Count = TEXT("MMStats_JoinAttemptFailedDuplicate_Count");
const FString FUTMatchmakingStats::MMStatsJoinAttemptFailedOther_Count = TEXT("MMStats_JoinAttemptFailedOther_Count");

/**
 * Debug output for the contents of a recorded stats event
 *
 * @param StatsEvent event type recorded
 * @param Attributes attribution of the event
 */
inline void PrintEventAndAttributes(const FString& StatsEvent, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	UE_LOG(LogUTAnalytics, Display, TEXT("Event: %s"), *StatsEvent);
	for (int32 AttrIdx=0; AttrIdx<Attributes.Num(); AttrIdx++)
	{
		const FAnalyticsEventAttribute& Attr = Attributes[AttrIdx];
		UE_LOG(LogUTAnalytics, Display, TEXT("\t%s : %s"), *Attr.AttrName, *Attr.AttrValue);
	}
}

FUTMatchmakingStats::FUTMatchmakingStats() :
	StatsVersion(STATS_VERSION),
	bAnalyticsInProgress(false),
	ControllerId(INVALID_CONTROLLERID)
{
}

void FUTMatchmakingStats::StartTimer(MMStats_Timer& Timer)
{
	Timer.MSecs = FPlatformTime::Seconds();
	Timer.bInProgress = true;
}

void FUTMatchmakingStats::StopTimer(MMStats_Timer& Timer)
{
	if (Timer.bInProgress)
	{
		Timer.MSecs = (FPlatformTime::Seconds() - Timer.MSecs) * 1000;
		Timer.bInProgress = false;
	}
}

void FUTMatchmakingStats::StartMatchmaking(EMatchmakingSearchType::Type InSearchType, TArray<FPlayerReservation>& PartyMembers, int32 LocalControllerId)
{
	FDateTime UTCTime = FDateTime::UtcNow();
	CompleteSearch.Timestamp = UTCTime.ToString();
	CompleteSearch.IniSearchType = InSearchType;
	ControllerId = LocalControllerId;

	for (int32 PlayerIdx = 0; PlayerIdx < PartyMembers.Num(); PlayerIdx++)
	{
		new (CompleteSearch.Members) MMStats_PartyMember(PartyMembers[PlayerIdx].UniqueId.GetUniqueNetId());
	}

	StartTimer(CompleteSearch.SearchTime);
	bAnalyticsInProgress = true;
}

void FUTMatchmakingStats::StartSearchAttempt(EMatchmakingSearchType::Type CurrentSearchType)
{
	if (bAnalyticsInProgress)
	{
		//stop any active timer in last search attempt
		if (CompleteSearch.SearchAttempts.Num() > 0)
		{
			FinalizeSearchAttempt(CompleteSearch.SearchAttempts.Last());
		}

		new (CompleteSearch.SearchAttempts) MMStats_Attempt();
		CompleteSearch.SearchAttempts.Last().SearchType = CurrentSearchType;
		StartTimer(CompleteSearch.SearchAttempts.Last().AttemptTime);
		CompleteSearch.TotalSearchAttempts++;

		if (CompleteSearch.SearchTypeCount.IsValidIndex(CurrentSearchType))
		{
			CompleteSearch.SearchTypeCount[CurrentSearchType]++;
		}

		//prevent size of attempts array goes too big
		if (CompleteSearch.SearchAttempts.Num() > ATTEMPT_RECORDS_CONSTRAINT)
		{
			CompleteSearch.SearchAttempts.RemoveAt(0);
		}
	}
}

void FUTMatchmakingStats::EndMatchmakingAttempt(EMatchmakingAttempt::Type Result)
{
	if (bAnalyticsInProgress)
	{
		if (CompleteSearch.SearchAttempts.Num() > 0)
		{
			MMStats_Attempt& SearchAttempt = CompleteSearch.SearchAttempts.Last();
			StopTimer(SearchAttempt.AttemptTime);
			SearchAttempt.AttemptResult = Result;
			CompleteSearch.TotalSearchResults += SearchAttempt.SearchPass.SearchResults.Num();
		}
		//use the latest result to update the complete search result
		CompleteSearch.EndResult = Result;
	}
}

void FUTMatchmakingStats::StartQosPass()
{
	if (bAnalyticsInProgress && CompleteSearch.SearchAttempts.Num() > 0)
	{
		StartTimer(CompleteSearch.SearchAttempts.Last().QosPass.SearchTime);
	}
}

void FUTMatchmakingStats::EndQosPass(const FUTOnlineSessionSearchBase& SearchSettings, const FString& DatacenterId)
{
	if (bAnalyticsInProgress && CompleteSearch.SearchAttempts.Num() > 0)
	{
		MMStats_Attempt& SearchAttempt = CompleteSearch.SearchAttempts.Last();
		StopTimer(SearchAttempt.QosPass.SearchTime);
		SearchAttempt.QosPass.BestDatacenterId = DatacenterId;
		for (int32 SearchIdx = 0; SearchIdx < SearchSettings.SearchResults.Num(); SearchIdx++)
		{
			const FOnlineSessionSearchResult& SearchResult = SearchSettings.SearchResults[SearchIdx];

			MMStats_QosSearchResult& NewSearchResult = *new (SearchAttempt.QosPass.SearchResults) MMStats_QosSearchResult();
			NewSearchResult.OwnerId = SearchResult.Session.OwningUserId;
			NewSearchResult.PingInMs = SearchResult.PingInMs;
			NewSearchResult.bIsValid = SearchResult.IsValid();

			FString TmpRegion;
			if (SearchResult.Session.SessionSettings.Get(SETTING_REGION, TmpRegion))
			{
				NewSearchResult.DatacenterId = TmpRegion;
			}
		}
	}
}

FUTMatchmakingStats::MMStats_SearchPass& FUTMatchmakingStats::GetCurrentSearchPass()
{
	check(CompleteSearch.SearchAttempts.Num() > 0);
	MMStats_Attempt& SearchAttempt = CompleteSearch.SearchAttempts.Last();
	return SearchAttempt.SearchPass;
}

void FUTMatchmakingStats::StartSearchPass()
{
	if (bAnalyticsInProgress && CompleteSearch.SearchAttempts.Num() > 0 )
	{
		MMStats_SearchPass& NewSearch = GetCurrentSearchPass();
		StartTimer(NewSearch.SearchTime);
	}
}

void FUTMatchmakingStats::EndSearchPass(const FUTOnlineSessionSearchBase& SearchSettings)
{
	if (bAnalyticsInProgress && CompleteSearch.SearchAttempts.Num() > 0 )
	{
		MMStats_SearchPass& CurrentSearchPass = GetCurrentSearchPass();
		StopTimer(CurrentSearchPass.SearchTime);

		for (int32 SearchIdx = 0; SearchIdx < SearchSettings.SearchResults.Num(); SearchIdx++)
		{
			const FOnlineSessionSearchResult& SearchResult = SearchSettings.SearchResults[SearchIdx];

			MMStats_SearchResult& NewSearchResult = *new (CurrentSearchPass.SearchResults) MMStats_SearchResult();
			NewSearchResult.OwnerId = SearchResult.Session.OwningUserId;
			NewSearchResult.bIsDedicated = SearchResult.Session.SessionSettings.bIsDedicated;
			NewSearchResult.bIsValid = SearchResult.IsValid();
		}
	}
}

void FUTMatchmakingStats::EndSearchPass(const FOnlineSessionSearchResult& SearchResult)
{
	if (bAnalyticsInProgress && CompleteSearch.SearchAttempts.Num() > 0)
	{
		MMStats_SearchPass& CurrentSearchPass = GetCurrentSearchPass();
		StopTimer(CurrentSearchPass.SearchTime);

		MMStats_SearchResult& NewSearchResult = *new (CurrentSearchPass.SearchResults) MMStats_SearchResult();
		NewSearchResult.OwnerId = SearchResult.Session.OwningUserId;
		NewSearchResult.bIsDedicated = SearchResult.Session.SessionSettings.bIsDedicated;
		NewSearchResult.bIsValid = SearchResult.IsValid();
	}
}

void FUTMatchmakingStats::StartJoinAttempt(int32 SessionIndex)
{
	if (bAnalyticsInProgress && CompleteSearch.SearchAttempts.Num() > 0)
	{
		MMStats_Attempt& SearchAttempt = CompleteSearch.SearchAttempts.Last();
		if (SearchAttempt.SearchPass.SearchResults.IsValidIndex(SessionIndex))
		{
			MMStats_SearchResult& SearchResult = SearchAttempt.SearchPass.SearchResults[SessionIndex];
			StartTimer(SearchResult.JoinActionTime);
		}
	}
}

void FUTMatchmakingStats::EndJoinAttempt(int32 SessionIndex, EPartyReservationResult::Type Result)
{
	if (bAnalyticsInProgress && CompleteSearch.SearchAttempts.Num() > 0)
	{
		MMStats_Attempt& SearchAttempt = CompleteSearch.SearchAttempts.Last();
		if (SearchAttempt.SearchPass.SearchResults.IsValidIndex(SessionIndex))
		{
			MMStats_SearchResult& SearchResult = SearchAttempt.SearchPass.SearchResults[SessionIndex];
			StopTimer(SearchResult.JoinActionTime);
			switch (Result)
			{
			case EPartyReservationResult::PartyLimitReached:
				SearchResult.JoinResult = EMatchmakingJoinAction::JoinFailed_Full;
				break;
			case EPartyReservationResult::RequestTimedOut:
				SearchResult.JoinResult = EMatchmakingJoinAction::JoinFailed_Timeout;
				break;
			case EPartyReservationResult::ReservationAccepted:
				SearchResult.JoinResult = EMatchmakingJoinAction::JoinSucceeded;
				break;
			case EPartyReservationResult::ReservationDenied:
				SearchResult.JoinResult = EMatchmakingJoinAction::JoinFailed_Denied;
				break;
			case EPartyReservationResult::ReservationDenied_Banned:
				SearchResult.JoinResult = EMatchmakingJoinAction::JoinFailed_Banned;
				break;
			case EPartyReservationResult::ReservationDuplicate:
				SearchResult.JoinResult = EMatchmakingJoinAction::JoinFailed_Duplicate;
				break;
			case EPartyReservationResult::ReservationRequestCanceled:
				SearchResult.JoinResult = EMatchmakingJoinAction::JoinFailed_Canceled;
				break;
			case EPartyReservationResult::GeneralError:
			case EPartyReservationResult::BadSessionId:
			case EPartyReservationResult::IncorrectPlayerCount:
			case EPartyReservationResult::ReservationNotFound:
			case EPartyReservationResult::ReservationInvalid:
			default:
				SearchResult.JoinResult = EMatchmakingJoinAction::JoinFailed_Other;
				break;
			}
		}
	}
}

void FUTMatchmakingStats::Finalize()
{
	StopTimer(CompleteSearch.SearchTime);
	for (auto& SearchAttempt : CompleteSearch.SearchAttempts)
	{
		FinalizeSearchAttempt(SearchAttempt);
	}
	bAnalyticsInProgress = false;
}

void FUTMatchmakingStats::FinalizeSearchPass(MMStats_SearchPass& SearchPass)
{
	StopTimer(SearchPass.SearchTime);
	for (int32 SessionIdx = 0; SessionIdx < SearchPass.SearchResults.Num(); SessionIdx++)
	{
		MMStats_SearchResult& SearchResult = SearchPass.SearchResults[SessionIdx];
		StopTimer(SearchResult.JoinActionTime);
	}
}

void FUTMatchmakingStats::FinalizeSearchAttempt(MMStats_Attempt& SearchAttempt){
	StopTimer(SearchAttempt.QosPass.SearchTime);
	StopTimer(SearchAttempt.AttemptTime);
	FinalizeSearchPass(SearchAttempt.SearchPass);
}

/**
* @EventName MMStats_SingleSearchEvent
* @Trigger Matchmaking completed for some reason
* @Type static
* @EventParam MMStats_SessionId string Guid of this whole matchmaking process
* @EventParam MMStats_Version integer Matchmaking analytics version
* @EventParam MMStats_SearchType string Matchmaking type of this single search
* @EventParam MMStats_AttemptTime float Total time used in this single matchmaking search, includes delay after each join attempt actions (ms)
* @EventParam MMStats_AttemptEndResult string End result of this single matchmaking search
* @EventParam MMStats_AttemptSearchResultCount integer Number of results return in this single matchmaking search
* @EventParam MMStats_QosDatacenterId string Selected data center for this single matchmaking
* @EventParam MMStats_QosTotalTime float Time spent on finding data center (ms)
* @EventParam MMStats_QosNumResults integer Number of Qos results returned
* @EventParam MMStats_QosSearchDetails Array JSON serialization of Qos results, seperated by comma
* @EventParam MMStats_SearchPassTime float Time spent on finding sessions (ms)
* @EventParam MMStats_TotalJoinAttemptTime float Time spent on trying to join sessions  (ms)
* @EventParam MMStats_NumDedicatedResults integer Number of dedicated results
* @EventParam MMStats_NumSearchPassResults integer Number of results got from finding sessions
* @EventParam MMStats_JoinAttemptNone_Count integer Counting of EMatchmakingJoinAction::NoAttempt
* @EventParam MMStats_JoinAttemptSucceeded_Count integer Counting of EMatchmakingJoinAction::JoinSucceeded
* @EventParam MMStats_JoinAttemptFailedFull_Count integer Counting of EMatchmakingJoinAction::JoinFailed_Full
* @EventParam MMStats_JoinAttemptFailedTimeout_Count integer Counting of EMatchmakingJoinAction::JoinFailed_Timeout
* @EventParam MMStats_JoinAttemptFailedDenied_Count integer Counting of EMatchmakingJoinAction::JoinFailed_Denied
* @EventParam MMStats_JoinAttemptFailedDuplicate_Count integer Counting of EMatchmakingJoinAction::JoinFailed_Duplicate
* @EventParam MMStats_JoinAttemptFailedOther_Count integer Counting of EMatchmakingJoinAction::JoinFailed_Other
* @EventParam MMStats_SearchResultDetails Array JSON serialization of search pass and join results in this single matchmaking, separated by comma
* @Comments Analytics data for a single matchmaking search attempt
*/
void FUTMatchmakingStats::ParseSearchAttempt(TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, FGuid& SessionId, MMStats_Attempt& SearchAttempt)
{
	TArray<FAnalyticsEventAttribute> MMAttributes;
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSessionGuid, SessionId.ToString()));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsVersion, StatsVersion));

	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchType, EMatchmakingSearchType::ToString(SearchAttempt.SearchType)));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchAttemptTime, SearchAttempt.AttemptTime.MSecs));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchAttemptEndResult, EMatchmakingAttempt::ToString(SearchAttempt.AttemptResult)));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsAttemptSearchResultCount, SearchAttempt.SearchPass.SearchResults.Num()));
	//Qos stats
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsQosDatacenterId, SearchAttempt.QosPass.BestDatacenterId));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsQosTotalTime, SearchAttempt.QosPass.SearchTime.MSecs));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsQosNumResults, SearchAttempt.QosPass.SearchResults.Num()));
	FString QosDetail;
	if (SearchAttempt.QosPass.SearchResults.Num() > 0)
	{
		for (auto& QosResult : SearchAttempt.QosPass.SearchResults)
		{
			QosDetail += FString::Printf(TEXT("{\"OwnerId\":\"%s\", \"DatacenterId\":\"%s\", \"PingInMs\":%d, \"bIsValid\":%s},"),
											QosResult.OwnerId.IsValid() ? *QosResult.OwnerId->ToString() : TEXT("Unknown"),
											*QosResult.DatacenterId,
											QosResult.PingInMs,
											QosResult.bIsValid ? TEXT("true") : TEXT("false")
										);
		}
		QosDetail = QosDetail.LeftChop(1);
	}
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsQosSearchResultDetails, QosDetail));
	//SearchPass stats
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchPassTime, SearchAttempt.SearchPass.SearchTime.MSecs));

	int32 TotalSearchResults = 0;
	int32 TotalDedicated = 0;
	double TotalJoinAttemptTime = 0;
	TArray<int32> JoinActionCount;
	JoinActionCount.AddZeroed(EMatchmakingJoinAction::JoinFailed_Other + 1);
	for (auto& SearchResult : SearchAttempt.SearchPass.SearchResults)
	{
		if (SearchResult.bIsValid)
		{
			JoinActionCount[SearchResult.JoinResult]++;
			TotalDedicated += SearchResult.bIsDedicated ? 1 : 0;
			TotalSearchResults++;
			TotalJoinAttemptTime += SearchResult.JoinActionTime.MSecs;
		}
	}

	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsTotalJoinAttemptTime, TotalJoinAttemptTime));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsTotalDedicatedCount, TotalDedicated));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchPassNumResults, TotalSearchResults));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsJoinAttemptNone_Count, JoinActionCount[EMatchmakingJoinAction::NoAttempt]));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsJoinAttemptSucceeded_Count, JoinActionCount[EMatchmakingJoinAction::JoinSucceeded]));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsJoinAttemptFailedFull_Count, JoinActionCount[EMatchmakingJoinAction::JoinFailed_Full]));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsJoinAttemptFailedTimeout_Count, JoinActionCount[EMatchmakingJoinAction::JoinFailed_Timeout]));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsJoinAttemptFailedDenied_Count, JoinActionCount[EMatchmakingJoinAction::JoinFailed_Denied]));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsJoinAttemptFailedDuplicate_Count, JoinActionCount[EMatchmakingJoinAction::JoinFailed_Duplicate]));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsJoinAttemptFailedOther_Count, JoinActionCount[EMatchmakingJoinAction::JoinFailed_Other]));
	FString SearchResultDetail;
	if (SearchAttempt.SearchPass.SearchResults.Num() > 0)
	{
		for (auto& SearchResult : SearchAttempt.SearchPass.SearchResults)
		{
			SearchResultDetail += FString::Printf(TEXT("{\"OwnerId\":\"%s\", \"bIsDedicated\":%s, \"JoinActionTime\":%.0f, \"JoinResult\":\"%s\"},"),
													SearchResult.OwnerId.IsValid() ? *SearchResult.OwnerId->ToString() : TEXT("Unknown"),
													SearchResult.bIsDedicated ? TEXT("true") : TEXT("false"),
													SearchResult.JoinActionTime.MSecs,
													EMatchmakingJoinAction::ToString(SearchResult.JoinResult)
												);
		}
		SearchResultDetail = SearchResultDetail.LeftChop(1);
	}
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchResultDetails, SearchResultDetail));
	AnalyticsProvider->RecordEvent(MMStatsSingleSearchEvent, MMAttributes);
}


/**
* @EventName MMStats_CompleteSearchEvent
* @Trigger Matchmaking completed for some reason(Joined successfully or canceled)
* @Type static
* @EventParam MMStats_SessionId string Guid of this whole matchmaking process
* @EventParam MMStats_Version integer Matchmaking analytics version
* @EventParam MMStats_Timestamp string Timestamp when this whole matchmaking started
* @EventParam MMStats_UserId string User who started the matchmaking
* @EventParam MMStats_TotalSearchTime float Total time this complete matchmaking took, includes delay between two single searches (ms)
* @EventParam MMStats_TotalAttemptCount integer Total number of search attempts in this complete matchmaking
* @EventParam MMStats_TotalSearchResultsCount integer Sum of results of all matchmaking attempts from the complete matchmaking process
* @EventParam MMStats_InitialSearchType string Initial search type of this complete matchmaking process
* @EventParam MMStats_EndSearchType string Search type of the last search attempt in this complete matchmaking process 
* @EventParam MMStats_EndSearchResult string Result of the last search attempt in this complete matchmaking process
* @EventParam MMStats_EndQosDatacenterId string Data center selected in the last search attempt
* @EventParam MMStats_PartyMember Array Party member Ids, seperated by comma
* @Comments Analytics data for a complete matchmaking search attempt
*/
void FUTMatchmakingStats::ParseMatchmakingResult(TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, FGuid& SessionId, const FUniqueNetId& MatchmakingUserId)
{
	TArray<FAnalyticsEventAttribute> MMAttributes;

	// Matchmaking results header
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSessionGuid, SessionId.ToString()));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsVersion, StatsVersion));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchTimestamp, CompleteSearch.Timestamp));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchUserId, MatchmakingUserId.ToString()));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsTotalSearchTime, CompleteSearch.SearchTime.MSecs));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchAttemptCount, CompleteSearch.TotalSearchAttempts));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsTotalSearchResults, CompleteSearch.TotalSearchResults));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsIniSearchType, EMatchmakingSearchType::ToString(CompleteSearch.IniSearchType)));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsEndSearchType, EMatchmakingSearchType::ToString(CompleteSearch.SearchAttempts.Num()>0 ? CompleteSearch.SearchAttempts.Last().SearchType : CompleteSearch.IniSearchType)));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsEndSearchResult, EMatchmakingAttempt::ToString(CompleteSearch.EndResult)));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsEndQosDatacenterId, CompleteSearch.SearchAttempts.Num()>0 ? CompleteSearch.SearchAttempts.Last().QosPass.BestDatacenterId : TEXT("Unknown")));

	//put party member id into single column, separated by comma
	FString PartyMembers;
	if (CompleteSearch.Members.Num() > 0)
	{
		for (auto& MemberId : CompleteSearch.Members)
		{
			PartyMembers += MemberId.UniqueId->ToString() + ",";
		}
		PartyMembers = PartyMembers.LeftChop(1);
	}
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsPartyMember, PartyMembers));
	
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchTypeNoneCount, CompleteSearch.SearchTypeCount.IsValidIndex(EMatchmakingSearchType::None) ? CompleteSearch.SearchTypeCount[EMatchmakingSearchType::None] : 0));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchTypeEmptyServerCount, CompleteSearch.SearchTypeCount.IsValidIndex(EMatchmakingSearchType::EmptyServer) ? CompleteSearch.SearchTypeCount[EMatchmakingSearchType::EmptyServer] : 0));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchTypeExistingSessionCount, CompleteSearch.SearchTypeCount.IsValidIndex(EMatchmakingSearchType::ExistingSession) ? CompleteSearch.SearchTypeCount[EMatchmakingSearchType::ExistingSession] : 0));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchTypeJoinPresenceCount, CompleteSearch.SearchTypeCount.IsValidIndex(EMatchmakingSearchType::JoinPresence) ? CompleteSearch.SearchTypeCount[EMatchmakingSearchType::JoinPresence] : 0));
	MMAttributes.Add(FAnalyticsEventAttribute(MMStatsSearchTypeJoinInviteCount, CompleteSearch.SearchTypeCount.IsValidIndex(EMatchmakingSearchType::JoinInvite) ? CompleteSearch.SearchTypeCount[EMatchmakingSearchType::JoinInvite] : 0));

	AnalyticsProvider->RecordEvent(MMStatsCompleteSearchEvent, MMAttributes);

	//Constraint the result to be sent within the number indicated by ATTEMPT_SEND_CONSTRAINT 
	if (CompleteSearch.SearchAttempts.Num() > ATTEMPT_SEND_CONSTRAINT)
	{
		CompleteSearch.SearchAttempts.RemoveAt(0, CompleteSearch.SearchAttempts.Num() - ATTEMPT_SEND_CONSTRAINT);
	}
	for (auto& SearchAttempt : CompleteSearch.SearchAttempts)
	{
		ParseSearchAttempt(AnalyticsProvider, SessionId, SearchAttempt);
	}

}

void FUTMatchmakingStats::EndMatchmakingAndUpload(TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FUniqueNetId& MatchmakingUserId)
{
	if (bAnalyticsInProgress)
	{
		Finalize();

		if (AnalyticsProvider.IsValid())
		{		
			// GUID representing the entire matchmaking attempt
			FGuid MMStatsGuid;
			FPlatformMisc::CreateGuid(MMStatsGuid);

			ParseMatchmakingResult(AnalyticsProvider, MMStatsGuid, MatchmakingUserId);
		}
		else
		{
			UE_LOG(LogUTAnalytics, Log, TEXT("No analytics provider for matchmaking analytics upload."));
		}
	}
}

int32 FUTMatchmakingStats::GetMMLocalControllerId()
{
	return ControllerId;
}
