// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PartyBeaconClient.h"
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
	
	/**
	 * Tell the server about the reservation request being made (new empty server configuration)
	 *
	 * @param InSessionId reference session id to make sure the session is the correct one
	 * @param ReservationData all reservation data for the request
	 * @param Reservation pending reservation request to make with server
	 */
	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerEmptyServerReservationRequest(const FString& InSessionId, const FEmptyServerReservation ReservationData, struct FPartyReservation Reservation);
	
	/**
	 * Reestablish a connection to the beacon with an existing reservation
	 *
	 * @param DesiredHost search result of the server to make the request from
	 * @param RequestingPartyLeader unique id of party leader
	 */
	void Reconnect(const FOnlineSessionSearchResult& DesiredHost, const FUniqueNetIdRepl& RequestingPartyLeader);
	
	/** @return if this beacon client has used and finished Reconnect */
	bool IsReconnected() const { return bHasReconnected; }

	/** 
	 * Mark this client beacon as having successfully reconnected
	 */
	void MarkReconnected() { bHasReconnected = true; }
	
	/** RPC alerting the client beacon that is allowed to proceed past its reservation; fires a delegate */
	UFUNCTION(client, reliable)
	void ClientAllowedToProceedFromReservation();

	/** RPC alerting the client beacon that it has timed out waiting to be allowed to proceed past its reservation; fires a delegate */
	UFUNCTION(client, reliable)
	void ClientAllowedToProceedFromReservationTimeout();

protected:

	virtual void OnConnected() override;

	/** Time beacon will wait to establish a reconnection with the beacon host */
	UPROPERTY(Config)
	float ReconnectionInitialTimeout;
	
	/** Time beacon will wait for packets after establishing a reconnection before giving up */
	UPROPERTY(Config)
	float ReconnectionTimeout;
	
	/** Has this party beacon "reconnected" via the Reconnect function */
	UPROPERTY()
	bool bHasReconnected;

	/** moving to base class */
	bool ConnectInternal(const FOnlineSessionSearchResult& DesiredHost);

	/** Pending empty reservation data */
	UPROPERTY()
	FEmptyServerReservation PendingEmptyReservation;

	/** Delegate fired when the client is allowed to proceed from its reservation */
	FOnAllowedToProceedFromReservation ProceedFromReservation;

	/** Delegate fired when the client times out waiting for permission to proceed from its reservation */
	FOnAllowedToProceedFromReservationTimeout ProceedFromReservationTimeout;
};
