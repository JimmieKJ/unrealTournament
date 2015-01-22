// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UdpMessagingPrivatePCH.h"
#include "UdpSerializeMessageTask.h"


/* FUdpMessageTransport structors
 *****************************************************************************/

FUdpMessageTransport::FUdpMessageTransport( const FIPv4Endpoint& InLocalEndpoint, const FIPv4Endpoint& InMulticastEndpoint, uint8 InMulticastTtl )
	: LocalEndpoint(InLocalEndpoint)
	, MessageProcessor(nullptr)
	, MessageProcessorThread(nullptr)
	, MulticastEndpoint(InMulticastEndpoint)
	, MulticastReceiver(nullptr)
	, MulticastSocket(nullptr)
	, MulticastTtl(InMulticastTtl)
	, SocketSubsystem(ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	, UnicastReceiver(nullptr)
	, UnicastSocket(nullptr)
{ }


FUdpMessageTransport::~FUdpMessageTransport()
{
	StopTransport();
}


/* IMessageTransport interface
 *****************************************************************************/

bool FUdpMessageTransport::StartTransport()
{
	// create & initialize unicast socket
	UnicastSocket = FUdpSocketBuilder(TEXT("UdpMessageUnicastSocket"))
		.AsNonBlocking()
		.WithMulticastLoopback()
		.BoundToEndpoint(LocalEndpoint)
		.WithReceiveBufferSize(UDP_MESSAGING_RECEIVE_BUFFER_SIZE);

	if (UnicastSocket == nullptr)
	{
		GLog->Logf(TEXT("UdpMessageTransport.StartTransport: Failed to create unicast socket on %s"), *LocalEndpoint.ToText().ToString());

		return false;
	}

	// create & initialize multicast socket (optional)
	MulticastSocket = FUdpSocketBuilder(TEXT("UdpMessageMulticastSocket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToAddress(LocalEndpoint.GetAddress())
		.BoundToPort(MulticastEndpoint.GetPort())
		.JoinedToGroup(MulticastEndpoint.GetAddress())
		.WithMulticastLoopback()
		.WithMulticastTtl(MulticastTtl)
		.WithReceiveBufferSize(UDP_MESSAGING_RECEIVE_BUFFER_SIZE);

	if (MulticastSocket == nullptr)
	{
		GLog->Logf(TEXT("UdpMessageTransport.StartTransport: Failed to create multicast socket on %s, joined to %s with TTL %i"), *LocalEndpoint.ToText().ToString(), *MulticastEndpoint.ToText().ToString(), MulticastTtl);
	}

	// initialize threads
	FTimespan ThreadWaitTime = FTimespan::FromMilliseconds(100);

	MessageProcessor = new FUdpMessageProcessor(UnicastSocket, FGuid::NewGuid(), MulticastEndpoint);
	MessageProcessor->OnMessageReassembled().BindRaw(this, &FUdpMessageTransport::HandleProcessorMessageReassembled);
	MessageProcessor->OnNodeDiscovered().BindRaw(this, &FUdpMessageTransport::HandleProcessorNodeDiscovered);
	MessageProcessor->OnNodeLost().BindRaw(this, &FUdpMessageTransport::HandleProcessorNodeLost);

	if (MulticastSocket != nullptr)
	{
		MulticastReceiver = new FUdpSocketReceiver(MulticastSocket, ThreadWaitTime, TEXT("UdpMessageMulticastReceiver"));
		MulticastReceiver->OnDataReceived().BindRaw(this, &FUdpMessageTransport::HandleSocketDataReceived);
	}

	UnicastReceiver = new FUdpSocketReceiver(UnicastSocket, ThreadWaitTime, TEXT("UdpMessageUnicastReceiver"));
	UnicastReceiver->OnDataReceived().BindRaw(this, &FUdpMessageTransport::HandleSocketDataReceived);

	return true;
}


void FUdpMessageTransport::StopTransport()
{
	// shut down threads
	delete MulticastReceiver;
	MulticastReceiver = nullptr;

	delete UnicastReceiver;
	UnicastReceiver = nullptr;

	delete MessageProcessor;
	MessageProcessor = nullptr;

	// destroy sockets
	if (MulticastSocket != nullptr)
	{
		SocketSubsystem->DestroySocket(MulticastSocket);
		MulticastSocket = nullptr;
	}
	
	if (UnicastSocket != nullptr)
	{
		SocketSubsystem->DestroySocket(UnicastSocket);
		UnicastSocket = nullptr;
	}
}


bool FUdpMessageTransport::TransportMessage( const IMessageContextRef& Context, const TArray<FGuid>& Recipients )
{
	if (MessageProcessor == nullptr)
	{
		return false;
	}

	if (Context->GetRecipients().Num() > UDP_MESSAGING_MAX_RECIPIENTS)
	{
		return false;
	}

	FUdpSerializedMessageRef SerializedMessage = MakeShareable(new FUdpSerializedMessage());
	TGraphTask<FUdpSerializeMessageTask>::CreateTask().ConstructAndDispatchWhenReady(Context, SerializedMessage);

	// publish the message
	if (Recipients.Num() == 0)
	{
		return MessageProcessor->EnqueueOutboundMessage(SerializedMessage, FGuid());
	}

	// send the message
	for (int32 Index = 0; Index < Recipients.Num(); ++Index)
	{
		if (!MessageProcessor->EnqueueOutboundMessage(SerializedMessage, Recipients[Index]))
		{
			return false;
		}
	}

	return true;
}


/* FUdpMessageTransport event handlers
 *****************************************************************************/

void FUdpMessageTransport::HandleProcessorMessageReassembled( const FUdpReassembledMessageRef& ReassembledMessage, const IMessageAttachmentPtr& Attachment, const FGuid& NodeId )
{
	// @todo gmp: move message deserialization into an async task
	FUdpDeserializedMessageRef DeserializedMessage = MakeShareable(new FUdpDeserializedMessage(Attachment));

	if (DeserializedMessage->Deserialize(ReassembledMessage))
	{
		MessageReceivedDelegate.ExecuteIfBound(DeserializedMessage, NodeId);
	}
}


void FUdpMessageTransport::HandleSocketDataReceived( const FArrayReaderPtr& Data, const FIPv4Endpoint& Sender )
{
	if (MessageProcessor != nullptr)
	{
		MessageProcessor->EnqueueInboundSegment(Data, Sender);
	}
}
