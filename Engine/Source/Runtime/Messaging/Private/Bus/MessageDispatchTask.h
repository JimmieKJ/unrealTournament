// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements an asynchronous task for dispatching a message to a recipient.
 */
class FMessageDispatchTask
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InThread The name of the thread to dispatch the message on.
	 * @param InContext The context of the message to dispatch.
	 * @param InRecipient The message recipient.
	 * @param InTracer The message tracer to notify.
	 */
	FMessageDispatchTask( ENamedThreads::Type InThread, IMessageContextRef InContext, IReceiveMessagesWeakPtr InRecipient, FMessageTracerPtr InTracer )
		: Context(InContext)
		, RecipientPtr(InRecipient)
		, Thread(InThread)
		, TracerPtr(InTracer)
	{ }

public:

	/**
	 * Performs the actual task.
	 *
	 * @param CurrentThread The thread that this task is executing on.
	 * @param MyCompletionGraphEvent The completion event.
	 */
	void DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent )
	{
		IReceiveMessagesPtr Recipient = RecipientPtr.Pin();

		if (Recipient.IsValid())
		{
			FMessageTracerPtr Tracer = TracerPtr.Pin();

			if (Tracer.IsValid())
			{
				Tracer->TraceDispatchedMessage(Context, Recipient.ToSharedRef(), true);
			}
		
			Recipient->ReceiveMessage(Context);

			if (TracerPtr.IsValid())
			{
				Tracer->TraceHandledMessage(Context, Recipient.ToSharedRef());
			}
		}
	}
	
	/**
	 * Returns the name of the thread that this task should run on.
	 *
	 * @return Thread name.
	 */
	ENamedThreads::Type GetDesiredThread()
	{
		return Thread;
	}

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
	static ESubsequentsMode::Type GetSubsequentsMode() 
	{ 
		return ESubsequentsMode::TrackSubsequents; 
	}

private:

	/** Holds the message context. */
	IMessageContextRef Context;

	/** Holds a reference to the recipient. */
	IReceiveMessagesWeakPtr RecipientPtr;

	/** Holds the name of the thread that the router is running on. */
	ENamedThreads::Type Thread;

	/** Holds a pointer to the message tracer. */
	FMessageTracerWeakPtr TracerPtr;
};
