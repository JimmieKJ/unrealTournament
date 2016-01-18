// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTServerBeaconLobbyHostListener.h"
#include "UTServerBeaconLobbyHostObject.h"
#include "UTServerBeaconClient.h"
#include "UTServerBeaconLobbyClient.h"

AUTServerBeaconLobbyHostListener::AUTServerBeaconLobbyHostListener(const FObjectInitializer& PCIP) :
Super(PCIP)
{
	NetDriverName = FName(TEXT("LobbyBeaconDriver"));
}
