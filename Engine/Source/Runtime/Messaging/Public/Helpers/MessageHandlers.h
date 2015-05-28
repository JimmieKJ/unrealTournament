// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreUObject.h"
#include "IMessageContext.h"


/**
 * Template for handlers of one specific message type (via raw function pointers).
 *
 * @param MessageType The type of message to handle.
 * @param HandlerType The type of the handler class.
 */
template<typename MessageType, typename HandlerType>
class TRawMessageHandler
	: public IMessageHandler
{
public:

	/** Type definition for function pointers that are compatible with this TRawMessageHandler. */
	typedef void (HandlerType::*FuncType)( const MessageType&, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& );

public:

	/**
	 * Creates and initializes a new message handler.
	 *
	 * @param InHandler The object that will handle the messages.
	 * @param InHandlerFunc The object's message handling function.
	 */
	TRawMessageHandler( HandlerType* InHandler, FuncType InHandlerFunc )
		: Handler(InHandler)
		, HandlerFunc(InHandlerFunc)
	{
		check(InHandler != nullptr);
	}

	/** Virtual destructor. */
	~TRawMessageHandler() { }

public:

	// IMessageHandler interface
	
	virtual void HandleMessage( const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context ) override
	{
		if (Context->GetMessageType() == MessageType::StaticStruct()->GetFName())
		{
			(Handler->*HandlerFunc)(*static_cast<const MessageType*>(Context->GetMessage()), Context);
		}	
	}
	
private:

	/** Holds a pointer to the object handling the messages. */
	HandlerType* Handler;

	/** Holds a pointer to the actual handler function. */
	FuncType HandlerFunc;
};


/**
 * Template for handlers of one specific message type (via function objects).
 *
 * @param MessageType The type of message to handle.
 */
template<typename MessageType>
class TFunctionMessageHandler
	: public IMessageHandler
{
public:

	/** Type definition for function objects that are compatible with this TFunctionMessageHandler. */
	typedef TFunction<void(const MessageType&, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>&)> FuncType;

public:

	/**
	 * Creates and initializes a new message handler.
	 *
	 * @param InHandler The object that will handle the messages.
	 * @param InHandlerFunc The object's message handling function.
	 */
	TFunctionMessageHandler( FuncType InHandlerFunc )
		: HandlerFunc(InHandlerFunc)
	{ }

	/** Virtual destructor. */
	~TFunctionMessageHandler() { }

public:

	// IMessageHandler interface
	
	virtual void HandleMessage( const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context ) override
	{
		if (Context->GetMessageType() == MessageType::StaticStruct()->GetFName())
		{
			HandlerFunc(*static_cast<const MessageType*>(Context->GetMessage()), Context);
		}	
	}
	
private:

	/** Holds a pointer to the actual handler function. */
	FuncType HandlerFunc;
};
