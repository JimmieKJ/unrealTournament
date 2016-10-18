// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#if WITH_PROFILE
#include "OnlineSubsystemMcp.h"
#include "GameServiceMcp.h"
#include "OnlineHttpRequest.h"
#endif

#include "OnlineSubsystemUtils.h"
#include "JsonUtilities.h"

#include "UTMcpUtils.h"

template<typename USTRUCT_T>
static FString JsonSerialize(const USTRUCT_T& Params)
{
	FString JsonOut;
	if (!FJsonObjectConverter::UStructToJsonObjectString(USTRUCT_T::StaticStruct(), &Params, JsonOut, 0, 0))
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("Failed to serialize JSON"));
		return FString();
	}
	return JsonOut;
}

UUTMcpUtils* UUTMcpUtils::McpUtilsSingleton = nullptr;

UUTMcpUtils::UUTMcpUtils(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
#if WITH_PROFILE
, McpSubsystem(nullptr)
#endif
, GameAccountId(nullptr)
{
}

UUTMcpUtils* UUTMcpUtils::Get(UWorld* World, const TSharedPtr<const FUniqueNetId>& InGameAccountId)
{
	check(World);
	if (McpUtilsSingleton && McpUtilsSingleton->OwnerWorld.Get() != World)
	{
		// new UWorld, throw this one away and make a new utils object
		McpUtilsSingleton->RemoveFromRoot();
		McpUtilsSingleton = nullptr;
	}

#if WITH_PROFILE
	if (!McpUtilsSingleton)
	{
		FOnlineSubsystemMcp* WorldMcp = (FOnlineSubsystemMcp*)Online::GetSubsystem(World, MCP_SUBSYSTEM);
		if (WorldMcp == nullptr)
		{
			// can't get UTMcpUtils in no-mcp mode
			return nullptr;
		}

		// ok, save it
		McpUtilsSingleton = NewObject<UUTMcpUtils>();
		McpUtilsSingleton->OwnerWorld = World;
		McpUtilsSingleton->McpSubsystem = WorldMcp;
		McpUtilsSingleton->AddToRoot();
	}

	// set this every time
	if (InGameAccountId.IsValid() && InGameAccountId->IsValid())
	{
		McpUtilsSingleton->GameAccountId = InGameAccountId;
	}
#endif

	return McpUtilsSingleton;
}

#if WITH_PROFILE
TSharedRef<FOnlineHttpRequest> UUTMcpUtils::CreateRequest(const FString& Verb, const FString& Path) const
{
	check(McpSubsystem);

	// create the request
	TSharedRef<FOnlineHttpRequest> HttpRequest = McpSubsystem->CreateRequest();
	HttpRequest->SetURL(McpSubsystem->GetMcpGameService()->GetBaseUrl() + Path);
	HttpRequest->SetVerb(Verb);
	return HttpRequest;
}
#endif

#if WITH_PROFILE
void UUTMcpUtils::SendRequest(TSharedRef<FOnlineHttpRequest>& HttpRequest, const TFunction<bool(const FHttpResponsePtr& HttpResponse)>& OnComplete)
{
	check(McpSubsystem);

	// bind the callback delegate
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &ThisClass::HttpRequestComplete, OnComplete);

	UE_LOG(LogOnlineGame, Verbose, TEXT("MCP-Utils: Dispatching request to %s"), *HttpRequest->GetURL());
	
	// dispatch with User or Client auth
	FString ErrorStr;
	bool bStarted = false;
	FGameServiceMcpPtr McpGameService = McpSubsystem->GetMcpGameService();
	if (IsRunningDedicatedServer() || !GameAccountId.IsValid())
	{
		const FServicePermissionsMcp* ServicePerms = McpGameService->GetMcpConfiguration().GetServicePermissionsByName(FServicePermissionsMcp::DefaultServer);
		if (ServicePerms != NULL)
		{
			bStarted = McpSubsystem->ProcessRequestAsClient(*McpGameService, ServicePerms->Id, HttpRequest, ErrorStr);
		}
		else
		{
			ErrorStr = TEXT("Unable to find service permissions for server.");
		}
	}
	else
	{
		bStarted = McpSubsystem->ProcessRequest(*McpGameService, *GameAccountId, HttpRequest, ErrorStr);
	}

	if (!bStarted)
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("Failed to start profile request. Error: %s"), *ErrorStr);
		McpSubsystem->ExecuteNextTick([HttpRequest]() {
			HttpRequest->CancelRequest();
		});
	}
}
#endif

#if WITH_PROFILE
void UUTMcpUtils::HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, TFunction<bool(const FHttpResponsePtr& HttpResponse)> OnComplete)
{
	if (HttpResponse.IsValid() && bSucceeded)
	{
		// fire the completion handler in all cases
		if (OnComplete(HttpResponse))
		{
			UE_LOG_ONLINE(Verbose, TEXT("McpUtils request %s complete. code=%d"),
				*HttpRequest->GetURL(), HttpResponse->GetResponseCode());
		}
		else
		{
			UE_LOG_ONLINE(Error, TEXT("McpUtils request %s failed. code=%d response=%s"),
				*HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
		}
	}
	else
	{
		UE_LOG_ONLINE(Error, TEXT("McpUtils request %s failed. No Response"), *HttpRequest->GetURL());

		// fire the completion handler
		OnComplete(FHttpResponsePtr());
	}
}
#endif

template<typename RESPONSE_T>
static TFunction<bool(const FHttpResponsePtr&)> SimpleResponseHandler(const TFunction<void(const FOnlineError&, const RESPONSE_T&)>& Callback)
{
	return [Callback](const FHttpResponsePtr& HttpResponse) {
		FOnlineError Result;
		RESPONSE_T Response;

#if WITH_PROFILE
		// parse the result
		FMcpQueryResult McpResult;
		auto JsonValue = McpResult.Parse(HttpResponse);
		Result.bSucceeded = McpResult.bSucceeded;

		if (Result.bSucceeded)
		{
			if (!JsonValue.IsValid() || !FJsonObjectConverter::JsonObjectToUStruct(JsonValue->AsObject().ToSharedRef(), &Response, 0, 0))
			{
				Result.bSucceeded = false;
				Result.SetFromErrorCode(TEXT("UnableToParseResponse"));
			}
		}
#endif

		// fire the callback
		Callback(Result, Response);
		return Result.bSucceeded;
	};
}

void UUTMcpUtils::GetEstimatedWaitTimes(const FGetEstimatedWaitTimesCb& Callback)
{
#if WITH_PROFILE
	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/wait_times/estimate");
	auto HttpRequest = CreateRequest(TEXT("GET"), ServerPath
		);

	// send the request
	SendRequest(HttpRequest, [Callback](const FHttpResponsePtr& HttpResponse) {

		FOnlineError Result;
		FEstimatedWaitTimeInfo EstimatedWaitTimeInfo;
		if (HttpResponse.IsValid() && EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()) && HttpResponse->GetContentType() == TEXT("application/json"))
		{
			auto JsonValue = FMcpQueryResult::Parse(Result, HttpResponse);
			for (const auto& It : JsonValue->AsArray())
			{
				FWaitTimeInfo WaitTime;
				const TSharedPtr<FJsonObject>& Obj = It->AsObject();
				if (Obj.IsValid())
				{
					WaitTime.RatingType = Obj->GetStringField(TEXT("ratingType"));
					WaitTime.NumSamples = Obj->GetIntegerField(TEXT("numSamples"));
					WaitTime.AverageWaitTimeSecs = Obj->GetNumberField(TEXT("averageWaitTimeSecs"));
					EstimatedWaitTimeInfo.WaitTimes.Add(WaitTime);
				}
			}
		}
		else if (HttpResponse.IsValid())
		{
			UE_LOG(LogOnlineGame, Warning, TEXT("GetEstimatedWaitTimes Error: %d %s"), EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()) ? 1 : 0, *HttpResponse->GetContentType());
		}

		Callback(Result, EstimatedWaitTimeInfo);
		return Result.bSucceeded;
	});
#endif
}

void UUTMcpUtils::ReportWaitTime(const FString& RatingType, int32 WaitTime, const FReportWaitTimeCb& Callback)
{
#if WITH_PROFILE
	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/wait_times/report/`ratingType/`waitTime");
	auto HttpRequest = CreateRequest(TEXT("GET"), ServerPath
		.Replace(TEXT("`ratingType"), *RatingType, ESearchCase::CaseSensitive)
		.Replace(TEXT("`waitTime"), *FString::FromInt(WaitTime), ESearchCase::CaseSensitive)
		);

	// send the request
	SendRequest(HttpRequest, [Callback](const FHttpResponsePtr& HttpResponse) {
		// parse the result
		FOnlineError Result;
		FMcpQueryResult::Parse(Result, HttpResponse);

		// fire the callback
		Callback(Result);
		return Result.bSucceeded;
	});
#endif
}

void UUTMcpUtils::GetTeamElo(const FString& RatingType, const TArray<FUniqueNetIdRepl>& AccountIds, int32 SocialPartySize, const FGetTeamEloCb& Callback)
{
#if WITH_PROFILE
	// build parameters struct
	FRankedTeamInfo Team;
	Team.SocialPartySize = SocialPartySize;
	for (const FUniqueNetIdWrapper& AccountId : AccountIds)
	{
		if (AccountId.IsValid())
		{
			FRankedTeamMemberInfo Member;
			Member.AccountId = AccountId.ToString();
			Team.Members.Add(Member);
		}
	}

	// check for error case
	if (Team.Members.Num() <= 0)
	{
		McpSubsystem->ExecuteNextTick([Callback]() {
			Callback(FOnlineError(TEXT("NoTeamMembers")), FTeamElo());
		});
		return;
	}

	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/team/elo/`ratingType");
	auto HttpRequest = CreateRequest(TEXT("POST"), ServerPath
		.Replace(TEXT("`ratingType"), *RatingType, ESearchCase::CaseSensitive)
		);

	// send the request
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(JsonSerialize(Team));
	SendRequest(HttpRequest, SimpleResponseHandler(Callback));
#endif
}

void UUTMcpUtils::GetTeamHighestMmr(const FString& RatingType, const TArray<FUniqueNetIdRepl>& AccountIds, const FGetTeamHighestMmrCb& Callback)
{
#if WITH_PROFILE
	// build parameters struct
	FRankedTeamInfo Team;
	for (const FUniqueNetIdWrapper& AccountId : AccountIds)
	{
		if (AccountId.IsValid())
		{
			FRankedTeamMemberInfo Member;
			Member.AccountId = AccountId.ToString();
			Team.Members.Add(Member);
		}
	}

	// check for error case
	if (Team.Members.Num() <= 0)
	{
		McpSubsystem->ExecuteNextTick([Callback]() {
			Callback(FOnlineError(TEXT("NoTeamMembers")), FHighestMmr());
		});
		return;
	}

	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/team/highestmmr/`ratingType");
	auto HttpRequest = CreateRequest(TEXT("POST"), ServerPath
		.Replace(TEXT("`ratingType"), *RatingType, ESearchCase::CaseSensitive)
		);

	// send the request
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(JsonSerialize(Team));
	SendRequest(HttpRequest, SimpleResponseHandler(Callback));
#endif
}

void UUTMcpUtils::GetAccountMmr(const FString& RatingType, const FGetAccountMmrCb& Callback)
{
#if WITH_PROFILE
	if (!GameAccountId.IsValid())
	{
		return;
	}

	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/account/`accountId/mmr/`ratingType");
	auto HttpRequest = CreateRequest(TEXT("GET"), ServerPath
		.Replace(TEXT("`accountId"), *GameAccountId->ToString(), ESearchCase::CaseSensitive)
		.Replace(TEXT("`ratingType"), *RatingType, ESearchCase::CaseSensitive)
		);

	// send the request
	SendRequest(HttpRequest, SimpleResponseHandler(Callback));
#endif
}

void UUTMcpUtils::GetSpecifiedAccountMmr(const FString& RatingType, const TSharedPtr<const FUniqueNetId> AccountIDToQuery, const FGetAccountMmrCb& Callback)
{
#if WITH_PROFILE
	if (!AccountIDToQuery.IsValid())
	{
		return;
	}

	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/account/`accountId/mmr/`ratingType");
	auto HttpRequest = CreateRequest(TEXT("GET"), ServerPath
		.Replace(TEXT("`accountId"), *AccountIDToQuery->ToString(), ESearchCase::CaseSensitive)
		.Replace(TEXT("`ratingType"), *RatingType, ESearchCase::CaseSensitive)
		);

	// send the request
	SendRequest(HttpRequest, SimpleResponseHandler(Callback));

#endif
}

void UUTMcpUtils::GetBulkAccountMmr(const TArray<FString>& RatingTypes, const FGetBulkAccountMmrCb& Callback)
{
#if WITH_PROFILE
	if (!GameAccountId.IsValid())
	{
		return;
	}

	FBulkAccountMmrQuery Query;
	Query.RatingTypes = RatingTypes;

	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/account/`accountId/mmrbulk");
	auto HttpRequest = CreateRequest(TEXT("POST"), ServerPath
		.Replace(TEXT("`accountId"), *GameAccountId->ToString(), ESearchCase::CaseSensitive)
		);

	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(JsonSerialize(Query));

	// send the request
	SendRequest(HttpRequest, SimpleResponseHandler(Callback));
#endif
}

void UUTMcpUtils::GetBulkSpecifiedAccountMmr(const TArray<FString>& RatingTypes, const TSharedPtr<const FUniqueNetId> AccountIDToQuery, const FGetBulkAccountMmrCb& Callback)
{
#if WITH_PROFILE
	if (!AccountIDToQuery.IsValid())
	{
		return;
	}

	FBulkAccountMmrQuery Query;
	Query.RatingTypes = RatingTypes;

	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/account/`accountId/mmrbulk");
	auto HttpRequest = CreateRequest(TEXT("POST"), ServerPath
		.Replace(TEXT("`accountId"), *AccountIDToQuery->ToString(), ESearchCase::CaseSensitive)
		);

	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(JsonSerialize(Query));

	// send the request
	SendRequest(HttpRequest, SimpleResponseHandler(Callback));
#endif
}

void UUTMcpUtils::GetAccountLeague(const FString& LeagueType, const FGetAccountLeagueCb& Callback)
{
#if WITH_PROFILE
	if (!GameAccountId.IsValid())
	{
		return;
	}

	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/account/`accountId/league/`leagueType");
	auto HttpRequest = CreateRequest(TEXT("GET"), ServerPath
		.Replace(TEXT("`accountId"), *GameAccountId->ToString(), ESearchCase::CaseSensitive)
		.Replace(TEXT("`leagueType"), *LeagueType, ESearchCase::CaseSensitive)
		);

	// send the request
	SendRequest(HttpRequest, SimpleResponseHandler(Callback));
#endif
}

void UUTMcpUtils::ReportRankedMatchResult(const FRankedMatchResult& MatchResult, const FReportRankedMatchResultCb& Callback)
{
#if WITH_PROFILE
	// check for error case
	if (MatchResult.RedTeam.Members.Num() <= 0 && MatchResult.BlueTeam.Members.Num() <= 0)
	{
		McpSubsystem->ExecuteNextTick([Callback]() {
			Callback(FOnlineError(TEXT("NoRedOrBlueTeamMembers")));
		});
		return;
	}

	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/team/match_result");
	auto HttpRequest = CreateRequest(TEXT("POST"), ServerPath);

	// send the request
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(JsonSerialize(MatchResult));
	SendRequest(HttpRequest, [Callback](const FHttpResponsePtr& HttpResponse) {
		// parse the result
		FMcpQueryResult QueryResult;
		QueryResult.Parse(HttpResponse);

		FOnlineError Result(QueryResult.bSucceeded);

		// fire the callback
		Callback(Result);
		return Result.bSucceeded;
	});
#endif
}