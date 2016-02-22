// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMatchmakingGather.h"
#include "UTOnlineGameSettings.h"
#include "UTPartyBeaconState.h"
#include "UTGameInstance.h"

#define LOCTEXT_NAMESPACE "UTMatchmaking"

UUTMatchmakingGather::UUTMatchmakingGather(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
}

void UUTMatchmakingGather::StartMatchmaking()
{
	Super::StartMatchmaking();
	if (CurrentParams.StartWith == EMatchmakingStartLocation::CreateNew)
	{
		StartGatheringSession();
	}
	else
	{
		FindGatheringSession();
	}
}

void UUTMatchmakingGather::RestartMatchmaking()
{
	check(!GetWorldTimerManager().IsTimerActive(FindGatherTimerHandle));

	FTimerDelegate TimerDelegate;
	if ((CurrentParams.Flags & EMatchmakingFlags::CreateNewOnly) == EMatchmakingFlags::CreateNewOnly)
	{
		TimerDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::StartGatheringSession);
	}
	else
	{
		TimerDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::FindGatheringSession);
	}

	GetWorldTimerManager().SetTimer(FindGatherTimerHandle, TimerDelegate, FIND_GATHERING_RESTART_DELAY, false);
}

void UUTMatchmakingGather::CancelMatchmaking()
{
	GetWorldTimerManager().ClearTimer(StartGatherTimerHandle);
	GetWorldTimerManager().ClearTimer(FindGatherTimerHandle);

	Super::CancelMatchmaking();
}

void UUTMatchmakingGather::FindGatheringSession()
{
	UE_LOG(LogOnlineGame, Log, TEXT("FindGatheringSession"));

	bool bResult = false;
	if (CurrentParams.ControllerId != INVALID_CONTROLLERID)
	{
		MMPass->OnMatchmakingStateChange() = OnMatchmakingStateChange();
		MMPass->OnSearchFailure().BindUObject(this, &ThisClass::OnSearchFailure);
		MMPass->OnJoinSessionComplete().BindUObject(this, &ThisClass::OnJoinGatheringSessionComplete);
		MMPass->OnNoSearchResultsFound().BindUObject(this, &ThisClass::OnNoGatheringSessionsFound);
		MMPass->OnCancelledSearchComplete().BindUObject(this, &ThisClass::OnGatheringSearchCancelled);

		TSharedPtr<FUTOnlineSessionSearchBase> NewSearchSettings = MakeShareable(new FUTOnlineSessionSearchGather(CurrentParams.PlaylistId, false, false));
		
		// Order here is important (prefer level match over "closer to full" match for now)
		NewSearchSettings->QuerySettings.Set(SETTING_NEEDS, CurrentParams.PartySize, EOnlineComparisonOp::GreaterThanEquals);
		NewSearchSettings->QuerySettings.Set(SETTING_NEEDSSORT, SORT_ASC, EOnlineComparisonOp::Near);
		
		FUTSearchPassParams SearchPassParams;
		SearchPassParams.ControllerId = CurrentParams.ControllerId;
		SearchPassParams.SessionName = SessionName;
		SearchPassParams.BestDatacenterId = CurrentParams.DatacenterId;
		SearchPassParams.Flags = CurrentParams.Flags;
		MMPass->FindSession(SearchPassParams, NewSearchSettings.ToSharedRef());
		bResult = true;
	}

	if (!bResult)
	{
		OnNoGatheringSessionsFound();
	}
}

void UUTMatchmakingGather::StartGatheringSession()
{
	UE_LOG(LogOnlineGame, Log, TEXT("StartGatheringSession"));
	bool bResult = false;

	FEmptyServerReservation ReservationData(CurrentParams.PlaylistId);

	if (ReservationData.IsValid())
	{
		if (CurrentParams.ControllerId != INVALID_CONTROLLERID)
		{
			MMPass->OnMatchmakingStateChange() = OnMatchmakingStateChange();
			MMPass->OnSearchFailure().BindUObject(this, &ThisClass::OnSearchFailure);
			MMPass->OnJoinSessionComplete().BindUObject(this, &ThisClass::OnJoinEmptyGatheringServer);
			MMPass->OnNoSearchResultsFound().BindUObject(this, &ThisClass::OnNoEmptyGatheringServersFound);
			MMPass->OnCancelledSearchComplete().BindUObject(this, &ThisClass::OnGatheringSearchCancelled);

			TSharedPtr<FUTOnlineSessionSearchBase> NewSearchSettings = MakeShareable(new FUTOnlineSessionSearchEmptyDedicated(ReservationData, false, false));

			FUTSearchPassParams SearchPassParams;
			SearchPassParams.ControllerId = CurrentParams.ControllerId;
			SearchPassParams.SessionName = SessionName;
			SearchPassParams.BestDatacenterId = CurrentParams.DatacenterId;
			SearchPassParams.Flags = CurrentParams.Flags;
			MMPass->FindEmptySession(SearchPassParams, NewSearchSettings.ToSharedRef());
			bResult = true;
		}
	}

	if (!bResult)
	{
		OnNoEmptyGatheringServersFound();
	}
}

void UUTMatchmakingGather::OnJoinGatheringSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnlineGame, Log, TEXT("OnJoinGatheringSessionComplete %s %d"), *SessionName.ToString(), bWasSuccessful);

	UUTGameInstance* GameInstance = GetUTGameInstance();
	check(GameInstance);

	bool bWillTravel = false;
	if (bWasSuccessful)
	{
		OnMatchmakingSuccess();
		bWillTravel = true;
	}

	if (!bWillTravel)
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("Failed to join session %s"), *SessionName.ToString());
		// cleanup session created by search pass
		CleanupJoinFailure();
	}
}

void UUTMatchmakingGather::OnNoGatheringSessionsFound()
{
	UE_LOG(LogOnline, Log, TEXT("OnNoGatheringSessionsFound"));

	check(MMPass);
	MMPass->ClearMatchmakingDelegates();

	const float ChanceToHost = GetChanceToHost();

	float Choice = FMath::FRand() * 100.0f;
	if (Choice <= ChanceToHost)
	{
		// Host a gathering session
		FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::StartGatheringSession);
		GetWorldTimerManager().SetTimer(StartGatherTimerHandle, TimerDelegate, START_GATHERING_DELAY, false);
	}
	else
	{
		// Go back to the top and search again
		RestartMatchmaking();
	}
}

void UUTMatchmakingGather::OnJoinEmptyGatheringServer(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnlineGame, Log, TEXT("OnJoinEmptyGatheringServer %s %d"), *SessionName.ToString(), bWasSuccessful);

	UUTGameInstance* GameInstance = GetUTGameInstance();
	check(GameInstance);

	bool bWillTravel = false;
	if (bWasSuccessful)
	{
		OnMatchmakingSuccess();
		bWillTravel = true;
	}

	if (!bWillTravel)
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("Failed to join session %s"), *SessionName.ToString());
		// cleanup session created by search pass
		CleanupJoinFailure();
	}
}

void UUTMatchmakingGather::OnNoEmptyGatheringServersFound()
{
	UE_LOG(LogOnline, Log, TEXT("OnNoEmptyGatheringServersFound"));

	// Go back to the top
	RestartMatchmaking();
}

void UUTMatchmakingGather::OnGatheringSearchCancelled()
{
	UE_LOG(LogOnline, Log, TEXT("OnGatheringSearchCancelled"));

	check(MMPass);

	FOnlineSessionSearchResult EmptyResult;
	SignalMatchmakingComplete(EMatchmakingCompleteResult::Cancelled, EmptyResult);
}

void UUTMatchmakingGather::OnSearchFailure()
{
	UE_LOG(LogOnline, Log, TEXT("OnSearchFailure"));
	CleanupJoinFailure();
}

void UUTMatchmakingGather::OnMatchmakingSuccess()
{
	UE_LOG(LogOnline, Log, TEXT("OnMatchmakingSuccess"));

	check(MMPass);
	FOnlineSessionSearchResult SearchResult = MMPass->GetCurrentSearchResult();
	check(SearchResult.IsValid());

	SignalMatchmakingComplete(EMatchmakingCompleteResult::Success, SearchResult);
}

float UUTMatchmakingGather::GetChanceToHost() const
{
	const float DefaultHostChance = 40.f;

	float ChanceToHost = DefaultHostChance;

	if (CurrentParams.StartWith == EMatchmakingStartLocation::CreateNew)
	{
		ChanceToHost = 100.0f;
	}
	else if (CurrentParams.ChanceToHostOverride > 0.f)
	{
		ChanceToHost = CurrentParams.ChanceToHostOverride;
	}

	return ChanceToHost;
}

#undef LOCTEXT_NAMESPACE