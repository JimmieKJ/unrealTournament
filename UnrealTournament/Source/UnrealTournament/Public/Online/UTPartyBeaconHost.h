// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Online/OnlineSubsystemUtils/Public/PartyBeaconHost.h"
#include "PartyBeaconState.h"
#include "UTPartyBeaconState.h"
#include "UTPartyBeaconHost.generated.h"

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS(transient, config = Engine)
class AUTPartyBeaconHost : public APartyBeaconHost
{
	GENERATED_UCLASS_BODY()

	virtual bool InitHostBeacon(int32 InTeamCount, int32 InTeamSize, int32 InMaxReservations, FName InSessionName, int32 InForceTeamNum = 0) override;
	virtual bool InitFromBeaconState(UPartyBeaconState* PrevState) override;

protected:

	// Begin APartyBeaconHost Interface 
	virtual TSubclassOf<UPartyBeaconState> GetPartyBeaconHostClass() const override;
	// End APartyBeaconHost Interface 

	/** Cached version of the host state */
	UUTPartyBeaconState* UTState;
};