// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


struct FMessageAddress;
class IMessageInterceptor;
class IReceiveMessages;


/**
 * Interface for classes that authorize message subscriptions.
 */
class IAuthorizeMessageRecipients
{
public:

	/**
	 * Authorizes a request to intercept messages of the specified type.
	 *
	 * @param Interceptor The message interceptor to authorize.
	 * @param MessageType The type of messages to intercept.
	 * @return true if the request was authorized, false otherwise.
	 */
	virtual bool AuthorizeInterceptor( const TSharedRef<IMessageInterceptor, ESPMode::ThreadSafe>& Interceptor, const FName& MessageType ) = 0;

	/**
	 * Authorizes a request to register the specified recipient.
	 *
	 * @param Recipient The recipient to register.
	 * @param Address The recipient's address.
	 * @return true if the request was authorized, false otherwise.
	 */
	virtual bool AuthorizeRegistration( const TSharedRef<IReceiveMessages, ESPMode::ThreadSafe>& Recipient, const FMessageAddress& Address ) = 0;

	/**
	 * Authorizes a request to add a subscription for the specified topic pattern.
	 *
	 * @param Subscriber The subscriber.
	 * @param TopicPattern The message topic pattern to subscribe to.
	 * @return true if the request is authorized, false otherwise.
	 */
	virtual bool AuthorizeSubscription( const TSharedRef<IReceiveMessages, ESPMode::ThreadSafe>& Subscriber, const FName& TopicPattern ) = 0;

	/**
	 * Authorizes a request to unregister the specified recipient.
	 *
	 * @param Address The address of the recipient to unregister.
	 * @return true if the request was authorized, false otherwise.
	 */
	virtual bool AuthorizeUnregistration( const FMessageAddress& Address ) = 0;

	/**
	 * Authorizes a request to remove a subscription for the specified topic pattern.
	 *
	 * @param Subscriber The subscriber.
	 * @param TopicPattern The message topic pattern to unsubscribe from.
	 * @return true if the request is authorized, false otherwise.
	 */
	virtual bool AuthorizeUnsubscription( const TSharedRef<IReceiveMessages, ESPMode::ThreadSafe>& Subscriber, const FName& TopicPattern ) = 0;

public:

	/** Virtual destructor. */
	virtual ~IAuthorizeMessageRecipients() { }
};


/** Type definition for shared pointers to instances of IAuthorizeMessageRecipients. */
typedef TSharedPtr<IAuthorizeMessageRecipients> IAuthorizeMessageRecipientsPtr;

/** Type definition for shared references to instances of IAuthorizeMessageRecipients. */
typedef TSharedRef<IAuthorizeMessageRecipients> IAuthorizeMessageRecipientsRef;
