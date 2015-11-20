// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Runtime/Online/OnlineSubsystemUtils/Public/OnlineBeaconHost.h"
#include "Runtime/Online/OnlineSubsystemUtils/Public/OnlineBeaconHostObject.h"
#include "UTServerBeaconLobbyHostObject.h"
#include "UTServerBeaconLobbyHostListener.generated.h"

UCLASS(transient, notplaceable, config = Engine)
class UNREALTOURNAMENT_API AUTServerBeaconLobbyHostListener : public AOnlineBeaconHost
{
	GENERATED_UCLASS_BODY()
};
