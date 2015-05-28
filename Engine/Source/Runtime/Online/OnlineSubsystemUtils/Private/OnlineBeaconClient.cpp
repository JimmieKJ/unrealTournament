// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "Net/DataChannel.h"
#include "Net/UnrealNetwork.h"

#define BEACON_RPC_TIMEOUT 15.0f

AOnlineBeaconClient::AOnlineBeaconClient(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	BeaconOwner(nullptr),
	BeaconConnection(nullptr)
{
	NetDriverName = FName(TEXT("BeaconDriverClient"));
	bOnlyRelevantToOwner = true;
}

AOnlineBeaconHostObject* AOnlineBeaconClient::GetBeaconOwner() const
{
	return BeaconOwner;
}

void AOnlineBeaconClient::SetBeaconOwner(AOnlineBeaconHostObject* InBeaconOwner)
{
	BeaconOwner = InBeaconOwner;
}

UNetConnection* AOnlineBeaconClient::GetNetConnection() const
{
	return BeaconConnection;
}

bool AOnlineBeaconClient::InitClient(FURL& URL)
{
	bool bSuccess = false;

	if(URL.Valid)
	{
		if (InitBase() && NetDriver)
		{
			FString Error;
			if (NetDriver->InitConnect(this, URL, Error))
			{
				NetDriver->SetWorld(GetWorld());
				NetDriver->Notify = this;
				NetDriver->InitialConnectTimeout = BeaconConnectionInitialTimeout;
				NetDriver->ConnectionTimeout = BeaconConnectionInitialTimeout;

				// Send initial message.
				uint8 IsLittleEndian = uint8(PLATFORM_LITTLE_ENDIAN);
				check(IsLittleEndian == !!IsLittleEndian); // should only be one or zero
				BeaconConnection = NetDriver->ServerConnection;

				uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();
				
				FNetControlMessage<NMT_Hello>::Send(NetDriver->ServerConnection, IsLittleEndian, LocalNetworkVersion);
				NetDriver->ServerConnection->FlushNet();

				bSuccess = true;
			}
			else
			{
				// error initializing the network stack...
				UE_LOG(LogNet, Log, TEXT("AOnlineBeaconClient::InitClient failed"));
				OnFailure();
			}
		}
	}

	return bSuccess;
}

void AOnlineBeaconClient::OnFailure()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Client beacon (%s) connection failure, handling connection timeout."), *GetName());
	HostConnectionFailure.ExecuteIfBound();
	Super::OnFailure();
}

void AOnlineBeaconClient::ClientOnConnected_Implementation()
{
	Role = ROLE_Authority;
	SetReplicates(true);
	SetAutonomousProxy(true);

	// Fail safe for connection to server but no client connection RPC
	GetWorldTimerManager().ClearTimer(TimerHandle_OnFailure);

	if (NetDriver)
	{
		// Increase timeout while we are connected
		NetDriver->InitialConnectTimeout = BeaconConnectionTimeout;
		NetDriver->ConnectionTimeout = BeaconConnectionTimeout;
	}

	// Call the overloaded function for this client class
	OnConnected();
}

void AOnlineBeaconClient::DestroyBeacon()
{
	UWorld* World = GetWorld();
	if (World)
	{
		// Fail safe for connection to server but no client connection RPC
		GetWorldTimerManager().ClearTimer(TimerHandle_OnFailure);
	}

	Super::DestroyBeacon();
}

void AOnlineBeaconClient::OnNetCleanup(UNetConnection* Connection)
{
	AOnlineBeaconHostObject* BeaconHost = GetBeaconOwner();
	if (BeaconHost)
	{
		BeaconHost->RemoveClientActor(this);
	}
}

void AOnlineBeaconClient::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch)
{
	if(NetDriver->ServerConnection)
	{
		check(Connection == NetDriver->ServerConnection);

		// We are the client
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
		UE_LOG(LogNet, Log, TEXT("Beacon: Client received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
		switch (MessageType)
		{
		case NMT_BeaconWelcome:
			{
				Connection->ClientResponse = TEXT("0");
				FNetControlMessage<NMT_Netspeed>::Send(Connection, Connection->CurrentNetSpeed);

				FString BeaconType = GetBeaconType();
				if (!BeaconType.IsEmpty())
				{
					FNetControlMessage<NMT_BeaconJoin>::Send(Connection, BeaconType);
					NetDriver->ServerConnection->FlushNet();
				}
				else
				{
					// Force close the session
					UE_LOG(LogNet, Log, TEXT("Beacon close from invalid beacon type"));
					OnFailure();
				}
				break;
			}
		case NMT_BeaconAssignGUID:
			{
				FNetworkGUID NetGUID;
				FNetControlMessage<NMT_BeaconAssignGUID>::Receive(Bunch, NetGUID);
				if (NetGUID.IsValid())
				{
					Connection->Driver->GuidCache->RegisterNetGUID_Client( NetGUID, this );

					FString BeaconType = GetBeaconType();
					FNetControlMessage<NMT_BeaconNetGUIDAck>::Send(Connection, BeaconType);
					// Server will send ClientOnConnected() when it gets this control message

					// Fail safe for connection to server but no client connection RPC
					FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &AOnlineBeaconClient::OnFailure);
					GetWorldTimerManager().SetTimer(TimerHandle_OnFailure, TimerDelegate, BEACON_RPC_TIMEOUT, false);
				}
				else
				{
					// Force close the session
					UE_LOG(LogNet, Log, TEXT("Beacon close from invalid NetGUID"));
					OnFailure();
				}
				break;
			}
		case NMT_Upgrade:
			{
				// Report mismatch.
				uint32 RemoteNetworkVersion;
				FNetControlMessage<NMT_Upgrade>::Receive(Bunch, RemoteNetworkVersion);
				// Upgrade
				const FString ConnectionError = NSLOCTEXT("Engine", "ClientOutdated", "The match you are trying to join is running an incompatible version of the game.  Please try upgrading your game version.").ToString();
				GEngine->BroadcastNetworkFailure(GetWorld(), NetDriver, ENetworkFailure::OutdatedClient, ConnectionError);
				break;
			}
		case NMT_Failure:
			{
				FString ErrorMsg;
				FNetControlMessage<NMT_Failure>::Receive(Bunch, ErrorMsg);
				if (ErrorMsg.IsEmpty())
				{
					ErrorMsg = NSLOCTEXT("NetworkErrors", "GenericBeaconConnectionFailed", "Beacon Connection Failed.").ToString();
				}

				// Force close the session
				UE_LOG(LogNet, Log, TEXT("Beacon close from NMT_Failure %s"), *ErrorMsg);
				OnFailure();
				break;
			}
		case NMT_BeaconJoin:
		case NMT_BeaconNetGUIDAck:
		default:
			{
				// Force close the session
				UE_LOG(LogNet, Log, TEXT("Beacon close from unexpected control message"));
				OnFailure();
				break;
			}
		}
	}	
}
