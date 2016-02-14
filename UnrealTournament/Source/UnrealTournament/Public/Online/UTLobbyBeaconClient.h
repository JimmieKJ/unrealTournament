// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LobbyBeaconClient.h"
#include "UTLobbyBeaconClient.generated.h"

class AUTLobbyBeaconState;

UCLASS(transient, config = Engine, notplaceable, abstract)
class AUTLobbyBeaconClient : public ALobbyBeaconClient
{
	GENERATED_UCLASS_BODY()
};
