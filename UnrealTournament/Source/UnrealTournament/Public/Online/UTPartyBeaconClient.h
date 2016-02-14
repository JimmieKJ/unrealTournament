// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Online/OnlineSubsystemUtils/Public/PartyBeaconClient.h"
#include "UTPartyBeaconState.h"
#include "UTPartyBeaconClient.generated.h"


/** Delegate fired when a client is told it is allowed to proceed past a reservation (into a lobby) */
DECLARE_DELEGATE(FOnAllowedToProceedFromReservation);

/** Delegate fired when a client times out waiting for permission to proceed past a reservation (into a lobby) */
DECLARE_DELEGATE(FOnAllowedToProceedFromReservationTimeout);

/**
 * A beacon client used for making reservations with an existing game session
 */
UCLASS(transient, config = Engine)
class AUTPartyBeaconClient : public APartyBeaconClient
{
	GENERATED_UCLASS_BODY()

public:

	/** Simple accessor for the delegate fired when the client is allowed to proceed from its reservation */
	FOnAllowedToProceedFromReservation& OnAllowedToProceedFromReservation() { return ProceedFromReservation; }

	/** Simple accessor for the delegate fired when the client times out waiting for permission to proceed from its reservation */
	FOnAllowedToProceedFromReservationTimeout& OnAllowedToProceedFromReservationTimeout() { return ProceedFromReservationTimeout; }
	
	/**
	 * Initiate a reservation request to configure an empty server
	 *
	 * @param DesiredHost search result of the server to make the request from
	 * @param ReservationData all reservation data for the request
	 * @param RequestingPartyLeader unique id of party leader
	 * @param PartyMembers array of party members, including party leader
	 */
	virtual void EmptyServerReservationRequest(const FOnlineSessionSearchResult& DesiredHost, const FEmptyServerReservation& ReservationData, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PartyMembers);

protected:
	
	/** Pending empty reservation data */
	UPROPERTY()
	FEmptyServerReservation PendingEmptyReservation;

	/** Delegate fired when the client is allowed to proceed from its reservation */
	FOnAllowedToProceedFromReservation ProceedFromReservation;

	/** Delegate fired when the client times out waiting for permission to proceed from its reservation */
	FOnAllowedToProceedFromReservationTimeout ProceedFromReservationTimeout;
};
