// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LwsWebSocket.h"

#if WITH_WEBSOCKETS

#include "Ssl.h"

namespace {
	static const struct lws_extension LwsExtensions[] = {
		{
			"permessage-deflate",
			lws_extension_callback_pm_deflate,
			"permessage-deflate; client_max_window_bits"
		},
		{
			"deflate-frame",
			lws_extension_callback_pm_deflate,
			"deflate_frame"
		},
		// zero terminated:
		{ nullptr, nullptr, nullptr }
	};
}

FLwsWebSocket::FLwsWebSocket(const FString& InUrl, const TArray<FString>& InProtocols)
	: LwsContext(nullptr)
	, LwsConnection(nullptr)
	, Url(InUrl)
	, Protocols(InProtocols)
	, ReceiveBuffer()
	, CloseCode(0)
	, CloseReason()
	, bIsConnecting(false)
{
	struct lws_context_creation_info ContextInfo = {};

	InitLwsProtocols();
	ContextInfo.port = CONTEXT_PORT_NO_LISTEN;
	ContextInfo.protocols = LwsProtocols.GetData();
	ContextInfo.uid = -1;
	ContextInfo.gid = -1;
	ContextInfo.options |= LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED | LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS;
	ContextInfo.ssl_cipher_list = nullptr;
	LwsContext = lws_create_context(&ContextInfo);
}

FLwsWebSocket::~FLwsWebSocket()
{
	Close(LWS_CLOSE_STATUS_GOINGAWAY, TEXT("Bye"));

	if(LwsContext != nullptr)
	{
		lws_context_destroy(LwsContext);
	}
	LwsContext = nullptr;
	DeleteLwsProtocols();
}

void FLwsWebSocket::InitLwsProtocols()
{
	LwsProtocols.Empty(Protocols.Num()+1);
	for(FString Protocol : Protocols)
	{
		FTCHARToUTF8 ConvertName(*Protocol);

		// We need to hold on to the converted strings
		ANSICHAR *Converted = static_cast<ANSICHAR*>(FMemory::Malloc(ConvertName.Length()+1));
		FCStringAnsi::Strcpy(Converted, ConvertName.Length(), ConvertName.Get());
		LwsProtocols.Add({Converted, &FLwsWebSocket::CallbackWrapper, 0, 65536});
	}
	// Add a zero terminator at the end as we don't pass the length to LWS
	LwsProtocols.Add({nullptr, nullptr, 0, 0});
}


void FLwsWebSocket::DeleteLwsProtocols()
{
	for (auto Element : LwsProtocols)
	{
		if (Element.name != nullptr)
		{
			FMemory::Free((void*)(Element.name));
			Element = {};
		}
	}
}

bool FLwsWebSocket::Tick(float DeltaTime)
{
	if (LwsContext != nullptr)
	{
		// Hold on to a reference to this object, so we will not get destroyed while executing lws_service
		TSharedPtr<FLwsWebSocket> Keep = SharedThis(this);
		lws_service(LwsContext, 0);
	}
	return true;
}

void FLwsWebSocket::DelayConnectionError(const FString& Error)
{
	// report connection error on the next tick, so we don't invoke event handlers on the current stack frame
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([=](float) -> bool
	{
		bIsConnecting = false;
		OnConnectionError().Broadcast(Error);
		return false;
	}));
}

void FLwsWebSocket::Connect()
{
	bIsConnecting = true;
	if (LwsContext == nullptr)
	{
		DelayConnectionError(TEXT("Invalid context"));
		return;
	}
	if (LwsConnection != nullptr)
	{
		DelayConnectionError(TEXT("Already connected"));
		return;
	}

	struct lws_client_connect_info ConnectInfo = {};
	ConnectInfo.context = LwsContext;
	FTCHARToUTF8 UrlUtf8(*Url);
	const char *UrlProtocol, *TmpUrlPath;
	char UrlPath[300];

	if (lws_parse_uri((char*) UrlUtf8.Get(), &UrlProtocol, &ConnectInfo.address, &ConnectInfo.port, &TmpUrlPath))
	{
		DelayConnectionError(TEXT("Bad URL"));
		return;
	}
	UrlPath[0] = '/';
	FCStringAnsi::Strncpy(UrlPath+1, TmpUrlPath, sizeof(UrlPath)-2);
	UrlPath[sizeof(UrlPath)-1]='\0';
	ConnectInfo.path = UrlPath;
	ConnectInfo.host = ConnectInfo.address;
	ConnectInfo.origin = ConnectInfo.address;
	ConnectInfo.ietf_version_or_minus_one = -1;
	ConnectInfo.client_exts = LwsExtensions;

	// Use SSL and require a valid cerver cert
	if (FCStringAnsi::Stricmp(UrlProtocol, "wss") == 0 )
	{
		ConnectInfo.ssl_connection = 1;
	}
	// Use SSL, and allow self-signed certs
	else if (FCStringAnsi::Stricmp(UrlProtocol, "wss+insecure") == 0 )
	{
		ConnectInfo.ssl_connection = 2;
	}
	// No encryption
	else if (FCStringAnsi::Stricmp(UrlProtocol, "ws") == 0 )
	{
		ConnectInfo.ssl_connection = 0;
	}
	// Else return an error
	else
	{
		DelayConnectionError(FString::Printf(TEXT("Bad protocol '%s'. Use either 'ws', 'wss', or 'wss+insecure'"), UTF8_TO_TCHAR(UrlProtocol)));
		return;
	}

	FString Combined = FString::Join(Protocols, TEXT(","));
	FTCHARToUTF8 CombinedUTF8(*Combined);
	ConnectInfo.protocol = CombinedUTF8.Get();
	ConnectInfo.userdata = this;

	if (lws_client_connect_via_info(&ConnectInfo) == nullptr)
	{
		DelayConnectionError(TEXT("Could not initialize connection"));
	}

}

void FLwsWebSocket::Close(int32 Code, const FString& Reason)
{
	CloseCode = Code;
	CloseReason = Reason;
	if (LwsConnection != nullptr && SendQueue.IsEmpty())
	{
		lws_callback_on_writable(LwsConnection);
	}
}

void FLwsWebSocket::Send(const void* Data, SIZE_T Size, bool bIsBinary)
{
	bool QueueWasEmpty = SendQueue.IsEmpty();
	SendQueue.Enqueue(MakeShareable(new FLwsSendBuffer((const uint8*) Data, Size, bIsBinary)));
	if (LwsConnection != nullptr && QueueWasEmpty)
	{
		lws_callback_on_writable(LwsConnection);
	}
}

void FLwsWebSocket::Send(const FString& Data)
{
	FTCHARToUTF8 Converted(*Data);
	Send((uint8*)Converted.Get(), Converted.Length(), false);
}

void FLwsWebSocket::FlushQueues()
{
	TSharedPtr<FLwsSendBuffer> Buffer;
	while (SendQueue.Dequeue(Buffer)); // Dequeue all elements until empty
	ReceiveBuffer.Empty(0); // Also clear temporary receive buffer
}

void FLwsWebSocket::SendFromQueue()
{
	if (LwsConnection == nullptr)
	{
		return;
	}

	TSharedPtr<FLwsSendBuffer> CurrentBuffer;
	if (SendQueue.Peek(CurrentBuffer))
	{
		bool FinishedSending = CurrentBuffer->Write(LwsConnection);
		if (FinishedSending)
		{
			SendQueue.Dequeue(CurrentBuffer);
		}
	}

	// If we still have data to send, ask for a notification when ready to send more
	if (!SendQueue.IsEmpty())
	{
		lws_callback_on_writable(LwsConnection);
	}
}

bool FLwsSendBuffer::Write(struct lws* LwsConnection)
{
	enum lws_write_protocol WriteProtocol;
	if (BytesWritten > 0)
	{
		WriteProtocol = LWS_WRITE_CONTINUATION;
	}
	else
	{
		WriteProtocol = bIsBinary?LWS_WRITE_BINARY:LWS_WRITE_TEXT;
	}

	int32 Offset = LWS_PRE + BytesWritten;
	int32 CurrentBytesWritten = lws_write(LwsConnection, Payload.GetData() + Offset, Payload.Num() - Offset, WriteProtocol);

	if (CurrentBytesWritten > 0)
	{
		BytesWritten += CurrentBytesWritten;
	}
	else
	{
		// @TODO: indicate error
		return true;
	}

	return BytesWritten + (int32)(LWS_PRE) >= Payload.Num();
}

int FLwsWebSocket::CallbackWrapper(struct lws *Instance, enum lws_callback_reasons Reason, void *UserData, void *Data, size_t Length)
{
	FLwsWebSocket* Self = reinterpret_cast<FLwsWebSocket*>(UserData);
	switch (Reason)
	{
	case LWS_CALLBACK_PROTOCOL_INIT:
	case LWS_CALLBACK_PROTOCOL_DESTROY:
	case LWS_CALLBACK_GET_THREAD_ID:
	case LWS_CALLBACK_LOCK_POLL:
	case LWS_CALLBACK_UNLOCK_POLL:
	case LWS_CALLBACK_ADD_POLL_FD:
	case LWS_CALLBACK_DEL_POLL_FD:
	case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
	case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
	
	break;
	case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
	case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
	{
		SSL_CTX* SslContext = reinterpret_cast<SSL_CTX*>(UserData);
		FSslModule::Get().GetCertificateManager().AddCertificatesToSslContext(SslContext);
		break;
	}
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		Self->bIsConnecting = false;
		Self->LwsConnection = Instance;
		if (!Self->SendQueue.IsEmpty())
		{
			lws_callback_on_writable(Self->LwsConnection);
		}
		Self->OnConnected().Broadcast();
		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:
	{
		SIZE_T BytesLeft = lws_remaining_packet_payload(Instance);
		if(Self->OnMessage().IsBound())
		{
			FUTF8ToTCHAR Convert((const ANSICHAR*)Data, Length);
			Self->ReceiveBuffer.Append(Convert.Get(), Convert.Length());
			if (BytesLeft == 0)
			{
				Self->OnMessage().Broadcast(Self->ReceiveBuffer);
				Self->ReceiveBuffer.Empty();
			}
		}
		Self->OnRawMessage().Broadcast(Data, Length, BytesLeft);
		break;
	}
	case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
	{
		Self->LwsConnection = nullptr;
		Self->FlushQueues();
		if (Self->OnClosed().IsBound())
		{
			uint16 CloseStatus = *((uint16*)Data);
#if PLATFORM_LITTLE_ENDIAN
			// The status is the first two bytes of the message in network byte order
			CloseStatus = BYTESWAP_ORDER16(CloseStatus);
#endif
			FUTF8ToTCHAR Convert((const ANSICHAR*)Data + sizeof(uint16), Length - sizeof(uint16));
			Self->OnClosed().Broadcast(CloseStatus, Convert.Get(), true);
			return 1; // Close the connection without logging if the user handles Close events
		}
		break;
	}
	case LWS_CALLBACK_WSI_DESTROY:
		// Getting a WSI_DESTROY before a connection has been established and no errors reported usually means there was a timeout establishing a connection
		if (Self->bIsConnecting)
		{
			//Self->OnConnectionError().Broadcast(TEXT("Connection timed out"));
			//Self->bIsConnecting = false;
		}
		if (Self->LwsConnection)
		{
			Self->LwsConnection = nullptr;
		}
		break;
	case LWS_CALLBACK_CLOSED:
	{
		bool ClientInitiated = Self->LwsConnection == nullptr;
		Self->LwsConnection = nullptr;
		Self->OnClosed().Broadcast(LWS_CLOSE_STATUS_NORMAL, ClientInitiated?TEXT("Successfully closed connection to server"):TEXT("Connection closed by server"), ClientInitiated);
		Self->FlushQueues();
		break;
	}
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
	{
		Self->LwsConnection = nullptr;
		FUTF8ToTCHAR Convert((const ANSICHAR*)Data, Length);
		Self->OnConnectionError().Broadcast(Convert.Get());
		Self->FlushQueues();
		return -1;
		break;
	}
	case LWS_CALLBACK_RECEIVE_PONG:
		break;
	case LWS_CALLBACK_CLIENT_WRITEABLE:
	case LWS_CALLBACK_SERVER_WRITEABLE:
	{
		if (Self->CloseCode != 0)
		{
			Self->LwsConnection = nullptr;
			FTCHARToUTF8 Convert(*Self->CloseReason);
			// This only sets the reason for closing the connection:
			lws_close_reason(Instance, (enum lws_close_status)Self->CloseCode, (unsigned char *)Convert.Get(), (size_t)Convert.Length());
			Self->CloseCode = 0;
			Self->CloseReason = FString();
			Self->FlushQueues();
			return -1; // Returning non-zero will close the current connection
		}
		else
		{
			Self->SendFromQueue();
		}
		break;
	}
	default:
		break;
	}
	return 0;
}

#endif // #if WITH_WEBSOCKETS