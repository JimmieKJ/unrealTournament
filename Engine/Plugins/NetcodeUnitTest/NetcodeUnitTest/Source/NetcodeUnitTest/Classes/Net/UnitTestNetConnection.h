// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IpConnection.h"
#include "UnitTestNetConnection.generated.h"


/**
 * A net connection for enabling unit testing through barebones/minimal client connections
 */
UCLASS(transient)
class UUnitTestNetConnection : public UIpConnection
{
	GENERATED_UCLASS_BODY()


	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState,
							int32 InMaxPacket=0, int32 InPacketOverhead=0) override;

#if TARGET_UE4_CL < CL_INITCONNPARAM
	virtual void InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed=0) override;
#else
	virtual void InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed=0,
								int32 InMaxPacket=0) override;
#endif


	virtual void LowLevelSend(void* Data, int32 Count) override;

	virtual void ReceivedRawPacket(void* Data, int32 Count) override;

	virtual void HandleClientPlayer(class APlayerController* PC, class UNetConnection* NetConnection) override;


	/**
	 * Delegate for hooking 'LowLevelSend'
	 *
	 * @param Data		The data being sent
	 * @param Count		The number of bytes being sent
	 */
	DECLARE_DELEGATE_TwoParams(FLowLevelSendDel, void* /*Data*/, int32 /*Count*/);

	/**
	 * Delegate for hooking 'ReceivedRawPacket'
	 * 
	 * @param Data		The data received
	 * @param Count		The number of bytes received
	 */
	DECLARE_DELEGATE_TwoParams(FReceivedRawPacketDel, void* /*Data*/, int32& /*Count*/);

	/**
	 * Delegate for notifying on (and optionally blocking) replicated actor creation
	 *
	 * @param ActorClass	The class of the actor being replicated
	 * @param bActorChannel	Whether or not this actor creation is from an actor channel
	 * @return				Whether or not to allow creation of the actor
	 */
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnReplicatedActorSpawn, UClass* /*ActorClass*/, bool /*bActorChannel*/);


	/** Delegate for hooking LowLevelSend */
	FLowLevelSendDel		LowLevelSendDel;

	/** Delegate for hooking ReceivedRawPacket */
	FReceivedRawPacketDel	ReceivedRawPacketDel;

	/** Delegate for notifying on replicated actor creation */
	FOnReplicatedActorSpawn	ReplicatedActorSpawnDel;
};





