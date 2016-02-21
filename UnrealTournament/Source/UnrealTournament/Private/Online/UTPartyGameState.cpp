// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMatchmakingPolicy.h"
#include "UTPartyBeaconClient.h"
#include "UTPartyMemberState.h"
#include "UTParty.h"
#include "UTMatchmaking.h"
#include "UTPartyGameState.h"

void FUTPartyRepState::Reset()
{
	FPartyState::Reset();
	PartyProgression = EUTPartyState::Menus;
	bLobbyConnectionStarted = false;
	MatchmakingResult = EMatchmakingCompleteResult::NotStarted;
	SessionId.Empty();
}

UUTPartyGameState::UUTPartyGameState(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	PartyMemberStateClass = UUTPartyMemberState::StaticClass();

	InitPartyState(&PartyState);
	ReservationBeaconClientClass = AUTPartyBeaconClient::StaticClass();
}

void UUTPartyGameState::RegisterFrontendDelegates()
{
	UWorld* World = GetWorld();

	Super::RegisterFrontendDelegates();

	UUTParty* UTParty = Cast<UUTParty>(GetOuter());
	if (UTParty)
	{
		TSharedPtr<const FOnlinePartyId> ThisPartyId = GetPartyId();
		const FOnlinePartyTypeId ThisPartyTypeId = GetPartyTypeId();

		if (ensure(ThisPartyId.IsValid()) && ThisPartyTypeId == UUTParty::GetPersistentPartyTypeId())
		{
			// Persistent party features (move to subclass eventually)
			UUTGameInstance* GameInstance = GetTypedOuter<UUTGameInstance>();
			check(GameInstance);
			UUTMatchmaking* Matchmaking = GameInstance->GetMatchmaking();
			if (Matchmaking)
			{
				Matchmaking->OnMatchmakingStarted().AddUObject(this, &ThisClass::OnPartyMatchmakingStarted);
				Matchmaking->OnMatchmakingComplete().AddUObject(this, &ThisClass::OnPartyMatchmakingComplete);
			}
		}
	}
}

void UUTPartyGameState::UnregisterFrontendDelegates()
{
	Super::UnregisterFrontendDelegates();

	UWorld* World = GetWorld();

	UUTGameInstance* GameInstance = GetTypedOuter<UUTGameInstance>();
	if (GameInstance)
	{
		UUTMatchmaking* Matchmaking = GameInstance->GetMatchmaking();
		if (Matchmaking)
		{
			Matchmaking->OnMatchmakingStarted().RemoveAll(this);
			Matchmaking->OnMatchmakingComplete().RemoveAll(this);
		}
	}
}

void UUTPartyGameState::OnPartyMatchmakingStarted()
{
	if (IsLocalPartyLeader())
	{
		PartyState.PartyProgression = EUTPartyState::Matchmaking;
		UpdatePartyData(OwningUserId);
		SetAcceptingMembers(false, EJoinPartyDenialReason::Busy);
	}
}

void UUTPartyGameState::OnPartyMatchmakingComplete(EMatchmakingCompleteResult Result)
{
	if (IsLocalPartyLeader())
	{
		PartyState.MatchmakingResult = Result;

		if (Result == EMatchmakingCompleteResult::Success)
		{
			PartyState.PartyProgression = EUTPartyState::PostMatchmaking;
			//DoLeaderMatchmakingCompleteAnalytics();
		}
		else
		{
			UpdateAcceptingMembers();
			PartyState.PartyProgression = EUTPartyState::Menus;
		}

		UpdatePartyData(OwningUserId);
	}
	else
	{
		if (Result == EMatchmakingCompleteResult::Cancelled)
		{
			// Should only come from leader promotion or leader matchmaking cancellation
			// Party members can't cancel while passenger
		}
		else if (Result != EMatchmakingCompleteResult::Success)
		{
			UUTParty* UTParty = Cast<UUTParty>(GetOuter());
			if (ensure(UTParty))
			{
				UTParty->LeaveAndRestorePersistentParty();
			}
		}
	}
}

void UUTPartyGameState::OnLobbyConnectionStarted()
{
	SetLocation(EUTPartyMemberLocation::ConnectingToLobby);

	if (IsLocalPartyLeader())
	{
		PartyState.PartyProgression = EUTPartyState::PostMatchmaking;
		PartyState.bLobbyConnectionStarted = true;
		UpdatePartyData(OwningUserId);
	}
}

void UUTPartyGameState::OnLobbyConnectionAttemptFailed()
{
	CurrentSession = FOnlineSessionSearchResult();
	SetLocation(EUTPartyMemberLocation::PreLobby);

	if (IsLocalPartyLeader())
	{
		PartyState.PartyProgression = EUTPartyState::Menus;
		PartyState.bLobbyConnectionStarted = false;
		UpdatePartyData(OwningUserId);
	}

	int32 PartySize = GetPartySize();
	if (PartySize > 1)
	{
		UUTParty* UTParty = Cast<UUTParty>(GetOuter());
		UTParty->LeaveAndRestorePersistentParty();
	}
	else
	{
		UpdateAcceptingMembers();
	}
}

void UUTPartyGameState::OnLobbyWaitingForPlayers()
{
	SetLocation(EUTPartyMemberLocation::Lobby);
}

void UUTPartyGameState::OnLobbyConnectingToGame()
{
	// Possibly deal with pending approvals?
	CleanupReservationBeacon();
	SetLocation(EUTPartyMemberLocation::JoiningGame);
}

void UUTPartyGameState::OnLobbyDisconnected()
{
	CurrentSession = FOnlineSessionSearchResult();
	SetLocation(EUTPartyMemberLocation::PreLobby);

	if (IsLocalPartyLeader())
	{
		PartyState.PartyProgression = EUTPartyState::Menus;
		PartyState.bLobbyConnectionStarted = false;
		PartyState.SessionId.Empty();
		UpdatePartyData(OwningUserId);
	}

	UpdateAcceptingMembers();
}

void UUTPartyGameState::SetLocation(EUTPartyMemberLocation NewLocation)
{
	UWorld* World = GetWorld();
	if (World)
	{
		for (FLocalPlayerIterator It(GEngine, World); It; ++It)
		{
			ULocalPlayer* LP = Cast<ULocalPlayer>(*It);
			if (LP)
			{
				TSharedPtr<const FUniqueNetId> UniqueId = LP->GetPreferredUniqueNetId();

				FUniqueNetIdRepl PartyMemberId(UniqueId);
				UUTPartyMemberState* PartyMember = Cast<UUTPartyMemberState>(GetPartyMember(PartyMemberId));
				if (PartyMember)
				{
					PartyMember->SetLocation(NewLocation);
				}
			}
		}

		if (NewLocation == EUTPartyMemberLocation::InGame)
		{
			if (PartyConsoleVariables::CVarAcceptJoinsDuringLoad.GetValueOnGameThread())
			{
				UE_LOG(LogParty, Verbose, TEXT("Handling party reservation approvals after load into game."));
				ConnectToReservationBeacon();
			}
		}
	}
}

void UUTPartyGameState::OnConnectToLobby(const FOnlineSessionSearchResult& SearchResult, const FString& CriticalMissionSessionId)
{
	if (ensure(SearchResult.IsValid()))
	{
		// Everyone notes the search result for party leader migration
		CurrentSession = SearchResult;

		if (IsLocalPartyLeader())
		{
			PartyState.PartyProgression = EUTPartyState::PostMatchmaking;

			const FUniqueNetId& SessionId = SearchResult.Session.SessionInfo->GetSessionId();
			PartyState.SessionId = SessionId.ToString();

			UpdatePartyData(OwningUserId);

			if (ReservationBeaconClient == nullptr)
			{
				// Handle any party join requests now
				UE_LOG(LogParty, Verbose, TEXT("Handling party reservation approvals after connection to lobby."));
				ConnectToReservationBeacon();
			}
		}
	}
}