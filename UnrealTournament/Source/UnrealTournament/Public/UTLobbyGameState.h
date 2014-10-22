// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLobbyPlayerState.h"

#include "UTLobbyGameState.generated.h"

UCLASS(notplaceable, Config = Game)
class UNREALTOURNAMENT_API AUTLobbyGameState : public AGameState
{
	GENERATED_UCLASS_BODY()

	/** server settings */
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo)
	FString LobbyName;
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo)
	FString LobbyMOTD;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Lobby)
	TArray<AUTLobbyMatchInfo*> AvailableMatches;

	virtual void BroadcastMatchMessage(AUTLobbyPlayerState* SenderPS, const FString& Message);

	virtual AUTLobbyMatchInfo* AddMatch(AUTLobbyPlayerState* PlayerOwner);
	virtual void RemoveMatch(AUTLobbyPlayerState* PlayerOwner);

};



