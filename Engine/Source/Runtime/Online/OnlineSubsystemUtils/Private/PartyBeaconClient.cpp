// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "OnlineSessionInterface.h"

APartyBeaconClient::APartyBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	bPendingReservationSent(false),
	bCancelReservation(false)
{
}

void APartyBeaconClient::RequestReservation(const FOnlineSessionSearchResult& DesiredHost, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PartyMembers)
{
	bool bSuccess = false;
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
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
					PendingReservation.PartyLeader = RequestingPartyLeader;
					PendingReservation.PartyMembers = PartyMembers;
					bPendingReservationSent = false;
					bSuccess = true;
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("RequestReservation: Failure to init client beacon with %s."), *ConnectURL.ToString());
				}
			}
		}
	}
	if (!bSuccess)
	{
		OnFailure();
	}
}

void APartyBeaconClient::CancelReservation()
{
	if (PendingReservation.PartyLeader.IsValid())
	{
		bCancelReservation = true;
		if (bPendingReservationSent)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Sending cancel reservation request."));
			ServerCancelReservationRequest(PendingReservation.PartyLeader);
		}
		else
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Reservation request never sent, no need to send cancelation request."));
			ReservationRequestComplete.ExecuteIfBound( EPartyReservationResult::ReservationRequestCanceled );
		}
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Unable to cancel reservation request with invalid party leader."));
	}
}

void APartyBeaconClient::OnConnected()
{
	if (!bCancelReservation)
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Party beacon connection established, sending join reservation request."));
		ServerReservationRequest(DestSessionId, PendingReservation);
		bPendingReservationSent = true;
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Reservation request previously canceled, aborting reservation request."));
		ReservationRequestComplete.ExecuteIfBound( EPartyReservationResult::ReservationRequestCanceled );
	}
}

bool APartyBeaconClient::ServerReservationRequest_Validate(const FString& SessionId, FPartyReservation Reservation)
{
	return !SessionId.IsEmpty() && Reservation.PartyLeader.IsValid() && Reservation.PartyMembers.Num() > 0;
}

void APartyBeaconClient::ServerReservationRequest_Implementation(const FString& SessionId, FPartyReservation Reservation)
{
	APartyBeaconHost* BeaconHost = Cast<APartyBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		PendingReservation = Reservation;
		BeaconHost->ProcessReservationRequest(this, SessionId, Reservation);
	}
}

bool APartyBeaconClient::ServerCancelReservationRequest_Validate(FUniqueNetIdRepl PartyLeader)
{
	return true;
}

void APartyBeaconClient::ServerCancelReservationRequest_Implementation(FUniqueNetIdRepl PartyLeader)
{
	APartyBeaconHost* BeaconHost = Cast<APartyBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		bCancelReservation = true;
		BeaconHost->ProcessCancelReservationRequest(this, PartyLeader);
	}
}

void APartyBeaconClient::ClientReservationResponse_Implementation(EPartyReservationResult::Type ReservationResponse)
{
	UE_LOG(LogBeacon, Verbose, TEXT("Party beacon response received %s"), EPartyReservationResult::ToString(ReservationResponse));
	ReservationRequestComplete.ExecuteIfBound(ReservationResponse);
}
