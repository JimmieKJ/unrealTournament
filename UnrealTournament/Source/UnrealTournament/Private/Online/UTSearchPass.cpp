// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTSearchPass.h"
#include "UTGameSession.h"
#include "UTGameInstance.h"
#include "UTSessionHelper.h"
#include "UTOnlineGameSettings.h"
#include "UTMatchmakingPolicy.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineIdentityMcp.h"
#include "Qos.h"
#include "UTPartyBeaconState.h"

#define LOCTEXT_NAMESPACE "UTMatchmaking"

FUTSearchPassParams::FUTSearchPassParams() :
	ControllerId(INVALID_CONTROLLERID),
	SessionName(NAME_None),
	Flags(EMatchmakingFlags::None)
{}

UUTSearchPass::UUTSearchPass(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	SessionHelper(nullptr),
	SearchFilter(nullptr)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		OnFindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete);
		OnCancelSessionsCompleteDelegate = FOnCancelFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnCancelFindSessionsComplete);
	}
}

UWorld* UUTSearchPass::GetWorld() const
{
	check(GEngine);
	check(GEngine->GameViewport);
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	return GameInstance->GetWorld();
}

FTimerManager& UUTSearchPass::GetWorldTimerManager() const
{
	return GetWorld()->GetTimerManager();
}

UUTGameInstance* UUTSearchPass::GetUTGameInstance() const
{
	check(GEngine);
	check(GEngine->GameViewport);
	return Cast<UUTGameInstance>(GEngine->GameViewport->GetGameInstance());
}

void UUTSearchPass::ClearMatchmakingDelegates()
{
	OnMatchmakingStateChange().Unbind();
	OnJoinSessionComplete().Unbind();
	OnNoSearchResultsFound().Unbind();
	OnCancelledSearchComplete().Unbind();
}

FMMAttemptState UUTSearchPass::GetSearchResultStatus() const
{
	FMMAttemptState Status;

	Status.State = CurrentSearchPassState.MatchmakingState;

	if (SearchFilter.IsValid())
	{
		if (SearchFilter->SearchState == EOnlineAsyncTaskState::Done)
		{
			Status.BestSessionIdx = CurrentSearchPassState.BestSessionIdx;
			Status.NumSearchResults = SearchFilter->SearchResults.Num();
			Status.LastBeaconResponse = CurrentSearchPassState.LastBeaconResponse;
		}
	}
	else
	{
		Status.LastBeaconResponse = CurrentSearchPassState.LastBeaconResponse;
	}

	return Status;
}

const FOnlineSessionSearchResult& UUTSearchPass::GetCurrentSearchResult() const
{
	if (SearchFilter.IsValid())
	{
		if (SearchFilter->SearchState == EOnlineAsyncTaskState::Done && SearchFilter->SearchResults.IsValidIndex(CurrentSearchPassState.BestSessionIdx))
		{
			return SearchFilter->SearchResults[CurrentSearchPassState.BestSessionIdx];
		}
	}
	
	static FOnlineSessionSearchResult EmptyResult;
	return EmptyResult;
}

void UUTSearchPass::RequestEmptyServerReservation()
{
	// Trigger matchmaking state change for UI
	OnMatchmakingStateChange().ExecuteIfBound(CurrentSearchPassState.MatchmakingState, CurrentSearchPassState.MatchmakingState);

	UWorld* World = GetWorld();

	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		TSharedPtr<const FUniqueNetId> PartyLeaderId = IdentityInt->GetUniquePlayerId(CurrentSearchParams.ControllerId);
		if (PartyLeaderId.IsValid())
		{
			if (CurrentSearchPassState.MMStats.IsValid())
			{
				CurrentSearchPassState.MMStats->StartJoinAttempt(CurrentSearchPassState.BestSessionIdx);
			}

			SessionHelper = NewObject<UUTSessionHelper>();
			SessionHelper->OnSessionHelperStateChange().BindUObject(this, &ThisClass::SessionHelperStateChanged);

			// Attempt a reservation with the server
			FOnUTReserveSessionComplete CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnSessionReservationRequestComplete);

			const FOnlineSessionSearchResult& SearchResult = GetCurrentSearchResult();
			check(SearchResult.IsValid());

			FEmptyServerReservation ReservationData(SearchFilter->GetPlaylistId());
			if ((CurrentSearchParams.Flags & EMatchmakingFlags::Private) == EMatchmakingFlags::Private)
			{
				ReservationData.bMakePrivate = true;
			}

			SessionHelper->ReserveEmptySession(PartyLeaderId, CurrentSearchParams.SessionName, SearchResult, ReservationData, CompletionDelegate);
		}
		else
		{
			OnSessionReservationRequestComplete(EUTSessionHelperJoinResult::ReservationFailure);
		}
	}
	else
	{
		OnSessionReservationRequestComplete(EUTSessionHelperJoinResult::ReservationFailure);
	}
}

void UUTSearchPass::RequestReservation()
{
	// Trigger matchmaking state change for UI
	OnMatchmakingStateChange().ExecuteIfBound(CurrentSearchPassState.MatchmakingState, CurrentSearchPassState.MatchmakingState);

	UWorld* World = GetWorld();

	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		TSharedPtr<const FUniqueNetId> PartyLeaderId = IdentityInt->GetUniquePlayerId(CurrentSearchParams.ControllerId);
		if (PartyLeaderId.IsValid())
		{
			if (CurrentSearchPassState.MMStats.IsValid())
			{
				CurrentSearchPassState.MMStats->StartJoinAttempt(CurrentSearchPassState.BestSessionIdx);
			}

			SessionHelper = NewObject<UUTSessionHelper>();
			SessionHelper->OnSessionHelperStateChange().BindUObject(this, &ThisClass::SessionHelperStateChanged);

			// Attempt a reservation with the server
			FOnUTReserveSessionComplete CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnSessionReservationRequestComplete);

			const FOnlineSessionSearchResult& SearchResult = GetCurrentSearchResult();
			check(SearchResult.IsValid());

			if ((CurrentSearchParams.Flags & EMatchmakingFlags::NoReservation) == EMatchmakingFlags::NoReservation)
			{
				SessionHelper->SkipReservation(PartyLeaderId, CurrentSearchParams.SessionName, SearchResult, CompletionDelegate);
			}
			else
			{
				SessionHelper->ReserveSession(PartyLeaderId, CurrentSearchParams.SessionName, SearchResult, CompletionDelegate);
			}
		}
		else
		{
			OnSessionReservationRequestComplete(EUTSessionHelperJoinResult::ReservationFailure);
		}
	}
	else
	{
		OnSessionReservationRequestComplete(EUTSessionHelperJoinResult::ReservationFailure);
	}
}

bool UUTSearchPass::IsMatchmaking() const
{
	return (CurrentSearchPassState.MatchmakingState >= EMatchmakingState::AcquiringLock);
}

void UUTSearchPass::OnSessionReservationRequestComplete(EUTSessionHelperJoinResult::Type ReservationResult)
{
	CurrentSearchPassState.LastBeaconResponse = SessionHelper ? SessionHelper->GetLastBeaconResponse() : TEnumAsByte<EPartyReservationResult::Type>(EPartyReservationResult::NoResult);

	if (CurrentSearchPassState.MMStats.IsValid())
	{
		CurrentSearchPassState.MMStats->EndJoinAttempt(CurrentSearchPassState.BestSessionIdx, CurrentSearchPassState.LastBeaconResponse);
	}

	if (IsMatchmaking())
	{
		if (ReservationResult == EUTSessionHelperJoinResult::ReservationSuccess)
		{
			// Change state now to prevent cancellation while we are waiting
			ProcessMatchmakingStateChange(EMatchmakingState::JoiningExistingSession);
			GetWorldTimerManager().SetTimer(JoinReservedSessionTimerHandle, this, &ThisClass::JoinReservedSession, JOIN_RESERVED_SESSION_DELAY, false);
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Session reservation request for index (%d) rejected with %s"), CurrentSearchPassState.BestSessionIdx, EPartyReservationResult::ToString(CurrentSearchPassState.LastBeaconResponse));
			if (CurrentSearchPassState.MatchmakingState == EMatchmakingState::TestingEmptyServers)
			{
				GetWorldTimerManager().SetTimer(ContinueEmptyDedicatedMatchmakingTimerHandle, this, &ThisClass::ContinueTestingEmptyServers, CONTINUE_MATCHMAKING_DELAY, false);
			}
			else if (CurrentSearchPassState.MatchmakingState == EMatchmakingState::TestingExistingSessions)
			{
				GetWorldTimerManager().SetTimer(ContinueMatchmakingTimerHandle, this, &ThisClass::ContinueTestingExistingSessions, CONTINUE_MATCHMAKING_DELAY, false);
			}
			else
			{
				// Failsafe
				UE_LOG(LogOnlineGame, Warning, TEXT("OnSessionReservationRequestComplete: Unexpected state %s"), EMatchmakingState::ToString(CurrentSearchPassState.MatchmakingState));
				ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
			}
		}
	}
	else if (CurrentSearchPassState.LastBeaconResponse != EPartyReservationResult::ReservationRequestCanceled)
	{
		ProcessMatchmakingStateChange(EMatchmakingState::NotMatchmaking);
	}
	else
	{
		// Cancellation has its own path
		UE_LOG(LogOnlineGame, Warning, TEXT("OnSessionReservationRequestComplete: Reservation cancellation in state %s"), EMatchmakingState::ToString(CurrentSearchPassState.MatchmakingState));
	}
}

void UUTSearchPass::OnFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnFindSessionsComplete bSuccess: %d"), bWasSuccessful);
	
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(GetWorld());
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);
	}

	if (IsMatchmaking())
	{
		if (bWasSuccessful)
		{
			if (CurrentSearchPassState.MMStats.IsValid())
			{
				CurrentSearchPassState.MMStats->EndSearchPass(*SearchFilter);
			}

#if !UE_BUILD_SHIPPING
			UE_LOG(LogOnlineGame, Verbose, TEXT("Num Search Results: %d"), SearchFilter->SearchResults.Num());
			for (int32 SearchIdx = 0; SearchIdx < SearchFilter->SearchResults.Num(); SearchIdx++)
			{
				const FOnlineSessionSearchResult& SearchResult = SearchFilter->SearchResults[SearchIdx];
				DumpSession(&SearchResult.Session);
			}
#endif

			if (CurrentSearchPassState.MatchmakingState == EMatchmakingState::FindingEmptyServer)
			{
				StartTestingEmptyServers();
			}
			else if (CurrentSearchPassState.MatchmakingState == EMatchmakingState::FindingExistingSession)
			{
				StartTestingExistingSessions();
			}
		}
		else
		{
			ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
		}
	}
}

void UUTSearchPass::ResetBestSessionVars()
{
	CurrentSearchPassState.BestSessionIdx = -1;
}

void UUTSearchPass::ChooseBestSession()
{
	if (SearchFilter.IsValid())
	{
		// Start searching from where we left off
		for (int32 SessionIndex = CurrentSearchPassState.BestSessionIdx + 1; SessionIndex < SearchFilter->SearchResults.Num(); SessionIndex++)
		{
			if (SearchFilter->SearchResults[SessionIndex].IsValid())
			{
				// Found the match that we want
				CurrentSearchPassState.BestSessionIdx = SessionIndex;
				return;
			}
		}
	}
	CurrentSearchPassState.BestSessionIdx = -1;
}

void UUTSearchPass::StartTestingExistingSessions()
{
	CurrentSearchPassState.LastBeaconResponse = EPartyReservationResult::NoResult;
	ResetBestSessionVars();
	ProcessMatchmakingStateChange(EMatchmakingState::TestingExistingSessions);
	ContinueTestingExistingSessions();
}

void UUTSearchPass::StartTestingEmptyServers()
{
	CurrentSearchPassState.LastBeaconResponse = EPartyReservationResult::NoResult;
	ResetBestSessionVars();
	ProcessMatchmakingStateChange(EMatchmakingState::TestingEmptyServers);
	ContinueTestingEmptyServers();
}

void UUTSearchPass::ContinueTestingExistingSessions()
{	
	ChooseBestSession();
	if (IsMatchmaking())
	{
		if (SearchFilter.IsValid() &&
			SearchFilter->SearchResults.IsValidIndex(CurrentSearchPassState.BestSessionIdx))
		{
			RequestReservation();
		}
		else
		{
			ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
		}
	}
}

void UUTSearchPass::ContinueTestingEmptyServers()
{	
	ChooseBestSession();
	if (IsMatchmaking())
	{
		if (SearchFilter.IsValid() &&
			SearchFilter->SearchResults.IsValidIndex(CurrentSearchPassState.BestSessionIdx))
		{
			RequestEmptyServerReservation();
		}
		else
		{
			ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
		}
	}
}

void UUTSearchPass::JoinReservedSession()
{
	FOnUTJoinSessionComplete CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::OnJoinSessionCompleteInternal);

	// State change occurred at timer setup for this call
	if (SessionHelper)
	{
		if (SearchFilter.IsValid() && SearchFilter->SearchResults.IsValidIndex(CurrentSearchPassState.BestSessionIdx))
		{
			SessionHelper->JoinReservedSession(CompletionDelegate);
		}
		else
		{
			int32 NumSearchResults = SearchFilter.IsValid() ? SearchFilter->SearchResults.Num() : -1;
			UE_LOG(LogOnlineGame, Warning, TEXT("JoinReservedSession: Invalid session index %d, num results: %d"), CurrentSearchPassState.BestSessionIdx, NumSearchResults);
			CompletionDelegate.ExecuteIfBound(EUTSessionHelperJoinResult::JoinSessionFailure);
		}
	}
	else
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("JoinReservedSession: No session helper"));
		CompletionDelegate.ExecuteIfBound(EUTSessionHelperJoinResult::JoinSessionFailure);
	}
}

void UUTSearchPass::ProcessNoMatchesAvailable()
{
	// Give this delegate a chance to unwind
	FTimerDelegate NoMatchesTimer;

	NoMatchesTimer.BindUObject(this, &ThisClass::OnNoMatchesAvailableComplete);
	GetWorldTimerManager().SetTimer(OnNoMatchesAvailableCompleteTimerHandle, NoMatchesTimer, NO_MATCHES_AVAILABLE_DELAY, false);
}

void UUTSearchPass::OnNoMatchesAvailableComplete()
{
	ProcessMatchmakingStateChange(EMatchmakingState::NotMatchmaking);
	OnNoSearchResultsFound().ExecuteIfBound();
}

void UUTSearchPass::FindSession(FUTSearchPassParams& InSearchParams, const TSharedRef<FUTOnlineSessionSearchBase>& InSearchFilter)
{
	if (CurrentSearchPassState.MMStats.IsValid())
	{
		CurrentSearchPassState.MMStats->StartSearchAttempt(EMatchmakingSearchType::ExistingSession);
	}

	ProcessMatchmakingStateChange(EMatchmakingState::FindingExistingSession);
	FindInternal(InSearchParams, InSearchFilter);
}

void UUTSearchPass::FindEmptySession(FUTSearchPassParams& InSearchParams, const TSharedRef<FUTOnlineSessionSearchBase>& InSearchFilter)
{
	if (CurrentSearchPassState.MMStats.IsValid())
	{
		CurrentSearchPassState.MMStats->StartSearchAttempt(EMatchmakingSearchType::EmptyServer);
	}
	
	ProcessMatchmakingStateChange(EMatchmakingState::FindingEmptyServer);
	ContinueFindEmptySession(InSearchParams, InSearchFilter);
}

void UUTSearchPass::ContinueFindEmptySession(const FUTSearchPassParams InSearchParams, const TSharedRef<FUTOnlineSessionSearchBase> InSearchFilter)
{
	if (IsMatchmaking())
	{
		ProcessMatchmakingStateChange(EMatchmakingState::FindingEmptyServer);
		FindInternal(InSearchParams, InSearchFilter);
	}
	else
	{
		ProcessMatchmakingStateChange(EMatchmakingState::NotMatchmaking);
	}
}

void UUTSearchPass::FindInternal(const FUTSearchPassParams& InSearchParams, const TSharedRef<FUTOnlineSessionSearchBase>& InSearchFilter)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	if (OnlineSub)
	{
		CurrentSearchParams = InSearchParams;

		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			if (!InSearchParams.BestDatacenterId.IsEmpty())
			{
				InSearchFilter->QuerySettings.Set(SETTING_REGION, InSearchParams.BestDatacenterId, EOnlineComparisonOp::Equals);
			}

			SearchFilter = InSearchFilter;
			OnFindSessionsCompleteDelegateHandle = SessionInt->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegate);
			SessionInt->FindSessions(InSearchParams.ControllerId, InSearchFilter);
		}
		else
		{
			ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
		}
	}
	else
	{
		ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
	}
}

void UUTSearchPass::CancelMatchmaking()
{
	if (IsMatchmaking() &&
		CurrentSearchPassState.MatchmakingState != EMatchmakingState::HandlingFailure &&
		CurrentSearchPassState.MatchmakingState != EMatchmakingState::JoiningExistingSession &&
		CurrentSearchPassState.MatchmakingState != EMatchmakingState::JoinSuccess)
	{
		ProcessMatchmakingStateChange(EMatchmakingState::CancelingMatchmaking);
	}
}

void UUTSearchPass::OnJoinSessionCompleteInternal(EUTSessionHelperJoinResult::Type JoinResult)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnJoinSessionCompleteInternal Result: %s"), EUTSessionHelperJoinResult::ToString(JoinResult));

	if (JoinResult == EUTSessionHelperJoinResult::JoinSessionSuccess)
	{
		ProcessMatchmakingStateChange(EMatchmakingState::JoinSuccess);
	}
	else
	{
		ProcessMatchmakingStateChange(EMatchmakingState::HandlingFailure);
		ProcessMatchmakingStateChange(EMatchmakingState::NotMatchmaking);
	}
}

void UUTSearchPass::ProcessCancelMatchmaking()
{
	ResetBestSessionVars();

	FTimerManager& TM = GetWorldTimerManager();
	TM.ClearTimer(RequestEmptyServerReservationTimerHandle);
	TM.ClearTimer(RequestReservationTimerHandle);
	TM.ClearTimer(ContinueEmptyDedicatedMatchmakingTimerHandle);
	TM.ClearTimer(ContinueMatchmakingTimerHandle);

	if (SessionHelper)
	{
		// Tell the host to remove our reservation
		SessionHelper->CancelReservation();
		SessionHelper->Reset();
		SessionHelper = nullptr;
	}

	CurrentSearchPassState.bWasCanceled = true;

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(GetWorld());
	if (SessionInt.IsValid() && SearchFilter.IsValid() &&
		SearchFilter->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		OnCancelSessionsCompleteDelegateHandle = SessionInt->AddOnCancelFindSessionsCompleteDelegate_Handle(OnCancelSessionsCompleteDelegate);
		SessionInt->CancelFindSessions();
	}
	else
	{
		// Give this delegate a chance to unwind
		FTimerDelegate CancelTimer;
		CancelTimer.BindUObject(this, &ThisClass::FinishCancelMatchmaking);
		TM.SetTimer(FinishCancelMatchmakingTimerHandle, CancelTimer, 0.1f, false);
	}
}

void UUTSearchPass::OnCancelFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(GetWorld());
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegateHandle);
		SessionInt->ClearOnCancelFindSessionsCompleteDelegate_Handle(OnCancelSessionsCompleteDelegateHandle);
	}

	// Give this delegate a chance to unwind
	FTimerDelegate FinishCancelTimer;
	FinishCancelTimer.BindUObject(this, &ThisClass::FinishCancelMatchmaking);
	GetWorldTimerManager().SetTimerForNextTick(FinishCancelTimer);
}

void UUTSearchPass::FinishCancelMatchmaking()
{
	// Give this delegate a chance to unwind
	FTimerDelegate CancelTimer;
	CancelTimer.BindUObject(this, &ThisClass::OnCancelMatchmakingComplete);
	GetWorldTimerManager().SetTimer(OnCancelMatchmakingCompleteTimerHandle, CancelTimer, CANCEL_MATCHMAKING_DELAY, false);
}

void UUTSearchPass::OnCancelMatchmakingComplete()
{
	ProcessMatchmakingStateChange(EMatchmakingState::NotMatchmaking);
}

void UUTSearchPass::ProcessNotMatchmaking()
{
	if (CurrentSearchPassState.bWasFailure)
	{
		OnSearchFailure().ExecuteIfBound();
		CurrentSearchPassState.bWasFailure = false;
	}
	else if (CurrentSearchPassState.bWasCanceled)
	{
		// Trigger the cancel delegate at the very end
		OnCancelledSearchComplete().ExecuteIfBound();
		CurrentSearchPassState.bWasCanceled = false;
	}

	if (SessionHelper)
	{
		SessionHelper->Reset();
		SessionHelper = nullptr;
	}

	SearchFilter = nullptr;
}

void UUTSearchPass::ProcessJoinSuccess()
{
	JoinSessionComplete.Execute(CurrentSearchParams.SessionName, true);
}

void UUTSearchPass::SessionHelperStateChanged(EUTSessionHelperJoinState::Type OldState, EUTSessionHelperJoinState::Type NewState)
{
	UE_LOG(LogOnlineGame, Display, TEXT("Helper state change %s -> %s"), EUTSessionHelperJoinState::ToString(OldState), EUTSessionHelperJoinState::ToString(NewState));
}

void UUTSearchPass::ProcessMatchmakingStateChange(EMatchmakingState::Type NewState)
{
	if (NewState != CurrentSearchPassState.MatchmakingState)
	{
		UE_LOG(LogOnlineGame, Display, TEXT("Matchmaking state change %s -> %s"), EMatchmakingState::ToString(CurrentSearchPassState.MatchmakingState), EMatchmakingState::ToString(NewState));
		check(CurrentSearchPassState.MatchmakingState != EMatchmakingState::CancelingMatchmaking ||
			 (CurrentSearchPassState.MatchmakingState == EMatchmakingState::CancelingMatchmaking && (NewState == EMatchmakingState::NotMatchmaking || NewState == EMatchmakingState::ReleasingLock)) ||
			 (CurrentSearchPassState.MatchmakingState == EMatchmakingState::ReleasingLock && NewState == EMatchmakingState::NotMatchmaking));

		EMatchmakingState::Type OldState = CurrentSearchPassState.MatchmakingState;
		CurrentSearchPassState.MatchmakingState = NewState;

		if (CurrentSearchPassState.MMStats.IsValid())
		{
			switch (NewState)
			{
			case EMatchmakingState::FindingEmptyServer:
			case EMatchmakingState::FindingExistingSession:
				CurrentSearchPassState.MMStats->StartSearchPass();
				break;
			case EMatchmakingState::CancelingMatchmaking:
				CurrentSearchPassState.MMStats->EndMatchmakingAttempt(EMatchmakingAttempt::LocalCancel);
				break;
			case EMatchmakingState::HandlingFailure:
				CurrentSearchPassState.MMStats->EndMatchmakingAttempt(EMatchmakingAttempt::EndedInError);
				break;
			case EMatchmakingState::NotMatchmaking:
				CurrentSearchPassState.MMStats->EndMatchmakingAttempt(EMatchmakingAttempt::EndedNoResults);
				break;
			case EMatchmakingState::JoinSuccess:
				CurrentSearchPassState.MMStats->EndMatchmakingAttempt(EMatchmakingAttempt::SuccessJoin);
				break;
			default:
				break;
			}
		}

		switch (NewState)
		{
		case EMatchmakingState::JoinSuccess:
			ProcessJoinSuccess();
			break;
		case EMatchmakingState::CancelingMatchmaking:
			ProcessCancelMatchmaking();
			break;
		case EMatchmakingState::HandlingFailure:
			CurrentSearchPassState.bWasFailure = true;
			break;
		case EMatchmakingState::NoMatchesAvailable:
			ProcessNoMatchesAvailable();
			break;
		case EMatchmakingState::NotMatchmaking:
			ProcessNotMatchmaking();
			break;
		default:
			break;
		}

		OnMatchmakingStateChange().ExecuteIfBound(OldState, NewState);
	}
}

void UUTSearchPass::SetMMStatsSharePtr(TSharedPtr<FUTMatchmakingStats> MMStatsSharePtr)
{
	CurrentSearchPassState.MMStats = MMStatsSharePtr;
}

void UUTSearchPass::ReleaseMMStatsSharePtr()
{
	CurrentSearchPassState.MMStats = nullptr;
}

#undef LOCTEXT_NAMESPACE