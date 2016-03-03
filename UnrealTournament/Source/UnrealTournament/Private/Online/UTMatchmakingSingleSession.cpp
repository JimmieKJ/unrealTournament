// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMatchmakingSingleSession.h"
#include "UTGameInstance.h"
#include "UTOnlineGameSettingsBase.h"
#include "OnlineSubsystemUtils.h"

#define LOCTEXT_NAMESPACE "UTMatchmaking"

UUTMatchmakingSingleSession::UUTMatchmakingSingleSession(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	SessionHelper(nullptr)
{
}

void UUTMatchmakingSingleSession::Init(const FMatchmakingParams& InParams)
{
	SessionName = GameSessionName;
	CurrentParams = InParams;
}

void UUTMatchmakingSingleSession::StartMatchmaking()
{
	if (!CurrentParams.SessionId.IsEmpty())
	{
		Super::StartMatchmaking();
		//CurrentSessionParams.MMStats = MMStats;
		
		JoinSessionById(GameSessionName, FUniqueNetIdString(CurrentParams.SessionId));
	}
}

void UUTMatchmakingSingleSession::CancelMatchmaking()
{
	// can't cancel for now
}

bool UUTMatchmakingSingleSession::IsMatchmaking() const
{
	return CurrentSessionParams.State >= EMatchmakingState::AcquiringLock;
}

void UUTMatchmakingSingleSession::JoinSessionById(FName SessionName, const FUniqueNetId& SessionId)
{
	if (!IsMatchmaking())
	{
		bool bSuccess = false;

		if (SessionId.IsValid())
		{
			UUTGameInstance* UTGameInstance = GetUTGameInstance();
			check(UTGameInstance);

			// check not already in this session
			if (!UTGameInstance->IsInSession(SessionId))
			{
				UWorld* World = GetWorld();
				check(World);

				IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
				if (SessionInt.IsValid())
				{
					IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
					if (IdentityInt.IsValid())
					{
						TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInt->GetUniquePlayerId(CurrentParams.ControllerId);
						if (LocalUserId.IsValid())
						{
							// switch to finding session state
							ProcessMatchmakingStateChange(EMatchmakingState::FindingExistingSession);

							FOnSingleSessionResultCompleteDelegate CompletionDelegate;
							CompletionDelegate.BindUObject(this, &ThisClass::OnFindSessionByIdComplete, SessionName);
							SessionInt->FindSessionById(*LocalUserId, SessionId, SessionId, CompletionDelegate);
							bSuccess = true;
						}
						else
						{
							CurrentSessionParams.FailureReason = LOCTEXT("NoUniqueId", "Unknown player accepting invite!");
						}
					}
					else
					{
						CurrentSessionParams.FailureReason = LOCTEXT("NoIdentityInterface", "No identity interface!");
					}
				}
				else
				{
					CurrentSessionParams.FailureReason = LOCTEXT("NoSessionInterface", "No session interface!");
				}
			}
			else
			{
				CurrentSessionParams.FailureReason = LOCTEXT("AlreadyInSession", "Already in this session!");
			}
		}
		else
		{
			CurrentSessionParams.FailureReason = LOCTEXT("InvalidSession", "Invalid Session!");
		}

		if (!bSuccess)
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("JoinSessionFailure: %s"), *CurrentSessionParams.FailureReason.ToString());
			ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
		}
	}
	else
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("Join in progress!"));
	}
}

void UUTMatchmakingSingleSession::OnFindSessionByIdComplete(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& SearchResult, FName SessionName)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnFindSessionByIdComplete LocalUserNum=%d bSuccess: %d"), LocalUserNum, bWasSuccessful);

	if (bWasSuccessful &&
		SearchResult.IsValid())
	{
		JoinSessionInternal(SessionName, SearchResult);
	}
	else
	{
		// Find session failure
		CurrentSessionParams.FailureReason = NSLOCTEXT("FortJoinPresence", "FindSessionFailure", "Session has expired.");
		ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
	}
}

void UUTMatchmakingSingleSession::JoinSessionInternal(FName SessionName, const FOnlineSessionSearchResult& SearchResult)
{
	bool bSuccess = false;
	if (SearchResult.IsValid())
	{
		UWorld* World = GetWorld();
		check(World);

		IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
		if (IdentityInt.IsValid())
		{
			TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInt->GetUniquePlayerId(CurrentParams.ControllerId);
			if (LocalUserId.IsValid())
			{
				int32 IsRanked = 0;
				if (!SearchResult.Session.SessionSettings.Get(SETTING_RANKED, IsRanked) || !IsRanked)
				{
					ProcessMatchmakingStateChange(EMatchmakingState::NotMatchmaking);
					bSuccess = true;

					UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(GetWorld(), CurrentParams.ControllerId));
					if (LP)
					{
						LP->JoinSession(SearchResult, false);
					}
				}
				else
				{
					ProcessMatchmakingStateChange(EMatchmakingState::TestingExistingSessions);

					SessionHelper = NewObject<UUTSessionHelper>();

					// Start client beacon reservation process
					FOnUTReserveSessionComplete CompletionDelegate;
					CompletionDelegate.BindUObject(this, &ThisClass::OnReserveSessionComplete);

					if ((CurrentParams.Flags & EMatchmakingFlags::NoReservation) == EMatchmakingFlags::NoReservation)
					{
						SessionHelper->SkipReservation(LocalUserId, SessionName, SearchResult, CompletionDelegate);
					}
					else
					{
						SessionHelper->ReserveSession(LocalUserId, SessionName, SearchResult, CompletionDelegate);
					}
					bSuccess = true;
				}
			}
		}
	}

	if (!bSuccess)
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("Invalid session result"));
		CurrentSessionParams.FailureReason = NSLOCTEXT("FortJoinPresence", "InvalidSearchResult", "Invalid Session!");
		ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
	}
}

void UUTMatchmakingSingleSession::OnReserveSessionComplete(EUTSessionHelperJoinResult::Type ReserveResult)
{
	CurrentSessionParams.LastBeaconResponse = SessionHelper ? SessionHelper->GetLastBeaconResponse() : TEnumAsByte<EPartyReservationResult::Type>(EPartyReservationResult::GeneralError);

	if (ReserveResult == EUTSessionHelperJoinResult::ReservationSuccess)
	{
		CleanupExistingSessionsAndContinue();
	}
	else
	{
		// emit some message to the user (but no need to cleanup, any existing game is still salvageable)
		UE_LOG(LogOnlineGame, Verbose, TEXT("Reservation failure %s"), EUTSessionHelperJoinResult::ToString(ReserveResult));

		CurrentSessionParams.FailureReason = EPartyReservationResult::GetDisplayString(SessionHelper->GetLastBeaconResponse());
		ProcessMatchmakingStateChange(EMatchmakingState::NoMatchesAvailable);
	}
}

void UUTMatchmakingSingleSession::CleanupExistingSessionsAndContinue()
{
	ProcessMatchmakingStateChange(EMatchmakingState::CleaningUpExisting);

	UUTGameInstance* UTGameInstance = GetUTGameInstance();
	check(UTGameInstance);

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(GetWorld());
	if (SessionInt.IsValid())
	{
		FOnlineSessionSettings* SessionSettings = SessionInt->GetSessionSettings(GameSessionName);
		if (SessionSettings)
		{
			// Cleanup the existing game session before proceeding
			FOnDestroySessionCompleteDelegate CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnCleanupForJoinComplete);
			UTGameInstance->SafeSessionDelete(GameSessionName, CompletionDelegate);
		}
		else
		{
			// Proceed directly to joining the session
			OnCleanupForJoinComplete(GameSessionName, true);
		}
	}
	else
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("No session interface!"));
		OnCleanupForJoinComplete(GameSessionName, false);
	}
}

void UUTMatchmakingSingleSession::OnCleanupForJoinComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName == GameSessionName)
	{
		if (bWasSuccessful)
		{
			ProcessMatchmakingStateChange(EMatchmakingState::JoiningExistingSession);

			FOnUTJoinSessionComplete CompletionDelegate;
			CompletionDelegate.BindUObject(this, &UUTMatchmakingSingleSession::OnJoinReservedSessionComplete);
			SessionHelper->JoinReservedSession(CompletionDelegate);
		}
		else
		{
			// @TODO emit some message to the user
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cleanup failure"));
			ProcessMatchmakingStateChange(EMatchmakingState::HandlingFailure);
		}
	}
}

void UUTMatchmakingSingleSession::OnJoinReservedSessionComplete(EUTSessionHelperJoinResult::Type JoinResult)
{
	if (JoinResult == EUTSessionHelperJoinResult::JoinSessionSuccess)
	{
		ProcessMatchmakingStateChange(EMatchmakingState::JoinSuccess);
	}
	else
	{
		// @TODO emit some message to the user
		UE_LOG(LogOnlineGame, Verbose, TEXT("Join session failure"));
		ProcessMatchmakingStateChange(EMatchmakingState::HandlingFailure);
	}
}

void UUTMatchmakingSingleSession::ProcessMatchmakingStateChange(EMatchmakingState::Type NewState)
{
	if (NewState != CurrentSessionParams.State)
	{
		UE_LOG(LogOnlineGame, Display, TEXT("Matchmaking state change %s -> %s"), EMatchmakingState::ToString(CurrentSessionParams.State), EMatchmakingState::ToString(NewState));

		EMatchmakingState::Type OldState = CurrentSessionParams.State;
		CurrentSessionParams.State = NewState;

		if ((OldState == EMatchmakingState::JoinSuccess) ||
			(OldState == EMatchmakingState::HandlingFailure))
		{
			UE_LOG(LogOnlineGame, Error, TEXT("WEIRD STATE TRANSITION!"));
		}
		
		switch (NewState)
		{
		case EMatchmakingState::NotMatchmaking:
			break;
		case EMatchmakingState::FindingExistingSession:
			break;
		case EMatchmakingState::TestingExistingSessions:
			break;
		case EMatchmakingState::CleaningUpExisting:
			break;
		case EMatchmakingState::JoiningExistingSession:
			break;
		case EMatchmakingState::NoMatchesAvailable:
			ProcessNoMatchesAvailable();
			break;
		case EMatchmakingState::JoinSuccess:
			ProcessJoinSuccess();
			break;
		case EMatchmakingState::HandlingFailure:
			ProcessFailure();
			break;
		default:
			break;
		}

		OnMatchmakingStateChange().ExecuteIfBound(OldState, NewState);
	}
}

void UUTMatchmakingSingleSession::ProcessJoinSuccess()
{
	check(SessionHelper);

	FOnlineSessionSearchResult SearchResult = SessionHelper->GetSearchResult();
	check(SearchResult.IsValid());

	SessionHelper->Reset();
	SessionHelper = nullptr;

	SignalMatchmakingComplete(EMatchmakingCompleteResult::Success, SearchResult);
}

void UUTMatchmakingSingleSession::ProcessFailure()
{
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface();
	FOnlineSessionSettings* SessionSettings = SessionInt->GetSessionSettings(GameSessionName);
	if (SessionSettings)
	{
		UUTGameInstance* UTGameInstance = GetUTGameInstance();
		check(UTGameInstance);

		// Cleanup the existing game session before proceeding
		FOnDestroySessionCompleteDelegate CompletionDelegate;
		CompletionDelegate.BindUObject(this, &ThisClass::OnCleanupAfterFailureComplete);
		UTGameInstance->SafeSessionDelete(GameSessionName, CompletionDelegate);
	}
	else
	{
		// Failsafe case when there was no "new game" session created
		OnCleanupAfterFailureComplete(GameSessionName, true);
	}
}

void UUTMatchmakingSingleSession::OnCleanupAfterFailureComplete(FName SessionName, bool bWasSuccesful)
{
	// Give this delegate a chance to unwind
	FTimerDelegate CleanupFailureTimer;
	CleanupFailureTimer.BindUObject(this, &ThisClass::OnCleanupAfterFailureCompleteContinued);
	FTimerHandle OnCleanupAfterFailureCompleteContinuedTimerHandle;
	GetWorldTimerManager().SetTimer(OnCleanupAfterFailureCompleteContinuedTimerHandle, CleanupFailureTimer, 0.1f, false);
}

void UUTMatchmakingSingleSession::OnCleanupAfterFailureCompleteContinued()
{
	ProcessMatchmakingStateChange(EMatchmakingState::NotMatchmaking);

	if (SessionHelper)
	{
		SessionHelper->Reset();
		SessionHelper = nullptr;
	}

	FOnlineSessionSearchResult EmptyResult;
	SignalMatchmakingComplete(EMatchmakingCompleteResult::Failure, EmptyResult);
}

void UUTMatchmakingSingleSession::ProcessNoMatchesAvailable()
{
	// Give this delegate a chance to unwind
	FTimerDelegate NoSessionTimer;
	NoSessionTimer.BindUObject(this, &ThisClass::OnProcessNoMatchesAvailableComplete);
	GetWorldTimerManager().SetTimerForNextTick(NoSessionTimer);
}

void UUTMatchmakingSingleSession::OnProcessNoMatchesAvailableComplete()
{
	ProcessMatchmakingStateChange(EMatchmakingState::NotMatchmaking);
	
	if (SessionHelper)
	{
		SessionHelper->Reset();
		SessionHelper = nullptr;
	}

	FOnlineSessionSearchResult EmptyResult;
	SignalMatchmakingComplete(EMatchmakingCompleteResult::NoResults, EmptyResult);
}

#undef LOCTEXT_NAMESPACE