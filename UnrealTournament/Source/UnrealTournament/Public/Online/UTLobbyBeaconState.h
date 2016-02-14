// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LobbyBeaconState.h"
#include "UTLobbyBeaconState.generated.h"

/**
 * Shared state of the game from the lobby perspective
 * Duplicates much of the data in the traditional AGameState object for sharing with players
 * connected via beacon only
 */
UCLASS(transient, config = Game, notplaceable, abstract)
class UNREALTOURNAMENT_API AUTLobbyBeaconState : public ALobbyBeaconState
{
	GENERATED_UCLASS_BODY()
};