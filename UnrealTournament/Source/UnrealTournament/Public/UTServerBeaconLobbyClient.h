// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Runtime/Online/OnlineSubsystemUtils/Classes/OnlineBeaconClient.h"
#include "UTServerBeaconClient.h"
#include "UTServerBeaconLobbyClient.generated.h"


/**
* This because allows the 
*/
UCLASS(transient, notplaceable, config = Engine)
class UNREALTOURNAMENT_API AUTServerBeaconLobbyClient : public AOnlineBeaconClient
{
	GENERATED_UCLASS_BODY()

	virtual FString GetBeaconType() override { return TEXT("UTLobbyBeacon"); }

	virtual void InitLobbyBeacon(FURL LobbyURL, uint32 LobbyInstanceID, FGuid InstanceGUID, FString AccessKey);
	virtual void OnConnected();
	virtual void OnFailure();
	
	virtual void UpdateMatch(FString Update);
	virtual void UpdatePlayer(FUniqueNetIdRepl PlayerID, const FString& PlayerName, int32 PlayerScore, bool bSpectator, bool bLastUpdate);
	virtual void EndGame(FString FinalUpdate);
	virtual void Empty();

	/**
	 *	NotifyInstanceIsReady will be called when the game instance is ready.  It will replicate to the lobby server and tell the lobby to have all of the clients in this match transition
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_NotifyInstanceIsReady(uint32 InstanceID, FGuid InstanceGUID);

	/**
	 * Tells the Lobby to update it's description on the stats
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_UpdateMatch(uint32 InstanceID, const FString& Update);

	/**
	 * Tells the Lobby to update it's badge
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_UpdateBadge(uint32 InstanceID, const FString& Update);


	/**
	 *	Allows the instance to update the lobby regarding a given player.
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_UpdatePlayer(uint32 InstanceID, FUniqueNetIdRepl PlayerID, const FString& PlayerName, int32 PlayerScore, bool bSpectator, bool bLastUpdate);

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

	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_RequestNextMap(uint32 InstanceID, const FString& CurrentMap);

	/**
	 *	Tells the instance what map to travel to.  If NextMap is empty, then it's the end of the map list and
	 *  players will return to the Hub.
	 **/
	UFUNCTION(client, reliable)
	virtual void InstanceNextMap(const FString& NextMap);

	/**
	 *	When we connect, if this is a connection between a hub and a dedicated instance, this function
	 * will be called to let the hub know I want to be a dedicated instance.
	 **/
	UFUNCTION(server, reliable, WithValidation)
	virtual void Lobby_IsDedicatedInstance(FGuid InstanceGUID, const FString& InHubKey, const FString& ServerName);

	/**
	 *	Called from the hub, this will let a dedicated instance know it's been authorized and is connected to 
	 *  the Hub.
	 **/
	UFUNCTION(client, reliable)
	virtual void AuthorizeDedicatedInstance(FGuid HubGuid);

	// Will cause the hub to prime the AllowedMapPackages list with all maps available on the server.
	UFUNCTION(server, reliable, withvalidation)
	virtual void Lobby_PrimeMapList(const FString& MapPrefix);

	UFUNCTION(client, reliable)
	virtual void Instance_ReceiveMap(const FString& MapPackageName, const FString& MapTitle, const FString& MapScreenshotReference, int32 Index);

	UFUNCTION(server, reliable, withvalidation)
	virtual void Lobby_SendNextMap(int32 LastIndex);

	UFUNCTION(client, reliable)
	virtual void Instance_ReceiveHubID(FGuid HubGuid);

protected:

	// Will be set to true when the game instance is empty and has asked the lobby to kill it
	bool bInstancePendingKill;

	uint32 GameInstanceID;
	FGuid GameInstanceGUID;

	bool bDedicatedInstance;

	FString HubKey;

	TArray<TSharedPtr<FMapListItem>> AllowedMaps;

};
