// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "McpProfileSysPCH.h"
#include "OnlineIdentityMcp.h"
#include "McpProfileGroup.h"
#include "McpProfile.h"
#include "GameServiceMcp.h"
#include "Runtime/JsonUtilities/Public/JsonUtilities.h"
#include "OnlineHttpRequest.h"

FDedicatedServerUrlContext FDedicatedServerUrlContext::Default;
FClientUrlContext FClientUrlContext::Default;

UMcpProfileGroup::UMcpProfileGroup(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bIsInitialized = false;
	bIsServer = false;
	bIsProcessingRequestGenerator = false;
	OnlineSubMcp = nullptr;
	LocalTimeOffset.Zero();
}

//virtual 
void UMcpProfileGroup::Initialize(FOnlineSubsystemMcp* OnlineSysIn, bool bIsServerIn, const FUniqueNetIdWrapper& GameAccountIdIn)
{
	check(!bIsInitialized); // multiple initialize not allowed
	bIsInitialized = true;

	OnlineSubMcp = OnlineSysIn;
	bIsServer = bIsServerIn;
	GameAccountId = GameAccountIdIn;
	CredentialsAccountId = GameAccountId;
	PlayerName = GameAccountId.ToString(); // default
}

UMcpProfile* UMcpProfileGroup::AddProfile(const FString& ProfileId, TSubclassOf<UMcpProfile> ProfileClass, bool bReplaceExisting)
{
	// make sure this is a new ID
	for (FProfileEntry& Entry : ProfileList)
	{
		if (Entry.ProfileId == ProfileId)
		{
			if (bReplaceExisting)
			{
				Entry.ProfileObject = NewObject<UMcpProfile>(GetOuter(), *ProfileClass);
				Entry.ProfileObject->Initialize(this, ProfileId);
			}
			return Entry.ProfileObject;
		}
	}

	// make a new entry
	FProfileEntry Entry;
	Entry.ProfileId = ProfileId;
	Entry.ProfileObject = NewObject<UMcpProfile>(GetOuter(), *ProfileClass);
	Entry.ProfileObject->Initialize(this, ProfileId);
	ProfileList.Add(Entry);
	return Entry.ProfileObject;
}

UMcpProfile* UMcpProfileGroup::GetProfile(const FString& ProfileId) const
{
	for (const FProfileEntry& Entry : ProfileList)
	{
		if (Entry.ProfileId == ProfileId)
		{
			return Entry.ProfileObject;
		}
	}
	return nullptr;
}

bool UMcpProfileGroup::IsCurrentInterval(const FDateTime& Instant, int32 HoursPerInterval, FDateTime* IntervalEnd) const
{
	// get the current time in UTC
	FDateTime Now = GetServerDateTime();

	// get the interval surrounding Instant
	FDateTime Begin, End;
	GetRefreshInterval(Begin, End, Instant, HoursPerInterval);
	if (IntervalEnd != nullptr)
	{
		*IntervalEnd = End;
	}

	// return whether now is within that interval
	return Now >= Begin && Now < End;
}

void UMcpProfileGroup::GetRefreshInterval(FDateTime& StartOut, FDateTime& EndOut, const FDateTime& Center, int32 HoursPerInterval) const
{
	// get an instant for the beginning of today
	int32 Year = Center.GetYear();
	int32 Month = Center.GetMonth();
	int32 Day = Center.GetDay();
	FDateTime Today = FDateTime(Year, Month, Day);

	// figure out the calendar date for tomorrow
	Day = Day + 1;
	int32 DaysInMonth = FDateTime::DaysInMonth(Year, Month);
	if (Day > DaysInMonth)
	{
		Day = 1;
		++Month;
		if (Month > 12)
		{
			Month = 1;
			++Year;
		}
	}
	FDateTime Tomorrow = FDateTime(Year, Month, Day);

	// we can't support intervals more than a day and still remain locked to the gregorian calendar
	if (HoursPerInterval <= 0)
	{
		StartOut = Today;
		EndOut = Tomorrow;
		return;
	}

	// what is the length of the interval
	FTimespan Interval = FTimespan::FromHours(HoursPerInterval);

	// compute a cutoff so we don't have a tiny interval at the end of days with more than 24 hours (leapseconds)
	FDateTime Cutoff = Tomorrow - FTimespan::FromMinutes(59);

	// see which interval Center is actually in
	StartOut = Today;
	EndOut = StartOut + Interval;
	while (EndOut < Cutoff)
	{
		if (Center < EndOut)
		{
			return;
		}
		StartOut = EndOut;
		EndOut += Interval;
	}

	// clamp at end of day (not cutoff)
	EndOut = Tomorrow;
}

void UMcpProfileGroup::RefreshAllProfiles()
{
	for (const FProfileEntry& Entry : ProfileList)
	{
		Entry.ProfileObject->ForceQueryProfile(FMcpQueryComplete());
	}
}

FString UMcpProfileGroup::FormatUrl(const FString& Route, const FString& AccountId, const FString& ProfileId) const
{
	check(OnlineSubMcp);
	if (OnlineSubMcp->GetMcpGameService().IsValid())
	{
		FString FinalRoute = Route.Replace(TEXT("`accountId"), *AccountId, ESearchCase::CaseSensitive);
		FinalRoute = FinalRoute.Replace(TEXT("`profileId"), *ProfileId, ESearchCase::CaseSensitive);
		return OnlineSubMcp->GetMcpGameService()->GetBaseUrl() + FinalRoute;
	}
	return FString();
}

bool UMcpProfileGroup::TriggerMcpRpcCommand(UFunction* Function, void* Parameters, UMcpProfile* SourceProfile)
{
	// verify preconditions
	check(Function);
	check(Parameters);
	check(SourceProfile);

	// Look for a request options struct to override the default one
	FBaseUrlContext* Context = NULL;
	for(TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		UProperty* Property = *It;

		if (Property->HasAnyPropertyFlags(CPF_RepSkip))
		{
			UStructProperty *StructProperty = Cast<UStructProperty>(Property);
			if (StructProperty && StructProperty->Struct->IsChildOf(FBaseUrlContext::StaticStruct()))
			{
				FBaseUrlContext* Value = (FBaseUrlContext*)Property->ContainerPtrToValuePtr<uint8>(Parameters);
				if (Value)
				{
					Context = Value;
					break;
				}
			}
		}
	}
	check(Context != nullptr); // we need to have a context object somewhere in the parameters array

	return TriggerMcpRpcCommand(Function->GetName(), Function, Parameters, CPF_Parm, *Context, SourceProfile);
}

bool UMcpProfileGroup::TriggerMcpRpcCommand(const FString& CommandName, const UStruct* Struct, const void* Data, int32 CheckFlags, const FBaseUrlContext& Context, UMcpProfile* SourceProfile)
{
	check(Struct);
	check(Data);
	check(SourceProfile);

	FMcpQueryResult QueryResult;
	if (OnlineSubMcp)
	{
		// build URL based on context
		FString CommandRoute;
		static const TCHAR* ConfigSection = TEXT("OnlineSubsystemMcp.McpProfile");
		if (GConfig->GetString(ConfigSection, *Context.GetUrlConfig(), CommandRoute, GEngineIni))
		{
			TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
			//Construct JSON object
			if(FJsonObjectConverter::UStructToJsonObject(Struct,Data,JsonObject,CheckFlags,CPF_ReturnParm | CPF_RepSkip))
			{

				// compute the URL
				FString CommandUrl = FormatUrl(CommandRoute, GetGameAccountId().ToString(), SourceProfile->GetProfileId());
				CommandUrl = CommandUrl.Replace(TEXT("`fname"), *CommandName, ESearchCase::CaseSensitive);

				// make an http request
				TSharedRef<FOnlineHttpRequest> HttpRequest = OnlineSubMcp->CreateRequest();
				HttpRequest->SetURL(CommandUrl);
				HttpRequest->SetVerb(TEXT("POST"));
				HttpRequest->SetHeader(TEXT("Accept"), TEXT("*/*"));

				// let the source profile prepare the http request
				if (SourceProfile->PrepareRpcCommand(CommandName, HttpRequest, JsonObject))
				{
					// bind our delegate after prepare
					HttpRequest->OnProcessRequestComplete().BindUObject(this, &ThisClass::GenericResponse_HttpRequestComplete, CommandName, Context.GetCallback());

					// queue the request
					QueueRequest(HttpRequest, Context.GetCredentials(), SourceProfile);
					QueryResult.bSucceeded = true;
				}
				else
				{
					QueryResult.ErrorCode = TEXT("InternalError");
					QueryResult.ErrorMessage = NSLOCTEXT("McpProfile", "FailPrepare", "Request failed to prepare");
				}
			}
			else
			{
				QueryResult.ErrorCode = TEXT("InternalError");
				QueryResult.ErrorMessage = NSLOCTEXT("McpProfile", "FailSerialize", "UStruct request failed to convert to JSON");
			}
		}
		else
		{
			QueryResult.ErrorCode = TEXT("InternalError");
			QueryResult.ErrorMessage = FText::Format(NSLOCTEXT("McpProfile", "FailConfig", "Unable to resolve UrlConfig at section '{0}' '{1}'"),
				FText::FromString(ConfigSection), FText::FromString(Context.GetUrlConfig()));
		}
	}
	else
	{
		QueryResult.ErrorCode = TEXT("InternalError");
		QueryResult.ErrorMessage = NSLOCTEXT("McpProfile", "NoMcp", "Request failed due to no Mcp");
	}

	// dispatch failures to callback immediately
	if (!QueryResult.bSucceeded)
	{
		UE_LOG(LogProfileSys, Warning, TEXT("MCP-Profile: Error dispatching %s %s"), *CommandName, *QueryResult.ErrorMessage.ToString());
		Context.GetCallback().ExecuteIfBound(QueryResult);
	}

	return QueryResult.bSucceeded;
}

void UMcpProfileGroup::SendRequestNow(const FProfilePendingHttpRequest& Request)
{
	if (Request.IsRequestGenerator())
	{
		HandleRequestGenerator(Request);
		return;
	}

	check(Request.HttpRequest.IsValid());
	FString Url = Request.HttpRequest->GetURL();
	if (Request.SourceProfile)
	{
		int32 QueryPos = 0;
		if (Url.FindChar(TCHAR('?'), QueryPos))
		{
			// append to existing query string
			Url = FString::Printf(TEXT("%s&rvn=%lld"), *Url, Request.SourceProfile->GetProfileRevision());
		}
		else
		{
			// add a query string
			Url = FString::Printf(TEXT("%s?rvn=%lld"), *Url, Request.SourceProfile->GetProfileRevision());
		}
		Request.HttpRequest->SetURL(Url);
	}
	UE_LOG(LogProfileSys, Verbose, TEXT("MCP-Profile: Dispatching request to %s"), *Url);

	bool bRequestQueued = false;
	check(OnlineSubMcp);
	FGameServiceMcpPtr McpGameService = OnlineSubMcp->GetMcpGameService();
	FString ErrorStr;

	// Queue request based on credentials
	switch (Request.Context)
	{
		case FBaseUrlContext::CXC_Client:
			if (CredentialsAccountId.IsValid())
			{
				TSharedRef<FOnlineHttpRequest> RequestRef = Request.HttpRequest.ToSharedRef();
				OnlineSubMcp->ProcessRequest(*McpGameService, *CredentialsAccountId, RequestRef, ErrorStr);
				bRequestQueued = true;
			}
			else
			{
				ErrorStr = TEXT("Invalid client CredentialsAccountId.");
			}
			break;
#if WITH_SERVER_CODE
		case FBaseUrlContext::CXC_DedicatedServer:
		{
			const FServicePermissionsMcp* ServicePerms = McpGameService->GetMcpConfiguration().GetServicePermissionsByName(FServicePermissionsMcp::DefaultServer);
			if (ServicePerms != NULL)
			{
				TSharedRef<FOnlineHttpRequest> RequestRef = Request.HttpRequest.ToSharedRef();
				OnlineSubMcp->ProcessRequestAsClient(*McpGameService, ServicePerms->Id, RequestRef, ErrorStr);
				bRequestQueued = true;
			}
			else
			{
				ErrorStr = TEXT("Unable to find service permissions for server.");
			}
			break;
		}
#else
		case FBaseUrlContext::CXC_DedicatedServer:
			ErrorStr = TEXT("FATAL: Should not be possible to issue dedicated server commands without WITH_SERVER_CODE");
			break;
#endif // WITH_SERVER_CODE

#if UE_BUILD_SHIPPING
		case FBaseUrlContext::CXC_Cheater:
			ErrorStr = TEXT("FATAL: Should not be possible to issue cheater commands in UE_BUILD_SHIPPING");
			break;
#else
		case FBaseUrlContext::CXC_Cheater:
		{
			const FServicePermissionsMcp* ServicePerms = McpGameService->GetMcpConfiguration().GetServicePermissionsByName(TEXT("Cheater"));
			if (ServicePerms != NULL)
			{
				TSharedRef<FOnlineHttpRequest> RequestRef = Request.HttpRequest.ToSharedRef();
				OnlineSubMcp->ProcessRequestAsClient(*McpGameService, ServicePerms->Id, RequestRef, ErrorStr);
				bRequestQueued = true;
			}
			else
			{
				ErrorStr = TEXT("Unable to find service permissions for cheater.");
			}
			break;
		}
#endif // UE_BUILD_SHIPPING
	default:
		ErrorStr = TEXT("Invalid request context.");
		break;
	}

	if (!ErrorStr.IsEmpty())
	{
		UE_LOG(LogProfileSys, Warning, TEXT("SendRequestNow Error: %s"), *ErrorStr);
	}

	if (!bRequestQueued)
	{
		UE_LOG(LogProfileSys, Warning, TEXT("Failed to start profile request."));
		TSharedRef<FOnlineHttpRequest> RequestRef = Request.HttpRequest.ToSharedRef();
		OnlineSubMcp->CancelRequest(RequestRef);
	}
}

void UMcpProfileGroup::GenericResponse_HttpRequestComplete(TSharedRef<FHttpRetrySystem::FRequest>& InHttpRequest, bool bSucceeded, FString FunctionName, FMcpQueryComplete Callback)
{
	FOnlineHttpRequest& HttpRequest = static_cast<FOnlineHttpRequest&>(InHttpRequest.Get());
	const FHttpResponsePtr HttpResponse = HttpRequest.GetResponse();

	// attempt to parse MCP version from the header
	if (bSucceeded && HttpResponse.IsValid())
	{
		// check about mcpRevision
		FString McpVersionHeader = HttpResponse->GetHeader(TEXT("X-EpicGames-McpVersion"));
		if (McpVersionHeader != LastMcpVersion)
		{
			LastMcpVersion = McpVersionHeader;
			UE_LOG(LogProfileSys, Display, TEXT("MCP-Version = %s"), *McpVersionHeader);
		}

		// log the response code
		const FString LogMessage = FString::Printf(TEXT("MCP-Profile (CL=%s): Received HTTP %d response to %s request."), *McpVersionHeader, (int)HttpResponse->GetResponseCode(), *FunctionName);
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			UE_LOG(LogProfileSys, Verbose, TEXT("%s"), *LogMessage);
		}
		else
		{
			UE_LOG(LogProfileSys, Log, TEXT("%s"), *LogMessage);
		}
	}

	// parse the result
	FMcpQueryResult QueryResult;
	TSharedPtr<FJsonValue> JsonPayload = QueryResult.Parse(HttpResponse);

	// handle the response
	if (QueryResult.bSucceeded)
	{
		if (JsonPayload.IsValid() && JsonPayload->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject>& JsonObject = JsonPayload->AsObject();

			// synchronize with server time
			FDateTime ServerTime;
			if (FDateTime::ParseIso8601(*JsonObject->GetStringField(TEXT("serverTime")), ServerTime))
			{
				// calculate an offset in ticks we can use to synchronize our clocks with MCP (we don't care if MCP is wrong, just that we can sync with it)
				LocalTimeOffset = ServerTime - FDateTime::UtcNow();
			}

			// if we got multi-update response, process those first
			const TArray< TSharedPtr<FJsonValue> >* MultiUpdate = nullptr;
			if (JsonObject->TryGetArrayField(TEXT("multiUpdate"), MultiUpdate))
			{
				for (const TSharedPtr<FJsonValue>& Node : *MultiUpdate)
				{
					TSharedPtr<FJsonObject> JsonNode = Node->AsObject();
					FString AuxProfileId = JsonNode->GetStringField(TEXT("profileId"));
					UMcpProfile* AuxProfile = GetProfile(AuxProfileId);
					if (AuxProfile != nullptr)
					{
						AuxProfile->HandleMcpResponse(JsonNode, QueryResult);
					}
				}
			}

			// what ProfileId does this go to
			FString MainProfileId = JsonObject->GetStringField(TEXT("profileId"));
			UMcpProfile* MainProfile = GetProfile(MainProfileId);
			if (MainProfile != nullptr)
			{
				MainProfile->HandleMcpResponse(JsonObject, QueryResult);
			}
		}
		else
		{
			FString CorrelationId = HttpRequest.GetHeader("X-Epic-Correlation-ID");
			UE_LOG(LogProfileSys, Error, TEXT("McpProfile::%s %s %d Did not return a JSON object"), *FunctionName, !CorrelationId.IsEmpty() ? *CorrelationId : TEXT("0"), QueryResult.HttpResult);
		}
	}
	else
	{
		FString CorrelationId = HttpRequest.GetHeader("X-Epic-Correlation-ID");
		UE_LOG(LogProfileSys, Error, TEXT("McpProfile::%s %s %d %s - %s %s"), *FunctionName, 
			!CorrelationId.IsEmpty() ? *CorrelationId : TEXT("0"), 
			QueryResult.HttpResult, *QueryResult.ErrorCode, 
			*QueryResult.ErrorMessage.ToString(), *QueryResult.ErrorRaw);
	}

	// Call request delegate
	Callback.ExecuteIfBound(QueryResult);
}

void UMcpProfileGroup::QueueRequest(const TSharedRef<class FOnlineHttpRequest>& HttpRequest, FBaseUrlContext::EContextCredentials Context, UMcpProfile* SourceProfile, bool bImmediate)
{
	FProfilePendingHttpRequest PendingRequest;
	PendingRequest.HttpRequest = HttpRequest;
	PendingRequest.Context = Context;
	PendingRequest.SourceProfile = SourceProfile;
	if (PendingRequest.SourceProfile == nullptr || bImmediate)
	{
		// send the request now. No need to serialize requests that don't reference a particular account (these are assumed to be global)
		UE_LOG(LogProfileSys, VeryVerbose, TEXT("Sending immediate request"));
		SendRequestNow(PendingRequest);
	}
	else
	{
		// wrap the callback to call ProcessNextRequest after execution
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &ThisClass::QueueRequest_Completed, HttpRequest->OnProcessRequestComplete());

		// add to the pending queue
		PendingRequests.Add(PendingRequest);
		UE_LOG(LogProfileSys, VeryVerbose, TEXT("Queued request, pending total now *%i*"), PendingRequests.Num());

		// issue a request immediately if this just got added to the queue
		if (PendingRequests.Num() == 1)
		{
			UE_LOG(LogProfileSys, VeryVerbose, TEXT("Processing only request on queue *%i*"), PendingRequests.Num());
			SendRequestNow(PendingRequests[0]);
		}
	}
}

void UMcpProfileGroup::QueueRequest_Completed(TSharedRef<FHttpRetrySystem::FRequest>& InHttpRequest, bool bSucceeded, FOnProcessRequestCompleteDelegate OriginalDelegate)
{
	const FHttpResponsePtr HttpResponse = InHttpRequest->GetResponse();

	// call the original delegate
	OriginalDelegate.ExecuteIfBound(InHttpRequest, bSucceeded);

	// see if there are any more requests to process
	if (PendingRequests.Num() > 0)
	{
		// Remove entry that just called this after finishing
		UE_LOG(LogProfileSys, VeryVerbose, TEXT("Finished processing request, removing first of *%i*"), PendingRequests.Num());
		PendingRequests.RemoveAt(0);
		if (PendingRequests.Num() > 0)
		{
			UE_LOG(LogProfileSys, VeryVerbose, TEXT("Processing next request on queue of *%i*"), PendingRequests.Num());
			SendRequestNow(PendingRequests[0]);
			return;
		}
	}
	UE_LOG(LogProfileSys, VeryVerbose, TEXT("No more requests to process. *%i*"), PendingRequests.Num());
}

void UMcpProfileGroup::QueueRequestGenerator(FMcpGenerateRequestDelegate RequestGenerator, UMcpProfile* SourceProfile)
{
	if (bIsProcessingRequestGenerator)
	{
		UE_LOG(LogProfileSys, Error, TEXT("Unsafe to call UMcpProfileGroup::QueueRequestGenerator from a request generator callback! Request is ignored."));
		return;
	}

	FProfilePendingHttpRequest PendingRequest;
	PendingRequest.GenerateDelegate = RequestGenerator;
	PendingRequest.SourceProfile = SourceProfile;

	// add to the pending queue
	PendingRequests.Add(PendingRequest);
	UE_LOG(LogProfileSys, VeryVerbose, TEXT("Queued request, pending total now *%i*"), PendingRequests.Num());

	// issue a request immediately if this just got added to the queue
	if (PendingRequests.Num() == 1)
	{
		UE_LOG(LogProfileSys, VeryVerbose, TEXT("Processing only request on queue *%i*"), PendingRequests.Num());
		SendRequestNow(PendingRequests[0]);
	}
}

void UMcpProfileGroup::HandleRequestGenerator(const FProfilePendingHttpRequest& PendingRequest)
{
	// If we get here, the first one in the list should be a request generator
	if (!ensure(PendingRequests.Num() > 0 && PendingRequests[0].IsRequestGenerator()))
	{
		return;
	}

	// Cannot have a double nested request
	check(!bIsProcessingRequestGenerator);
	
	// Cache off the current request queue and restore it after the callback
	TArray<FProfilePendingHttpRequest> RemainingPendingRequests = PendingRequests;

	// Remove this request from the remaining list
	RemainingPendingRequests.RemoveAt(0);

	// Remove everything other than 0 from the real list, so we can add to it from nested requests
	PendingRequests.SetNum(1);

	bIsProcessingRequestGenerator = true;

	// This will probably add requests to the queue
	PendingRequest.GenerateDelegate.ExecuteIfBound(PendingRequest.SourceProfile);

	bIsProcessingRequestGenerator = false;

	// Remove entry that just called this after finishing
	UE_LOG(LogProfileSys, VeryVerbose, TEXT("Finished processing request, removing first of *%i*"), PendingRequests.Num());
	PendingRequests.RemoveAt(0);

	// Append the remaining list, so they happen after whatever we just queued
	PendingRequests.Append(RemainingPendingRequests);

	if (PendingRequests.Num() > 0)
	{
		UE_LOG(LogProfileSys, VeryVerbose, TEXT("Processing next request on queue of *%i*"), PendingRequests.Num());
		SendRequestNow(PendingRequests[0]);
		return;
	}
	UE_LOG(LogProfileSys, VeryVerbose, TEXT("No more requests to process. *%i*"), PendingRequests.Num());
}
