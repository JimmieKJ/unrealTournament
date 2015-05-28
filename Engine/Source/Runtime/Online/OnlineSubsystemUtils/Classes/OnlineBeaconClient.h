// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "TimerManager.h"
#include "Runtime/Online/OnlineSubsystemUtils/Classes/OnlineBeacon.h"
#include "OnlineBeaconClient.generated.h"

class AOnlineBeaconHost;
class AOnlineBeaconHostObject;
class UNetConnection;
class FInBunch;

/**
 * Delegate triggered on failures to connect to a host beacon
 */
DECLARE_DELEGATE(FOnHostConnectionFailure);

/**
 * Base class for any unique beacon connectivity, paired with an AOnlineBeaconHostObject implementation 
 *
 * This is the actual actor that replicates across client/server and where all RPCs occur
 * On the host, the life cycle is managed by an AOnlineBeaconHostObject
 * On the client, the life cycle is managed by the game 
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API AOnlineBeaconClient : public AOnlineBeacon
{
	GENERATED_UCLASS_BODY()

	// Begin AActor Interface
	void OnNetCleanup(UNetConnection* Connection) override;
	virtual UNetConnection* GetNetConnection() const override;
	// End AActor Interface

	// Begin FNetworkNotify Interface
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override;
	// End FNetworkNotify Interface

	// Begin OnlineBeacon Interface
	virtual void OnFailure() override;
	virtual void DestroyBeacon() override;
	// End OnlineBeacon Interface

	/**
	 * Initialize the client beacon with connection endpoint
	 *	Creates the net driver and attempts to connect with the destination
	 *
	 * @param URL destination
	 *
	 * @return true if connection is being attempted, false otherwise
	 */
	bool InitClient(FURL& URL);
	
	/**
	 * A connection has been made and RPC/replication can begin
	 */
	virtual void OnConnected() {};

	/**
	 * Delegate triggered on failures to connect to a host beacon
	 */
	FOnHostConnectionFailure& OnHostConnectionFailure() { return HostConnectionFailure; }

	/**
	 * Get the owner of this beacon actor, some host that is listening for connections
	 * (server side only, clients have no access)
	 *
	 * @return owning host of this actor
	 */
	AOnlineBeaconHostObject* GetBeaconOwner() const;
	
	/**
	 * Set the owner of this beacon actor, some host that is listening for connections
	 * (server side only, clients have no access)
	 *
	 * @return owning host of this actor
	 */
	void SetBeaconOwner(AOnlineBeaconHostObject* InBeaconOwner);

	/**
	 * Associate this beacon with a network connection
	 *
	 * @param NetConnection connection that the beacon will communicate over
	 */
	virtual void SetNetConnection(UNetConnection* NetConnection)
	{
		BeaconConnection = NetConnection;
	}

protected:

	/** Owning beacon host of this beacon actor */
	UPROPERTY()
	AOnlineBeaconHostObject* BeaconOwner;

	/** Network connection associated with this beacon client instance */
	UPROPERTY()
	UNetConnection* BeaconConnection;

	/** Delegate for host beacon connection failures */
	FOnHostConnectionFailure HostConnectionFailure;

	/** Handle for efficient management of OnFailure timer */
	FTimerHandle TimerHandle_OnFailure;

private:

	/**
	 * Called on the server side to open up the actor channel that will allow RPCs to occur
	 */
	UFUNCTION(client, reliable)
	virtual void ClientOnConnected();

	friend class AOnlineBeaconHost;
	friend class AOnlineBeaconHostObject;
};