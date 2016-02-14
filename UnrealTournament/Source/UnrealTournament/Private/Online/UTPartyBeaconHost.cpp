// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "Online/UTPartyBeaconHost.h"
#include "Online/UTPartyBeaconClient.h"
#include "Online/UTPartyBeaconState.h"

AUTPartyBeaconHost::AUTPartyBeaconHost(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	UTState(NULL)
{
	ClientBeaconActorClass = AUTPartyBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();
}

TSubclassOf<UPartyBeaconState> AUTPartyBeaconHost::GetPartyBeaconHostClass() const
{
	return UUTPartyBeaconState::StaticClass();
}

bool AUTPartyBeaconHost::InitHostBeacon(int32 InTeamCount, int32 InTeamSize, int32 InMaxReservations, FName InSessionName, int32 InForceTeamNum)
{
	if (Super::InitHostBeacon(InTeamCount, InTeamSize, InMaxReservations, InSessionName, InForceTeamNum))
	{
		UTState = Cast<UUTPartyBeaconState>(State);
	}

	return UTState != NULL;
}

bool AUTPartyBeaconHost::InitFromBeaconState(UPartyBeaconState* PrevState)
{
	if (Super::InitFromBeaconState(PrevState))
	{
		UTState = Cast<UUTPartyBeaconState>(State);
	}

	return UTState != NULL;
}