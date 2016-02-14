// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLobbyBeaconClient.h"
#include "UTLobbyBeaconState.h"
#include "UTLobbyBeaconHost.h"

AUTLobbyBeaconHost::AUTLobbyBeaconHost(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAllowReservationsToProceedToLobby(false)
	, LastReservationCountForPermissionTimeoutChange(0)
	, LobbyPermissionTimeout(0.f)
{
	ClientBeaconActorClass = AUTLobbyBeaconClient::StaticClass();
	LobbyStateClass = AUTLobbyBeaconState::StaticClass();
}