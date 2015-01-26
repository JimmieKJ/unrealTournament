// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

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

void AUTServerBeaconClient::ClientPing_Implementation(const FServerBeaconInfo ServerInfo, int32 InstanceCount)
{
	Ping = (GetWorld()->RealTimeSeconds - PingStartTime) * 1000.0f;
	UE_LOG(LogBeacon, Log, TEXT("Ping %f"), Ping);

	HostServerInfo = ServerInfo;

	if (InstanceCount > 0)
	{
		ServerSendInstances(-1);
	}
	else
	{
		OnServerRequestResults.ExecuteIfBound(this, HostServerInfo);
	}
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

		ServerInfo.MOTD = GameState->ServerMOTD;
	}

	AUTGameMode* GameMode = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode)
	{
		ServerInfo.ServerRules = TEXT("");
		GameMode->BuildServerResponseRules(ServerInfo.ServerRules);	
	}

	int32 InstanceCount = GameMode->GetInstanceData(InstanceHostNames, InstanceDescriptions);
	
	ClientPing(ServerInfo, InstanceCount);
}

void AUTServerBeaconClient::SetBeaconNetDriverName(FString InBeaconName)
{
	BeaconNetDriverName = FName(*InBeaconName);
	NetDriverName = BeaconNetDriverName;
}

bool AUTServerBeaconClient::ServerSendInstances_Validate(int32 LastInstanceIndex) { return true; }
void AUTServerBeaconClient::ServerSendInstances_Implementation(int32 LastInstanceIndex)
{
	LastInstanceIndex++;
	
	if (LastInstanceIndex < InstanceHostNames.Num() && LastInstanceIndex < InstanceDescriptions.Num() )
	{
		ClientRecieveInstance_Implementation(LastInstanceIndex, InstanceHostNames.Num(), InstanceHostNames[LastInstanceIndex], InstanceDescriptions[LastInstanceIndex]);
	}

	ClientRecieveInstance_Implementation(-1, InstanceHostNames.Num(), TEXT(""), TEXT(""));
}

void AUTServerBeaconClient::ClientRecieveInstance_Implementation(uint32 InstanceCount, uint32 TotalInstances, const FString& InstanceHostName, const FString& InstanceDescription)
{
	if (InstanceCount >= 0 && InstanceCount < TotalInstances)
	{
		InstanceHostNames.Add(InstanceHostName);
		InstanceDescriptions.Add(InstanceDescription);
	}
	else
	{
		OnServerRequestResults.ExecuteIfBound(this, HostServerInfo);		
	}
}
