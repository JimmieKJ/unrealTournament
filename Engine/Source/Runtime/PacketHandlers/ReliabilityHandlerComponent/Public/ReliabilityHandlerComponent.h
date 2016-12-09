// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "PacketHandler.h"

// Symmetric Stream cipher
class RELIABILITYHANDLERCOMPONENT_API ReliabilityHandlerComponent : public HandlerComponent
{
public:
	/* Initializes default data */
	ReliabilityHandlerComponent();

	/* Initializes the handler component */
	virtual void Initialize() override;

	/* Whether the handler component is valid */
	virtual bool IsValid() const override;

	/* Tick every frame */
	virtual void Tick(float DeltaTime) override;

	/* Handles any incoming packets */
	virtual void Incoming(FBitReader& Packet) override;

	/* Handles any outgoing packets */
	virtual void Outgoing(FBitWriter& Packet) override;

	/* Queues a packet for resending */
	virtual void QueuePacketForResending(uint8* Packet, int32 CountBits);

protected:
	/* Buffered Packets in case they need to be resent */
	TQueue<BufferedPacket*> BufferedPackets;

	/* Latest Packet ID */
	uint32 LocalPacketID;

	/* Latest Packet ID that was ACKED */
	uint32 LocalPacketIDACKED;

	/* Latest Remote Packet ID */
	uint32 RemotePacketID;
	
	/* Latest Remote Packet ID that was ACKED */
	uint32 RemotePacketIDACKED;

	/* How long to wait before resending an UNACKED packet */
	float ResendResolutionTime;
};

/* Reliability Module Interface */
class FReliabilityHandlerComponentModuleInterface : public FPacketHandlerComponentModuleInterface
{
public:
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};
