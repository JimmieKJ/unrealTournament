// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Runtime/Online/HTTP/Public/Http.h"
#include "Runtime/Online/OnlineSubsystem/Public/OnlineError.h"

#include "UTMcpUtils.generated.h"


USTRUCT()
struct FRankedTeamMemberInfo
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY()
	FString AccountId;
	UPROPERTY()
	int32 Score;
	UPROPERTY()
	bool IsBot;
};

USTRUCT()
struct FRankedTeamInfo
{
	GENERATED_USTRUCT_BODY()
public:
	FRankedTeamInfo() : SocialPartySize(1) {}

	UPROPERTY()
	TArray<FRankedTeamMemberInfo> Members;
	UPROPERTY()
	int32 SocialPartySize;
};


USTRUCT()
struct FRankedMatchInfo
{
	GENERATED_USTRUCT_BODY()
public:
	FRankedMatchInfo() : RedScore(0.0f), MatchLengthSeconds(0) {}

	UPROPERTY()
	FString SessionId;
	UPROPERTY()
	float RedScore;
	UPROPERTY()
	int32 MatchLengthSeconds;
};

USTRUCT()
struct FRankedMatchResult
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY()
	FString RatingType;
	UPROPERTY()
	FRankedMatchInfo MatchInfo;
	UPROPERTY()
	FRankedTeamInfo RedTeam;
	UPROPERTY()
	FRankedTeamInfo BlueTeam;
};

USTRUCT()
struct FTeamElo
{
	GENERATED_USTRUCT_BODY()
public:
	FTeamElo() : Rating(0) {}

	UPROPERTY()
	int32 Rating;
};

USTRUCT()
struct FBulkAccountMmrQuery
{
	GENERATED_USTRUCT_BODY()
public:
	FBulkAccountMmrQuery() {}
	
	UPROPERTY()
	TArray<FString> RatingTypes;
};

USTRUCT()
struct FAccountMmr
{
	GENERATED_USTRUCT_BODY()
public:
	FAccountMmr() : Rating(0), NumGamesPlayed(0) {}

	UPROPERTY()
	int32 Rating;
	UPROPERTY()
	int32 NumGamesPlayed;
};

USTRUCT()
struct FBulkAccountMmr
{
	GENERATED_USTRUCT_BODY()
public:
	FBulkAccountMmr() {}

	UPROPERTY()
	TArray<FString> RatingTypes;
	UPROPERTY()
	TArray<int32> Ratings;
	UPROPERTY()
	TArray<int32> NumGamesPlayed;
};

USTRUCT()
struct FAccountLeague
{
	GENERATED_USTRUCT_BODY()
public:
	FAccountLeague() : Tier(0), Division(0), Points(0), IsInPromotionSeries(false), PromotionMatchesAttempted(0), 
					   PromotionMatchesWon(0), PlacementMatchesAttempted(0) {}

	UPROPERTY()
	int32 Tier;
	UPROPERTY()
	int32 Division;
	UPROPERTY()
	int32 Points;
	UPROPERTY()
	bool IsInPromotionSeries;
	UPROPERTY()
	int32 PromotionMatchesAttempted;
	UPROPERTY()
	int32 PromotionMatchesWon;
	UPROPERTY()
	int32 PlacementMatchesAttempted;
};

// Class that handles various talking to various endpoints with mcp
UCLASS(config = Game)
class UNREALTOURNAMENT_API UUTMcpUtils : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	static UUTMcpUtils* Get(UWorld* World, const TSharedPtr<const FUniqueNetId>& InGameAccountId);

public:
	typedef TFunction<void(const FOnlineError& /*Result*/, const FTeamElo& /*Response*/)> FGetTeamEloCb;

	/**
	* Query the server for the effective Elo of the listed accounts
	*
	* @param RatingType The type of rating we are querying for
	* @param AccountIds Array of Epic accountIDs for the players on the team
	* @param SocialPartySize How big the social party was that this team was seeded from.
	* @param Callback Callback delegate
	*/
	void GetTeamElo(const FString& RatingType, const TArray<FUniqueNetIdRepl>& AccountIds, int32 SocialPartySize, const FGetTeamEloCb& Callback);

public:
	typedef TFunction<void(const FOnlineError& /*Result*/, const FAccountMmr& /*Response*/)> FGetAccountMmrCb;

	/**
	* Query the server for an individual user's MMR
	* NOTE: this currently will only succeed if this is the user's own account ID
	*
	* @param RatingType The type of rating we are querying for
	* @param Callback Callback delegate
	*/
	void GetAccountMmr(const FString& RatingType, const FGetAccountMmrCb& Callback);

public:
	typedef TFunction<void(const FOnlineError& /*Result*/, const FBulkAccountMmr& /*Response*/)> FGetBulkAccountMmrCb;

	/**
	* Query the server for an individual user's MMR
	* NOTE: this currently will only succeed if this is the user's own account ID
	*
	* @param RatingType The type of rating we are querying for
	* @param Callback Callback delegate
	*/
	void GetBulkAccountMmr(const TArray<FString>& RatingTypes, const FGetBulkAccountMmrCb& Callback);

public:
	typedef TFunction<void(const FOnlineError& /*Result*/, const FAccountLeague& /*Response*/)> FGetAccountLeagueCb;

	/**
	* Query the server for an individual user's league info
	* NOTE: this currently will only succeed if this is the user's own account ID
	*
	* @param LeagueType The type of rating we are querying for
	* @param Callback Callback delegate
	*/
	void GetAccountLeague(const FString& LeagueType, const FGetAccountLeagueCb& Callback);

public:
	typedef TFunction<void(const FOnlineError& /*Result*/)> FReportRankedMatchResultCb;

	/**
	* Dedicated Server Only
	* Report the results of a ranked match to MCP for MMR adjustment.
	*
	* @param MatchResult The input structure containing team rosters for both teams as well as info about who won
	* @param Callback Called when the request completes
	*/
	void ReportRankedMatchResult(const FRankedMatchResult& MatchResult, const FReportRankedMatchResultCb& Callback);

private:

	static UUTMcpUtils* McpUtilsSingleton;

#if WITH_PROFILE
	/** Access to the online subsystem */
	FOnlineSubsystemMcp* McpSubsystem;
#endif

	/** Game Account Id of the requestor */
	TSharedPtr<const FUniqueNetId> GameAccountId;

#if WITH_PROFILE
	TSharedRef<FOnlineHttpRequest> CreateRequest(const FString& Verb, const FString& Path) const;

	/** Generic send http request */
	void SendRequest(const TSharedRef<class IHttpRequest>& Request, const TFunction<bool(const FHttpResponsePtr& HttpResponse)>& OnComplete);

	/**
	 * Delegate fired when Http Requests complete (generic)
	 */
	void HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TFunction<bool(const FHttpResponsePtr& HttpResponse)> OnComplete);
#endif

	TWeakObjectPtr<UWorld> OwnerWorld;
};