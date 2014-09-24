// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

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

	virtual void BroadcastMatchMessage(AUTLobbyPlayerState* SenderPS, const FString& Message);

};



