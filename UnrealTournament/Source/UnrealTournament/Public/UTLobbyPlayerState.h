// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLobbyPlayerState.generated.h"

class AUTLobbyMatchInfo;

UCLASS(notplaceable)
class UNREALTOURNAMENT_API AUTLobbyPlayerState : public APlayerState
{
	GENERATED_UCLASS_BODY()

	/** server settings */
	UPROPERTY(Replicated)
	AUTLobbyMatchInfo* CurrentMatch;

	UFUNCTION(client, reliable)
	virtual void ClientRecieveChat(FName Destination, AUTLobbyPlayerState* Sender, const FString& ChatText);

	virtual void AddedToMatch(AUTLobbyMatchInfo* Match);
	virtual void RemovedFromMatch(AUTLobbyMatchInfo* Match);

};



