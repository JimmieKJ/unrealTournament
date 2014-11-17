// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLobbyPlayerState.generated.h"

class AUTLobbyMatchInfo;

UCLASS(notplaceable)
class UNREALTOURNAMENT_API AUTLobbyPlayerState : public AUTPlayerState
{
	GENERATED_UCLASS_BODY()

	/** server settings */
	UPROPERTY(Replicated)
	AUTLobbyMatchInfo* CurrentMatch;

	virtual void MatchButtonPressed();
	
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerCreateMatch();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerDestroyOrLeaveMatch();

	void JoinMatch(AUTLobbyMatchInfo* MatchToJoin);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerJoinMatch(AUTLobbyMatchInfo* MatchToJoin);

	virtual void AddedToMatch(AUTLobbyMatchInfo* Match);
	virtual void RemovedFromMatch(AUTLobbyMatchInfo* Match);

	UFUNCTION(Client, Reliable)
	virtual void ClientMatchError(const FText &MatchErrorMessage);

};



