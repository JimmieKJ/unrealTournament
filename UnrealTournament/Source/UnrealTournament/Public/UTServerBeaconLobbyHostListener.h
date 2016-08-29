// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineBeaconHost.h"
#include "OnlineBeaconHostObject.h"
#include "UTServerBeaconLobbyHostObject.h"
#include "UTServerBeaconLobbyHostListener.generated.h"

UCLASS(transient, notplaceable, config = Engine)
class UNREALTOURNAMENT_API AUTServerBeaconLobbyHostListener : public AOnlineBeaconHost
{
	GENERATED_UCLASS_BODY()
};
