// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "IMessageInterceptor.h"
#include "IMessageSubscription.h"
#include "IMessageTracer.h"
#include "IReceiveMessages.h"
#include "ISendMessages.h"


/**
 * Implements a topic-based message router.
 */
class FMessageRouter
	: public FRunnable
{
	DECLARE_DELEGATE(CommandDelegate)

public:

	/** Default constructor. */
	FMessageRouter();

	/** Destructor. */
	~FMessageRouter();

public:

	/**
	 * Adds a message interceptor.
	 *
	 * @param Interceptor The interceptor to add.
	 * @param MessageType The type of messages to intercept.
	 */
	FORCEINLINE void AddInterceptor( const IMessageInterceptorRef& Interceptor, const FName& MessageType )
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleAddInterceptor, Interceptor, MessageType));
	}

	/**
	 * Adds a recipient.
	 *
	 * @param Address The address of the recipient to add.
	 * @param Recipient The recipient.
	 */
	FORCEINLINE void AddRecipient( const FMessageAddress& Address, const IReceiveMessagesRef& Recipient )
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleAddRecipient, Address, IReceiveMessagesWeakPtr(Recipient)));
	}

	/**
	 * Adds a subscription.
	 *
	 * @param Subscription The subscription to add.
	 */
	FORCEINLINE void AddSubscription( const IMessageSubscriptionRef& Subscription )
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleAddSubscriber, Subscription));
	}

	/**
	 * Gets the message tracer.
	 *
	 * @return Weak pointer to the message tracer.
	 */
	FORCEINLINE IMessageTracerRef GetTracer()
	{
		return Tracer;
	}

	/**
	 * Removes a message interceptor.
	 *
	 * @param Interceptor The interceptor to remove.
	 * @param MessageType The type of messages to stop intercepting.
	 */
	FORCEINLINE void RemoveInterceptor( const IMessageInterceptorRef& Interceptor, const FName& MessageType )
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleRemoveInterceptor, Interceptor, MessageType));
	}

	/**
	 * Removes a recipient.
	 *
	 * @param Address The address of the recipient to remove.
	 */
	FORCEINLINE void RemoveRecipient( const FMessageAddress& Address )
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleRemoveRecipient, Address));
	}

	/**
	 * Removes a subscription.
	 *
	 * @param Subscriber The subscriber to stop routing messages to.
	 * @param MessageType The type of message to unsubscribe from (NAME_None = all types).
	 */
	FORCEINLINE void RemoveSubscription( const IReceiveMessagesRef& Subscriber, const FName& MessageType )
	{
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleRemoveSubscriber, IReceiveMessagesWeakPtr(Subscriber), MessageType));
	}

	/**
	 * Routes a message to the specified recipients.
	 *
	 * @param Context The context of the message to route.
	 */
	FORCEINLINE void RouteMessage( const IMessageContextRef& Context )
	{
		Tracer->TraceSentMessage(Context);
		EnqueueCommand(FSimpleDelegate::CreateRaw(this, &FMessageRouter::HandleRouteMessage, Context));
	}

public:

	// FRunnable interface

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

protected:

	/**
	 * Calculates the time that the thread will wait for new work.
	 *
	 * @return Wait time.
	 */
	FTimespan CalculateWaitTime();

	/**
	 * Queues up a router command.
	 *
	 * @param Command The command to queue up.
	 * @return true if the command was enqueued, false otherwise.
	 */
	FORCEINLINE bool EnqueueCommand( CommandDelegate Command )
	{
		if (!Commands.Enqueue(Command))
		{
			return false;
		}

		WorkEvent->Trigger();

		return true;
	}

	/**
	 * Filters a collection of subscriptions using the given message context.
	 *
	 * @param Subscriptions The subscriptions to filter.
	 * @param Context The message context to filter by.
	 * @param Sender The message sender (may be nullptr if the sender has no subscriptions).
	 * @param OutRecipients Will hold the collection of recipients.
	 */
	void FilterSubscriptions( TArray<IMessageSubscriptionPtr>& Subscriptions, const IMessageContextRef& Context, TArray<IReceiveMessagesPtr>& OutRecipients );

	/**
	 * Dispatches a single message to its recipients.
	 *
	 * @param Message The message to dispatch.
	 */
	void DispatchMessage( const IMessageContextRef& Message );

	/** Processes all delayed messages. */
	void ProcessDelayedMessages();

private:

	// Structure for delayed messages.
	struct FDelayedMessage
	{
		// Holds the context of the delayed message.
		IMessageContextPtr Context;

		// Holds a sequence number used by the delayed message queue.
		int64 Sequence;


		// Default constructor.
		FDelayedMessage() { }

		// Creates and initializes a new instance.
		FDelayedMessage( const IMessageContextRef& InContext, int64 InSequence )
			: Context(InContext)
			, Sequence(InSequence)
		{ }

		// Comparison operator for heap sorting.
		bool operator<( const FDelayedMessage& Other ) const
		{
			const FTimespan Difference = Other.Context->GetTimeSent() - Context->GetTimeSent();

			if (Difference == FTimespan::Zero())
			{
				return (Sequence < Other.Sequence);
			}

			return (Difference > FTimespan::Zero());
		}
	};

private:

	/** Handles adding message interceptors. */
	void HandleAddInterceptor( IMessageInterceptorRef Interceptor, FName MessageType );

	/** Handles adding message recipients. */
	void HandleAddRecipient( FMessageAddress Address, IReceiveMessagesWeakPtr RecipientPtr );

	/** Handles adding of subscriptions. */
	void HandleAddSubscriber( IMessageSubscriptionRef Subscription );

	/** Handles the removal of message interceptors. */
	void HandleRemoveInterceptor( IMessageInterceptorRef Interceptor, FName MessageType );

	/** Handles the removal of message recipients. */
	void HandleRemoveRecipient( FMessageAddress Address );

	/** Handles the removal of subscribers. */
	void HandleRemoveSubscriber( IReceiveMessagesWeakPtr SubscriberPtr, FName MessageType );

	/** Handles the routing of messages. */
	void HandleRouteMessage( IMessageContextRef Context );

private:

	/** Maps message types to interceptors. */
	TMap<FName, TArray<IMessageInterceptorPtr>> ActiveInterceptors;

	/** Maps message addresses to recipients. */
	TMap<FMessageAddress, IReceiveMessagesWeakPtr> ActiveRecipients;

	/** Maps message types to subscriptions. */
	TMap<FName, TArray<IMessageSubscriptionPtr>> ActiveSubscriptions;

	/** Holds the router command queue. */
	TQueue<CommandDelegate, EQueueMode::Mpsc> Commands;

	/** Holds the current time. */
	FDateTime CurrentTime;

	/** Holds the collection of delayed messages. */
	TArray<FDelayedMessage> DelayedMessages;

	/** Holds a sequence number for delayed messages. */
	int64 DelayedMessagesSequence;

	/** Holds a flag indicating that the thread is stopping. */
	bool Stopping;

	/** Holds the message tracer. */
	FMessageTracerRef Tracer;

	/** Holds an event signaling that work is available. */
	FEvent* WorkEvent;
};
