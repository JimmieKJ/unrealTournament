// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "OnlineSubsystemMcp.h"
#include "OnlineSubsystemUtils.h"
#include "GameServiceMcp.h"
#include "JsonUtilities.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineHttpRequest.h"

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
, McpSubsystem(nullptr)
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
	return McpUtilsSingleton;
}

TSharedRef<FOnlineHttpRequest> UUTMcpUtils::CreateRequest(const FString& Verb, const FString& Path) const
{
	check(McpSubsystem);

	// create the request
	TSharedRef<FOnlineHttpRequest> HttpRequest = McpSubsystem->CreateRequest();
	HttpRequest->SetURL(McpSubsystem->GetMcpGameService()->GetBaseUrl() + Path);
	HttpRequest->SetVerb(Verb);
	return HttpRequest;
}

void UUTMcpUtils::SendRequest(const TSharedRef<FOnlineHttpRequest>& RequestIn, const TFunction<bool(const FHttpResponsePtr& HttpResponse)>& OnComplete)
{
	check(McpSubsystem);
	TSharedRef<FOnlineHttpRequest> Request = RequestIn;

	// bind the callback delegate
	Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::HttpRequestComplete, OnComplete);
	UE_LOG(LogOnlineGame, Verbose, TEXT("MCP-Utils: Dispatching request to %s"), *Request->GetURL());

	// dispatch with User or Client auth
	FString ErrorStr;
	bool bStarted = false;
	FGameServiceMcpPtr McpGameService = McpSubsystem->GetMcpGameService();
	if (IsRunningDedicatedServer() || !GameAccountId.IsValid())
	{
		const FServicePermissionsMcp* ServicePerms = McpGameService->GetMcpConfiguration().GetServicePermissionsByName(FServicePermissionsMcp::DefaultServer);
		if (ServicePerms != NULL)
		{
			bStarted = McpSubsystem->ProcessRequestAsClient(*McpGameService, ServicePerms->Id, Request, ErrorStr);
		}
		else
		{
			ErrorStr = TEXT("Unable to find service permissions for server.");
		}
	}
	else
	{
		bStarted = McpSubsystem->ProcessRequest(*McpGameService, *GameAccountId, Request, ErrorStr);
	}

	if (!bStarted)
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("Failed to start profile request. Error: %s"), *ErrorStr);
		FOnlineSubsystemMcp* Capture = McpSubsystem; // this is safe since it's this subsystem that's calling tick in the first place
		McpSubsystem->ExecuteNextTick([Capture, Request]() {
			TSharedRef<FOnlineHttpRequest> RequestStupid = Request; // needed because CancelRequest is not const-correct
			Capture->CancelRequest(RequestStupid);
		});
	}
}

void UUTMcpUtils::HttpRequestComplete(TSharedRef<FHttpRetrySystem::FRequest>& HttpRequest, bool bSucceeded, TFunction<bool(const FHttpResponsePtr& HttpResponse)> OnComplete)
{
	FHttpResponsePtr HttpResponse = HttpRequest->GetResponse();
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

template<typename RESPONSE_T>
static TFunction<bool(const FHttpResponsePtr&)> SimpleResponseHandler(const TFunction<void(const FOnlineError&, const RESPONSE_T&)>& Callback)
{
	return [Callback](const FHttpResponsePtr& HttpResponse) {
		FOnlineError Result;
		RESPONSE_T Response;

		// parse the result
		auto JsonValue = FMcpQueryResult::Parse(Result, HttpResponse);
		if (Result.bSucceeded)
		{
			if (!JsonValue.IsValid() || !FJsonObjectConverter::JsonObjectToUStruct(JsonValue->AsObject().ToSharedRef(), &Response, 0, 0))
			{
				Result.bSucceeded = false;
				Result.SetFromErrorCode(TEXT("UnableToParseResponse"));
			}
		}

		// fire the callback
		Callback(Result, Response);
		return Result.bSucceeded;
	};
}

void UUTMcpUtils::GetTeamElo(const FString& RatingType, const TArray<FUniqueNetIdRepl>& AccountIds, int32 SocialPartySize, const FGetTeamEloCb& Callback)
{
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
}

void UUTMcpUtils::GetAccountMmr(const FString& RatingType, const FGetAccountMmrCb& Callback)
{
	// build request URL
	static const FString ServerPath = TEXT("/api/game/v2/ratings/account/`accountId/mmr/`ratingType");
	auto HttpRequest = CreateRequest(TEXT("GET"), ServerPath
		.Replace(TEXT("`accountId"), *GameAccountId->ToString(), ESearchCase::CaseSensitive)
		.Replace(TEXT("`ratingType"), *RatingType, ESearchCase::CaseSensitive)
		);

	// send the request
	SendRequest(HttpRequest, SimpleResponseHandler(Callback));
}

void UUTMcpUtils::ReportRankedMatchResult(const FRankedMatchResult& MatchResult, const FReportRankedMatchResultCb& Callback)
{
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
		FOnlineError Result;
		FMcpQueryResult::Parse(Result, HttpResponse);

		// fire the callback
		Callback(Result);
		return Result.bSucceeded;
	});
}