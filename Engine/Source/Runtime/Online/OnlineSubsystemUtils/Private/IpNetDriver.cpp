// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IpNetDriver.cpp: Unreal IP network driver.
Notes:
	* See \msdev\vc98\include\winsock.h and \msdev\vc98\include\winsock2.h 
	  for Winsock WSAE* errors returned by Windows Sockets.
=============================================================================*/

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "Engine/Channel.h"

#include "IPAddress.h"
#include "Sockets.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

/** Size of the network recv buffer */
#define NETWORK_MAX_PACKET (576)

UIpNetDriver::UIpNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UIpNetDriver::IsAvailable() const
{
	// IP driver always valid for now
	return true;
}

ISocketSubsystem* UIpNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get();
}

FSocket * UIpNetDriver::CreateSocket()
{
	// Create UDP socket and enable broadcasting.
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("UIpNetDriver::CreateSocket: Unable to find socket subsystem"));
		return NULL;
	}

	return SocketSubsystem->CreateSocket( NAME_DGram, TEXT( "Unreal" ) );
}

int UIpNetDriver::GetClientPort()
{
	return 0;
}

bool UIpNetDriver::InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error )
{
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("Unable to find socket subsystem"));
		return false;
	}

	// Derived types may have already allocated a socket

	// Create the socket that we will use to communicate with
	Socket = CreateSocket();

	if( Socket == NULL )
	{
		Socket = 0;
		Error = FString::Printf( TEXT("WinSock: socket failed (%i)"), (int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}
	if (SocketSubsystem->RequiresChatDataBeSeparate() == false &&
		Socket->SetBroadcast() == false)
	{
		Error = FString::Printf( TEXT("%s: setsockopt SO_BROADCAST failed (%i)"), SocketSubsystem->GetSocketAPIName(), (int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}

	if (Socket->SetReuseAddr(bReuseAddressAndPort) == false)
	{
		UE_LOG(LogNet, Log, TEXT("setsockopt with SO_REUSEADDR failed"));
	}

	if (Socket->SetRecvErr() == false)
	{
		UE_LOG(LogNet, Log, TEXT("setsockopt with IP_RECVERR failed"));
	}

	// Increase socket queue size, because we are polling rather than threading
	// and thus we rely on Windows Sockets to buffer a lot of data on the server.
	int32 RecvSize = bInitAsClient ? 0x8000 : 0x20000;
	int32 SendSize = bInitAsClient ? 0x8000 : 0x20000;
	Socket->SetReceiveBufferSize(RecvSize,RecvSize);
	Socket->SetSendBufferSize(SendSize,SendSize);
	UE_LOG(LogInit, Log, TEXT("%s: Socket queue %i / %i"), SocketSubsystem->GetSocketAPIName(), RecvSize, SendSize );

	// Bind socket to our port.
	LocalAddr = SocketSubsystem->GetLocalBindAddr(*GLog);
	
	LocalAddr->SetPort(bInitAsClient ? GetClientPort() : URL.Port);
	
	int32 AttemptPort = LocalAddr->GetPort();
	int32 BoundPort = SocketSubsystem->BindNextPort( Socket, *LocalAddr, MaxPortCountToTry + 1, 1 );
	if (BoundPort == 0)
	{
		Error = FString::Printf( TEXT("%s: binding to port %i failed (%i)"), SocketSubsystem->GetSocketAPIName(), AttemptPort,
			(int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}
	if( Socket->SetNonBlocking() == false )
	{
		Error = FString::Printf( TEXT("%s: SetNonBlocking failed (%i)"), SocketSubsystem->GetSocketAPIName(),
			(int32)SocketSubsystem->GetLastErrorCode());
		return false;
	}

	// Success.
	return true;
}

bool UIpNetDriver::InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
	if( !InitBase( true, InNotify, ConnectURL, false, Error ) )
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ConnectURL: %s: %s"), *ConnectURL.ToString(), *Error);
		return false;
	}

	// Create new connection.
	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), NetConnectionClass);
	ServerConnection->InitLocalConnection( this, Socket, ConnectURL, USOCK_Pending);
	UE_LOG(LogNet, Log, TEXT("Game client on port %i, rate %i"), ConnectURL.Port, ServerConnection->CurrentNetSpeed );

	// Create channel zero.
	GetServerConnection()->CreateChannel( CHTYPE_Control, 1, 0 );

	return true;
}

bool UIpNetDriver::InitListen( FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error )
{
	if( !InitBase( false, InNotify, LocalURL, bReuseAddressAndPort, Error ) )
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ListenURL: %s: %s"), *LocalURL.ToString(), *Error);
		return false;
	}

	// Update result URL.
	//LocalURL.Host = LocalAddr->ToString(false);
	LocalURL.Port = LocalAddr->GetPort();
	UE_LOG(LogNet, Log, TEXT("%s IpNetDriver listening on port %i"), *GetDescription(), LocalURL.Port );

	return true;
}

void UIpNetDriver::TickDispatch( float DeltaTime )
{
	Super::TickDispatch( DeltaTime );

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	// Process all incoming packets.
	uint8 Data[NETWORK_MAX_PACKET];
	TSharedRef<FInternetAddr> FromAddr = SocketSubsystem->CreateInternetAddr();
	for( ; Socket != NULL; )
	{
		int32 BytesRead = 0;
		// Get data, if any.
		CLOCK_CYCLES(RecvCycles);
		bool bOk = Socket->RecvFrom(Data, sizeof(Data), BytesRead, *FromAddr);
		UNCLOCK_CYCLES(RecvCycles);
		// Handle result.
		if( bOk == false )
		{
			ESocketErrors Error = SocketSubsystem->GetLastErrorCode();
			if(Error == SE_EWOULDBLOCK ||
			   Error == SE_NO_ERROR)
			{
				// No data or no error?
				break;
			}
			else
			{
				if( Error != SE_ECONNRESET && Error != SE_UDP_ERR_PORT_UNREACH )
				{
					UE_LOG(LogNet, Warning, TEXT("UDP recvfrom error: %i (%s) from %s"),
						(int32)Error,
						SocketSubsystem->GetSocketError(Error),
						*FromAddr->ToString(true));
					break;
				}
			}
		}
		// Figure out which socket the received data came from.
		UIpConnection* Connection = NULL;
		if (GetServerConnection() && (*GetServerConnection()->RemoteAddr == *FromAddr))
		{
			Connection = GetServerConnection();
		}
		for( int32 i=0; i<ClientConnections.Num() && !Connection; i++ )
		{
			UIpConnection* TestConnection = (UIpConnection*)ClientConnections[i]; 
            check(TestConnection);
			if(*TestConnection->RemoteAddr == *FromAddr)
			{
				Connection = TestConnection;
			}
		}

		if( bOk == false )
		{
			if( Connection )
			{
				if( Connection != GetServerConnection() )
				{
					// We received an ICMP port unreachable from the client, meaning the client is no longer running the game
					// (or someone is trying to perform a DoS attack on the client)

					// rcg08182002 Some buggy firewalls get occasional ICMP port
					// unreachable messages from legitimate players. Still, this code
					// will drop them unceremoniously, so there's an option in the .INI
					// file for servers with such flakey connections to let these
					// players slide...which means if the client's game crashes, they
					// might get flooded to some degree with packets until they timeout.
					// Either way, this should close up the usual DoS attacks.
					if ((Connection->State != USOCK_Open) || (!AllowPlayerPortUnreach))
					{
						if (LogPortUnreach)
						{
							UE_LOG(LogNet, Log, TEXT("Received ICMP port unreachable from client %s.  Disconnecting."),
								*FromAddr->ToString(true));
						}
						Connection->CleanUp();
					}
				}
			}
			else
			{
				if (LogPortUnreach)
				{
					UE_LOG(LogNet, Log, TEXT("Received ICMP port unreachable from %s.  No matching connection found."),
						*FromAddr->ToString(true));
				}
			}
		}
		else
		{
			// If we didn't find a client connection, maybe create a new one.
			if( !Connection )
			{
				// Determine if allowing for client/server connections
				const bool bAcceptingConnection = Notify->NotifyAcceptingConnection() == EAcceptConnection::Accept;

				if (bAcceptingConnection)
				{
					Connection = NewObject<UIpConnection>(GetTransientPackage(), NetConnectionClass);
                    check(Connection);
					Connection->InitRemoteConnection( this, Socket,  FURL(), *FromAddr, USOCK_Open);
					Notify->NotifyAcceptedConnection( Connection );
					AddClientConnection(Connection);
				}
			}

			// Send the packet to the connection for processing.
			if( Connection )
			{
				Connection->ReceivedRawPacket( Data, BytesRead );
			}
		}
	}
}

void UIpNetDriver::ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, class UObject* SubObject )
{
	bool bIsServer = IsServer();

	UNetConnection* Connection = NULL;
	if (bIsServer)
	{
		if ((Function->FunctionFlags & FUNC_NetMulticast))
		{
			// Multicast functions go to every client
			TArray<UNetConnection*> UniqueRealConnections;
			for (int32 i=0; i<ClientConnections.Num(); ++i)
			{
				Connection = ClientConnections[i];
				if (Connection && Connection->ViewTarget)
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
						FNetViewer Viewer(Connection, 0.f);
						IsRelevant = Actor->IsNetRelevantFor(Viewer.InViewer, Viewer.ViewTarget, Viewer.ViewLocation);
					}
					
					if (IsRelevant)
					{
						if (Connection->GetUChildConnection() != NULL)
						{
							Connection = ((UChildConnection*)Connection)->Parent;
						}
						
						InternalProcessRemoteFunction( Actor, SubObject, Connection, Function, Parameters, OutParms, Stack, bIsServer );
					}
				}
			}			

			// Replicate any RPCs to the replay net driver so that they can get saved in network replays
			UNetDriver* NetDriver = GEngine->FindNamedNetDriver(GetWorld(), NAME_DemoNetDriver);
			if (NetDriver)
			{
				NetDriver->ProcessRemoteFunction(Actor, Function, Parameters, OutParms, Stack, SubObject);
			}
			// Return here so we don't call InternalProcessRemoteFunction again at the bottom of this function
			return;
		}
	}

	// Send function data to remote.
	Connection = Actor->GetNetConnection();
	if (Connection)
	{
		InternalProcessRemoteFunction( Actor, SubObject, Connection, Function, Parameters, OutParms, Stack, bIsServer );
	}
}

FString UIpNetDriver::LowLevelGetNetworkNumber()
{
	return LocalAddr->ToString(true);
}

void UIpNetDriver::LowLevelDestroy()
{
	Super::LowLevelDestroy();

	// Close the socket.
	if( Socket && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
		if( !Socket->Close() )
		{
			UE_LOG(LogExit, Log, TEXT("closesocket error (%i)"), (int32)SocketSubsystem->GetLastErrorCode() );
		}
		// Free the memory the OS allocated for this socket
		SocketSubsystem->DestroySocket(Socket);
		Socket = NULL;
		UE_LOG(LogExit, Log, TEXT("%s shut down"),*GetDescription() );
	}

}


bool UIpNetDriver::HandleSocketsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	Ar.Logf(TEXT(""));
	if (Socket != NULL)
	{
		TSharedRef<FInternetAddr> LocalInternetAddr = GetSocketSubsystem()->CreateInternetAddr();
		Socket->GetAddress(*LocalInternetAddr);
		Ar.Logf(TEXT("%s Socket: %s"), *GetDescription(), *LocalInternetAddr->ToString(true));
	}		
	else
	{
		Ar.Logf(TEXT("%s Socket: null"), *GetDescription());
	}
	return UNetDriver::Exec( InWorld, TEXT("SOCKETS"),Ar);
}

bool UIpNetDriver::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd,TEXT("SOCKETS")))
	{
		return HandleSocketsCommand( Cmd, Ar, InWorld );
	}
	return UNetDriver::Exec( InWorld, Cmd,Ar);
}

UIpConnection* UIpNetDriver::GetServerConnection() 
{
	return (UIpConnection*)ServerConnection;
}

