// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "Online/UTPartyBeaconClient.h"
#include "Online/UTPartyBeaconHost.h"
#include "OnlineSubsystemUtils.h"

AUTPartyBeaconClient::AUTPartyBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
}


void AUTPartyBeaconClient::EmptyServerReservationRequest(const FOnlineSessionSearchResult& DesiredHost, const FEmptyServerReservation& ReservationData, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PartyMembers)
{
	UE_LOG(LogBeacon, Verbose, TEXT("Creating empty server reservation request %d."), ReservationData.PlaylistId);

	if (ReservationData.IsValid())
	{
		PendingEmptyReservation = ReservationData;
		RequestReservation(DesiredHost, RequestingPartyLeader, PartyMembers);
		RequestType = EClientRequestType::EmptyServerReservation;
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("Beacon party client. Invalid reservation request Playlist:%d"),
			ReservationData.PlaylistId);

		RequestType = EClientRequestType::EmptyServerReservation;
		OnFailure();
	}
}