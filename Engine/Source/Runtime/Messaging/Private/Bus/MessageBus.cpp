// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingPrivatePCH.h"


/* FMessageBus structors
 *****************************************************************************/

FMessageBus::FMessageBus( const IAuthorizeMessageRecipientsPtr& InRecipientAuthorizer )
	: RecipientAuthorizer(InRecipientAuthorizer)
{
	Router = new FMessageRouter();
	RouterThread = FRunnableThread::Create(Router, TEXT("FMessageBus.Router"), 128 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());

	check(Router != nullptr);
}


FMessageBus::~FMessageBus( )
{
	Shutdown();

	delete Router;
}


/* IMessageBus interface
 *****************************************************************************/

void FMessageBus::Forward( const IMessageContextRef& Context, const TArray<FMessageAddress>& Recipients, const FTimespan& Delay, const ISendMessagesRef& Forwarder )
{
	Router->RouteMessage(MakeShareable(new FMessageContext(Context, Forwarder->GetSenderAddress(), Recipients, EMessageScope::Process, FDateTime::UtcNow() + Delay, FTaskGraphInterface::Get().GetCurrentThreadIfKnown())));
}


IMessageTracerRef FMessageBus::GetTracer()
{
	return Router->GetTracer();
}


void FMessageBus::Intercept( const IMessageInterceptorRef& Interceptor, const FName& MessageType )
{
	if (MessageType != NAME_None)
	{
		if (!RecipientAuthorizer.IsValid() || RecipientAuthorizer->AuthorizeInterceptor(Interceptor, MessageType))
		{
			Router->AddInterceptor(Interceptor, MessageType);
		}			
	}
}


FOnMessageBusShutdown& FMessageBus::OnShutdown()
{
	return ShutdownDelegate;
}


void FMessageBus::Publish( void* Message, UScriptStruct* TypeInfo, EMessageScope Scope, const FTimespan& Delay, const FDateTime& Expiration, const ISendMessagesRef& Publisher )
{
	Router->RouteMessage(MakeShareable(new FMessageContext(Message, TypeInfo, nullptr, Publisher->GetSenderAddress(), TArray<FMessageAddress>(), Scope, FDateTime::UtcNow() + Delay, Expiration, FTaskGraphInterface::Get().GetCurrentThreadIfKnown())));
}


void FMessageBus::Register( const FMessageAddress& Address, const IReceiveMessagesRef& Recipient )
{
	Router->AddRecipient(Address, Recipient);
}


void FMessageBus::Send( void* Message, UScriptStruct* TypeInfo, const IMessageAttachmentPtr& Attachment, const TArray<FMessageAddress>& Recipients, const FTimespan& Delay, const FDateTime& Expiration, const ISendMessagesRef& Sender )
{
	Router->RouteMessage(MakeShareable(new FMessageContext(Message, TypeInfo, Attachment, Sender->GetSenderAddress(), Recipients, EMessageScope::Network, FDateTime::UtcNow() + Delay, Expiration, FTaskGraphInterface::Get().GetCurrentThreadIfKnown())));
}


void FMessageBus::Shutdown( )
{
	if (RouterThread != nullptr)
	{
		ShutdownDelegate.Broadcast();

		RouterThread->Kill(true);
		delete RouterThread;
		RouterThread = nullptr;
	}
}


IMessageSubscriptionPtr FMessageBus::Subscribe( const IReceiveMessagesRef& Subscriber, const FName& MessageType, const FMessageScopeRange& ScopeRange )
{
	if (MessageType != NAME_None)
	{
		if (!RecipientAuthorizer.IsValid() || RecipientAuthorizer->AuthorizeSubscription(Subscriber, MessageType))
		{
			IMessageSubscriptionRef Subscription = MakeShareable(new FMessageSubscription(Subscriber, MessageType, ScopeRange));
			Router->AddSubscription(Subscription);

			return Subscription;
		}
	}

	return nullptr;
}


void FMessageBus::Unintercept( const IMessageInterceptorRef& Interceptor, const FName& MessageType )
{
	if (MessageType != NAME_None)
	{
		Router->RemoveInterceptor(Interceptor, MessageType);
	}
}


void FMessageBus::Unregister( const FMessageAddress& Address )
{
	if (!RecipientAuthorizer.IsValid() || RecipientAuthorizer->AuthorizeUnregistration(Address))
	{
		Router->RemoveRecipient(Address);
	}
}


void FMessageBus::Unsubscribe( const IReceiveMessagesRef& Subscriber, const FName& MessageType )
{
	if (MessageType != NAME_None)
	{
		if (!RecipientAuthorizer.IsValid() || RecipientAuthorizer->AuthorizeUnsubscription(Subscriber, MessageType))
		{
			Router->RemoveSubscription(Subscriber, MessageType);
		}
	}
}
