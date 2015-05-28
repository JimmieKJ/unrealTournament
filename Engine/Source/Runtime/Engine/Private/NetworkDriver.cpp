// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetworkDriver.cpp: Unreal network driver base class.
=============================================================================*/

#include "EnginePrivate.h"
#include "Net/DataReplication.h"
#include "Net/UnrealNetwork.h"
#include "Net/NetworkProfiler.h"
#include "Net/RepLayout.h"
#include "Engine/ActorChannel.h"
#include "Engine/VoiceChannel.h"
#include "GameFramework/GameNetworkManager.h"
#include "OnlineSubsystemUtils.h"
#include "NetworkingDistanceConstants.h"
#include "DataChannel.h"
#include "Engine/PackageMapClient.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameMode.h"

#if UE_SERVER
#include "PerfCountersModule.h"
#endif

// Default net driver stats
DEFINE_STAT(STAT_Ping);
DEFINE_STAT(STAT_Channels);
DEFINE_STAT(STAT_InRate);
DEFINE_STAT(STAT_OutRate);
DEFINE_STAT(STAT_InPackets);
DEFINE_STAT(STAT_OutPackets);
DEFINE_STAT(STAT_InBunches);
DEFINE_STAT(STAT_OutBunches);
DEFINE_STAT(STAT_OutLoss);
DEFINE_STAT(STAT_InLoss);
DEFINE_STAT(STAT_NumConsideredActors);
DEFINE_STAT(STAT_PrioritizedActors);
DEFINE_STAT(STAT_NumRelevantActors);
DEFINE_STAT(STAT_NumRelevantDeletedActors);
DEFINE_STAT(STAT_NumReplicatedActorAttempts);
DEFINE_STAT(STAT_NumReplicatedActors);
DEFINE_STAT(STAT_NumActorChannels);
DEFINE_STAT(STAT_NumActors);
DEFINE_STAT(STAT_NumNetActors);
DEFINE_STAT(STAT_NumDormantActors);
DEFINE_STAT(STAT_NumInitiallyDormantActors);
DEFINE_STAT(STAT_NumActorChannelsReadyDormant);
DEFINE_STAT(STAT_NumNetGUIDsAckd);
DEFINE_STAT(STAT_NumNetGUIDsPending);
DEFINE_STAT(STAT_NumNetGUIDsUnAckd);
DEFINE_STAT(STAT_ObjPathBytes);
DEFINE_STAT(STAT_NetGUIDInRate);
DEFINE_STAT(STAT_NetGUIDOutRate);
DEFINE_STAT(STAT_NetSaturated);

// Voice specific stats
DEFINE_STAT(STAT_VoiceBytesSent);
DEFINE_STAT(STAT_VoiceBytesRecv);
DEFINE_STAT(STAT_VoicePacketsSent);
DEFINE_STAT(STAT_VoicePacketsRecv);
DEFINE_STAT(STAT_PercentInVoice);
DEFINE_STAT(STAT_PercentOutVoice);

#if UE_BUILD_SHIPPING
#define DEBUG_REMOTEFUNCTION(Format, ...)
#else
#define DEBUG_REMOTEFUNCTION(Format, ...) UE_LOG(LogNet, VeryVerbose, Format, __VA_ARGS__);
#endif

// CVars
static TAutoConsoleVariable<int32> CVarSetNetDormancyEnabled(
	TEXT("net.DormancyEnable"),
	1,
	TEXT("Enables Network Dormancy System for reducing CPU and bandwidth overhead of infrequently updated actors\n")
	TEXT("1 Enables network dormancy. 0 disables network dormancy."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNetDormancyDraw(
	TEXT("net.DormancyDraw"),
	0,
	TEXT("Draws debug information for network dormancy\n")
	TEXT("1 Enables network dormancy debugging. 0 disables."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarNetDormancyDrawCullDistance(
	TEXT("net.DormancyDrawCullDistance"),
	5000.f,
	TEXT("Cull distance for net.DormancyDraw. World Units")
	TEXT("Max world units an actor can be away from the local view to draw its dormancy status"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNetDormancyValidate(
	TEXT("net.DormancyValidate"),
	0,
	TEXT("Validates that dormant actors do not change state while in a dormant state (on server only)")
	TEXT("0: Dont validate. 1: Validate on wake up. 2: Validate on each net update"),
	ECVF_Default);

/*-----------------------------------------------------------------------------
	UNetDriver implementation.
-----------------------------------------------------------------------------*/

UNetDriver::UNetDriver(const FObjectInitializer& ObjectInitializer)
:	UObject(ObjectInitializer)
,	MaxInternetClientRate(10000)
, 	MaxClientRate(15000)
,	ClientConnections()
,	Time( 0.f )
,	InBytes(0)
,	OutBytes(0)
,	NetGUIDOutBytes(0)
,	NetGUIDInBytes(0)
,	InPackets(0)
,	OutPackets(0)
,	InBunches(0)
,	OutBunches(0)
,	InPacketsLost(0)
,	OutPacketsLost(0)
,	InOutOfOrderPackets(0)
,	OutOutOfOrderPackets(0)
,	StatUpdateTime(0.0)
,	StatPeriod(1.f)
,	NetTag(0)
,	DebugRelevantActors(false)
,	ProcessQueuedBunchesCurrentFrameMilliseconds(0.0f)
{
}

void UNetDriver::PostInitProperties()
{
	Super::PostInitProperties();
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
#if DO_ENABLE_NET_TEST
		// read the settings from .ini and command line, with the command line taking precedence
		PacketSimulationSettings.LoadConfig();
		PacketSimulationSettings.RegisterCommands();
		PacketSimulationSettings.ParseSettings(FCommandLine::Get());
#endif
		RoleProperty		= FindObjectChecked<UProperty>( AActor::StaticClass(), TEXT("Role"      ) );
		RemoteRoleProperty	= FindObjectChecked<UProperty>( AActor::StaticClass(), TEXT("RemoteRole") );

		GuidCache			= TSharedPtr< FNetGUIDCache >( new FNetGUIDCache( this ) );
		NetCache			= TSharedPtr< FClassNetCacheMgr >( new FClassNetCacheMgr() );

		ProfileStats		= FParse::Param(FCommandLine::Get(),TEXT("profilestats"));
	}
	// By default we're the game net driver and any child ones must override this
	NetDriverName = NAME_GameNetDriver;
}

void UNetDriver::AssertValid()
{
}

void UNetDriver::TickFlush(float DeltaSeconds)
{
	if ( IsServer() && ClientConnections.Num() > 0 && ClientConnections[0]->InternalAck == false )
	{
		// Update all clients.
#if WITH_SERVER_CODE
		int32 Updated = ServerReplicateActors( DeltaSeconds );
		static int32 LastUpdateCount = 0;
		// Only log the zero replicated actors once after replicating an actor
		if ((LastUpdateCount && !Updated) || Updated)
		{
			UE_LOG(LogNetTraffic, Verbose, TEXT("%s replicated %d actors"), *GetDescription(), Updated);
		}
		LastUpdateCount = Updated;
#endif // WITH_SERVER_CODE
	}

	// Reset queued bunch amortization timer
	ProcessQueuedBunchesCurrentFrameMilliseconds = 0.0f;

#if STATS
	// Update network stats (only main game net driver for now)
	if (NetDriverName == NAME_GameNetDriver && 
		Time - StatUpdateTime > StatPeriod && ( ClientConnections.Num() > 0 || ServerConnection != NULL ))
	{
		int32 Ping = 0;
		int32 NumOpenChannels = 0;
		int32 NumActorChannels = 0;
		int32 NumDormantActors = 0;
		int32 NumActorChannelsReadyDormant = 0;
		int32 NumActors = 0;
		int32 AckCount = 0;
		int32 UnAckCount = 0;
		int32 PendingCount = 0;
		int32 NetSaturated = 0;

		if (FThreadStats::IsCollectingData())
		{
			float RealTime = Time - StatUpdateTime;

			// Use the elapsed time to keep things scaled to one measured unit
			InBytes = FMath::TruncToInt(InBytes / RealTime);
			OutBytes = FMath::TruncToInt(OutBytes / RealTime);

			NetGUIDOutBytes = FMath::TruncToInt(NetGUIDOutBytes / RealTime);
			NetGUIDInBytes = FMath::TruncToInt(NetGUIDInBytes / RealTime);

			// Save off for stats later

			InBytesPerSecond = InBytes;
			OutBytesPerSecond = OutBytes;

			InPackets = FMath::TruncToInt(InPackets / RealTime);
			OutPackets = FMath::TruncToInt(OutPackets / RealTime);
			InBunches = FMath::TruncToInt(InBunches / RealTime);
			OutBunches = FMath::TruncToInt(OutBunches / RealTime);
			OutPacketsLost = FMath::TruncToInt(100.f * OutPacketsLost / FMath::Max((float)OutPackets,1.f));
			InPacketsLost = FMath::TruncToInt(100.f * InPacketsLost / FMath::Max((float)InPackets + InPacketsLost,1.f));
			
			if (ServerConnection != NULL && ServerConnection->PlayerController != NULL && ServerConnection->PlayerController->PlayerState != NULL)
			{
				Ping = FMath::TruncToInt(ServerConnection->PlayerController->PlayerState->ExactPing);
			}

			if (ServerConnection != NULL)
			{
				NumOpenChannels = ServerConnection->OpenChannels.Num();
			}
			for (int32 i = 0; i < ClientConnections.Num(); i++)
			{
				NumOpenChannels += ClientConnections[i]->OpenChannels.Num();
			}

			// Use the elapsed time to keep things scaled to one measured unit
			VoicePacketsSent = FMath::TruncToInt(VoicePacketsSent / RealTime);
			VoicePacketsRecv = FMath::TruncToInt(VoicePacketsRecv / RealTime);
			VoiceBytesSent = FMath::TruncToInt(VoiceBytesSent / RealTime);
			VoiceBytesRecv = FMath::TruncToInt(VoiceBytesRecv / RealTime);

			// Determine voice percentages
			VoiceInPercent = (InBytes > 0) ? FMath::TruncToInt(100.f * (float)VoiceBytesRecv / (float)InBytes) : 0;
			VoiceOutPercent = (OutBytes > 0) ? FMath::TruncToInt(100.f * (float)VoiceBytesSent / (float)OutBytes) : 0;

			UNetConnection * Connection = (ServerConnection ? ServerConnection : (ClientConnections.Num() > 0 ? ClientConnections[0] : NULL));
			if (Connection)
			{
				NumActorChannels = Connection->ActorChannels.Num();
				NumDormantActors = Connection->DormantActors.Num();

				for (auto It = Connection->ActorChannels.CreateIterator(); It; ++It)
				{
					UActorChannel* Chan = It.Value();
					if(Chan && Chan->ReadyForDormancy(true))
					{
						NumActorChannelsReadyDormant++;
					}
				}

				if (World)
				{
					for (FActorIterator It(World); It; ++It)
					{
						NumActors++;
					}
				}

				Connection->PackageMap->GetNetGUIDStats(AckCount, UnAckCount, PendingCount);

				NetSaturated = Connection->IsNetReady(false) ? 0 : 1;
			}
		}

#if UE_SERVER
		IPerfCounters* PerfCounters = IPerfCountersModule::Get().GetPerformanceCounters();
		if (PerfCounters)
		{
			// Update total connections
			PerfCounters->Set(TEXT("NumConnections"), ClientConnections.Num());

			if (ClientConnections.Num() > 0)
			{
				// Update per connection statistics
				float MinPing = MAX_FLT;
				float AvgPing = 0;
				float MaxPing = -MAX_FLT;
				float PingCount = 0;

				for (int32 i = 0; i < ClientConnections.Num(); i++)
				{
					UNetConnection* Connection = ClientConnections[i];

					if (Connection != nullptr)
					{
						if (Connection->PlayerController != nullptr && Connection->PlayerController->PlayerState != nullptr)
						{
							// Ping value calculated per client
							float ConnPing = Connection->PlayerController->PlayerState->ExactPing;

							if (ConnPing < MinPing)
							{
								MinPing = ConnPing;
							}

							if (ConnPing > MaxPing)
							{
								MaxPing = ConnPing;
							}

							AvgPing += ConnPing;
							PingCount++;
						}
					}
				}

				PerfCounters->Set(TEXT("AvgPing"), AvgPing / PingCount);
				PerfCounters->Set(TEXT("MaxPing"), MaxPing);
				PerfCounters->Set(TEXT("MinPing"), MinPing);
			}
			else
			{
				PerfCounters->Set(TEXT("AvgPing"), 0.0f);
				PerfCounters->Set(TEXT("MaxPing"), 0);
				PerfCounters->Set(TEXT("MinPing"), 0);
			}
		}
#endif // UE_SERVER

		// Copy the net status values over
		SET_DWORD_STAT(STAT_Ping, Ping);
		SET_DWORD_STAT(STAT_Channels, NumOpenChannels);

		SET_DWORD_STAT(STAT_OutLoss, OutPacketsLost);
		SET_DWORD_STAT(STAT_InLoss, InPacketsLost);
		SET_DWORD_STAT(STAT_InRate, InBytes);
		SET_DWORD_STAT(STAT_OutRate, OutBytes);
		SET_DWORD_STAT(STAT_InPackets, InPackets);
		SET_DWORD_STAT(STAT_OutPackets, OutPackets);
		SET_DWORD_STAT(STAT_InBunches, InBunches);
		SET_DWORD_STAT(STAT_OutBunches, OutBunches);

		SET_DWORD_STAT(STAT_NetGUIDInRate, NetGUIDInBytes);
		SET_DWORD_STAT(STAT_NetGUIDOutRate, NetGUIDOutBytes);

		SET_DWORD_STAT(STAT_VoicePacketsSent, VoicePacketsSent);
		SET_DWORD_STAT(STAT_VoicePacketsRecv, VoicePacketsRecv);
		SET_DWORD_STAT(STAT_VoiceBytesSent, VoiceBytesSent);
		SET_DWORD_STAT(STAT_VoiceBytesRecv, VoiceBytesRecv);

		SET_DWORD_STAT(STAT_PercentInVoice, VoiceInPercent);
		SET_DWORD_STAT(STAT_PercentOutVoice, VoiceOutPercent);

		SET_DWORD_STAT(STAT_NumActorChannels, NumActorChannels);
		SET_DWORD_STAT(STAT_NumDormantActors, NumDormantActors);
		SET_DWORD_STAT(STAT_NumActors, NumActors);
		SET_DWORD_STAT(STAT_NumActorChannelsReadyDormant, NumActorChannelsReadyDormant);
		SET_DWORD_STAT(STAT_NumNetGUIDsAckd, AckCount);
		SET_DWORD_STAT(STAT_NumNetGUIDsPending, UnAckCount);
		SET_DWORD_STAT(STAT_NumNetGUIDsUnAckd, PendingCount);
		SET_DWORD_STAT(STAT_NetSaturated, NetSaturated);
		
		// Reset everything
		InBytes = 0;
		OutBytes = 0;
		NetGUIDOutBytes = 0;
		NetGUIDInBytes = 0;
		InPackets = 0;
		OutPackets = 0;
		InBunches = 0;
		OutBunches = 0;
		OutPacketsLost = 0;
		InPacketsLost = 0;
		VoicePacketsSent = 0;
		VoiceBytesSent = 0;
		VoicePacketsRecv = 0;
		VoiceBytesRecv = 0;
		VoiceInPercent = 0;
		VoiceOutPercent = 0;
		StatUpdateTime = Time;
	}
#endif // STATS

	// Poll all sockets.
	if( ServerConnection )
	{
		// Queue client voice packets in the server's voice channel
		ProcessLocalClientPackets();
		ServerConnection->Tick();
	}
	else
	{
		// Queue up any voice packets the server has locally
		ProcessLocalServerPackets();
	}

	for( int32 i=0; i<ClientConnections.Num(); i++ )
	{
		ClientConnections[i]->Tick();
	}

	if (CVarNetDormancyDraw.GetValueOnGameThread() > 0)
	{
		DrawNetDriverDebug();
	}

	// Update properties that are unmapped, try to hook up the object pointers if they exist now
	for ( auto It = UnmappedReplicators.CreateIterator(); It; ++It )
	{
		if ( !It->IsValid() )
		{
			// These are weak references, so if the object has been freed, we can stop checking
			It.RemoveCurrent();
			continue;
		}

		bool bHasMoreUnmapped = false;

		It->Pin().Get()->UpdateUnmappedObjects( bHasMoreUnmapped );
		
		if ( !bHasMoreUnmapped )
		{
			// If there are no more unmapped objects, we can also stop checking
			It.RemoveCurrent();
		}
	}
}

/**
 * Determines which other connections should receive the voice packet and
 * queues the packet for those connections. Used for sending both local/remote voice packets.
 *
 * @param VoicePacket the packet to be queued
 * @param CameFromConn the connection this packet came from (NULL if local)
 */
void UNetDriver::ReplicateVoicePacket(TSharedPtr<FVoicePacket> VoicePacket, UNetConnection* CameFromConn)
{
	// Iterate the connections and see if they want the packet
	for (int32 Index = 0; Index < ClientConnections.Num(); Index++)
	{
		UNetConnection* Conn = ClientConnections[Index];
		// Skip the originating connection
		if (CameFromConn != Conn)
		{
			// If server then determine if it should replicate the voice packet from another sender to this connection
			const bool bReplicateAsServer = !bIsPeer && Conn->ShouldReplicateVoicePacketFrom(*VoicePacket->GetSender());
			// If client peer then determine if it should send the voice packet to another client peer
			//const bool bReplicateAsPeer = (bIsPeer && AllowPeerVoice) && Conn->ShouldReplicateVoicePacketToPeer(Conn->PlayerId);

			if (bReplicateAsServer)// || bReplicateAsPeer)
			{
				UVoiceChannel* VoiceChannel = Conn->GetVoiceChannel();
				if (VoiceChannel != NULL)
				{
					// Add the voice packet for network sending
					VoiceChannel->AddVoicePacket(VoicePacket);
				}
			}
		}
	}
}

/**
 * Process any local talker packets that need to be sent to clients
 */
void UNetDriver::ProcessLocalServerPackets()
{
	if (World)
	{
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(World);
		if (VoiceInt.IsValid())
		{
			// Process all of the local packets
			for (int32 Index = 0; Index < VoiceInt->GetNumLocalTalkers(); Index++)
			{
				// Returns a ref counted copy of the local voice data or NULL if nothing to send
				TSharedPtr<FVoicePacket> LocalPacket = VoiceInt->GetLocalPacket(Index);
				// Check for something to send for this local talker
				if (LocalPacket.IsValid())
				{
					// See if anyone wants this packet
					ReplicateVoicePacket(LocalPacket, NULL);

					// once all local voice packets are processed then call ClearVoicePackets()
				}
			}
		}
	}
}

/**
 * Process any local talker packets that need to be sent to the server
 */
void UNetDriver::ProcessLocalClientPackets()
{
	if (World)
	{
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(World);
		if (VoiceInt.IsValid())
		{
			UVoiceChannel* VoiceChannel = ServerConnection->GetVoiceChannel();
			if (VoiceChannel)
			{
				// Process all of the local packets
				for (int32 Index = 0; Index < VoiceInt->GetNumLocalTalkers(); Index++)
				{
					// Returns a ref counted copy of the local voice data or NULL if nothing to send
					TSharedPtr<FVoicePacket> LocalPacket = VoiceInt->GetLocalPacket(Index);
					// Check for something to send for this local talker
					if (LocalPacket.IsValid())
					{
						// If there is a voice channel to the server, submit the packets
						//if (ShouldSendVoicePacketsToServer())
						{
							// Add the voice packet for network sending
							VoiceChannel->AddVoicePacket(LocalPacket);
						}

						// once all local voice packets are processed then call ClearLocalVoicePackets()
					}
				}
			}
		}
	}
}

void UNetDriver::PostTickFlush()
{
	if (World)
	{
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(World);
		if (VoiceInt.IsValid())
		{
			VoiceInt->ClearVoicePackets();
		}
	}
}


bool UNetDriver::InitConnectionClass(void)
{
	if (NetConnectionClass == NULL && NetConnectionClassName != TEXT(""))
	{
		NetConnectionClass = LoadClass<UNetConnection>(NULL,*NetConnectionClassName,NULL,LOAD_None,NULL);
		if (NetConnectionClass == NULL)
		{
			UE_LOG(LogNet, Error,TEXT("Failed to load class '%s'"),*NetConnectionClassName);
		}
	}
	return NetConnectionClass != NULL;
}

bool UNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	bool bSuccess = InitConnectionClass();
	Notify = InNotify;
	return bSuccess;
}

ENetMode UNetDriver::GetNetMode() const
{
	// Special case for PIE - forcing dedicated server behavior
#if WITH_EDITOR
	if (IsServer() && World && World->WorldType == EWorldType::PIE)
	{
		if ( GEngine->GetWorldContextFromWorldChecked(World).RunAsDedicated )
		{
			return NM_DedicatedServer;
		}
	}
#endif

	// Normal
	return (IsServer() ? (GIsClient ? NM_ListenServer : NM_DedicatedServer) : NM_Client);
}

void UNetDriver::RegisterTickEvents(class UWorld* InWorld)
{
	if (InWorld)
	{
		TickDispatchDelegateHandle  = InWorld->OnTickDispatch ().AddUObject(this, &UNetDriver::TickDispatch);
		TickFlushDelegateHandle     = InWorld->OnTickFlush    ().AddUObject(this, &UNetDriver::TickFlush);
		PostTickFlushDelegateHandle = InWorld->OnPostTickFlush().AddUObject(this, &UNetDriver::PostTickFlush);
	}
}

void UNetDriver::UnregisterTickEvents(class UWorld* InWorld)
{
	if (InWorld)
	{
		InWorld->OnTickDispatch ().Remove(TickDispatchDelegateHandle);
		InWorld->OnTickFlush    ().Remove(TickFlushDelegateHandle);
		InWorld->OnPostTickFlush().Remove(PostTickFlushDelegateHandle);
	}
}

/** Shutdown all connections managed by this net driver */
void UNetDriver::Shutdown()
{
	// Client closing connection to server
	if (ServerConnection)
	{
		// Calls Channel[0]->Close to send a close bunch to server
		ServerConnection->Close();
		ServerConnection->FlushNet();
	}

	// Server closing connections with clients
	if (ClientConnections.Num() > 0)
	{
		for (int32 ClientIndex = 0; ClientIndex < ClientConnections.Num(); ClientIndex++)
		{
			FString ErrorMsg = NSLOCTEXT("NetworkErrors", "HostClosedConnection", "Host closed the connection.").ToString();
			FNetControlMessage<NMT_Failure>::Send(ClientConnections[ClientIndex], ErrorMsg);
			ClientConnections[ClientIndex]->FlushNet(true);
		}

		for (int32 ClientIndex = ClientConnections.Num() - 1; ClientIndex >= 0; ClientIndex--)
		{
			if (ClientConnections[ClientIndex]->PlayerController)
			{
				APawn* Pawn = ClientConnections[ClientIndex]->PlayerController->GetPawn();
				if( Pawn )
				{
					Pawn->Destroy( true );
				}
			}

			// Calls Close() internally and removes from ClientConnections
			ClientConnections[ClientIndex]->CleanUp();
		}
	}

#if DO_ENABLE_NET_TEST
	PacketSimulationSettings.UnregisterCommands();
#endif
}

bool UNetDriver::IsServer() const
{
	// Client connections ALWAYS set the server connection object in InitConnect()
	// @todo ONLINE improve this with a bool
	return ServerConnection == NULL;
}

void UNetDriver::TickDispatch( float DeltaTime )
{
	SendCycles=RecvCycles=0;

	// Get new time.
	Time += DeltaTime;

	// Checks for standby cheats if enabled
	UpdateStandbyCheatStatus();

	// Delete any straggler connections.
	if( !ServerConnection )
	{
		for( int32 i=ClientConnections.Num()-1; i>=0; i-- )
		{
			if( ClientConnections[i]->State==USOCK_Closed )
			{
				ClientConnections[i]->CleanUp();
			}
		}
	}
}

bool UNetDriver::IsLevelInitializedForActor(const AActor* InActor, const UNetConnection* InConnection) const
{
	check(InActor);
	check(InConnection);
	check(World == InActor->GetWorld());

	// we can't create channels while the client is in the wrong world
	const bool bCorrectWorld = (InConnection->ClientWorldPackageName == World->GetOutermost()->GetFName() && InConnection->ClientHasInitializedLevelFor(InActor));
	// exception: Special case for PlayerControllers as they are required for the client to travel to the new world correctly			
	const bool bIsConnectionPC = (InActor == InConnection->PlayerController);
	return bCorrectWorld || bIsConnectionPC;
}

//
// Internal RPC calling.
//
void UNetDriver::InternalProcessRemoteFunction
	(
	AActor*			Actor,
	UObject*		SubObject,
	UNetConnection*	Connection,
	UFunction*		Function,
	void*			Parms,
	FOutParmRec*	OutParms,
	FFrame*			Stack,
	bool			IsServer
	)
{
	// get the top most function
	while( Function->GetSuperFunction() )
	{
		Function = Function->GetSuperFunction();
	}

	// If saturated and function is unimportant, skip it. Note unreliable multicasts are queued at the actor channel level so they are not gated here.
	if( !(Function->FunctionFlags & FUNC_NetReliable) && (!(Function->FunctionFlags & FUNC_NetMulticast)) && !Connection->IsNetReady(0) )
	{
		DEBUG_REMOTEFUNCTION(TEXT("Network saturated, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return;
	}

	// Route RPC calls to actual connection
	if (Connection->GetUChildConnection())
	{
		Connection = ((UChildConnection*)Connection)->Parent;
	}

	// If we have a subobject, thats who we are actually calling this on. If no subobject, we are calling on the actor.
	UObject* TargetObj = SubObject ? SubObject : Actor;

	// Make sure this function exists for both parties.
	const FClassNetCache* ClassCache = NetCache->GetClassNetCache( TargetObj->GetClass() );
	if (!ClassCache)
	{
		DEBUG_REMOTEFUNCTION(TEXT("ClassNetCache empty, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return;
	}
		
	const FFieldNetCache* FieldCache = ClassCache->GetFromField( Function );

	if ( !FieldCache )
	{
		DEBUG_REMOTEFUNCTION(TEXT("FieldCache empty, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return;
	}
		
	// Get the actor channel.
	UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
	if( !Ch )
	{
		if( IsServer )
		{
			if ( Actor->IsPendingKillPending() )
			{
				// Don't try opening a channel for me, I am in the process of being destroyed. Ignore my RPCs.
				return;
			}

			if (IsLevelInitializedForActor(Actor, Connection))
			{
				Ch = (UActorChannel *)Connection->CreateChannel( CHTYPE_Actor, 1 );
			}
			else
			{
				UE_LOG(LogNet, Verbose, TEXT("Can't send function '%s' on '%s': Client hasn't loaded the level for this Actor"), *Function->GetName(), *Actor->GetName());
				return;
			}
		}
		if (!Ch)
		{
			return;
		}
		if (IsServer)
		{
			Ch->SetChannelActor(Actor);
		}	
	}

	// Make sure initial channel-opening replication has taken place.
	if( Ch->OpenPacketId.First==INDEX_NONE )
	{
		if (!IsServer)
		{
			DEBUG_REMOTEFUNCTION(TEXT("Initial channel replication has not occurred, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
			return;
		}

		// triggering replication of an Actor while already in the middle of replication can result in invalid data being sent and is therefore illegal
		if (Ch->bIsReplicatingActor)
		{
			FString Error(FString::Printf(TEXT("Attempt to replicate function '%s' on Actor '%s' while it is in the middle of variable replication!"), *Function->GetName(), *Actor->GetName()));
			UE_LOG(LogScript, Error, TEXT("%s"), *Error);
			ensureMsg(false, *Error);
			return;
		}

		// Bump the ReplicationFrame value to invalidate any properties marked as "unchanged" for this frame.
		ReplicationFrame++;
		
		Ch->ReplicateActor();
	}

	// Form the RPC preamble.
	FOutBunch Bunch( Ch, 0 );

	// Reliability.
	//warning: RPC's might overflow, preventing reliable functions from getting thorough.
	if (Function->FunctionFlags & FUNC_NetReliable)
	{
		Bunch.bReliable = 1;
	}

	// Queue unreliable multicast 
	bool QueueBunch = (!Bunch.bReliable && Function->FunctionFlags & FUNC_NetMulticast);

	if (!QueueBunch)
	{
		Ch->BeginContentBlock(TargetObj, Bunch);
	}
	
	const int NumStartingHeaderBits = Bunch.GetNumBits();

	//UE_LOG(LogScript, Log, TEXT("   Call %s"),Function->GetFullName());
	if ( Connection->InternalAck )
	{
		uint32 Checksum = FieldCache->FieldChecksum;
		Bunch << Checksum;
	}
	else
	{
		check( FieldCache->FieldNetIndex <= ClassCache->GetMaxIndex() );
		Bunch.WriteIntWrapped(FieldCache->FieldNetIndex, ClassCache->GetMaxIndex()+1);
	}

	const int HeaderBits = Bunch.GetNumBits() - NumStartingHeaderBits;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Bunch.DebugString = FString::Printf(TEXT("%.2f RPC: %s - %s"), Connection->Driver->Time, *Actor->GetName(), *Function->GetName());
#endif

	TArray< UProperty * > LocalOutParms;

	if( Stack == nullptr )
	{
		// Look for CPF_OutParm's, we'll need to copy these into the local parameter memory manually
		// The receiving side will pull these back out when needed
		for ( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
		{
			if ( It->HasAnyPropertyFlags( CPF_OutParm ) )
			{
				if ( OutParms == NULL )
				{
					UE_LOG( LogNet, Warning, TEXT( "Missing OutParms. Property: %s, Function: %s, Actor: %s" ), *It->GetName(), *Function->GetName(), *Actor->GetName() );
					continue;
				}

				FOutParmRec * Out = OutParms;

				checkSlow( Out );

				while ( Out->Property != *It )
				{
					Out = Out->NextOutParm;
					checkSlow( Out );
				}

				void* Dest = It->ContainerPtrToValuePtr< void >( Parms );

				const int32 CopySize = It->ElementSize * It->ArrayDim;

				check( ( (uint8*)Dest - (uint8*)Parms ) + CopySize <= Function->ParmsSize );

				It->CopyCompleteValue( Dest, Out->PropAddr );

				LocalOutParms.Add( *It );
			}
		}
	}

	// verify we haven't overflowed unacked bunch buffer (Connection is not net ready)
	//@warning: needs to be after parameter evaluation for script stack integrity
	if (Bunch.IsError())
	{
		if (!Bunch.bReliable)
		{
			// Not reliable, so not fatal. This can happen a lot in debug builds at startup if client is slow to get in game
			UE_LOG(LogNet, Warning, TEXT("Can't send function '%s' on '%s': Reliable buffer overflow. FieldCache->FieldNetIndex: %d Max %d. Ch MaxPacket: %d"), *Function->GetName(), *Actor->GetName(), FieldCache->FieldNetIndex, ClassCache->GetMaxIndex(), Ch->Connection->MaxPacket);
		}
		else
		{
			// The connection has overflowed the reliable buffer. We cannot recover from this. Disconnect this user.
			UE_LOG(LogNet, Warning, TEXT("Closing connection. Can't send function '%s' on '%s': Reliable buffer overflow. FieldCache->FieldNetIndex: %d Max %d. Ch MaxPacket: %d."), *Function->GetName(), *Actor->GetName(), FieldCache->FieldNetIndex, ClassCache->GetMaxIndex(), Ch->Connection->MaxPacket);

			FString ErrorMsg = NSLOCTEXT("NetworkErrors", "ClientReliableBufferOverflow", "Outgoing reliable buffer overflow").ToString();
			FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
			Connection->FlushNet(true);
			Connection->Close();
		}
		return;
	}

	// Use the replication layout to send the rpc parameter values
	TSharedPtr<FRepLayout> RepLayout = GetFunctionRepLayout( Function );
	RepLayout->SendPropertiesForRPC( Actor, Function, Ch, Bunch, Parms );

	const int ParameterBits = Bunch.GetNumBits() - HeaderBits;

	// Destroy the memory used for the copied out parameters
	for ( int32 i = 0; i < LocalOutParms.Num(); i++ )
	{
		check( LocalOutParms[i]->HasAnyPropertyFlags( CPF_OutParm ) );
		LocalOutParms[i]->DestroyValue_InContainer( Parms );
	}

	// Write footer for content we just wrote
	if ( !QueueBunch )
	{
		Ch->EndContentBlock( TargetObj, Bunch );
	}

	const int FooterBits = Bunch.GetNumBits() - HeaderBits - ParameterBits;

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("net.RPC.Debug"));
	bool LogAsWarning = (CVar && CVar->GetValueOnGameThread() == 1);

	// Send the bunch.
	if( Bunch.IsError() )
	{
		UE_LOG(LogNet, Log, TEXT("Error: Can't send function '%s' on '%s': RPC bunch overflowed (too much data in parameters?)"), *Function->GetName(), *TargetObj->GetFullName());
		ensureMsgf(false,TEXT("Error: Can't send function '%s' on '%s': RPC bunch overflowed (too much data in parameters?)"), *Function->GetName(), *TargetObj->GetFullName());
	}
	else if (Ch->Closing)
	{
		UE_LOG(LogNetTraffic, Log, TEXT("RPC bunch on closing channel") );
	}
	else
	{
		// Make sure we're tracking all the bits in the bunch
		check(Bunch.GetNumBits() == HeaderBits + ParameterBits + FooterBits);

		if (QueueBunch)
		{
			// Unreliable multicast functions are queued and sent out during property replication
			if (LogAsWarning)
			{
				UE_LOG(LogNetTraffic, Warning,	TEXT("      Queing unreliable multicast RPC: %s::%s [%.1f bytes]"), *Actor->GetName(), *Function->GetName(), Bunch.GetNumBits() / 8.f );
			}
			else
			{
				UE_LOG(LogNetTraffic, Log,		TEXT("      Queing unreliable multicast RPC: %s::%s [%.1f bytes]"), *Actor->GetName(), *Function->GetName(), Bunch.GetNumBits() / 8.f );
			}

			NETWORK_PROFILER(GNetworkProfiler.TrackQueuedRPC(Connection, TargetObj, Actor, Function, HeaderBits, ParameterBits, FooterBits));
			Ch->QueueRemoteFunctionBunch(TargetObj, Function, Bunch);
		}
		else
		{
			if (LogAsWarning)
			{
				UE_LOG(LogNetTraffic, Warning,	TEXT("      Sent RPC: %s::%s [%.1f bytes]"), *Actor->GetName(), *Function->GetName(), Bunch.GetNumBits() / 8.f );
			}
			else
			{
				UE_LOG(LogNetTraffic, Log,		TEXT("      Sent RPC: %s::%s [%.1f bytes]"), *Actor->GetName(), *Function->GetName(), Bunch.GetNumBits() / 8.f );
			}

			NETWORK_PROFILER(GNetworkProfiler.TrackSendRPC(Actor, Function, HeaderBits, ParameterBits, FooterBits));
			Ch->SendBunch( &Bunch, 1 );
		}
	}
}

void UNetDriver::UpdateStandbyCheatStatus(void)
{
#if WITH_SERVER_CODE
	// Only the server needs to check
	if (ServerConnection == NULL && ClientConnections.Num())
	{
		// Only check for cheats if enabled and one wasn't previously detected
		if (bIsStandbyCheckingEnabled &&
			bHasStandbyCheatTriggered == false &&
			ClientConnections.Num() > 2)
		{
			int32 CountBadTx = 0;
			int32 CountBadRx = 0;
			int32 CountBadPing = 0;
			
			UWorld* FoundWorld = NULL;
			// Look at each connection checking for a receive time and an ack time
			for (int32 Index = 0; Index < ClientConnections.Num(); Index++)
			{
				UNetConnection* NetConn = ClientConnections[Index];
				// Don't check connections that aren't fully formed (still loading & no controller)
				// Controller won't be present until the join message is sent, which is after loading has completed
				if (NetConn)
				{
					APlayerController* PlayerController = NetConn->PlayerController;
					if(PlayerController)
					{
						if( PlayerController->GetWorld() && 
							PlayerController->GetWorld()->GetTimeSeconds() - PlayerController->CreationTime > JoinInProgressStandbyWaitTime &&
							// Ignore players with pending delete (kicked/timed out, but connection not closed)
							PlayerController->IsPendingKillPending() == false)
						{
							if (!FoundWorld)
							{
								FoundWorld = PlayerController->GetWorld();
							}
							else
							{
								check(FoundWorld == PlayerController->GetWorld());
							}
							if (Time - NetConn->LastReceiveTime > StandbyRxCheatTime)
							{
								CountBadRx++;
							}
							if (Time - NetConn->LastRecvAckTime > StandbyTxCheatTime)
							{
								CountBadTx++;
							}
							// Check for host tampering or crappy upstream bandwidth
							if (PlayerController->PlayerState &&
								PlayerController->PlayerState->Ping * 4 > BadPingThreshold)
							{
								CountBadPing++;
							}
						}
					}
				}
			}
			
			if (FoundWorld)
			{
				AGameMode* const GameMode = FoundWorld->GetAuthGameMode();
				AGameNetworkManager* const NetworkManager = FoundWorld->NetworkManager;
				if (NetworkManager)
				{
					// See if we hit the percentage required for either TX or RX standby detection
					if (float(CountBadRx) / float(ClientConnections.Num()) > PercentMissingForRxStandby)
					{
						bHasStandbyCheatTriggered = true;
						// Send to the GameMode for processing
						NetworkManager->StandbyCheatDetected(STDBY_Rx);
					}
					else if (float(CountBadPing) / float(ClientConnections.Num()) > PercentForBadPing)
					{
						bHasStandbyCheatTriggered = true;
						// Send to the GameMode for processing
						NetworkManager->StandbyCheatDetected(STDBY_BadPing);
					}
					// Check for the host not sending to the clients, but only during a match
					else if ( GameMode && GameMode->IsMatchInProgress() &&
						float(CountBadTx) / float(ClientConnections.Num()) > PercentMissingForTxStandby)
					{
						bHasStandbyCheatTriggered = true;
						// Send to the GameMode for processing
						NetworkManager->StandbyCheatDetected(STDBY_Tx);
					}
				}
			}
		}
	}
#endif
}

void UNetDriver::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Prevent referenced objects from being garbage collected.
	Ar << ClientConnections << ServerConnection << RoleProperty << RemoteRoleProperty;

	if (Ar.IsCountingMemory())
	{
		ClientConnections.CountBytes(Ar);
	}
}
void UNetDriver::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Destroy server connection.
		if( ServerConnection )
		{
			ServerConnection->CleanUp();
		}
		// Destroy client connections.
		while( ClientConnections.Num() )
		{
			UNetConnection* ClientConnection = ClientConnections[0];
			ClientConnection->CleanUp();
		}
		// Low level destroy.
		LowLevelDestroy();

		// Delete the guid cache
		GuidCache.Reset();
	}
	else
	{
		check(ServerConnection==NULL);
		check(ClientConnections.Num()==0);
		check(!GuidCache.IsValid());
	}
	Super::FinishDestroy();
}

void UNetDriver::LowLevelDestroy()
{
	// We are closing down all our sockets and low level communications.
	// Sever the link with UWorld to ensure we don't tick again
	SetWorld(NULL);
}


#if !UE_BUILD_SHIPPING
bool UNetDriver::HandleSocketsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Print list of open connections.
	Ar.Logf( TEXT("%s Connections:"), *GetDescription() );
	if( ServerConnection )
	{
		Ar.Logf( TEXT("   Server %s"), *ServerConnection->LowLevelDescribe() );
		for( int32 i=0; i<ServerConnection->OpenChannels.Num(); i++ )
		{
			Ar.Logf( TEXT("      Channel %i: %s"), ServerConnection->OpenChannels[i]->ChIndex, *ServerConnection->OpenChannels[i]->Describe() );
		}
	}
#if WITH_SERVER_CODE
	for( int32 i=0; i<ClientConnections.Num(); i++ )
	{
		UNetConnection* Connection = ClientConnections[i];
		Ar.Logf( TEXT("   Client %s"), *Connection->LowLevelDescribe() );
		for( int32 j=0; j<Connection->OpenChannels.Num(); j++ )
		{
			Ar.Logf( TEXT("      Channel %i: %s"), Connection->OpenChannels[j]->ChIndex, *Connection->OpenChannels[j]->Describe() );
		}
	}
#endif
	return true;
}

bool UNetDriver::HandlePackageMapCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Print packagemap for open connections
	Ar.Logf(TEXT("Package Map:"));
	if (ServerConnection != NULL)
	{
		Ar.Logf(TEXT("   Server %s"), *ServerConnection->LowLevelDescribe());
		ServerConnection->PackageMap->LogDebugInfo(Ar);
	}
#if WITH_SERVER_CODE
	for (int32 i = 0; i < ClientConnections.Num(); i++)
	{
		UNetConnection* Connection = ClientConnections[i];
		Ar.Logf( TEXT("   Client %s"), *Connection->LowLevelDescribe() );
		Connection->PackageMap->LogDebugInfo(Ar);
	}
#endif
	return true;
}

bool UNetDriver::HandleNetFloodCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	UNetConnection* TestConn = NULL;
	if (ServerConnection != NULL)
	{
		TestConn = ServerConnection;
	}
#if WITH_SERVER_CODE
	else if (ClientConnections.Num() > 0)
	{
		TestConn = ClientConnections[0];
	}
#endif
	if (TestConn != NULL)
	{
		Ar.Logf(TEXT("Flooding connection 0 with control messages"));

		for (int32 i = 0; i < 256 && TestConn->State == USOCK_Open; i++)
		{
			FNetControlMessage<NMT_Netspeed>::Send(TestConn, TestConn->CurrentNetSpeed);
			TestConn->FlushNet();
		}
	}
	return true;
}

bool UNetDriver::HandleNetDebugTextCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Send a text string for testing connection
	FString TestStr = FParse::Token(Cmd,false);
	if (ServerConnection != NULL)
	{
		UE_LOG(LogNet, Log, TEXT("%s sending NMT_DebugText [%s] to [%s]"), 
			*GetDescription(),*TestStr, *ServerConnection->LowLevelDescribe());

		FNetControlMessage<NMT_DebugText>::Send(ServerConnection,TestStr);
		ServerConnection->FlushNet(true);
	}
#if WITH_SERVER_CODE
	else
	{
		for (int32 ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
		{
			UNetConnection* Connection = ClientConnections[ClientIdx];
			if (Connection)
			{
				UE_LOG(LogNet, Log, TEXT("%s sending NMT_DebugText [%s] to [%s]"), 
					*GetDescription(),*TestStr, *Connection->LowLevelDescribe());

				FNetControlMessage<NMT_DebugText>::Send(Connection,TestStr);
				Connection->FlushNet(true);
			}
		}
	}
#endif // WITH_SERVER_CODE
	return true;
}

bool UNetDriver::HandleNetDisconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString Msg = NSLOCTEXT("NetworkErrors", "NETDISCONNECTMSG", "NETDISCONNECT MSG").ToString();
	if (ServerConnection != NULL)
	{
		UE_LOG(LogNet, Log, TEXT("%s disconnecting connection from host [%s]"), 
			*GetDescription(),*ServerConnection->LowLevelDescribe());

		FNetControlMessage<NMT_Failure>::Send(ServerConnection, Msg);
	}
#if WITH_SERVER_CODE
	else
	{
		for (int32 ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
		{
			UNetConnection* Connection = ClientConnections[ClientIdx];
			if (Connection)
			{
				UE_LOG(LogNet, Log, TEXT("%s disconnecting from client [%s]"), 
					*GetDescription(),*Connection->LowLevelDescribe());

				FNetControlMessage<NMT_Failure>::Send(Connection, Msg);
				Connection->FlushNet(true);
			}
		}
	}
#endif
	return true;
}

bool UNetDriver::HandleNetDumpServerRPCCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if WITH_SERVER_CODE
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		bool bHasNetFields = false;

		for ( int32 i = 0; i < ClassIt->NetFields.Num(); i++ )
		{
			UFunction * Function = Cast<UFunction>( ClassIt->NetFields[i] );

			if ( Function != NULL && Function->FunctionFlags & FUNC_NetServer )
			{
				bHasNetFields = true;
				break;
			}
		}

		if ( !bHasNetFields )
		{
			continue;
		}

		Ar.Logf( TEXT( "Class: %s" ), *ClassIt->GetName() );

		for ( int32 i = 0; i < ClassIt->NetFields.Num(); i++ )
		{
			UFunction * Function = Cast<UFunction>( ClassIt->NetFields[i] );

			if ( Function != NULL && Function->FunctionFlags & FUNC_NetServer )
			{
				const FClassNetCache * ClassCache = NetCache->GetClassNetCache( *ClassIt );

				const FFieldNetCache * FieldCache = ClassCache->GetFromField( Function );

				TArray< UProperty * > Parms;

				for ( TFieldIterator<UProperty> It( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
				{
					Parms.Add( *It );
				}

				if ( Parms.Num() == 0 )
				{
					Ar.Logf( TEXT( "    [0x%03x] %s();" ), FieldCache->FieldNetIndex, *Function->GetName() );
					continue;
				}

				FString ParmString;

				for ( int32 j = 0; j < Parms.Num(); j++ )
				{
					if ( Cast<UStructProperty>( Parms[j] ) )
					{
						ParmString += Cast<UStructProperty>( Parms[j] )->Struct->GetName();
					}
					else
					{
						ParmString += Parms[j]->GetClass()->GetName();
					}

					ParmString += TEXT( " " );

					ParmString += Parms[j]->GetName();

					if ( j < Parms.Num() - 1 )
					{
						ParmString += TEXT( ", " );
					}
				}						

				Ar.Logf( TEXT( "    [0x%03x] %s( %s );" ), FieldCache->FieldNetIndex, *Function->GetName(), *ParmString );
			}
		}
	}
#endif
	return true;
}

#endif // !UE_BUILD_SHIPPING

bool UNetDriver::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !UE_BUILD_SHIPPING
	if( FParse::Command(&Cmd,TEXT("SOCKETS")) )
	{
		return HandleSocketsCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("PACKAGEMAP")))
	{
		return HandlePackageMapCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("NETFLOOD")))
	{
		return HandleNetFloodCommand( Cmd, Ar );
	}
#if DO_ENABLE_NET_TEST
	// This will allow changing the Pkt* options at runtime
	else if (PacketSimulationSettings.ParseSettings(Cmd))
	{
		if (ServerConnection)
		{
			// Notify the server connection of the change
			ServerConnection->UpdatePacketSimulationSettings();
		}
#if WITH_SERVER_CODE
		else
		{
			// Notify all client connections that the settings have changed
			for (int32 Index = 0; Index < ClientConnections.Num(); Index++)
			{
				ClientConnections[Index]->UpdatePacketSimulationSettings();
			}
		}		
#endif
		return true;
	}
#endif
	else if (FParse::Command(&Cmd, TEXT("NETDEBUGTEXT")))
	{
		return HandleNetDebugTextCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("NETDISCONNECT")))
	{
		return HandleNetDisconnectCommand( Cmd, Ar );	
	}
	else if (FParse::Command(&Cmd, TEXT("DUMPSERVERRPC")))
	{
		return HandleNetDumpServerRPCCommand( Cmd, Ar );	
	}
	else
#endif // !UE_BUILD_SHIPPING
	{
		return false;
	}
}

FActorDestructionInfo *	CreateDestructionInfo( UNetDriver * NetDriver, AActor* ThisActor, FActorDestructionInfo *DestructionInfo )
{
	if (DestructionInfo)
		return DestructionInfo;

	FNetworkGUID NetGUID = NetDriver->GuidCache->GetOrAssignNetGUID( ThisActor );

	FActorDestructionInfo &NewInfo = NetDriver->DestroyedStartupOrDormantActors.FindOrAdd( NetGUID );
	NewInfo.DestroyedPosition = ThisActor->GetActorLocation();
	NewInfo.NetGUID = NetGUID;
	NewInfo.ObjOuter = ThisActor->GetOuter();
	NewInfo.PathName = ThisActor->GetName();

	// get the level for the object
	const ULevel* Level = NULL;
	for (const UObject* Obj = ThisActor; Obj != NULL; Obj = Obj->GetOuter())
	{
		Level = Cast<const ULevel>(Obj);
		if (Level != NULL)
		{
			break;
		}
	}

	if (Level && !Level->IsPersistentLevel() )
	{
		NewInfo.StreamingLevelName = Level->GetOutermost()->GetFName();
	}
	else
	{
		NewInfo.StreamingLevelName = NAME_None;
	}

	return &NewInfo;
}

void UNetDriver::NotifyActorDestroyed( AActor* ThisActor, bool IsSeamlessTravel )
{
	// Remove the actor from the property tracker map
	RepChangedPropertyTrackerMap.Remove(ThisActor);
#if WITH_SERVER_CODE

	FActorDestructionInfo* DestructionInfo = NULL;
	const bool bIsServer = ServerConnection == NULL;
	if (bIsServer)
	{
		const bool bIsActorStatic = !GuidCache->IsDynamicObject( ThisActor );
		const bool bActorHasRole = ThisActor->GetRemoteRole() != ROLE_None;
		const bool ShouldCreateDestructionInfo = bIsServer && bIsActorStatic && bActorHasRole && !IsSeamlessTravel;

		if(ShouldCreateDestructionInfo)
		{
			UE_LOG(LogNet, VeryVerbose, TEXT("NotifyActorDestroyed %s - StartupActor"), *ThisActor->GetPathName() );
			DestructionInfo = CreateDestructionInfo( this, ThisActor, DestructionInfo);
		}

		for( int32 i=ClientConnections.Num()-1; i>=0; i-- )
		{
			UNetConnection* Connection = ClientConnections[i];
			if( ThisActor->bNetTemporary )
				Connection->SentTemporaries.Remove( ThisActor );
			UActorChannel* Channel = Connection->ActorChannels.FindRef(ThisActor);
			if( Channel )
			{
				check(Channel->OpenedLocally);
				Channel->bClearRecentActorRefs = false;
				Channel->Close();
			}
			else if(ShouldCreateDestructionInfo || Connection->DormantActors.Contains(ThisActor) || Connection->RecentlyDormantActors.Contains(ThisActor))
			{
				// Make a new destruction info if necessary. It is necessary if the actor is dormant or recently dormant because
				// even though the client knew about the actor at some point, it doesn't have a channel to handle destruction.
				DestructionInfo = CreateDestructionInfo(this, ThisActor, DestructionInfo);
				Connection->DestroyedStartupOrDormantActors.Add(DestructionInfo->NetGUID);
			}

			// This should be rare, but remove from OwnedConsiderList
			for (int32 ActorIdx=0; ActorIdx < Connection->OwnedConsiderList.Num(); ++ActorIdx)
			{
				if (Connection->OwnedConsiderList[ActorIdx] == ThisActor)
				{
					// Swap with last element, shrink list size by 1
					Connection->OwnedConsiderList.RemoveAtSwap(ActorIdx, 1, false);
					break;
				}
			}

			// Remove it from any dormancy lists				
			Connection->RecentlyDormantActors.Remove( ThisActor );
			Connection->DormantReplicatorMap.Remove( ThisActor );
			Connection->DormantActors.Remove( ThisActor );
		}
	}
#endif // WITH_SERVER_CODE
}

void UNetDriver::NotifyStreamingLevelUnload( ULevel* Level)
{
	if (ServerConnection && ServerConnection->PackageMap)
	{
		UE_LOG(LogNet, Log, TEXT("NotifyStreamingLevelUnload: %s"), *Level->GetName() );

		if (Level->LevelScriptActor)
		{
			UActorChannel * Channel = ServerConnection->ActorChannels.FindRef((AActor*)Level->LevelScriptActor);
			if (Channel)
			{
				UE_LOG(LogNet, Log, TEXT("NotifyStreamingLevelUnload: BREAKING"));

				Channel->Actor = NULL;
				Channel->Broken = true;
				Channel->CleanupReplicators();
			}
		}

		ServerConnection->PackageMap->NotifyStreamingLevelUnload(Level);
	}

	for( int32 i=ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = ClientConnections[i];
		if (Connection && Connection->PackageMap)
		{
			Connection->PackageMap->NotifyStreamingLevelUnload(Level);
		}
	}
}

/** Called when an actor is being unloaded during a seamless travel or do due level streaming 
 *  The main point is that it calls the normal NotifyActorDestroyed to destroy the channel on the server
 *	but also removes the Actor reference, sets broken flag, and cleans up actor class references on clients.
 */
void UNetDriver::NotifyActorLevelUnloaded( AActor* TheActor )
{
	// server
	NotifyActorDestroyed(TheActor, true);
	// client
	if (ServerConnection != NULL)
	{
		// we can't kill the channel until the server says so, so just clear the actor ref and break the channel
		UActorChannel* Channel = ServerConnection->ActorChannels.FindRef(TheActor);
		if (Channel != NULL)
		{
			ServerConnection->ActorChannels.Remove(TheActor);
			Channel->Actor = NULL;
			Channel->Broken = true;
			Channel->CleanupReplicators();
		}
	}
}

/** UNetDriver::FlushActorDormancy(AActor* Actor)
 *	 Flushes the actor from the NetDriver's dormant list and/or cancels pending dormancy on the actor channel.
 *
 *	 This does not change the Actor's actual NetDormant state. If a dormant actor is Flushed, it will net update at least one more
 *	 time, and then go back to dormant.
 */
void UNetDriver::FlushActorDormancy(AActor* Actor)
{
	// Note: Going into dormancy is completely handled in ServerReplicateActor. We want to avoid
	// event-based handling of going into dormancy, because we have to deal with connections joining in progress.
	// It is better to have ::ServerReplicateActor check the AActor and UChannel's states to determined if an actor
	// needs to be moved into dormancy. The same amount of work will be done (1 time per connection when an actor goes dorm)
	// and we avoid having to do special things when a new client joins.
	//
	// Going out of dormancy can be event based like this since it only affects clients already joined. Its more efficient in this
	// way too, since we dont have to check every dormant actor in ::ServerReplicateActor to see if it needs to go out of dormancy

#if WITH_SERVER_CODE
	if (CVarSetNetDormancyEnabled.GetValueOnGameThread() == 0)
		return;

	check(Actor);
	check(ServerConnection == NULL);

	// Go through each connection and remove the actor from the dormancy list
	for (int32 i=0; i < ClientConnections.Num(); ++i)
	{
		UNetConnection *NetConnection = ClientConnections[i];
		if(NetConnection != NULL)
		{
			NetConnection->FlushDormancy(Actor);
		}
	}
#endif // WITH_SERVER_CODE
}

UChildConnection* UNetDriver::CreateChild(UNetConnection* Parent)
{
	UE_LOG(LogNet, Log, TEXT("Creating child connection with %s parent"), *Parent->GetName());
	auto Child = NewObject<UChildConnection>();
	Child->Driver = this;
	Child->URL = FURL();
	Child->State = Parent->State;
	Child->URL.Host = Parent->URL.Host;
	Child->Parent = Parent;
	Child->PackageMap = Parent->PackageMap;
	Child->CurrentNetSpeed = Parent->CurrentNetSpeed;
	Parent->Children.Add(Child);
	return Child;
}

void UNetDriver::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UNetDriver* This = CastChecked<UNetDriver>(InThis);
	Super::AddReferencedObjects(This, Collector);
}

#if DO_ENABLE_NET_TEST

class FPacketSimulationConsoleCommandVisitor 
{
public:
	static void OnPacketSimulationConsoleCommand(const TCHAR *Name, IConsoleObject* CVar, TArray<IConsoleObject*>& Sink)
	{
		Sink.Add(CVar);
	}
};

/** reads in settings from the .ini file 
 * @note: overwrites all previous settings
 */
void FPacketSimulationSettings::LoadConfig()
{
	if (GConfig->GetInt(TEXT("PacketSimulationSettings"), TEXT("PktLoss"), PktLoss, GEngineIni))
	{
		PktLoss = FMath::Clamp<int32>(PktLoss, 0, 100);
	}
	
	bool InPktOrder = !!PktOrder;
	GConfig->GetBool(TEXT("PacketSimulationSettings"), TEXT("PktOrder"), InPktOrder, GEngineIni);
	PktOrder = int32(InPktOrder);
	
	GConfig->GetInt(TEXT("PacketSimulationSettings"), TEXT("PktLag"), PktLag, GEngineIni);
	
	if (GConfig->GetInt(TEXT("PacketSimulationSettings"), TEXT("PktDup"), PktDup, GEngineIni))
	{
		PktDup = FMath::Clamp<int32>(PktDup, 0, 100);
	}

	if (GConfig->GetInt(TEXT("PacketSimulationSettings"), TEXT("PktLagVariance"), PktLagVariance, GEngineIni))
	{
		PktLagVariance = FMath::Clamp<int32>(PktLagVariance, 0, 100);
	}
}

void FPacketSimulationSettings::RegisterCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	
	// Register exec commands with the console manager for auto-completion if they havent been registered already by another net driver
	if (!ConsoleManager.IsNameRegistered(TEXT("Net PktLoss=")))
	{
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktLoss="), TEXT("PktLoss=<n> (simulates network packet loss)"));
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktOrder="), TEXT("PktOrder=<n> (simulates network packet received out of order)"));
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktDup="), TEXT("PktDup=<n> (simulates sending/receiving duplicate network packets)"));
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktLag="), TEXT("PktLag=<n> (simulates network packet lag)"));
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktLagVariance="), TEXT("PktLagVariance=<n> (simulates variable network packet lag)"));
	}
}


void FPacketSimulationSettings::UnregisterCommands()
{
	// Never unregister the console commands. Since net drivers come and go, and we can sometimes have more than 1, etc. 
	// We could do better bookkeeping for this, but its not worth it right now. Just ensure the commands are always there for tab completion.
#if 0
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	// Gather up all relevant console commands
	TArray<IConsoleObject*> PacketSimulationConsoleCommands;
	ConsoleManager.ForEachConsoleObject(
		FConsoleObjectVisitor::CreateStatic< TArray<IConsoleObject*>& >(
		&FPacketSimulationConsoleCommandVisitor::OnPacketSimulationConsoleCommand,
		PacketSimulationConsoleCommands ), TEXT("Net Pkt"));

	// Unregister them from the console manager
	for(int32 i = 0; i < PacketSimulationConsoleCommands.Num(); ++i)
	{
		ConsoleManager.UnregisterConsoleObject(PacketSimulationConsoleCommands[i]);
	}
#endif
}

/**
 * Reads the settings from a string: command line or an exec
 *
 * @param Stream the string to read the settings from
 */
bool FPacketSimulationSettings::ParseSettings(const TCHAR* Cmd)
{
	// note that each setting is tested.
	// this is because the same function will be used to parse the command line as well
	bool bParsed = false;

	if( FParse::Value(Cmd,TEXT("PktLoss="), PktLoss) )
	{
		bParsed = true;
		FMath::Clamp<int32>( PktLoss, 0, 100 );
		UE_LOG(LogNet, Log, TEXT("PktLoss set to %d"), PktLoss);
	}
	if( FParse::Value(Cmd,TEXT("PktOrder="), PktOrder) )
	{
		bParsed = true;
		FMath::Clamp<int32>( PktOrder, 0, 1 );
		UE_LOG(LogNet, Log, TEXT("PktOrder set to %d"), PktOrder);
	}
	if( FParse::Value(Cmd,TEXT("PktLag="), PktLag) )
	{
		bParsed = true;
		UE_LOG(LogNet, Log, TEXT("PktLag set to %d"), PktLag);
	}
	if( FParse::Value(Cmd,TEXT("PktDup="), PktDup) )
	{
		bParsed = true;
		FMath::Clamp<int32>( PktDup, 0, 100 );
		UE_LOG(LogNet, Log, TEXT("PktDup set to %d"), PktDup);
	}
	if( FParse::Value(Cmd,TEXT("PktLagVariance="), PktLagVariance) )
	{
		bParsed = true;
		FMath::Clamp<int32>( PktLagVariance, 0, 100 );
		UE_LOG(LogNet, Log, TEXT("PktLagVariance set to %d"), PktLagVariance);
	}
	return bParsed;
}

#endif

FNetViewer::FNetViewer(UNetConnection* InConnection, float DeltaSeconds) :
	InViewer(InConnection->PlayerController ? InConnection->PlayerController : InConnection->OwningActor),
	ViewTarget(InConnection->ViewTarget),
	ViewLocation(ForceInit),
	ViewDir(ForceInit)
{
	check(InConnection->OwningActor);
	check(!InConnection->PlayerController || (InConnection->PlayerController == InConnection->OwningActor));

	APlayerController* ViewingController = InConnection->PlayerController;

	// Get viewer coordinates.
	ViewLocation = ViewTarget->GetActorLocation();
	if (ViewingController)
	{
		FRotator ViewRotation = ViewingController->GetControlRotation();
		ViewingController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		ViewDir = ViewRotation.Vector();
	}

	// Compute ahead-vectors for prediction.
	FVector Ahead = FVector::ZeroVector;
	if (InConnection->TickCount & 1)
	{
		float PredictSeconds = (InConnection->TickCount & 2) ? 0.4f : 0.9f;
		Ahead = PredictSeconds * ViewTarget->GetVelocity();
		APawn* ViewerPawn = Cast<APawn>(ViewTarget);
		if( ViewerPawn && ViewerPawn->GetMovementBase() && ViewerPawn->GetMovementBase()->GetOwner() )
		{
			Ahead += PredictSeconds * ViewerPawn->GetMovementBase()->GetOwner()->GetVelocity();
		}
		if (!Ahead.IsZero())
		{
			FHitResult Hit(1.0f);
			Hit.Location = ViewLocation + Ahead;

			static FName NAME_ServerForwardView = FName(TEXT("ServerForwardView"));
			UWorld* World = NULL;
			if( InConnection->PlayerController )
			{
				World = InConnection->PlayerController->GetWorld();
			}
			else
			{
				World = ViewerPawn->GetWorld();
			}
			check( World );
			World->LineTraceSingleByObjectType(Hit, ViewLocation, Hit.Location, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(NAME_ServerForwardView, true, ViewTarget));
			ViewLocation = Hit.Location;
		}
	}
}

FActorPriority::FActorPriority(UNetConnection* InConnection, UActorChannel* InChannel, AActor* InActor, const TArray<struct FNetViewer>& Viewers, bool bLowBandwidth)
	: Actor(InActor), Channel(InChannel), DestructionInfo(NULL)
{	
	float Time  = Channel ? (InConnection->Driver->Time - Channel->LastUpdateTime) : InConnection->Driver->SpawnPrioritySeconds;
	// take the highest priority of the viewers on this connection
	Priority = 0;
	for (int32 i = 0; i < Viewers.Num(); i++)
	{
		Priority = FMath::Max<int32>(Priority, FMath::RoundToInt(65536.0f * Actor->GetNetPriority(Viewers[i].ViewLocation, Viewers[i].ViewDir, Viewers[i].InViewer, Viewers[i].ViewTarget, InChannel, Time, bLowBandwidth)));
	}
}

FActorPriority::FActorPriority(class UNetConnection* InConnection, struct FActorDestructionInfo * Info, const TArray<struct FNetViewer>& Viewers )
	: Actor(NULL), Channel(NULL), DestructionInfo(Info)
{
	
	Priority = 0;

	for (int32 i = 0; i < Viewers.Num(); i++)
	{
		float Time  = InConnection->Driver->SpawnPrioritySeconds;

		FVector Dir = DestructionInfo->DestroyedPosition - Viewers[i].ViewLocation;
		float DistSq = Dir.SizeSquared();
		
		// adjust priority based on distance and whether actor is in front of viewer
		if ( (Viewers[i].ViewDir | Dir) < 0.f )
		{
			if ( DistSq > NEARSIGHTTHRESHOLDSQUARED )
				Time *= 0.2f;
			else if ( DistSq > CLOSEPROXIMITYSQUARED )
				Time *= 0.4f;
		}
		else if ( DistSq > MEDSIGHTTHRESHOLDSQUARED )
			Time *= 0.4f;

		Priority = FMath::Max<int32>(Priority, 65536.0f * Time);
	}
}

int32 UNetDriver::ServerReplicateActors(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NetServerRepActorsTime);

#if WITH_SERVER_CODE
	if ( ClientConnections.Num() == 0 )
	{
		return 0;
	}

	int32 Updated = 0;

	// Bump the ReplicationFrame value to invalidate any properties marked as "unchanged" for this frame.
	ReplicationFrame++;

	int32 NumClientsToTick = ClientConnections.Num();

	// by default only throttle update for listen servers unless specified on the commandline
	static bool bForceClientTickingThrottle = FParse::Param(FCommandLine::Get(),TEXT("limitclientticks"));
	if (bForceClientTickingThrottle || GetNetMode() == NM_ListenServer)
	{
		// determine how many clients to tick this frame based on GEngine->NetTickRate (always tick at least one client), double for lan play
		static float DeltaTimeOverflow = 0.f;
		// updates are doubled for lan play
		static bool LanPlay = FParse::Param(FCommandLine::Get(),TEXT("lanplay"));
		//@todo - ideally we wouldn't want to tick more clients with a higher deltatime as that's not going to be good for performance and probably saturate bandwidth in hitchy situations, maybe 
		// come up with a solution that is greedier with higher framerates, but still won't risk saturating server upstream bandwidth
		float ClientUpdatesThisFrame = GEngine->NetClientTicksPerSecond * (DeltaSeconds + DeltaTimeOverflow) * (LanPlay ? 2.f : 1.f);
		NumClientsToTick = FMath::Min<int32>(NumClientsToTick,FMath::TruncToInt(ClientUpdatesThisFrame));
		//UE_LOG(LogNet, Log, TEXT("%2.3f: Ticking %d clients this frame, %2.3f/%2.4f"),GetWorld()->GetTimeSeconds(),NumClientsToTick,DeltaSeconds,ClientUpdatesThisFrame);
		if (NumClientsToTick == 0)
		{
			// if no clients are ticked this frame accumulate the time elapsed for the next frame
			DeltaTimeOverflow += DeltaSeconds;
			return 0;
		}
		DeltaTimeOverflow = 0.f;
	}

	bool bNetRelevantActorCount = false;
	int32 NetRelevantActorCount = 0;

	check( World );

	FMemMark Mark(FMemStack::Get());
	// initialize connections
	bool bFoundReadyConnection = false; // whether we have at least one connection ready for property replication
	for( int32 ConnIdx = 0; ConnIdx < ClientConnections.Num(); ConnIdx++ )
	{
		UNetConnection* Connection = ClientConnections[ConnIdx];
		check(Connection);
		check(Connection->State == USOCK_Pending || Connection->State == USOCK_Open || Connection->State == USOCK_Closed);
		checkSlow(Connection->GetUChildConnection() == NULL);

		// Handle not ready channels.
		//@note: we cannot add Connection->IsNetReady(0) here to check for saturation, as if that's the case we still want to figure out the list of relevant actors
		//			to reset their NetUpdateTime so that they will get sent as soon as the connection is no longer saturated
		AActor* OwningActor = Connection->OwningActor;
		if (OwningActor != NULL && Connection->State == USOCK_Open && (Connection->Driver->Time - Connection->LastReceiveTime < 1.5f))
		{
			check( World == OwningActor->GetWorld() );
			if( !bNetRelevantActorCount )
			{
				NetRelevantActorCount = World->GetNetRelevantActorCount() + 2;
				bNetRelevantActorCount = true;
			}
			bFoundReadyConnection = true;
			
			// the view target is what the player controller is looking at OR the owning actor itself when using beacons
			Connection->ViewTarget = Connection->PlayerController ? Connection->PlayerController->GetViewTarget() : OwningActor;
			//@todo - eliminate this mallocs if the connection isn't going to actually be updated this frame (currently needed to verify owner relevancy below)
			Connection->OwnedConsiderList.Empty(NetRelevantActorCount);

			for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
			{
				UNetConnection *Child = Connection->Children[ChildIdx];
				APlayerController* ChildPlayerController = Child->PlayerController;
				if (ChildPlayerController != NULL)
				{
					Child->ViewTarget = ChildPlayerController->GetViewTarget();
					Child->OwnedConsiderList.Empty(NetRelevantActorCount);
				}
				else
				{
					Child->ViewTarget = NULL;
				}
			}
		}
		else
		{
			Connection->ViewTarget = NULL;
			for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
			{
				Connection->Children[ChildIdx]->ViewTarget = NULL;
			}
		}
	}

	// early out if no connections are ready to receive data
	if (!bFoundReadyConnection)
	{
		return 0;
	}

	// The world should be set if bFoundReadyConnection is true
	check(World);

	// make list of actors to consider to relevancy checking and replication
	TArray<AActor*> ConsiderList;
	ConsiderList.Reserve(NetRelevantActorCount);

	int32 NumInitiallyDormant = 0;

	// Add WorldSettings to consider list if we have one
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if( WorldSettings )
	{
		if (WorldSettings->GetRemoteRole() != ROLE_None && WorldSettings->NetDriverName == NetDriverName)
		{
			// For performance reasons, make sure we don't resize the array. It should already be appropriately sized above!
			ensure(ConsiderList.Num() < ConsiderList.Max());
			ConsiderList.Add(WorldSettings);
		}
	}

	bool bCPUSaturated		= false;
	float ServerTickTime	= GEngine->GetMaxTickRate( DeltaSeconds );
	if ( ServerTickTime == 0.f )
	{
		ServerTickTime = DeltaSeconds;
	}
	else
	{
		ServerTickTime	= 1.f/ServerTickTime;
		bCPUSaturated	= DeltaSeconds > 1.2f * ServerTickTime;
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_NetConsiderActorsTime);
		UE_LOG(LogNetTraffic, Log, TEXT("UWorld::ServerTickClients, Building ConsiderList %4.2f"), World->GetTimeSeconds());

		SET_DWORD_STAT( STAT_NumNetActors, World->NetworkActors.Num() );

		for ( int i = World->NetworkActors.Num() - 1; i >= 0 ; i-- )		// Traverse list backwards so we can easily remove items
		{
			AActor* Actor = World->NetworkActors[i];

			if (Actor->IsPendingKill() )
			{
				World->NetworkActors.RemoveAtSwap( i );
				continue;
			}

			if (Actor->GetRemoteRole()==ROLE_None)
			{
				World->NetworkActors.RemoveAtSwap( i );
				continue;
			}

			// This actor may belong to a different net driver, make sure this is the correct one
			// (this can happen when using beacon net drivers for example)
			if ( Actor->NetDriverName != NetDriverName )
			{
				continue;
			}

			// Don't send actors that may still be streaming in
			ULevel* Level = Actor->GetLevel();
			if ( Level->HasVisibilityRequestPending() || Level->bIsAssociatingLevel )
			{
				continue;
			}

			if ( Actor->NetDormancy == DORM_Initial && Actor->IsNetStartupActor() )
			{
				// This stat isn't that useful in its current form when using NetworkActors list
				// We'll want to track initially dormant actors some other way to track them with stats
				SCOPE_CYCLE_COUNTER(STAT_NetInitialDormantCheckTime);		
				NumInitiallyDormant++;
				World->NetworkActors.RemoveAtSwap( i );
				//UE_LOG(LogNetTraffic, Log, TEXT("Skipping Actor %s - its initially dormant!"), *Actor->GetName() );
				continue;
			}

			check( Actor->NeedsLoadForClient() );			// We have no business sending this unless the client can load

			check (World == Actor->GetWorld());
			if( (Actor->bPendingNetUpdate || World->TimeSeconds > Actor->NetUpdateTime) ) 
			{
				// if this actor isn't being considered due to a previous ServerTickClients() call where not all clients were able to replicate the actor
				if (!Actor->bPendingNetUpdate)
				{
					UE_LOG(LogNetTraffic, Log, TEXT("actor %s requesting new net update, time: %2.3f"), *Actor->GetName(), World->TimeSeconds);
					// then set the next update time
					Actor->NetUpdateTime = World->TimeSeconds + FMath::SRand() * ServerTickTime + 1.f/Actor->NetUpdateFrequency; // FIXME - cache 1/netupdatefreq
					// and mark when the actor first requested an update
					//@note: using Time because it's compared against UActorChannel.LastUpdateTime which also uses that value
					Actor->LastNetUpdateTime = Time;
				}
				/*
				else
				{
					UE_LOG(LogNetTraffic, Log, TEXT("actor %s still pending update, time since update request: %2.3f"),*Actor->GetName(),World->TimeSeconds-Actor->LastNetUpdateTime);
				}
				*/
				// and clear the pending update flag assuming all clients will be able to consider it
				Actor->bPendingNetUpdate = false;

				bool bWasConsidered = false;

				// if this actor relevant to any client
				if ( !Actor->bOnlyRelevantToOwner ) 
				{
					// add it to the list to consider below
					// For performance reasons, make sure we don't resize the array. It should already be appropriately sized above!
					ensure(ConsiderList.Num() < ConsiderList.Max());
					ConsiderList.Add(Actor);

					bWasConsidered = true;
				}
				else
				{
					const AActor* ActorOwner = Actor->GetNetOwner();
					if ( ActorOwner )
					{
						// iterate through each connection (and child connections) looking for an owner for this actor
						for ( int32 ConnIdx = 0; ConnIdx < ClientConnections.Num(); ConnIdx++ )
						{
							UNetConnection* ClientConnection = ClientConnections[ConnIdx];
							UNetConnection* Connection = ClientConnection;
							int32 ChildIndex = 0;
							bool bCloseChannel = true;
							while (Connection != NULL)
							{
								if (Connection->ViewTarget != NULL)
								{
									if (ActorOwner == Connection->PlayerController || 
										(Connection->PlayerController && ActorOwner == Connection->PlayerController->GetPawn()) ||
										Connection->ViewTarget->IsRelevancyOwnerFor(Actor, ActorOwner, Connection->OwningActor))
									{
										// For performance reasons, make sure we don't resize the array. It should already be appropriately sized above!
										ensure(Connection->OwnedConsiderList.Num() < Connection->OwnedConsiderList.Max());
										Connection->OwnedConsiderList.Add(Actor);
										bCloseChannel = false;
										
										bWasConsidered = true;
									}
								}
								else
								{
									// don't ever close the channel if one or more child connections don't have a Viewer to check relevancy with
									bCloseChannel = false;
								}
								// iterate to the next child connection if available
								Connection = (ChildIndex < ClientConnection->Children.Num()) ? ClientConnection->Children[ChildIndex++] : NULL;
							}
							// if it's not being considered, but there is an open channel for this actor already, close it
							if (bCloseChannel)
							{
								UActorChannel* Channel = ClientConnection->ActorChannels.FindRef(Actor);
								if (Channel != NULL && Time - Channel->RelevantTime >= RelevantTimeout)
								{
									Channel->Close();
								}
							}
						}
					}
				}

				if ( bWasConsidered )
				{
					Actor->PreReplication( *FindOrCreateRepChangedPropertyTracker( Actor ).Get() );
				}
			}
			/*
			else
			{
				if( Actor->GetAPawn() && (Actor->RemoteRole!=ROLE_None) && (World->GetTimeSeconds() <= Actor->NetUpdateTime) ) 
				{
					UE_LOG(LogNetTraffic, Log, TEXT("%s skipped in considerlist because of NetUpdateTime %f"), *Actor->GetName(), (World->GetTimeSeconds() - Actor->NetUpdateTime) );
				}
			}
			*/
		}
	}

	SET_DWORD_STAT(STAT_NumInitiallyDormantActors,NumInitiallyDormant);
	SET_DWORD_STAT(STAT_NumConsideredActors,ConsiderList.Num());

	for( int32 i=0; i < ClientConnections.Num(); i++ )
	{
		UNetConnection* Connection = ClientConnections[i];
		check(Connection);
		int32 ActorUpdatesThisConnection = 0;
		int32 ActorUpdatesThisConnectionSent = 0;

		// if this client shouldn't be ticked this frame
		if (i >= NumClientsToTick)
		{
			//UE_LOG(LogNet, Log, TEXT("skipping update to %s"),*Connection->GetName());
			// then mark each considered actor as bPendingNetUpdate so that they will be considered again the next frame when the connection is actually ticked
			for (int32 ConsiderIdx = 0; ConsiderIdx < ConsiderList.Num(); ConsiderIdx++)
			{
				AActor *Actor = ConsiderList[ConsiderIdx];
				// if the actor hasn't already been flagged by another connection,
				if (Actor != NULL && !Actor->bPendingNetUpdate)
				{
					// find the channel
					UActorChannel *Channel = Connection->ActorChannels.FindRef(Actor);
					// and if the channel last update time doesn't match the last net update time for the actor
					if (Channel != NULL && Channel->LastUpdateTime < Actor->LastNetUpdateTime)
					{
						//UE_LOG(LogNet, Log, TEXT("flagging %s for a future update"),*Actor->GetName());
						// flag it for a pending update
						Actor->bPendingNetUpdate = true;
					}
				}
			}
			// clear the time sensitive flag to avoid sending an extra packet to this connection
			Connection->TimeSensitive = false;

			Connection->OwnedConsiderList.Empty();
			for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
			{
				if (Connection->Children[ChildIdx])
				{
					Connection->Children[ChildIdx]->OwnedConsiderList.Empty();
				}
			}
		}
		else if (Connection->ViewTarget)
		{
			int32 j;
			int32 ConsiderCount	= 0;
			int32 DeletedCount = 0;
			
			int32 NetRelevantCount = 0;
			FActorPriority* PriorityList = NULL;
			FActorPriority** PriorityActors = NULL;

			TArray<FNetViewer>& ConnectionViewers = WorldSettings->ReplicationViewers;

			float PruneActors = 0.f;
			CLOCK_CYCLES(PruneActors);
			FMemMark RelevantActorMark(FMemStack::Get());

			// Prioritize actors for this connection
			{
				SCOPE_CYCLE_COUNTER(STAT_NetPrioritizeActorsTime);

				// send ClientAdjustment if necessary
				// we do this here so that we send a maximum of one per packet to that client; there is no value in stacking additional corrections
				if (Connection->PlayerController)
				{
					Connection->PlayerController->SendClientAdjustment();
				}
				
				for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
				{
					if (Connection->Children[ChildIdx]->PlayerController != NULL)
					{
						Connection->Children[ChildIdx]->PlayerController->SendClientAdjustment();
					}
				}

				// Get list of visible/relevant actors.
				
				NetTag++;
				Connection->TickCount++;

				// Set up to skip all sent temporary actors
				for( j=0; j<Connection->SentTemporaries.Num(); j++ )
				{
					Connection->SentTemporaries[j]->NetTag = NetTag;
				}

				// set the replication viewers to the current connection (and children) so that actors can determine who is currently being considered for relevancy checks
				ConnectionViewers.Reset();
				new(ConnectionViewers) FNetViewer(Connection, DeltaSeconds);
				for (j = 0; j < Connection->Children.Num(); j++)
				{
					if (Connection->Children[j]->ViewTarget != NULL)
					{
						new(ConnectionViewers) FNetViewer(Connection->Children[j], DeltaSeconds);
					}
				}

				// Make list of all actors to consider.
				check(World == Connection->OwningActor->GetWorld());
				
				NetRelevantCount = World->GetNetRelevantActorCount() + DestroyedStartupOrDormantActors.Num();
				PriorityList = new(FMemStack::Get(),NetRelevantCount+2)FActorPriority;
				PriorityActors = new(FMemStack::Get(),NetRelevantCount+2)FActorPriority*;

				// determine whether we should priority sort the list of relevant actors based on the saturation/bandwidth of the current connection
				//@note - if the server is currently CPU saturated then do not sort until framerate improves
				check(World == Connection->ViewTarget->GetWorld());
				AGameMode const* const GameMode = World->GetAuthGameMode();
				bool bLowNetBandwidth = !bCPUSaturated && (Connection->CurrentNetSpeed / float(GameMode->NumPlayers + GameMode->NumBots) < 500.f );

				for( AActor* Actor : ConsiderList )
				{
					UActorChannel* Channel = Connection->ActorChannels.FindRef(Actor);

					// Skip Actor if dormant
					if ( CVarSetNetDormancyEnabled.GetValueOnGameThread() == 1 )
					{
						// If actor is already dormant on this channel, then skip replication entirely
						if ( Connection->DormantActors.Contains( Actor ) )
						{
							// net.DormancyValidate can be set to 2 to validate dormant actor properties on every replicate
							// (this could be moved to be done every tick instead of every net update if necessary, but seems excessive)
							if ( CVarNetDormancyValidate.GetValueOnGameThread() == 2 )
							{
								TSharedRef< FObjectReplicator > * Replicator = Connection->DormantReplicatorMap.Find( Actor );

								if ( Replicator != NULL )
								{
									Replicator->Get().ValidateAgainstState( Actor );
								}
							}

							continue;
						}

						// If actor might need to go dormant on this channel, then check
						if (Actor->NetDormancy > DORM_Awake && Channel && !Channel->bPendingDormancy && !Channel->Dormant )
						{
							bool ShouldGoDormant = true;
							if (Actor->NetDormancy == DORM_DormantPartial)
							{
								float LastReplicationTime  = Channel ? (Connection->Driver->Time - Channel->LastUpdateTime) : Connection->Driver->SpawnPrioritySeconds;
								for (int32 viewerIdx = 0; viewerIdx < ConnectionViewers.Num(); viewerIdx++)
								{
									if (!Actor->GetNetDormancy(ConnectionViewers[viewerIdx].ViewLocation, ConnectionViewers[viewerIdx].ViewDir, ConnectionViewers[viewerIdx].InViewer, ConnectionViewers[viewerIdx].ViewTarget, Channel, Time, bLowNetBandwidth))
									{
										ShouldGoDormant = false;
										break;
									}
								}
							}

							if (ShouldGoDormant)
							{
								// Channel is marked to go dormant now once all properties have been replicated (but is not dormant yet)
								Channel->StartBecomingDormant();
							}
						}
					}


					// Skip actor if not relevant and theres no channel already.
					// Historically Relevancy checks were deferred until after prioritization because they were expensive (line traces).
					// Relevancy is now cheap and we are dealing with larger lists of considered actors, so we want to keep the list of
					// prioritized actors low.
					if (!Channel)
					{
						if ( !IsLevelInitializedForActor(Actor, Connection) )
						{
							// If the level this actor belongs to isn't loaded on client, don't bother sending
							continue;
						}
						bool Relevant = false;
						for (int32 viewerIdx = 0; viewerIdx < ConnectionViewers.Num(); viewerIdx++)
						{
							if(Actor->IsNetRelevantFor(ConnectionViewers[viewerIdx].InViewer, ConnectionViewers[viewerIdx].ViewTarget, ConnectionViewers[viewerIdx].ViewLocation))
							{
								Relevant = true;
								break;
							}
						}
						if (!Relevant)
						{
							continue;
						}
					}

					if( Actor->NetTag!=NetTag ) // Do not consider actor for this connection if this connection has it marked dormant
					{
						UE_LOG(LogNetTraffic, Log, TEXT("Consider %s alwaysrelevant %d frequency %f "),*Actor->GetName(), Actor->bAlwaysRelevant, Actor->NetUpdateFrequency);
						Actor->NetTag                 = NetTag;
						PriorityList  [ConsiderCount] = FActorPriority(Connection, Channel, Actor, ConnectionViewers, bLowNetBandwidth);
						PriorityActors[ConsiderCount] = PriorityList + ConsiderCount;
						ConsiderCount++;

						if (DebugRelevantActors)
						{
							LastPrioritizedActors.Add(Actor);
						}
					}
				}

				// Add in deleted actors
				for (auto It = Connection->DestroyedStartupOrDormantActors.CreateIterator(); It; ++It)
				{
					FActorDestructionInfo &DInfo = DestroyedStartupOrDormantActors.FindChecked(*It);
					PriorityList  [ConsiderCount] = FActorPriority(Connection, &DInfo, ConnectionViewers);
					PriorityActors[ConsiderCount] = PriorityList + ConsiderCount;
					ConsiderCount++;
					DeletedCount++;
				}

				UNetConnection* NextConnection = Connection;
				int32 ChildIndex = 0;
				while (NextConnection != NULL)
				{
					for (AActor* Actor : NextConnection->OwnedConsiderList)
					{
						UE_LOG(LogNetTraffic, Log, TEXT("Consider owned %s always relevant %d frequency %f  "),*Actor->GetName(), Actor->bAlwaysRelevant,Actor->NetUpdateFrequency);
						if (Actor->NetTag != NetTag)
						{
							UActorChannel* Channel = Connection->ActorChannels.FindRef(Actor);
							Actor->NetTag                 = NetTag;
							PriorityList  [ConsiderCount] = FActorPriority(NextConnection, Channel, Actor, ConnectionViewers, bLowNetBandwidth);
							PriorityActors[ConsiderCount] = PriorityList + ConsiderCount;
							ConsiderCount++;

							if (DebugRelevantActors)
							{
								LastPrioritizedActors.Add(Actor);
							}
						}
					}
					NextConnection->OwnedConsiderList.Empty();

					NextConnection = (ChildIndex < Connection->Children.Num()) ? Connection->Children[ChildIndex++] : NULL;
				}

				SET_DWORD_STAT(STAT_PrioritizedActors,ConsiderCount);
				SET_DWORD_STAT(STAT_NumRelevantDeletedActors,DeletedCount);

				// Sort by priority
				struct FCompareFActorPriority
				{
					FORCEINLINE bool operator()( const FActorPriority& A, const FActorPriority& B ) const
					{
						return B.Priority < A.Priority;
					}
				};
				Sort( PriorityActors, ConsiderCount, FCompareFActorPriority() );

			} // END PRIORITIZE

			// Update all relevant actors in sorted order.
			bool bNewSaturated = !Connection->IsNetReady(0);
			if (bNewSaturated)
			{
				j = 0;
			}
			else
			{
				UE_LOG(LogNetTraffic, Log, TEXT("START"));
				int32 FinalRelevantCount = 0;
				for (j = 0; j < ConsiderCount; j++)
				{
					// Deletion entry
					if (PriorityActors[j]->Actor == NULL && PriorityActors[j]->DestructionInfo)
					{
						// Make sure client has streaming level loaded
						if (PriorityActors[j]->DestructionInfo->StreamingLevelName != NAME_None && !Connection->ClientVisibleLevelNames.Contains(PriorityActors[j]->DestructionInfo->StreamingLevelName))
						{
							// This deletion entry is for an actor in a streaming level the connection doesn't have loaded, so skip it
							continue;
						}

						UActorChannel* Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
						if (Channel)
						{
							FinalRelevantCount++;
							UE_LOG(LogNetTraffic, Log, TEXT("Server replicate actor creating destroy channel for NetGUID <%s,%s> Priority: %d"), *PriorityActors[j]->DestructionInfo->NetGUID.ToString(), *PriorityActors[j]->DestructionInfo->PathName, PriorityActors[j]->Priority );

							Channel->SetChannelActorForDestroy( PriorityActors[j]->DestructionInfo ); // Send a close bunch on the new channel
							Connection->DestroyedStartupOrDormantActors.Remove( PriorityActors[j]->DestructionInfo->NetGUID ); // Remove from connections to-be-destroyed list (close bunch of reliable, so it will make it there)
						}
						continue;
					}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					static IConsoleVariable* DebugObjectCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.PackageMap.DebugObject"));
					if (DebugObjectCvar && !DebugObjectCvar->GetString().IsEmpty() && PriorityActors[j]->Actor && PriorityActors[j]->Actor->GetName().Contains(DebugObjectCvar->GetString()) )
					{
						UE_LOG(LogNetPackageMap, Log, TEXT("Evaluating actor for replication %s"), *PriorityActors[j]->Actor->GetName());
					}
#endif

					// Normal actor replication
					UActorChannel* Channel     = PriorityActors[j]->Channel;
					UE_LOG(LogNetTraffic, Log, TEXT(" Maybe Replicate %s"),*PriorityActors[j]->Actor->GetName());
					if ( !Channel || Channel->Actor ) //make sure didn't just close this channel
					{ 
						AActor*		Actor       = PriorityActors[j]->Actor;
						bool		bIsRelevant = false;

						const bool bLevelInitializedForActor = IsLevelInitializedForActor(Actor, Connection);

						// only check visibility on already visible actors every 1.0 + 0.5R seconds
						// bTearOff actors should never be checked
						if ( bLevelInitializedForActor )
						{
							if (!Actor->bTearOff && (!Channel || Time - Channel->RelevantTime > 1.f))
							{
								for (int32 k = 0; k < ConnectionViewers.Num(); k++)
								{
									if (Actor->IsNetRelevantFor(ConnectionViewers[k].InViewer, ConnectionViewers[k].ViewTarget, ConnectionViewers[k].ViewLocation))
									{
										bIsRelevant = true;
										break;
									}
									else
									{
										//UE_LOG(LogNetPackageMap, Warning, TEXT("Actor NonRelevant: %s"), *Actor->GetName() );
										if (DebugRelevantActors)
										{
											LastNonRelevantActors.Add(Actor);
										}
									}
								}
							}
						}
						else
						{
							// Actor is no longer relevant because the world it is/was in is not loaded by client
							// exception: player controllers should never show up here
							UE_LOG(LogNetTraffic, Log, TEXT("- Level not initialized for actor %s"), *Actor->GetName());
						}
						
						// if the actor is now relevant or was recently relevant
						const bool bIsRecentlyRelevant = bIsRelevant || (Channel && Time - Channel->RelevantTime < RelevantTimeout);

						if( bIsRecentlyRelevant )
						{	
							FinalRelevantCount++;

							// Find or create the channel for this actor.
							// we can't create the channel if the client is in a different world than we are
							// or the package map doesn't support the actor's class/archetype (or the actor itself in the case of serializable actors)
							// or it's an editor placed actor and the client hasn't initialized the level it's in
							if ( Channel == NULL && GuidCache->SupportsObject(Actor->GetClass()) &&
									GuidCache->SupportsObject(Actor->IsNetStartupActor() ? Actor : Actor->GetArchetype()) )
							{
								if (bLevelInitializedForActor)
								{
									// Create a new channel for this actor.
									Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
									if( Channel )
									{
										Channel->SetChannelActor( Actor );
									}
								}
								// if we couldn't replicate it for a reason that should be temporary, and this Actor is updated very infrequently, make sure we update it again soon
								else if (Actor->NetUpdateFrequency < 1.0f)
								{
									UE_LOG(LogNetTraffic, Log, TEXT("Unable to replicate %s"),*Actor->GetName());
									Actor->NetUpdateTime = Actor->GetWorld()->TimeSeconds + 0.2f * FMath::FRand();
								}
							}

							if( Channel )
							{
								// if it is relevant then mark the channel as relevant for a short amount of time
								if( bIsRelevant )
								{
									Channel->RelevantTime = Time + 0.5f * FMath::SRand();
								}
								// if the channel isn't saturated
								if( Channel->IsNetReady(0) )
								{
									// replicate the actor
									UE_LOG(LogNetTraffic, Log, TEXT("- Replicate %s. %d"),*Actor->GetName(), PriorityActors[j]->Priority);
									if (DebugRelevantActors)
									{
										LastRelevantActors.Add( Actor );
									}

									if (Channel->ReplicateActor())
									{
										ActorUpdatesThisConnectionSent++;
										if (DebugRelevantActors)
										{
											LastSentActors.Add( Actor );
										}
									}
									ActorUpdatesThisConnection++;
									Updated++;
								}
								else
								{							
									UE_LOG(LogNetTraffic, Log, TEXT("- Channel saturated, forcing pending update for %s"),*Actor->GetName());
									// otherwise force this actor to be considered in the next tick again
									Actor->ForceNetUpdate();
								}
								// second check for channel saturation
								if (!Connection->IsNetReady(0))
								{
									bNewSaturated = true;
									break;
								}
							}
						}
						
						// If the actor wasn't recently relevant, or if it was torn off,
						// close the actor channel if it exists for this connection
						if ((!bIsRecentlyRelevant || Actor->bTearOff) && Channel != NULL)
						{
							// Non startup (map) actors have their channels closed immediately, which destroys them.
							// Startup actors get to keep their channels open.

							// Fixme: this should be a setting
							if ( !bLevelInitializedForActor || !Actor->IsNetStartupActor() )
							{
								UE_LOG(LogNetTraffic, Log, TEXT("- Closing channel for no longer relevant actor %s"),*Actor->GetName());
								Channel->Close();
							}
						}
					}
				}

				SET_DWORD_STAT(STAT_NumRelevantActors,FinalRelevantCount);
			}

			// relevant actors that could not be processed this frame are marked to be considered for next frame
			for ( int32 k=j; k<ConsiderCount; k++ )
			{
				AActor* Actor = PriorityActors[k]->Actor;
				if (!Actor)
				{
					// A deletion entry, skip it because we dont have anywhere to store a 'better give higher priority next time'
					continue;
				}

				UActorChannel* Channel = PriorityActors[k]->Channel;
				
				UE_LOG(LogNetTraffic, Verbose, TEXT("Saturated. %s"), *Actor->GetName());
				if (Channel != NULL && Time - Channel->RelevantTime <= 1.f)
				{
					UE_LOG(LogNetTraffic, Log, TEXT(" Saturated. Mark %s NetUpdateTime to be checked for next tick"), *Actor->GetName());
					Actor->bPendingNetUpdate = true;
				}
				else
				{
					for (int32 h = 0; h < ConnectionViewers.Num(); h++)
					{
						if (Actor->IsNetRelevantFor(ConnectionViewers[h].InViewer, ConnectionViewers[h].ViewTarget, ConnectionViewers[h].ViewLocation))
						{
							UE_LOG(LogNetTraffic, Log, TEXT(" Saturated. Mark %s NetUpdateTime to be checked for next tick"), *Actor->GetName());
							Actor->bPendingNetUpdate = true;
							if (Channel != NULL)
							{
								Channel->RelevantTime = Time + 0.5f * FMath::SRand();
							}
							break;
						}
					}
				}
			}
			RelevantActorMark.Pop();
			UE_LOG(LogNetTraffic, Log, TEXT("Potential %04i ConsiderList %03i ConsiderCount %03i Prune=%01.4f "),NetRelevantCount, 
						ConsiderList.Num(), ConsiderCount, FPlatformTime::ToMilliseconds(PruneActors) );

			SET_DWORD_STAT(STAT_NumReplicatedActorAttempts,ActorUpdatesThisConnection);
			SET_DWORD_STAT(STAT_NumReplicatedActors,ActorUpdatesThisConnectionSent);
		}
	}

	// shuffle the list of connections if not all connections were ticked
	if (NumClientsToTick < ClientConnections.Num())
	{
		int32 NumConnectionsToMove = NumClientsToTick;
		while (NumConnectionsToMove > 0)
		{
			// move all the ticked connections to the end of the list so that the other connections are considered first for the next frame
			UNetConnection *Connection = ClientConnections[0];
			ClientConnections.RemoveAt(0,1);
			ClientConnections.Add(Connection);
			NumConnectionsToMove--;
		}
	}
	Mark.Pop();

	if (DebugRelevantActors)
	{
		PrintDebugRelevantActors();
		LastPrioritizedActors.Empty();
		LastSentActors.Empty();
		LastRelevantActors.Empty();
		LastNonRelevantActors.Empty();

		DebugRelevantActors  = false;
	}

	return Updated;
#else
	return 0;
#endif // WITH_SERVER_CODE
}


void UNetDriver::PrintDebugRelevantActors()
{
	struct SLocal
	{
		static void AggregateAndPrint( TArray< TWeakObjectPtr<AActor> >	&List, FString txt )
		{
			TMap< TWeakObjectPtr<UClass>, int32>	ClassSummary;
			TMap< TWeakObjectPtr<UClass>, int32>	SuperClassSummary;

			for (auto It = List.CreateIterator(); It; ++It)
			{
				AActor* Actor = Cast<AActor>(It->Get());
				if (Actor)
				{

					ClassSummary.FindOrAdd(Actor->GetClass())++;
					if (Actor->GetClass()->GetSuperStruct())
					{
						SuperClassSummary.FindOrAdd( Actor->GetClass()->GetSuperClass() )++;
					}
				}
			}

			struct FCompareActorClassCount
			{
				FORCEINLINE bool operator()( int32 A, int32 B ) const
				{
					return A < B;
				}
			};


			ClassSummary.ValueSort(FCompareActorClassCount());
			SuperClassSummary.ValueSort(FCompareActorClassCount());

			UE_LOG(LogNet, Warning, TEXT("------------------------------") );
			UE_LOG(LogNet, Warning, TEXT(" %s Class Summary"), *txt );
			UE_LOG(LogNet, Warning, TEXT("------------------------------") );

			for (auto It = ClassSummary.CreateIterator(); It; ++It)
			{
				UE_LOG(LogNet, Warning, TEXT("%4d - %s (%s)"), It.Value(), *It.Key()->GetName(), It.Key()->GetSuperStruct() ? *It.Key()->GetSuperStruct()->GetName() : TEXT("NULL") );
			}

			UE_LOG(LogNet, Warning, TEXT("---------------------------------") );
			UE_LOG(LogNet, Warning, TEXT(" %s Parent Class Summary "), *txt );
			UE_LOG(LogNet, Warning, TEXT("------------------------------") );

			for (auto It = SuperClassSummary.CreateIterator(); It; ++It)
			{
				UE_LOG(LogNet, Warning, TEXT("%4d - %s (%s)"), It.Value(), *It.Key()->GetName(), It.Key()->GetSuperStruct() ? *It.Key()->GetSuperStruct()->GetName() : TEXT("NULL") );
			}

			UE_LOG(LogNet, Warning, TEXT("---------------------------------") );
			UE_LOG(LogNet, Warning, TEXT(" %s Total: %d"), *txt, List.Num() );
			UE_LOG(LogNet, Warning, TEXT("---------------------------------") );
		}
	};

	SLocal::AggregateAndPrint( LastPrioritizedActors, TEXT(" Prioritized Actor") );
	SLocal::AggregateAndPrint( LastRelevantActors, TEXT(" Relevant Actor") );
	SLocal::AggregateAndPrint( LastNonRelevantActors, TEXT(" NonRelevant Actor") );
	SLocal::AggregateAndPrint( LastSentActors, TEXT(" Sent Actor") );

	UE_LOG(LogNet, Warning, TEXT("---------------------------------") );
	UE_LOG(LogNet, Warning, TEXT(" Num Connections: %d"), ClientConnections.Num() );

	UE_LOG(LogNet, Warning, TEXT("---------------------------------") );

}

void UNetDriver::DrawNetDriverDebug()
{
	UNetConnection *Connection = (ServerConnection ? ServerConnection : (ClientConnections.Num() >= 1 ? ClientConnections[0] : NULL));
	if (!Connection)
	{
		return;
	}

	ULocalPlayer*	LocalPlayer = NULL;
	for(FLocalPlayerIterator It(GEngine, GetWorld());It;++It)
	{
		LocalPlayer = *It;
		break;
	}
	if (!LocalPlayer)
	{
		return;
	}

	const float CullDist = CVarNetDormancyDrawCullDistance.GetValueOnGameThread();

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		if ((It->GetActorLocation() - LocalPlayer->LastViewLocation).Size() > CullDist)
		{
			continue;
		}
		
		FColor	DrawColor;
		if (Connection->DormantActors.Contains(*It))
		{
			DrawColor = FColor::Red;
		}
		else if (Connection->ActorChannels.FindRef(*It) != NULL)
		{
			DrawColor = FColor::Green;
		}
		else
		{
			continue;
		}

		FBox Box = 	It->GetComponentsBoundingBox();
		DrawDebugBox( GetWorld(), Box.GetCenter(), Box.GetExtent(), FQuat::Identity, DrawColor, false );
	}
}

bool UNetDriver::NetObjectIsDynamic(const UObject *Object) const
{
	const UActorComponent *ActorComponent = Cast<const UActorComponent>(Object);
	if (ActorComponent)
	{
		// Actor components are dynamic if their owning actor is.
		return NetObjectIsDynamic(Object->GetOuter());
	}

	const AActor *Actor = Cast<const AActor>(Object);
	if (!Actor || Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || Actor->IsNetStartupActor())
	{
		return false;
	}

	return true;
}

void UNetDriver::AddClientConnection(UNetConnection * NewConnection)
{
	UE_LOG( LogNet, Log, TEXT( "Added client connection.  Remote address = %s" ), *NewConnection->LowLevelGetRemoteAddress( true ) );

	ClientConnections.Add(NewConnection);

	for (auto It = DestroyedStartupOrDormantActors.CreateIterator(); It; ++It)
	{
		if (It.Key().IsStatic())
		{
			UE_LOG(LogNet, Verbose, TEXT("Adding actor NetGUID <%s> to new connection's destroy list"), *It.Key().ToString());
			NewConnection->DestroyedStartupOrDormantActors.Add(It.Key());
		}
	}
}

void UNetDriver::SetWorld(class UWorld* InWorld)
{
	if (World)
	{
		// Remove old world association
		UnregisterTickEvents(World);
		World = NULL;
		Notify = NULL;
	}

	if (InWorld)
	{
		// Setup new world association
		World = InWorld;
		Notify = InWorld;
		RegisterTickEvents(InWorld);
	}
}

void UNetDriver::ResetGameWorldState()
{
	DestroyedStartupOrDormantActors.Empty();

	if ( NetCache.IsValid() )
	{
		NetCache->ClearClassNetCache();	// Clear the cache net: it will recreate itself after seamless travel
	}

	if (ServerConnection)
	{
		ServerConnection->ResetGameWorldState();
	}
	for (auto It = ClientConnections.CreateIterator(); It; ++It)
	{
		(*It)->ResetGameWorldState();
	}
}

void UNetDriver::CleanPackageMaps()
{
	if ( GuidCache.IsValid() )
	{ 
		GuidCache->CleanReferences();
	}
}

void UNetDriver::PreSeamlessTravelGarbageCollect()
{
	ResetGameWorldState();
}

void UNetDriver::PostSeamlessTravelGarbageCollect()
{
	CleanPackageMaps();
}

static void	DumpRelevantActors( UWorld* InWorld )
{
	UNetDriver *NetDriver = InWorld->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	NetDriver->DebugRelevantActors = true;
}

TSharedPtr<FRepChangedPropertyTracker> UNetDriver::FindOrCreateRepChangedPropertyTracker(UObject* Obj)
{
	TSharedPtr<FRepChangedPropertyTracker> * GlobalPropertyTrackerPtr = RepChangedPropertyTrackerMap.Find( Obj );

	if ( !GlobalPropertyTrackerPtr ) 
	{
		FRepChangedPropertyTracker * Tracker = new FRepChangedPropertyTracker();

		GetObjectClassRepLayout( Obj->GetClass() )->InitChangedTracker( Tracker );

		GlobalPropertyTrackerPtr = &RepChangedPropertyTrackerMap.Add( Obj, TSharedPtr<FRepChangedPropertyTracker>( Tracker ) );
	}

	return *GlobalPropertyTrackerPtr;
}

TSharedPtr<FRepLayout> UNetDriver::GetObjectClassRepLayout( UClass * Class )
{
	TSharedPtr<FRepLayout> * RepLayoutPtr = RepLayoutMap.Find( Class );

	if ( !RepLayoutPtr ) 
	{
		FRepLayout * RepLayout = new FRepLayout();
		RepLayout->InitFromObjectClass( Class );
		RepLayoutPtr = &RepLayoutMap.Add( Class, TSharedPtr<FRepLayout>( RepLayout ) );
	}

	return *RepLayoutPtr;
}

TSharedPtr<FRepLayout> UNetDriver::GetFunctionRepLayout( UFunction * Function )
{
	TSharedPtr<FRepLayout> * RepLayoutPtr = RepLayoutMap.Find( Function );

	if ( !RepLayoutPtr ) 
	{
		FRepLayout * RepLayout = new FRepLayout();
		RepLayout->InitFromFunction( Function );
		RepLayoutPtr = &RepLayoutMap.Add( Function, TSharedPtr<FRepLayout>( RepLayout ) );
	}

	return *RepLayoutPtr;
}

TSharedPtr<FRepLayout> UNetDriver::GetStructRepLayout( UStruct * Struct )
{
	TSharedPtr<FRepLayout> * RepLayoutPtr = RepLayoutMap.Find( Struct );

	if ( !RepLayoutPtr ) 
	{
		FRepLayout * RepLayout = new FRepLayout();
		RepLayout->InitFromStruct( Struct );
		RepLayoutPtr = &RepLayoutMap.Add( Struct, TSharedPtr<FRepLayout>( RepLayout ) );
	}

	return *RepLayoutPtr;
}

FAutoConsoleCommandWithWorld	DumpRelevantActorsCommand(
	TEXT("net.DumpRelevantActors"), 
	TEXT( "Dumps information on relevant actors during next network update" ), 
	FConsoleCommandWithWorldDelegate::CreateStatic(DumpRelevantActors)
	);

/**
 * Exec handler that routes online specific execs to the proper subsystem
 *
 * @param InWorld World context
 * @param Cmd 	the exec command being executed
 * @param Ar 	the archive to log results to
 *
 * @return true if the handler consumed the input, false to continue searching handlers
 */
static bool NetDriverExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bHandled = false;

	// Ignore any execs that don't start with NET
	if (FParse::Command(&Cmd, TEXT("NET")))
	{
		UNetDriver* NamedDriver = NULL;
		TCHAR TokenStr[128];

		// Route the command to a specific beacon if a name is specified or all of them otherwise
		if (FParse::Token(Cmd, TokenStr, ARRAY_COUNT(TokenStr), true))
		{
			NamedDriver = GEngine->FindNamedNetDriver(InWorld, FName(TokenStr));
			if (NamedDriver != NULL)
			{
				bHandled = NamedDriver->Exec(InWorld, Cmd, Ar);
			}
			else
			{
				FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(InWorld);

				Cmd -= FCString::Strlen(TokenStr);
				for (int32 NetDriverIdx=0; NetDriverIdx < Context.ActiveNetDrivers.Num(); NetDriverIdx++)
				{
					NamedDriver = Context.ActiveNetDrivers[NetDriverIdx].NetDriver;
					if (NamedDriver)
					{
						bHandled |= NamedDriver->Exec(InWorld, Cmd, Ar);
					}
				}
			}
		}
	}

	return bHandled;
}

/** Our entry point for all net driver related exec routing */
FStaticSelfRegisteringExec NetDriverExecRegistration(NetDriverExec);
