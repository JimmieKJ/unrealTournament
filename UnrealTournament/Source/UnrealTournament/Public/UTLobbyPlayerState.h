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
	virtual void BeginPlay() override;

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

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void Server_ReadyToBeginDataPush();

	UFUNCTION(Client, Reliable)
	virtual void Client_BeginDataPush(int32 ExpectedSendCount);


protected:

	// This is a system that pulls all of the needed allowed data for setting up custom rulesets.  When a LPS becomes live on the client, it 
	// will request a data push from the server.  As data comes in, it will fill out the data in the Lobby Game State.  

	int32 TotalBlockCount;
	int32 CurrentBlock;

	// checks if the client is ready for a datapush.  Both the LPS and LGS must exist on the client before the system can begin.
	// Normally this will be the case when Client_BeginDataPush is called, however, replication order isn't guarenteed so we have
	// to make sure it's ready.  If not, set a time and try again in 1/2 a second.
	virtual void CheckDataPushReady();

	// Tells the server to send me a new block.
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void Server_SendDataBlock(int32 Block);

	// Tells the client to receive a block.
	UFUNCTION(Client, Reliable)
	virtual void Client_ReceiveBlock(int32 Block, FAllowedData Data);

};



