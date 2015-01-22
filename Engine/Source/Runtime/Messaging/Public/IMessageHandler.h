// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreUObject.h"


// forward declarations
class IMessageContext;


/**
 * Interface for message handlers.
 */
class IMessageHandler
{
public:

	/**
	 * Gets the message type handled by this handler.
	 *
	 * @return The name of the message type.
	 * @see HandleMessage
	 */
	virtual const FName GetHandledMessageType() const = 0;

	/**
	 * Handles the specified message.
	 *
	 * @param Context The context of the message to handle.
	 * @see GetHandledMessageType
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


/**
 * Template for message handler function pointer types.
 *
 * @param MessageType The type of message to handle.
 * @param HandlerType The type of the handler class.
 */
template<typename MessageType, typename HandlerType>
struct TMessageHandlerFunc
{
	typedef void (HandlerType::*Type)( const MessageType&, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& );
};


/**
 * Template for message handlers.
 *
 * @param MessageType The type of message to handle.
 * @param HandlerType The type of the handler class.
 */
template<typename MessageType, typename HandlerType>
class TMessageHandler
	: public IMessageHandler
{
public:

	/**
	 * Creates and initializes a new message handler.
	 *
	 * @param InHandler The object that will handle the messages.
	 * @param InHandlerFunc The object's message handling function.
	 */
	TMessageHandler( HandlerType* InHandler, typename TMessageHandlerFunc<MessageType, HandlerType>::Type InHandlerFunc )
		: Handler(InHandler)
		, HandlerFunc(InHandlerFunc)
	{
		check(InHandler != nullptr);
	}

public:

	// IMessageHandler interface
	
	virtual const FName GetHandledMessageType() const override
	{
		return MessageType::StaticStruct()->GetFName();
	}

	virtual void HandleMessage( const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context ) override
	{
		(Handler->*HandlerFunc)(*static_cast<const MessageType*>(Context->GetMessage()), Context);
	}
	
private:

	/** Holds a pointer to the object handling the messages. */
	HandlerType* Handler;

	/** Holds a pointer to the actual handler function. */
	typename TMessageHandlerFunc<MessageType, HandlerType>::Type HandlerFunc;
};
