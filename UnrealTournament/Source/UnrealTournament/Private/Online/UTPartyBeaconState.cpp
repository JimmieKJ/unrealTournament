// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPartyBeaconState.h"

UUTPartyBeaconState::UUTPartyBeaconState(const FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
}

void UUTPartyBeaconState::ChangeSessionOwner(const TSharedRef<const FUniqueNetId>& InNewSessionOwner)
{
	GameSessionOwner = InNewSessionOwner;
}

void UUTPartyBeaconState::SetUserConfiguration(const FEmptyServerReservation& InReservationData, const TSharedPtr<const FUniqueNetId>& InGameSessionOwner)
{
	ReservationData = InReservationData;
	GameSessionOwner = InGameSessionOwner;
}