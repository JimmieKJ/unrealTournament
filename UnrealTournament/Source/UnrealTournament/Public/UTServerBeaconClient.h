// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Runtime/Online/OnlineSubsystemUtils/Classes/OnlineBeaconClient.h"
#include "UTServerBeaconClient.generated.h"

USTRUCT()
struct FServerBeaconInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString ServerRules;

	UPROPERTY()
	FString ServerPlayers;

	UPROPERTY()
	FString MOTD;

	UPROPERTY()
	FString CurrentMap;

};


class AUTServerBeaconClient;
DECLARE_DELEGATE_TwoParams(FServerRequestResultsDelegate, AUTServerBeaconClient*, FServerBeaconInfo);
DECLARE_DELEGATE_OneParam(FServerRequestFailureDelegate, AUTServerBeaconClient*);


/**
* A beacon client used for making reservations with an existing game session
*/
UCLASS(transient, notplaceable, config = Engine)
class UNREALTOURNAMENT_API AUTServerBeaconClient : public AOnlineBeaconClient
{
	GENERATED_UCLASS_BODY()

	// Begin AOnlineBeacon Interface
	virtual FString GetBeaconType() override { return TEXT("UTServerBeacon"); }
	// End AOnlineBeacon Interface

	// Begin AOnlineBeaconClient Interface
	virtual void OnConnected() override;
	virtual void OnFailure() override;
	// End AOnlineBeaconClient Interface

	float PingStartTime;
	float Ping;
	
	/** Send a ping RPC to the host */
	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerPing();

	/** Send a Pong RPC to the client */
	UFUNCTION(client, reliable)
	virtual void ClientPong();

	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerRequestInfo();

	UFUNCTION(client, reliable)
	virtual void ClientReceiveInfo(const FServerBeaconInfo ServerInfo, int32 NumInstances);

	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerRequestInstances(int32 LastInstanceIndex);

	UFUNCTION(client, reliable)
	virtual void ClientReceiveInstance(uint32 InInstanceCount, uint32 TotalInstances, const FString& InstanceRuleIcon, const FString& InstanceDescription);

	UFUNCTION(client, reliable)
	virtual void ClientReceivedAllInstance(uint32 FinalCount);

	// Asks the hub if this client can get added to a quick play session.  This will be called because
	// the quickplay manager has decided this server is the best match.  The server will respond with one of the 3 functions below.
	UFUNCTION(server, reliable, withvalidation)
	virtual void ServerRequestQuickplay(const FString& MatchType, const FString& ClientUniqueId, int32 ELORank);

	// If no quick play matches are available, let the client know to pick a new server.  This can occur if all of the available instances
	// are taken on a hub.  
	UFUNCTION(client, reliable)
	virtual void ClientQuickplayNotAvailable();

	// If the hub has to spool up an instance for a quickplay match, this function will be called to let the client know a join is coming.  ClientJoinQuickplay below will
	// be triggered when the instance is ready.
	UFUNCTION(client, reliable)
	virtual void ClientWaitForQuickplay();

	// This will be called when a hub is ready for a client to transition to an instance for quick play. 
	UFUNCTION(client, reliable)
	virtual void ClientJoinQuickplay(const FString& InstanceGuid);

	FServerRequestResultsDelegate OnServerRequestResults;
	FServerRequestFailureDelegate OnServerRequestFailure;

	FString ServerMOTD;

	TArray<FGuid> InstanceIDs;
	TArray<FServerInstanceData> InstanceInfo;
	int32 InstanceCount;
	
protected:
	FServerBeaconInfo HostServerInfo;

};
