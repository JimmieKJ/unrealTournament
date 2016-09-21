// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "Online/UTPartyBeaconClient.h"
#include "Online/UTPartyBeaconHost.h"
#include "OnlineSubsystemUtils.h"
#include "UTSessionHelper.h"

AUTPartyBeaconClient::AUTPartyBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
}

void AUTPartyBeaconClient::OnConnected()
{
	bool bExistingSessionRequest = PendingEmptyReservation.PlaylistId < 0;
	if ((RequestType == EClientRequestType::ExistingSessionReservation || RequestType == EClientRequestType::ReservationUpdate) && bExistingSessionRequest)
	{
		Super::OnConnected();
	}
	else if (RequestType == EClientRequestType::EmptyServerReservation)
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Beacon connection established, sending empty server reservation request."));
		ServerEmptyServerReservationRequest(DestSessionId, PendingEmptyReservation, PendingReservation);
		bPendingReservationSent = true;
	}
	else if (RequestType == EClientRequestType::Reconnect)
	{
		RequestType = EClientRequestType::ExistingSessionReservation;
		Super::OnConnected();
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("Failed to handle reservation request type %s"), ToString(RequestType));
	}
}

bool AUTPartyBeaconClient::ServerEmptyServerReservationRequest_Validate(const FString& InSessionId, const FEmptyServerReservation ReservationData, FPartyReservation Reservation)
{
	if (!InSessionId.IsEmpty() && ReservationData.IsValid() && Reservation.IsValid())
	{
		return true;
	}

	return false;
}

void AUTPartyBeaconClient::ServerEmptyServerReservationRequest_Implementation(const FString& InSessionId, const FEmptyServerReservation ReservationData, FPartyReservation Reservation)
{
	UE_LOG(LogBeacon, Verbose, TEXT("Received empty server reservation request SessionId: %s Playlist: %d"), *InSessionId, ReservationData.PlaylistId);
	AUTPartyBeaconHost* BeaconHost = Cast<AUTPartyBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		RequestType = EClientRequestType::EmptyServerReservation;
		PendingEmptyReservation = ReservationData;
		PendingReservation = Reservation;
		BeaconHost->ProcessEmptyServerReservationRequest(this, InSessionId, ReservationData, Reservation);
	}
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

bool AUTPartyBeaconClient::ConnectInternal(const FOnlineSessionSearchResult& DesiredHost)
{
	bool bSuccess = false;
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			FString ConnectInfo;
			if (SessionInt->GetResolvedConnectString(DesiredHost, BeaconPort, ConnectInfo))
			{
				FURL ConnectURL(NULL, *ConnectInfo, TRAVEL_Absolute);
				if (InitClient(ConnectURL) && DesiredHost.Session.SessionInfo.IsValid())
				{
					DestSessionId = DesiredHost.Session.SessionInfo->GetSessionId().ToString();
					bSuccess = true;
				}
				else
				{
					UE_LOG(LogBeacon, Warning, TEXT("Failure to init client beacon with %s."), *ConnectURL.ToString());
				}
			}
		}
	}

	return bSuccess;
}

void AUTPartyBeaconClient::Reconnect(const FOnlineSessionSearchResult& DesiredHost, const FUniqueNetIdRepl& RequestingPartyLeader)
{
	RequestType = EClientRequestType::Reconnect;

	if (ConnectInternal(DesiredHost))
	{
		PendingReservation.PartyLeader = RequestingPartyLeader;
		BuildPartyMembers(GetWorld(), RequestingPartyLeader, PendingReservation.PartyMembers);

		bPendingReservationSent = false;

		// Configure the net driver with the reconnection-specific timeouts
		if (NetDriver)
		{
			NetDriver->InitialConnectTimeout = ReconnectionInitialTimeout;
			NetDriver->ConnectionTimeout = ReconnectionTimeout;
		}
	}
	else
	{
		OnFailure();
	}
}

void AUTPartyBeaconClient::ClientAllowedToProceedFromReservation_Implementation()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Received allowed to proceed from reservation"));
	OnAllowedToProceedFromReservation().ExecuteIfBound();
}

void AUTPartyBeaconClient::ClientAllowedToProceedFromReservationTimeout_Implementation()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Received timeout on waiting for allowance to proceed from reservation"));
	OnAllowedToProceedFromReservationTimeout().ExecuteIfBound();
}