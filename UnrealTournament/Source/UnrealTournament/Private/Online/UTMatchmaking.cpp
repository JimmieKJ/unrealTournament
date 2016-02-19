// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPartyBeaconClient.h"
#include "UTParty.h"
#include "UTPartyGameState.h"
#include "UTMatchmaking.h"
#include "QoSInterface.h"
#include "QoSEvaluator.h"

#define LOCTEXT_NAMESPACE "UTMatchmaking"
#define JOIN_ACK_FAILSAFE_TIMER 30.0f

FUTCachedMatchmakingSearchParams::FUTCachedMatchmakingSearchParams()
	: MatchmakingType(EUTMatchmakingType::Ranked)
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
	ReservationBeaconClient(nullptr),
	LobbyBeaconClient(nullptr)
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
	check(UTParty);

	UnregisterDelegates();

	UTParty->OnPartyJoined().AddUObject(this, &ThisClass::OnPartyJoined);
	UTParty->OnPartyLeft().AddUObject(this, &ThisClass::OnPartyLeft);
	UTParty->OnPartyMemberLeft().AddUObject(this, &ThisClass::OnPartyMemberLeft);
	UTParty->OnPartyMemberPromoted().AddUObject(this, &ThisClass::OnPartyMemberPromoted);
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

AUTLobbyBeaconClient* UUTMatchmaking::GetLobbyBeaconClient() const
{
	return LobbyBeaconClient;
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
		else
		{
			UWorld* World = GetWorld();
			FTimerManager& TimerManager = World->GetTimerManager();

			bool bReservationTimerActive = TimerManager.IsTimerActive(ConnectToReservationBeaconTimerHandle);
			bool bLobbyTimerActive = TimerManager.IsTimerActive(ConnectToLobbyTimerHandle);
			bool bLobbyConnectionNotFinished = LobbyBeaconClient && LobbyBeaconClient->GetConnectionState() != EBeaconConnectionState::Open;
			if (bReservationTimerActive || bLobbyTimerActive || bLobbyConnectionNotFinished)
			{
				UE_LOG(LogOnline, Log, TEXT("Lobby connection cancelled during leader promotion"));
				CancelMatchmaking();
			}
		}
	}
}

void UUTMatchmaking::OnClientPartyStateChanged(EUTPartyState NewPartyState)
{
	UE_LOG(LogOnline, Log, TEXT("OnClientPartyStateChanged %d"), (int32)NewPartyState);
}

void UUTMatchmaking::DisconnectFromLobby()
{
	UUTGameInstance* GameInstance = GetUTGameInstance();
	check(GameInstance);

	UWorld* World = GameInstance->GetWorld();

	// Signal the start of lobby disconnect
	OnLobbyDisconnecting().Broadcast();

	if (World)
	{
		World->GetTimerManager().ClearTimer(ConnectToLobbyTimerHandle);
		World->GetTimerManager().ClearTimer(ConnectToReservationBeaconTimerHandle);
		World->GetTimerManager().ClearTimer(JoiningAckTimerHandle);

		AUTGameState* UTGameState = Cast<AUTGameState>(World->GetGameState());
		if (UTGameState)
		{
			AUTLobbyBeaconState* LobbyGameState = UTGameState->GetLobbyGameState();
			if (LobbyGameState)
			{
				LobbyGameState->Destroy();
				UTGameState->SetLobbyGameState(nullptr);
			}
		}
	}

	// If the reservation beacon client still exists, haven't transitioned to the lobby yet, so cancel reservation
	if (ReservationBeaconClient)
	{
		ReservationBeaconClient->CancelReservation();
	}

	CleanupLobbyBeacons();

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(/*World*/);
	if (SessionInt.IsValid())
	{
		FOnlineSessionSettings* SessionSettings = SessionInt->GetSessionSettings(GameSessionName);
		if (SessionSettings)
		{
			// Cleanup the existing game session before proceeding
			FOnDestroySessionCompleteDelegate CompletionDelegate;
			CompletionDelegate.BindUObject(this, &ThisClass::OnDisconnectFromLobbyComplete);
			GameInstance->SafeSessionDelete(GameSessionName, CompletionDelegate);
		}
		else
		{
			// Proceed directly to disconnect
			OnDisconnectFromLobbyComplete(GameSessionName, true);
		}
	}
	else
	{
		UE_LOG(LogOnlineGame, Verbose, TEXT("No session interface!"));
		OnDisconnectFromLobbyComplete(GameSessionName, false);
	}

	// Clear cached data so reconnection attempts aren't made against faulty data
	ClearCachedMatchmakingData();
}

void UUTMatchmaking::OnDisconnectFromLobbyComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnDisconnectFromLobbyComplete %s Success: %d"), *SessionName.ToString(), bWasSuccessful);
	OnLobbyDisconnected().Broadcast();
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

void UUTMatchmaking::CleanupLobbyBeacons()
{
	if (LobbyBeaconClient)
	{
		LobbyBeaconClient->DisconnectFromLobby();
		LobbyBeaconClient->OnJoiningGame().Unbind();
		LobbyBeaconClient->OnJoiningGameAck().Unbind();
		LobbyBeaconClient->OnHostConnectionFailure().Unbind();
		LobbyBeaconClient->OnLobbyConnectionEstablished().Unbind();
		LobbyBeaconClient->OnLoginComplete().Unbind();
		LobbyBeaconClient->DestroyBeacon();
		LobbyBeaconClient = nullptr;
	}

	CleanupReservationBeacon();
}


void UUTMatchmaking::CancelMatchmaking()
{
	if (Matchmaking)
	{
		ensure(LobbyBeaconClient == nullptr && ReservationBeaconClient == nullptr);
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
			ensure(LobbyBeaconClient == nullptr && ReservationBeaconClient == nullptr);
		}
		else if (TimerManager.IsTimerActive(ConnectToLobbyTimerHandle))
		{
			// caught right before lobby beacon connection, but have a reservation beacon connection
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cancelling between reservation connect and lobby connect"));
			ensure(LobbyBeaconClient == nullptr);
		}
		else if (LobbyBeaconClient)
		{
			// already established a lobby connection, cancel the reservation and disconnect
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cancelling while in lobby"));
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cancelling with no timer and no lobby"));
		}

		// Everything will cancel if possible and cleanup lobby locally
		DisconnectFromLobby();
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

			// Ensure on the lobby timer, as the code should never have gotten to this state if disconnecting from the reservation beacon
			ensure(!WorldTimerManager.IsTimerActive(ConnectToLobbyTimerHandle));

			World->GetTimerManager().ClearTimer(ConnectToLobbyTimerHandle);
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

#undef LOCTEXT_NAMESPACE