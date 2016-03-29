// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMessageContext;


/**
 * Interface for message interceptors.
 */
class IMessageInterceptor
{
public:

	/**
	 * Gets the interceptor's name (for debugging purposes).
	 *
	 * @return The debug name.
	 * @see GetInterceptorId
	 */
	virtual FName GetDebugName() const = 0;

	/**
	 * Gets the interceptor's unique identifier (for debugging purposes).
	 *
	 * @return The interceptor's identifier.
	 * @see GetDebugName
	 */
	virtual const FGuid& GetInterceptorId() const = 0;

	/**
	 * Intercepts a message before it is being passed to the message router.
	 *
	 * @param Context The context of the message to intercept.
	 * @return true if the message was intercepted and should not be routed, false otherwise.
	 */
	virtual bool InterceptMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMessageInterceptor() { }
};


/** Type definition for shared pointers to instances of IInterceptMessages. */
typedef TSharedPtr<IMessageInterceptor, ESPMode::ThreadSafe> IMessageInterceptorPtr;

/** Type definition for shared references to instances of IInterceptMessages. */
typedef TSharedRef<IMessageInterceptor, ESPMode::ThreadSafe> IMessageInterceptorRef;
