// Copyright 2016 Oculus VR, LLC All Rights reserved.

#pragma once

#include "OnlineSubsystemOculusPrivatePCH.h"

#include "IPAddressOculus.h"
#include "OculusNetConnection.h"
#include "OculusNetDriver.h"
#include "OnlineSubsystemOculus.h"

#include "Net/DataChannel.h"

bool UOculusNetDriver::IsAvailable() const
{
	// Net driver won't work if the online subsystem doesn't exist
	IOnlineSubsystem* OculusSubsystem = IOnlineSubsystem::Get(OCULUS_SUBSYSTEM);
	if (OculusSubsystem)
	{
		return true;
	}
	return false;
}

ISocketSubsystem* UOculusNetDriver::GetSocketSubsystem()
{
	/** Not used */
	return nullptr;
}

FSocket * UOculusNetDriver::CreateSocket()
{
	/** Not used */
	return nullptr;
}

bool UOculusNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	// Listen for network state
	if (!NetworkingConnectionStateChangeDelegateHandle.IsValid())
	{
		auto OnlineSubsystem = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get(OCULUS_SUBSYSTEM));
		NetworkingConnectionStateChangeDelegateHandle =
			OnlineSubsystem->GetNotifDelegate(ovrMessage_Notification_Networking_ConnectionStateChange)
			.AddUObject(this, &UOculusNetDriver::OnNetworkingConnectionStateChange);
	}

	return true;
}

bool UOculusNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	UE_LOG(LogNet, Verbose, TEXT("Connecting to host: %s"), *ConnectURL.ToString(true));

	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		return false;
	}

	auto OculusAddr = new FInternetAddrOculus(ConnectURL);
	if (!OculusAddr->IsValid())
	{
		return false;
	}

	// Create an unreal connection to the server
	UOculusNetConnection* Connection = NewObject<UOculusNetConnection>(NetConnectionClass);
	check(Connection);
	TSharedRef<FInternetAddr> InternetAddr = MakeShareable(OculusAddr);
	Connection->InitRemoteConnection(this, nullptr, ConnectURL, *InternetAddr, ovr_Net_IsConnected(OculusAddr->GetID()) ? USOCK_Open : USOCK_Pending);

	ServerConnection = Connection;
	Connections.Add(OculusAddr->GetID(), Connection);

	// Connect via OVR_Networking
	ovr_Net_Connect(OculusAddr->GetID());

	// Create the control channel so we can send the Hello message
	ServerConnection->CreateChannel(CHTYPE_Control, true, INDEX_NONE);

	return true;
}

bool UOculusNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	if (!InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	// Listen for incoming peers
	if (!PeerConnectRequestDelegateHandle.IsValid())
	{
		auto OnlineSubsystem = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get(OCULUS_SUBSYSTEM));
		PeerConnectRequestDelegateHandle =
			OnlineSubsystem->GetNotifDelegate(ovrMessage_Notification_Networking_PeerConnectRequest)
			.AddUObject(this, &UOculusNetDriver::OnNewNetworkingPeerRequest);
	}

	UE_LOG(LogNet, Verbose, TEXT("Init as a listen server"));

	return true;
}

void UOculusNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);

	// Process all incoming packets.
	for (;;) {
		auto Packet = ovr_Net_ReadPacket();
		if (!Packet) {
			break;
		}
		auto PeerID = ovr_Packet_GetSenderID(Packet);
		auto PacketSize = static_cast<int32>(ovr_Packet_GetSize(Packet));
		if (Connections.Contains(PeerID) && Connections[PeerID]->State == EConnectionState::USOCK_Open)
		{
			UE_LOG(LogNet, VeryVerbose, TEXT("Got a raw packet of size %d"), PacketSize);

			auto Data = (uint8 *)ovr_Packet_GetBytes(Packet);
			Connections[PeerID]->ReceivedRawPacket(Data, PacketSize);
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("There is no connection to: %llu"), PeerID);
		}
		ovr_Packet_Free(Packet);
	}
}

void UOculusNetDriver::ProcessRemoteFunction(class AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, class UObject* SubObject)
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
				if (Connection && Connection->ViewTarget)
				{
					// Do relevancy check if unreliable.
					// Reliables will always go out. This is odd behavior. On one hand we wish to guarantee "reliables always get there". On the other
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

						InternalProcessRemoteFunction(Actor, SubObject, Connection, Function, Parameters, OutParms, Stack, bIsServer);
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
		InternalProcessRemoteFunction(Actor, SubObject, Connection, Function, Parameters, OutParms, Stack, bIsServer);
	}
	else
	{
		UE_LOG(LogNet, Warning, TEXT("UOculusNetDriver::ProcesRemoteFunction: No owning connection for actor %s. Function %s will not be processed."), *Actor->GetName(), *Function->GetName());
	}
}

void UOculusNetDriver::OnNewNetworkingPeerRequest(ovrMessageHandle Message, bool bIsError)
{
	// Ignore the peer if not accepting new connections
	if (Notify->NotifyAcceptingConnection() != EAcceptConnection::Accept)
	{
		UE_LOG(LogNet, Warning, TEXT("Not accepting more new connections"));
		return;
	}

	auto NetworkingPeer = ovr_Message_GetNetworkingPeer(Message);
	auto PeerID = ovr_NetworkingPeer_GetID(NetworkingPeer);

	UE_LOG(LogNet, Verbose, TEXT("New incoming peer request: %llu"), PeerID);

	// Create an unreal connection to the client
	UOculusNetConnection* Connection = NewObject<UOculusNetConnection>(NetConnectionClass);
	check(Connection);
	auto OculusAddr = new FInternetAddrOculus(PeerID);
	TSharedRef<FInternetAddr> InternetAddr = MakeShareable(OculusAddr);
	Connection->InitRemoteConnection(this, nullptr, FURL(), *InternetAddr, ovr_Net_IsConnected(PeerID) ? USOCK_Open : USOCK_Pending);

	AddClientConnection(Connection);

	Connections.Add(PeerID, Connection);

	// Accept the connection
	ovr_Net_Accept(PeerID);
	Notify->NotifyAcceptedConnection(Connection);

	// Listen for the 'Hello' message
	Connection->SetClientLoginState(EClientLoginState::LoggingIn);
	Connection->SetExpectedClientLoginMsgType(NMT_Hello);
}

void UOculusNetDriver::OnNetworkingConnectionStateChange(ovrMessageHandle Message, bool bIsError)
{

	auto NetworkingPeer = ovr_Message_GetNetworkingPeer(Message);

	auto PeerID = ovr_NetworkingPeer_GetID(NetworkingPeer);

	UE_LOG(LogNet, Verbose, TEXT("%llu changed network connection state"), PeerID);

	if (!Connections.Contains(PeerID))
	{
		UE_LOG(LogNet, Warning, TEXT("Peer ID not found in connections: %llu"), PeerID);
		return;
	}

	auto Connection = Connections[PeerID];

	auto State = ovr_NetworkingPeer_GetState(NetworkingPeer);
	if (State == ovrPeerState_Connected)
	{
		UE_LOG(LogNet, Verbose, TEXT("%llu is connected"), PeerID);
		Connection->State = EConnectionState::USOCK_Open;
	}
	else if (State == ovrPeerState_Closed)
	{
		UE_LOG(LogNet, Verbose, TEXT("%llu is closed"), PeerID);
		Connection->State = EConnectionState::USOCK_Closed;
	}
	else if (State == ovrPeerState_Timeout)
	{
		UE_LOG(LogNet, Warning, TEXT("%llu timed out"), PeerID);
		Connection->State = EConnectionState::USOCK_Closed;
	}
	else
	{
		UE_LOG(LogNet, Warning, TEXT("%llu is in an unknown state"), PeerID);
	}
}

void UOculusNetDriver::Shutdown()
{
	Super::Shutdown();

	auto OnlineSubsystem = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get(OCULUS_SUBSYSTEM));
	if (PeerConnectRequestDelegateHandle.IsValid())
	{
		OnlineSubsystem->RemoveNotifDelegate(ovrMessage_Notification_Networking_PeerConnectRequest, PeerConnectRequestDelegateHandle);
		PeerConnectRequestDelegateHandle.Reset();
	}
	if (NetworkingConnectionStateChangeDelegateHandle.IsValid())
	{
		OnlineSubsystem->RemoveNotifDelegate(ovrMessage_Notification_Networking_ConnectionStateChange, NetworkingConnectionStateChangeDelegateHandle);
		NetworkingConnectionStateChangeDelegateHandle.Reset();
	}
}

bool UOculusNetDriver::IsNetResourceValid()
{
	if (!IsAvailable())
	{
		return false;
	}

	// The listen server is always available
	if (IsServer())
	{
		return true;
	}

	// The clients need to wait until the connection is established before sending packets
	return ServerConnection->State == EConnectionState::USOCK_Open;
}
