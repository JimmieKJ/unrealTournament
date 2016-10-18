// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TcpMessagingPrivatePCH.h"

/** Header sent over the connection as soon as it's opened */
struct FTcpMessageHeader
{
	uint32 MagicNumber;
	uint32 Version;
	FGuid NodeId;

	FTcpMessageHeader()
	:	MagicNumber(0)
	,	Version(0)
	{}

	FTcpMessageHeader(const FGuid& InNodeId)
	:	MagicNumber(TCP_MESSAGING_TRANSPORT_PROTOCOL_MAGIC)
	,	Version(TCP_MESSAGING_TRANSPORT_PROTOCOL_VERSION)
	,	NodeId(InNodeId)
	{}

	bool IsValid()
	{
		return
			MagicNumber == TCP_MESSAGING_TRANSPORT_PROTOCOL_MAGIC &&
			Version == TCP_MESSAGING_TRANSPORT_PROTOCOL_VERSION &&
			NodeId.IsValid();
	}

	FGuid GetNodeId()
	{
		return NodeId;
	}

	// Serializer
	friend FArchive& operator<<(FArchive& Ar, FTcpMessageHeader& H)
	{
		return Ar << H.MagicNumber << H.Version << H.NodeId;
	}
};


/* FTcpMessageTransportConnection structors
 *****************************************************************************/

FTcpMessageTransportConnection::FTcpMessageTransportConnection(FSocket* InSocket, const FIPv4Endpoint& InRemoteEndpoint, int32 InConnectionRetryDelay)
	: ConnectionState(STATE_Connecting)
	, OpenedTime(FDateTime::UtcNow())
	, RemoteEndpoint(InRemoteEndpoint)
	, LocalNodeId(FGuid::NewGuid())
	, bSentHeader(false)
	, bReceivedHeader(false)
	, Socket(InSocket)
	, Thread(nullptr)
	, TotalBytesReceived(0)
	, TotalBytesSent(0)
	, bRun(false)
	, ConnectionRetryDelay(InConnectionRetryDelay)
{
	int32 NewSize = 0;
	Socket->SetReceiveBufferSize(2*1024*1024, NewSize);
	Socket->SetSendBufferSize(2*1024*1024, NewSize);
}

FTcpMessageTransportConnection::~FTcpMessageTransportConnection()
{
	if (Thread != nullptr)
	{
		bRun = false;
		Thread->WaitForCompletion();
		delete Thread;
	}

	if (Socket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	}
}

/* FTcpMessageTransportConnection interface
 *****************************************************************************/

void FTcpMessageTransportConnection::Start()
{
	check(Thread == nullptr);
	bRun = true;
	Thread = FRunnableThread::Create(this, TEXT("FTcpMessageTransportConnection"), 128 * 1024, TPri_Normal);
}

bool FTcpMessageTransportConnection::Receive(TSharedPtr<FTcpDeserializedMessage, ESPMode::ThreadSafe>& OutMessage, FGuid& OutSenderNodeId)
{
	if(Inbox.Dequeue(OutMessage))
	{
		OutSenderNodeId = RemoteNodeId;
		return true;
	}
	return false;
}


bool FTcpMessageTransportConnection::Send(FTcpSerializedMessagePtr Message)
{
	if (GetConnectionState() == STATE_Connected)
	{
		return Outbox.Enqueue(Message);
	}

	return false;
}


/* FRunnable interface
 *****************************************************************************/

bool FTcpMessageTransportConnection::Init()
{
	return (Socket != nullptr);
}

uint32 FTcpMessageTransportConnection::Run()
{
	while (bRun)
	{
		// Check if we can are ready to send or receive, and if so try sending and receiving messages.
		// Otherwise, check for a connection error.
		if (((Socket->Wait(ESocketWaitConditions::WaitForReadOrWrite, FTimespan::Zero()) && (!ReceiveMessages() || !SendMessages())) ||
			(Socket->GetConnectionState() == SCS_ConnectionError)) && bRun)
		{
			// Disconnected. Reconnect if requested.
			if (ConnectionRetryDelay > 0)
			{
				UE_LOG(LogTcpMessaging, Verbose, TEXT("Connection to '%s' failed, retrying..."), *RemoteEndpoint.ToString());
				FPlatformProcess::Sleep(ConnectionRetryDelay);

				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
				Socket = FTcpSocketBuilder(TEXT("FTcpMessageTransport.RemoteConnection"));
				if (Socket && Socket->Connect(RemoteEndpoint.ToInternetAddr().Get()))
				{
					bSentHeader = false;
					bReceivedHeader = false;
					UpdateConnectionState(STATE_DisconnectReconnectPending);
					RemoteNodeId.Invalidate();
				}
				else
				{
					if (Socket)
					{
						ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
						Socket = nullptr;
					}
					bRun = false;
				}
			}
			else
			{
				bRun = false;
			}
		}

		FPlatformProcess::Sleep(0.0f);
	}

	UpdateConnectionState(STATE_Disconnected);
	RemoteNodeId.Invalidate();
	ClosedTime = FDateTime::UtcNow();

	// Clear the delegate to remove a reference to this connection
	ConnectionStateChangedDelegate.Unbind();
	return 0;
}

void FTcpMessageTransportConnection::Stop()
{
	if (Socket)
	{
		Socket->Close();
	}
}

void FTcpMessageTransportConnection::Exit()
{
	// do nothing
}


/* FTcpMessageTransportConnection implementation
 *****************************************************************************/

void FTcpMessageTransportConnection::Close()
{
	// We let the orderly shutdown proceed, so we get disconnect notifications
	if (Socket)
	{
		Socket->Close();
	}

	if (Thread != nullptr)
	{
		bRun = false;
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

uint64 FTcpMessageTransportConnection::GetTotalBytesReceived() const
{
	return TotalBytesReceived;
}

uint64 FTcpMessageTransportConnection::GetTotalBytesSent() const
{
	return TotalBytesSent;
}

FText FTcpMessageTransportConnection::GetName() const
{
	return RemoteEndpoint.ToText();
}

FTimespan FTcpMessageTransportConnection::GetUptime() const
{
	if (ConnectionState == STATE_Connected)
	{
		return (FDateTime::UtcNow() - OpenedTime);
	}
		
	return (ClosedTime - OpenedTime);
}

FTcpMessageTransportConnection::EConnectionState FTcpMessageTransportConnection::GetConnectionState() const
{
	return ConnectionState;
}

FGuid FTcpMessageTransportConnection::GetRemoteNodeId() const
{
	return RemoteNodeId;
}

void FTcpMessageTransportConnection::UpdateConnectionState(EConnectionState NewConnectionState)
{
	ConnectionState = NewConnectionState;
	ConnectionStateChangedDelegate.ExecuteIfBound();
}

bool FTcpMessageTransportConnection::ReceiveMessages()
{
	uint32 PendingDataSize = 0;

	// check if the socket has closed
	{
		int32 BytesRead;
		uint8 Dummy;
		if (!Socket->Recv(&Dummy, 1, BytesRead, ESocketReceiveFlags::Peek))
		{
			return false;
		}
	}
	
	if (!bReceivedHeader)
	{
		if (Socket->HasPendingData(PendingDataSize) && PendingDataSize >= sizeof(FTcpMessageHeader))
		{
			FArrayReader HeaderData = FArrayReader(true);
			HeaderData.SetNumUninitialized(sizeof(FTcpMessageHeader));
			int32 BytesRead = 0;
			if (!Socket->Recv(HeaderData.GetData(), sizeof(FTcpMessageHeader), BytesRead))
			{
				return false;
			}

			check(BytesRead == sizeof(FTcpMessageHeader));
			TotalBytesReceived += BytesRead;

			FTcpMessageHeader MessageHeader;
			HeaderData << MessageHeader;

			if (!MessageHeader.IsValid())
			{
				return false;
			}
			else
			{
				RemoteNodeId = MessageHeader.GetNodeId();
				bReceivedHeader = true;
				OpenedTime = FDateTime::UtcNow();
				UpdateConnectionState(STATE_Connected);
			}
		}
		else
		{
			// no header yet
			return true;
		}
	}

	// if we received a payload size...
	while (Socket->HasPendingData(PendingDataSize) && (PendingDataSize >= sizeof(uint16)))
	{
		int32 BytesRead = 0;

		FArrayReader MessagesizeData = FArrayReader(true);
		MessagesizeData.SetNumUninitialized(sizeof(uint16));

		// ... read it from the stream without removing it...
		if (!Socket->Recv(MessagesizeData.GetData(), sizeof(uint16), BytesRead, ESocketReceiveFlags::Peek))
		{
			return false;
		}

		check(BytesRead == sizeof(uint16));

		uint16 Messagesize;
		MessagesizeData << Messagesize;

		// ... and if we received the complete payload...
		if (Socket->HasPendingData(PendingDataSize) && (PendingDataSize >= sizeof(uint16) + Messagesize))
		{
			// ... then remove the payload size from the stream...
			if (!Socket->Recv((uint8*)&Messagesize, sizeof(uint16), BytesRead))
			{
				return false;
			}

			check(BytesRead == sizeof(uint16));
			TotalBytesReceived += BytesRead;

			// ... receive the payload
			FArrayReaderPtr Payload = MakeShareable(new FArrayReader(true));
			Payload->SetNumUninitialized(Messagesize);

			if (!Socket->Recv(Payload->GetData(), Payload->Num(), BytesRead))
			{
				return false;
			}

			check(BytesRead == Messagesize);
			TotalBytesReceived += BytesRead;

			// @todo gmp: move message deserialization into an async task
			FTcpDeserializedMessage* DeserializedMessage = new FTcpDeserializedMessage(nullptr);
			if (DeserializedMessage->Deserialize(Payload))
			{
				Inbox.Enqueue(MakeShareable(DeserializedMessage));
			}
		}
		else
		{
			break;
		}
	}

	return true;
}

bool FTcpMessageTransportConnection::SendMessages()
{
	if (Outbox.IsEmpty() && bSentHeader)
	{
		return true;
	}

	if (!Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::Zero()))
	{
		return true;
	}

	if (!bSentHeader)
	{
		FArrayWriter HeaderData;
		FTcpMessageHeader MessageHeader(LocalNodeId);
		HeaderData << MessageHeader;

		int32 BytesSent = 0;
		if (!Socket->Send(HeaderData.GetData(), sizeof(FTcpMessageHeader), BytesSent))
		{
			return false;
		}

		bSentHeader = true;
		TotalBytesSent += BytesSent;
	}
	else
	{
		FTcpSerializedMessagePtr Message;
		while (Outbox.Dequeue(Message))
		{
			int32 BytesSent = 0;
			const TArray<uint8>& Payload = Message->GetDataArray();

			// send the payload size
			FArrayWriter MessagesizeData = FArrayWriter(true);
			uint16 Messagesize = Payload.Num();
			MessagesizeData << Messagesize;

			if (!Socket->Send(MessagesizeData.GetData(), sizeof(uint16), BytesSent))
			{
				return false;
			}

			TotalBytesSent += BytesSent;

			// send the payload
			if (!Socket->Send(Payload.GetData(), Payload.Num(), BytesSent))
			{
				return false;
			}

			TotalBytesSent += BytesSent;

			// return if the socket is no longer writable, or that was the last message
			if (Outbox.IsEmpty() || !Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::Zero()))
			{
				return true;
			}
		}
	}
	return true;
}

