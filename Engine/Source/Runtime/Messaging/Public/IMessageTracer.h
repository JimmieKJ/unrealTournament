// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMessageContext;


/** Type definition for shared pointers to instances of FMessageTracerAddressInfo. */
typedef TSharedPtr<struct FMessageTracerAddressInfo> FMessageTracerAddressInfoPtr;

/** Type definition for shared references to instances of FMessageTracerAddressInfo. */
typedef TSharedRef<struct FMessageTracerAddressInfo> FMessageTracerAddressInfoRef;

/** Type definition for shared pointers to instances of FMessageTracerDispatchState. */
typedef TSharedPtr<struct FMessageTracerDispatchState> FMessageTracerDispatchStatePtr;

/** Type definition for shared references to instances of FMessageTracerDispatchState. */
typedef TSharedRef<struct FMessageTracerDispatchState> FMessageTracerDispatchStateRef;

/** Type definition for shared pointers to instances of FMessageTracerEndpointInfo. */
typedef TSharedPtr<struct FMessageTracerEndpointInfo> FMessageTracerEndpointInfoPtr;

/** Type definition for shared references to instances of FMessageTracerEndpointInfo. */
typedef TSharedRef<struct FMessageTracerEndpointInfo> FMessageTracerEndpointInfoRef;

/** Type definition for shared pointers to instances of FMessageTracerMessageInfo. */
typedef TSharedPtr<struct FMessageTracerMessageInfo> FMessageTracerMessageInfoPtr;

/** Type definition for shared references to instances of FMessageTracerMessageInfo. */
typedef TSharedRef<struct FMessageTracerMessageInfo> FMessageTracerMessageInfoRef;

/** Type definition for shared pointers to instances of FMessageTracerTypeInfo. */
typedef TSharedPtr<struct FMessageTracerTypeInfo> FMessageTracerTypeInfoPtr;

/** Type definition for shared references to instances of FMessageTracerTypeInfo. */
typedef TSharedRef<struct FMessageTracerTypeInfo> FMessageTracerTypeInfoRef;


/**
 * Enumerates message dispatch types.
 */
enum class EMessageTracerDispatchTypes
{
	/** The message is being dispatched directly. */
	Direct,

	/** The message hasn't been dispatched yet. */
	Pending,

	/** The message is being dispatched using the task graph system. */
	TaskGraph
};


/**
 * Structure for message dispatch states.
 */
struct FMessageTracerDispatchState
{
	/** Holds the dispatch latency (in seconds). */
	double DispatchLatency;

	/** Holds the message's dispatch type for the specified endpoint. */
	EMessageTracerDispatchTypes DispatchType;

	/** Holds the endpoint to which the message was or is being dispatched. */
	FMessageTracerEndpointInfoPtr EndpointInfo;

	/** The thread on which the endpoint receives messages. */
	ENamedThreads::Type RecipientThread;

	/** Holds the time at which the message was dispatched. */
	double TimeDispatched;

	/** Holds the time at which the message was actually handled (0.0 = not handled yet). */
	double TimeHandled;
};


/**
 * Structure for a recipient's address information.
 */
struct FMessageTracerAddressInfo
{
	/** Holds a recipient's message address. */
	FMessageAddress Address;

	/** Holds the time at which this address was registered. */
	double TimeRegistered;

	/** Holds the time at which this address was unregistered. */
	double TimeUnregistered;
};


/**
 * Structure for message endpoint debug information.
 */
struct FMessageTracerEndpointInfo
{
	/** Holds the recipient's address information. */
	TMap<FMessageAddress, FMessageTracerAddressInfoPtr> AddressInfos;

	/** Holds the recipient's human readable name. */
	FName Name;

	/** Holds the list of messages received by this recipient. */
	TArray<FMessageTracerMessageInfoPtr> ReceivedMessages;

	/** Holds a flag indicating whether this is a remote recipient. */
	bool Remote;

	/** Holds the list of messages sent by this recipient. */
	TArray<FMessageTracerMessageInfoPtr> SentMessages;
};


/**
 * Structure for message debug information.
 */
struct FMessageTracerMessageInfo
{
	/** Holds a pointer to the message context. */
	IMessageContextPtr Context;

	/** Holds the message's dispatch states per endpoint. */
	TMap<FMessageTracerEndpointInfoPtr, FMessageTracerDispatchStatePtr> DispatchStates;

	/** Pointer to the sender's endpoint information. */
	FMessageTracerEndpointInfoPtr SenderInfo;

	/** Holds the time at which the message was routed (0.0 = pending). */
	double TimeRouted;

	/** Holds the time at which the message was sent. */
	double TimeSent;

	/** Pointer to the message's type information. */
	FMessageTracerTypeInfoPtr TypeInfo;
};


/**
 * Structure for message type debug information.
 */
struct FMessageTracerTypeInfo
{
	/** Holds the collection of messages of this type. */
	TArray<FMessageTracerMessageInfoPtr> Messages;

	/** Holds a name of the message type. */
	FName TypeName;
};


/**
 * Interface for message tracers.
 *
 * This interface provides access to the message tracer that is built into a message bus. Message tracers are
 * able to monitor and record all activities on a message bus. This includes the sending and receiving of messages,
 * the registering and unregistering of message endpoints and interceptors and adding and removal of subscriptions.
 */
class IMessageTracer
{
public:

	/**
	 * Breaks message routing.
	 *
	 * @see Continue, IsBreaking, Step
	 */
	virtual void Break() = 0;

	/**
	 * Continues message routing from the current breakpoint.
	 *
	 * @see Break, Step
	 */
	virtual void Continue() = 0;

	/**
	 * Checks whether the tracer is currently at a breakpoint.
	 *
	 * @return true if at breakpoint, false otherwise.
	 * @see Break
	 */
	virtual bool IsBreaking() const = 0;

	/**
	 * Checks whether the tracer is currently running.
	 *
	 * @return true if the tracer is running, false otherwise.
	 *
	 * @see Start, Stop
	 */
	virtual bool IsRunning() const = 0;

	/** Resets the tracer. */
	virtual void Reset() = 0;

	/**
	 * Starts the tracer.
	 *
	 * @see IsRunning, Stop
	 */
	virtual void Start() = 0;

	/**
	 * Steps the tracer to the next message.
	 *
	 * @see Break, Continue
	 */
	virtual void Step() = 0;

	/**
	 * Stops the tracer.
	 *
	 * @see IsRunning, Start
	 */
	virtual void Stop() = 0;

	/**
	 * Ticks the tracer.
	 *
	 * @param DeltaTime The time in seconds since the last tick.
	 * @return true if any events were processed.
	 */
	virtual bool Tick( float DeltaTime ) = 0;

public:

	/**
	 * Gets the list of known message endpoints.
	 *
	 * @param OutEndpoints Will contain the list of endpoints.
	 * @return The number of endpoints returned.
	 */
	virtual int32 GetEndpoints( TArray<FMessageTracerEndpointInfoPtr>& OutEndpoints ) const = 0;

	/**
	 * Gets the collection of known messages.
	 *
	 * @return The messages.
	 */
	virtual int32 GetMessages( TArray<FMessageTracerMessageInfoPtr>& OutMessages ) const = 0;

	/**
	 * Gets the list of known message types filtered by name.
	 *
	 * @param NameFilter The name substring to filter with.
	 * @param OutTypes Will contain the list of message types.
	 * @return The number of message types returned.
	 */
	virtual int32 GetMessageTypes( TArray<FMessageTracerTypeInfoPtr>& OutTypes ) const = 0;

	/**
	 * Checks whether there are any messages in the history.
	 *
	 * @return true if there are messages, false otherwise.
	 */
	virtual bool HasMessages() const = 0;

public:

	/**
	 * A delegate that is executed when the collection of known messages has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(IMessageTracer, FOnMessageAdded, FMessageTracerMessageInfoRef)
	virtual FOnMessageAdded& OnMessageAdded() = 0;

	/**
	 * A delegate that is executed when the message history has been reset.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT(IMessageTracer, FOnMessagesReset)
	virtual FOnMessagesReset& OnMessagesReset() = 0;

	/**
	 * A delegate that is executed when the collection of known messages types has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(IMessageTracer, FOnTypeAdded, FMessageTracerTypeInfoRef)
	virtual FOnTypeAdded& OnTypeAdded() = 0;

protected:

	/** Hidden destructor. The life time of a message tracer is managed by the message bus. */
	virtual ~IMessageTracer() { }
};


/** Type definition for shared pointers to instances of IMessageTracer. */
typedef TSharedPtr<IMessageTracer, ESPMode::ThreadSafe> IMessageTracerPtr;

/** Type definition for shared references to instances of IMessageTracer. */
typedef TSharedRef<IMessageTracer, ESPMode::ThreadSafe> IMessageTracerRef;
