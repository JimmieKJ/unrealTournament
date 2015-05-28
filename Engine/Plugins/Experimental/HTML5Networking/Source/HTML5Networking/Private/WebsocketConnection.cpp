// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HTML5NetworkingPCH.h"

#include "IPAddress.h"
#include "Sockets.h"
#include "Net/NetworkProfiler.h"
#include "Net/DataChannel.h"

#include "WebSocketConnection.h"
#include "WebSocketNetDriver.h"
#include "WebSocket.h"
/*-----------------------------------------------------------------------------
Declarations.
-----------------------------------------------------------------------------*/

// Size of a UDP header.
#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)
#define SLIP_HEADER_SIZE   (UDP_HEADER_SIZE+4)
#define WINSOCK_MAX_PACKET (512)

UWebSocketConnection::UWebSocketConnection(const FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer),
WebSocket(NULL)
{
}

void UWebSocketConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// Pass the call up the chain
	Super::InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? WINSOCK_MAX_PACKET : InMaxPacket,
		InPacketOverhead == 0 ? SLIP_HEADER_SIZE : InPacketOverhead);

}

void UWebSocketConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? WINSOCK_MAX_PACKET : InMaxPacket,
		InPacketOverhead == 0 ? SLIP_HEADER_SIZE : InPacketOverhead);

	// Figure out IP address from the host URL

	// Initialize our send bunch
	InitSendBuffer();
}

void UWebSocketConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? WINSOCK_MAX_PACKET : InMaxPacket,
		InPacketOverhead == 0 ? SLIP_HEADER_SIZE : InPacketOverhead);

	// Initialize our send bunch
	InitSendBuffer();

	// This is for a client that needs to log in, setup ClientLoginState and ExpectedClientLoginMsgType to reflect that
	SetClientLoginState(EClientLoginState::LoggingIn);
	SetExpectedClientLoginMsgType(NMT_Hello);
}

void UWebSocketConnection::LowLevelSend(void* Data, int32 Count)
{
	int32 BytesSent = 0;
	WebSocket->Send((uint8*)Data, Count);
}

FString UWebSocketConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return WebSocket->RemoteEndPoint();
}

FString UWebSocketConnection::LowLevelDescribe()
{
	return FString::Printf
		(
		TEXT(" remote=%s local=%s state: %s"),
		*WebSocket->RemoteEndPoint(),
		*WebSocket->LocalEndPoint(),
		State == USOCK_Pending ? TEXT("Pending")
		: State == USOCK_Open ? TEXT("Open")
		: State == USOCK_Closed ? TEXT("Closed")
		: TEXT("Invalid")
		);
}


void UWebSocketConnection::SetWebSocket(FWebSocket* InWebSocket)
{
	WebSocket = InWebSocket; 
}

FWebSocket* UWebSocketConnection::GetWebSocket()
{
	return WebSocket;
}


void UWebSocketConnection::Tick()
{
	UNetConnection::Tick();
	WebSocket->Tick();
}

void UWebSocketConnection::FinishDestroy()
{
	Super::FinishDestroy();
	delete WebSocket; 
	WebSocket = NULL;

}

