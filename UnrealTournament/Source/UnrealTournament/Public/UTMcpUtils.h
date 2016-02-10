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
	/** Only used for DM */
	UPROPERTY()
	TArray<int32> IndividualScores;
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
struct FAccountElo
{
	GENERATED_USTRUCT_BODY()
public:
	FAccountElo() : Rating(0), NumGamesPlayed(0) {}

	UPROPERTY()
	int32 Rating;
	UPROPERTY()
	int32 NumGamesPlayed;
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
	typedef TFunction<void(const FOnlineError& /*Result*/, const FAccountElo& /*Response*/)> FGetAccountEloCb;

	/**
	* Query the server for an individual user's MMR (needed for team builder)
	* NOTE: this currently will only succeed if this is the user's own account ID
	*
	* @param RatingType The type of rating we are querying for
	* @param Callback Callback delegate
	*/
	void GetAccountElo(const FString& RatingType, const FGetAccountEloCb& Callback);

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

	/** Access to the online subsystem */
	FOnlineSubsystemMcp* McpSubsystem;
	/** Game Account Id of the requestor */
	TSharedPtr<const FUniqueNetId> GameAccountId;

	TSharedRef<FOnlineHttpRequest> CreateRequest(const FString& Verb, const FString& Path) const;

	/** Generic send http request */
	void SendRequest(const TSharedRef<class FOnlineHttpRequest>& Request, const TFunction<bool(const FHttpResponsePtr& HttpResponse)>& OnComplete);

	/**
	* Delegate fired when Http Requests complete (generic)
	*/
	void HttpRequestComplete(TSharedRef<FHttpRetrySystem::FRequest>& HttpRequest, bool bSucceeded, TFunction<bool(const FHttpResponsePtr& HttpResponse)> OnComplete);
	
	TWeakObjectPtr<UWorld> OwnerWorld;
};