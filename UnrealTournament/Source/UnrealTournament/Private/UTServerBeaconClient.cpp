// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLobbyGameState.h"
#include "UTLobbyMatchInfo.h"
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
		BaseGame->GetInstanceData(Instances);
	}

	UE_LOG(LogBeacon, Verbose, TEXT("<--- Sending Info %i"), Instances.Num());
	ClientReceiveInfo(ServerInfo, Instances.Num());
}

void AUTServerBeaconClient::ClientReceiveInfo_Implementation(const FServerBeaconInfo ServerInfo, int32 NumInstances)
{
	HostServerInfo = ServerInfo;
	ExpectedInstanceCount = NumInstances;

	if (ExpectedInstanceCount > 0)
	{
		UE_LOG(LogBeacon, Verbose,  TEXT("---> Received Info [%i] Requesting Instance Data"), NumInstances);
		ServerRequestNextInstance(-1);
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("---> Received Info [%i] DONE!!!!"), NumInstances);
		OnServerRequestResults.ExecuteIfBound(this, HostServerInfo);
	}
}

bool AUTServerBeaconClient::ServerRequestNextInstance_Validate(int32 LastInstanceIndex) { return true; }
void AUTServerBeaconClient::ServerRequestNextInstance_Implementation(int32 LastInstanceIndex)
{
	LastInstanceIndex++;
	UE_LOG(LogBeacon, Verbose,TEXT("ServerRequestInstances %i %i"), Instances.Num(), LastInstanceIndex);

	if ( LastInstanceIndex >= 0 && LastInstanceIndex < Instances.Num() )
	{
		UE_LOG(LogBeacon, Verbose, TEXT("<--- Sending Instance [%i]"), LastInstanceIndex);

		TSharedPtr<FServerInstanceData> Instance = Instances[LastInstanceIndex];
		ClientReceiveInstance(LastInstanceIndex, Instances.Num(), Instance->InstanceId, Instance->RulesTitle, Instance->MapName, Instance->NumPlayers, Instance->MaxPlayers, Instance->NumFriends, Instance->Flags, Instance->Rank, Instance->bTeamGame);
	}
	else
	{
		UE_LOG(LogBeacon, Verbose, TEXT("<--- Out of Instances [%i] %i"), LastInstanceIndex, Instances.Num());

		ClientReceivedAllInstance(Instances.Num());
	}
}

void AUTServerBeaconClient::ClientReceiveInstance_Implementation(int32 InInstanceCount, int32 TotalInstances, FGuid InstanceId, const FString& InstanceRuleName, const FString& InstanceMap, int32 InstanceNumPlayers, int32 InstanceMaxPlayers, int32 InstanceNumFriends, uint32 InstanceFlags, int32 InstanceRank, bool bTeamGame)
{
	UE_LOG(LogBeacon, Verbose, TEXT("---> Received Instance [%i] Rule Icon[%s] Desc [%s]"), InInstanceCount, *InstanceRuleName, *InstanceMap);
	if (InInstanceCount >= 0 && InInstanceCount < TotalInstances)
	{
		Instances.Add(FServerInstanceData::Make(InstanceId, InstanceRuleName, InstanceMap, InstanceNumPlayers, InstanceMaxPlayers, InstanceNumFriends, InstanceFlags, InstanceRank, bTeamGame ));
	}

	ServerRequestNextInstancePlayer(InInstanceCount, -1);
}

void AUTServerBeaconClient::ClientReceivedAllInstance_Implementation(int32 FinalCount)
{
	if (Instances.Num() != FinalCount)
	{
		UE_LOG(UT, Log, TEXT("ERROR: Instance Names/Descriptions doesn't meet the final size requirement: %i vs %i"), Instances.Num(), FinalCount);
	}

	UE_LOG(LogBeacon, Verbose, TEXT("---> Got them All DONE!!!!  [%i vs %i]"), Instances.Num(), FinalCount );

	OnServerRequestResults.ExecuteIfBound(this, HostServerInfo);		
}


bool AUTServerBeaconClient::ServerRequestNextInstancePlayer_Validate(int32 InstanceIndex, int32 LastInstancePlayersIndex) { return true; }
void AUTServerBeaconClient::ServerRequestNextInstancePlayer_Implementation(int32 InstanceIndex, int32 LastInstancePlayersIndex)
{
	LastInstancePlayersIndex++;

	UE_LOG(LogBeacon, Verbose,TEXT("ServerRequestInstancePlayers %i has %i players"), InstanceIndex, LastInstancePlayersIndex);

	if (InstanceIndex >= 0 && InstanceIndex < Instances.Num())
	{
		if ( LastInstancePlayersIndex >=0 && LastInstancePlayersIndex < Instances[InstanceIndex]->Players.Num())
		{
			ClientReceiveInstancePlayer(InstanceIndex, LastInstancePlayersIndex, Instances[InstanceIndex]->Players[LastInstancePlayersIndex] );
			return;
		}
		else
		{
			UE_LOG(LogBeacon, Verbose, TEXT("<--- Out of Instances [%i] %i"), LastInstancePlayersIndex, Instances[InstanceIndex]->Players.Num());
			ClientReceivedAllInstancePlayers(InstanceIndex, Instances[InstanceIndex]->Players.Num());
		}
	}
}

void AUTServerBeaconClient::ClientReceiveInstancePlayer_Implementation(int32 InstanceIndex, int32 InInstancePlayersCount, const FMatchPlayerListStruct& inPlayerInfo)
{
	if (InstanceIndex >=0 && InstanceIndex <= Instances.Num())
	{
		TSharedPtr<FServerInstanceData> Instance = Instances[InstanceIndex];
		if (Instance.IsValid())
		{
			Instance->Players.Add(FMatchPlayerListStruct(inPlayerInfo.PlayerName, inPlayerInfo.PlayerId, inPlayerInfo.PlayerScore, inPlayerInfo.TeamNum));
		}
	}
	ServerRequestNextInstancePlayer(InstanceIndex, InInstancePlayersCount);
}

void AUTServerBeaconClient::ClientReceivedAllInstancePlayers_Implementation(int32 InstanceIndex, int32 FinalCount)
{
	ServerRequestNextInstance(InstanceIndex);
}

bool AUTServerBeaconClient::ServerRequestQuickplay_Validate(const FString& MatchType, int32 ELORank) { return true; }
void AUTServerBeaconClient::ServerRequestQuickplay_Implementation(const FString& MatchType, int32 ELORank)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		// Have the lobby see if we can quick play this player.	
		LobbyGameState->HandleQuickplayRequest(this, MatchType, ELORank);
	}
	else
	{
		ClientQuickplayNotAvailable();	
	}
}

void AUTServerBeaconClient::ClientQuickplayNotAvailable_Implementation()
{
	OnRequestQuickplay.ExecuteIfBound(this, EQuickMatchResults::CantJoin, TEXT(""));
}

void AUTServerBeaconClient::ClientWaitForQuickplay_Implementation(uint32 bNewInstance)
{
	OnRequestQuickplay.ExecuteIfBound(this, bNewInstance == 1 ? EQuickMatchResults::WaitingForStartNew : EQuickMatchResults::WaitingForStart, TEXT(""));
}

void AUTServerBeaconClient::ClientJoinQuickplay_Implementation(const FString& InstanceGuid)
{
	OnRequestQuickplay.ExecuteIfBound(this, EQuickMatchResults::Join, InstanceGuid);
}

bool AUTServerBeaconClient::ServerRequestInstanceJoin_Validate(const FString& InstanceId, bool bSpectator, int32 Rank) { return true; }
void AUTServerBeaconClient::ServerRequestInstanceJoin_Implementation(const FString& InstanceId, bool bSpectator, int32 Rank)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->RequestInstanceJoin(this, InstanceId, bSpectator, Rank);
	}
}

void AUTServerBeaconClient::ClientRequestInstanceResult_Implementation(EInstanceJoinResult::Type JoinResult, const FString& Params)
{
	if (OnRequestJoinInstanceResult.IsBound())
	{
		OnRequestJoinInstanceResult.Execute(JoinResult, Params);
	}
}

