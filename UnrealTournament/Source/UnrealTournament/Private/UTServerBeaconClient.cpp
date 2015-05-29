// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTServerBeaconClient.h"

AUTServerBeaconClient::AUTServerBeaconClient(const class FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
	PingStartTime = -1;
}

void AUTServerBeaconClient::OnConnected()
{
	Super::OnConnected();

	UE_LOG(LogBeacon, Verbose, TEXT("---> PING"));

	// Tell the server that we want to ping
	PingStartTime = GetWorld()->RealTimeSeconds;
	ServerPing();
}

void AUTServerBeaconClient::OnFailure()
{
	UE_LOG(LogBeacon, Verbose, TEXT("UTServer beacon connection failure, handling connection timeout."));
	OnServerRequestFailure.ExecuteIfBound(this);
	Super::OnFailure();
	PingStartTime = -2;
}

bool AUTServerBeaconClient::ServerPing_Validate()
{
	return true;
}

void AUTServerBeaconClient::ServerPing_Implementation()
{
	UE_LOG(LogBeacon, Verbose, TEXT("<--- PONG"));
	// Respond to the client
	ClientPong();
}

void AUTServerBeaconClient::ClientPong_Implementation()
{
	Ping = (GetWorld()->RealTimeSeconds - PingStartTime) * 1000.0f;

	UE_LOG(LogBeacon, Verbose, TEXT("---> Requesting Info %f"), Ping);

	// Ask for additional server info
	ServerRequestInfo();
}

bool AUTServerBeaconClient::ServerRequestInfo_Validate() { return true; }
void AUTServerBeaconClient::ServerRequestInfo_Implementation()
{
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

		ServerInfo.MOTD = GameState->ServerMOTD;
		ServerInfo.CurrentMap = GetWorld()->GetMapName();
	}

	AUTGameMode* GameMode = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode)
	{
		ServerInfo.ServerRules = TEXT("");
		GameMode->BuildServerResponseRules(ServerInfo.ServerRules);	
	}

	int32 NumInstances = 0;
	AUTBaseGameMode* BaseGame = Cast<AUTBaseGameMode>(GetWorld()->GetAuthGameMode());
	if (BaseGame)
	{
		NumInstances = BaseGame->GetInstanceData(InstanceHostNames, InstanceDescriptions);
	}

	UE_LOG(LogBeacon, Verbose, TEXT("<--- Sending Info %i"), NumInstances);
	ClientReceiveInfo(ServerInfo, NumInstances);
}

void AUTServerBeaconClient::ClientReceiveInfo_Implementation(const FServerBeaconInfo ServerInfo, int32 NumInstances)
{
	HostServerInfo = ServerInfo;
	InstanceCount = NumInstances;

	if (NumInstances > 0)
	{
		UE_LOG(LogBeacon, Verbose,  TEXT("---> Received Info [%i] Requesting Instance Data"), NumInstances);
		ServerRequestInstances(-1);
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("---> Received Info [%i] DONE!!!!"), NumInstances);
		OnServerRequestResults.ExecuteIfBound(this, HostServerInfo);
	}
}

bool AUTServerBeaconClient::ServerRequestInstances_Validate(int32 LastInstanceIndex) { return true; }
void AUTServerBeaconClient::ServerRequestInstances_Implementation(int32 LastInstanceIndex)
{
	LastInstanceIndex++;

	UE_LOG(LogBeacon, Verbose,TEXT("ServerRequestInstances %i/%i %i"), InstanceHostNames.Num(), InstanceDescriptions.Num(), LastInstanceIndex);
	if (LastInstanceIndex < InstanceHostNames.Num() && LastInstanceIndex < InstanceDescriptions.Num() )
	{
		UE_LOG(LogBeacon, Verbose, TEXT("<--- Sending Instance [%i]"), LastInstanceIndex);
		ClientReceiveInstance(LastInstanceIndex, InstanceHostNames.Num(), InstanceHostNames[LastInstanceIndex], InstanceDescriptions[LastInstanceIndex]);
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("<--- Out of Instances [%i] %i"), LastInstanceIndex, InstanceHostNames.Num());
		ClientReceivedAllInstance(InstanceHostNames.Num());
	}
}

void AUTServerBeaconClient::ClientReceiveInstance_Implementation(uint32 InInstanceCount, uint32 TotalInstances, const FString& InstanceHostName, const FString& InstanceDescription)
{
	UE_LOG(LogBeacon, Verbose, TEXT("---> Received Instance [%i] Host [%s] Desc [%s]"), InInstanceCount, *InstanceHostName, *InstanceDescription);
	if (InInstanceCount >= 0 && InInstanceCount < TotalInstances)
	{
		InstanceHostNames.Add(InstanceHostName);
		InstanceDescriptions.Add(InstanceDescription);
	}

	ServerRequestInstances(InInstanceCount);
}

void AUTServerBeaconClient::ClientReceivedAllInstance_Implementation(uint32 FinalCount)
{
	if (InstanceHostNames.Num() != InstanceDescriptions.Num() || InstanceHostNames.Num() != FinalCount)
	{
		UE_LOG(UT, Log, TEXT("ERROR: Instance Names/Descriptions doesn't meet the final size requirement: %i/%i vs %i"), InstanceHostNames.Num(), InstanceDescriptions.Num(), FinalCount);
	}

	UE_LOG(LogBeacon, Verbose, TEXT("---> Got them All DONE!!!!  [%i vs %i]"), InstanceHostNames.Num(), FinalCount );

	OnServerRequestResults.ExecuteIfBound(this, HostServerInfo);		
}
