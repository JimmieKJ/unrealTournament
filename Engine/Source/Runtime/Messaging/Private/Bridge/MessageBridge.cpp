// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessagingPrivatePCH.h"


/* FMessageBridge structors
 *****************************************************************************/

FMessageBridge::FMessageBridge( const FMessageAddress InAddress, const IMessageBusRef& InBus, const IMessageTransportRef& InTransport )
	: Address(InAddress)
	, Bus(InBus)
	, Enabled(false)
	, Id(FGuid::NewGuid())
	, Transport(InTransport)
{
	Bus->OnShutdown().AddRaw(this, &FMessageBridge::HandleMessageBusShutdown);
	Transport->OnNodeLost().BindRaw(this, &FMessageBridge::HandleTransportNodeLost);
	Transport->OnMessageReceived().BindRaw(this, &FMessageBridge::HandleTransportMessageReceived);
}


FMessageBridge::~FMessageBridge()
{
	Shutdown();

	if (Bus.IsValid())
	{
		Bus->OnShutdown().RemoveAll(this);
		Bus->Unregister(Address);

		TArray<FMessageAddress> RemovedAddresses;

		for (int32 AddressIndex = 0; AddressIndex < RemovedAddresses.Num(); ++AddressIndex)
		{
			Bus->Unregister(RemovedAddresses[AddressIndex]);
		}
	}
}


/* IMessageBridge interface
 *****************************************************************************/

void FMessageBridge::Disable() 
{
	if (!Enabled)
	{
		return;
	}

	// disable subscription & transport
	if (MessageSubscription.IsValid())
	{
		MessageSubscription->Disable();
	}

	if (Transport.IsValid())
	{
		Transport->StopTransport();
	}

	Enabled = false;
}


void FMessageBridge::Enable()
{
	if (Enabled || !Bus.IsValid() || !Transport.IsValid())
	{
		return;
	}

	// enable subscription & transport
	if (!Transport->StartTransport())
	{
		return;
	}

	Bus->Register(Address, AsShared());

	if (MessageSubscription.IsValid())
	{
		MessageSubscription->Enable();
	}
	else
	{
		MessageSubscription = Bus->Subscribe(AsShared(), NAME_All, FMessageScopeRange::AtLeast(EMessageScope::Network));
	}

	Enabled = true;
}


/* IReceiveMessages interface
 *****************************************************************************/

void FMessageBridge::ReceiveMessage( const IMessageContextRef& Context )
{
	if (!Enabled)
	{
		return;
	}

	// get remote nodes
	TArray<FGuid> RemoteNodes;

	if (Context->GetRecipients().Num() > 0)
	{
		RemoteNodes = AddressBook.GetNodesFor(Context->GetRecipients());

		if (RemoteNodes.Num() == 0)
		{
			return;
		}
	}

	// forward message to remote nodes
	Transport->TransportMessage(Context, RemoteNodes);
}


/* ISendMessages interface
 *****************************************************************************/

void FMessageBridge::NotifyMessageError( const IMessageContextRef& Context, const FString& Error )
{
	// deprecated
}


/* FMessageBridge implementation
 *****************************************************************************/

void FMessageBridge::Shutdown()
{
	Disable();

	if (Transport.IsValid())
	{
		Transport->OnMessageReceived().Unbind();
		Transport->OnNodeLost().Unbind();
	}
}


/* FMessageBridge callbacks
 *****************************************************************************/

void FMessageBridge::HandleMessageBusShutdown()
{
	Shutdown();
	Bus.Reset();
}


void FMessageBridge::HandleTransportMessageReceived( const IMessageContextRef& Context, const FGuid& NodeId )
{
	if (!Enabled || !Bus.IsValid())
	{
		return;
	}

	// discard expired messages
	if (Context->GetExpiration() < FDateTime::UtcNow())
	{
		return;
	}

	// register newly discovered endpoints
	if (!AddressBook.Contains(Context->GetSender()))
	{
		AddressBook.Add(Context->GetSender(), NodeId);
		Bus->Register(Context->GetSender(), AsShared());
	}

	// forward message to local bus
	Bus->Forward(Context, Context->GetRecipients(), FTimespan::Zero(), AsShared());
}


void FMessageBridge::HandleTransportNodeLost( const FGuid& LostNodeId )
{
	TArray<FMessageAddress> RemovedAddresses;

	// update address book
	AddressBook.RemoveNode(LostNodeId, RemovedAddresses);

	// unregister endpoints
	if (Bus.IsValid())
	{
		for (int32 AddressIndex = 0; AddressIndex < RemovedAddresses.Num(); ++AddressIndex)
		{
			Bus->Unregister(RemovedAddresses[AddressIndex]);
		}
	}
}
