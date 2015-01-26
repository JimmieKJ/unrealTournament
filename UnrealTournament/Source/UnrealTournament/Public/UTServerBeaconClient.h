// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

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

	virtual void SetBeaconNetDriverName(FString InBeaconName);

	/** Send a ping RPC to the client */
	UFUNCTION(client, reliable)
	virtual void ClientPing(const FServerBeaconInfo ServerInfo, int32 InstanceCount);

	/** Send a pong RPC to the host */
	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerPong();

	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerSendInstances(int32 LastInstanceIndex);

	UFUNCTION(client, reliable)
	virtual void ClientRecieveInstance(uint32 InstanceCount, uint32 TotalInstances, const FString& InstanceHostName, const FString& InstanceDescription);

	FServerRequestResultsDelegate OnServerRequestResults;
	FServerRequestFailureDelegate OnServerRequestFailure;

	FString ServerMOTD;

	TArray<FString> InstanceHostNames;
	TArray<FString> InstanceDescriptions;
	int32 InstanceCount;
	
protected:
	FServerBeaconInfo HostServerInfo;

};
