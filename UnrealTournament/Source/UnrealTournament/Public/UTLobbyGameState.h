// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLobbyPlayerState.h"

#include "UTLobbyGameState.generated.h"

UCLASS(notplaceable, Config = Game)
class UNREALTOURNAMENT_API AUTLobbyGameState : public AUTGameState
{
	GENERATED_UCLASS_BODY()

	/** Holds a list of GameMode classes that can be configured for this lobby.  In BeginPlay() these classes will be loaded and  */
	UPROPERTY(Replicated, Config)
	TArray<FString> AllowedGameModeClasses;

	/** server settings */
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo)
	FString LobbyName;
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo)
	FString LobbyMOTD;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Lobby)
	TArray<AUTLobbyMatchInfo*> AvailableMatches;

	// A list of all maps that are allow the be chosen from.  We might want to consider auto-filling this but I want the
	// admin to be able to select which maps he wants to offer.  The Client will then filter the list by what maps they have installed.
	// We can at some point add a button or some mechanism  to allow players to pull missing maps from the marketplace.
	UPROPERTY(Config)
	TArray<FString> AllowedMaps;

	virtual void BroadcastChat(AUTLobbyPlayerState* SenderPS, FName Destination, const FString& Message);
	virtual AUTLobbyMatchInfo* AddMatch(AUTLobbyPlayerState* PlayerOwner);
	virtual void RemoveMatch(AUTLobbyPlayerState* PlayerOwner);

	virtual void SortPRIArray();

};



