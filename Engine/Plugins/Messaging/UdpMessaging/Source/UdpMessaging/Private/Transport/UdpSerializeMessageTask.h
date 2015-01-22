// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "JsonStructSerializerBackend.h"
#include "StructSerializer.h"


/**
 * Implements an asynchronous task for serializing a message.
 */
class FUdpSerializeMessageTask
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InMessageContext The context of the message to serialize.
	 * @param InSerializedMessage Will hold the serialized message data.
	 */
	FUdpSerializeMessageTask( IMessageContextRef InMessageContext, FUdpSerializedMessageRef InSerializedMessage )
		: MessageContext(InMessageContext)
		, SerializedMessage(InSerializedMessage)
	{ }

public:

	/**
	 * Performs the actual task.
	 *
	 * @param CurrentThread The thread that this task is executing on.
	 * @param MyCompletionGraphEvent The completion event.
	 */
	void DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent );
	
	/**
	 * Returns the name of the thread that this task should run on.
	 *
	 * @return Thread name.
	 */
	ENamedThreads::Type GetDesiredThread();

	/**
	 * Gets the task's stats tracking identifier.
	 *
	 * @return Stats identifier.
	 */
	TStatId GetStatId() const;

	/**
	 * Gets the mode for tracking subsequent tasks.
	 *
	 * @return Always track subsequent tasks.
	 */
	static ESubsequentsMode::Type GetSubsequentsMode();

private:

	/** Holds the context of the message to serialize. */
	IMessageContextRef MessageContext;

	/** Holds a reference to the serialized message data. */
	FUdpSerializedMessageRef SerializedMessage;
};
