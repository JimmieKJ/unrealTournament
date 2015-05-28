// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetConnection.cpp: Unreal connection base class.
=============================================================================*/

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "Net/NetworkProfiler.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "DataChannel.h"
#include "Engine/PackageMapClient.h"
#include "GameFramework/GameMode.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

/*-----------------------------------------------------------------------------
	UNetConnection implementation.
-----------------------------------------------------------------------------*/

UNetConnection* UNetConnection::GNetConnectionBeingCleanedUp = NULL;

UNetConnection::UNetConnection(const FObjectInitializer& ObjectInitializer)
:	UPlayer(ObjectInitializer)
,	Driver				( NULL )
,	PackageMap			( NULL )
,	ViewTarget			( NULL )
,   OwningActor			( NULL )
,	MaxPacket			( 0 )
,	InternalAck			( false )
,	State				( USOCK_Invalid )
,	PacketOverhead		( 0 )
,	ResponseId			( 0 )

,	QueuedBytes			( 0 )
,	TickCount			( 0 )
,	ConnectTime			( 0.0 )

,	AllowMerge			( false )
,	TimeSensitive		( false )
,	LastOutBunch		( NULL )

,	StatPeriod			( 1.f  )
,	BestLag				( 9999 )
,	AvgLag				( 9999 )

,	LagAcc				( 9999 )
,	BestLagAcc			( 9999 )
,	LagCount			( 0 )
,	LastTime			( 0 )
,	FrameTime			( 0 )
,	CumulativeTime		( 0 )
,	AverageFrameTime	( 0 )
,	CountedFrames		( 0 )

,	SendBuffer			( 0 )
,	InPacketId			( -1 )
,	OutPacketId			( 0 ) // must be initialized as OutAckPacketId + 1 so loss of first packet can be detected
,	OutAckPacketId		( -1 )
,	LastPingAck			( 0.f )
,	LastPingAckPacketId	( -1 )
,	ClientWorldPackageName( NAME_None )
{
}

/**
 * Initialize common settings for this connection instance
 *
 * @param InDriver the net driver associated with this connection
 * @param InSocket the socket associated with this connection
 * @param InURL the URL to init with
 * @param InState the connection state to start with for this connection
 * @param InMaxPacket the max packet size that will be used for sending
 * @param InPacketOverhead the packet overhead for this connection type
 */
void UNetConnection::InitBase(UNetDriver* InDriver,class FSocket* InSocket, const FURL& InURL, EConnectionState InState,int32 InMaxPacket,int32 InPacketOverhead)
{
	// Owning net driver
	Driver = InDriver;

	// Stats
	StatUpdateTime = Driver->Time;
	LastReceiveTime = Driver->Time;
	LastSendTime = Driver->Time;
	LastTickTime = Driver->Time;
	LastRecvAckTime = Driver->Time;
	ConnectTime = Driver->Time;

	// Current state
	State = InState;
	// Copy the URL
	URL = InURL;

	// Use the passed in values
	MaxPacket = InMaxPacket;
	PacketOverhead = InPacketOverhead;
	check(MaxPacket && PacketOverhead);

#if DO_ENABLE_NET_TEST
	// Copy the command line settings from the net driver
	UpdatePacketSimulationSettings();
#endif

	// Other parameters.
	CurrentNetSpeed = URL.HasOption(TEXT("LAN")) ? GetDefault<UPlayer>()->ConfiguredLanSpeed : GetDefault<UPlayer>()->ConfiguredInternetSpeed;

	if ( CurrentNetSpeed == 0 )
	{
		CurrentNetSpeed = 2600;
	}
	else
	{
		CurrentNetSpeed = FMath::Max<int32>(CurrentNetSpeed, 1800);
	}

	// Create package map.
	auto PackageMapClient = NewObject<UPackageMapClient>(this);
	PackageMapClient->Initialize(this, Driver->GuidCache);
	PackageMap = PackageMapClient;

	// Create the voice channel
	CreateChannel(CHTYPE_Voice, true, VOICE_CHANNEL_INDEX);
}

/**
 * Initializes an "addressless" connection with the passed in settings
 *
 * @param InDriver the net driver associated with this connection
 * @param InState the connection state to start with for this connection
 * @param InURL the URL to init with
 * @param InConnectionSpeed Optional connection speed override
 */
void UNetConnection::InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed)
{
	Driver = InDriver;
	// We won't be sending any packets, so use a default size
	MaxPacket = 512;
	PacketOverhead = 0;
	State = InState;

#if DO_ENABLE_NET_TEST
	// Copy the command line settings from the net driver
	UpdatePacketSimulationSettings();
#endif

	// Get the 
	if (InConnectionSpeed)
	{
		CurrentNetSpeed = InConnectionSpeed;
	}
	else
	{

		CurrentNetSpeed =  URL.HasOption(TEXT("LAN")) ? GetDefault<UPlayer>()->ConfiguredLanSpeed : GetDefault<UPlayer>()->ConfiguredInternetSpeed;
		if ( CurrentNetSpeed == 0 )
		{
			CurrentNetSpeed = 2600;
		}
		else
		{
			CurrentNetSpeed = FMath::Max<int32>(CurrentNetSpeed, 1800);
		}
	}

	// Create package map.
	auto PackageMapClient = NewObject<UPackageMapClient>(this);
	PackageMapClient->Initialize(this, Driver->GuidCache);
	PackageMap = PackageMapClient;
}

void UNetConnection::Serialize( FArchive& Ar )
{
	UObject::Serialize( Ar );
	Ar << PackageMap;
	for( int32 i=0; i<MAX_CHANNELS; i++ )
	{
		Ar << Channels[i];
	}

	if (Ar.IsCountingMemory())
	{
		Children.CountBytes(Ar);
		ClientVisibleLevelNames.CountBytes(Ar);
		QueuedAcks.CountBytes(Ar);
		ResendAcks.CountBytes(Ar);
		OpenChannels.CountBytes(Ar);
		SentTemporaries.CountBytes(Ar);
		ActorChannels.CountBytes(Ar);
		DormantActors.CountBytes(Ar);
	}
}

void UNetConnection::Close()
{
	if (Driver != NULL && State != USOCK_Closed)
	{
		NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("CLOSE"), *(GetName() + TEXT(" ") + LowLevelGetRemoteAddress())));
		UE_LOG(LogNet, Log, TEXT("UNetConnection::Close: Name: %s, Driver: %s, PC: %s, Owner: %s, Channels: %i, RemoteAddr: %s, Time: %s"), 
			*GetName(), 
			*Driver->GetDescription(), 
			PlayerController ? *PlayerController->GetName() : TEXT("NULL"),
			OwningActor ? *OwningActor->GetName() : TEXT("NULL"),
			OpenChannels.Num(),
			*LowLevelGetRemoteAddress(true),
			*FDateTime::UtcNow().ToString(TEXT("%Y.%m.%d-%H.%M.%S")));

		if (Channels[0] != NULL)
		{
			Channels[0]->Close();
		}
		State = USOCK_Closed;
		FlushNet();
	}
	else
	{
		UE_LOG(LogNet, Verbose, TEXT("UNetConnection::Close: Already closed. Name: %s"), *GetName() );
	}

	LogCallLastTime		= 0;
	LogCallCount		= 0;
	LogSustainedCount	= 0;
}

void UNetConnection::CleanUp()
{
	// Remove UChildConnection(s)
	for (int32 i = 0; i < Children.Num(); i++)
	{
		Children[i]->CleanUp();
	}
	Children.Empty();

	if ( State != USOCK_Closed )
	{
		UE_LOG(LogNet, Log, TEXT("UNetConnection::Cleanup: Closing open connection. Name: %s, RemoteAddr: %s Driver: %s, PC: %s, Owner: %s"),
			*GetName(),
			*LowLevelGetRemoteAddress(true),
			Driver ? *Driver->NetDriverName.ToString() : TEXT("NULL"),
			PlayerController ? *PlayerController->GetName() : TEXT("NoPC"),
			OwningActor ? *OwningActor->GetName() : TEXT("No Owner"));
	}

	Close();

	if (Driver != NULL)
	{
		// Remove from driver.
		if (Driver->ServerConnection)
		{
			check(Driver->ServerConnection == this);
			Driver->ServerConnection = NULL;
		}
		else
		{
			check(Driver->ServerConnection == NULL);
			verify(Driver->ClientConnections.Remove(this) == 1);
		}
	}

	// Kill all channels.
	for (int32 i = OpenChannels.Num() - 1; i >= 0; i--)
	{
		UChannel* OpenChannel = OpenChannels[i];
		if (OpenChannel != NULL)
		{
			OpenChannel->ConditionalCleanUp();
		}
	}

	PackageMap = NULL;

	if (GIsRunning)
	{
		if (OwningActor != NULL)
		{	
			// Cleanup/Destroy the connection actor & controller
			if (!OwningActor->HasAnyFlags( RF_BeginDestroyed | RF_FinishDestroyed ) )
			{
				// UNetConnection::CleanUp can be called from UNetDriver::FinishDestroyed that is called from GC.
				OwningActor->OnNetCleanup(this);
			}
			OwningActor = NULL;
			PlayerController = NULL;
		}
		else 
		{
			UWorld* World = Driver ? Driver->GetWorld() : NULL;
			if (World)
			{
				AGameMode* const GameMode = World->GetAuthGameMode();
				if (GameMode)
				{
					GameMode->NotifyPendingConnectionLost();
				}
			}
		}
	}

	CleanupDormantActorState();

	Driver = NULL;
}

UChildConnection::UChildConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UChildConnection::CleanUp()
{
	if (GIsRunning)
	{
		if (OwningActor != NULL)
		{
			if ( !OwningActor->HasAnyFlags( RF_BeginDestroyed | RF_FinishDestroyed ) )
			{
				// Cleanup/Destroy the connection actor & controller	
				OwningActor->OnNetCleanup(this);
			}

			OwningActor = NULL;
			PlayerController = NULL;
		}
	}
	PackageMap = NULL;
	Driver = NULL;
}

void UNetConnection::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		CleanUp();
	}

	Super::FinishDestroy();
}

void UNetConnection::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UNetConnection* This = CastChecked<UNetConnection>(InThis);
	// Let GC know that we're potentially referencing some Actor objects
	for (int32 ActorIndex = 0; ActorIndex < This->OwnedConsiderList.Num(); ++ActorIndex)
	{
		Collector.AddReferencedObject( This->OwnedConsiderList[ActorIndex], This );
	}

	// Let GC know that we're referencing some UChannel objects
	for( int32 ChIndex=0; ChIndex < MAX_CHANNELS; ++ChIndex )
	{
		Collector.AddReferencedObject( This->Channels[ChIndex], This );
	}

	// Let GC know that we're referencing some UActorChannel objects
	for ( auto It = This->KeepProcessingActorChannelBunchesMap.CreateIterator(); It; ++It )
	{
		Collector.AddReferencedObject( It.Value(), This );
	}

	Super::AddReferencedObjects(This, Collector);
}

bool UNetConnection::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if ( Super::Exec( InWorld, Cmd,Ar) )
	{
		return true;
	}
	else if ( GEngine->Exec( InWorld, Cmd, Ar ) )
	{
		return true;
	}
	return false;
}
void UNetConnection::AssertValid()
{
	// Make sure this connection is in a reasonable state.
	check(State==USOCK_Closed || State==USOCK_Pending || State==USOCK_Open);

}
void UNetConnection::SendPackageMap()
{
}

bool UNetConnection::ClientHasInitializedLevelFor(const UObject* TestObject) const
{
	check(Driver);
	checkSlow(Driver->IsServer());

	// get the level for the object
	const ULevel* Level = NULL;
	for (const UObject* Obj = TestObject; Obj != NULL; Obj = Obj->GetOuter())
	{
		Level = Cast<const ULevel>(Obj);
		if (Level != NULL)
		{
			break;
		}
	}

	UWorld* World = Driver->GetWorld();
	check(World);
	return (Level == NULL || (Level->IsPersistentLevel() && World->GetOutermost()->GetFName() == ClientWorldPackageName) ||
			ClientVisibleLevelNames.Contains(Level->GetOutermost()->GetFName()) );
}

void UNetConnection::ValidateSendBuffer()
{
	if ( SendBuffer.IsError() )
	{
		UE_LOG( LogNetTraffic, Fatal, TEXT( "UNetConnection::ValidateSendBuffer: Out.IsError() == true. NumBits: %i, NumBytes: %i, MaxBits: %i" ), SendBuffer.GetNumBits(), SendBuffer.GetNumBytes(), SendBuffer.GetMaxBits() );
	}
}

void UNetConnection::InitSendBuffer()
{
	check(MaxPacket > 0);
	// Initialize the one outgoing buffer.
	if (MaxPacket * 8 == SendBuffer.GetMaxBits())
	{
		// Reset all of our values to their initial state without a malloc/free
		SendBuffer.Reset();
	}
	else
	{
		// First time initialization needs to allocate the buffer
		SendBuffer = FBitWriter(MaxPacket * 8);
	}

	ResetPacketBitCounts();

	ValidateSendBuffer();
}

void UNetConnection::ReceivedRawPacket( void* InData, int32 Count )
{
	uint8* Data = (uint8*)InData;

	// Handle an incoming raw packet from the driver.
	UE_LOG(LogNetTraffic, Verbose, TEXT("%6.3f: Received %i"), FPlatformTime::Seconds() - GStartTime, Count );
	int32 PacketBytes = Count + PacketOverhead;
	InBytes += PacketBytes;
	Driver->InBytes += PacketBytes;
	Driver->InPackets++;
	if( Count>0 )
	{
		uint8 LastByte = Data[Count-1];
		if( LastByte )
		{
			int32 BitSize = Count*8-1;
			while( !(LastByte & 0x80) )
			{
				LastByte *= 2;
				BitSize--;
			}
			FBitReader Reader( Data, BitSize );
			ReceivedPacket( Reader );
		}
		else 
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "Packet missing trailing 1" ) );
			Close();	// If they have the secret key (or haven't opened the control channel yet) and get here, assume they are being malicious
		}
	}
	else 
	{
		UE_LOG( LogNetTraffic, Error, TEXT( "Received zero-size packet" ) );
		Close();	// If they have the secret key (or haven't opened the control channel yet) and get here, assume they are being malicious
	}
}


void UNetConnection::FlushNet(bool bIgnoreSimulation)
{
	// Update info.
	ValidateSendBuffer();
	LastEnd = FBitWriterMark();
	TimeSensitive = 0;

	// If there is any pending data to send, send it.
	if ( SendBuffer.GetNumBits() || ( Driver->Time-LastSendTime > Driver->KeepAliveTime && !InternalAck && State != USOCK_Closed ) )
	{
		// If sending keepalive packet, still write the packet id
		if ( SendBuffer.GetNumBits() == 0 )
		{
			WriteBitsToSendBuffer( NULL, 0 );		// This will force the packet id to be written
		}

		const int NumBitsPrePadding = SendBuffer.GetNumBits();

		// Make sure packet size is byte-aligned.
		SendBuffer.WriteBit( 1 );
		while( SendBuffer.GetNumBits() & 7 )
		{
			SendBuffer.WriteBit( 0 );
		}
		ValidateSendBuffer();

		NumPaddingBits += SendBuffer.GetNumBits() - NumBitsPrePadding;

		NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(this));

		// Send now.
#if DO_ENABLE_NET_TEST
		// if the connection is closing/being destroyed/etc we need to send immediately regardless of settings
		// because we won't be around to send it delayed
		if (State == USOCK_Closed || IsGarbageCollecting() || bIgnoreSimulation)
		{
			// Checked in FlushNet() so each child class doesn't have to implement this
			if (Driver->IsNetResourceValid())
			{
				LowLevelSend(SendBuffer.GetData(), SendBuffer.GetNumBytes());
			}
		}
		else if( PacketSimulationSettings.PktOrder )
		{
			DelayedPacket& B = *(new(Delayed)DelayedPacket);
			B.Data.AddUninitialized( SendBuffer.GetNumBytes() );
			FMemory::Memcpy( B.Data.GetData(), SendBuffer.GetData(), SendBuffer.GetNumBytes() );

			for( int32 i=Delayed.Num()-1; i>=0; i-- )
			{
				if( FMath::FRand()>0.50 )
				{
					if( !PacketSimulationSettings.PktLoss || FMath::FRand()*100.f > PacketSimulationSettings.PktLoss )
					{
						// Checked in FlushNet() so each child class doesn't have to implement this
						if (Driver->IsNetResourceValid())
						{
							LowLevelSend( (char*)&Delayed[i].Data[0], Delayed[i].Data.Num() );
						}
					}
					Delayed.RemoveAt( i );
				}
			}
		}
		else if( PacketSimulationSettings.PktLag )
		{
			if( !PacketSimulationSettings.PktLoss || FMath::FRand()*100.f > PacketSimulationSettings.PktLoss )
			{
				DelayedPacket& B = *(new(Delayed)DelayedPacket);
				B.Data.AddUninitialized( SendBuffer.GetNumBytes() );
				FMemory::Memcpy( B.Data.GetData(), SendBuffer.GetData(), SendBuffer.GetNumBytes() );
				B.SendTime = FPlatformTime::Seconds() + (double(PacketSimulationSettings.PktLag)  + 2.0f * (FMath::FRand() - 0.5f) * double(PacketSimulationSettings.PktLagVariance))/ 1000.f;
			}
		}
		else if( !PacketSimulationSettings.PktLoss || FMath::FRand()*100.f >= PacketSimulationSettings.PktLoss )
		{
#endif
			// Checked in FlushNet() so each child class doesn't have to implement this
			if (Driver->IsNetResourceValid())
			{
				LowLevelSend( SendBuffer.GetData(), SendBuffer.GetNumBytes() );
			}
#if DO_ENABLE_NET_TEST
			if( PacketSimulationSettings.PktDup && FMath::FRand()*100.f < PacketSimulationSettings.PktDup )
			{
				// Checked in FlushNet() so each child class doesn't have to implement this
				if (Driver->IsNetResourceValid())
				{
					LowLevelSend( (char*)SendBuffer.GetData(), SendBuffer.GetNumBytes() );
				}
			}
		}
#endif
		// Update stuff.
		const int32 Index = OutPacketId & (ARRAY_COUNT(OutLagPacketId)-1);
		OutLagPacketId [Index] = OutPacketId;
		OutLagTime     [Index] = Driver->Time;
		OutPacketId++;
		Driver->OutPackets++;
		LastSendTime = Driver->Time;
		const int32 PacketBytes = SendBuffer.GetNumBytes() + PacketOverhead;
		QueuedBytes += PacketBytes;
		OutBytes += PacketBytes;
		Driver->OutBytes += PacketBytes;
		InitSendBuffer();
	}

	// Move acks around.
	for( int32 i=0; i<QueuedAcks.Num(); i++ )
	{
		ResendAcks.Add(QueuedAcks[i]);
	}
	QueuedAcks.Empty(32);
}

int32 UNetConnection::IsNetReady( bool Saturate )
{
	// Return whether we can send more data without saturation the connection.
	if( Saturate )
		QueuedBytes = -SendBuffer.GetNumBytes();
	return QueuedBytes+SendBuffer.GetNumBytes() <= 0;
}

void UNetConnection::ReadInput( float DeltaSeconds )
{}

void UNetConnection::ReceivedNak( int32 NakPacketId )
{
	// Update pending NetGUIDs
	PackageMap->ReceivedNak(NakPacketId);

	// Tell channels about Nak
	for( int32 i=OpenChannels.Num()-1; i>=0; i-- )
	{
		UChannel* Channel = OpenChannels[i];
		Channel->ReceivedNak( NakPacketId );
		if( Channel->OpenPacketId.InRange(NakPacketId) )
			Channel->ReceivedAcks(); //warning: May destroy Channel.
	}
}

/** 
 * Generates a 32 bit value from some input data blob
 * @BunchData	Data blob used to produce 32 bit value
 * @NumBits		Number of bits in data blob
 * @PacketId	PacketId of the packet used to produce this ack data
*/
static uint32 CalcPingAckData( const uint8* BunchData, const int32 NumBits, const int32 PacketId )
{
	// Simply walk the bunch data, based upon OutPacketId, to get 'good enough' random data for verification
	const int32	BunchBytesFloor = NumBits / 8;
	const int32	PingAckIdx		= ( PacketId % MAX_PACKETID ) / PING_ACK_PACKET_INTERVAL;

	return	( BunchData[( PingAckIdx + 0 ) % BunchBytesFloor] << 24 )	+ ( BunchData[( PingAckIdx + 1 ) % BunchBytesFloor] << 16 ) +
			( BunchData[( PingAckIdx + 2 ) % BunchBytesFloor] << 8 )	+ ( BunchData[( PingAckIdx + 3 ) % BunchBytesFloor] );
}

void UNetConnection::ReceivedPacket( FBitReader& Reader )
{
	AssertValid();

	// Handle PacketId.
	if( Reader.IsError() )
	{
		ensureMsgf(false, TEXT("Packet too small") );
		return;
	}

	ValidateSendBuffer();

	// Update receive time to avoid timeout.
	LastReceiveTime = Driver->Time;

	// Check packet ordering.
	const int32 PacketId = InternalAck ? InPacketId + 1 : MakeRelative(Reader.ReadInt(MAX_PACKETID),InPacketId,MAX_PACKETID);
	if( PacketId > InPacketId )
	{
		const int32 PacketsLost = PacketId - InPacketId - 1;
		
		if ( PacketsLost > 10 )
		{
			UE_LOG( LogNetTraffic, Warning, TEXT( "High single frame packet loss: %i" ), PacketsLost );
		}

		InPacketsLost += PacketsLost;
		Driver->InPacketsLost += PacketsLost;
		InPacketId = PacketId;
	}
	else
	{
		Driver->InOutOfOrderPackets++;
		// Protect against replay attacks
		// We already protect against this for reliable bunches, and unreliable properties
		// The only bunch we would process would be unreliable RPC's, which could allow for replay attacks
		// So rather than add individual protection for unreliable RPC's as well, just kill it at the source, 
		// which protects everything in one fell swoop
		return;		
	}

	// Detect packets on the client, which should trigger PingAck verification
	bool bPendingSendPingAck = Driver->ServerConnection != NULL && (PacketId % PING_ACK_PACKET_INTERVAL) == 0;
	bool bGotPingAckData = false;
	uint32 OutPingAckData = 0;

	bool bSkipAck = false;

	// Disassemble and dispatch all bunches in the packet.
	while( !Reader.AtEnd() && State!=USOCK_Closed )
	{
		// Parse the bunch.
		int32 StartPos = Reader.GetPosBits();
		bool IsAck = !!Reader.ReadBit();
		if ( Reader.IsError() )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "Bunch missing ack flag" ) );
			Close();
			return;
		}

		// Process the bunch.
		if( IsAck )
		{
			LastRecvAckTime = Driver->Time;

			// This is an acknowledgment.
			int32 AckPacketId = MakeRelative(Reader.ReadInt(MAX_PACKETID),OutAckPacketId,MAX_PACKETID);
			if( Reader.IsError() )
			{
				UE_LOG( LogNetTraffic, Error, TEXT( "Bunch missing ack" ) );
				Close();
				return;
			}


			// Detect ack packets on the server, containing PingAckData
			bool bPingAck = Driver->ServerConnection == NULL && (AckPacketId % PING_ACK_PACKET_INTERVAL) == 0;
			bool bHasPingAckData = false;
			uint32 InPingAckData = 0;

			if (bPingAck)
			{
				bHasPingAckData = !!Reader.ReadBit();

				if (bHasPingAckData)
				{
					Reader.Serialize(&InPingAckData, sizeof(uint32));
				}
			}


			// For clientside monitoring of ping, watch out for duplicate acks (they come one tick later than the original ack),
			//	because if you don't, the calculated ping value will be high by one tick
			bool bPotentialDupeAck = false;

			// Resend any old reliable packets that the receiver hasn't acknowledged.
			if( AckPacketId>OutAckPacketId )
			{
				for (int32 NakPacketId = OutAckPacketId + 1; NakPacketId<AckPacketId; NakPacketId++, OutPacketsLost++, Driver->OutPacketsLost++)
				{
					UE_LOG(LogNetTraffic, Verbose, TEXT("   Received virtual nak %i (%.1f)"), NakPacketId, (Reader.GetPosBits()-StartPos)/8.f );
					ReceivedNak( NakPacketId );
				}
				OutAckPacketId = AckPacketId;
			}
			else if( AckPacketId<OutAckPacketId )
			{
				//warning: Double-ack logic makes this unmeasurable.
				//OutOrdAcc++;

				bPotentialDupeAck = true;
			}

			// Update lag.
			int32 Index = AckPacketId & (ARRAY_COUNT(OutLagPacketId)-1);
			if( OutLagPacketId[Index]==AckPacketId )
			{
				float NewLag = Driver->Time - OutLagTime[Index] - (FrameTime/2.f);

				LagAcc += NewLag;
				LagCount++;

				if (PlayerController != NULL)
				{
					// Verify PingAck's, and pass notification up
					if (bPingAck && bHasPingAckData &&
						PingAckDataCache[(AckPacketId % MAX_PACKETID)/PING_ACK_PACKET_INTERVAL] == InPingAckData)
					{
						PlayerController->UpdatePing(NewLag);
					}
					// For clients monitoring their own ping, trigger UpdatePing so long as this is not a duplicate ack
					else if (Driver->ServerConnection != NULL && !bPotentialDupeAck)
					{
						PlayerController->UpdatePing(NewLag);
					}
				}
			}

			if ( PackageMap != NULL )
			{
				PackageMap->ReceivedAck( AckPacketId );
			}

			// Forward the ack to the channel.
			UE_LOG(LogNetTraffic, Verbose, TEXT("   Received ack %i (%.1f)"), AckPacketId, (Reader.GetPosBits()-StartPos)/8.f );
			for( int32 i=OpenChannels.Num()-1; i>=0; i-- )
			{
				UChannel* Channel = OpenChannels[i];
				
				if( Channel->OpenPacketId.Last==AckPacketId ) // Necessary for unreliable "bNetTemporary" channels.
				{
					Channel->OpenAcked = 1;
				}
				
				for( FOutBunch* OutBunch=Channel->OutRec; OutBunch; OutBunch=OutBunch->Next )
				{
					if (OutBunch->bOpen)
					{
						UE_LOG(LogNet, VeryVerbose, TEXT("Channel %i reset Ackd because open is reliable. "), Channel->ChIndex );
						Channel->OpenAcked  = 0; // We have a reliable open bunch, don't let the above code set the OpenAcked state,
												 // it must be set in UChannel::ReceivedAcks to verify all open bunches were received.
					}

					if( OutBunch->PacketId==AckPacketId )
					{
						OutBunch->ReceivedAck = 1;
					}
				}				
				Channel->ReceivedAcks(); //warning: May destroy Channel.
			}
		}
		else
		{
			// Parse the incoming data.
			FInBunch Bunch( this );
			int32 IncomingStartPos		= Reader.GetPosBits();
			uint8 bControl				= Reader.ReadBit();
			Bunch.PacketId				= PacketId;
			Bunch.bOpen					= bControl ? Reader.ReadBit() : 0;
			Bunch.bClose				= bControl ? Reader.ReadBit() : 0;
			Bunch.bDormant				= Bunch.bClose ? Reader.ReadBit() : 0;
			Bunch.bReliable				= Reader.ReadBit();
			Bunch.ChIndex				= Reader.ReadInt( MAX_CHANNELS );
			Bunch.bHasGUIDs				= Reader.ReadBit();
			Bunch.bHasMustBeMappedGUIDs	= Reader.ReadBit();
			Bunch.bPartial				= Reader.ReadBit();

			if ( Bunch.bReliable )
			{
				if ( InternalAck )
				{
					// We can derive the sequence for 100% reliable connections
					Bunch.ChSequence = InReliable[Bunch.ChIndex] + 1;
				}
				else
				{
					// If this is a reliable bunch, use the last processed reliable sequence to read the new reliable sequence
					Bunch.ChSequence = MakeRelative( Reader.ReadInt( MAX_CHSEQUENCE ), InReliable[Bunch.ChIndex], MAX_CHSEQUENCE );
				}
			} 
			else if ( Bunch.bPartial )
			{
				// If this is an unreliable partial bunch, we simply use packet sequence since we already have it
				Bunch.ChSequence = PacketId;
			}
			else
			{
				Bunch.ChSequence = 0;
			}

			Bunch.bPartialInitial = Bunch.bPartial ? Reader.ReadBit() : 0;
			Bunch.bPartialFinal	  = Bunch.bPartial ? Reader.ReadBit() : 0;
			Bunch.ChType       = (Bunch.bReliable||Bunch.bOpen) ? Reader.ReadInt(CHTYPE_MAX) : CHTYPE_None;
			int32 BunchDataBits  = Reader.ReadInt( UNetConnection::MaxPacket*8 );

			if ((Bunch.bClose || Bunch.bOpen) && UE_LOG_ACTIVE(LogNetDormancy,VeryVerbose) )
			{
				UE_LOG(LogNetDormancy, VeryVerbose, TEXT("Received: %s"), *Bunch.ToString());
			}

			if (UE_LOG_ACTIVE(LogNetTraffic,VeryVerbose))
			{
				UE_LOG(LogNetTraffic, VeryVerbose, TEXT("Received: %s"), *Bunch.ToString());
			}

			const int32 HeaderPos = Reader.GetPosBits();

			if( Reader.IsError() )
			{
				UE_LOG( LogNetTraffic, Error, TEXT( "Bunch header overflowed" ) );
				Close();
				return;
			}
			Bunch.SetData( Reader, BunchDataBits );
			if( Reader.IsError() )
			{
				// Bunch claims it's larger than the enclosing packet.
				UE_LOG( LogNetTraffic, Error, TEXT( "Bunch data overflowed (%i %i+%i/%i)" ), IncomingStartPos, HeaderPos, BunchDataBits, Reader.GetNumBits() );
				Close();
				return;
			}

			if (Bunch.bHasGUIDs)
			{
				Driver->NetGUIDInBytes += (BunchDataBits + (HeaderPos - IncomingStartPos)) >> 3;
			}

			if( Bunch.bReliable )
			{
				UE_LOG(LogNetTraffic, Verbose, TEXT("   Reliable Bunch, Channel %i Sequence %i: Size %.1f+%.1f"), Bunch.ChIndex, Bunch.ChSequence, (HeaderPos-IncomingStartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			}
			else
			{
				UE_LOG(LogNetTraffic, Verbose, TEXT("   Unreliable Bunch, Channel %i: Size %.1f+%.1f"), Bunch.ChIndex, (HeaderPos-IncomingStartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			}

			if ( Bunch.bOpen )
			{
				UE_LOG(LogNetTraffic, Verbose, TEXT("   bOpen Bunch, Channel %i Sequence %i: Size %.1f+%.1f"), Bunch.ChIndex, Bunch.ChSequence, (HeaderPos-IncomingStartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			}

			if ( Channels[Bunch.ChIndex] == NULL && ( Bunch.ChIndex != 0 || Bunch.ChType != CHTYPE_Control ) )
			{
				// Can't handle other channels until control channel exists.
				if ( Channels[0] == NULL )
				{
					UE_LOG( LogNetTraffic, Error, TEXT( "UNetConnection::ReceivedPacket: Received non-control bunch before control channel was created. ChIndex: %i, ChType: %i" ), Bunch.ChIndex, Bunch.ChType );
					Close();
					return;
				}
				// on the server, if we receive bunch data for a channel that doesn't exist while we're still logging in,
				// it's either a broken client or a new instance of a previous connection,
				// so reject it
				else if ( PlayerController == NULL && Driver->ClientConnections.Contains( this ) )
				{
					UE_LOG( LogNetTraffic, Error, TEXT( "UNetConnection::ReceivedPacket: Received non-control bunch before player controller was assigned. ChIndex: %i, ChType: %i" ), Bunch.ChIndex, Bunch.ChType );
					Close();
					return;
				}
			}
			// ignore control channel close if it hasn't been opened yet
			if ( Bunch.ChIndex == 0 && Channels[0] == NULL && Bunch.bClose && Bunch.ChType == CHTYPE_Control )
			{
				UE_LOG( LogNetTraffic, Error, TEXT( "UNetConnection::ReceivedPacket: Received control channel close before open" ) );
				Close();
				return;
			}

			// Handle grabbing of PingAckData, for client PingAck verification (need a minimum of 32bits data, for OutPingAckData)
			if (bPendingSendPingAck && !bGotPingAckData && BunchDataBits >= 32 &&
				FMath::Abs(Driver->Time - LastPingAck) >= PING_ACK_DELAY)
			{
				OutPingAckData = CalcPingAckData( Bunch.GetData(), Bunch.GetNumBits(), PacketId );
				bGotPingAckData = true;
				LastPingAck = Driver->Time;
			}

			// Receiving data.
			UChannel* Channel = Channels[Bunch.ChIndex];

			// Ignore if reliable packet has already been processed.
			if ( Bunch.bReliable && Bunch.ChSequence <= InReliable[Bunch.ChIndex] )
			{
				UE_LOG( LogNetTraffic, Log, TEXT( "UNetConnection::ReceivedPacket: Received outdated bunch (Channel %d Current Sequence %i)" ), Bunch.ChIndex, InReliable[Bunch.ChIndex] );
				continue;
			}
			
			// If opening the channel with an unreliable packet, check that it is "bNetTemporary", otherwise discard it
			if( !Channel && !Bunch.bReliable )
			{
				// Unreliable bunches that open channels should be bOpen && (bClose || bPartial)
				// NetTemporary usually means one bunch that is unreliable (bOpen and bClose):	1(bOpen, bClose)
				// But if that bunch export NetGUIDs, it will get split into 2 bunches:			1(bOpen, bPartial) - 2(bClose).
				// (the initial actor bunch itself could also be split into multiple bunches. So bPartial is the right check here)

				const bool ValidUnreliableOpen = Bunch.bOpen && (Bunch.bClose || Bunch.bPartial);
				if (!ValidUnreliableOpen)
				{
					UE_LOG( LogNetTraffic, Warning, TEXT( "      Received unreliable bunch before open (Channel %d Current Sequence %i)" ), Bunch.ChIndex, InReliable[Bunch.ChIndex] );
					// Since we won't be processing this packet, don't ack it
					// We don't want the sender to think this bunch was processed when it really wasn't
					bSkipAck = true;
					continue;
				}
			}

			// Create channel if necessary.
			if( !Channel )
			{
				// Validate channel type.
				if ( !UChannel::IsKnownChannelType( Bunch.ChType ) )
				{
					// Unknown type.
					UE_LOG( LogNetTraffic, Error, TEXT( "UNetConnection::ReceivedPacket: Connection unknown channel type (%i)" ), (int)Bunch.ChType );
					Close();
					return;
				}

				// Reliable (either open or later), so create new channel.
				UE_LOG(LogNetTraffic, Log, TEXT("      Bunch Create %i: ChType %i, bReliable: %i, bPartial: %i, bPartialInitial: %i, bPartialFinal: %i"), Bunch.ChIndex, Bunch.ChType, (int)Bunch.bReliable, (int)Bunch.bPartial, (int)Bunch.bPartialInitial, (int)Bunch.bPartialFinal );
				Channel = CreateChannel( (EChannelType)Bunch.ChType, false, Bunch.ChIndex );

				// Notify the server of the new channel.
				if( !Driver->Notify->NotifyAcceptingChannel( Channel ) )
				{
					// Channel refused, so close it, flush it, and delete it.
					UE_LOG(LogNetDormancy, Verbose, TEXT("      NotifyAcceptingChannel Failed! Channel: %s"), *Channel->Describe() );

					FOutBunch CloseBunch( Channel, 1 );
					check(!CloseBunch.IsError());
					check(CloseBunch.bClose);
					CloseBunch.bReliable = 1;
					Channel->SendBunch( &CloseBunch, 0 );
					FlushNet();
					Channel->ConditionalCleanUp();
					if( Bunch.ChIndex==0 )
					{
						UE_LOG(LogNetTraffic, Log, TEXT("Channel 0 create failed") );
						State = USOCK_Closed;
					}
					continue;
				}
			}

			// Dispatch the raw, unsequenced bunch to the channel.
			bool bLocalSkipAck = false;
			Channel->ReceivedRawBunch( Bunch, bLocalSkipAck ); //warning: May destroy channel.
			if ( bLocalSkipAck )
			{
				bSkipAck = true;
			}
			Driver->InBunches++;

			// Disconnect if we received a corrupted packet from the client (eg server crash attempt).
			if ( !Driver->ServerConnection && ( Bunch.IsCriticalError() || Bunch.IsError() ) )
			{
				UE_LOG( LogNetTraffic, Error, TEXT("Received corrupted packet data from client %s.  Disconnecting."), *LowLevelGetRemoteAddress() );
				State = USOCK_Closed;
			}
		}
	}

	ValidateSendBuffer();

	check( !bSkipAck || !InternalAck );		// 100% reliable connections shouldn't be skipping acks

	// Acknowledge the packet.
	if ( !bSkipAck )
	{
		SendAck(PacketId, true, bGotPingAckData, OutPingAckData);
	}
}

int32 UNetConnection::WriteBitsToSendBuffer( 
	const uint8 *	Bits, 
	const int32		SizeInBits, 
	const uint8 *	ExtraBits, 
	const int32		ExtraSizeInBits,
	EWriteBitsDataType DataType)
{
	ValidateSendBuffer();

	const int32 TotalSizeInBits = SizeInBits + ExtraSizeInBits;

	// Flush if we can't add to current buffer
	if ( TotalSizeInBits > GetFreeSendBufferBits() )
	{
		FlushNet();
	}

	// Remember start position in case we want to undo this write
	// Store this after the possible flush above so we have the correct start position in the case that we do flush
	LastStart = FBitWriterMark( SendBuffer );

	// If this is the start of the queue, make sure to add the packet id
	if ( SendBuffer.GetNumBits() == 0 && !InternalAck )
	{
		SendBuffer.WriteIntWrapped( OutPacketId, MAX_PACKETID );
		ValidateSendBuffer();

		NumPacketIdBits += SendBuffer.GetNumBits();
	}

	// Add the bits to the queue
	if ( SizeInBits )
	{
		SendBuffer.SerializeBits( const_cast< uint8* >( Bits ), SizeInBits );
		ValidateSendBuffer();
	}

	// Add any extra bits
	if ( ExtraSizeInBits )
	{
		SendBuffer.SerializeBits( const_cast< uint8* >( ExtraBits ), ExtraSizeInBits );
		ValidateSendBuffer();
	}

	const int32 RememberedPacketId = OutPacketId;

	switch ( DataType )
	{
		case EWriteBitsDataType::Bunch:
			NumBunchBits += SizeInBits + ExtraSizeInBits;
			break;
		case EWriteBitsDataType::Ack:
			NumAckBits += SizeInBits + ExtraSizeInBits;
			break;
		default:
			break;
	}

	// Flush now if we are full
	if ( GetFreeSendBufferBits() == 0 )
	{
		FlushNet();
	}

	return RememberedPacketId;
}

/** Returns number of bits left in current packet that can be used without causing a flush  */
int64 UNetConnection::GetFreeSendBufferBits()
{
	// If we haven't sent anything yet, make sure to account for the packet header + trailer size
	// Otherwise, we only need to account for trailer size
	const int32 ExtraBits = ( SendBuffer.GetNumBits() > 0 ) ? MAX_PACKET_TRAILER_BITS : MAX_PACKET_HEADER_BITS + MAX_PACKET_TRAILER_BITS;

	const int32 NumberOfFreeBits = SendBuffer.GetMaxBits() - ( SendBuffer.GetNumBits() + ExtraBits );

	check( NumberOfFreeBits >= 0 );

	return NumberOfFreeBits;
}

void UNetConnection::PopLastStart()
{
	NumBunchBits -= SendBuffer.GetNumBits() - LastStart.GetNumBits();
	LastStart.Pop(SendBuffer);
	NETWORK_PROFILER(GNetworkProfiler.PopSendBunch(this));
}

void UNetConnection::PurgeAcks()
{
	for ( int32 i = 0; i < ResendAcks.Num(); i++ )
	{
		SendAck( ResendAcks[i], 0 );
	}

	ResendAcks.Empty(32);
}


void UNetConnection::SendAck( int32 AckPacketId, bool FirstTime/*=1*/, bool bHavePingAckData/*=0*/, uint32 PingAckData/*=0*/ )
{
	ValidateSendBuffer();

	if( !InternalAck )
	{
		if( FirstTime )
		{
			PurgeAcks();
			QueuedAcks.Add(AckPacketId);
		}

		FBitWriter AckData( 32, true );

		AckData.WriteBit( 1 );
		AckData.WriteIntWrapped(AckPacketId, MAX_PACKETID);

		const bool bPingAck = Driver->ServerConnection != NULL && (AckPacketId % PING_ACK_PACKET_INTERVAL) == 0;

		if ( bPingAck )
		{
			AckData.WriteBit( bHavePingAckData );

			if ( bHavePingAckData )
			{
				AckData.Serialize( &PingAckData, sizeof( uint32 ) );
			}
		}

		NETWORK_PROFILER( GNetworkProfiler.TrackSendAck( AckData.GetNumBits() ) );

		WriteBitsToSendBuffer( AckData.GetData(), AckData.GetNumBits(), nullptr, 0, EWriteBitsDataType::Ack );

		AllowMerge = false;

		UE_LOG(LogNetTraffic, Log, TEXT("   Send ack %i"), AckPacketId);
	}
}


int32 UNetConnection::SendRawBunch( FOutBunch& Bunch, bool InAllowMerge )
{
	ValidateSendBuffer();
	check(!Bunch.ReceivedAck);
	check(!Bunch.IsError());
	Driver->OutBunches++;
	TimeSensitive = 1;

	// Build header.
	FBitWriter Header( MAX_BUNCH_HEADER_BITS );
	Header.WriteBit( 0 );
	Header.WriteBit( Bunch.bOpen || Bunch.bClose );
	if( Bunch.bOpen || Bunch.bClose )
	{
		Header.WriteBit( Bunch.bOpen );
		Header.WriteBit( Bunch.bClose );
		if( Bunch.bClose )
		{
			Header.WriteBit( Bunch.bDormant );
		}
	}
	Header.WriteBit( Bunch.bReliable );
	Header.WriteIntWrapped(Bunch.ChIndex, MAX_CHANNELS);
	Header.WriteBit( Bunch.bHasGUIDs );
	Header.WriteBit( Bunch.bHasMustBeMappedGUIDs );
	Header.WriteBit( Bunch.bPartial );

	if ( Bunch.bReliable && !InternalAck )
	{
		Header.WriteIntWrapped(Bunch.ChSequence, MAX_CHSEQUENCE);
	}

	if (Bunch.bPartial)
	{
		Header.WriteBit( Bunch.bPartialInitial );
		Header.WriteBit( Bunch.bPartialFinal );
	}

	if (Bunch.bReliable || Bunch.bOpen)
	{
		Header.WriteIntWrapped(Bunch.ChType, CHTYPE_MAX);
	}
	
	Header.WriteIntWrapped(Bunch.GetNumBits(), UNetConnection::MaxPacket * 8);
	check(!Header.IsError());

	// Remember start position.
	AllowMerge      = InAllowMerge;
	Bunch.Time      = Driver->Time;

	if ((Bunch.bClose || Bunch.bOpen) && UE_LOG_ACTIVE(LogNetDormancy,VeryVerbose) )
	{
		UE_LOG(LogNetDormancy, VeryVerbose, TEXT("Sending: %s"), *Bunch.ToString());
	}

	if (UE_LOG_ACTIVE(LogNetTraffic,VeryVerbose))
	{
		UE_LOG(LogNetTraffic, VeryVerbose, TEXT("Sending: %s"), *Bunch.ToString());
	}

	NETWORK_PROFILER(GNetworkProfiler.PushSendBunch(this, &Bunch, Header.GetNumBits(), Bunch.GetNumBits()));

	// Write the bits to the buffer and remember the packet id used
	Bunch.PacketId = WriteBitsToSendBuffer( Header.GetData(), Header.GetNumBits(), Bunch.GetData(), Bunch.GetNumBits(), EWriteBitsDataType::Bunch );

	UE_LOG(LogNetTraffic, Verbose, TEXT("UNetConnection::SendRawBunch. ChIndex: %d. Bits: %d. PacketId: %d"), Bunch.ChIndex, Bunch.GetNumBits(), Bunch.PacketId );

	if ( PackageMap && Bunch.bHasGUIDs )
	{
		PackageMap->NotifyBunchCommit( Bunch.PacketId, Bunch.ExportNetGUIDs );
	}

	if (Bunch.bHasGUIDs)
	{
		Driver->NetGUIDOutBytes += (Header.GetNumBits() + Bunch.GetNumBits()) >> 3;
	}

	// Verified client ping tracking - caches some semi-random bytes of the packet, for ping validation
	if (Driver->ServerConnection == NULL && Bunch.PacketId != LastPingAckPacketId && (Bunch.PacketId % PING_ACK_PACKET_INTERVAL) == 0 &&
		Bunch.GetNumBits() >= 32)
	{
		const uint32 PingAckData = CalcPingAckData( Bunch.GetData(), Bunch.GetNumBits(), Bunch.PacketId );

		const int32 PingAckIdx = (Bunch.PacketId % MAX_PACKETID) / PING_ACK_PACKET_INTERVAL;

		PingAckDataCache[PingAckIdx] = PingAckData;

		// Multiple bunches get written per-packet, but PingAck only uses the first bunch, so don't process more bunches this PacketId
		LastPingAckPacketId = Bunch.PacketId;
	}

	return Bunch.PacketId;
}

UChannel* UNetConnection::CreateChannel( EChannelType ChType, bool bOpenedLocally, int32 ChIndex )
{
	check(UChannel::IsKnownChannelType(ChType));
	AssertValid();

	// If no channel index was specified, find the first available.
	if( ChIndex==INDEX_NONE )
	{
		int32 FirstChannel = 1;
		// Control channel is hardcoded to live at location 0
		if ( ChType == CHTYPE_Control )
		{
			FirstChannel = 0;
		}

		// If this is a voice channel, use its predefined channel index
		if (ChType == CHTYPE_Voice)
		{
			FirstChannel = VOICE_CHANNEL_INDEX;
		}

		// Search the channel array for an available location
		for( ChIndex=FirstChannel; ChIndex<MAX_CHANNELS; ChIndex++ )
		{
			if( !Channels[ChIndex] )
			{
				break;
			}
		}
		// Fail to create if the channel array is full
		if( ChIndex==MAX_CHANNELS )
		{
			return NULL;
		}
	}

	// Make sure channel is valid.
	check(ChIndex<MAX_CHANNELS);
	check(Channels[ChIndex]==NULL);

	// Create channel.
	UChannel* Channel = NewObject<UChannel>(GetTransientPackage(), UChannel::ChannelClasses[ChType]);
	Channel->Init( this, ChIndex, bOpenedLocally );
	Channels[ChIndex] = Channel;
	OpenChannels.Add(Channel);
	UE_LOG(LogNetTraffic, Log, TEXT("Created channel %i of type %i"), ChIndex, (int32)ChType);

	return Channel;
}

/**
 * @return Finds the voice channel for this connection or NULL if none
 */
UVoiceChannel* UNetConnection::GetVoiceChannel()
{
	return Channels[VOICE_CHANNEL_INDEX] != NULL && Channels[VOICE_CHANNEL_INDEX]->ChType == CHTYPE_Voice ?
		(UVoiceChannel*)Channels[VOICE_CHANNEL_INDEX] : NULL;
}

void UNetConnection::Tick()
{
	AssertValid();

	// Lag simulation.
#if DO_ENABLE_NET_TEST
	if( PacketSimulationSettings.PktLag )
	{
		for( int32 i=0; i < Delayed.Num(); i++ )
		{
			if( FPlatformTime::Seconds() > Delayed[i].SendTime )
			{
				LowLevelSend( (char*)&Delayed[i].Data[0], Delayed[i].Data.Num() );
				Delayed.RemoveAt( i );
				i--;
			}
		}
	}
#endif

	// Get frame time.
	double CurrentTime = FPlatformTime::Seconds();
	FrameTime = CurrentTime - LastTime;
	LastTime = CurrentTime;
	CumulativeTime += FrameTime;
	CountedFrames++;
	if(CumulativeTime > 1.f)
	{
		AverageFrameTime = CumulativeTime / CountedFrames;
		CumulativeTime = 0;
		CountedFrames = 0;
	}

	// Pretend everything was acked, for 100% reliable connections or demo recording.
	if( InternalAck )
	{
		OutAckPacketId = OutPacketId;

		LastReceiveTime = Driver->Time;
		for( int32 i=OpenChannels.Num()-1; i>=0; i-- )
		{
			UChannel* It = OpenChannels[i];
			for( FOutBunch* OutBunch=It->OutRec; OutBunch; OutBunch=OutBunch->Next )
			{
				OutBunch->ReceivedAck = 1;
			}
			It->OpenAcked = 1;
			It->ReceivedAcks();
		}
	}

	// Update stats.
	if( Driver->Time - StatUpdateTime > StatPeriod )
	{
		// Update stats.
		float const RealTime = Driver->Time - StatUpdateTime;
		if( LagCount )
		{
			AvgLag = LagAcc/LagCount;
		}
		BestLag = AvgLag;

		// Init counters.
		LagAcc = 0;
		StatUpdateTime = Driver->Time;
		BestLagAcc = 9999;
		LagCount = 0;
		InPacketsLost = 0;
		OutPacketsLost = 0;
		InBytes = 0;
		OutBytes = 0;
	}

	// Compute time passed since last update.
	float DeltaTime     = Driver->Time - LastTickTime;
	LastTickTime        = Driver->Time;

	// Handle timeouts.
	float Timeout = Driver->InitialConnectTimeout;
	if ( (State!=USOCK_Pending) && (bPendingDestroy || (PlayerController && PlayerController->bShortConnectTimeOut) ) )
	{
		Timeout = bPendingDestroy ? 2.f : Driver->ConnectionTimeout;
	}
	bool bUseTimeout = true;
#if WITH_EDITOR
	// Do not time out in PIE since the server is local.
	bUseTimeout = !(GEditor && GEditor->PlayWorld);
#endif
	if ( bUseTimeout && Driver->Time - LastReceiveTime > Timeout )
	{
		// Timeout.
		FString Error = FString::Printf(TEXT("UNetConnection::Tick: Connection TIMED OUT. Closing connection. Driver: %s, Elapsed: %f, Threshold: %f, RemoteAddr: %s, PC: %s, Owner: %s"),
			*Driver->GetName(),
			Driver->Time - LastReceiveTime,
			Timeout, *LowLevelGetRemoteAddress(true),
			PlayerController ? *PlayerController->GetName() : TEXT("NoPC"),
			OwningActor ? *OwningActor->GetName() : TEXT("No Owner")
			);
		UE_LOG(LogNet, Warning, TEXT("%s"), *Error);
		GEngine->BroadcastNetworkFailure(Driver->GetWorld(), Driver, ENetworkFailure::ConnectionTimeout, Error);
		Close();

		if (Driver == NULL)
		{
            // Possible that the Broadcast above caused someone to kill the net driver, early out
			return;
		}
	}
	else
	{
		// Tick the channels.
		for( int32 i=OpenChannels.Num()-1; i>=0; i-- )
		{
			OpenChannels[i]->Tick();
		}

		for ( auto It = KeepProcessingActorChannelBunchesMap.CreateIterator(); It; ++It )
		{
			if ( It.Value() == NULL || It.Value()->IsPendingKill() )
			{
				It.RemoveCurrent();
				UE_LOG( LogNet, Verbose, TEXT( "UNetConnection::Tick: Removing from KeepProcessingActorChannelBunchesMap before done processing bunches. Num: %i" ), KeepProcessingActorChannelBunchesMap.Num() );
				continue;
			}

			check( It.Value()->ChIndex == -1 );

			if ( It.Value()->ProcessQueuedBunches() )
			{
				// Since we are done processing bunches, we can now actually clean this channel up
				It.Value()->ConditionalCleanUp();

				// Remove the channel from the map
				It.RemoveCurrent();
				UE_LOG( LogNet, VeryVerbose, TEXT( "UNetConnection::Tick: Removing from KeepProcessingActorChannelBunchesMap. Num: %i" ), KeepProcessingActorChannelBunchesMap.Num() );
			}
		}

		// If channel 0 has closed, mark the connection as closed.
		if( Channels[0]==NULL && (OutReliable[0]!=0 || InReliable[0]!=0) )
		{
			State = USOCK_Closed;
		}
	}

	// Flush.
	PurgeAcks();
	if( TimeSensitive || Driver->Time-LastSendTime>Driver->KeepAliveTime )
	{
		FlushNet();
	}

	// Update queued byte count.
	// this should be at the end so that the cap is applied *after* sending (and adjusting QueuedBytes for) any remaining data for this tick
	float DeltaBytes = CurrentNetSpeed * DeltaTime;
	QueuedBytes -= FMath::TruncToInt(DeltaBytes);
	float AllowedLag = 2.f * DeltaBytes;
	if (QueuedBytes < -AllowedLag)
	{
		QueuedBytes = FMath::TruncToInt(-AllowedLag);
	}
}

void UNetConnection::HandleClientPlayer( APlayerController *PC, UNetConnection* NetConnection )
{
	check(Driver->GetWorld());

	// Hook up the Viewport to the new player actor.
	ULocalPlayer*	LocalPlayer = NULL;
	for(FLocalPlayerIterator It(GEngine, Driver->GetWorld());It;++It)
	{
		LocalPlayer = *It;
		break;
	}

	// Detach old player.
	check(LocalPlayer);
	if( LocalPlayer->PlayerController )
	{
		if (LocalPlayer->PlayerController->Role == ROLE_Authority)
		{
			// local placeholder PC while waiting for connection to be established
			LocalPlayer->PlayerController->GetWorld()->DestroyActor(LocalPlayer->PlayerController);
		}
		else
		{
			// tell the server the swap is complete
			// we cannot use a replicated function here because the server has already transferred ownership and will reject it
			// so use a control channel message
			int32 Index = INDEX_NONE;
			FNetControlMessage<NMT_PCSwap>::Send(this, Index);
		}
		LocalPlayer->PlayerController->Player = NULL;
		LocalPlayer->PlayerController->NetConnection = NULL;
		LocalPlayer->PlayerController = NULL;
	}

	LocalPlayer->CurrentNetSpeed = CurrentNetSpeed;

	// Init the new playerpawn.
	PC->Role = ROLE_AutonomousProxy;
	PC->SetPlayer(LocalPlayer);
	PC->NetConnection = NetConnection;
	UE_LOG(LogNet, Verbose, TEXT("%s setplayer %s"),*PC->GetName(),*LocalPlayer->GetName());
	LastReceiveTime = Driver->Time;
	State = USOCK_Open;
	PlayerController = PC;
	OwningActor = PC;

	UWorld* World = PlayerController->GetWorld();
	// if we have already loaded some sublevels, tell the server about them
	for (int32 i = 0; i < World->StreamingLevels.Num(); ++i)
	{
		ULevelStreaming* LevelStreaming = World->StreamingLevels[i];
		if (LevelStreaming != NULL)
		{
			const ULevel* Level = LevelStreaming->GetLoadedLevel();
			if ( Level != NULL && Level->bIsVisible && !Level->bClientOnlyVisible )
			{
				// Remap packagename for PIE networking before sending out to server
				FName PackageName = Level->GetOutermost()->GetFName();
				FString PackageNameStr = PackageName.ToString();
				if (GEngine->NetworkRemapPath(Driver->GetWorld(), PackageNameStr, false))
				{
					PackageName = FName(*PackageNameStr);
				}

				PC->ServerUpdateLevelVisibility( PackageName, true );
			}
		}
	}

	// if we have splitscreen viewports, ask the server to join them as well
	bool bSkippedFirst = false;
	for (FLocalPlayerIterator It(GEngine, Driver->GetWorld()); It; ++It)
	{
		if (*It != LocalPlayer)
		{
			// send server command for new child connection
			It->SendSplitJoin();
		}
	}
}

void UChildConnection::HandleClientPlayer(APlayerController* PC, UNetConnection* NetConnection)
{
	// find the first player that doesn't already have a connection
	ULocalPlayer* NewPlayer = NULL;
	uint8 CurrentIndex = 0;
	for (FLocalPlayerIterator It(GEngine, Driver->GetWorld()); It; ++It, CurrentIndex++)
	{
		if (CurrentIndex == PC->NetPlayerIndex)
		{
			NewPlayer = *It;
			break;
		}
	}

	if (NewPlayer == NULL)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogNet, Error, TEXT("Failed to find LocalPlayer for received PlayerController '%s' with index %d. PlayerControllers:"), *PC->GetName(), int32(PC->NetPlayerIndex));
		check( PC->GetWorld() );
		for (TActorIterator<APlayerController> It(PC->GetWorld()); It; ++It)
		{
			if (It->Role < ROLE_Authority)
			{
				UE_LOG(LogNet, Log, TEXT(" - %s"), *It->GetFullName());
			}
		}
		UE_LOG(LogNet, Fatal,TEXT("Failed to find LocalPlayer for received PlayerController"));
#else
		return; // avoid crash
#endif
	}

	// Detach old player.
	check(NewPlayer);
	if (NewPlayer->PlayerController != NULL)
	{
		if (NewPlayer->PlayerController->Role == ROLE_Authority)
		{
			// local placeholder PC while waiting for connection to be established
			NewPlayer->PlayerController->GetWorld()->DestroyActor(NewPlayer->PlayerController);
		}
		else
		{
			// tell the server the swap is complete
			// we cannot use a replicated function here because the server has already transferred ownership and will reject it
			// so use a control channel message
			int32 Index = Parent->Children.Find(this);
			FNetControlMessage<NMT_PCSwap>::Send(Parent, Index);
		}
		NewPlayer->PlayerController->Player = NULL;
		NewPlayer->PlayerController->NetConnection = NULL;
		NewPlayer->PlayerController = NULL;
	}

	NewPlayer->CurrentNetSpeed = CurrentNetSpeed;

	// Init the new playerpawn.
	PC->Role = ROLE_AutonomousProxy;
	PC->SetPlayer(NewPlayer);
	PC->NetConnection = NetConnection;
	UE_LOG(LogNet, Verbose, TEXT("%s setplayer %s"), *PC->GetName(), *NewPlayer->GetName());
	PlayerController = PC;
	OwningActor = PC;
}

#if DO_ENABLE_NET_TEST

void UNetConnection::UpdatePacketSimulationSettings(void)
{
	check(Driver);
	PacketSimulationSettings.PktLoss = Driver->PacketSimulationSettings.PktLoss;
	PacketSimulationSettings.PktOrder = Driver->PacketSimulationSettings.PktOrder;
	PacketSimulationSettings.PktDup = Driver->PacketSimulationSettings.PktDup;
	PacketSimulationSettings.PktLag = Driver->PacketSimulationSettings.PktLag;
	PacketSimulationSettings.PktLagVariance = Driver->PacketSimulationSettings.PktLagVariance;
}
#endif

/**
 * Called to determine if a voice packet should be replicated to this
 * connection or any of its child connections
 *
 * @param Sender - the sender of the voice packet
 *
 * @return true if it should be sent on this connection, false otherwise
 */
bool UNetConnection::ShouldReplicateVoicePacketFrom(const FUniqueNetId& Sender)
{
	if (PlayerController &&
		// Has the handshaking of the mute list completed?
		PlayerController->MuteList.bHasVoiceHandshakeCompleted)
	{	
		// Check with the owning player controller first.
		if (Sender.IsValid() &&
			// Determine if the server should ignore replication of voice packets that are already handled by a peer connection
			//(!Driver->AllowPeerVoice || !Actor->HasPeerConnection(Sender)) &&
			// Determine if the sender was muted for the local player 
			PlayerController->IsPlayerMuted(Sender) == false)
		{
			// The parent wants to allow, but see if any child connections want to mute
			for (int32 Index = 0; Index < Children.Num(); Index++)
			{
				if (Children[Index]->ShouldReplicateVoicePacketFrom(Sender) == false)
				{
					// A child wants to mute, so skip
					return false;
				}
			}
			// No child wanted to block it so accept
			return true;
		}
	}
	// Not able to handle voice yet or player is muted on this connection
	return false;
}

bool UNetConnection::ActorIsAvailableOnClient(const AActor* ThisActor)
{
	return (ActorChannels.Contains(ThisActor) || DormantActors.Contains(ThisActor) || RecentlyDormantActors.Contains(ThisActor));
}

void UNetConnection::ResetGameWorldState()
{
	//Clear out references and do whatever else so that nothing holds onto references that it doesn't need to.
	DestroyedStartupOrDormantActors.Empty();
	RecentlyDormantActors.Empty();
	DormantActors.Empty();
	ClientVisibleLevelNames.Empty();
	KeepProcessingActorChannelBunchesMap.Empty();
	DormantReplicatorMap.Empty();

	CleanupDormantActorState();
}

void UNetConnection::CleanupDormantActorState()
{
	DormantReplicatorMap.Empty();
}

void UNetConnection::FlushDormancy( class AActor* Actor )
{
	UE_LOG( LogNetDormancy, Verbose, TEXT( "FlushDormancy: %s. Connection: %s" ), *Actor->GetName(), *GetName() );
	
	// Remove actor from dormant list
	if ( DormantActors.Remove( Actor ) > 0 )
	{
		FlushDormancyForObject( Actor );

		for ( UActorComponent* ActorComp : Actor->GetReplicatedComponents() )
		{
			if ( ActorComp && ActorComp->GetIsReplicated() )
			{
				FlushDormancyForObject( ActorComp );
			}
		}

		UE_LOG( LogNetDormancy, Verbose, TEXT( "    Found in DormantActors list!" ) );

		RecentlyDormantActors.Add( Actor );
	}

	// If channel is pending dormancy, cancel it
			
	// If the close bunch was already sent, that is fine, by reseting the dormant flag
	// here, the server will not add the actor to the dormancy list when he closes the channel 
	// after he gets the client ack. The result is the channel will close but be open again
	// right away
	UActorChannel * Ch = ActorChannels.FindRef(Actor);

	if ( Ch != NULL )
	{
		UE_LOG( LogNetDormancy, Verbose, TEXT( "    Found Channel[%d] '%s'. Reseting Dormancy. Ch->Closing: %d" ), Ch->ChIndex, *Ch->Describe(), Ch->Closing );

		Ch->Dormant = 0;
		Ch->bPendingDormancy = 0;
	}

}

/** Wrapper for validating an objects dormancy state, and to prepare the object for replication again */
void UNetConnection::FlushDormancyForObject( UObject* Object )
{
	static const auto ValidateCVar = IConsoleManager::Get().FindTConsoleVariableDataInt( TEXT( "net.DormancyValidate" ) );
	const bool ValidateProperties = ( ValidateCVar && ValidateCVar->GetValueOnGameThread() == 1 );

	TSharedRef< FObjectReplicator > * Replicator = DormantReplicatorMap.Find( Object );

	if ( Replicator != NULL )
	{
		if ( ValidateProperties )
		{
			Replicator->Get().ValidateAgainstState( Object );
		}

		DormantReplicatorMap.Remove( Object );

		// Set to NULL to force a new replicator to be created using the objects current state
		// It's totally possible to let this replicator fall through, and continue on where we left off 
		// which could send all the changed properties since this object went dormant
		Replicator = NULL;	
	}

	if ( Replicator == NULL )
	{
		Replicator = &DormantReplicatorMap.Add( Object, TSharedRef<FObjectReplicator>( new FObjectReplicator() ) );

		Replicator->Get().InitWithObject( Object, this, false );		// Init using the objects current state

		// Flush the must be mapped GUIDs, the initialization may add them, but they're phantom and will be remapped when actually sending
		UPackageMapClient * PackageMapClient = CastChecked< UPackageMapClient >(PackageMap);

		if (PackageMapClient)
		{
			TArray< FNetworkGUID >& MustBeMappedGuidsInLastBunch = PackageMapClient->GetMustBeMappedGuidsInLastBunch();
			MustBeMappedGuidsInLastBunch.Empty();
		}	
	}
}

/** Wrapper for setting the current client login state, so we can trap for debugging, and verbosity purposes. */
void UNetConnection::SetClientLoginState( const EClientLoginState::Type NewState )
{
	if ( ClientLoginState == NewState )
	{
		UE_LOG(LogNet, Verbose, TEXT("UNetConnection::SetClientLoginState: State same: %s"), EClientLoginState::ToString( NewState ) );
		return;
	}

	UE_LOG(LogNet, Verbose, TEXT("UNetConnection::SetClientLoginState: State changing from %s to %s"), EClientLoginState::ToString( ClientLoginState ), EClientLoginState::ToString( NewState ) );
	ClientLoginState = NewState;
}

/** Wrapper for setting the current expected client login msg type. */
void UNetConnection::SetExpectedClientLoginMsgType( const uint8 NewType )
{
	if ( ExpectedClientLoginMsgType == NewType )
	{
		UE_LOG(LogNet, Verbose, TEXT("UNetConnection::SetExpectedClientLoginMsgType: Type same: %i"), NewType );
		return;
	}

	UE_LOG(LogNet, Verbose, TEXT("UNetConnection::SetExpectedClientLoginMsgType: Type changing from %i to %i"), ExpectedClientLoginMsgType, NewType );
	ExpectedClientLoginMsgType = NewType;
}

/** This function validates that ClientMsgType is the next expected msg type. */
bool UNetConnection::IsClientMsgTypeValid( const uint8 ClientMsgType )
{
	if ( ClientLoginState == EClientLoginState::LoggingIn )
	{
		// If client is logging in, we are expecting a certain msg type each step of the way
		if ( ClientMsgType != ExpectedClientLoginMsgType )
		{
			// Not the expected msg type
			UE_LOG(LogNet, Log, TEXT("UNetConnection::IsClientMsgTypeValid FAILED: (ClientMsgType != ExpectedClientLoginMsgType) Remote Address=%s"), *LowLevelGetRemoteAddress());
			return false;
		}
	} 
	else
	{
		// Once a client is logged in, we no longer expect any of the msg types below
		if ( ClientMsgType == NMT_Hello || ClientMsgType == NMT_Login )
		{
			// We don't want to see these msg types once the client is fully logged in
			UE_LOG(LogNet, Log, TEXT("UNetConnection::IsClientMsgTypeValid FAILED: Invalid msg after being logged in - Remote Address=%s"), *LowLevelGetRemoteAddress());
			return false;
		}
	}

	return true;
}

/**
* This function tracks the number of log calls per second for this client, 
* and disconnects the client if it detects too many calls are made per second
*/
bool UNetConnection::TrackLogsPerSecond()
{
	const double NewTime = FPlatformTime::Seconds();

	const double LogCallTotalTime = NewTime - LogCallLastTime;

	LogCallCount++;

	static const double LOG_AVG_THRESHOLD				= 0.5;		// Frequency to check threshold
	static const double	MAX_LOGS_PER_SECOND_INSTANT		= 60;		// If they hit this limit, they will instantly get disconnected
	static const double	MAX_LOGS_PER_SECOND_SUSTAINED	= 5;		// If they sustain this logs/second for a certain count, they get disconnected
	static const double	MAX_SUSTAINED_COUNT				= 10;		// If they sustain MAX_LOGS_PER_SECOND_SUSTAINED for this count, they get disconnected (5 seconds currently)

	if ( LogCallTotalTime > LOG_AVG_THRESHOLD )
	{
		const double LogsPerSecond = (double)LogCallCount / LogCallTotalTime;

		LogCallLastTime = NewTime;
		LogCallCount	= 0;

		if ( LogsPerSecond > MAX_LOGS_PER_SECOND_INSTANT )
		{
			// Hit this instant limit, we instantly disconnect them
			UE_LOG( LogNet, Warning, TEXT( "UNetConnection::TrackLogsPerSecond instant FAILED. LogsPerSecond: %f, RemoteAddr: %s" ), (float)LogsPerSecond, *LowLevelGetRemoteAddress() );
			Close();		// Close the connection
			return false;
		}

		if ( LogsPerSecond > MAX_LOGS_PER_SECOND_SUSTAINED )
		{
			// Hit the sustained limit, count how many times we get here
			LogSustainedCount++;

			// Warn that we are approaching getting disconnected (will be useful when going over historical logs)
			UE_LOG( LogNet, Warning, TEXT( "UNetConnection::TrackLogsPerSecond: LogsPerSecond > MAX_LOGS_PER_SECOND_SUSTAINED. LogSustainedCount: %i, LogsPerSecond: %f, RemoteAddr: %s" ), LogSustainedCount, (float)LogsPerSecond, *LowLevelGetRemoteAddress() );

			if ( LogSustainedCount > MAX_SUSTAINED_COUNT )
			{
				// Hit the sustained limit for too long, disconnect them
				UE_LOG( LogNet, Warning, TEXT( "UNetConnection::TrackLogsPerSecond: LogSustainedCount > MAX_SUSTAINED_COUNT. LogsPerSecond: %f, RemoteAddr: %s" ), (float)LogsPerSecond, *LowLevelGetRemoteAddress() );
				Close();		// Close the connection
				return false;
			}
		}
		else
		{
			// Reset sustained count since they are not above the threshold
			LogSustainedCount = 0;
		}
	}

	return true;
}

void UNetConnection::ResetPacketBitCounts()
{
	NumPacketIdBits = 0;
	NumBunchBits = 0;
	NumAckBits = 0;
	NumPaddingBits = 0;
}