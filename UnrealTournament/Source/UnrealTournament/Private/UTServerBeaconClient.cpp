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
	OnServerRequestFailure.ExecuteIfBound(this);
	Super::OnFailure();
	PingStartTime = -2;
}

void AUTServerBeaconClient::ClientPing_Implementation(const FServerBeaconInfo ServerInfo)
{
	Ping = (GetWorld()->RealTimeSeconds - PingStartTime) * 1000.0f;
	UE_LOG(LogBeacon, Log, TEXT("Ping %f"), Ping);

	OnServerRequestResults.ExecuteIfBound(this, ServerInfo);
}

bool AUTServerBeaconClient::ServerPong_Validate()
{
	return true;
}

void AUTServerBeaconClient::ServerPong_Implementation()
{
	UE_LOG(LogBeacon, Log, TEXT("Pong"));

	FServerBeaconInfo ServerInfo;
	
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		ServerInfo.ServerPlayers = TEXT("");

		// Add the Players section3
		for (int32 i=0; i < GameState->PlayerArray.Num(); i++)
		{
			FString PlayerName =  GameState->PlayerArray[i]->PlayerName;
			FString PlayerScore = FString::Printf(TEXT("%i"), int32(GameState->PlayerArray[i]->Score));
			FString UniqueID = GameState->PlayerArray[i]->UniqueId.IsValid() ? GameState->PlayerArray[i]->UniqueId->ToString() : TEXT("none");
			ServerInfo.ServerPlayers += FString::Printf(TEXT("%s\t%s\t%s\t"), *PlayerName, *PlayerScore, *UniqueID);
		}
	}

	AUTGameMode* GameMode = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode)
	{
		ServerInfo.ServerRules = TEXT("");
		GameMode->BuildServerResponseRules(ServerInfo.ServerRules);	
	}

	ClientPing(ServerInfo);
}

void AUTServerBeaconClient::SetBeaconNetDriverName(FString InBeaconName)
{
	BeaconNetDriverName = FName(*InBeaconName);
	NetDriverName = BeaconNetDriverName;
}