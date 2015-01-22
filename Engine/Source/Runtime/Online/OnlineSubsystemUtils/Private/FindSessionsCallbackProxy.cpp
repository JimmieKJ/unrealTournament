// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "FindSessionsCallbackProxy.h"

//////////////////////////////////////////////////////////////////////////
// UFindSessionsCallbackProxy

UFindSessionsCallbackProxy::UFindSessionsCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Delegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnCompleted))
	, bUseLAN(false)
{
}

UFindSessionsCallbackProxy* UFindSessionsCallbackProxy::FindSessions(UObject* WorldContextObject, class APlayerController* PlayerController, int MaxResults, bool bUseLAN)
{
	UFindSessionsCallbackProxy* Proxy = NewObject<UFindSessionsCallbackProxy>();
	Proxy->PlayerControllerWeakPtr = PlayerController;
	Proxy->bUseLAN = bUseLAN;
	Proxy->MaxResults = MaxResults;
	Proxy->WorldContextObject = WorldContextObject;
	return Proxy;
}

void UFindSessionsCallbackProxy::Activate()
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("FindSessions"), GEngine->GetWorldFromContextObject(WorldContextObject));
	Helper.QueryIDFromPlayerController(PlayerControllerWeakPtr.Get());

	if (Helper.IsValid())
	{
		auto Sessions = Helper.OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->AddOnFindSessionsCompleteDelegate(Delegate);
			
			SearchObject = MakeShareable(new FOnlineSessionSearch);
			SearchObject->MaxSearchResults = MaxResults;
			SearchObject->bIsLanQuery = bUseLAN;
			SearchObject->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

			Sessions->FindSessions(*Helper.UserID, SearchObject.ToSharedRef());

			// OnQueryCompleted will get called, nothing more to do now
			return;
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("Sessions not supported by Online Subsystem"), ELogVerbosity::Warning);
		}
	}

	// Fail immediately
	TArray<FBlueprintSessionResult> Results;
	OnFailure.Broadcast(Results);
}

void UFindSessionsCallbackProxy::OnCompleted(bool bSuccess)
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("FindSessionsCallback"), GEngine->GetWorldFromContextObject(WorldContextObject));
	Helper.QueryIDFromPlayerController(PlayerControllerWeakPtr.Get());

	if (Helper.IsValid())
	{
		auto Sessions = Helper.OnlineSub->GetSessionInterface();
		if (Sessions.IsValid())
		{
			Sessions->ClearOnFindSessionsCompleteDelegate(Delegate);
		}
	}

	TArray<FBlueprintSessionResult> Results;

	if (bSuccess && SearchObject.IsValid())
	{
		// Just log the results for now, will need to add a blueprint-compatible search result struct
		for (auto& Result : SearchObject->SearchResults)
		{
			FString ResultText = FString::Printf(TEXT("Found a session. Ping is %d"), Result.PingInMs);

			FFrame::KismetExecutionMessage(*ResultText, ELogVerbosity::Log);

			FBlueprintSessionResult BPResult;
			BPResult.OnlineResult = Result;
			Results.Add(BPResult);
		}

		OnSuccess.Broadcast(Results);
	}
	else
	{
		OnFailure.Broadcast(Results);
	}
}

int32 UFindSessionsCallbackProxy::GetPingInMs(const FBlueprintSessionResult& Result)
{
	return Result.OnlineResult.PingInMs;
}

FString UFindSessionsCallbackProxy::GetServerName(const FBlueprintSessionResult& Result)
{
	return Result.OnlineResult.Session.OwningUserName;
}

int32 UFindSessionsCallbackProxy::GetCurrentPlayers(const FBlueprintSessionResult& Result)
{
	return Result.OnlineResult.Session.SessionSettings.NumPublicConnections - Result.OnlineResult.Session.NumOpenPublicConnections;
}

int32 UFindSessionsCallbackProxy::GetMaxPlayers(const FBlueprintSessionResult& Result)
{
	return Result.OnlineResult.Session.SessionSettings.NumPublicConnections;
}
