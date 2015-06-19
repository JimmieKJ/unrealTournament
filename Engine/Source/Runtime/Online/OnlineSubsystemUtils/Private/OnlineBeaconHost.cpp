// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "Net/UnrealNetwork.h"
#include "Net/DataChannel.h"

AOnlineBeaconHost::AOnlineBeaconHost(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	NetDriverName = FName(TEXT("BeaconDriverHost"));
}

void AOnlineBeaconHost::OnNetCleanup(UNetConnection* Connection)
{
	UE_LOG(LogBeacon, Error, TEXT("Cleaning up a beacon host!"));
}

bool AOnlineBeaconHost::InitHost()
{
	FURL URL(nullptr, TEXT(""), TRAVEL_Absolute);

	// Allow the command line to override the default port
	int32 PortOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("BeaconPort="), PortOverride) && PortOverride != 0)
	{
		ListenPort = PortOverride;
	}

	URL.Port = ListenPort;
	if(URL.Valid)
	{
		if (InitBase() && NetDriver)
		{
			FString Error;
			if (NetDriver->InitListen(this, URL, false, Error))
			{
				ListenPort = URL.Port;
				NetDriver->SetWorld(GetWorld());
				NetDriver->Notify = this;
				NetDriver->InitialConnectTimeout = BeaconConnectionInitialTimeout;
				NetDriver->ConnectionTimeout = BeaconConnectionTimeout;
				return true;
			}
			else
			{
				// error initializing the network stack...
				UE_LOG(LogNet, Log, TEXT("AOnlineBeaconHost::InitHost failed"));
				OnFailure();
			}
		}
	}

	return false;
}

void AOnlineBeaconHost::HandleNetworkFailure(UWorld* World, class UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (NetDriver && NetDriver->NetDriverName == NetDriverName)
	{
		// Timeouts from clients are ignored
		if (FailureType != ENetworkFailure::ConnectionTimeout)
		{
			Super::HandleNetworkFailure(World, NetDriver, FailureType, ErrorString);
		}
	}
}

void AOnlineBeaconHost::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch)
{
	if(NetDriver->ServerConnection == nullptr)
	{
		bool bCloseConnection = false;

		// We are the server.
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
		UE_LOG(LogNet, Verbose, TEXT("Beacon: Host received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif
		switch (MessageType)
		{
		case NMT_Hello:
			{
				UE_LOG(LogNet, Log, TEXT("Beacon Hello"));
				uint8 IsLittleEndian;

				uint32 RemoteNetworkVersion = 0;
				uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();

				FNetControlMessage<NMT_Hello>::Receive(Bunch, IsLittleEndian, RemoteNetworkVersion);

				if (!FNetworkVersion::IsNetworkCompatible(LocalNetworkVersion, RemoteNetworkVersion))
				{
					FNetControlMessage<NMT_Upgrade>::Send(Connection, LocalNetworkVersion);
					bCloseConnection = true;
				}
				else
				{
					Connection->Challenge = FString::Printf(TEXT("%08X"), FPlatformTime::Cycles());
					FNetControlMessage<NMT_BeaconWelcome>::Send(Connection);
					Connection->FlushNet();
				}
				break;
			}
		case NMT_Netspeed:
			{
				int32 Rate;
				FNetControlMessage<NMT_Netspeed>::Receive(Bunch, Rate);
				Connection->CurrentNetSpeed = FMath::Clamp(Rate, 1800, NetDriver->MaxClientRate);
				UE_LOG(LogNet, Log, TEXT("Beacon: Client netspeed is %i"), Connection->CurrentNetSpeed);
				break;
			}
		case NMT_BeaconJoin:
			{
				FString ErrorMsg;
				FString BeaconType;
				FNetControlMessage<NMT_BeaconJoin>::Receive(Bunch, BeaconType);
				UE_LOG(LogNet, Log, TEXT("Beacon Join %s"), *BeaconType);

				if (Connection->ClientWorldPackageName == NAME_None)
				{
					AOnlineBeaconClient* ClientActor = GetClientActor(Connection);
					if (ClientActor == nullptr)
					{
						UWorld* World = GetWorld();
						Connection->ClientWorldPackageName = World->GetOutermost()->GetFName();

						AOnlineBeaconClient* NewClientActor = nullptr;
						FOnBeaconSpawned* OnBeaconSpawnedDelegate = OnBeaconSpawnedMapping.Find(BeaconType);
						if (OnBeaconSpawnedDelegate && OnBeaconSpawnedDelegate->IsBound())
						{
							NewClientActor = OnBeaconSpawnedDelegate->Execute(Connection);
						}

						if (NewClientActor && BeaconType == NewClientActor->GetBeaconType())
						{
							FNetworkGUID NetGUID = Connection->Driver->GuidCache->AssignNewNetGUID_Server( NewClientActor );
							NewClientActor->SetNetConnection(Connection);
							Connection->OwningActor = NewClientActor;
							NewClientActor->Role = ROLE_None;
							NewClientActor->SetReplicates(false);
							check(NetDriverName == NetDriver->NetDriverName);
							NewClientActor->NetDriverName = NetDriverName;
							ClientActors.Add(NewClientActor);
							FNetControlMessage<NMT_BeaconAssignGUID>::Send(Connection, NetGUID);
						}
						else
						{
							ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnFailureError", "Join failure, Couldn't spawn beacon.").ToString();
						}
					}
					else
					{
						ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnExistingActorError", "Join failure, existing beacon actor.").ToString();
					}
				}
				else
				{
					ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnClientWorldPackageNameError", "Join failure, existing ClientWorldPackageName.").ToString();
				}

				if (!ErrorMsg.IsEmpty())
				{
					UE_LOG(LogNet, Log, TEXT("%s"), *ErrorMsg);
					FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
					bCloseConnection = true;
				}

				break;
			}
		case NMT_BeaconNetGUIDAck:
			{
				FString BeaconType;
				FNetControlMessage<NMT_BeaconNetGUIDAck>::Receive(Bunch, BeaconType);

				AOnlineBeaconClient* ClientActor = GetClientActor(Connection);
				if (ClientActor && BeaconType == ClientActor->GetBeaconType())
				{
					ClientActor->Role = ROLE_Authority;
					ClientActor->SetReplicates(true);
					ClientActor->SetAutonomousProxy(true);
					// Send an RPC to the client to open the actor channel and guarantee RPCs will work
					ClientActor->ClientOnConnected();
					UE_LOG(LogNet, Log, TEXT("Beacon Handshake complete!"));
					FOnBeaconConnected* OnBeaconConnectedDelegate = OnBeaconConnectedMapping.Find(BeaconType);
					if (OnBeaconConnectedDelegate)
					{
						OnBeaconConnectedDelegate->ExecuteIfBound(ClientActor, Connection);
					}
				}
				else
				{
					// Failed to connect.
					FString ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnNetGUIDAckError", "Join failure, no actor at NetGUIDAck.").ToString();
					UE_LOG(LogNet, Log, TEXT("%s"), *ErrorMsg);
					FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
					bCloseConnection = true;
				}
				break;
			}
		case NMT_BeaconWelcome:
		case NMT_BeaconAssignGUID:
		default:
			{
				FString ErrorMsg = NSLOCTEXT("NetworkErrors", "BeaconSpawnUnexpectedError", "Join failure, unexpected control message.").ToString();
				UE_LOG(LogNet, Log, TEXT("%s"), *ErrorMsg);
				FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
				bCloseConnection = true;
			}
			break;
		}

		if (bCloseConnection)
		{		
			AOnlineBeaconClient* ClientActor = GetClientActor(Connection);
			if (ClientActor)
			{
				RemoveClientActor(ClientActor);
			}

			Connection->FlushNet(true);
			Connection->Close();
		}
	}
}

void AOnlineBeaconHost::GetSeamlessTravelActorList(bool bToEntry, TArray<AActor*>& ActorList)
{
	for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ChildIdx++)
	{
		ActorList.Add(Children[ChildIdx]);
	}

	for (int32 ClientIdx = 0; ClientIdx < ClientActors.Num(); ClientIdx++)
	{
		ActorList.Add(ClientActors[ClientIdx]);
	}
}

AOnlineBeaconClient* AOnlineBeaconHost::GetClientActor(UNetConnection* Connection)
{
	for (int32 ClientIdx=0; ClientIdx < ClientActors.Num(); ClientIdx++)
	{
		if (ClientActors[ClientIdx]->GetNetConnection() == Connection)
		{
			return ClientActors[ClientIdx];
		}
	}

	return nullptr;
}

void AOnlineBeaconHost::RemoveClientActor(AOnlineBeaconClient* ClientActor)
{
	if (ClientActor)
	{
		ClientActors.RemoveSingleSwap(ClientActor);
		ClientActor->Destroy();
	}
}

void AOnlineBeaconHost::RegisterHost(AOnlineBeaconHostObject* NewHostObject)
{
	const FString& BeaconType = NewHostObject->GetBeaconType();

	bool bFound = false;
	for (int32 HostIdx=0; HostIdx < Children.Num(); HostIdx++)
	{
		AOnlineBeaconHostObject* HostObject = Cast<AOnlineBeaconHostObject>(Children[HostIdx]);
		if (HostObject && HostObject->GetBeaconType() == BeaconType)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		NewHostObject->SetOwner(this);
		OnBeaconSpawned(BeaconType).BindUObject(NewHostObject, &AOnlineBeaconHostObject::SpawnBeaconActor);
		OnBeaconConnected(BeaconType).BindUObject(NewHostObject, &AOnlineBeaconHostObject::ClientConnected);
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("Beacon host type %s already exists"), *BeaconType);
	}
}

void AOnlineBeaconHost::UnregisterHost(const FString& BeaconType)
{
	AOnlineBeaconHostObject* HostObject = GetHost(BeaconType);
	if (HostObject)
	{
		HostObject->SetOwner(nullptr);
	}
	
	OnBeaconSpawned(BeaconType).Unbind();
	OnBeaconConnected(BeaconType).Unbind();
}

AOnlineBeaconHostObject* AOnlineBeaconHost::GetHost(const FString& BeaconType)
{
	for (int32 HostIdx=0; HostIdx < Children.Num(); HostIdx++)
	{
		AOnlineBeaconHostObject* HostObject = Cast<AOnlineBeaconHostObject>(Children[HostIdx]);
		if (HostObject && HostObject->GetBeaconType() == BeaconType)
		{
			return HostObject;
		}
	}

	return nullptr;
}

AOnlineBeaconHost::FOnBeaconSpawned& AOnlineBeaconHost::OnBeaconSpawned(const FString& BeaconType)
{ 
	FOnBeaconSpawned* BeaconDelegate = OnBeaconSpawnedMapping.Find(BeaconType);
	if (BeaconDelegate == nullptr)
	{
		FOnBeaconSpawned NewDelegate;
		OnBeaconSpawnedMapping.Add(BeaconType, NewDelegate);
		BeaconDelegate = OnBeaconSpawnedMapping.Find(BeaconType);
	}

	return *BeaconDelegate; 
}

AOnlineBeaconHost::FOnBeaconConnected& AOnlineBeaconHost::OnBeaconConnected(const FString& BeaconType)
{ 
	FOnBeaconConnected* BeaconDelegate = OnBeaconConnectedMapping.Find(BeaconType);
	if (BeaconDelegate == nullptr)
	{
		FOnBeaconConnected NewDelegate;
		OnBeaconConnectedMapping.Add(BeaconType, NewDelegate);
		BeaconDelegate = OnBeaconConnectedMapping.Find(BeaconType);
	}

	return *BeaconDelegate; 
}


AOnlineBeaconHostObject::AOnlineBeaconHostObject(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	BeaconTypeName(TEXT("UNDEFINED"))
{
	PrimaryActorTick.bCanEverTick = true;
}

EBeaconState::Type AOnlineBeaconHostObject::GetBeaconState() const
{
	AOnlineBeaconHost* BeaconHost = Cast<AOnlineBeaconHost>(GetOwner());
	if (BeaconHost)
	{
		return BeaconHost->GetBeaconState();
	}

	return EBeaconState::DenyRequests;
}

void AOnlineBeaconHostObject::RemoveClientActor(AOnlineBeaconClient* ClientActor)
{
	AOnlineBeaconHost* BeaconHost = Cast<AOnlineBeaconHost>(GetOwner());
	if (BeaconHost)
	{
		BeaconHost->RemoveClientActor(ClientActor);
	}
}