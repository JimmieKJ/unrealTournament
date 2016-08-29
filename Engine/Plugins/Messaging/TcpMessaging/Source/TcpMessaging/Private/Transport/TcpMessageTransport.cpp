// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TcpMessagingPrivatePCH.h"
#include "TcpSerializeMessageTask.h"


/* FTcpMessageTransport structors
 *****************************************************************************/

FTcpMessageTransport::FTcpMessageTransport(const FIPv4Endpoint& InListenEndpoint, const TArray<FIPv4Endpoint>& InConnectToEndpoints, int32 InConnectionRetryDelay)
	: ListenEndpoint(InListenEndpoint)
	, ConnectToEndpoints(InConnectToEndpoints)
	, ConnectionRetryDelay(InConnectionRetryDelay)
	, bStopping(false)
	, SocketSubsystem(ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	, Listener(nullptr)
{
	Thread = FRunnableThread::Create(this, TEXT("FTcpMessageTransport"), 128 * 1024, TPri_Normal);
}


FTcpMessageTransport::~FTcpMessageTransport()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
	}

	StopTransport();
}


/* IMessageTransport interface
 *****************************************************************************/

bool FTcpMessageTransport::StartTransport()
{
	if (ListenEndpoint != FIPv4Endpoint::Any)
	{
		Listener = new FTcpListener(ListenEndpoint);
		Listener->OnConnectionAccepted().BindRaw(this, &FTcpMessageTransport::HandleListenerConnectionAccepted);
	}

	// outgoing connections
	for (auto& ConnectToEndPoint : ConnectToEndpoints)
	{
		AddOutgoingConnection(ConnectToEndPoint);
	}

	return true;
}

void FTcpMessageTransport::AddOutgoingConnection(const FIPv4Endpoint& Endpoint)
{
	FSocket* Socket = FTcpSocketBuilder(TEXT("FTcpMessageTransport.RemoteConnection"));
	if (Socket)
	{
		if (!Socket->Connect(*Endpoint.ToInternetAddr()))
		{
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		}
		else
		{
			PendingConnections.Enqueue(MakeShareable(new FTcpMessageTransportConnection(Socket, Endpoint, ConnectionRetryDelay)));
		}
	}
}

void FTcpMessageTransport::RemoveOutgoingConnection(const FIPv4Endpoint& Endpoint)
{
	ConnectionEndpointsToRemove.Enqueue(Endpoint);
}

void FTcpMessageTransport::StopTransport()
{
	bStopping = true;

	if (Listener)
	{
		delete Listener;
		Listener = nullptr;
	}

	for (auto& Connection : Connections)
	{
		Connection->Close();
	}
	Connections.Empty();
	PendingConnections.Empty();
	ConnectionEndpointsToRemove.Empty();
}


bool FTcpMessageTransport::TransportMessage(const IMessageContextRef& Context, const TArray<FGuid>& Recipients)
{
	if (Context->GetRecipients().Num() > TCP_MESSAGING_MAX_RECIPIENTS)
	{
		return false;
	}

	// Handle any queued changes to the NodeConnectionMap
	FNodeConnectionMapUpdate UpdateInfo;
	while (NodeConnectionMapUpdates.Dequeue(UpdateInfo))
	{
		check(UpdateInfo.NodeId.IsValid());
		if (UpdateInfo.bNewNode)
		{
			TSharedPtr<FTcpMessageTransportConnection> ConnectionPtr = UpdateInfo.Connection.Pin();
			if (ConnectionPtr.IsValid())
			{
				NodeConnectionMap.Add(UpdateInfo.NodeId, ConnectionPtr);
			}
		}
		else
		{
			NodeConnectionMap.Remove(UpdateInfo.NodeId);
		}
	}

	// Work out which connections we need to send this message to.
	TArray<TSharedPtr<FTcpMessageTransportConnection>> RecipientConnections;

	if (Recipients.Num() == 0)
	{
		// broadcast the message to all connections
		RecipientConnections = Connections;
	}
	else
	{
		// Find connections for each recipient.  We do not transport unicast messages for unknown nodes.
		for (auto& Recipient : Recipients)
		{
			TSharedPtr<FTcpMessageTransportConnection>* RecipientConnection = NodeConnectionMap.Find(Recipient);
			if (RecipientConnection)
			{
				RecipientConnections.AddUnique(*RecipientConnection);
			}
		}
	}

	if(RecipientConnections.Num() == 0)
	{
		return false;
	}

	FTcpSerializedMessageRef SerializedMessage = MakeShareable(new FTcpSerializedMessage());

	TGraphTask<FTcpSerializeMessageTask>::CreateTask().ConstructAndDispatchWhenReady(Context, SerializedMessage, RecipientConnections);

	return true;
}


/* FRunnable interface
*****************************************************************************/

bool FTcpMessageTransport::Init()
{
	return true;
}

uint32 FTcpMessageTransport::Run()
{
	while (!bStopping)
	{
		// new connections
		{
			TSharedPtr<FTcpMessageTransportConnection> Connection;
			while (PendingConnections.Dequeue(Connection))
			{
				Connection->OnTcpMessageTransportConnectionStateChanged().BindRaw(this, &FTcpMessageTransport::HandleConnectionStateChanged, Connection);
				Connection->Start();
				Connections.Add(Connection);
			}
		}

		// connections to remove
		{
			FIPv4Endpoint Endpoint;
			while (ConnectionEndpointsToRemove.Dequeue(Endpoint))
			{
				for (int32 Index = 0; Index < Connections.Num(); Index++)
				{
					auto& Connection = Connections[Index];
					if (Connection->GetRemoteEndpoint() == Endpoint)
					{
						Connection->Close();
						break;
					}
				}	
			}
		}

		int32 ActiveConnections = 0;
		for (int32 Index = 0;Index < Connections.Num(); Index++)
		{
			auto& Connection = Connections[Index];

			// handle disconnected by remote
			switch (Connection->GetConnectionState())
			{
			case FTcpMessageTransportConnection::STATE_Connected:
				ActiveConnections++;
				break;
			case FTcpMessageTransportConnection::STATE_Disconnected:
				Connections.RemoveAtSwap(Index);
				Index--;
				break;
			default:
				break;
			}
		}

		// incoming messages
		{
			for (auto& Connection : Connections)
			{
				TSharedPtr<FTcpDeserializedMessage, ESPMode::ThreadSafe> Message;
				FGuid SenderNodeId;
				while (Connection->Receive(Message, SenderNodeId))
				{
					MessageReceivedDelegate.ExecuteIfBound(Message.ToSharedRef(), SenderNodeId);
				}
			}
		}
				
		FPlatformProcess::Sleep(ActiveConnections > 0 ? 0.01f : 1.f);
	}

	return 0;
}

void FTcpMessageTransport::Stop()
{
	bStopping = true;
}

void FTcpMessageTransport::Exit()
{
	// do nothing
}



/* FTcpMessageTransport event handlers
*****************************************************************************/

bool FTcpMessageTransport::HandleListenerConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	PendingConnections.Enqueue(MakeShareable(new FTcpMessageTransportConnection(ClientSocket, ClientEndpoint, 0)));
	return true;
}

void FTcpMessageTransport::HandleConnectionStateChanged(TSharedPtr<FTcpMessageTransportConnection> Connection)
{
	FIPv4Endpoint RemoteEndpoint = Connection->GetRemoteEndpoint();
	FGuid NodeId = Connection->GetRemoteNodeId();
	FTcpMessageTransportConnection::EConnectionState State = Connection->GetConnectionState();

	if (State == FTcpMessageTransportConnection::STATE_Connected)
	{
		NodeConnectionMapUpdates.Enqueue(FNodeConnectionMapUpdate(true, NodeId, TWeakPtr<FTcpMessageTransportConnection>(Connection)));
		NodeDiscoveredDelegate.ExecuteIfBound(NodeId);
		UE_LOG(LogTcpMessaging, Warning, TEXT("Discovered node '%s' on connection '%s'..."), *NodeId.ToString(), *RemoteEndpoint.ToString());
	}
	else
	{
		if (NodeId.IsValid())
		{
			UE_LOG(LogTcpMessaging, Warning, TEXT("Lost node '%s' on connection '%s'..."), *NodeId.ToString(), *RemoteEndpoint.ToString());
			NodeConnectionMapUpdates.Enqueue(FNodeConnectionMapUpdate(false, NodeId, TWeakPtr<FTcpMessageTransportConnection>(Connection)));
			NodeLostDelegate.ExecuteIfBound(NodeId);
		}
	}
}
