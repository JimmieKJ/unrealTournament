// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineBeaconHost.h"
#include "OnlineBeaconHostObject.h"
#include "UTServerBeaconHost.generated.h"

/**
* A beacon host used for taking reservations for an existing game session
*/
UCLASS(transient, notplaceable, config = Engine)
class UNREALTOURNAMENT_API AUTServerBeaconHost : public AOnlineBeaconHostObject
{
	GENERATED_UCLASS_BODY()

	// Begin AOnlineBeaconHost Interface 
	virtual AOnlineBeaconClient* SpawnBeaconActor(class UNetConnection* ClientConnection) override;
	virtual void ClientConnected(class AOnlineBeaconClient* NewClientActor, class UNetConnection* ClientConnection);
	// End AOnlineBeaconHost Interface 

	virtual bool Init();
};
