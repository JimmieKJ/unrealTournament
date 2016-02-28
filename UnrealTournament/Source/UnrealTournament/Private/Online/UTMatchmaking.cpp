// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPartyBeaconClient.h"
#include "UTParty.h"
#include "UTPartyGameState.h"
#include "UTMatchmaking.h"
#include "UTMatchmakingGather.h"
#include "UTMatchmakingSingleSession.h"
#include "QoSInterface.h"
#include "QoSEvaluator.h"

#define LOCTEXT_NAMESPACE "UTMatchmaking"
#define JOIN_ACK_FAILSAFE_TIMER 30.0f

FUTCachedMatchmakingSearchParams::FUTCachedMatchmakingSearchParams()
	: MatchmakingType(EUTMatchmakingType::Gathering)
	, MatchmakingParams()
	, bValid(false)
{}

FUTCachedMatchmakingSearchParams::FUTCachedMatchmakingSearchParams(EUTMatchmakingType InType, const FMatchmakingParams& InParams)
	: MatchmakingType(InType)
	, MatchmakingParams(InParams)
	, bValid(true)
{}

bool FUTCachedMatchmakingSearchParams::IsValid() const
{
	return bValid;
}

void FUTCachedMatchmakingSearchParams::Invalidate()
{
	bValid = false;
}

EUTMatchmakingType FUTCachedMatchmakingSearchParams::GetMatchmakingType() const
{
	return MatchmakingType;
}

const FMatchmakingParams& FUTCachedMatchmakingSearchParams::GetMatchmakingParams() const
{
	return MatchmakingParams;
}

UUTMatchmaking::UUTMatchmaking(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	ReservationBeaconClientClass(nullptr),
	ReservationBeaconClient(nullptr)
{
	ReservationBeaconClientClass = AUTPartyBeaconClient::StaticClass();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FQosInterface* QosInterface = FQosInterface::Get();
		QosEvaluator = ensure(QosInterface) ? QosInterface->CreateQosEvaluator() : nullptr;
	}
}

void UUTMatchmaking::Init()
{
	RegisterDelegates();
}

void UUTMatchmaking::BeginDestroy()
{
	Super::BeginDestroy();
	UnregisterDelegates();
}

void UUTMatchmaking::RegisterDelegates()
{
	UUTGameInstance* GameInstance = GetUTGameInstance();
	check(GameInstance);

	UUTParty* UTParty = GameInstance->GetParties();

	UnregisterDelegates();

	if (UTParty)
	{
		UTParty->OnPartyJoined().AddUObject(this, &ThisClass::OnPartyJoined);
		UTParty->OnPartyLeft().AddUObject(this, &ThisClass::OnPartyLeft);
		UTParty->OnPartyMemberLeft().AddUObject(this, &ThisClass::OnPartyMemberLeft);
		UTParty->OnPartyMemberPromoted().AddUObject(this, &ThisClass::OnPartyMemberPromoted);
	}
}

void UUTMatchmaking::UnregisterDelegates()
{
	UUTGameInstance* GameInstance = GetUTGameInstance();
	if (GameInstance)
	{
		UUTParty* UTParty = GameInstance->GetParties();
		if (UTParty)
		{
			UTParty->OnPartyJoined().RemoveAll(this);
			UTParty->OnPartyLeft().RemoveAll(this);
			UTParty->OnPartyMemberLeft().RemoveAll(this);
			UTParty->OnPartyMemberPromoted().RemoveAll(this);
		}
	}
}

void UUTMatchmaking::OnAllowedToProceedFromReservationTimeout()
{
	DisconnectFromReservationBeacon();
}

UWorld* UUTMatchmaking::GetWorld() const
{
	UGameInstance* GameInstance = Cast<UGameInstance>(GetOuter());
	check(GameInstance);
	return GameInstance->GetWorld();
}

UUTGameInstance* UUTMatchmaking::GetUTGameInstance() const
{
	return Cast<UUTGameInstance>(GetOuter());
}

AUTPlayerController* UUTMatchmaking::GetOwningController() const
{
	if (ControllerId < INVALID_CONTROLLERID)
	{
		UWorld* World = GetWorld();
		check(World);

		ULocalPlayer* LP = GEngine->GetLocalPlayerFromControllerId(World, ControllerId);
		APlayerController* PC = LP ? LP->PlayerController : nullptr;
		return Cast<AUTPlayerController>(PC);
	}

	return nullptr;
}

void UUTMatchmaking::OnPartyJoined(UPartyGameState* InParty)
{
	TSharedPtr<const FOnlinePartyId> PartyId = InParty ? InParty->GetPartyId() : nullptr;
	UE_LOG(LogOnline, Log, TEXT("OnPartyJoined %s"), PartyId.IsValid() ? *PartyId->ToString() : TEXT("Invalid!"));

	if (InParty)
	{
		UUTPartyGameState* UTPartyGameState = Cast<UUTPartyGameState>(InParty);
		check(UTPartyGameState);
		UTPartyGameState->OnClientPartyStateChanged().AddUObject(this, &ThisClass::OnClientPartyStateChanged);
		UTPartyGameState->OnClientMatchmakingComplete().AddUObject(this, &ThisClass::OnClientMatchmakingComplete);
		UTPartyGameState->OnClientSessionIdChanged().AddUObject(this, &ThisClass::OnClientSessionIdChanged);
	}
}

void UUTMatchmaking::OnPartyLeft(UPartyGameState* InParty, EMemberExitedReason Reason)
{
	TSharedPtr<const FOnlinePartyId> PartyId = InParty ? InParty->GetPartyId() : nullptr;
	UE_LOG(LogOnline, Log, TEXT("OnPartyLeft %s"), PartyId.IsValid() ? *PartyId->ToString() : TEXT("Invalid!"));

	if (InParty)
	{
		// If in the middle of matchmaking, cleanup and end all matchmaking, lobbies, etc
		UUTGameInstance* GameInstance = GetUTGameInstance();
		if (GameInstance)
		{
			UUTParty* UTParty = GameInstance->GetParties();
			if (UTParty)
			{
				TSharedPtr<const FOnlinePartyId> PersistentPartyId = UTParty->GetPersistentPartyId();
				TSharedPtr<const FOnlinePartyId> CurrPartyId = InParty->GetPartyId();

				bool bIsPersistentParty = PersistentPartyId.IsValid() && CurrPartyId.IsValid() && (*PersistentPartyId == *CurrPartyId);
				if (bIsPersistentParty || !PersistentPartyId.IsValid())
				{
					// This will cancel or leave a lobby
					CancelMatchmaking();
				}
			}
		}

		UUTPartyGameState* UTPartyGameState = Cast<UUTPartyGameState>(InParty);
		check(UTPartyGameState);
		UTPartyGameState->OnClientPartyStateChanged().RemoveAll(this);
		UTPartyGameState->OnClientMatchmakingComplete().RemoveAll(this);
		UTPartyGameState->OnClientSessionIdChanged().RemoveAll(this);
	}
}

void UUTMatchmaking::OnPartyMemberLeft(UPartyGameState* InParty, const FUniqueNetIdRepl& InMemberId, EMemberExitedReason Reason)
{
	TSharedPtr<const FOnlinePartyId> PartyId = InParty ? InParty->GetPartyId() : nullptr;
	UE_LOG(LogOnline, Log, TEXT("OnPartyMemberLeft %s %s"), PartyId.IsValid() ? *PartyId->ToString() : TEXT("Invalid!"), *InMemberId->ToString());
}

void UUTMatchmaking::OnPartyMemberPromoted(UPartyGameState* InParty, const FUniqueNetIdRepl& InMemberId)
{
	TSharedPtr<const FOnlinePartyId> PartyId = InParty ? InParty->GetPartyId() : nullptr;
	UE_LOG(LogOnline, Log, TEXT("OnPartyMemberPromoted %s %s"), PartyId.IsValid() ? *PartyId->ToString() : TEXT("Invalid!"), *InMemberId->ToString());

	if (InParty)
	{
		if (Matchmaking && Matchmaking->IsMatchmaking())
		{
			UUTGameInstance* GameInstance = GetUTGameInstance();
			if (ensure(GameInstance))
			{
				UUTParty* UTParty = GameInstance->GetParties();
				if (ensure(UTParty))
				{
					TSharedPtr<const FOnlinePartyId> PersistentPartyId = UTParty->GetPersistentPartyId();
					TSharedPtr<const FOnlinePartyId> CurrPartyId = InParty->GetPartyId();

					if (PersistentPartyId.IsValid() && CurrPartyId.IsValid() &&
						(*PersistentPartyId == *CurrPartyId))
					{
						UE_LOG(LogOnline, Log, TEXT("Matchmaking cancelled during leader promotion"));
						CancelMatchmaking();
					}
				}
			}
		}
	}
}

void UUTMatchmaking::OnClientPartyStateChanged(EUTPartyState NewPartyState)
{
	UE_LOG(LogOnline, Log, TEXT("OnClientPartyStateChanged %d"), (int32)NewPartyState);
}

void UUTMatchmaking::CleanupReservationBeacon()
{
	if (ReservationBeaconClient)
	{
		ReservationBeaconClient->OnHostConnectionFailure().Unbind();
		ReservationBeaconClient->OnReservationRequestComplete().Unbind();
		ReservationBeaconClient->OnReservationCountUpdate().Unbind();
		ReservationBeaconClient->OnReservationFull().Unbind();
		ReservationBeaconClient->OnAllowedToProceedFromReservation().Unbind();
		ReservationBeaconClient->OnAllowedToProceedFromReservationTimeout().Unbind();
		ReservationBeaconClient->DestroyBeacon();
		ReservationBeaconClient = nullptr;
	}
}

void UUTMatchmaking::CancelMatchmaking()
{
	if (Matchmaking)
	{
		ensure(ReservationBeaconClient == nullptr);
		if (QosEvaluator && QosEvaluator->IsActive())
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cancelling during qos evaluation"));
			QosEvaluator->Cancel();
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cancelling during matchmaking"));
			Matchmaking->CancelMatchmaking();
		}
	}
	else
	{
		UWorld* World = GetWorld();
		FTimerManager& TimerManager = World->GetTimerManager();

		if (TimerManager.IsTimerActive(ConnectToReservationBeaconTimerHandle))
		{
			// caught right before actually trying to reconnect to beacon, no lobby beacon active
			// should tell server about cancel but currently don't
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cancelling between matchmaking and reservation reconnect"));
			ensure(ReservationBeaconClient == nullptr);
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cancelling with no timer and no lobby"));
		}
	}

	ClearCachedMatchmakingData();
}

void UUTMatchmaking::ClearCachedMatchmakingData()
{
	CachedSearchResult = FOnlineSessionSearchResult();
	CachedMatchmakingSearchParams.Invalidate();
}

void UUTMatchmaking::DisconnectFromReservationBeacon()
{
	UUTGameInstance* GameInstance = GetUTGameInstance();
	if (ensure(GameInstance))
	{
		UWorld* World = GameInstance->GetWorld();

		if (ensure(World))
		{
			FTimerManager& WorldTimerManager = World->GetTimerManager();

			World->GetTimerManager().ClearTimer(ConnectToReservationBeaconTimerHandle);
		}

		if (ensure(ReservationBeaconClient))
		{
			ReservationBeaconClient->CancelReservation();
			CleanupReservationBeacon();
		}

		IOnlineSessionPtr SessionInt = Online::GetSessionInterface(/*World*/);
		if (SessionInt.IsValid())
		{
			FOnlineSessionSettings* SessionSettings = SessionInt->GetSessionSettings(GameSessionName);
			if (SessionSettings)
			{
				// Cleanup the existing game session before proceeding
				FOnDestroySessionCompleteDelegate CompletionDelegate;
				CompletionDelegate.BindUObject(this, &ThisClass::OnDisconnectFromReservationBeaconComplete);
				GameInstance->SafeSessionDelete(GameSessionName, CompletionDelegate);
			}
			else
			{
				// Proceed directly to disconnect
				OnDisconnectFromReservationBeaconComplete(GameSessionName, true);
			}
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("No session interface!"));
			OnDisconnectFromReservationBeaconComplete(GameSessionName, false);
		}
	}
}

void UUTMatchmaking::OnDisconnectFromReservationBeaconComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		ReattemptMatchmakingFromCachedParameters();
	}
}

void UUTMatchmaking::ReattemptMatchmakingFromCachedParameters()
{
	if (CachedMatchmakingSearchParams.IsValid())
	{
		UUTGameInstance* UTGameInstance = GetUTGameInstance();
		if (ensure(UTGameInstance))
		{
			UUTParty* Parties = UTGameInstance->GetParties();
			if (ensure(Parties))
			{
				UUTPartyGameState* PersistentParty = Parties->GetUTPersistentParty();
				if (ensure(PersistentParty))
				{
					// Construct matchmaking params from the cached ones, replacing the party size and matchmaking level, as both
					// may have changed since original matchmaking
					FMatchmakingParams ParamsToUse = CachedMatchmakingSearchParams.GetMatchmakingParams();
					ParamsToUse.PartySize = PersistentParty->GetPartySize();

					// UT specific search verb here
				}
			}
		}
	}
}

void UUTMatchmaking::OnClientMatchmakingComplete(EMatchmakingCompleteResult Result)
{
	UE_LOG(LogOnline, Log, TEXT("OnClientMatchmakingComplete %d"), (int32)Result);
}

bool UUTMatchmaking::FindGatheringSession(const FMatchmakingParams& InParams)
{
	bool bResult = false;
	if (InParams.ControllerId != INVALID_CONTROLLERID)
	{
		UUTMatchmakingGather* MatchmakingGather = NewObject<UUTMatchmakingGather>();
		if (MatchmakingGather)
		{
			// Starting a new find, clear out any cached matchmaking data
			ClearCachedMatchmakingData();

			ControllerId = InParams.ControllerId;

			FMatchmakingParams MatchmakingParams(InParams);
			if ((MatchmakingParams.Flags & EMatchmakingFlags::CreateNewOnly) == EMatchmakingFlags::CreateNewOnly)
			{
				MatchmakingParams.StartWith = EMatchmakingStartLocation::CreateNew;
			}

			CachedMatchmakingSearchParams = FUTCachedMatchmakingSearchParams(EUTMatchmakingType::Gathering, InParams);

			MatchmakingGather->OnMatchmakingStateChange().BindUObject(this, &ThisClass::OnGatherMatchmakingStateChangeInternal);
			MatchmakingGather->OnMatchmakingComplete().BindUObject(this, &ThisClass::OnGatherMatchmakingComplete);

			Matchmaking = MatchmakingGather;

			OnMatchmakingStarted().Broadcast();
			bResult = true;

#if 0
			// No QoS servers running right now
			FOnQosSearchComplete CompletionDelegate = FOnQosSearchComplete::CreateUObject(this, &ThisClass::ContinueMatchmaking, MatchmakingParams);
			QosEvaluator->FindDatacenters(ControllerId, CompletionDelegate);
#else
			FString FakeDatacenterId = TEXT("USA");
			ContinueMatchmaking(EQosCompletionResult::Success, FakeDatacenterId, MatchmakingParams);
#endif
		}
	}

	return bResult;
}

bool UUTMatchmaking::FindSessionAsClient(const FMatchmakingParams& InParams)
{
	bool bResult = false;
	if (InParams.ControllerId != INVALID_CONTROLLERID)
	{
		UUTMatchmakingSingleSession* MatchmakingSingleSession = NewObject<UUTMatchmakingSingleSession>();
		if (MatchmakingSingleSession)
		{
			// Starting a new find, clear out any cached matchmaking data
			ClearCachedMatchmakingData();

			ControllerId = InParams.ControllerId;
			
			FMatchmakingParams MatchmakingParams(InParams);
			MatchmakingParams.StartWith = EMatchmakingStartLocation::FindSingle;

			MatchmakingSingleSession->OnMatchmakingStateChange().BindUObject(this, &ThisClass::OnSingleSessionMatchmakingStateChangeInternal);
			MatchmakingSingleSession->OnMatchmakingComplete().BindUObject(this, &ThisClass::OnSingleSessionMatchmakingComplete);

			MatchmakingSingleSession->Init(MatchmakingParams);
			MatchmakingSingleSession->StartMatchmaking();

			Matchmaking = MatchmakingSingleSession;

			OnMatchmakingStarted().Broadcast();
			bResult = true;

			// FindSessionAsClient intentionally does not cache matchmaking params, as it is intended to go to a very specific session
		}
	}

	return bResult;
}

void UUTMatchmaking::OnGatherMatchmakingStateChangeInternal(EMatchmakingState::Type OldState, EMatchmakingState::Type NewState)
{
	FMMAttemptState MMState;
	if (Matchmaking)
	{
		MMState = Matchmaking->GetMatchmakingState();
	}

	OnMatchmakingStateChange().Broadcast(OldState, NewState, MMState);
}

void UUTMatchmaking::OnSingleSessionMatchmakingStateChangeInternal(EMatchmakingState::Type OldState, EMatchmakingState::Type NewState)
{
	FMMAttemptState MMState;
	if (Matchmaking)
	{
		MMState = Matchmaking->GetMatchmakingState();
	}

	OnMatchmakingStateChange().Broadcast(OldState, NewState, MMState);
}

void UUTMatchmaking::ContinueMatchmaking(EQosCompletionResult Result, const FString& DatacenterId, FMatchmakingParams InParams)
{
	UE_LOG(LogOnline, Log, TEXT("ContinueMatchmaking %d"), (int32)Result);

	QosEvaluator->SetAnalyticsProvider(nullptr);

	if (Matchmaking &&
		(Result == EQosCompletionResult::Cached || Result == EQosCompletionResult::Success)
		)
	{
		InParams.DatacenterId = DatacenterId;
		Matchmaking->Init(InParams);
		Matchmaking->StartMatchmaking();
	}
	else
	{
		FOnlineSessionSearchResult EmptySearchResult;
		OnMatchmakingCompleteInternal(Result == EQosCompletionResult::Canceled ? EMatchmakingCompleteResult::Cancelled : EMatchmakingCompleteResult::Failure, EmptySearchResult);
	}
}

void UUTMatchmaking::OnMatchmakingCompleteInternal(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult)
{
	if (Matchmaking)
	{
		Matchmaking = nullptr;
	}

	OnMatchmakingComplete().Broadcast(Result);
}

void UUTMatchmaking::OnGatherMatchmakingComplete(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult)
{
	if (Result == EMatchmakingCompleteResult::Success && SearchResult.IsValid())
	{
		AUTPlayerController* UTPC = GetOwningController();
		FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::ConnectToReservationBeacon, SearchResult);
		UTPC->GetWorldTimerManager().SetTimer(ConnectToReservationBeaconTimerHandle, TimerDelegate, CONNECT_TO_RESERVATION_BEACON_DELAY, false);

		OnMatchmakingCompleteInternal(Result, SearchResult);
	}
	else if (Result == EMatchmakingCompleteResult::Cancelled)
	{
		OnMatchmakingCompleteInternal(Result, SearchResult);
	}
	else
	{
		OnMatchmakingCompleteInternal(Result, SearchResult);
	}
}

void UUTMatchmaking::OnSingleSessionMatchmakingComplete(EMatchmakingCompleteResult Result, const FOnlineSessionSearchResult& SearchResult)
{
	if (Result == EMatchmakingCompleteResult::Success && SearchResult.IsValid())
	{
		AUTPlayerController* UTPC = GetOwningController();
		FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::ConnectToReservationBeacon, SearchResult);
		UTPC->GetWorldTimerManager().SetTimer(ConnectToReservationBeaconTimerHandle, TimerDelegate, CONNECT_TO_RESERVATION_BEACON_DELAY, false);

		OnMatchmakingCompleteInternal(Result, SearchResult);
	}
	else if (Result == EMatchmakingCompleteResult::Cancelled)
	{
		OnMatchmakingCompleteInternal(Result, SearchResult);
	}
	else
	{
		OnMatchmakingCompleteInternal(Result, SearchResult);
	}
}

void UUTMatchmaking::ConnectToReservationBeacon(FOnlineSessionSearchResult SearchResult)
{
	UWorld* World = GetWorld();

	AUTPlayerController* PC = GetOwningController();
	if (PC && PC->PlayerState && PC->PlayerState->UniqueId.IsValid())
	{
		if (PC->IsPartyLeader())
		{
			// Reconnect to the reservation beacon to maintain our place in the game (just until actual joined, holds place for all party members)
			ReservationBeaconClient = World->SpawnActor<AUTPartyBeaconClient>(ReservationBeaconClientClass);
			if (ReservationBeaconClient)
			{
				UE_LOG(LogOnlineGame, Verbose, TEXT("Created reservation beacon %s."), ReservationBeaconClient ? *ReservationBeaconClient->GetName() : *FString(TEXT("")));

				ReservationBeaconClient->OnHostConnectionFailure().BindUObject(this, &ThisClass::OnReservationBeaconConnectionFailure);
				ReservationBeaconClient->OnReservationRequestComplete().BindUObject(this, &ThisClass::OnReconnectResponseReceived, SearchResult);
				ReservationBeaconClient->OnReservationCountUpdate().BindUObject(this, &ThisClass::OnReservationCountUpdate);
				ReservationBeaconClient->OnReservationFull().BindUObject(this, &ThisClass::OnReservationFull);
				ReservationBeaconClient->OnAllowedToProceedFromReservation().BindUObject(this, &ThisClass::OnAllowedToProceedFromReservation);
				ReservationBeaconClient->OnAllowedToProceedFromReservationTimeout().BindUObject(this, &ThisClass::OnAllowedToProceedFromReservationTimeout);
				ReservationBeaconClient->Reconnect(SearchResult, PC->PlayerState->UniqueId);

				// Advertise the search result found
				OnConnectToLobby().Broadcast(SearchResult);
			}
			else
			{
				OnReservationBeaconConnectionFailure();
			}
		}
		else
		{
			// Lobby connection here, but probably unnecessary for UT
		}
	}
	else
	{
		OnReservationBeaconConnectionFailure();
	}
}

void UUTMatchmaking::OnReservationBeaconConnectionFailure()
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("Reservation beacon failure %s."), ReservationBeaconClient ? *ReservationBeaconClient->GetName() : *FString(TEXT("")));
	//DisconnectFromReservationBeacon();

}

void UUTMatchmaking::OnReservationCountUpdate(int32 NumRemaining)
{
	UE_LOG(LogOnline, Log, TEXT("OnReservationCountUpdate %d"), NumRemaining);
}

void UUTMatchmaking::OnReservationFull()
{
	UE_LOG(LogOnline, Log, TEXT("OnReservationFull"));

	TravelToServer();
}

void UUTMatchmaking::OnAllowedToProceedFromReservation()
{
	UE_LOG(LogOnline, Log, TEXT("OnAllowedToProceedFromReservation"));

	UWorld* World = GetWorld();
	if (ensure(World))
	{
		// Have a valid search result, clean-up the beacon and proceed to the lobby
		if (CachedSearchResult.IsValid())
		{
			CleanupReservationBeacon();

			// Connect to game
		}
		else
		{
			// This shouldn't be possible (something would have to internally modify the search result), but as a guard, cause a timeout
			OnAllowedToProceedFromReservationTimeout();
		}
	}
}

void UUTMatchmaking::OnReconnectResponseReceived(EPartyReservationResult::Type ReservationResponse, FOnlineSessionSearchResult SearchResult)
{
	UE_LOG(LogOnline, Log, TEXT("OnReconnectResponseReceived %s"), EPartyReservationResult::ToString(ReservationResponse));

	if (ReservationResponse == EPartyReservationResult::ReservationDuplicate)
	{
		if (ensure(ReservationBeaconClient))
		{
			ReservationBeaconClient->MarkReconnected();
		}

		// Cache the search result, it will be used when notified that it is alright to proceed to the lobby
		CachedSearchResult = SearchResult;
	}
	else if (ReservationResponse == EPartyReservationResult::ReservationRequestCanceled)
	{
		// Already connected to the lobby, not matchmaking, party leader gets this and wants to preserve party
	}
	else
	{
		OnReservationBeaconConnectionFailure();
	}
}

void UUTMatchmaking::TravelToServer()
{
	bool bWillTravel = false;

	UUTGameInstance* GameInstance = GetUTGameInstance();
	check(GameInstance);

	if (GameInstance->ClientTravelToSession(ControllerId, GameSessionName))
	{
		bWillTravel = false;
	}

	if (!bWillTravel)
	{
		// Heavy handed clean up
		//DisconnectFromLobby();
	}
}

void UUTMatchmaking::OnClientSessionIdChanged(const FString& SessionId)
{
	UE_LOG(LogOnline, Log, TEXT("OnClientSessionIdChanged %s"), *SessionId);
	
	if (!SessionId.IsEmpty())
	{
		FMatchmakingParams MMParams;
		MMParams.ControllerId = ControllerId;
		MMParams.SessionId = SessionId;
		MMParams.StartWith = EMatchmakingStartLocation::FindSingle;
		MMParams.Flags = EMatchmakingFlags::NoReservation;

		bool bSuccess = FindSessionAsClient(MMParams);
	}
	else
	{
		CancelMatchmaking();
	}
}

#undef LOCTEXT_NAMESPACE