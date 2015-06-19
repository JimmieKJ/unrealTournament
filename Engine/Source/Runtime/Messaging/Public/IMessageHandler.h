// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for message handlers.
 */
class IMessageHandler
{
public:

	/**
	 * Handles the specified message.
	 *
	 * @param Context The context of the message to handle.
	 */
	virtual void HandleMessage( const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context ) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMessageHandler() { }
};


/** Type definition for shared pointers to instances of IMessageHandler. */
typedef TSharedPtr<IMessageHandler, ESPMode::ThreadSafe> IMessageHandlerPtr;

/** Type definition for shared references to instances of IMessageHandler. */
typedef TSharedRef<IMessageHandler, ESPMode::ThreadSafe> IMessageHandlerRef;
