// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageBridge.h"
#include "IMessageContext.h"
#include "IMessageTransport.h"
#include "IReceiveMessages.h"
#include "ISendMessages.h"


/**
 * Implements a message bridge.
 *
 * A message bridge is a special message endpoint that connects multiple message buses
 * running in different processes or on different devices. This allows messages that are
 * available in one system to also be available on other systems.
 *
 * Message bridges use an underlying transport layer to channel the messages between two
 * or more systems. Such layers may utilize system specific technologies, such as network
 * sockets or shared memory to communicate with remote bridges. The bridge acts as a map
 * from message addresses to remote nodes and vice versa.
 *
 * @see IMessageBus, IMessageTransport
 */
class FMessageBridge
	: public TSharedFromThis<FMessageBridge, ESPMode::ThreadSafe>
	, public IMessageBridge
	, public IReceiveMessages
	, public ISendMessages
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InAddress The message address for this bridge.
	 * @param InBus The message bus that this node is connected to.
	 * @param InTransport The transport mechanism to use.
	 */
	FMessageBridge( const FMessageAddress InAddress, const IMessageBusRef& InBus, const IMessageTransportRef& InTransport );

	/** Destructor. */
	~FMessageBridge();

public:

	// IMessageBridge interface

	virtual void Disable() override;
	virtual void Enable() override;

	virtual bool IsEnabled() const override
	{
		return Enabled;
	}

public:

	// IReceiveMessages interface

	virtual FName GetDebugName() const override
	{
		return *FString::Printf(TEXT("FMessageBridge (%s)"), *Transport->GetDebugName().ToString());
	}

	virtual const FGuid& GetRecipientId() const override
	{
		return Id;
	}

	virtual ENamedThreads::Type GetRecipientThread() const override
	{
		return ENamedThreads::AnyThread;
	}

	virtual bool IsLocal() const override
	{
		return false;
	}

	virtual void ReceiveMessage( const IMessageContextRef& Context ) override;

public:

	// ISendMessages interface

	virtual FMessageAddress GetSenderAddress() override
	{
		return Address;
	}

	virtual void NotifyMessageError( const IMessageContextRef& Context, const FString& Error ) override;

protected:

	/** Shuts down the bridge. */
	void Shutdown();

private:

	/** Callback for message bus shutdowns. */
	void HandleMessageBusShutdown();

	/** Callback for messages received from the transport layer. */
	void HandleTransportMessageReceived( const IMessageContextRef& Envelope, const FGuid& NodeId );

	/** Callback for lost remote nodes. */
	void HandleTransportNodeLost( const FGuid& LostNodeId );

private:

	/** Holds the bridge's address. */
	FMessageAddress Address;

	/** Holds the address book. */
	FMessageAddressBook AddressBook;

	/** Holds a reference to the bus that this bridge is attached to. */
	IMessageBusPtr Bus;

	/** Hold a flag indicating whether this endpoint is active. */
	bool Enabled;

	/** Holds the bridge's unique identifier (for debugging purposes). */
	const FGuid Id;

	/** Holds the message subscription for outbound messages. */
	IMessageSubscriptionPtr MessageSubscription;

	/** Holds the message transport object. */
	IMessageTransportPtr Transport;
};
