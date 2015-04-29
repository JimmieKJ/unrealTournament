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

	virtual void PreInitializeComponents() override;

	// The match this player is currently in.
	UPROPERTY(Replicated, replicatedUsing = OnRep_CurrentMatch )
	AUTLobbyMatchInfo* CurrentMatch;

	// The previous match this player played in.
	UPROPERTY()
	AUTLobbyMatchInfo* PreviousMatch;

	// Client-Side.  Will be called from the UI when the player presses the Create Match and Exit Match Button.
	virtual void MatchButtonPressed();

	// Server-Side.  Attempt to create a new match
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerCreateMatch();

	// Server-Side.  Attempt to leave and or destory the existing match this player is in.
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerDestroyOrLeaveMatch();

	// Server-Side.  Attept to Join a match
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerJoinMatch(AUTLobbyMatchInfo* MatchToJoin, bool bAsSpectator);

	// Server-Side - Called when this player has been added to a match
	virtual void AddedToMatch(AUTLobbyMatchInfo* Match);

	// Server-Side - Called when this player has been removed from a match
	virtual void RemovedFromMatch(AUTLobbyMatchInfo* Match);

	// Client-Side - Causes a error message to occur
	UFUNCTION(Client, Reliable)
	virtual void ClientMatchError(const FText& MatchErrorMessage, int32 OptionalInt = 0);

	/**
	 *	Tells a client to connect to a game instance.  InstanceGUID is the server GUID of the game instance this client should connect to.  The client
	 *  will use the OSS to pull the server information and perform the connection.  LobbyGUID is the server GUID of the lobbyt to return to.  The client
	 *  should cache this off and use it to return later.
	 **/
	UFUNCTION(Client, Reliable)
	virtual void ClientConnectToInstance(const FString& GameInstanceGUIDString, const FString& LobbyGUIDString, bool bAsSpectator);

	// Allows for UI/etc to pickup changes to the current match.
	FCurrentMatchChangedDelegate CurrentMatchChangedDelegate;

	// Will be true if the client has returned from a match previously (?RTM=1 on the URL)
	bool bReturnedFromMatch;

protected:

	// This flag is managed on both sides of the pond but isn't replicated.  It's only valid on the server and THIS client.
	bool bIsInMatch;

	UFUNCTION()
	void OnRep_CurrentMatch();

public:
	FString DesiredQuickStartGameMode;
	// The Unique ID of a friend this player wants to join
	FString DesiredFriendToJoin;

};



