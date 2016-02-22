// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Online/OnlineSubsystemUtils/Public/PartyBeaconHost.h"
#include "PartyBeaconState.h"
#include "UTPartyBeaconState.h"
#include "UTPartyBeaconHost.generated.h"

class AUTPartyBeaconClient;

/**
 * Delegate called when an unconfigured beacon gets a configuration request.  
 * All normal reservations are rejected until a configuration attempt has been made
 * 
 * @param GameSessionOwner player making request who will be considered the owner for all authoritative requests
 * @param ReservationData server configuration parameters
 */
DECLARE_DELEGATE_TwoParams(FOnServerConfigurationRequest, const FUniqueNetIdRepl&, const FEmptyServerReservation&);

/**
 * Delegated called when the beacon host processes a reconnect attempt for a beacon client
 * 
 * @param	Client being processed
 * @param	Reservation result
 */
DECLARE_DELEGATE_TwoParams(FOnProcessReconnectForClient, AUTPartyBeaconClient*, EPartyReservationResult::Type);

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS(transient, config = Engine)
class AUTPartyBeaconHost : public APartyBeaconHost
{
	GENERATED_UCLASS_BODY()

	virtual bool InitHostBeacon(int32 InTeamCount, int32 InTeamSize, int32 InMaxReservations, FName InSessionName, int32 InForceTeamNum = 0) override;
	virtual bool InitFromBeaconState(UPartyBeaconState* PrevState) override;
	virtual void ProcessReservationRequest(APartyBeaconClient* Client, const FString& SessionId, const FPartyReservation& ReservationRequest) override;
	virtual void HandlePlayerLogout(const FUniqueNetIdRepl& PlayerId) override;
	virtual void DumpReservations() const override;
	
	/**
	 * Simple accessor for the delegate fired when a beacon client's reconnect request is processed
	 */
	FOnProcessReconnectForClient& OnProcessReconnectForClient() { return ProcessReconnectForClient; }
	
	/**
	 * Delegate called when an unconfigured beacon gets a configuration request.  
	 * All normal reservations are rejected until a configuration attempt has been made
	 */
	FOnServerConfigurationRequest& OnServerConfigurationRequest() { return ServerConfigurationRequest; }
	
	/**
	 * Attempts to add a party reservation to the beacon, configuring the beacon in the process
     *
	 * @param ReservationData all reservation data for the request
     * @param ReservationRequest reservation attempt
     *
     * @return add attempt result
	 */
	EPartyReservationResult::Type AddEmptyServerReservationRequest(const FEmptyServerReservation& ReservationData, const FPartyReservation& ReservationRequest);

	/**
	 * Handle a reservation request received from an incoming client (configure new server)
	 *
	 * @param Client client beacon making the request
	 * @param SessionId id of the session to join
	 * @param ReservationData all reservation data for the request
	 * @param ReservationRequest payload of request
	 */
	void ProcessEmptyServerReservationRequest(APartyBeaconClient* Client, const FString& SessionId, const FEmptyServerReservation& ReservationData, const FPartyReservation& ReservationRequest);

	/**
	 * @return the index of the game mode in use
	 */
	const int32 GetPlaylistId() const { return UTState ? UTState->GetPlaylistId() : INDEX_NONE; }

protected:

	// Begin APartyBeaconHost Interface 
	virtual TSubclassOf<UPartyBeaconState> GetPartyBeaconHostClass() const override;
	// End APartyBeaconHost Interface 

	/** Cached version of the host state */
	UUTPartyBeaconState* UTState;

	/** Delegate triggered when processing a reconnect request for a client */
	FOnProcessReconnectForClient ProcessReconnectForClient;

	/** Delegate triggered when processing a configuration request */
	FOnServerConfigurationRequest ServerConfigurationRequest;
};