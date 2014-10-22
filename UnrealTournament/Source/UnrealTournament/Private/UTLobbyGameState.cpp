// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/GameState.h"
#include "UTLobbyMatchInfo.h"
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
			PS->ClientRecieveChat(ChatDestinations::Global, SenderPS, Message);
		}
	}
}

AUTLobbyMatchInfo* AUTLobbyGameState::AddMatch(AUTLobbyPlayerState* PlayerOwner)
{
	if (PlayerOwner->CurrentMatch != NULL)
	{
		// This player is already in a match.  Look to see if he is the host and if he is, don't allow.
		if (PlayerOwner->CurrentMatch->OwnersPlayerState == PlayerOwner)
		{
			return NULL;
		}
	
		if (PlayerOwner->CurrentMatch->CurrentState == ELobbyMatchState::WaitingForPlayers)
		{
			// Remove me from this before adding.
			PlayerOwner->CurrentMatch->RemovePlayer(PlayerOwner);
		}
	}

	AUTLobbyMatchInfo* NewMatchInfo = GetWorld()->SpawnActor<AUTLobbyMatchInfo>();
	if (NewMatchInfo)
	{
		NewMatchInfo->AddPlayer(PlayerOwner,true);
		AvailableMatches.Add(NewMatchInfo);
		return NewMatchInfo;
	}
		
	return NULL;
}

void AUTLobbyGameState::RemoveMatch(AUTLobbyPlayerState* PlayerOwner)
{
	AUTLobbyMatchInfo* Match;
	Match = PlayerOwner->CurrentMatch;
	if (Match)
	{
		// Trigger all players being removed
		Match->RemovePlayer(PlayerOwner);	
		AvailableMatches.Remove(Match);
	}
}
