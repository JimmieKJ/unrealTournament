// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a message transport technology using shared memory.
 */
class FShmMessageTransport
	: public IMessageTransport
{
public:

	/**
	 * Creates and initializes a new instance.
	 */
	FShmMessageTransport();

	/** Destructor. */
	virtual ~FShmMessageTransport();

public:

	// IMessageTransport interface

	virtual FName GetDebugName() const override
	{
		return "ShmMessageTransport";
	}

	virtual FOnMessageReceived& OnMessageReceived() override
	{
		return MessageReceivedDelegate;
	}

	virtual FOnNodeDiscovered& OnNodeDiscovered() override
	{
		return NodeDiscoveredDelegate;
	}

	virtual FOnNodeLost& OnNodeLost() override
	{
		return NodeLostDelegate;
	}

	virtual bool StartTransport() override;
	virtual void StopTransport() override;
	virtual bool TransportMessage(const IMessageContextRef& Context, const TArray<FGuid>& Recipients) override;

private:

	/** Holds a delegate to be invoked when a message was received on the transport channel. */
	FOnMessageReceived MessageReceivedDelegate;

	/** Holds a delegate to be invoked when a network node was discovered. */
	FOnNodeDiscovered NodeDiscoveredDelegate;

	/** Holds a delegate to be invoked when a network node was lost. */
	FOnNodeLost NodeLostDelegate;
};
