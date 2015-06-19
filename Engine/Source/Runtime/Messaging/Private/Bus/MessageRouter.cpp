// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingPrivatePCH.h"


/* FMessageRouter structors
 *****************************************************************************/

FMessageRouter::FMessageRouter()
	: DelayedMessagesSequence(0)
	, Stopping(false)
	, Tracer(MakeShareable(new FMessageTracer()))
{
	ActiveSubscriptions.FindOrAdd(NAME_All);
	WorkEvent = FPlatformProcess::GetSynchEventFromPool(true);
}


FMessageRouter::~FMessageRouter()
{
	FPlatformProcess::ReturnSynchEventToPool(WorkEvent);
	WorkEvent = nullptr;
}


/* FRunnable interface
 *****************************************************************************/

bool FMessageRouter::Init()
{
	return true;
}


uint32 FMessageRouter::Run()
{
	CurrentTime = FDateTime::UtcNow();

	while (!Stopping)
	{
		if (WorkEvent->Wait(CalculateWaitTime()))
		{
			CurrentTime = FDateTime::UtcNow();

			CommandDelegate Command;

			while (Commands.Dequeue(Command))
			{
				Command.Execute();
			}

			WorkEvent->Reset();
		}

		ProcessDelayedMessages();
	}

	return 0;
}


void FMessageRouter::Stop()
{
	Tracer->Stop();

	Stopping = true;
}


void FMessageRouter::Exit()
{
	TArray<IReceiveMessagesWeakPtr> Recipients;

	// gather all subscribed and registered recipients
	for (TMap<FMessageAddress, IReceiveMessagesWeakPtr>::TConstIterator It(ActiveRecipients); It; ++It)
	{
		Recipients.AddUnique(It.Value());
	}

	for (TMap<FName, TArray<IMessageSubscriptionPtr> >::TIterator It(ActiveSubscriptions); It; ++It)
	{
		TArray<IMessageSubscriptionPtr>& Subscribers = It.Value();

		for (int32 Index = 0; Index < Subscribers.Num(); ++Index)
		{
			Recipients.AddUnique(Subscribers[Index]->GetSubscriber());
		}
	}
}


/* FMessageRouter implementation
 *****************************************************************************/

FTimespan FMessageRouter::CalculateWaitTime()
{
	FTimespan WaitTime = FTimespan::FromMilliseconds(100);

	if (DelayedMessages.Num() > 0)
	{
		FTimespan DelayedTime = DelayedMessages.HeapTop().Context->GetTimeSent() - CurrentTime;

		if (DelayedTime < WaitTime)
		{
			return DelayedTime;
		}
	}

	return WaitTime;
}


void FMessageRouter::DispatchMessage( const IMessageContextRef& Context )
{
	if (Context->IsValid())
	{
		TArray<IReceiveMessagesPtr> Recipients;

		// get recipients, either from the context...
		const TArray<FMessageAddress>& RecipientList = Context->GetRecipients();

		if (RecipientList.Num() > 0)
		{
			for (int32 Index = 0; Index < RecipientList.Num(); Index++)
			{
				IReceiveMessagesPtr Recipient = ActiveRecipients.FindRef(RecipientList[Index]).Pin();

				if (Recipient.IsValid())
				{
					Recipients.AddUnique(Recipient);
				}
				else
				{
					ActiveRecipients.Remove(RecipientList[Index]);
				}
			}
		}
		// ... or from subscriptions
		else
		{
			FilterSubscriptions(ActiveSubscriptions.FindOrAdd(Context->GetMessageType()), Context, Recipients);
			FilterSubscriptions(ActiveSubscriptions.FindOrAdd(NAME_All), Context, Recipients);
		}

		// dispatch the message
		for (int32 RecipientIndex = 0; RecipientIndex < Recipients.Num(); RecipientIndex++)
		{
			IReceiveMessagesPtr Recipient = Recipients[RecipientIndex];
			ENamedThreads::Type RecipientThread = Recipient->GetRecipientThread();

			if (RecipientThread == ENamedThreads::AnyThread)
			{
				Tracer->TraceDispatchedMessage(Context, Recipient.ToSharedRef(), false);
				Recipient->ReceiveMessage(Context);
				Tracer->TraceHandledMessage(Context, Recipient.ToSharedRef());
			}
			else
			{
				TGraphTask<FMessageDispatchTask>::CreateTask().ConstructAndDispatchWhenReady(RecipientThread, Context, Recipient, Tracer);
			}
		}
	}
}


void FMessageRouter::FilterSubscriptions( TArray<IMessageSubscriptionPtr>& Subscriptions, const IMessageContextRef& Context, TArray<IReceiveMessagesPtr>& OutRecipients )
{
	EMessageScope MessageScope = Context->GetScope();

	for (int32 SubscriptionIndex = 0; SubscriptionIndex < Subscriptions.Num(); ++SubscriptionIndex)
	{
		const IMessageSubscriptionPtr& Subscription = Subscriptions[SubscriptionIndex];

		if (Subscription->IsEnabled() && Subscription->GetScopeRange().Contains(MessageScope))
		{
			IReceiveMessagesPtr Subscriber = Subscription->GetSubscriber().Pin();

			if (Subscriber.IsValid())
			{
				if (MessageScope == EMessageScope::Thread)
				{
					ENamedThreads::Type RecipientThread = Subscriber->GetRecipientThread();
					ENamedThreads::Type SenderThread = Context->GetSenderThread();

					if (RecipientThread != SenderThread)
					{
						continue;
					}
				}

				OutRecipients.AddUnique(Subscriber);
			}
			else
			{
				Subscriptions.RemoveAtSwap(SubscriptionIndex);
				--SubscriptionIndex;
			}
		}
	}
}


void FMessageRouter::ProcessDelayedMessages()
{
	FDelayedMessage DelayedMessage;

	while ((DelayedMessages.Num() > 0) && (DelayedMessages.HeapTop().Context->GetTimeSent() <= CurrentTime))
	{
		DelayedMessages.HeapPop(DelayedMessage);
		DispatchMessage(DelayedMessage.Context.ToSharedRef());
	}
}


/* FMessageRouter callbacks
 *****************************************************************************/

void FMessageRouter::HandleAddInterceptor( IMessageInterceptorRef Interceptor, FName MessageType )
{
	ActiveInterceptors.FindOrAdd(MessageType).AddUnique(Interceptor);
	Tracer->TraceAddedInterceptor(Interceptor, MessageType);
}


void FMessageRouter::HandleAddRecipient( FMessageAddress Address, IReceiveMessagesWeakPtr RecipientPtr )
{
	IReceiveMessagesPtr Recipient = RecipientPtr.Pin();

	if (Recipient.IsValid())
	{
		ActiveRecipients.FindOrAdd(Address) = Recipient;
		Tracer->TraceAddedRecipient(Address, Recipient.ToSharedRef());
	}
}


void FMessageRouter::HandleAddSubscriber( IMessageSubscriptionRef Subscription )
{
	ActiveSubscriptions.FindOrAdd(Subscription->GetMessageType()).AddUnique(Subscription);
	Tracer->TraceAddedSubscription(Subscription);
}


void FMessageRouter::HandleRemoveInterceptor( IMessageInterceptorRef Interceptor, FName MessageType )
{
	if (MessageType == NAME_All)
	{
		for (TMap<FName, TArray<IMessageInterceptorPtr> >::TIterator It(ActiveInterceptors); It; ++It)
		{
			TArray<IMessageInterceptorPtr>& Interceptors = It.Value();
			Interceptors.Remove(Interceptor);
		}
	}
	else
	{
		TArray<IMessageInterceptorPtr>& Interceptors = ActiveInterceptors.FindOrAdd(MessageType);
		Interceptors.Remove(Interceptor);
	}

	Tracer->TraceRemovedInterceptor(Interceptor, MessageType);
}


void FMessageRouter::HandleRemoveRecipient( FMessageAddress Address )
{
	IReceiveMessagesPtr Recipient = ActiveRecipients.FindRef(Address).Pin();

	if (Recipient.IsValid())
	{
		ActiveRecipients.Remove(Address);
	}

	Tracer->TraceRemovedRecipient(Address);
}


void FMessageRouter::HandleRemoveSubscriber( IReceiveMessagesWeakPtr SubscriberPtr, FName MessageType )
{
	IReceiveMessagesPtr Subscriber = SubscriberPtr.Pin();

	if (Subscriber.IsValid())
	{
		for (TMap<FName, TArray<IMessageSubscriptionPtr> >::TIterator It(ActiveSubscriptions); It; ++It)
		{
			if ((MessageType == NAME_All) || (MessageType == It.Key()))
			{
				TArray<IMessageSubscriptionPtr>& Subsriptions = It.Value();

				for (int32 Index = 0; Index < Subsriptions.Num(); Index++)
				{
					if (Subsriptions[Index]->GetSubscriber().Pin() == Subscriber)
					{
						Subsriptions.RemoveAtSwap(Index);

						Tracer->TraceRemovedSubscription(Subsriptions[Index].ToSharedRef(), MessageType);

						break;
					}
				}
			}
		}
	}
}


void FMessageRouter::HandleRouteMessage( IMessageContextRef Context )
{
	// intercept routing
	TArray<IMessageInterceptorPtr>& Interceptors = ActiveInterceptors.FindOrAdd(Context->GetMessageType());

	for (TArray<IMessageInterceptorPtr>::TIterator It(Interceptors); It; ++It)
	{
		if ((*It)->InterceptMessage(Context))
		{
			Tracer->TraceInterceptedMessage(Context, It->ToSharedRef());

			return;
		}
	}

	// dispatch the message
	// @todo gmp: implement time synchronization between networked message endpoints
	if (false) //(Context->GetTimeSent() > CurrentTime)
	{
		DelayedMessages.HeapPush(FDelayedMessage(Context, ++DelayedMessagesSequence));
	}
	else
	{
		DispatchMessage(Context);
	}

	Tracer->TraceRoutedMessage(Context);
}
