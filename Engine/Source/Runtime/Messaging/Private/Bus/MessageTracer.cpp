// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingPrivatePCH.h"


/* FMessageTracer structors
 *****************************************************************************/

FMessageTracer::FMessageTracer()
	: Breaking(false)
	, ResetPending(false)
	, Running(false)
{
	ContinueEvent = FPlatformProcess::CreateSynchEvent();
}


FMessageTracer::~FMessageTracer()
{
	delete ContinueEvent;
}


/* IMessageTracer interface
 *****************************************************************************/

int32 FMessageTracer::GetEndpoints( TArray<FMessageTracerEndpointInfoPtr>& OutEndpoints ) const
{
	RecipientsToEndpointInfos.GenerateValueArray(OutEndpoints);

	return OutEndpoints.Num();
}


int32 FMessageTracer::GetMessages( TArray<FMessageTracerMessageInfoPtr>& OutMessages ) const
{
	MessageInfos.GenerateValueArray(OutMessages);

	return OutMessages.Num();
}


int32 FMessageTracer::GetMessageTypes( TArray<FMessageTracerTypeInfoPtr>& OutTypes ) const
{
	MessageTypes.GenerateValueArray(OutTypes);

	return OutTypes.Num();
}


bool FMessageTracer::Tick( float DeltaTime )
{
	if (ResetPending)
	{
		ResetMessages();
		ResetPending = false;
	}

	if (Traces.IsEmpty())
	{
		return false;
	}

	// process new traces
	TraceDelegate Trace;

	while (Traces.Dequeue(Trace))
	{
		Trace.Execute();
	}

	return true;
}


/* FMessageTracer implementation
 *****************************************************************************/

void FMessageTracer::ProcessAddedInterceptor( FString Name, FName MessageType, double TimeSeconds )
{
	
}


void FMessageTracer::ProcessAddedRecipient( FMessageAddress Address, FRecipientInfo RecipientInfo, double TimeSeconds )
{
	// create endpoint information
	FMessageTracerEndpointInfoPtr& EndpointInfo = RecipientsToEndpointInfos.FindOrAdd(RecipientInfo.Id);

	if (!EndpointInfo.IsValid())
	{
		EndpointInfo = MakeShareable(new FMessageTracerEndpointInfo());
	}

	// initialize endpoint information
	FMessageTracerAddressInfoRef AddressInfo = MakeShareable(new FMessageTracerAddressInfo());
	AddressInfo->Address = Address;
	AddressInfo->TimeRegistered = TimeSeconds;
	AddressInfo->TimeUnregistered = 0;

	EndpointInfo->AddressInfos.Add(Address, AddressInfo);
	EndpointInfo->Name = RecipientInfo.Name;
	EndpointInfo->Remote = RecipientInfo.Remote;

	// add to address table
	AddressesToEndpointInfos.Add(Address, EndpointInfo);
}


void FMessageTracer::ProcessAddedSubscriptionTrace( double TimeSeconds )
{

}


void FMessageTracer::ProcessDispatchedMessage( IMessageContextRef Context, double TimeSeconds, FGuid RecipientId, bool Async )
{
	FMessageTracerMessageInfoPtr MessageInfo = MessageInfos.FindRef(Context);

	if (!MessageInfo.IsValid())
	{
		return;
	}

	FMessageTracerEndpointInfoPtr& EndpointInfo = RecipientsToEndpointInfos.FindOrAdd(RecipientId);

	if (!EndpointInfo.IsValid())
	{
		return;
	}

	// update message information
	FMessageTracerDispatchStateRef DispatchState = MakeShareable(new FMessageTracerDispatchState());

	DispatchState->DispatchLatency = TimeSeconds - MessageInfo->TimeSent;
	DispatchState->DispatchType = Async ? EMessageTracerDispatchTypes::TaskGraph : EMessageTracerDispatchTypes::Direct;
	DispatchState->EndpointInfo = EndpointInfo;
	DispatchState->TimeDispatched = TimeSeconds;
	DispatchState->TimeHandled = 0.0;

	MessageInfo->DispatchStates.Add(EndpointInfo, DispatchState);

	// update database
	EndpointInfo->ReceivedMessages.Add(MessageInfo);
}


void FMessageTracer::ProcessHandledMessage( IMessageContextRef Context, double TimeSeconds, FGuid RecipientId )
{
	FMessageTracerMessageInfoPtr MessageInfo = MessageInfos.FindRef(Context);

	if (!MessageInfo.IsValid())
	{
		return;
	}

	FMessageTracerEndpointInfoPtr EndpointInfo = RecipientsToEndpointInfos.FindRef(RecipientId);

	if (!EndpointInfo.IsValid())
	{
		return;
	}

	// update message information
	FMessageTracerDispatchStatePtr DispatchState = MessageInfo->DispatchStates.FindRef(EndpointInfo);

	if (DispatchState.IsValid())
	{
		DispatchState->TimeHandled = TimeSeconds;
	}
}


void FMessageTracer::ProcessRemovedInterceptor( IMessageInterceptorRef Interceptor, FName MessageType, double TimeSeconds )
{

}


void FMessageTracer::ProcessRemovedRecipient( FMessageAddress Address, double TimeSeconds )
{
	FMessageTracerEndpointInfoPtr EndpointInfo = AddressesToEndpointInfos.FindRef(Address);

	if (!EndpointInfo.IsValid())
	{
		return;
	}

	// update endpoint information
	FMessageTracerAddressInfoPtr AddressInfo = EndpointInfo->AddressInfos.FindRef(Address);

	if (AddressInfo.IsValid())
	{
		AddressInfo->TimeUnregistered = TimeSeconds;
	}
}


void FMessageTracer::ProcessRemovedSubscription( FName MessageType, double TimeSeconds )
{

}


void FMessageTracer::ProcessRoutedMessage( IMessageContextRef Context, double TimeSeconds )
{
	// update message information
	FMessageTracerMessageInfoPtr MessageInfo = MessageInfos.FindRef(Context);

	if (MessageInfo.IsValid())
	{
		MessageInfo->TimeRouted = TimeSeconds;
	}
}


void FMessageTracer::ProcessSentMessage( IMessageContextRef Context, double TimeSeconds )
{
	FMessageTracerEndpointInfoPtr EndpointInfo = AddressesToEndpointInfos.FindRef(Context->GetSender());

	if (!EndpointInfo.IsValid())
	{
		return;
	}

	// create message info
	FMessageTracerMessageInfoRef MessageInfo = MakeShareable(new FMessageTracerMessageInfo());
	
	MessageInfo->Context = Context;
	MessageInfo->SenderInfo = EndpointInfo;
	MessageInfo->TimeRouted = 0.0;
	MessageInfo->TimeSent = TimeSeconds;
	MessageInfos.Add(Context, MessageInfo);

	// add message type
	FMessageTracerTypeInfoPtr& TypeInfo = MessageTypes.FindOrAdd(Context->GetMessageType());

	if (!TypeInfo.IsValid())
	{
		TypeInfo = MakeShareable(new FMessageTracerTypeInfo());
		TypeInfo->TypeName = Context->GetMessageType();

		TypeAddedDelegate.Broadcast(TypeInfo.ToSharedRef());
	}

	TypeInfo->Messages.Add(MessageInfo);

	// update database
	EndpointInfo->SentMessages.Add(MessageInfo);
	MessageInfo->TypeInfo = TypeInfo;

	MessagesAddedDelegate.Broadcast(MessageInfo);
}


void FMessageTracer::ResetMessages()
{
	MessageInfos.Reset();
	MessageTypes.Reset();

	for (TMap<FMessageAddress, FMessageTracerEndpointInfoPtr>::TIterator It(AddressesToEndpointInfos); It; ++It)
	{
		FMessageTracerEndpointInfoPtr& EndpointInfo = It.Value();
		EndpointInfo->ReceivedMessages.Reset();
		EndpointInfo->SentMessages.Reset();
	}

	MessagesResetDelegate.Broadcast();
}


bool FMessageTracer::ShouldBreak( const IMessageContextRef& Context ) const
{
	if (Breaking)
	{
		return true;
	}

	for (int32 BreakpointIndex = Breakpoints.Num() - 1; BreakpointIndex >= 0; --BreakpointIndex)
	{
		const IMessageTracerBreakpointPtr& Breakpoint = Breakpoints[BreakpointIndex];

		if (Breakpoint->IsEnabled() && Breakpoint->ShouldBreak(Context))
		{
			return true;
		}
	}

	return false;
}
