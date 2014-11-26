// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTServerBeaconLobbyClient.h"

AUTServerBeaconLobbyClient::AUTServerBeaconLobbyClient(const class FObjectInitializer& PCIP) :
Super(PCIP)
{
}

void AUTServerBeaconLobbyClient::InitLobbyBeacon(FURL LobbyURL, uint32 LobbyInstanceID, FGuid InstanceGUID)
{
	InitClient(LobbyURL);
	GameInstanceID = LobbyInstanceID;
	GameInstanceGUID = InstanceGUID;
}

void AUTServerBeaconLobbyClient::OnConnected()
{
}
void AUTServerBeaconLobbyClient::OnFailure()
{
}


void AUTServerBeaconLobbyClient::UpdateDescription(FString NewDescription)
{
	Lobby_UpdateDescription(GameInstanceID, NewDescription);
}

void AUTServerBeaconLobbyClient::EndGame(FString FinalDescription)
{
	Lobby_EndGame(GameInstanceID, FinalDescription);
}

void AUTServerBeaconLobbyClient::Empty()
{
	if (!bInstancePendingKill)
	{
		bInstancePendingKill = true;
		Lobby_InstanceEmpty(GameInstanceID);
	}
}


bool AUTServerBeaconLobbyClient::Lobby_NotifyInstanceIsReady_Validate(uint32 InstanceID, FGuid InstanceGUID) { return true; }
void AUTServerBeaconLobbyClient::Lobby_NotifyInstanceIsReady_Implementation(uint32 InstanceID, FGuid InstanceGUID)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_Ready(InstanceID, InstanceGUID);
	}
}

void AUTServerBeaconLobbyClient::SetBeaconNetDriverName(FString InBeaconName)
{
	BeaconNetDriverName = FName(*InBeaconName);
	NetDriverName = BeaconNetDriverName;
}

bool AUTServerBeaconLobbyClient::Lobby_UpdateDescription_Validate(uint32 InstanceID, const FString& NewDescription) { return true; }
void AUTServerBeaconLobbyClient::Lobby_UpdateDescription_Implementation(uint32 InstanceID, const FString& NewDescription)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_DescriptionUpdate(InstanceID, NewDescription);
	}
}

bool AUTServerBeaconLobbyClient::Lobby_EndGame_Validate(uint32 InstanceID, const FString& FinalDescription) { return true; }
void AUTServerBeaconLobbyClient::Lobby_EndGame_Implementation(uint32 InstanceID, const FString& FinalDescription)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_EndGame(InstanceID,FinalDescription);
	}
}

bool AUTServerBeaconLobbyClient::Lobby_InstanceEmpty_Validate(uint32 InstanceID) { return true; }
void AUTServerBeaconLobbyClient::Lobby_InstanceEmpty_Implementation(uint32 InstanceID)
{
	AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		LobbyGameState->GameInstance_Empty(InstanceID);
	}

}

