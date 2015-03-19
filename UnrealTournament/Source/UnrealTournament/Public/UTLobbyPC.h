	// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTLobbyPlayerState.h"
#include "UTBasePlayerController.h"
#include "UTLobbyPC.generated.h"

UCLASS(config=Game)
class UNREALTOURNAMENT_API AUTLobbyPC : public AUTBasePlayerController
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	AUTLobbyPlayerState* UTLobbyPlayerState;

	UPROPERTY()
	class AUTLobbyHUD* UTLobbyHUD;
		
	virtual void InitPlayerState();
	virtual void OnRep_PlayerState();

	virtual void SetName(const FString& S);

	virtual void ServerRestartPlayer_Implementation();
	virtual bool CanRestartPlayer();

	UFUNCTION(exec)
	virtual void MatchChat(FString Message);

	UFUNCTION(exec)
	virtual void GlobalChat(FString Message);

	virtual void Chat(FName Destination, FString Message);

	UFUNCTION(reliable, server, WithValidation)
	virtual void ServerChat(const FName Destination, const FString& Message);

	virtual void ReceivedPlayer();

	virtual void ServerDebugTest_Implementation(const FString& TestCommand);

	UFUNCTION(reliable, Server , WithValidation)
	virtual void ServerSetReady(uint32 bNewReadyToPlay);

	virtual void PlayerTick( float DeltaTime );

	UFUNCTION(exec)
	virtual void SetLobbyDebugLevel(int32 NewLevel);

	void MatchChanged(AUTLobbyMatchInfo* CurrentMatch);

protected:
	// Will be true when the initial player replication is completed.  At that point it's safe to bring up the menu
	bool bInitialReplicationCompleted;

};




