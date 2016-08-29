// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineBeaconHost.h"
#include "OnlineBeaconHostObject.h"
#include "UTServerBeaconHost.h"
#include "UTServerBeaconLobbyHostObject.generated.h"

UCLASS(transient, notplaceable, config = Engine)
class UNREALTOURNAMENT_API AUTServerBeaconLobbyHostObject : public AUTServerBeaconHost
{
	GENERATED_UCLASS_BODY()
	virtual AOnlineBeaconClient* SpawnBeaconActor(class UNetConnection* ClientConnection) override;
	virtual void ClientConnected(class AOnlineBeaconClient* NewClientActor, class UNetConnection* ClientConnection);

};
