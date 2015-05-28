// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlChannel.generated.h"


/**
 * A channel for exchanging connection control messages.
 */
UCLASS(transient, customConstructor)
class ENGINE_API UControlChannel
	: public UChannel
{
	GENERATED_UCLASS_BODY()

	/**
	 * Used to interrogate the first packet received to determine endianess
	 * of the sending client
	 */
	bool bNeedsEndianInspection;

	/** 
	 * provides an extra buffer beyond RELIABLE_BUFFER for control channel messages
	 * as we must be able to guarantee delivery for them
	 * because they include package map updates and other info critical to client/server synchronization
	 */
	TArray< TArray<uint8> > QueuedMessages;

	/** maximum size of additional buffer
	 * if this is exceeded as well, we kill the connection.  @TODO FIXME temporarily huge until we figure out how to handle 1 asset/package implication on packagemap
	 */
	enum { MAX_QUEUED_CONTROL_MESSAGES = 32768 };

	/**
	 * Inspects the packet for endianess information. Validates this information
	 * against what the client sent. If anything seems wrong, the connection is
	 * closed
	 *
	 * @param Bunch the packet to inspect
	 *
	 * @return true if the packet is good, false otherwise (closes socket)
	 */
	bool CheckEndianess(FInBunch& Bunch);

	/** adds the given bunch to the QueuedMessages list. Closes the connection if MAX_QUEUED_CONTROL_MESSAGES is exceeded */
	void QueueMessage(const FOutBunch* Bunch);

	/**
	 * Default constructor
	 */
	UControlChannel(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: UChannel(ObjectInitializer)
	{
		ChannelClasses[CHTYPE_Control]      = GetClass();
		ChType								= CHTYPE_Control;
	}

	virtual void Init( UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally ) override;

	// Begin UChannel interface.
	virtual FPacketIdRange SendBunch(FOutBunch* Bunch, bool Merge) override;

	virtual void Tick() override;
	// End UChannel interface.


	/** Handle an incoming bunch. */
	virtual void ReceivedBunch( FInBunch& Bunch ) override;

	/** Describe the text channel. */
	virtual FString Describe() override;
};
