// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMatchmakingPolicy.h"
#include "UTPartyBeaconClient.h"
#include "UTPartyGameState.h"

void FUTPartyRepState::Reset()
{
	FPartyState::Reset();
	bLobbyConnectionStarted = false;
	MatchmakingResult = EMatchmakingCompleteResult::NotStarted;
	SessionId.Empty();
}

UUTPartyGameState::UUTPartyGameState(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	InitPartyState(&PartyState);
	ReservationBeaconClientClass = AUTPartyBeaconClient::StaticClass();
}