// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMatchmakingPolicy.h"
#include "UTGameInstance.h"
#include "OnlineSubsystemUtils.h"
#include "UTSearchPass.h"
#include "UTGameViewportClient.h"

#define LOCTEXT_NAMESPACE "UTMatchmaking"

UUTMatchmakingPolicy::UUTMatchmakingPolicy(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	bMatchmakingInProgress(false),
	MMPass(nullptr)
{
}

void UUTMatchmakingPolicy::Init(const FMatchmakingParams& InParams)
{
	SessionName = GameSessionName;
	CurrentParams = InParams;

	MMPass = NewObject<UUTSearchPass>();
}

FMMAttemptState UUTMatchmakingPolicy::GetMatchmakingState() const
{
	if (MMPass)
	{
		return MMPass->GetSearchResultStatus();
	}

	FMMAttemptState EmptyState;
	return EmptyState;
}

bool UUTMatchmakingPolicy::IsMatchmaking() const
{
	bool bSearchPassIsMatchmaking = false;
	if (MMPass)
	{
		bSearchPassIsMatchmaking = MMPass->IsMatchmaking();
	}

	check(!bSearchPassIsMatchmaking || (bMatchmakingInProgress && bSearchPassIsMatchmaking));
	return bMatchmakingInProgress;
}

void UUTMatchmakingPolicy::StartMatchmaking()
{
	bMatchmakingInProgress = true;
}

void UUTMatchmakingPolicy::CancelMatchmaking()
{
	if (bMatchmakingInProgress)
	{
		if (MMPass && MMPass->IsMatchmaking())
		{
			// This lower call will bubble up a complete event
			MMPass->CancelMatchmaking();
		}
		else
		{
			// Handle the cancel ourselves
			FOnlineSessionSearchResult EmptyResult;
			SignalMatchmakingComplete(EMatchmakingCompleteResult::Cancelled, EmptyResult);
		}
	}
}

void UUTMatchmakingPolicy::SignalMatchmakingComplete(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult)
{
	bMatchmakingInProgress = false;

	//finish matchmaking analytics and upload data
	//EndAnalytics();

	if (MMPass)
	{
		MMPass->ClearMatchmakingDelegates();
		MMPass = nullptr;
	}

	// If the result isn't a hard failure, clear any accumulated failure messages, as they are of no consequence to the end user
	if ((Result != EMatchmakingCompleteResult::Failure) && (Result != EMatchmakingCompleteResult::CreateFailure) && GEngine)
	{
		UUTGameViewportClient* UTGameViewportClient = Cast<UUTGameViewportClient>(GEngine->GameViewport);
		if (UTGameViewportClient)
		{
			//UTGameViewportClient->ClearFailureMessage();
		}
	}

	OnMatchmakingComplete().ExecuteIfBound(Result, SearchResult);
}


UWorld* UUTMatchmakingPolicy::GetWorld() const
{
	check(GEngine);
	check(GEngine->GameViewport);
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	return GameInstance->GetWorld();
}

FTimerManager& UUTMatchmakingPolicy::GetWorldTimerManager() const
{
	return GetWorld()->GetTimerManager();
}

UUTGameInstance* UUTMatchmakingPolicy::GetUTGameInstance() const
{
	check(GEngine);
	check(GEngine->GameViewport);
	return Cast<UUTGameInstance>(GEngine->GameViewport->GetGameInstance());
}

void UUTMatchmakingPolicy::CleanupJoinFailure()
{
	UUTGameInstance* GameInstance = GetUTGameInstance();
	check(GameInstance);

	// Need to cleanup the possibly good session that we aren't joining
	FOnDestroySessionCompleteDelegate CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnJoinFailureCleanupComplete);
	GameInstance->SafeSessionDelete(GameSessionName, CompletionDelegate);
}

void UUTMatchmakingPolicy::OnJoinFailureCleanupComplete(FName InSessionName, bool bWasSuccessful)
{
	FOnlineSessionSearchResult EmptyResult;
	SignalMatchmakingComplete(EMatchmakingCompleteResult::Failure, EmptyResult);
}

#undef LOCTEXT_NAMESPACE