// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerState.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyPlayerState.h"
#include "Net/UnrealNetwork.h"

AUTLobbyPlayerState::AUTLobbyPlayerState(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

}

void AUTLobbyPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTLobbyPlayerState, CurrentMatch);
}

void AUTLobbyPlayerState::ClientRecieveChat_Implementation(FName Destination, AUTLobbyPlayerState* Sender, const FString& ChatText)
{
	if (Sender)
	{
		UE_LOG(UT, Log, TEXT("Chat: [%s] from %s: %s"), *Destination.ToString(), *Sender->PlayerName, *ChatText);
	}
}
