// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLobbyPlayerState.generated.h"

class AUTLobbyMatchInfo;

class AUTLobbyPlayerState;

DECLARE_DELEGATE_OneParam(FCurrentMatchChangedDelegate, AUTLobbyPlayerState*)

UCLASS(notplaceable)
class UNREALTOURNAMENT_API AUTLobbyPlayerState : public AUTPlayerState
{
	GENERATED_UCLASS_BODY()

	/** server settings */
	UPROPERTY(Replicated, replicatedUsing = OnRep_CurrentMatch )
	AUTLobbyMatchInfo* CurrentMatch;

	UPROPERTY()
	AUTLobbyMatchInfo* PreviousMatch;

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

	/**
	 *	Tells a client to connect to a game instance.  InstanceGUID is the server GUID of the game instance this client should connect to.  The client
	 *  will use the OSS to pull the server information and perform the connection.  LobbyGUID is the server GUID of the lobbyt to return to.  The client
	 *  should cache this off and use it to return later.
	 **/
	UFUNCTION(Client, Reliable)
	virtual void ClientConnectToInstance(const FString& GameInstanceGUIDString, const FString& LobbyGUIDString, bool bAsSpectator);

	FCurrentMatchChangedDelegate CurrentMatchChangedDelegate;

protected:
	UFUNCTION()
	void OnRep_CurrentMatch();

};



