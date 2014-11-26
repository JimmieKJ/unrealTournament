// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Runtime/Online/OnlineSubsystemUtils/Classes/OnlineBeaconClient.h"
#include "UTSErverBeaconClient.h"
#include "UTServerBeaconLobbyClient.generated.h"


/**
* This because allows the 
*/
UCLASS(transient, notplaceable, config = Engine)
class UNREALTOURNAMENT_API AUTServerBeaconLobbyClient : public AOnlineBeaconClient
{
	GENERATED_UCLASS_BODY()

	virtual FString GetBeaconType() override { return TEXT("UTLobbyBeacon"); }

	virtual void InitLobbyBeacon(FURL LobbyURL, uint32 LobbyInstanceID, FGuid InstanceGUID);
	virtual void OnConnected();
	virtual void OnFailure();

	virtual void SetBeaconNetDriverName(FString InBeaconName);

	virtual void UpdateDescription(FString NewDescription);
	virtual void EndGame(FString FinalDescription);
	virtual void Empty();

	/**
	 *	NotifyInstanceIsReady will be called when the game instance is ready.  It will replicate to the lobby server and tell the lobby to have all of the clients in this match transition
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_NotifyInstanceIsReady(uint32 InstanceID, FGuid InstanceGUID);

	/**
	 * Tells the Lobby to update it's description on the panel
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_UpdateDescription(uint32 InstanceID, const FString& NewDescription);

	/**
	 *	Tells the Lobby that this instance is at Game Over
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_EndGame(uint32 InstanceID, const FString& FinalDescription);

	/**
	 *	Tells the Lobby server that this instance is empty and it can be recycled.
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_InstanceEmpty(uint32 InstanceID);

protected:

	// Will be set to true when the game instance is empty and has asked the lobby to kill it
	bool bInstancePendingKill;

	uint32 GameInstanceID;
	FGuid GameInstanceGUID;

};
