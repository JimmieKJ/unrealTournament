/// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Ticker.h"
#include "Containers/Queue.h"

#if WITH_WEBSOCKETS

#include "IWebSocket.h"

#if PLATFORM_WINDOWS
#   include "WindowsHWrapper.h"
#	include "AllowWindowsPlatformTypes.h"
#endif

#include "libwebsockets.h"

#if PLATFORM_WINDOWS
#	include "HideWindowsPlatformTypes.h"
#endif

struct FLwsSendBuffer
{
	bool bIsBinary;
	int32 BytesWritten;
	TArray<uint8> Payload;

	FLwsSendBuffer(const uint8* Data, SIZE_T Size, bool InIsBinary)
		: bIsBinary(InIsBinary)
		, BytesWritten(0)
	{
		Payload.AddDefaulted(LWS_PRE); // Reserve space for WS header data
		Payload.Append(Data, Size);
	}

	bool Write(struct lws* LwsConnection);

};


class FLwsWebSocket
	: public IWebSocket
	, public FTickerObjectBase
	, public TSharedFromThis<FLwsWebSocket>
{

public:

	virtual ~FLwsWebSocket();

	// FTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

	// IWebSocket
	virtual void Connect() override;
	virtual void Close(int32 Code = 1000, const FString& Reason = FString()) override;

	virtual bool IsConnected() override
	{
		return LwsConnection != nullptr;
	}

	virtual void Send(const FString& Data);
	virtual void Send(const void* Data, SIZE_T Size, bool bIsBinary) override;

	/**
	 * Delegate called when a web socket connection has been established
	 *
	 */
	DECLARE_DERIVED_EVENT(FLwsWebSocket, IWebSocket::FWebSocketConnectedEvent, FWebSocketConnectedEvent);
	virtual FWebSocketConnectedEvent& OnConnected() override
	{
		return ConnectedEvent;
	}

	/**
	 * Delegate called when a web socket connection has been established
	 *
	 */
	DECLARE_DERIVED_EVENT(FLwsWebSocket, IWebSocket::FWebSocketConnectionErrorEvent, FWebSocketConnectionErrorEvent);
	virtual FWebSocketConnectionErrorEvent& OnConnectionError() override
	{
		return ConnectionErrorEvent;
	}

	/**
	 * Delegate called when a web socket connection has been closed
	 *
	 */
	DECLARE_DERIVED_EVENT(FLwsWebSocket, IWebSocket::FWebSocketClosedEvent, FWebSocketClosedEvent);
	virtual FWebSocketClosedEvent& OnClosed() override
	{
		return ClosedEvent;
	}

	/**
	 * Delegate called when a web socket message has been received
	 *
	 */
	DECLARE_DERIVED_EVENT(FLwsWebSocket, IWebSocket::FWebSocketMessageEvent, FWebSocketMessageEvent);
	virtual FWebSocketMessageEvent& OnMessage() override
	{
		return MessageEvent;
	}

	/**
	 * Delegate called when a raw web socket message has been received
	 *
	 */
	DECLARE_DERIVED_EVENT(FLwsWebSocket, IWebSocket::FWebSocketRawMessageEvent, FWebSocketRawMessageEvent);
	virtual FWebSocketRawMessageEvent& OnRawMessage() override
	{
		return RawMessageEvent;
	}

private:

	FLwsWebSocket(const FString& Url, const TArray<FString>& Protocols);
	void InitLwsProtocols();
	void DeleteLwsProtocols();
	void SendFromQueue();
	void FlushQueues();
	void DelayConnectionError(const FString& Error);

	static int CallbackWrapper(struct lws *Connection, enum lws_callback_reasons Reason, void *UserData, void *Data, size_t Length);

	FWebSocketConnectedEvent ConnectedEvent;
	FWebSocketConnectionErrorEvent ConnectionErrorEvent;
	FWebSocketClosedEvent ClosedEvent;
	FWebSocketMessageEvent MessageEvent;
	FWebSocketRawMessageEvent RawMessageEvent;

	TArray<struct lws_protocols> LwsProtocols;
	struct lws_context *LwsContext;
	struct lws *LwsConnection;
	FString Url;
	TArray<FString> Protocols;

	FString ReceiveBuffer;
	TQueue<TSharedPtr<FLwsSendBuffer>, EQueueMode::Spsc> SendQueue;
	int32 CloseCode;
	FString CloseReason;
	bool bIsConnecting;

	friend class FWebSocketsModule;
};

#endif
