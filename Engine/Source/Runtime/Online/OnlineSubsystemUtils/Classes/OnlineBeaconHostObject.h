// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Runtime/Online/OnlineSubsystemUtils/Classes/OnlineBeacon.h"
#include "OnlineBeaconHostObject.generated.h"

class AOnlineBeaconClient;
class UNetConnection;

/**
 * Base class for any unique beacon connectivity, paired with an AOnlineBeaconClient implementation 
 *
 * By defining a beacon type and implementing the ability to spawn unique AOnlineBeaconClients, any two instances of the engine
 * can communicate with each other without officially connecting through normal Unreal networking
 */
UCLASS(transient, config=Engine, notplaceable)
class ONLINESUBSYSTEMUTILS_API AOnlineBeaconHostObject : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Custom name for this beacon */
	UPROPERTY(Transient)
	FString BeaconTypeName;

	/** Get the state of the beacon (accepting/rejecting connections) */
	EBeaconState::Type GetBeaconState() const;

	/** Get the type of beacon implemented */
	const FString& GetBeaconType() const { return BeaconTypeName; }

	/**
	 * Each beacon host must be able to spawn the appropriate client beacon actor to communicate with the initiating client
	 *
	 * @return new client beacon actor that this beacon host knows how to communicate with
	 */
	virtual AOnlineBeaconClient* SpawnBeaconActor(UNetConnection* ClientConnection) PURE_VIRTUAL(AOnlineBeaconHostObject::SpawnBeaconActor, return NULL;);

	/**
	 * Delegate triggered when a new client connection is made
	 *
	 * @param NewClientActor new client beacon actor
	 * @param ClientConnection new connection established
	 */
	virtual void ClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection) PURE_VIRTUAL(AOnlineBeaconHostObject::ClientConnected, );

	/**
	 * Remove a client beacon actor from the list of active connections
	 *
	 * @param ClientActor client beacon actor to remove
	 */
	virtual void RemoveClientActor(AOnlineBeaconClient* ClientActor);
};