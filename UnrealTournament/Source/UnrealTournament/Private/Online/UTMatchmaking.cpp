// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPartyBeaconClient.h"
#include "UTParty.h"
#include "UTPartyGameState.h"
#include "PartyMemberState.h"
#include "UTMatchmaking.h"
#include "UTMatchmakingGather.h"
#include "UTMatchmakingSingleSession.h"
#include "UTPlaylistManager.h"
#include "UTMcpUtils.h"
#include "QosInterface.h"
#include "QosRegionManager.h"

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
		//FQosInterface* QosInterface = FQosInterface::Get();
		//QosEvaluator = ensure(QosInterface) ? QosInterface->CreateQosEvaluator() : nullptr;
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

AUTBasePlayerController* UUTMatchmaking::GetOwningController() const
{
	if (ControllerId < INVALID_CONTROLLERID)
	{
		UWorld* World = GetWorld();
		check(World);

		ULocalPlayer* LP = GEngine->GetLocalPlayerFromControllerId(World, ControllerId);
		APlayerController* PC = LP ? LP->PlayerController : nullptr;
		return Cast<AUTBasePlayerController>(PC);
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
		UTPartyGameState->OnLeaderPartyStateChanged().AddUObject(this, &ThisClass::OnLeaderPartyStateChanged);
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
		UTPartyGameState->OnLeaderPartyStateChanged().RemoveAll(this);
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

void UUTMatchmaking::OnLeaderPartyStateChanged(EUTPartyState NewPartyState)
{
	OnPartyStateChange().Broadcast(NewPartyState);
}

void UUTMatchmaking::OnClientPartyStateChanged(EUTPartyState NewPartyState)
{
	UE_LOG(LogOnline, Log, TEXT("OnClientPartyStateChanged %d"), (int32)NewPartyState);

	OnPartyStateChange().Broadcast(NewPartyState);

	if (NewPartyState == EUTPartyState::TravelToServer)
	{
		if (!Matchmaking || Matchmaking->GetMatchmakingState().State == EMatchmakingState::Type::JoinSuccess)
		{
			UE_LOG(LogOnlineGame, Display, TEXT("Follower matchmaking complete, travelling to server"));
			TravelToServer();
		}
		else
		{
			UE_LOG(LogOnlineGame, Display, TEXT("Follower matchmaking not ready to travel to server, queuing travel request"));
			bQueuedTravelToServer = true;
		}
	}

	if (NewPartyState == EUTPartyState::Menus)
	{
		AUTBasePlayerController* PC = GetOwningController();
		if (PC)
		{
			UUTLocalPlayer* LP = PC->GetUTLocalPlayer();
			if (LP && !LP->IsPartyLeader() && !LP->IsMenuGame())
			{
				LP->ReturnToMainMenu();
			}
		}
	}
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
		/*
		if (QosEvaluator && QosEvaluator->IsActive())
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("Cancelling during qos evaluation"));
			QosEvaluator->Cancel();
		}
		else*/
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

		DisconnectFromLobby();
	}

	ClearCachedMatchmakingData();

	bQueuedTravelToServer = false;
}

void UUTMatchmaking::DisconnectFromLobby()
{
	// If the reservation beacon client still exists, haven't transitioned to the lobby yet, so cancel reservation
	if (ReservationBeaconClient)
	{
		ReservationBeaconClient->CancelReservation();
	}
	
	CleanupReservationBeacon();

	UUTGameInstance* GameInstance = GetUTGameInstance();
	if (GameInstance)
	{
		IOnlineSessionPtr SessionInt = Online::GetSessionInterface();
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
		}
	}

	// Clear cached data so reconnection attempts aren't made against faulty data
	ClearCachedMatchmakingData();
}

void UUTMatchmaking::OnDisconnectFromLobbyComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnDisconnectFromLobbyComplete %s Success: %d"), *SessionName.ToString(), bWasSuccessful);
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

void UUTMatchmaking::RetryFindGatheringSession()
{
	FMatchmakingParams NewParams;
	NewParams = CachedMatchmakingSearchParams.GetMatchmakingParams();
	NewParams.MinimumEloRangeBeforeHosting = FMath::Min(1000, NewParams.MinimumEloRangeBeforeHosting * 2);

	if (CachedSearchResult.Session.SessionInfo.IsValid())
	{
		NewParams.SessionIdToSkip = CachedSearchResult.Session.SessionInfo->GetSessionId().ToString();
	}

	// This disconnect is async so this is unsafe and should get rewritten
	DisconnectFromLobby();

	FindGatheringSession(NewParams);
}

bool UUTMatchmaking::FindGatheringSession(const FMatchmakingParams& InParams)
{
	bool bResult = false;
	EstimatedWaitTime = 0;
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

			OnMatchmakingStarted().Broadcast(InParams.bRanked);
			bResult = true;

#if 0
			// No QoS servers running right now
			FOnQosSearchComplete CompletionDelegate = FOnQosSearchComplete::CreateUObject(this, &ThisClass::ContinueMatchmaking, MatchmakingParams);
			QosEvaluator->FindDatacenters(ControllerId, CompletionDelegate);
#else
			// Pretend that QoS query actually happened, DatacenterId was passed from profile settings
			LookupTeamElo(EQosCompletionResult::Success, MatchmakingParams.DatacenterId, MatchmakingParams);
#endif
		}
	}

	return bResult;
}

bool UUTMatchmaking::FindSessionAsClient(const FMatchmakingParams& InParams)
{
	bool bResult = false;
	EstimatedWaitTime = 0;
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

			CachedMatchmakingSearchParams = FUTCachedMatchmakingSearchParams(EUTMatchmakingType::Session, InParams);

			MatchmakingSingleSession->OnMatchmakingStateChange().BindUObject(this, &ThisClass::OnSingleSessionMatchmakingStateChangeInternal);
			MatchmakingSingleSession->OnMatchmakingComplete().BindUObject(this, &ThisClass::OnSingleSessionMatchmakingComplete);

			MatchmakingSingleSession->Init(MatchmakingParams);
			MatchmakingSingleSession->StartMatchmaking();

			Matchmaking = MatchmakingSingleSession;

			OnMatchmakingStarted().Broadcast(InParams.bRanked);
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

void UUTMatchmaking::LookupTeamElo(EQosCompletionResult QoSResult, const FString& DatacenterId, FMatchmakingParams InParams)
{
	UE_LOG(LogOnline, Log, TEXT("LookupTeamElo %d"), (int32)QoSResult);

	//QosEvaluator->SetAnalyticsProvider(nullptr);
	
	UUTMcpUtils* McpUtils = nullptr;
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		IOnlineIdentityPtr OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
		if (OnlineIdentityInterface.IsValid())
		{
			McpUtils = UUTMcpUtils::Get(GetWorld(), OnlineIdentityInterface->GetUniquePlayerId(InParams.ControllerId));
			if (McpUtils == nullptr)
			{
				UE_LOG(LogOnline, Warning, TEXT("Unable to load McpUtils. Will not be able to read ELO from MCP"));
			}
		}
	}

	if (McpUtils && Matchmaking && QoSResult == EQosCompletionResult::Success)
	{
		InParams.DatacenterId = DatacenterId;

		UUTGameInstance* UTGameInstance = GetUTGameInstance();

		FString MatchRatingType = TEXT("RankedShowdownSkillRating");
		if (UTGameInstance)
		{
			UTGameInstance->GetPlaylistManager()->GetTeamEloRatingForPlaylist(InParams.PlaylistId, MatchRatingType);
		}
		
		TArray<UPartyMemberState*> PartyMembers;
		if (UTGameInstance)
		{
			UUTParty* Parties = UTGameInstance->GetParties();
			if (Parties)
			{
				UPartyGameState* Party = Parties->GetPersistentParty();
				if (Party)
				{
					Party->GetAllPartyMembers(PartyMembers);
				}
			}
		}

		TArray<FUniqueNetIdRepl> AccountIds;
		int32 PartySize = PartyMembers.Num();
		for (int32 i = 0; i < PartySize; i++)
		{
			AccountIds.Add(PartyMembers[i]->UniqueId);
		}

		McpUtils->GetTeamElo(MatchRatingType, AccountIds, PartySize, [this, InParams](const FOnlineError& TeamEloResult, const FTeamElo& Response)
		{
			if (!TeamEloResult.bSucceeded)
			{
				// best we can do is log an error
				UE_LOG(LogOnline, Warning, TEXT("Failed to read ELO from the server. (%d) %s %s"), TeamEloResult.HttpResult, *TeamEloResult.ErrorCode, *TeamEloResult.ErrorMessage.ToString());
				ContinueMatchmaking(1500, InParams);
			}
			else
			{
				ContinueMatchmaking(Response.Rating, InParams);
			}
		});

		McpUtils->GetEstimatedWaitTimes([this, MatchRatingType](const FOnlineError& Result, const FEstimatedWaitTimeInfo& EstimateWaitTimeInfo) {
			if (!Result.bSucceeded)
			{
				// best we can do is log an error
				UE_LOG(LogOnline, Warning, TEXT("Failed to get estimated wait times from the server. (%d) %s %s"), Result.HttpResult, *Result.ErrorCode, *Result.ErrorMessage.ToString());
			}
			else
			{
				for (int32 i = 0; i < EstimateWaitTimeInfo.WaitTimes.Num(); i++)
				{
					UE_LOG(LogOnline, Log, TEXT("%s %f"), *EstimateWaitTimeInfo.WaitTimes[i].RatingType, EstimateWaitTimeInfo.WaitTimes[i].AverageWaitTimeSecs);
					if (EstimateWaitTimeInfo.WaitTimes[i].RatingType == MatchRatingType)
					{
						EstimatedWaitTime = EstimateWaitTimeInfo.WaitTimes[i].AverageWaitTimeSecs;
					}
				}
			}
		});
	}
	else
	{
		FOnlineSessionSearchResult EmptySearchResult;
		OnMatchmakingCompleteInternal(QoSResult == EQosCompletionResult::Canceled ? EMatchmakingCompleteResult::Cancelled : EMatchmakingCompleteResult::Failure, EmptySearchResult);
	}
}

void UUTMatchmaking::ContinueMatchmaking(int32 TeamElo, FMatchmakingParams InParams)
{
	UE_LOG(LogOnline, Log, TEXT("ContinueMatchmaking TeamElo:%d"), (int32)TeamElo);
	
	InParams.TeamElo = TeamElo;
	if (Matchmaking)
	{
		Matchmaking->Init(InParams);
		Matchmaking->StartMatchmaking();
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
		AUTBasePlayerController* UTPC = GetOwningController();
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
	UE_LOG(LogOnline, Log, TEXT("OnSingleSessionMatchmakingComplete"));
	if (Result == EMatchmakingCompleteResult::Success && SearchResult.IsValid())
	{
		if ((CachedMatchmakingSearchParams.GetMatchmakingParams().Flags & EMatchmakingFlags::NoReservation) == EMatchmakingFlags::None)
		{
			AUTBasePlayerController* UTPC = GetOwningController();
			FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::ConnectToReservationBeacon, SearchResult);
			UTPC->GetWorldTimerManager().SetTimer(ConnectToReservationBeaconTimerHandle, TimerDelegate, CONNECT_TO_RESERVATION_BEACON_DELAY, false);
		}

		// Follower was fetching server info when the leader told the client to join
		if (bQueuedTravelToServer)
		{
			UE_LOG(LogOnlineGame, Display, TEXT("Follower matchmaking complete, travelling to server"));
			TravelToServer();
			bQueuedTravelToServer = false;
		}

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

	AUTBasePlayerController* PC = GetOwningController();
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

	UUTGameInstance* UTGameInstance = GetUTGameInstance();
	if (ensure(UTGameInstance))
	{
		UUTParty* Parties = UTGameInstance->GetParties();
		if (ensure(Parties))
		{
			UUTPartyGameState* PersistentParty = Parties->GetUTPersistentParty();
			if (ensure(PersistentParty))
			{
				PersistentParty->SetPlayersNeeded(NumRemaining);
			}
		}
	}
}

void UUTMatchmaking::OnReservationFull()
{
	UE_LOG(LogOnline, Log, TEXT("OnReservationFull"));
}

void UUTMatchmaking::TravelPartyToServer()
{
	TravelToServer();
	
	UUTGameInstance* UTGameInstance = GetUTGameInstance();
	if (ensure(UTGameInstance))
	{
		if (TimeMatchmakingStarted > 0)
		{
			UUTMcpUtils* McpUtils = UUTMcpUtils::Get(GetWorld(), TSharedPtr<const FUniqueNetId>());
			if (McpUtils)
			{
				FString MatchRatingType;
				if (UTGameInstance->GetPlaylistManager()->GetTeamEloRatingForPlaylist(CachedMatchmakingSearchParams.GetMatchmakingParams().PlaylistId, MatchRatingType))
				{
					// tell MCP about the match to update players' MMRs
					McpUtils->ReportWaitTime(MatchRatingType, GetWorld()->RealTimeSeconds - TimeMatchmakingStarted, [this, MatchRatingType](const FOnlineError& Result) {
						if (!Result.bSucceeded)
						{
							// best we can do is log an error
							UE_LOG(UT, Warning, TEXT("Failed to report wait time to the server. (%d) %s %s"), Result.HttpResult, *Result.ErrorCode, *Result.ErrorMessage.ToString());
						}
						else
						{
							UE_LOG(UT, Log, TEXT("Wait time reported to backend"));
						}
					});
				}
			}
		}

		UUTParty* Parties = UTGameInstance->GetParties();
		if (ensure(Parties))
		{
			UUTPartyGameState* PersistentParty = Parties->GetUTPersistentParty();
			if (ensure(PersistentParty))
			{
				PersistentParty->NotifyTravelToServer();
			}
		}
	}
}

void UUTMatchmaking::OnAllowedToProceedFromReservation()
{
	UE_LOG(LogOnline, Log, TEXT("OnAllowedToProceedFromReservation"));

	// Connect to game
	TravelPartyToServer();
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

int32 UUTMatchmaking::GetMatchmakingEloRange()
{
	if (Matchmaking)
	{
		return Matchmaking->GetMatchmakingParams().EloRange;
	}

	return 0;
}

int32 UUTMatchmaking::GetMatchmakingTeamElo()
{
	if (Matchmaking)
	{
		return Matchmaking->GetMatchmakingParams().TeamElo;
	}

	return 0;
}

bool UUTMatchmaking::IsRankedMatchmaking()
{
	if (Matchmaking)
	{
		return Matchmaking->GetMatchmakingParams().bRanked;
	}

	if (CachedMatchmakingSearchParams.IsValid())
	{
		return CachedMatchmakingSearchParams.GetMatchmakingParams().bRanked;
	}

	return false;
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

bool UUTMatchmaking::IsMatchmaking()
{
	if (Matchmaking)
	{
		return Matchmaking->IsMatchmaking();
	}
	return false;
}

int32 UUTMatchmaking::GetEstimatedMatchmakingTime()
{
	return EstimatedWaitTime;
}

#undef LOCTEXT_NAMESPACE