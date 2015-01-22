// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAuthorizeMessageRecipients.h"
#include "IMessageAttachment.h"
#include "IMessageBus.h"
#include "IMessageContext.h"
#include "IMessageInterceptor.h"
#include "IMessageSubscription.h"
#include "IReceiveMessages.h"
#include "ISendMessages.h"


// forward declarations
class FMessageRouter;
class FRunnableThread;


/**
 * Implements a message bus.
 */
class FMessageBus
	: public TSharedFromThis<FMessageBus, ESPMode::ThreadSafe>
	, public IMessageBus
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InRecipientAuthorizer An optional recipient authorizer.
	 */
	FMessageBus( const IAuthorizeMessageRecipientsPtr& InRecipientAuthorizer );

	/** Destructor. */
	~FMessageBus();

public:

	// IMessageBus interface

	virtual void Forward( const IMessageContextRef& Context, const TArray<FMessageAddress>& Recipients, const FTimespan& Delay, const ISendMessagesRef& Forwarder ) override;
	virtual IMessageTracerRef GetTracer() override;
	virtual void Intercept( const IMessageInterceptorRef& Interceptor, const FName& MessageType ) override;
	virtual FOnMessageBusShutdown& OnShutdown() override;
	virtual void Publish( void* Message, UScriptStruct* TypeInfo, EMessageScope Scope, const FTimespan& Delay, const FDateTime& Expiration, const ISendMessagesRef& Publisher ) override;
	virtual void Register( const FMessageAddress& Address, const IReceiveMessagesRef& Recipient ) override;
	virtual void Send( void* Message, UScriptStruct* TypeInfo, const IMessageAttachmentPtr& Attachment, const TArray<FMessageAddress>& Recipients, const FTimespan& Delay, const FDateTime& Expiration, const ISendMessagesRef& Sender ) override;
	virtual void Shutdown() override;
	virtual IMessageSubscriptionPtr Subscribe( const IReceiveMessagesRef& Subscriber, const FName& MessageType, const FMessageScopeRange& ScopeRange ) override;
	virtual void Unintercept( const IMessageInterceptorRef& Interceptor, const FName& MessageType ) override;
	virtual void Unregister( const FMessageAddress& Address ) override;
	virtual void Unsubscribe( const IReceiveMessagesRef& Subscriber, const FName& MessageType ) override;

private:

	/** Holds the message router. */
	FMessageRouter* Router;

	/** Holds the message router thread. */
	FRunnableThread* RouterThread;

	/** Holds the recipient authorizer. */
	IAuthorizeMessageRecipientsPtr RecipientAuthorizer;

	/** Holds bus shutdown delegate. */
	FOnMessageBusShutdown ShutdownDelegate;
};
