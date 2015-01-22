// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UDemoNetDriver.cpp: Simulated network driver for recording and playing back game sessions.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/DemoNetConnection.h"
#include "Engine/DemoPendingNetGame.h"
#include "Engine/ActorChannel.h"
#include "RepLayout.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/LevelStreamingKismet.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC( LogDemo, Log, All );

static TAutoConsoleVariable<float> CVarDemoRecordHz( TEXT( "demo.RecordHz" ), 10, TEXT( "Number of demo frames recorded per second" ) );
static TAutoConsoleVariable<float> CVarDemoTimeDilation( TEXT( "demo.TimeDilation" ), -1.0f, TEXT( "Override time dilation during demo playback (-1 = don't override)" ) );

static const int32 MAX_DEMO_READ_WRITE_BUFFER = 1024 * 2;

#define DEMO_CHECKSUMS 0		// When setting this to 1, this will invalidate all demos, you will need to re-record and playback

/*-----------------------------------------------------------------------------
	UDemoNetDriver.
-----------------------------------------------------------------------------*/

UDemoNetDriver::UDemoNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UDemoNetDriver::InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error )
{
	if ( Super::InitBase( bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error ) )
	{
		DemoFilename			= URL.Map;
		Time					= 0;
		bIsRecordingDemoFrame	= false;
		bDemoPlaybackDone		= false;
		EndOfStreamOffset		= 0;

		ResetDemoState();

		return true;
	}

	return false;
}

void UDemoNetDriver::FinishDestroy()
{
	if ( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		// Make sure we stop any recording that might be going on
		if ( ClientConnections.Num() > 0 )
		{
			StopDemo();
		}
	}

	Super::FinishDestroy();
}

FString UDemoNetDriver::LowLevelGetNetworkNumber()
{
	return FString( TEXT( "" ) );
}

#define NETWORK_DEMO_MAGIC			( 0x2CF5A13D )
#define NETWORK_DEMO_VERSION		( 0 )

struct FNetworkDemoHeader
{
	uint32	Magic;					// Magic to ensure we're opening the right file.
	uint32	Version;				// Version number to detect version mismatches.
	uint32	EngineNetVersion;		// Version of engine networking format
	FString LevelName;				// Name of level loaded for demo
	int32	NumFrames;				// Number of total frames in the demo
	float	TotalTime;				// Number of total time in seconds in demo
	int32	MetaDataOffset;			// Offset into the file where the meta data is stored for extra information that wasn't known at start of demo
	int32	NumStreamingLevels;		// Number of streaming levels

	FNetworkDemoHeader() : 
		Magic( NETWORK_DEMO_MAGIC ), 
		Version( NETWORK_DEMO_VERSION ),
		EngineNetVersion( GEngineNetVersion ),
		NumFrames( 0 ),
		TotalTime( 0 ),
		MetaDataOffset( 0 ),
		NumStreamingLevels( 0 )
	{}

	friend FArchive& operator << ( FArchive& Ar, FNetworkDemoHeader& Header )
	{
		Ar << Header.Magic;
		Ar << Header.Version;
		Ar << Header.LevelName;
		Ar << Header.NumFrames;
		Ar << Header.TotalTime;
		Ar << Header.MetaDataOffset;
		Ar << Header.NumStreamingLevels;

		return Ar;
	}
};

void UDemoNetDriver::ResetDemoState()
{
	DemoFrameNum	= 0;
	LastRecordTime	= 0;
	DemoDeltaTime	= 0;
	DemoTotalTime	= 0;
	DemoCurrentTime	= 0;
	DemoTotalFrames	= 0;
}

bool UDemoNetDriver::InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
	if ( GetWorld() == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "GetWorld() == nullptr" ) );
		return false;
	}

	if ( GetWorld()->GetGameInstance() == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "GetWorld()->GetGameInstance() == nullptr" ) );
		return false;
	}

	UGameInstance* GameInstance = GetWorld()->GetGameInstance();

	// handle default initialization
	if ( !InitBase( true, InNotify, ConnectURL, false, Error ) )
	{
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( TEXT( "InitBase FAILED" ) ) );
		return false;
	}

	ResetDemoState();

	// open the pre-recorded demo file
	FileAr = IFileManager::Get().CreateFileReader( *DemoFilename );

	if ( !FileAr )
	{
		Error = FString::Printf( TEXT( "Couldn't open demo file %s for reading" ), *DemoFilename );
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::DemoNotFound, FString( EDemoPlayFailure::ToString( EDemoPlayFailure::DemoNotFound ) ) );
		return false;
	}

	// Playback, local machine is a client, and the demo stream acts "as if" it's the server.
	ServerConnection = ConstructObject<UNetConnection>( UDemoNetConnection::StaticClass() );
	ServerConnection->InitConnection( this, USOCK_Pending, ConnectURL, 1000000 );

	// use the same byte format regardless of platform so that the demos are cross platform
	// DEMO_FIXME: This is messing up for some reason, investigate
	//FileAr->SetByteSwapping( true );

	FNetworkDemoHeader DemoHeader;

	(*FileAr) << DemoHeader;

	// Check magic value
	if ( DemoHeader.Magic != NETWORK_DEMO_MAGIC )
	{
		Error = FString( TEXT( "Demo file is corrupt" ) );
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::Corrupt, Error );
		return false;
	}

	// Check version
	if ( DemoHeader.Version != NETWORK_DEMO_VERSION )
	{
		Error = FString( TEXT( "Demo file version is incorrect" ) );
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::InvalidVersion, Error );
		return false;
	}

	// Create fake control channel
	ServerConnection->CreateChannel( CHTYPE_Control, 1 );

	DemoTotalFrames = DemoHeader.NumFrames;
	DemoTotalTime	= DemoHeader.TotalTime;

	UE_LOG( LogDemo, Log, TEXT( "Starting demo playback with demo. Filename: %s, Frames: %i, Version %i" ), *DemoFilename, DemoTotalFrames, DemoHeader.Version );

	// Bypass UDemoPendingNetLevel
	FString LoadMapError;

	FURL DemoURL;
	DemoURL.Map = DemoHeader.LevelName;

	FWorldContext * WorldContext = GEngine->GetWorldContextFromWorld( GetWorld() );

	if ( WorldContext == NULL )
	{
		Error = FString::Printf( TEXT( "No world context" ), *DemoFilename );
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( TEXT( "No world context" ) ) );
		return false;
	}

	GetWorld()->DemoNetDriver = NULL;
	SetWorld( NULL );

	UDemoPendingNetGame * NewPendingNetGame = new UDemoPendingNetGame( FObjectInitializer() );

	NewPendingNetGame->DemoNetDriver = this;

	WorldContext->PendingNetGame = NewPendingNetGame;

	if ( !GEngine->LoadMap( *WorldContext, DemoURL, NewPendingNetGame, LoadMapError ) )
	{
		Error = LoadMapError;
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: LoadMap failed: failed: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( TEXT( "LoadMap failed" ) ) );
		return false;
	}

	SetWorld( WorldContext->World() );
	WorldContext->World()->DemoNetDriver = this;
	WorldContext->PendingNetGame = NULL;

	// Remember where we are
	const int32 OldPos = FileAr->Tell();

	// Jump to meta data
	FileAr->Seek( DemoHeader.MetaDataOffset );

	// Read meta data
	for ( int32 i = 0; i < DemoHeader.NumStreamingLevels; ++i )
	{
		ULevelStreamingKismet* StreamingLevel = static_cast<ULevelStreamingKismet*>(StaticConstructObject(ULevelStreamingKismet::StaticClass(), GetWorld(), NAME_None, RF_NoFlags, NULL ) );

		StreamingLevel->bShouldBeLoaded		= true;
		StreamingLevel->bShouldBeVisible	= true;
		StreamingLevel->bShouldBlockOnLoad	= false;
		StreamingLevel->bInitiallyLoaded	= true;
		StreamingLevel->bInitiallyVisible	= true;

		FString PackageName;
		FString PackageNameToLoad;

		(*FileAr) << PackageName;
		(*FileAr) << PackageNameToLoad;
		(*FileAr) << StreamingLevel->LevelTransform;

		StreamingLevel->PackageNameToLoad = FName( *PackageNameToLoad );
		StreamingLevel->SetWorldAssetByPackageName( FName( *PackageName ) );

		GetWorld()->StreamingLevels.Add( StreamingLevel );

		UE_LOG( LogDemo, Log, TEXT( "  Loading streamingLevel: %s, %s" ), *PackageName, *PackageNameToLoad );
	}

	// Jump back to start of stream
	FileAr->Seek( OldPos );

	// Remember where the meta data is, this is where we must stop reading the demo stream
	EndOfStreamOffset = DemoHeader.MetaDataOffset;

	return true;
}

bool UDemoNetDriver::InitListen( FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error )
{
	if ( !InitBase( false, InNotify, ListenURL, bReuseAddressAndPort, Error ) )
	{
		return false;
	}

	check( World != NULL );

	class AWorldSettings * WorldSettings = World->GetWorldSettings();

	if ( !WorldSettings )
	{
		Error = TEXT( "No WorldSettings!!" );
		return false;
	}

	// Recording, local machine is server, demo stream acts "as if" it's a client.
	UDemoNetConnection* Connection = ConstructObject<UDemoNetConnection>( UDemoNetConnection::StaticClass() );
	Connection->InitConnection( this, USOCK_Open, ListenURL, 1000000 );
	Connection->InitSendBuffer();

	FileAr = IFileManager::Get().CreateFileWriter( *DemoFilename );
	ClientConnections.Add( Connection );

	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for writing"), *DemoFilename );//@todo demorec: localize
		return false;
	}

	// use the same byte format regardless of platform so that the demos are cross platform
	//@note: swap on non console platforms as the console archives have byte swapping compiled out by default
	//FileAr->SetByteSwapping(true);

	FNetworkDemoHeader DemoHeader;

	// NOTE - This must be the SAME string we write when we close the demo or it will throw the header off
	DemoHeader.LevelName = World->GetCurrentLevel()->GetOutermost()->GetName();

	// Write the initial header (a lot of the fields will be placeholder until we fill them in later)
	(*FileAr) << DemoHeader;

	// Spawn the demo recording spectator.
	SpawnDemoRecSpectator( Connection );

	return true;
}

void UDemoNetDriver::TickDispatch( float DeltaSeconds )
{
	Super::TickDispatch( DeltaSeconds );
}

void UDemoNetDriver::TickFlush( float DeltaSeconds )
{
	Super::TickFlush( DeltaSeconds );

	if ( FileAr )
	{
		if ( ClientConnections.Num() > 0 )
		{
			TickDemoRecord( DeltaSeconds );
		}
		else if ( ServerConnection != NULL )
		{
			// Wait until all levels are streamed in
			for ( int32 i = 0; i < World->StreamingLevels.Num(); ++i )
			{
				ULevelStreaming * StreamingLevel = World->StreamingLevels[i];
				if ( StreamingLevel != NULL && ( !StreamingLevel->IsLevelLoaded() || !StreamingLevel->GetLoadedLevel()->GetOutermost()->IsFullyLoaded() || !StreamingLevel->IsLevelVisible() ) )
				{
					// Abort, we have more streaming levels to load
					return;
				}
			}

			if ( CVarDemoTimeDilation.GetValueOnGameThread() >= 0.0f )
			{
				World->GetWorldSettings()->DemoPlayTimeDilation = CVarDemoTimeDilation.GetValueOnGameThread();
			}

			// Clamp time between 1000 hz, and 2 hz 
			// (this is useful when debugging and you set a breakpoint, you don't want all that time to pass in one frame)
			DeltaSeconds = FMath::Clamp( DeltaSeconds, 1.0f / 1000.0f, 1.0f / 2.0f );

			// We need to compensate for the fact that DeltaSeconds is real-time for net drivers
			DeltaSeconds *= World->GetWorldSettings()->GetEffectiveTimeDilation();

			// Update time dilation on spectator pawn to compensate for any demo dilation 
			//	(we want to continue to fly around in real-time)
			if ( SpectatorController != NULL )
			{
				if ( SpectatorController->GetSpectatorPawn() != NULL )
				{
					// Disable collision on the spectator
					SpectatorController->GetSpectatorPawn()->SetActorEnableCollision( false );
					
					SpectatorController->GetSpectatorPawn()->PrimaryActorTick.bTickEvenWhenPaused = true;

					USpectatorPawnMovement* SpectatorMovement = Cast<USpectatorPawnMovement>(SpectatorController->GetSpectatorPawn()->GetMovementComponent());

					if ( SpectatorMovement )
					{
						SpectatorMovement->bIgnoreTimeDilation = true;
						SpectatorMovement->PrimaryComponentTick.bTickEvenWhenPaused = true;
					}
				}
			}

			if ( bDemoPlaybackDone )
			{
				return;
			}

			if ( World->GetWorldSettings()->Pauser == NULL )
			{
				TickDemoPlayback( DeltaSeconds );
			}
		}
	}
}

void UDemoNetDriver::ProcessRemoteFunction( class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject )
{

}

bool UDemoNetDriver::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	return Super::Exec( InWorld, Cmd, Ar);
}

void UDemoNetDriver::StopDemo()
{
	if ( !ServerConnection && ClientConnections.Num() == 0 )
	{
		UE_LOG( LogDemo, Warning, TEXT( "StopDemo: No demo is playing" ) );
		return;
	}

	UE_LOG( LogDemo, Log, TEXT( "StopDemo: Demo %s stopped at frame %d" ), *DemoFilename, DemoFrameNum );

	if ( !ServerConnection )
	{
		// Finish writing the header and other information that goes at the end
		if ( FileAr != NULL && World != NULL )
		{
			DemoTotalFrames = DemoFrameNum;
			DemoTotalTime	= DemoCurrentTime;

			// Get the number of streaming levels so we can update the header with the correct info
			int32 NumStreamingLevels = 0;

			for ( int32 i = 0; i < World->StreamingLevels.Num(); ++i )
			{
				if ( World->StreamingLevels[i] != NULL )
				{
					NumStreamingLevels++;
				}
			}

			// Make a new header with updated info
			FNetworkDemoHeader DemoHeader;

			DemoHeader.LevelName			= World->GetCurrentLevel()->GetOutermost()->GetName();
			DemoHeader.NumFrames			= DemoTotalFrames;
			DemoHeader.TotalTime			= DemoTotalTime;
			DemoHeader.MetaDataOffset		= FileAr->Tell();
			DemoHeader.NumStreamingLevels	= NumStreamingLevels;

			// Seek to beginning
			FileAr->Seek( 0 );

			// Re-write header with new info
			(*FileAr) << DemoHeader;

			// Restore file position to end of stream
			FileAr->Seek( DemoHeader.MetaDataOffset );

			//
			// Write meta data
			//

			// Save out any levels that are in the streamed level list
			// This needs some work, but for now, to try and get games that use heavy streaming working
			for ( int32 i = 0; i < World->StreamingLevels.Num(); ++i )
			{
				if ( World->StreamingLevels[i] != NULL )
				{
					FString PackageName = World->StreamingLevels[i]->GetWorldAssetPackageName();
					FString PackageNameToLoad = World->StreamingLevels[i]->PackageNameToLoad.ToString();

					UE_LOG( LogDemo, Log, TEXT( "  StreamingLevel: %s, %s" ), *PackageName, *PackageNameToLoad );

					(*FileAr) << PackageName;
					(*FileAr) << PackageNameToLoad;
					(*FileAr) << World->StreamingLevels[i]->LevelTransform;
				}
			}
		}

		// let GC cleanup the object
		if ( ClientConnections.Num() > 0 && ClientConnections[0] != NULL )
		{
			ClientConnections[0]->Close();
			ClientConnections[0]->CleanUp(); // make sure DemoRecSpectator gets destroyed immediately
		}
	}
	else
	{
		// flush out any pending network traffic
		ServerConnection->FlushNet();

		ServerConnection->State = USOCK_Closed;
		ServerConnection->Close();
		ServerConnection->CleanUp(); // make sure DemoRecSpectator gets destroyed immediately
		ServerConnection = NULL;
	}

	delete FileAr;
	FileAr = NULL;

	check( ClientConnections.Num() == 0 );
	check( ServerConnection == NULL );
}

/*-----------------------------------------------------------------------------
Demo Recording tick.
-----------------------------------------------------------------------------*/

static void DemoReplicateActor(AActor* Actor, UNetConnection* Connection, bool IsNetClient)
{
	// All actors marked for replication are assumed to be relevant for demo recording.
	/*
	if
		(	Actor
		&&	((IsNetClient && Actor->bTearOff) || Actor->RemoteRole != ROLE_None || (IsNetClient && Actor->Role != ROLE_None && Actor->Role != ROLE_Authority) || Actor->bForceDemoRelevant)
		&&  (!Actor->bNetTemporary || !Connection->SentTemporaries.Contains(Actor))
		&& (Actor == Connection->PlayerController || Cast<APlayerController>(Actor) == NULL)
		)
	*/
	if ( Actor != NULL && Actor->GetRemoteRole() != ROLE_None && ( Actor == Connection->PlayerController || Cast< APlayerController >( Actor ) == NULL ) )
	{
		// Create a new channel for this actor.
		const bool StartupActor = Actor->IsNetStartupActor();
		UActorChannel* Channel = Connection->ActorChannels.FindRef( Actor );

		if ( !Channel && ( !StartupActor || Connection->ClientHasInitializedLevelFor( Actor ) ) )
		{
			// create a channel if possible
			Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
			if ( Channel != NULL )
			{
				Channel->SetChannelActor( Actor );
			}
		}

		if ( Channel )
		{
			// Send it out!
			check( !Channel->Closing );

			if ( Channel->IsNetReady( 0 ) )
			{
				Channel->ReplicateActor();
			}
		}
	}
}

void UDemoNetDriver::TickDemoRecord( float DeltaSeconds )
{
	if ( ClientConnections.Num() == 0 )
	{
		return;
	}

	if ( FileAr == NULL )
	{
		return;
	}

	DemoDeltaTime += DeltaSeconds;
	DemoCurrentTime += DeltaSeconds;

	const double CurrentSeconds = FPlatformTime::Seconds();

	const double RECORD_HZ		= CVarDemoRecordHz.GetValueOnGameThread();
	const double RECORD_DELAY	= 1.0 / RECORD_HZ;

	if ( CurrentSeconds - LastRecordTime < RECORD_DELAY )
	{
		return;		// Not enough real-time has passed to record another frame
	}

	LastRecordTime = CurrentSeconds;

	// Save out a frame
	DemoFrameNum++;
	ReplicationFrame++;

	// Save elapsed game time for this frame
	*FileAr << DemoDeltaTime;

#if DEMO_CHECKSUMS == 1
	uint32 DeltaTimeChecksum = FCrc::MemCrc32( &DemoDeltaTime, sizeof( DemoDeltaTime ), 0 );
	*FileAr << DeltaTimeChecksum;
#endif

	DemoDeltaTime = 0;

	// Make sure we don't have anything in the buffer for this new frame
	check( ClientConnections[0]->SendBuffer.GetNumBits() == 0 );

	bIsRecordingDemoFrame = true;

	// Dump any queued packets
	UDemoNetConnection * ClientDemoConnection = CastChecked< UDemoNetConnection >( ClientConnections[0] );

	for ( int32 i = 0; i < ClientDemoConnection->QueuedDemoPackets.Num(); i++ )
	{
		ClientDemoConnection->LowLevelSend( (char*)&ClientDemoConnection->QueuedDemoPackets[i].Data[0], ClientDemoConnection->QueuedDemoPackets[i].Data.Num() );
	}

	ClientDemoConnection->QueuedDemoPackets.Empty();

	const bool IsNetClient = ( GetWorld()->GetNetDriver() != NULL && GetWorld()->GetNetDriver()->GetNetMode() == NM_Client );

	DemoReplicateActor( World->GetWorldSettings(), ClientConnections[0], IsNetClient );

	for ( int32 i = 0; i < World->NetworkActors.Num(); i++ )
	{
		AActor* Actor = World->NetworkActors[i];

		Actor->PreReplication( *FindOrCreateRepChangedPropertyTracker( Actor ).Get() );
		DemoReplicateActor( Actor, ClientConnections[0], IsNetClient );
	}

	// Make sure nothing is left over
	ClientConnections[0]->FlushNet();

	check( ClientConnections[0]->SendBuffer.GetNumBits() == 0 );

	bIsRecordingDemoFrame = false;

	// Write a count of 0 to signal the end of the frame
	int32 EndCount = 0;

	*FileAr << EndCount;
}

bool UDemoNetDriver::ReadDemoFrame()
{
	if ( FileAr->IsError() )
	{
		StopDemo();
		return false;
	}

	if ( FileAr->AtEnd() || FileAr->Tell() >= EndOfStreamOffset )
	{
		bDemoPlaybackDone = true;

		// Pause all non player controller actors
		for ( int32 i = ServerConnection->OpenChannels.Num() - 1; i >= 0; i-- )
		{
			UChannel* OpenChannel = ServerConnection->OpenChannels[i];
			if ( OpenChannel != NULL )
			{
				UActorChannel* ActorChannel = Cast< UActorChannel >( OpenChannel );
				if ( ActorChannel != NULL && ActorChannel->GetActor() != NULL )
				{
					if ( Cast< APlayerController >( ActorChannel->GetActor() ) == NULL )
					{
						// Better way to pause each actor?
						ActorChannel->GetActor()->CustomTimeDilation = 0.0f;
					}
				}
			}
		}

		return false;
	}

	const int32 OldFilePos = FileAr->Tell();

	float ServerDeltaTime;

	// Peek at the next demo delta time, and see if we should process this frame
	*FileAr << ServerDeltaTime;

#if DEMO_CHECKSUMS == 1
	{
		uint32 ServerDeltaTimeCheksum = 0;
		*FileAr << ServerDeltaTimeCheksum;

		const uint32 DeltaTimeChecksum = FCrc::MemCrc32( &ServerDeltaTime, sizeof( ServerDeltaTime ), 0 );

		if ( DeltaTimeChecksum != ServerDeltaTimeCheksum )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: DeltaTimeChecksum != ServerDeltaTimeCheksum" ) );
			StopDemo();
			return false;
		}
	}
#endif

	if ( FileAr->IsError() )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: Failed to read demo ServerDeltaTime" ) );
		StopDemo();
		return false;
	}

	if ( DemoDeltaTime < ServerDeltaTime )//&& ServerConnection->State != USOCK_Pending )
	{
		// Not enough time has passed to read another frame
		FileAr->Seek( OldFilePos );
		return false;
	}

	DemoDeltaTime -= ServerDeltaTime;

	while ( true )
	{
		uint8 ReadBuffer[ MAX_DEMO_READ_WRITE_BUFFER ];

		int32 PacketBytes;

		*FileAr << PacketBytes;

		if ( FileAr->IsError() )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: Failed to read demo PacketBytes" ) );
			StopDemo();
			return false;
		}

		if ( PacketBytes == 0 )
		{
			break;
		}

		if ( PacketBytes > sizeof( ReadBuffer ) )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: PacketBytes > sizeof( ReadBuffer )" ) );

			StopDemo();

			if ( World != NULL && World->GetGameInstance() != NULL )
			{
				World->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( TEXT( "UDemoNetDriver::ReadDemoFrame: PacketBytes > sizeof( ReadBuffer )" ) ) );
			}

			return false;
		}

		// Read data from file.
		FileAr->Serialize( ReadBuffer, PacketBytes );

		if ( FileAr->IsError() )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: Failed to read demo file packet" ) );
			StopDemo();
			return false;
		}

#if DEMO_CHECKSUMS == 1
		{
			uint32 ServerChecksum = 0;
			*FileAr << ServerChecksum;

			const uint32 Checksum = FCrc::MemCrc32( ReadBuffer, PacketBytes, 0 );

			if ( Checksum != ServerChecksum )
			{
				UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: Checksum != ServerChecksum" ) );
				StopDemo();
				return false;
			}
		}
#endif

		// Process incoming packet.
		ServerConnection->ReceivedRawPacket( ReadBuffer, PacketBytes );

		if ( ServerConnection == NULL || ServerConnection->State == USOCK_Closed )
		{
			// Something we received resulted in the demo being stopped
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: ReceivedRawPacket closed connection" ) );
			StopDemo();
			return false;
		}
	}

	return true;
}

void UDemoNetDriver::TickDemoPlayback( float DeltaSeconds )
{
	if ( ServerConnection == NULL || ServerConnection->State == USOCK_Closed )
	{
		StopDemo();
		return;
	}

	DemoDeltaTime += DeltaSeconds;
	DemoCurrentTime += DeltaSeconds;

	while ( true )
	{
		// Read demo frames until we are caught up
		if ( !ReadDemoFrame() )
		{
			break;
		}

		DemoFrameNum++;
	}
}

void UDemoNetDriver::SpawnDemoRecSpectator( UNetConnection* Connection )
{
	check( Connection != NULL );

	UClass* C = StaticLoadClass( AActor::StaticClass(), NULL, *DemoSpectatorClass, NULL, LOAD_None, NULL );

	if ( C == NULL )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::SpawnDemoRecSpectator: Failed to load demo spectator class." ) );
		return;
	}

	APlayerController* Controller = World->SpawnActor<APlayerController>( C );

	if ( Controller == NULL )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::SpawnDemoRecSpectator: Failed to spawn demo spectator." ) );
		return;
	}

	for ( FActorIterator It( World ); It; ++It)
	{
		if ( It->IsA( APlayerStart::StaticClass() ) )
		{
			Controller->SetInitialLocationAndRotation( It->GetActorLocation(), It->GetActorRotation() );
			break;
		}
	}

	Controller->SetReplicates( true );
	Controller->SetAutonomousProxy( true );

	Controller->SetPlayer( Connection );
}

/*-----------------------------------------------------------------------------
	UDemoNetConnection.
-----------------------------------------------------------------------------*/

UDemoNetConnection::UDemoNetConnection( const FObjectInitializer& ObjectInitializer ) : Super( ObjectInitializer )
{
	MaxPacket = 512;
	InternalAck = true;
}

void UDemoNetConnection::InitConnection( UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed )
{
	// default implementation
	Super::InitConnection( InDriver, InState, InURL, InConnectionSpeed );

	InitSendBuffer();

	// the driver must be a DemoRecording driver (GetDriver makes assumptions to avoid Cast'ing each time)
	check( InDriver->IsA( UDemoNetDriver::StaticClass() ) );
}

FString UDemoNetConnection::LowLevelGetRemoteAddress( bool bAppendPort )
{
	return TEXT( "UDemoNetConnection" );
}

void UDemoNetConnection::LowLevelSend( void* Data, int32 Count )
{
	if ( Count == 0 )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UDemoNetConnection::LowLevelSend: Ignoring empty packet." ) );
		return;
	}

	if ( Count > MAX_DEMO_READ_WRITE_BUFFER )
	{
		UE_LOG( LogDemo, Fatal, TEXT( "UDemoNetConnection::LowLevelSend: Count > MAX_DEMO_READ_WRITE_BUFFER." ) );
	}

	if ( !GetDriver()->ServerConnection && GetDriver()->FileAr )
	{
		// If we're outside of an official demo frame, we need to queue this up or it will throw off the stream
		if ( !GetDriver()->bIsRecordingDemoFrame )
		{
			FQueuedDemoPacket & B = *( new( QueuedDemoPackets )FQueuedDemoPacket );
			B.Data.AddUninitialized( Count );
			FMemory::Memcpy( B.Data.GetData(), Data, Count );
			return;
		}

		*GetDriver()->FileAr << Count;
		GetDriver()->FileAr->Serialize( Data, Count );
		
#if DEMO_CHECKSUMS == 1
		uint32 Checksum = FCrc::MemCrc32( Data, Count, 0 );
		*GetDriver()->FileAr << Checksum;
#endif
	}
}

FString UDemoNetConnection::LowLevelDescribe()
{
	return TEXT( "Demo recording/playback driver connection" );
}

int32 UDemoNetConnection::IsNetReady( bool Saturate )
{
	return 1;
}

void UDemoNetConnection::FlushNet( bool bIgnoreSimulation )
{
	// in playback, there is no data to send except
	// channel closing if an error occurs.
	if ( GetDriver()->ServerConnection != NULL )
	{
		InitSendBuffer();
	}
	else
	{
		Super::FlushNet( bIgnoreSimulation );
	}
}

void UDemoNetConnection::HandleClientPlayer( APlayerController* PC, UNetConnection* NetConnection )
{
	Super::HandleClientPlayer( PC, NetConnection );

	// Assume this is our special spectator controller
	GetDriver()->SpectatorController = PC;

	for ( FActorIterator It( Driver->World ); It; ++It)
	{
		if ( It->IsA( APlayerStart::StaticClass() ) )
		{
			PC->SetInitialLocationAndRotation( It->GetActorLocation(), It->GetActorRotation() );

			if ( PC->GetPawn() )
			{
				PC->GetPawn()->TeleportTo( It->GetActorLocation(), It->GetActorRotation(), false, true );
			}
			break;
		}
	}
}

bool UDemoNetConnection::ClientHasInitializedLevelFor(const UObject* TestObject) const
{
	// We save all currently streamed levels into the demo stream so we can force the demo playback client
	// to stay in sync with the recording server
	// This may need to be tweaked or re-evaluated when we start recording demos on the client
	return ( GetDriver()->DemoFrameNum > 2 || Super::ClientHasInitializedLevelFor( TestObject ) );
}

/*-----------------------------------------------------------------------------
	UDemoPendingNetGame.
-----------------------------------------------------------------------------*/

UDemoPendingNetGame::UDemoPendingNetGame( const FObjectInitializer& ObjectInitializer ) : Super( ObjectInitializer )
{
}
