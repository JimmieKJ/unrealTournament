// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//
// Basic beacon
//
#pragma once
#include "OnlineBeacon.generated.h"

/** Time client beacon will wait to establish a connection with the beacon host */
#define BEACON_CONNECTION_INITIAL_TIMEOUT 5.0f
/** Time client beacon will wait after establishing a connection for packets before giving up */
#define BEACON_CONNECTION_TIMEOUT 45.0f

ONLINESUBSYSTEMUTILS_API DECLARE_LOG_CATEGORY_EXTERN(LogBeacon, Display, All);

/** States that a beacon can be in */
namespace EBeaconState
{
	enum Type
	{
		AllowRequests,
		DenyRequests
	};
}

/**
 * Base class for beacon communication (Unreal Networking, but outside normal gameplay traffic)
 */
UCLASS(transient, config=Engine, notplaceable)
class ONLINESUBSYSTEMUTILS_API AOnlineBeacon : public AActor, public FNetworkNotify
{
	GENERATED_UCLASS_BODY()

	// Begin AActor Interface
	virtual void OnActorChannelOpen(class FInBunch& InBunch, class UNetConnection* Connection) override;
	// End AActor Interface

	// Begin FNetworkNotify Interface
	virtual EAcceptConnection::Type NotifyAcceptingConnection() override;
	virtual void NotifyAcceptedConnection(class UNetConnection* Connection) override;
	virtual bool NotifyAcceptingChannel(class UChannel* Channel) override;
	virtual void NotifyControlMessage(class UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch) override;
	// End FNetworkNotify Interface

	/**
	 * Each beacon must have a unique type identifier
	 *
	 * @return string representing the type of beacon 
	 */
	virtual FString GetBeaconType() PURE_VIRTUAL(AOnlineBeacon::GetBeaconType, return TEXT(""););
	
    /**
	 * Get the current state of the beacon
	 *
	 * @return state of the beacon
	 */
	EBeaconState::Type GetBeaconState() const { return BeaconState; }

	virtual void HandleNetworkFailure(class UWorld* World, class UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/**
	 * Set Beacon state 
	 *
	 * @bPause should the beacon stop accepting requests
	 */
	void PauseReservationRequests(bool bPause)
	{
		if (bPause)
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Reservation Beacon Requests Paused."));
			NetDriver->SetWorld(NULL);
			NetDriver->Notify = this;
			BeaconState = EBeaconState::DenyRequests;
		}
		else
		{
			UE_LOG(LogBeacon, Verbose, TEXT("Reservation Beacon Requests Resumed."));
			NetDriver->SetWorld(GetWorld());
			NetDriver->Notify = this;
			BeaconState = EBeaconState::AllowRequests;
		}
	}

	/** Beacon cleanup and net driver destruction */
	virtual void DestroyBeacon();

	/**
	 * Associate this beacon with a network connection
	 *
	 * @param NetConnection connection that the beacon will communicate over
	 */
	virtual void SetNetConnection(class UNetConnection* NetConnection) 
	{
		BeaconConnection = NetConnection;
	}

	/**
	 * Get the network connection associated with this beacon
	 *
	 * @return net connection used in communication
	 */
	virtual UNetConnection* GetNetConnection() override;	

protected:
	/** Net driver routing network traffic */
	UPROPERTY()
	UNetDriver* NetDriver;
	/** Name of net driver to use for communication */
	UPROPERTY(Config)
	FName BeaconNetDriverName;
	/** Network connection */
	UPROPERTY()
	UNetConnection* BeaconConnection;
	/** State of beacon */
	EBeaconState::Type BeaconState;

	/** Common initialization for all beacon types */
	bool InitBase();

	/** Notification that failure needs to be handled */
	virtual void OnFailure();

	/** overridden to return that player controllers are capable of RPCs */
	virtual bool HasNetOwner() const override;
};
