// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMessageAttachment;
class IMessageData;


/**
 * Interface for message transport technologies.
 *
 * Licensees can implement this interface to add support for custom message transport
 * technologies that are not supported out of the box, i.e. custom network protocols or APIs.
 */
class IMessageTransport
{
public:

	/**
	 * Gets the name of this transport (for debugging purposes).
	 *
	 * @return The debug name.
	 */
	virtual FName GetDebugName() const = 0;

	/**
	 * Starts up the message transport.
	 *
	 * @return Whether the transport was started successfully.
	 * @see StopTransport
	 */
	virtual bool StartTransport() = 0;

	/**
	 * Shuts down the message transport.
	 *
	 * @see StartTransport
	 */
	virtual void StopTransport() = 0;

	/**
	 * Transports the given message data to the specified network nodes.
	 *
	 * @param Context The context of the message to transport.
	 * @param Recipients The transport nodes to send the message to.
	 * @return true if the message is being transported, false otherwise.
	 */
	virtual bool TransportMessage( const IMessageContextRef& Context, const TArray<FGuid>& Recipients ) = 0;

public:

	/**
	 * A delegate that is executed when message data has been received.
	 *
	 * @param Delegate The delegate to set.
	 */
	DECLARE_DELEGATE_TwoParams(FOnMessageReceived, const IMessageContextRef&, const FGuid&)
	virtual FOnMessageReceived& OnMessageReceived() = 0;

	/**
	 * A delegate that is executed when a transport node has been discovered.
	 *
	 * @param Delegate The delegate to set.
	 */
	DECLARE_DELEGATE_OneParam(FOnNodeDiscovered, const FGuid&)
	virtual FOnNodeDiscovered& OnNodeDiscovered() = 0;

	/**
	 * A delegate that is executed when a transport node has closed or timed out.
	 *
	 * @param Delegate The delegate to set.
	 */
	DECLARE_DELEGATE_OneParam(FOnNodeLost, const FGuid&)
	virtual FOnNodeLost& OnNodeLost() = 0;

protected:

	/** Virtual constructor. */
	~IMessageTransport() { }
};


/** Type definition for shared pointers to instances of ITransportMessages. */
typedef TSharedPtr<IMessageTransport, ESPMode::ThreadSafe> IMessageTransportPtr;

/** Type definition for shared references to instances of ITransportMessages. */
typedef TSharedRef<IMessageTransport, ESPMode::ThreadSafe> IMessageTransportRef;
