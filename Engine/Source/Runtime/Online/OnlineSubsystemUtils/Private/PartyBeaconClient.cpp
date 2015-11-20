// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "PartyBeaconClient.h"
#include "PartyBeaconHost.h"
#include "OnlineSessionInterface.h"

APartyBeaconClient::APartyBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	RequestType(EClientRequestType::NonePending),
	bPendingReservationSent(false),
	bCancelReservation(false)
{
}

bool APartyBeaconClient::RequestReservation(const FString& ConnectInfoStr, const FString& InSessionId, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PartyMembers)
{
	bool bSuccess = false;

	FURL ConnectURL(NULL, *ConnectInfoStr, TRAVEL_Absolute);
	if (InitClient(ConnectURL))
	{
		DestSessionId = InSessionId;
		PendingReservation.PartyLeader = RequestingPartyLeader;
		PendingReservation.PartyMembers = PartyMembers;
		bPendingReservationSent = false;
		RequestType = EClientRequestType::ExistingSessionReservation;
		bSuccess = true;
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("RequestReservation: Failure to init client beacon with %s."), *ConnectURL.ToString());
		RequestType = EClientRequestType::NonePending;
	}

	if (!bSuccess)
	{
		OnFailure();
	}

	return bSuccess;
}

bool APartyBeaconClient::RequestReservation(const FOnlineSessionSearchResult& DesiredHost, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PartyMembers)
{
	bool bSuccess = false;

	if (DesiredHost.IsValid())
	{
		UWorld* World = GetWorld();

		IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World);
		if (OnlineSub)
		{
			IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
			if (SessionInt.IsValid())
			{
				FString ConnectInfo;
				if (SessionInt->GetResolvedConnectString(DesiredHost, BeaconPort, ConnectInfo))
				{
					FString SessionId = DesiredHost.Session.SessionInfo->GetSessionId().ToString();
					return RequestReservation(ConnectInfo, SessionId, RequestingPartyLeader, PartyMembers);
				}
			}
		}
	}

	if (!bSuccess)
	{
		OnFailure();
	}

	return bSuccess;
}

bool APartyBeaconClient::RequestReservationUpdate(const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PlayersToAdd)
{
	bool bWasStarted = false;

	EBeaconConnectionState ConnectionState = GetConnectionState();
	if (ensure(ConnectionState == EBeaconConnectionState::Open))
	{
		RequestType = EClientRequestType::ReservationUpdate;
		PendingReservation.PartyLeader = RequestingPartyLeader;
		PendingReservation.PartyMembers = PlayersToAdd;
		ServerUpdateReservationRequest(DestSessionId, PendingReservation);
		bPendingReservationSent = true;
		bWasStarted = true;
	}

	return bWasStarted;
}

bool APartyBeaconClient::RequestReservationUpdate(const FString& ConnectInfoStr, const FString& InSessionId, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PlayersToAdd)
{
	bool bWasStarted = false;

	EBeaconConnectionState ConnectionState = GetConnectionState();
	if (ConnectionState != EBeaconConnectionState::Open)
	{
		// create a new pending reservation for these players in the same way as a new reservation request
		bWasStarted = RequestReservation(ConnectInfoStr, InSessionId, RequestingPartyLeader, PlayersToAdd);
		if (bWasStarted)
		{
			// Treat this reservation as an update to an existing reservation on the host
			RequestType = EClientRequestType::ReservationUpdate;
		}
	}
	else if (ConnectionState == EBeaconConnectionState::Open)
	{
		RequestReservationUpdate(RequestingPartyLeader, PlayersToAdd);
	}

	return bWasStarted;
}

bool APartyBeaconClient::RequestReservationUpdate(const FOnlineSessionSearchResult& DesiredHost, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PlayersToAdd)
{
	bool bWasStarted = false;

	EBeaconConnectionState ConnectionState = GetConnectionState();
	if (ConnectionState != EBeaconConnectionState::Open)
	{
		// create a new pending reservation for these players in the same way as a new reservation request
		bWasStarted = RequestReservation(DesiredHost, RequestingPartyLeader, PlayersToAdd);
		if (bWasStarted)
		{
			// Treat this reservation as an update to an existing reservation on the host
			RequestType = EClientRequestType::ReservationUpdate;
		}
	}
	else if (ConnectionState == EBeaconConnectionState::Open)
	{
		RequestReservationUpdate(RequestingPartyLeader, PlayersToAdd);
	}

	return bWasStarted;
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

	RequestType = EClientRequestType::NonePending;
}

void APartyBeaconClient::OnConnected()
{
	if (!bCancelReservation)
	{
		if (RequestType == EClientRequestType::ExistingSessionReservation)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Party beacon connection established, sending join reservation request."));
			ServerReservationRequest(DestSessionId, PendingReservation);
			bPendingReservationSent = true;
		}
		else if (RequestType == EClientRequestType::ReservationUpdate)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Party beacon connection established, sending reservation update request."));
			ServerUpdateReservationRequest(DestSessionId, PendingReservation);
			bPendingReservationSent = true;
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("Failed to handle reservation request type %s"), ToString(RequestType));
			OnFailure();
		}
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("Reservation request previously canceled, aborting reservation request."));
		ReservationRequestComplete.ExecuteIfBound(EPartyReservationResult::ReservationRequestCanceled);
		RequestType = EClientRequestType::NonePending;
	}
}

void APartyBeaconClient::OnFailure()
{
	RequestType = EClientRequestType::NonePending;
	Super::OnFailure();
}

bool APartyBeaconClient::ServerReservationRequest_Validate(const FString& SessionId, const FPartyReservation& Reservation)
{
	return !SessionId.IsEmpty() && Reservation.PartyLeader.IsValid() && Reservation.PartyMembers.Num() > 0;
}

void APartyBeaconClient::ServerReservationRequest_Implementation(const FString& SessionId, const FPartyReservation& Reservation)
{
	APartyBeaconHost* BeaconHost = Cast<APartyBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		PendingReservation = Reservation;
		RequestType = EClientRequestType::ExistingSessionReservation;
		BeaconHost->ProcessReservationRequest(this, SessionId, Reservation);
	}
}

bool APartyBeaconClient::ServerUpdateReservationRequest_Validate(const FString& SessionId, const FPartyReservation& ReservationUpdate)
{
	return !SessionId.IsEmpty() && ReservationUpdate.PartyLeader.IsValid() && ReservationUpdate.PartyMembers.Num() > 0;
}

void APartyBeaconClient::ServerUpdateReservationRequest_Implementation(const FString& SessionId, const FPartyReservation& ReservationUpdate)
{
	APartyBeaconHost* BeaconHost = Cast<APartyBeaconHost>(GetBeaconOwner());
	if (BeaconHost)
	{
		PendingReservation = ReservationUpdate;
		RequestType = EClientRequestType::ReservationUpdate;
		BeaconHost->ProcessReservationUpdateRequest(this, SessionId, ReservationUpdate);
	}
}

bool APartyBeaconClient::ServerCancelReservationRequest_Validate(const FUniqueNetIdRepl& PartyLeader)
{
	return true;
}

void APartyBeaconClient::ServerCancelReservationRequest_Implementation(const FUniqueNetIdRepl& PartyLeader)
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
	RequestType = EClientRequestType::NonePending;
}

void APartyBeaconClient::ClientSendReservationUpdates_Implementation(int32 NumRemainingReservations)
{
	UE_LOG(LogBeacon, Verbose, TEXT("Party beacon reservations remaining %d"), NumRemainingReservations);
	ReservationCountUpdate.ExecuteIfBound(NumRemainingReservations);
}

void APartyBeaconClient::ClientSendReservationFull_Implementation()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Party beacon reservations full"));
	ReservationFull.ExecuteIfBound();
}
