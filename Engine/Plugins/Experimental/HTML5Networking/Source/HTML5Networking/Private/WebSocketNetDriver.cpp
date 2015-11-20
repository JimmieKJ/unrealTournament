// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Unreal Websocket network driver.
=============================================================================*/

#include "HTML5NetworkingPCH.h"
#include "Engine/Channel.h"

#include "IPAddress.h"
#include "Sockets.h"

#include "WebSocketConnection.h"
#include "WebSocketNetDriver.h"
#include "WebSocketServer.h"
#include "WebSocket.h"


/*-----------------------------------------------------------------------------
Declarations.
-----------------------------------------------------------------------------*/

/** Size of the network recv buffer */
#define NETWORK_MAX_PACKET (576)

UWebSocketNetDriver::UWebSocketNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UWebSocketNetDriver::IsAvailable() const
{
	// Websocket driver always valid for now
	return true;
}



ISocketSubsystem* UWebSocketNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get();
}

FSocket * UWebSocketNetDriver::CreateSocket()
{
	return NULL; 
}

bool UWebSocketNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	return true;
}

bool UWebSocketNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		return false;
	}

	// Create new connection.
	ServerConnection = NewObject<UWebSocketConnection>(NetConnectionClass);

	TSharedRef<FInternetAddr> InternetAddr = GetSocketSubsystem()->CreateInternetAddr();
	bool Ok;
	InternetAddr->SetIp(*ConnectURL.Host, Ok);
	InternetAddr->SetPort(WebSocketPort);

	FWebSocket* WebSocket = new FWebSocket(*InternetAddr);
	UWebSocketConnection* Connection = (UWebSocketConnection*)ServerConnection;
	Connection->SetWebSocket(WebSocket);

	FWebsocketPacketRecievedCallBack CallBack;
	CallBack.BindUObject(Connection, &UWebSocketConnection::ReceivedRawPacket);
	WebSocket->SetRecieveCallBack(CallBack);

	FWebsocketInfoCallBack  ConnectedCallBack;
	ConnectedCallBack.BindUObject(this, &UWebSocketNetDriver::OnWebSocketServerConnected);
	WebSocket->SetConnectedCallBack(ConnectedCallBack);

	ServerConnection->InitLocalConnection(this, NULL, ConnectURL, USOCK_Pending);

	// Create channel zero.
	GetServerConnection()->CreateChannel(CHTYPE_Control, 1, 0);

	return true;
}

bool UWebSocketNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	if (!InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	WebSocketServer = new class FWebSocketServer();

	FWebsocketClientConnectedCallBack CallBack;
	CallBack.BindUObject(this, &UWebSocketNetDriver::OnWebSocketClientConnected);

	if(!WebSocketServer->Init(WebSocketPort, CallBack))
		return false; 

	WebSocketServer->Tick();
	LocalURL.Port = WebSocketPort;
	UE_LOG(LogHTML5Networking, Log, TEXT("%s WebSocketNetDriver listening on port %i"), *GetDescription(), LocalURL.Port);

	// server has no server connection. 
	ServerConnection = NULL; 
	return true;
}

void UWebSocketNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);

	if (WebSocketServer)
		WebSocketServer->Tick();
}

void UWebSocketNetDriver::ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, class UObject* SubObject)
{
	bool bIsServer = IsServer();

	UNetConnection* Connection = NULL;
	if (bIsServer)
	{
		if ((Function->FunctionFlags & FUNC_NetMulticast))
		{
			// Multicast functions go to every client
			TArray<UNetConnection*> UniqueRealConnections;
			for (int32 i = 0; i<ClientConnections.Num(); ++i)
			{
				Connection = ClientConnections[i];
				if (Connection)
				{
					// Do relevancy check if unreliable.
					// Reliables will always go out. This is odd behavior. On one hand we wish to garuntee "reliables always get there". On the other
					// hand, replicating a reliable to something on the other side of the map that is non relevant seems weird. 
					//
					// Multicast reliables should probably never be used in gameplay code for actors that have relevancy checks. If they are, the 
					// rpc will go through and the channel will be closed soon after due to relevancy failing.

					bool IsRelevant = true;
					if ((Function->FunctionFlags & FUNC_NetReliable) == 0)
					{
						if (Connection->ViewTarget)
						{
							FNetViewer Viewer(Connection, 0.f);
							IsRelevant = Actor->IsNetRelevantFor(Viewer.InViewer, Viewer.ViewTarget, Viewer.ViewLocation);
						}
						else
						{
							// No viewer for this connection(?), just let it go through.
							UE_LOG(LogHTML5Networking, Log, TEXT("Multicast function %s called on actor %s when a connection has no Viewer"), *Function->GetName(), *Actor->GetName());
						}
					}

					if (IsRelevant)
					{
						if (Connection->GetUChildConnection() != NULL)
						{
							Connection = ((UChildConnection*)Connection)->Parent;
						}

						InternalProcessRemoteFunction(Actor, SubObject, Connection, Function, Parameters, OutParms, Stack, bIsServer);
					}
				}
			}

			// Return here so we don't call InternalProcessRemoteFunction again at the bottom of this function
			return;
		}
	}

	// Send function data to remote.
	Connection = Actor->GetNetConnection();
	if (Connection)
	{
		InternalProcessRemoteFunction(Actor, SubObject, Connection, Function, Parameters, OutParms, Stack, bIsServer);
	}
}

FString UWebSocketNetDriver::LowLevelGetNetworkNumber()
{
	return WebSocketServer->Info(); 
}

void UWebSocketNetDriver::LowLevelDestroy()
{
	Super::LowLevelDestroy();
	delete WebSocketServer; 
	WebSocketServer = nullptr;
}

bool UWebSocketNetDriver::HandleSocketsCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	Ar.Logf(TEXT(""));
	if (WebSocketServer != NULL)
	{
		Ar.Logf(TEXT("Running WebSocket Server %s"), *WebSocketServer->Info());
	}
	else
	{
		check(GetServerConnection());
		Ar.Logf(TEXT("WebSocket client's EndPoint %s"), *(GetServerConnection()->WebSocket->RemoteEndPoint()));
	}
	return UNetDriver::Exec(InWorld, TEXT("SOCKETS"), Ar);
}

bool UWebSocketNetDriver::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("SOCKETS")))
	{
		return HandleSocketsCommand(Cmd, Ar, InWorld);
	}
	return UNetDriver::Exec(InWorld, Cmd, Ar);
}

UWebSocketConnection* UWebSocketNetDriver::GetServerConnection()
{
	return (UWebSocketConnection*)ServerConnection;
}

void UWebSocketNetDriver::OnWebSocketClientConnected(FWebSocket* ClientWebSocket)
{
	// Determine if allowing for client/server connections
	const bool bAcceptingConnection = Notify->NotifyAcceptingConnection() == EAcceptConnection::Accept;
	if (bAcceptingConnection)
	{

		UWebSocketConnection* Connection = NewObject<UWebSocketConnection>(NetConnectionClass);
		check(Connection); 

		TSharedRef<FInternetAddr> InternetAddr = GetSocketSubsystem()->CreateInternetAddr();
		bool Ok;

		InternetAddr->SetIp(*(ClientWebSocket->RemoteEndPoint()),Ok);
		InternetAddr->SetPort(0);
		Connection->SetWebSocket(ClientWebSocket);
		Connection->InitRemoteConnection(this, NULL, FURL(), *InternetAddr, USOCK_Open);
		
		Notify->NotifyAcceptedConnection(Connection);

		AddClientConnection(Connection);

 		FWebsocketPacketRecievedCallBack CallBack;
 		CallBack.BindUObject(Connection, &UWebSocketConnection::ReceivedRawPacket);
 		ClientWebSocket->SetRecieveCallBack(CallBack);

		UE_LOG(LogHTML5Networking, Log, TEXT(" Websocket server running on %s Accepted Connection from %s "), *WebSocketServer->Info(),*ClientWebSocket->RemoteEndPoint());
	}
}

bool UWebSocketNetDriver::IsNetResourceValid(void)
{
	if (	(WebSocketServer && !ServerConnection)//  Server 
		||	(!WebSocketServer && ServerConnection) // client
		)
	{
		return true; 
	}
		
	return false; 
}

// Just logging, not yet attached to html5 clients.
void UWebSocketNetDriver::OnWebSocketServerConnected()
{
	check(GetServerConnection());
	UE_LOG(LogHTML5Networking, Log, TEXT(" %s Websocket Client connected to server %s "), *GetDescription(), *GetServerConnection()->WebSocket->RemoteEndPoint());
}

