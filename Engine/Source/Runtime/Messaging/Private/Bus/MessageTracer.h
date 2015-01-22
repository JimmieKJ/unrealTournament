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
	/** Holds debug information for message recipients. */
	struct FRecipientInfo
	{
		/** Holds the recipient's unique identifier. */
		FGuid Id;

		/** Holds the recipient's name. */
		FName Name;

		/** Holds a flag indicating whether the recipient is a remote endpoint. */
		bool Remote;

		/** Creates and initializes a new instance. */
		FRecipientInfo( const FMessageAddress& Address, const IReceiveMessagesRef& Recipient )
			: Id(Recipient->GetRecipientId())
			, Name(Recipient->GetDebugName())
			, Remote(Recipient->IsRemote())
		{ }
	};

public:

	DECLARE_DELEGATE(TraceDelegate)

	/** Default constructor. */
	FMessageTracer();

	/** Destructor. */
	~FMessageTracer();

public:

	/**
	 * Notifies the tracer that a message interceptor has been added to the message bus.
	 *
	 * @param Interceptor The added interceptor.
	 * @param MessageType The type of messages being intercepted.
	 */
	void TraceAddedInterceptor( const IMessageInterceptorRef& Interceptor, const FName& MessageType )
	{
		FString Name = TEXT("@todo");
		Traces.Enqueue(FSimpleDelegate::CreateRaw(this, &FMessageTracer::ProcessAddedInterceptor, Name, MessageType, FPlatformTime::Seconds()));
	}

	/**
	 * Notifies the tracer that a message recipient has been added to the message bus.
	 *
	 * @param Address The address of the added recipient.
	 * @param Recipient The added recipient.
	 */
	void TraceAddedRecipient( const FMessageAddress& Address, const IReceiveMessagesRef& Recipient )
	{
		Traces.Enqueue(FSimpleDelegate::CreateRaw(this, &FMessageTracer::ProcessAddedRecipient, Address, FRecipientInfo(Address, Recipient), FPlatformTime::Seconds()));
	}

	/**
	 * Notifies the tracer that a message subscription has been added to the message bus.
	 *
	 * @param Subscription The added subscription.
	 */
	void TraceAddedSubscription( const IMessageSubscriptionRef& Subscription )
	{
		if (!Running)
		{
			return;
		}

		// @todo gmp: trace added subscriptions
	}

	/**
	 * Notifies the tracer that a message has been dispatched.
	 *
	 * @param Context The context of the dispatched message.
	 * @param Recipient The message recipient.
	 * @param Async Whether the message was dispatched asynchronously.
	 */
	void TraceDispatchedMessage( const IMessageContextRef& Context, const IReceiveMessagesRef& Recipient, bool Async )
	{
		if (!Running)
		{
			return;
		}

		Traces.Enqueue(FSimpleDelegate::CreateRaw(this, &FMessageTracer::ProcessDispatchedMessage, Context, FPlatformTime::Seconds(), Recipient->GetRecipientId(), Async));
	}

	/**
	 * Notifies the tracer that a message has been handled.
	 *
	 * @param Context The context of the dispatched message.
	 * @param Recipient The message recipient that handled the message.
	 */
	void TraceHandledMessage( const IMessageContextRef& Context, const IReceiveMessagesRef& Recipient )
	{
		if (!Running)
		{
			return;
		}

		Traces.Enqueue(FSimpleDelegate::CreateRaw(this, &FMessageTracer::ProcessHandledMessage, Context, FPlatformTime::Seconds(), Recipient->GetRecipientId()));
	}

	/**
	 * Notifies the tracer that a message has been intercepted.
	 *
	 * @param Context The context of the intercepted message.
	 * @param Interceptor The interceptor.
	 */
	void TraceInterceptedMessage( const IMessageContextRef& Context, const IMessageInterceptorRef& Interceptor )
	{
		if (Running)
		{
		}

		// @todo gmp: trace intercepted messages
	}

	/**
	 * Notifies the tracer that a message interceptor has been removed from the message bus.
	 *
	 * @param Interceptor The removed interceptor.
	 * @param MessageType The type of messages that is no longer being intercepted.
	 */
	void TraceRemovedInterceptor( const IMessageInterceptorRef& Interceptor, const FName& MessageType )
	{
		if (!Running)
		{
			return;
		}

		// @todo gmp: trace removed interceptors
	}

	/**
	 * Notifies the tracer that a recipient has been removed from the message bus.
	 *
	 * @param Address The address of the removed recipient.
	 */
	void TraceRemovedRecipient( const FMessageAddress& Address )
	{
		if (!Running)
		{
			return;
		}

		// @todo gmp: trace removed recipients
	}

	/**
	 * Notifies the tracer that a message subscription has been removed from the message bus.
	 *
	 * @param Subscriber The removed subscriber.
	 * @param MessageType The type of messages no longer being subscribed to.
	 */
	void TraceRemovedSubscription( const IMessageSubscriptionRef& Subscription, const FName& MessageType )
	{
		if (!Running)
		{
			return;
		}

		// @todo gmp: trace removed subscriptions
	}

	/**
	 * Notifies the tracer that a message has been routed.
	 *
	 * @param Context The context of the routed message.
	 */
	void TraceRoutedMessage( const IMessageContextRef& Context )
	{
		if (!Running)
		{
			return;
		}

		if (ShouldBreak(Context))
		{
			Breaking = true;
			ContinueEvent->Wait();
		}

		Traces.Enqueue(FSimpleDelegate::CreateRaw(this, &FMessageTracer::ProcessRoutedMessage, Context, FPlatformTime::Seconds()));
	}

	/**
	 * Notifies the tracer that a message has been sent.
	 *
	 * @param Context The context of the sent message.
	 */
	void TraceSentMessage( const IMessageContextRef& Context )
	{
		if (!Running)
		{
			return;
		}

		Traces.Enqueue(FSimpleDelegate::CreateRaw(this, &FMessageTracer::ProcessSentMessage, Context, FPlatformTime::Seconds()));
	}

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
	 * Enqueues a trace action for synchronized processing.
	 *
	 * @param Trace The action to enqueue.
	 */
	FORCEINLINE void EnqueueTrace( TraceDelegate Trace )
	{
		Traces.Enqueue(Trace);
	}

	/**
	 * Processes traces for added message interceptors.
	 *
	 * @param Name The name of the interceptor.
	 * @param MessageType The type of messages being intercepted.
	 * @param TimeSecond The time at which the interceptor was added.
	 */
	void ProcessAddedInterceptor( FString Name, FName MessageType, double TimeSeconds );

	/**
	 * Processes traces for added message recipients.
	 *
	 * @param Address The address of the added recipient.
	 * @param RecipientInfo Information about the recipient (name, ID, etc.)
	 * @param TimeSecond The time at which the recipient was added.
	 */
	void ProcessAddedRecipient( FMessageAddress Address, FRecipientInfo RecipientInfo, double TimeSeconds );

	/**
	 * Processes traces for added message subscriptions.
	 *
	 * @param TimeSecond The time at which the subscription was added.
	 */
	void ProcessAddedSubscriptionTrace( double TimeSeconds );

	/**
	 * Processes traces for dispatched messages.
	 *
	 * @param Context The context of the dispatched message.
	 * @param TimeSecond The time at which the message was dispatched.
	 * @param RecipientId The recipient's unique identifier.
	 * @param Async Whether the message was dispatched asynchronously.
	 */
	void ProcessDispatchedMessage( IMessageContextRef Context, double TimeSeconds, FGuid RecipientId, bool Async );

	/**
	 * Processes traces for handled messages.
	 *
	 * @param Context The context of the handled message.
	 * @param TimeSecond The time at which the message was handled.
	 * @param RecipientId The recipient's unique identifier.
	 */
	void ProcessHandledMessage( IMessageContextRef Context, double TimeSeconds, FGuid RecipientId );

	/**
	 * Processes traces for removed message interceptors.
	 *
	 * @param Interceptor The removed interceptor.
	 * @param MessageType The type of messages no longer being intercepted.
	 * @param TimeSecond The time at which the interceptor was removed.
	 */
	void ProcessRemovedInterceptor( IMessageInterceptorRef Interceptor, FName MessageType, double TimeSeconds );

	/**
	 * Processes traces for removed message recipients.
	 *
	 * @param Address The address of the removed recipient.
	 * @param TimeSecond The time at which the recipient was removed.
	 */
	void ProcessRemovedRecipient( FMessageAddress Address, double TimeSeconds );

	/**
	 * Processes traces for removed message subscriptions.
	 *
	 * @param MessageType The type of messages no longer being subscribed to.
	 * @param TimeSecond The time at which the subscription was removed.
	 */
	void ProcessRemovedSubscription( FName MessageType, double TimeSeconds );

	/**
	 * Processes traces for routed messages.
	 *
	 * @param Context The context of the routed message.
	 * @param TimeSecond The time at which the message was routed.
	 */
	void ProcessRoutedMessage( IMessageContextRef Context, double TimeSeconds );

	/**
	 * Processes traces for sent messages.
	 *
	 * @param Context The context of the sent message.
	 * @param SenderThread The name of the thread from which the message was sent.
	 * @param TimeSecond The time at which the message was sent.
	 */
	void ProcessSentMessage( IMessageContextRef Context, double TimeSeconds );

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

	/** Holds the trace actions queue. */
	TQueue<TraceDelegate, EQueueMode::Mpsc> Traces;

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
