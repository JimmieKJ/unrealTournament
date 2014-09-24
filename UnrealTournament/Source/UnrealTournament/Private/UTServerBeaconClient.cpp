// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTServerBeaconClient.h"

AUTServerBeaconClient::AUTServerBeaconClient(const class FPostConstructInitializeProperties& PCIP) :
Super(PCIP)
{
	PingStartTime = -1;
}

void AUTServerBeaconClient::OnConnected()
{
	Super::OnConnected();

	// Tell the server that we want to ping
	PingStartTime = GetWorld()->RealTimeSeconds;
	ServerPong();
}

void AUTServerBeaconClient::OnFailure()
{
	UE_LOG(LogBeacon, Verbose, TEXT("UTServer beacon connection failure, handling connection timeout."));

	Super::OnFailure();
	PingStartTime = -2;
}

void AUTServerBeaconClient::ClientPing_Implementation()
{
	UE_LOG(LogBeacon, Log, TEXT("Ping"));
	Ping = GetWorld()->RealTimeSeconds - PingStartTime;
}

bool AUTServerBeaconClient::ServerPong_Validate()
{
	return true;
}

void AUTServerBeaconClient::ServerPong_Implementation()
{
	UE_LOG(LogBeacon, Log, TEXT("Pong"));
	ClientPing();
}
