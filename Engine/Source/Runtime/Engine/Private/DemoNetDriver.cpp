// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UDemoNetDriver.cpp: Simulated network driver for recording and playing back game sessions.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/DemoNetConnection.h"
#include "Engine/DemoPendingNetGame.h"
#include "Engine/ActorChannel.h"
#include "Engine/PackageMapClient.h"
#include "RepLayout.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/LevelStreamingKismet.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "Engine/GameInstance.h"
#include "NetworkReplayStreaming.h"
#include "Net/UnrealNetwork.h"
#include "Net/NetworkProfiler.h"
#include "Net/DataReplication.h"

DEFINE_LOG_CATEGORY( LogDemo );

static TAutoConsoleVariable<float> CVarDemoRecordHz( TEXT( "demo.RecordHz" ), 10, TEXT( "Number of demo frames recorded per second" ) );
static TAutoConsoleVariable<float> CVarDemoTimeDilation( TEXT( "demo.TimeDilation" ), -1.0f, TEXT( "Override time dilation during demo playback (-1 = don't override)" ) );
static TAutoConsoleVariable<float> CVarDemoSkipTime( TEXT( "demo.SkipTime" ), 0, TEXT( "Skip fixed amount of network replay time (in seconds)" ) );
static TAutoConsoleVariable<int32> CVarEnableCheckpoints( TEXT( "demo.EnableCheckpoints" ), 1, TEXT( "Whether or not checkpoints save on the server" ) );
static TAutoConsoleVariable<int32> CVarGotoCheckpointIndex( TEXT( "demo.GotoCheckpointIndex" ), -2, TEXT( "For testing only, jump to a particular checkpoint" ) );
static TAutoConsoleVariable<float> CVarGotoTimeInSeconds( TEXT( "demo.GotoTimeInSeconds" ), -1, TEXT( "For testing only, jump to a particular time" ) );
static TAutoConsoleVariable<int32> CVarDemoFastForwardDestroyTearOffActors( TEXT( "demo.FastForwardDestroyTearOffActors" ), 1, TEXT( "If true, the driver will destroy any torn-off actors immediately while fast-forwarding a replay." ) );
static TAutoConsoleVariable<int32> CVarDemoFastForwardSkipRepNotifies( TEXT( "demo.FastForwardSkipRepNotifies" ), 1, TEXT( "If true, the driver will optimize fast-forwarding by deferring calls to RepNotify functions until the fast-forward is complete. " ) );
static TAutoConsoleVariable<int32> CVarDemoQueueCheckpointChannels( TEXT( "demo.QueueCheckpointChannels" ), 1, TEXT( "If true, the driver will put all channels created during checkpoint loading into queuing mode, to amortize the cost of spawning new actors across multiple frames." ) );

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
		bChannelsArePaused		= false;
		TimeToSkip				= 0.0f;
		bIsFastForwarding		= false;
		GotoCheckpointSkipExtraTimeInMS = -1;
		QueuedGotoTimeInSeconds	= -1.0f;
		bIsLoadingCheckpoint	= false;
		InitialLiveDemoTime		= 0;
		bWasStartStreamingSuccessful = true;

		ResetDemoState();

		ReplayStreamer = FNetworkReplayStreaming::Get().GetFactory().CreateReplayStreamer();

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

enum ENetworkVersionHistory
{
	HISTORY_INITIAL				= 1,
	HISTORY_SAVE_ABS_TIME_MS	= 2,			// We now save the abs demo time in ms for each frame (solves accumulation errors)
	HISTORY_INCREASE_BUFFER		= 3,			// Increased buffer size of packets, which invalidates old replays
	HISTORY_SAVE_ENGINE_VERSION	= 4				// Now saving engine net version + InternalProtocolVersion
};

static const uint32 NETWORK_DEMO_MAGIC				= 0x2CF5A13D;
static const uint32 NETWORK_DEMO_VERSION			= HISTORY_SAVE_ENGINE_VERSION;

static const uint32 NETWORK_DEMO_METADATA_MAGIC		= 0x3D06B24E;
static const uint32 NETWORK_DEMO_METADATA_VERSION	= 0;

struct FNetworkDemoHeader
{
	uint32	Magic;						// Magic to ensure we're opening the right file.
	uint32	Version;					// Version number to detect version mismatches.
	uint32	InternalProtocolVersion;	// Version of the engine internal network format
	uint32	EngineNetVersion;			// Version of engine networking format
	FString LevelName;					// Name of level loaded for demo
	
	FNetworkDemoHeader() : 
		Magic( NETWORK_DEMO_MAGIC ), 
		Version( NETWORK_DEMO_VERSION ),
		InternalProtocolVersion( FNetworkVersion::InternalProtocolVersion ),
		EngineNetVersion( GEngineNetVersion )
	{}

	friend FArchive& operator << ( FArchive& Ar, FNetworkDemoHeader& Header )
	{
		Ar << Header.Magic;

		// Check magic value
		if ( Header.Magic != NETWORK_DEMO_MAGIC )
		{
			UE_LOG( LogDemo, Error, TEXT( "Header.Magic != NETWORK_DEMO_MAGIC" ) );
			Ar.SetError();
			return Ar;
		}

		Ar << Header.Version;

		// Check version
		if ( Header.Version != NETWORK_DEMO_VERSION )
		{
			UE_LOG( LogDemo, Error, TEXT( "Header.Version != NETWORK_DEMO_VERSION" ) );
			Ar.SetError();
			return Ar;
		}

		// Check internal version
		Ar << Header.InternalProtocolVersion;

		if ( Header.InternalProtocolVersion != FNetworkVersion::InternalProtocolVersion )
		{
			UE_LOG( LogDemo, Error, TEXT( "Header.InternalProtocolVersion != FNetworkVersion::InternalProtocolVersion" ) );
			Ar.SetError();
			return Ar;
		}

		Ar << Header.EngineNetVersion;
		Ar << Header.LevelName;

		return Ar;
	}
};

struct FNetworkDemoMetadataHeader
{
	uint32	Magic;					// Magic to ensure we're opening the right file.
	uint32	Version;				// Version number to detect version mismatches.
	uint32	EngineNetVersion;		// Version of engine networking format
	int32	NumFrames;				// Number of total frames in the demo
	float	TotalTime;				// Number of total time in seconds in demo
	int32	NumStreamingLevels;		// Number of streaming levels

	FNetworkDemoMetadataHeader() : 
		Magic( NETWORK_DEMO_METADATA_MAGIC ), 
		Version( NETWORK_DEMO_METADATA_VERSION ),
		EngineNetVersion( GEngineNetVersion ),
		NumFrames( 0 ),
		TotalTime( 0 ),
		NumStreamingLevels( 0 )
	{}
	
	friend FArchive& operator << ( FArchive& Ar, FNetworkDemoMetadataHeader& Header )
	{
		Ar << Header.Magic;
		Ar << Header.Version;
		Ar << Header.NumFrames;
		Ar << Header.TotalTime;
		Ar << Header.NumStreamingLevels;

		return Ar;
	}
};

void UDemoNetDriver::ResetDemoState()
{
	DemoFrameNum	= 0;
	LastRecordTime	= 0;
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

	// handle default initialization
	if ( !InitBase( true, InNotify, ConnectURL, false, Error ) )
	{
		GetWorld()->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( TEXT( "InitBase FAILED" ) ) );
		return false;
	}

	GuidCache->SetIgnorePackageMismatchOverride( true );

	// Playback, local machine is a client, and the demo stream acts "as if" it's the server.
	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), UDemoNetConnection::StaticClass());
	ServerConnection->InitConnection( this, USOCK_Pending, ConnectURL, 1000000 );

	bWasStartStreamingSuccessful = true;
	// Friendly name isn't important for loading an existing replay.
	ReplayStreamer->StartStreaming( DemoFilename, FString(), false, FNetworkVersion::GetReplayVersion(), FOnStreamReadyDelegate::CreateUObject( this, &UDemoNetDriver::ReplayStreamingReady ) );

	return bWasStartStreamingSuccessful;
}

bool UDemoNetDriver::InitConnectInternal( FString& Error )
{
	UGameInstance* GameInstance = GetWorld()->GetGameInstance();

	ResetDemoState();

	FArchive* FileAr = ReplayStreamer->GetHeaderArchive();

	if ( !FileAr )
	{
		Error = FString::Printf( TEXT( "Couldn't open demo file %s for reading" ), *DemoFilename );
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::DemoNotFound, FString( EDemoPlayFailure::ToString( EDemoPlayFailure::DemoNotFound ) ) );
		return false;
	}

	FNetworkDemoHeader DemoHeader;

	(*FileAr) << DemoHeader;

	if ( FileAr->IsError() )
	{
		Error = FString( TEXT( "Demo file is corrupt" ) );
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
		GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::Corrupt, Error );
		return false;
	}

	// Create fake control channel
	ServerConnection->CreateChannel( CHTYPE_Control, 1 );

	// Attempt to read metadata if it exists
	FArchive* MetadataAr = ReplayStreamer->GetMetadataArchive();

	FNetworkDemoMetadataHeader MetadataHeader;

	if ( MetadataAr != nullptr )
	{
		(*MetadataAr) << MetadataHeader;

		// Check metadata magic value
		if ( MetadataHeader.Magic != NETWORK_DEMO_METADATA_MAGIC )
		{
			Error = FString( TEXT( "Demo metadata file is corrupt" ) );
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
			GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::Corrupt, Error );
			return false;
		}

		// Check version
		if ( MetadataHeader.Version != NETWORK_DEMO_METADATA_VERSION )
		{
			Error = FString( TEXT( "Demo metadata file version is incorrect" ) );
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::InitConnect: %s" ), *Error );
			GameInstance->HandleDemoPlaybackFailure( EDemoPlayFailure::InvalidVersion, Error );
			return false;
		}

		DemoTotalFrames = MetadataHeader.NumFrames;
		DemoTotalTime	= MetadataHeader.TotalTime;

		UE_LOG( LogDemo, Log, TEXT( "Starting demo playback with full demo and metadata. Filename: %s, Frames: %i, Version %i" ), *DemoFilename, DemoTotalFrames, DemoHeader.Version );
	}
	else
	{
		UE_LOG( LogDemo, Log, TEXT( "Starting demo playback with streaming demo, metadata file not found. Filename: %s, Version %i" ), *DemoFilename, DemoHeader.Version );
	}
	
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

	auto NewPendingNetGame = NewObject<UDemoPendingNetGame>();

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

	// Read meta data, if it exists
	for ( int32 i = 0; i < MetadataHeader.NumStreamingLevels; ++i )
	{
		ULevelStreamingKismet* StreamingLevel = NewObject<ULevelStreamingKismet>(GetWorld(), NAME_None, RF_NoFlags, NULL);

		StreamingLevel->bShouldBeLoaded		= true;
		StreamingLevel->bShouldBeVisible	= true;
		StreamingLevel->bShouldBlockOnLoad	= false;
		StreamingLevel->bInitiallyLoaded	= true;
		StreamingLevel->bInitiallyVisible	= true;

		FString PackageName;
		FString PackageNameToLoad;

		(*MetadataAr) << PackageName;
		(*MetadataAr) << PackageNameToLoad;
		(*MetadataAr) << StreamingLevel->LevelTransform;

		StreamingLevel->PackageNameToLoad = FName( *PackageNameToLoad );
		StreamingLevel->SetWorldAssetByPackageName( FName( *PackageName ) );

		GetWorld()->StreamingLevels.Add( StreamingLevel );

		UE_LOG( LogDemo, Log, TEXT( "  Loading streamingLevel: %s, %s" ), *PackageName, *PackageNameToLoad );
	}

	return true;
}

bool UDemoNetDriver::InitListen( FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error )
{
	if ( !InitBase( false, InNotify, ListenURL, bReuseAddressAndPort, Error ) )
	{
		return false;
	}

	GuidCache->SetIgnorePackageMismatchOverride( true );
	GuidCache->SetShouldUseNetworkChecksum( true );

	check( World != NULL );

	class AWorldSettings * WorldSettings = World->GetWorldSettings(); 

	if ( !WorldSettings )
	{
		Error = TEXT( "No WorldSettings!!" );
		return false;
	}

	// Recording, local machine is server, demo stream acts "as if" it's a client.
	UDemoNetConnection* Connection = NewObject<UDemoNetConnection>();
	Connection->InitConnection( this, USOCK_Open, ListenURL, 1000000 );
	Connection->InitSendBuffer();
	ClientConnections.Add( Connection );

	const TCHAR* FriendlyNameOption = ListenURL.GetOption( TEXT("DemoFriendlyName="), nullptr );

	ReplayStreamer->StartStreaming(
		DemoFilename,
		FriendlyNameOption != nullptr ? FString(FriendlyNameOption) : World->GetMapName(),
		true,
		FNetworkVersion::GetReplayVersion(),
		FOnStreamReadyDelegate::CreateUObject( this, &UDemoNetDriver::ReplayStreamingReady ) );

	FArchive* FileAr = ReplayStreamer->GetHeaderArchive();

	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for writing"), *DemoFilename );//@todo demorec: localize
		return false;
	}

	FNetworkDemoHeader DemoHeader;

	DemoHeader.LevelName = World->GetCurrentLevel()->GetOutermost()->GetName();

	// Write the header
	(*FileAr) << DemoHeader;

	// Spawn the demo recording spectator.
	SpawnDemoRecSpectator( Connection );

	return true;
}

void UDemoNetDriver::TickDispatch(float DeltaSeconds)
{
	Super::TickDispatch( DeltaSeconds );

	if ( ClientConnections.Num() == 0 && ServerConnection == nullptr )
	{
		// Nothing to do
		return;
	}

	if ( ReplayStreamer->GetLastError() != ENetworkReplayError::None )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickFlush: ReplayStreamer ERROR: %s" ), ENetworkReplayError::ToString( ReplayStreamer->GetLastError() ) );
		StopDemo();
		return;
	}

	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickFlush: FileAr == nullptr" ) );
		StopDemo();
		return;
	}

	if ( ClientConnections.Num() > 0 )
	{
		const double StartTime = FPlatformTime::Seconds();

		TickDemoRecord( DeltaSeconds );

		const double EndTime = FPlatformTime::Seconds();

		const double RecordTotalTime = ( EndTime - StartTime );

		MaxRecordTime = FMath::Max( MaxRecordTime, RecordTotalTime );

		AccumulatedRecordTime += RecordTotalTime;

		RecordCountSinceFlush++;

		const double ElapsedTime = EndTime - LastRecordAvgFlush;

		const double AVG_FLUSH_TIME_IN_SECONDS = 2;

		if ( ElapsedTime > AVG_FLUSH_TIME_IN_SECONDS && RecordCountSinceFlush > 0 )
		{
			const float AvgTimeMS = ( AccumulatedRecordTime / RecordCountSinceFlush ) * 1000;
			const float MaxRecordTimeMS = MaxRecordTime * 1000;

			if ( AvgTimeMS > 3.0f || MaxRecordTimeMS > 6.0f )
			{
				UE_LOG( LogDemo, Warning, TEXT( "UDemoNetDriver::TickFlush: SLOW FRAME. Avg: %2.2f, Max: %2.2f, Actors: %i" ), AvgTimeMS, MaxRecordTimeMS, World->NetworkActors.Num() );
			}

			LastRecordAvgFlush		= EndTime;
			AccumulatedRecordTime	= 0;
			MaxRecordTime			= 0;
			RecordCountSinceFlush	= 0;
		}
	}
	else if ( ServerConnection != NULL )
	{
		// Wait until all levels are streamed in
		for ( int32 i = 0; i < World->StreamingLevels.Num(); ++i )
		{
			ULevelStreaming * StreamingLevel = World->StreamingLevels[i];

			if ( StreamingLevel != NULL && StreamingLevel->ShouldBeLoaded() && (!StreamingLevel->IsLevelLoaded() || !StreamingLevel->GetLoadedLevel()->GetOutermost()->IsFullyLoaded() || !StreamingLevel->IsLevelVisible() ) )
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
			SpectatorController->CustomTimeDilation = 1.0f;

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

		if ( World->GetWorldSettings()->Pauser == NULL )
		{
			TickDemoPlayback( DeltaSeconds );
		}
	}
}

void UDemoNetDriver::ProcessRemoteFunction( class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject )
{
	if (ClientConnections.Num() > 0 && ClientConnections[0] != NULL)
	{
		if ((Function->FunctionFlags & FUNC_NetMulticast))
		{
			InternalProcessRemoteFunction(Actor, SubObject, ClientConnections[0], Function, Parameters, OutParms, Stack, IsServer());
		}
	}
}

bool UDemoNetDriver::ShouldClientDestroyTearOffActors() const
{
	if ( CVarDemoFastForwardDestroyTearOffActors.GetValueOnGameThread() != 0 )
	{
		return bIsFastForwarding;
	}

	return false;
}

bool UDemoNetDriver::ShouldSkipRepNotifies() const
{
	if ( CVarDemoFastForwardSkipRepNotifies.GetValueOnGameThread() != 0 )
	{
		return bIsFastForwarding;
	}

	return false;
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
		FArchive* MetadataAr = ReplayStreamer->GetMetadataArchive();

		// Finish writing the metadata
		if ( MetadataAr != NULL && World != NULL )
		{
			DemoTotalFrames = DemoFrameNum;
			DemoTotalTime	= DemoCurrentTime;

			// Get the number of streaming levels so we can update the metadata with the correct info
			int32 NumStreamingLevels = 0;

			for ( int32 i = 0; i < World->StreamingLevels.Num(); ++i )
			{
				if ( World->StreamingLevels[i] != NULL )
				{
					NumStreamingLevels++;
				}
			}

			// Make a header for the metadata
			FNetworkDemoMetadataHeader DemoHeader;

			DemoHeader.NumFrames			= DemoTotalFrames;
			DemoHeader.TotalTime			= DemoTotalTime;
			DemoHeader.NumStreamingLevels	= NumStreamingLevels;

			// Seek to beginning
			MetadataAr->Seek( 0 );

			// Write header
			(*MetadataAr) << DemoHeader;

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

					(*MetadataAr) << PackageName;
					(*MetadataAr) << PackageNameToLoad;
					(*MetadataAr) << World->StreamingLevels[i]->LevelTransform;
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

	ReplayStreamer->StopStreaming();

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
		const bool bShouldHaveChannel =
			Actor->bRelevantForNetworkReplays &&
			!Actor->bTearOff &&
			(!Actor->IsNetStartupActor() || Connection->ClientHasInitializedLevelFor(Actor));

		UActorChannel* Channel = Connection->ActorChannels.FindRef(Actor);

		if (bShouldHaveChannel && Channel == NULL)
		{
			// Create a new channel for this actor.
			Channel = (UActorChannel*)Connection->CreateChannel(CHTYPE_Actor, 1);
			if (Channel != NULL)
			{
				Channel->SetChannelActor(Actor);
			}
		}

		if (Channel != NULL && !Channel->Closing)
		{
			// Send it out!
			if (Channel->IsNetReady(0))
			{
				Channel->ReplicateActor();
			}
			
			// Close the channel if this actor shouldn't have one
			if (!bShouldHaveChannel)
			{
				Channel->Close();
			}
		}
	}
}

static void SerializeGuidCache( TSharedPtr< class FNetGUIDCache > GuidCache, FArchive* CheckpointArchive )
{
	int32 NumValues = 0;

	for ( auto It = GuidCache->ObjectLookup.CreateIterator(); It; ++It )
	{
		if ( It.Value().Object == NULL )
		{
			continue;
		}

		if ( !It.Value().Object->IsNameStableForNetworking() )
		{
			continue;
		}

		NumValues++;
	}

	*CheckpointArchive << NumValues;

	UE_LOG( LogDemo, Verbose, TEXT( "Checkpoint. SerializeGuidCache: %i" ), NumValues );

	for ( auto It = GuidCache->ObjectLookup.CreateIterator(); It; ++It )
	{
		if ( It.Value().Object == NULL )
		{
			continue;
		}

		if ( !It.Value().Object->IsNameStableForNetworking() )
		{
			continue;
		}

		FString PathName = It.Value().Object->GetName();

		*CheckpointArchive << It.Key();
		*CheckpointArchive << It.Value().OuterGUID;
		*CheckpointArchive << PathName;
		*CheckpointArchive << It.Value().NetworkChecksum;
		*CheckpointArchive << It.Value().PackageChecksum;

		uint8 Flags = 0;
		
		Flags |= It.Value().bNoLoad ? ( 1 << 0 ) : 0;
		Flags |= It.Value().bIgnoreWhenMissing ? ( 1 << 1 ) : 0;

		*CheckpointArchive << Flags;
	}
}

void UDemoNetDriver::SaveCheckpoint()
{
	FArchive* CheckpointArchive = ReplayStreamer->GetCheckpointArchive();

	if ( CheckpointArchive == nullptr )
	{
		// This doesn't mean error, it means the streamer isn't ready to save checkpoints
		return;
	}

	check( CheckpointArchive->TotalSize() == 0 );

	const double StartCheckpointTime = FPlatformTime::Seconds();

	// First, save the current guid cache
	SerializeGuidCache( GuidCache, CheckpointArchive );

	const uint32 GuidCacheSize = CheckpointArchive->TotalSize();

	// Save total absolute demo time in MS
	uint32 SavedAbsTimeMS = GetDemoCurrentTimeInMS();
	*CheckpointArchive << SavedAbsTimeMS;

	FURL CheckpointURL;
	CheckpointURL.Map = TEXT( "Checkpoint" );

	UDemoNetConnection* CheckpointConnection = NewObject<UDemoNetConnection>();
	ClientConnections.Add( CheckpointConnection );
	CheckpointConnection->InitConnection( this, USOCK_Open, CheckpointURL, 1000000 );
	CheckpointConnection->InitSendBuffer();

	// Some hackery to make the player thinks this checkpoint connection owns it
	CheckpointConnection->PlayerController					= ClientConnections[0]->PlayerController;
	CheckpointConnection->PlayerController->Player			= CheckpointConnection;
	CheckpointConnection->PlayerController->NetConnection	= CheckpointConnection;
	//CheckpointConnection->OwningActor						= CheckpointConnection->PlayerController;

	// Make sure we have the exact same actor channel indexes
	for ( auto It = ClientConnections[0]->ActorChannels.CreateIterator(); It; ++It )
	{
		UActorChannel* Channel = (UActorChannel*)CheckpointConnection->CreateChannel( CHTYPE_Actor, true, It.Value()->ChIndex );
		if ( Channel != NULL )
		{
			Channel->SetChannelActor( It.Value()->Actor );
		}
	}

	bSavingCheckpoint = true;

	// Replicate *only* the actors that were in the previous frame, we want to be able to re-create up to that point with this single checkpoint
	// It's important that we don't catch any new actors that the next frame will also catch, that will cause conflict with bOpen (the open will occur twice on the same channel)
	if ( CheckpointConnection->ActorChannels.Contains( World->GetWorldSettings() ) )
	{
		DemoReplicateActor( World->GetWorldSettings(), CheckpointConnection, false );
	}

	for ( int32 i = 0; i < World->NetworkActors.Num(); i++ )
	{
		AActor* Actor = World->NetworkActors[i];

		if ( CheckpointConnection->ActorChannels.Contains( Actor ) )
		{
			Actor->PreReplication( *FindOrCreateRepChangedPropertyTracker( Actor ).Get() );
			DemoReplicateActor( Actor, CheckpointConnection, false );
		}
	}

	CheckpointConnection->FlushNet();

	bSavingCheckpoint = false;

	// Write a count of 0 to signal the end of the frame
	int32 EndCount = 0;
	*CheckpointArchive << EndCount;

	// Undo hackery
	ClientConnections[0]->PlayerController->Player			= ClientConnections[0];
	ClientConnections[0]->PlayerController->NetConnection	= ClientConnections[0];

	CheckpointConnection->Close();
	CheckpointConnection->CleanUp();

	const uint32 CheckpointSize = CheckpointArchive->TotalSize() - GuidCacheSize;

	const int32 TotalSize = CheckpointArchive->TotalSize();

	if ( CheckpointArchive->TotalSize() > 0 )
	{
		ReplayStreamer->FlushCheckpoint( SavedAbsTimeMS );
	}

	const double EndCheckpointTime = FPlatformTime::Seconds();

	const float CheckpointTimeInMS = ( EndCheckpointTime - StartCheckpointTime ) * 1000.0f;

	UE_LOG( LogDemo, Log, TEXT( "Checkpoint. Total: %i, Rep size: %i, PackageMap: %u, Time: %2.2f" ), TotalSize, CheckpointSize, GuidCacheSize, CheckpointTimeInMS );
}

void UDemoNetDriver::TickDemoRecord( float DeltaSeconds )
{
	if ( ClientConnections.Num() == 0 )
	{
		return;
	}

	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr == NULL )
	{
		return;
	}

	DemoCurrentTime += DeltaSeconds;

	ReplayStreamer->UpdateTotalDemoTime( GetDemoCurrentTimeInMS() );

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

	// Save total absolute demo time in MS
	uint32 SavedAbsTimeMS = GetDemoCurrentTimeInMS();
	*FileAr << SavedAbsTimeMS;

#if DEMO_CHECKSUMS == 1
	uint32 DeltaTimeChecksum = FCrc::MemCrc32( &SavedAbsTimeMS, sizeof( SavedAbsTimeMS ), 0 );
	*FileAr << DeltaTimeChecksum;
#endif

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

	// Save a checkpoint if it's time
	if ( CVarEnableCheckpoints.GetValueOnGameThread() == 1 )
	{
		const double CHECKPOINT_DELAY = 30.0;

		if ( CurrentSeconds - LastCheckpointTime > CHECKPOINT_DELAY )
		{
			SaveCheckpoint();
			LastCheckpointTime = CurrentSeconds;
		}
	}
}

void UDemoNetDriver::PauseChannels( const bool bPause )
{
	if ( bPause == bChannelsArePaused )
	{
		return;
	}

	// Pause all non player controller actors
	// FIXME: Would love a more elegant way of handling this at a more global level
	for ( int32 i = ServerConnection->OpenChannels.Num() - 1; i >= 0; i-- )
	{
		UChannel* OpenChannel = ServerConnection->OpenChannels[i];

		UActorChannel* ActorChannel = Cast< UActorChannel >( OpenChannel );

		if ( ActorChannel == NULL )
		{
			continue;
		}

		ActorChannel->CustomTimeDilation = bPause ? 0.0f : 1.0f;

		if ( ActorChannel->GetActor() == SpectatorController )
		{
			continue;
		}

		if ( ActorChannel->GetActor() == NULL )
		{
			continue;
		}

		// Better way to pause each actor?
		ActorChannel->GetActor()->CustomTimeDilation = ActorChannel->CustomTimeDilation;
	}

	bChannelsArePaused = bPause;
}

bool UDemoNetDriver::ConditionallyReadDemoFrame()
{
	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr->IsError() )
	{
		StopDemo();
		return false;
	}

	if ( FileAr->AtEnd() )
	{
		bDemoPlaybackDone = true;
		PauseChannels( true );
		return false;
	}

	if ( ReplayStreamer->GetLastError() != ENetworkReplayError::None )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ConditionallyReadDemoFrame: ReplayStreamer ERROR: %s" ), ENetworkReplayError::ToString( ReplayStreamer->GetLastError() ) );
		StopDemo();
		return false;
	}

	if ( !ReplayStreamer->IsDataAvailable() )
	{
		PauseChannels( true );
		return false;
	} 

	const int32 OldFilePos = FileAr->Tell();

	uint32 SavedAbsTimeMS = 0;

	// Peek at the next demo delta time, and see if we should process this frame
	*FileAr << SavedAbsTimeMS;

#if DEMO_CHECKSUMS == 1
	{
		uint32 ServerDeltaTimeCheksum = 0;
		*FileAr << ServerDeltaTimeCheksum;

		const uint32 DeltaTimeChecksum = FCrc::MemCrc32( &SavedAbsTimeMS, sizeof( SavedAbsTimeMS ), 0 );

		if ( DeltaTimeChecksum != ServerDeltaTimeCheksum )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ConditionallyReadDemoFrame: DeltaTimeChecksum != ServerDeltaTimeCheksum" ) );
			StopDemo();
			return false;
		}
	}
#endif

	if ( FileAr->IsError() )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ConditionallyReadDemoFrame: Failed to read demo ServerDeltaTime" ) );
		StopDemo();
		return false;
	}

	if ( GetDemoCurrentTimeInMS() < SavedAbsTimeMS )
	{
		// Not enough time has passed to read another frame
		FileAr->Seek( OldFilePos );
		return false;
	}

	return ReadDemoFrame( FileAr );
}

bool UDemoNetDriver::ReadDemoFrame( FArchive* Archive )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ReadDemoFrame time"), STAT_ReplayReadDemoFrameTime, STATGROUP_Net);

	// We're going to read a frame, unpause channels
	PauseChannels( false );

	while ( true )
	{
		uint8 ReadBuffer[ MAX_DEMO_READ_WRITE_BUFFER ];

		int32 PacketBytes;

		*Archive << PacketBytes;

		if ( Archive->IsError() )
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
		Archive->Serialize( ReadBuffer, PacketBytes );

		if ( Archive->IsError() )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: Failed to read demo file packet" ) );
			StopDemo();
			return false;
		}

#if DEMO_CHECKSUMS == 1
		{
			uint32 ServerChecksum = 0;
			*Archive << ServerChecksum;

			const uint32 Checksum = FCrc::MemCrc32( ReadBuffer, PacketBytes, 0 );

			if ( Checksum != ServerChecksum )
			{
				UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrame: Checksum != ServerChecksum" ) );
				StopDemo();
				return false;
			}
		}
#endif

		if ( ServerConnection != NULL )
		{
			// Process incoming packet.
			ServerConnection->ReceivedRawPacket(ReadBuffer, PacketBytes);
		}

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

void UDemoNetDriver::SkipTime(const float InTimeToSkip)
{
	TimeToSkip = InTimeToSkip;
}

void UDemoNetDriver::GotoTimeInSeconds( const float TimeInSeconds )
{
	if ( QueuedGotoTimeInSeconds < 0 )
	{
		QueuedGotoTimeInSeconds = TimeInSeconds;
	}
}

void UDemoNetDriver::JumpToEndOfLiveReplay()
{
	UE_LOG( LogDemo, Log, TEXT( "UDemoNetConnection::JumpToEndOfLiveReplay." ) );

	const uint32 TotalDemoTimeInMS = ReplayStreamer->GetTotalDemoTime();

	DemoTotalTime = (float)TotalDemoTimeInMS / 1000.0f;

	// For now, use no buffer, try to get as close to the end as possible
	// This will likely mean the world is paused at first, but this might be ok considering it will be as live as it can be
	const uint32 BufferInMS = 0;//InitialLiveDemoTime != 0 ? 5 * 1000 : 0;

	const uint32 JoinTimeInMS = ReplayStreamer->GetTotalDemoTime() - BufferInMS;

	ReplayStreamer->GotoTimeInMS( JoinTimeInMS, FOnCheckpointReadyDelegate::CreateUObject( this, &UDemoNetDriver::CheckpointReady ) );

	OldDemoCurrentTime	= DemoCurrentTime;		// So we can restore on failure
	DemoCurrentTime		= (float)( JoinTimeInMS ) / 1000.0f;
	InitialLiveDemoTime = 0;
}

void UDemoNetDriver::TickDemoPlayback( float DeltaSeconds )
{
	if ( ServerConnection == NULL || ServerConnection->State == USOCK_Closed )
	{
		StopDemo();
		return;
	}

	// See if we need to jump to the end (or close to it) of a live game we just joined
	if ( InitialLiveDemoTime > 0 )
	{
		if ( !ReplayStreamer->IsLive() )
		{
			// The replay was finalized while we joined, so don't try to jump to end
			InitialLiveDemoTime = 0;
		}
		else
		{
			const uint32 TotalDemoTimeInMS = ReplayStreamer->GetTotalDemoTime();

			// Wait for the most recent live time
			if ( ReplayStreamer->GetTotalDemoTime() == InitialLiveDemoTime )
			{
				return;
			}

			JumpToEndOfLiveReplay();
		}
	}

	if ( GotoCheckpointArchive == NULL && !ReplayStreamer->IsLoadingCheckpoint() )
	{
		// Check checkpoint debug cvars
		check( GotoCheckpointSkipExtraTimeInMS == -1 );

		if ( CVarGotoCheckpointIndex.GetValueOnGameThread() >= -1 )
		{
			ReplayStreamer->GotoCheckpointIndex( CVarGotoCheckpointIndex.GetValueOnGameThread(), FOnCheckpointReadyDelegate::CreateUObject( this, &UDemoNetDriver::CheckpointReady ) );
			CVarGotoCheckpointIndex.AsVariable()->Set( TEXT( "-2" ), ECVF_SetByConsole );
		}

		if ( CVarGotoTimeInSeconds.GetValueOnGameThread() >= 0.0f )
		{
			ReplayStreamer->GotoTimeInMS( CVarGotoTimeInSeconds.GetValueOnGameThread() * 1000, FOnCheckpointReadyDelegate::CreateUObject( this, &UDemoNetDriver::CheckpointReady ) );
			CVarGotoTimeInSeconds.AsVariable()->Set( TEXT( "-1" ), ECVF_SetByConsole );
		}

		// Process any queued scrub time
		if ( QueuedGotoTimeInSeconds >= 0.0f )
		{
			ReplayStreamer->GotoTimeInMS( QueuedGotoTimeInSeconds * 1000, FOnCheckpointReadyDelegate::CreateUObject( this, &UDemoNetDriver::CheckpointReady ) );
			OldDemoCurrentTime = DemoCurrentTime;		// So we can restore on failure
			DemoCurrentTime = QueuedGotoTimeInSeconds;
			QueuedGotoTimeInSeconds = -1.0f;
		}
	}

	if ( CVarDemoSkipTime.GetValueOnGameThread() > 0 )
	{
		// Just overwrite existing value, cvar wins in this case
		SkipTime( CVarDemoSkipTime.GetValueOnGameThread() );		
		CVarDemoSkipTime.AsVariable()->Set( TEXT( "0" ), ECVF_SetByConsole );
	}

	// Update total demo time
	const uint32 TotalDemoTimeInMS = ReplayStreamer->GetTotalDemoTime();

	if ( TotalDemoTimeInMS > 0 )
	{
		DemoTotalTime = (float)TotalDemoTimeInMS / 1000.0f;
	}

	// See if we have time to skip
	if ( TimeToSkip > 0.0f && !ReplayStreamer->IsLoadingCheckpoint() )
	{
		const uint32 TimeInMSToCheck = FMath::Clamp( GetDemoCurrentTimeInMS() + (uint32)( TimeToSkip * 1000 ), (uint32)0, TotalDemoTimeInMS );

		ReplayStreamer->SetHighPriorityTimeRange( GetDemoCurrentTimeInMS(), TimeInMSToCheck );

		DemoCurrentTime += TimeToSkip;

		bIsFastForwarding = true;

		TimeToSkip = 0.0f;
	}

	// Make sure there is data available to read
	if ( !ReplayStreamer->IsDataAvailable() )
	{
		PauseChannels( true );
		return;
	}

	// Check to see if we have a pending checkpoint to read from
	if ( GotoCheckpointArchive != NULL )
	{
		LoadCheckpoint();
	}

	// If we're at the end of the demo, just pause channels and return
	if ( bDemoPlaybackDone )
	{
		PauseChannels( true );
		return;
	}

	// Advance demo time by seconds passed
	DemoCurrentTime += DeltaSeconds;

	// Clamp time
	if ( DemoCurrentTime > DemoTotalTime )
	{
		DemoCurrentTime = DemoTotalTime;
	}

	// Speculatively grab seconds now in case we need it to get the time it took to fast forward
	const double FastForwardStartSeconds = FPlatformTime::Seconds();

	// Read demo frames until we are caught up (this implicitly handles fast forward if DemoCurrentTime past many frames)
	while ( ConditionallyReadDemoFrame() )
	{
		DemoFrameNum++;
	}

	// Finalize any fast forward stuff that needs to happen
	if ( bIsFastForwarding )
	{
		bIsFastForwarding = false;

		// We may have been fast-forwarding immediately after loading a checkpoint
		// for fine-grained scrubbing. If so, at this point we are no longer loading a checkpoint.
		bIsLoadingCheckpoint = false;

		// Flush all pending RepNotifies that were built up during the fast-forward.
		if ( ServerConnection != nullptr)
		{
			for ( auto& ChannelPair : ServerConnection->ActorChannels )
			{
				if ( ChannelPair.Value != NULL )
				{
					for (auto& ReplicatorPair : ChannelPair.Value->ReplicationMap)
					{
						ReplicatorPair.Value->CallRepNotifies();
					}
				}
			}
		}

		const auto FastForwardTotalSeconds = FPlatformTime::Seconds() - FastForwardStartSeconds;

		UE_LOG( LogDemo, Log, TEXT( "Fast forward took %.2f seconds." ), FastForwardTotalSeconds );
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

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want these to save into a map
	SpectatorController = World->SpawnActor<APlayerController>( C, SpawnInfo );

	if ( SpectatorController == NULL )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::SpawnDemoRecSpectator: Failed to spawn demo spectator." ) );
		return;
	}

	for ( FActorIterator It( World ); It; ++It)
	{
		if ( It->IsA( APlayerStart::StaticClass() ) )
		{
			SpectatorController->SetInitialLocationAndRotation( It->GetActorLocation(), It->GetActorRotation() );
			break;
		}
	}

	SpectatorController->SetReplicates( true );
	SpectatorController->SetAutonomousProxy( true );

	SpectatorController->SetPlayer( Connection );
}

void UDemoNetDriver::ReplayStreamingReady( bool bSuccess, bool bRecord )
{
	bWasStartStreamingSuccessful = bSuccess;

	if ( !bSuccess )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UDemoNetConnection::ReplayStreamingReady: Failed." ) );
		GetWorld()->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::DemoNotFound, FString( EDemoPlayFailure::ToString( EDemoPlayFailure::DemoNotFound ) ) );
		return;
	}

	if ( !bRecord )
	{
		InitialLiveDemoTime = 0;

		FString Error;
		
		const double StartTime = FPlatformTime::Seconds();

		InitConnectInternal( Error );

		if ( ReplayStreamer->IsLive() && ReplayStreamer->GetTotalDemoTime() > 15 * 1000 )
		{
			// If the load time wasn't very long, jump to end now
			// Otherwise, defer it until we have a more recent replay time
			if ( FPlatformTime::Seconds() - StartTime < 10 )
			{
				JumpToEndOfLiveReplay();
			}
			else
			{
				UE_LOG( LogDemo, Log, TEXT( "UDemoNetConnection::ReplayStreamingReady: Deferring checkpoint until next available time." ) );
				InitialLiveDemoTime = ReplayStreamer->GetTotalDemoTime();
			}
		}
	}
}

void UDemoNetDriver::CheckpointReady( const bool bSuccess, const int64 SkipExtraTimeInMS )
{
	check( GotoCheckpointArchive == NULL );
	check( GotoCheckpointSkipExtraTimeInMS == -1 );

	if ( !bSuccess )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UDemoNetConnection::CheckpointReady: Failed to go to checkpoint." ) );
		DemoCurrentTime = OldDemoCurrentTime;
		return;
	}

	GotoCheckpointArchive			= ReplayStreamer->GetCheckpointArchive();
	GotoCheckpointSkipExtraTimeInMS = SkipExtraTimeInMS;
}

void UDemoNetDriver::LoadCheckpoint()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("LoadCheckpoint time"), STAT_ReplayCheckpointLoadTime, STATGROUP_Net);

	if ( GotoCheckpointArchive == NULL )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UDemoNetConnection::LoadCheckpoint: GotoCheckpointArchive == NULL." ) );
		return;
	}

	bIsLoadingCheckpoint = true;

	// Reset the never-queue GUID list, we'll rebuild it
	NonQueuedGUIDsForScrubbing.Empty();

	// Save off the current spectator position
	// Check for NULL, which can be the case if we haven't played any of the demo yet but want to fast forward (joining live game for example)
	if ( SpectatorController != NULL )
	{
		// Save off the SpectatorController's GUID so that we know not to queue his bunches
		AddNonQueuedActorForScrubbing( SpectatorController );
	}

	PauseChannels( false );

#if 1
	// Destroy all non startup actors. They will get restored with the checkpoint
	for ( FActorIterator It( GetWorld() ); It; ++It )
	{
		// If there are any existing actors that are bAlwaysRelevant, don't queue their bunches.
		// Actors that do queue their bunches might not appear immediately after the checkpoint is loaded,
		// and missing bAlwaysRelevant actors are more likely to cause noticeable artifacts.
		// NOTE - We are adding the actor guid here, under the assumption that the actor will reclaim the same guid when we load the checkpoint
		// This is normally the case, but could break if actors get destroyed and re-created with different guids during recording
		if ( It->bAlwaysRelevant )
		{
			AddNonQueuedActorForScrubbing(*It);
		}
		
		if ( *It == SpectatorController )
		{
			continue;
		}

		if ( It->GetOwner() == SpectatorController )
		{
			continue;
		}

		if ( It->IsNetStartupActor() )
		{
			continue;
		}

		GetWorld()->DestroyActor( *It, true );
	}

	// Find the SpectatorController on the channels, and make sure shutting down the connection doesn't destroy this actor
	for ( int32 i = ServerConnection->OpenChannels.Num() - 1; i >= 0; i-- )
	{
		UChannel* OpenChannel = ServerConnection->OpenChannels[i];
		if ( OpenChannel != NULL )
		{
			UActorChannel* ActorChannel = Cast< UActorChannel >( OpenChannel );
			if ( ActorChannel != NULL && ActorChannel->Actor == SpectatorController )
			{
				ActorChannel->Actor = NULL;
			}
		}
	}

	if ( ServerConnection->OwningActor == SpectatorController )
	{
		ServerConnection->OwningActor = NULL;
	}
#else
	for ( int32 i = ServerConnection->OpenChannels.Num() - 1; i >= 0; i-- )
	{
		UChannel* OpenChannel = ServerConnection->OpenChannels[i];
		if ( OpenChannel != NULL )
		{
			UActorChannel* ActorChannel = Cast< UActorChannel >( OpenChannel );
			if ( ActorChannel != NULL && ActorChannel->GetActor() != NULL && !ActorChannel->GetActor()->IsNetStartupActor() )
			{
				GetWorld()->DestroyActor( ActorChannel->GetActor(), true );
			}
		}
	}
#endif

	ServerConnection->Close();
	ServerConnection->CleanUp();

	FURL ConnectURL;
	ConnectURL.Map = DemoFilename;

	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), UDemoNetConnection::StaticClass());
	ServerConnection->InitConnection( this, USOCK_Pending, ConnectURL, 1000000 );

	// Create fake control channel
	ServerConnection->CreateChannel( CHTYPE_Control, 1 );

	// Remember the spectator network guid, so we can persist the spectator player across the checkpoint
	// (so the state and position persists)
	const FNetworkGUID SpectatorGUID = GuidCache->NetGUIDLookup.FindRef( SpectatorController );

	// Clean package map to prepare to restore it to the checkpoint state
	GuidCache->ObjectLookup.Empty();
	GuidCache->NetGUIDLookup.Empty();

	// Restore the spectator controller packagemap entry (so we find it when we process the checkpoint)
	if ( SpectatorGUID.IsValid() )
	{
		FNetGuidCacheObject& CacheObject = GuidCache->ObjectLookup.FindOrAdd( SpectatorGUID );

		CacheObject.Object = SpectatorController;
		check( CacheObject.Object != NULL );
		CacheObject.bNoLoad = true;
		GuidCache->NetGUIDLookup.Add( SpectatorController, SpectatorGUID );
	}

	if ( GotoCheckpointArchive->TotalSize() == 0 || GotoCheckpointArchive->TotalSize() == INDEX_NONE )
	{
		// This is the very first checkpoint, we'll read the stream from the very beginning in this case
		DemoCurrentTime			= 0;
		bDemoPlaybackDone		= false;

		if ( GotoCheckpointSkipExtraTimeInMS != -1 )
		{
			DemoCurrentTime += (float)GotoCheckpointSkipExtraTimeInMS / 1000;
			bIsFastForwarding = true;
		}
		else
		{
			bIsLoadingCheckpoint = false;
		}

		GotoCheckpointArchive			= NULL;
		GotoCheckpointSkipExtraTimeInMS = -1;
		return;
	}

	int32 NumValues = 0;
	*GotoCheckpointArchive << NumValues;

	for ( int32 i = 0; i < NumValues; i++ )
	{
		FNetworkGUID Guid;
		
		*GotoCheckpointArchive << Guid;
		
		FNetGuidCacheObject CacheObject;

		FString PathName;

		*GotoCheckpointArchive << CacheObject.OuterGUID;
		*GotoCheckpointArchive << PathName;
		*GotoCheckpointArchive << CacheObject.NetworkChecksum;
		*GotoCheckpointArchive << CacheObject.PackageChecksum;

		CacheObject.PathName = FName( *PathName );

		uint8 Flags = 0;
		*GotoCheckpointArchive << Flags;

		CacheObject.bNoLoad = ( Flags & ( 1 << 0 ) ) ? true : false;
		CacheObject.bIgnoreWhenMissing = ( Flags & ( 1 << 1 ) ) ? true : false;		

		GuidCache->ObjectLookup.Add( Guid, CacheObject );
	}

	uint32 SavedAbsTimeMS = 0;
	*GotoCheckpointArchive << SavedAbsTimeMS;

	DemoCurrentTime = (float)SavedAbsTimeMS / 1000.0f;

	bIsFastForwarding = true;

	ReadDemoFrame( GotoCheckpointArchive );

	// If we need to skip more time for fine scrubbing, set that up now
	if ( GotoCheckpointSkipExtraTimeInMS != -1 )
	{
		DemoCurrentTime += (float)GotoCheckpointSkipExtraTimeInMS / 1000;
	}
	else
	{
		bIsLoadingCheckpoint = false;
	}

	GotoCheckpointSkipExtraTimeInMS = -1;
	GotoCheckpointArchive			= NULL;
	bDemoPlaybackDone				= false;
}

bool UDemoNetDriver::ShouldQueueBunchesForActorGUID(FNetworkGUID InGUID) const
{
	if ( CVarDemoQueueCheckpointChannels.GetValueOnGameThread() == 0)
	{
		return false;
	}

	// While loading a checkpoint, queue most bunches so that we don't process them all on one frame.
	if (bIsLoadingCheckpoint)
	{
		return !NonQueuedGUIDsForScrubbing.Contains(InGUID);
	}

	return false;
}

void UDemoNetDriver::AddNonQueuedActorForScrubbing(AActor* Actor)
{
	auto FoundChannel = ServerConnection->ActorChannels.Find(Actor);
	if (FoundChannel != nullptr && *FoundChannel != nullptr)
	{
		FNetworkGUID ActorGUID = (*FoundChannel)->ActorNetGUID;
		NonQueuedGUIDsForScrubbing.Add(ActorGUID);
	}
}

/*-----------------------------------------------------------------------------
	UDemoNetConnection.
-----------------------------------------------------------------------------*/

UDemoNetConnection::UDemoNetConnection( const FObjectInitializer& ObjectInitializer ) : Super( ObjectInitializer )
{
	MaxPacket = MAX_DEMO_READ_WRITE_BUFFER;
	InternalAck = true;
}

void UDemoNetConnection::InitConnection( UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed )
{
	// default implementation
	Super::InitConnection( InDriver, InState, InURL, InConnectionSpeed );

	MaxPacket = MAX_DEMO_READ_WRITE_BUFFER;
	InternalAck = true;

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
	if ( GetDriver()->bSavingCheckpoint )
	{
		FArchive* CheckpointArchive = GetDriver()->ReplayStreamer->GetCheckpointArchive();

		if ( CheckpointArchive == nullptr )
		{
			return;
		}
	
		*CheckpointArchive << Count;
		CheckpointArchive->Serialize( Data, Count );
		return;
	}

	if ( Count == 0 )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UDemoNetConnection::LowLevelSend: Ignoring empty packet." ) );
		return;
	}

	if ( Count > MAX_DEMO_READ_WRITE_BUFFER )
	{
		UE_LOG( LogDemo, Fatal, TEXT( "UDemoNetConnection::LowLevelSend: Count > MAX_DEMO_READ_WRITE_BUFFER." ) );
	}

	FArchive* FileAr = GetDriver()->ReplayStreamer->GetStreamingArchive();

	if ( !GetDriver()->ServerConnection && FileAr )
	{
		// If we're outside of an official demo frame, we need to queue this up or it will throw off the stream
		if ( !GetDriver()->bIsRecordingDemoFrame )
		{
			FQueuedDemoPacket & B = *( new( QueuedDemoPackets )FQueuedDemoPacket );
			B.Data.AddUninitialized( Count );
			FMemory::Memcpy( B.Data.GetData(), Data, Count );
			return;
		}

		*FileAr << Count;
		FileAr->Serialize( Data, Count );
		
		NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(this));

#if DEMO_CHECKSUMS == 1
		uint32 Checksum = FCrc::MemCrc32( Data, Count, 0 );
		*FileAr << Checksum;
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
	// If the spectator is the same, assume this is for scrubbing, and we are keeping the old one
	// (so don't set the position, since we want to persist all that)
	if ( GetDriver()->SpectatorController == PC )
	{
		PC->Role			= ROLE_AutonomousProxy;
		PC->NetConnection	= NetConnection;
		LastReceiveTime		= Driver->Time;
		State				= USOCK_Open;
		PlayerController	= PC;
		OwningActor			= PC;
		return;
	}

	Super::HandleClientPlayer( PC, NetConnection );

	// Assume this is our special spectator controller
	GetDriver()->SpectatorController = PC;

	for ( FActorIterator It( Driver->World ); It; ++It)
	{
		if ( It->IsA( APlayerStart::StaticClass() ) )
		{
			PC->SetInitialLocationAndRotation( It->GetActorLocation(), It->GetActorRotation() );
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
