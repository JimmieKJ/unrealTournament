// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/GameState.h"
#include "UTLobbyGameMode.h"
#include "Net/UnrealNetwork.h"

AUTLobbyGameState::AUTLobbyGameState(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	LobbyName = TEXT("My First Lobby");
	LobbyMOTD = TEXT("Welcome!");
}

void AUTLobbyGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTLobbyGameState, LobbyName, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTLobbyGameState, LobbyMOTD, COND_InitialOnly);
}


void AUTLobbyGameState::BroadcastMatchMessage(AUTLobbyPlayerState* SenderPS, const FString& Message)
{
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(PlayerArray[i]);
		if (PS)	// TODO: Add some way for players to "mute" global chat
		{
			PS->ClientRecieveChat(ChatDestinations::GlobalChat, SenderPS, Message);
		}
	}
}