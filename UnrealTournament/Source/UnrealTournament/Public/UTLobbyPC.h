	// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTLobbyPlayerState.h"
#include "UTLobbyPC.generated.h"

UCLASS(config=Game)
class UNREALTOURNAMENT_API AUTLobbyPC : public APlayerController
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	AUTLobbyPlayerState* UTLobbyPlayerState;

	UPROPERTY()
	class AUTLobbyHUD* UTLobbyHUD;

	virtual void PostInitializeComponents();
	
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
};




