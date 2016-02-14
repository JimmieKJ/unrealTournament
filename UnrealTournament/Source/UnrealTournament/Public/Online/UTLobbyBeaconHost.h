// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LobbyBeaconHost.h"
#include "UTLobbyBeaconClient.h"
#include "UTLobbyBeaconHost.generated.h"

UCLASS(transient, config = Engine, notplaceable, abstract)
class AUTLobbyBeaconHost : public ALobbyBeaconHost
{
	GENERATED_UCLASS_BODY()
	
	/** Whether to allow beacon reservations to advance to register/login to the lobby or not */
	UPROPERTY(Transient)
	uint32 bAllowReservationsToProceedToLobby : 1;
	
	/** The reservation count that was present the last time the permission timer was modified */
	UPROPERTY()
	int32 LastReservationCountForPermissionTimeoutChange;

	/** If > 0.f, the amount of time to allow clients to wait for permission to proceed to the lobby before booting them out */
	UPROPERTY()
	float LobbyPermissionTimeout;

	/** Timer handle for the permission timer */
	FTimerHandle LobbyPermissionTimeoutHandle;
};