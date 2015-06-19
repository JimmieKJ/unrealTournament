// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineBeaconClient.h"
#include "PartyBeaconState.h"

#include "PartyBeaconClient.generated.h"

#define PARTY_BEACON_TYPE TEXT("PartyBeacon")

/**
 * Delegate triggered when a response from the party beacon host has been received
 *
 * @param ReservationResponse response from the server
 */
DECLARE_DELEGATE_OneParam(FOnReservationRequestComplete, EPartyReservationResult::Type);

/**
 * A beacon client used for making reservations with an existing game session
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API APartyBeaconClient : public AOnlineBeaconClient
{
	GENERATED_UCLASS_BODY()

	// Begin AOnlineBeacon Interface
	virtual FString GetBeaconType() override { return PARTY_BEACON_TYPE; }
	// End AOnlineBeacon Interface

	// Begin AOnlineBeaconClient Interface
	virtual void OnConnected() override;
	// End AOnlineBeaconClient Interface

	/**
	 * Sends a request to the remote host to allow the specified members to reserve space
	 * in the host's session. Note this request is async.
	 *
	 * @param DesiredHost the server that the connection will be made to
	 * @param RequestingPartyLeader the leader of this party that will be joining
	 * @param Players the list of players that want to reserve space
	 *
	 * @return true if the request able to be sent, false if it failed to send
	 */
	virtual void RequestReservation(const class FOnlineSessionSearchResult& DesiredHost, const FUniqueNetIdRepl& RequestingPartyLeader, const TArray<FPlayerReservation>& PartyMembers);

	/**
	 * Cancel an existing request to the remote host to revoke allocated space on the server.
	 * Note this request is async.
	 */
	virtual void CancelReservation();

	/**
	 * Response from the host session after making a reservation request
	 *
	 * @param ReservationResponse response from server
	 */
 	UFUNCTION(client, reliable)
	virtual void ClientReservationResponse(EPartyReservationResult::Type ReservationResponse);

	/**
	 * Delegate triggered when a response from the party beacon host has been received
	 *
	 * @return delegate to handle response from the server
	 */
	FOnReservationRequestComplete& OnReservationRequestComplete() { return ReservationRequestComplete; }

	/**
	* @return the pending reservation associated with this beacon client
	*/
	const FPartyReservation& GetPendingReservation() const { return PendingReservation; }
	
protected:

	/** Delegate for reservation request responses */
	FOnReservationRequestComplete ReservationRequestComplete;

	/** Session Id of the destination host */
	FString DestSessionId;
	/** Pending reservation that will be sent upon connection with the intended host */
	FPartyReservation PendingReservation;

	/** Has the reservation request been delivered */
	bool bPendingReservationSent;
	/** Has the reservation request been canceled */
	bool bCancelReservation;

	/**
	 * Tell the server about the reservation request being made
	 *
	 * @param Reservation pending reservation request to make with server
	 */
	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerReservationRequest(const FString& SessionId, struct FPartyReservation Reservation);

	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerCancelReservationRequest(struct FUniqueNetIdRepl PartyLeader);
};
