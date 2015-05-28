// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "IMessageInterceptor.h"
#include "IMessageSubscription.h"
#include "IMessageTracer.h"
#include "IMessageTracerBreakpoint.h"
#include "IReceiveMessages.h"


/**
 * Implements a message bus tracers.
 */
class FMessageTracer
	: public IMessageTracer
{
public:

	/** Default constructor. */
	FMessageTracer();

	/** Virtual destructor. */
	virtual ~FMessageTracer();

public:

	/**
	 * Notifies the tracer that a message interceptor has been added to the message bus.
	 *
	 * @param Interceptor The added interceptor.
	 * @param MessageType The type of messages being intercepted.
	 */
	void TraceAddedInterceptor( const IMessageInterceptorRef& Interceptor, const FName& MessageType );

	/**
	 * Notifies the tracer that a message recipient has been added to the message bus.
	 *
	 * @param Address The address of the added recipient.
	 * @param Recipient The added recipient.
	 */
	void TraceAddedRecipient( const FMessageAddress& Address, const IReceiveMessagesRef& Recipient );

	/**
	 * Notifies the tracer that a message subscription has been added to the message bus.
	 *
	 * @param Subscription The added subscription.
	 */
	void TraceAddedSubscription( const IMessageSubscriptionRef& Subscription );

	/**
	 * Notifies the tracer that a message has been dispatched.
	 *
	 * @param Context The context of the dispatched message.
	 * @param Recipient The message recipient.
	 * @param Async Whether the message was dispatched asynchronously.
	 */
	void TraceDispatchedMessage( const IMessageContextRef& Context, const IReceiveMessagesRef& Recipient, bool Async );

	/**
	 * Notifies the tracer that a message has been handled.
	 *
	 * @param Context The context of the dispatched message.
	 * @param Recipient The message recipient that handled the message.
	 */
	void TraceHandledMessage( const IMessageContextRef& Context, const IReceiveMessagesRef& Recipient );

	/**
	 * Notifies the tracer that a message has been intercepted.
	 *
	 * @param Context The context of the intercepted message.
	 * @param Interceptor The interceptor.
	 */
	void TraceInterceptedMessage( const IMessageContextRef& Context, const IMessageInterceptorRef& Interceptor );

	/**
	 * Notifies the tracer that a message interceptor has been removed from the message bus.
	 *
	 * @param Interceptor The removed interceptor.
	 * @param MessageType The type of messages that is no longer being intercepted.
	 */
	void TraceRemovedInterceptor( const IMessageInterceptorRef& Interceptor, const FName& MessageType );

	/**
	 * Notifies the tracer that a recipient has been removed from the message bus.
	 *
	 * @param Address The address of the removed recipient.
	 */
	void TraceRemovedRecipient( const FMessageAddress& Address );

	/**
	 * Notifies the tracer that a message subscription has been removed from the message bus.
	 *
	 * @param Subscriber The removed subscriber.
	 * @param MessageType The type of messages no longer being subscribed to.
	 */
	void TraceRemovedSubscription( const IMessageSubscriptionRef& Subscription, const FName& MessageType );

	/**
	 * Notifies the tracer that a message has been routed.
	 *
	 * @param Context The context of the routed message.
	 */
	void TraceRoutedMessage( const IMessageContextRef& Context );

	/**
	 * Notifies the tracer that a message has been sent.
	 *
	 * @param Context The context of the sent message.
	 */
	void TraceSentMessage( const IMessageContextRef& Context );

public:

	// IMessageTracer interface

	virtual void Break() override
	{
		Breaking = true;
	}

	virtual void Continue() override
	{
		if (!Breaking)
		{
			return;
		}

		Breaking = false;
		ContinueEvent->Trigger();
	}

	virtual int32 GetEndpoints( TArray<FMessageTracerEndpointInfoPtr>& OutEndpoints ) const override;
	virtual int32 GetMessages( TArray<FMessageTracerMessageInfoPtr>& OutMessages ) const override;
	virtual int32 GetMessageTypes( TArray<FMessageTracerTypeInfoPtr>& OutTypes ) const override;

	virtual bool HasMessages() const override
	{
		return (MessageInfos.Num() > 0);
	}

	virtual bool IsBreaking() const override
	{
		return Breaking;
	}

	virtual bool IsRunning() const override
	{
		return Running;
	}

	DECLARE_DERIVED_EVENT(FMessageTracer, IMessageTracer::FOnMessageAdded, FOnMessageAdded)
	virtual FOnMessageAdded& OnMessageAdded() override
	{
		return MessagesAddedDelegate;
	}

	DECLARE_DERIVED_EVENT(FMessageTracer, IMessageTracer::FOnMessagesReset, FOnMessagesReset)
	virtual FOnMessagesReset& OnMessagesReset() override
	{
		return MessagesResetDelegate;
	}

	DECLARE_DERIVED_EVENT(FMessageTracer, IMessageTracer::FOnTypeAdded, FOnTypeAdded)
	virtual FOnTypeAdded& OnTypeAdded() override
	{
		return TypeAddedDelegate;
	}

	virtual void Reset() override
	{
		ResetPending = true;
	}

	virtual void Start() override
	{
		if (Running)
		{
			return;
		}

		Running = true;
	}

	virtual void Step() override
	{
		if (!Breaking)
		{
			return;
		}

		ContinueEvent->Trigger();
	}

	virtual void Stop() override
	{
		if (!Running)
		{
			return;
		}

		Running = false;

		if (Breaking)
		{
			Breaking = false;
			ContinueEvent->Trigger();
		}
	}

	virtual bool Tick( float DeltaTime ) override;

protected:

	/**
	 * Resets traced messages.
	 */
	void ResetMessages();

	/**
	 * Checks whether the tracer should break on the given message.
	 *
	 * @param Context The context of the message to consider for breaking.
	 */
	bool ShouldBreak( const IMessageContextRef& Context ) const;

private:

	/** Holds the collection of endpoints for known message addresses. */
	TMap<FMessageAddress, FMessageTracerEndpointInfoPtr> AddressesToEndpointInfos;

	/** Holds a flag indicating whether a breakpoint was hit. */
	bool Breaking;

	/** Holds the collection of breakpoints. */
	TArray<IMessageTracerBreakpointPtr> Breakpoints;

	/** Holds the collection of message senders to break on when they send a message. */
	TArray<FMessageAddress> BreakOnSenders;

	/** Holds an event signaling that messaging routing can continue. */
	FEvent* ContinueEvent;

	/** Holds the collection of endpoints for known recipient identifiers. */
	TMap<FGuid, FMessageTracerEndpointInfoPtr> RecipientsToEndpointInfos;

	/** Holds the collection of known messages. */
	TMap<IMessageContextPtr, FMessageTracerMessageInfoPtr> MessageInfos;

	/** Holds the collection of known message types. */
	TMap<FName, FMessageTracerTypeInfoPtr> MessageTypes;

	/** Holds a flag indicating whether a reset is pending. */
	bool ResetPending;

	/** Holds a flag indicating whether the tracer is running. */
	bool Running;

	/** Handle to the registered TickDelegate. */
	FDelegateHandle TickDelegateHandle;

	/** Holds the trace actions queue. */
	TQueue<TFunction<void()>, EQueueMode::Mpsc> Traces;

private:

	/** Holds a delegate that is executed when a new message has been added to the collection of known messages. */
	FOnMessageAdded MessagesAddedDelegate;

	/** Holds a delegate that is executed when the message history has been reset. */
	FOnMessagesReset MessagesResetDelegate;

	/** Holds a delegate that is executed when a new type has been added to the collection of known message types. */
	FOnTypeAdded TypeAddedDelegate;
};


/** Type definition for weak pointers to instances of FMessageTracer. */
typedef TWeakPtr<FMessageTracer, ESPMode::ThreadSafe> FMessageTracerWeakPtr;

/** Type definition for shared pointers to instances of FMessageTracer. */
typedef TSharedPtr<FMessageTracer, ESPMode::ThreadSafe> FMessageTracerPtr;

/** Type definition for shared references to instances of FMessageTracer. */
typedef TSharedRef<FMessageTracer, ESPMode::ThreadSafe> FMessageTracerRef;
