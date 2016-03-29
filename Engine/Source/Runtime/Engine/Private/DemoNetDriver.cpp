// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UDemoNetDriver.cpp: Simulated network driver for recording and playing back game sessions.
=============================================================================*/


// @todo: LowLevelSend now includes the packet size in bits, but this is ignored locally.
//			Tracking of this must be added, if demos are to support PacketHandler's in the future (not presently needed).


#include "EnginePrivate.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/DemoNetConnection.h"
#include "Engine/DemoPendingNetGame.h"
#include "Engine/ActorChannel.h"
#include "Engine/PackageMapClient.h"
#include "Engine/NetworkObjectList.h"
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
#include "GameFramework/GameMode.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY( LogDemo );

static TAutoConsoleVariable<float> CVarDemoRecordHz( TEXT( "demo.RecordHz" ), 8, TEXT( "Maximum number of demo frames recorded per second" ) );
static TAutoConsoleVariable<float> CVarDemoMinRecordHz(TEXT("demo.MinRecordHz"), 0, TEXT("Minimum number of demo frames recorded per second (use with care)"));
static TAutoConsoleVariable<float> CVarDemoTimeDilation( TEXT( "demo.TimeDilation" ), -1.0f, TEXT( "Override time dilation during demo playback (-1 = don't override)" ) );
static TAutoConsoleVariable<float> CVarDemoSkipTime( TEXT( "demo.SkipTime" ), 0, TEXT( "Skip fixed amount of network replay time (in seconds)" ) );
static TAutoConsoleVariable<int32> CVarEnableCheckpoints( TEXT( "demo.EnableCheckpoints" ), 1, TEXT( "Whether or not checkpoints save on the server" ) );
static TAutoConsoleVariable<float> CVarGotoTimeInSeconds( TEXT( "demo.GotoTimeInSeconds" ), -1, TEXT( "For testing only, jump to a particular time" ) );
static TAutoConsoleVariable<int32> CVarDemoFastForwardDestroyTearOffActors( TEXT( "demo.FastForwardDestroyTearOffActors" ), 1, TEXT( "If true, the driver will destroy any torn-off actors immediately while fast-forwarding a replay." ) );
static TAutoConsoleVariable<int32> CVarDemoFastForwardSkipRepNotifies( TEXT( "demo.FastForwardSkipRepNotifies" ), 1, TEXT( "If true, the driver will optimize fast-forwarding by deferring calls to RepNotify functions until the fast-forward is complete. " ) );
static TAutoConsoleVariable<int32> CVarDemoQueueCheckpointChannels( TEXT( "demo.QueueCheckpointChannels" ), 1, TEXT( "If true, the driver will put all channels created during checkpoint loading into queuing mode, to amortize the cost of spawning new actors across multiple frames." ) );
static TAutoConsoleVariable<int32> CVarUseAdaptiveReplayUpdateFrequency( TEXT( "demo.UseAdaptiveReplayUpdateFrequency" ), 0, TEXT( "If 1, NetUpdateFrequency will be calculated based on how often actors actually write something when recording to a replay" ) );
static TAutoConsoleVariable<int32> CVarDemoAsyncLoadWorld( TEXT( "demo.AsyncLoadWorld" ), 0, TEXT( "If 1, we will use seamless server travel to load the replay world asynchronously" ) );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarDemoForceFailure( TEXT( "demo.ForceFailure" ), 0, TEXT( "" ) );
#endif

static const int32 MAX_DEMO_READ_WRITE_BUFFER = 1024 * 2;

#define DEMO_CHECKSUMS 0		// When setting this to 1, this will invalidate all demos, you will need to re-record and playback

class FJumpToLiveReplayTask : public FQueuedReplayTask
{
public:
	FJumpToLiveReplayTask( UDemoNetDriver* InDriver ) : FQueuedReplayTask( InDriver )
	{
		InitialTotalDemoTime	= Driver->ReplayStreamer->GetTotalDemoTime();
		TaskStartTime			= FPlatformTime::Seconds();
	}

	virtual void StartTask()
	{
	}

	virtual bool Tick() override
	{
		if ( !Driver->ReplayStreamer->IsLive() )
		{
			// The replay is no longer live, so don't try to jump to end
			return true;
		}

		// Wait for the most recent live time
		const bool bHasNewReplayTime = ( Driver->ReplayStreamer->GetTotalDemoTime() != InitialTotalDemoTime );

		// If we haven't gotten a new time from the demo by now, assume it might not be live, and just jump to the end now so we don't hang forever
		const bool bTimeExpired = ( FPlatformTime::Seconds() - TaskStartTime >= 15 );

		if ( bHasNewReplayTime || bTimeExpired )
		{
			if ( bTimeExpired )
			{
				UE_LOG( LogDemo, Warning, TEXT( "FJumpToLiveReplayTask::Tick: Too much time since last live update." ) );
			}

			// We're ready to jump to the end now
			Driver->JumpToEndOfLiveReplay();
			return true;
		}

		// Waiting to get the latest update
		return false;
	}

	virtual FString GetName() override
	{
		return TEXT( "FJumpToLiveReplayTask" );
	}

	uint32	InitialTotalDemoTime;		// Initial total demo time. This is used to wait until we get a more updated time so we jump to the most recent end time
	double	TaskStartTime;				// This is the time the task started. If too much real-time passes, we'll just jump to the current end
};

class FGotoTimeInSecondsTask : public FQueuedReplayTask
{
public:
	FGotoTimeInSecondsTask( UDemoNetDriver* InDriver, const float InTimeInSeconds ) : FQueuedReplayTask( InDriver ), TimeInSeconds( InTimeInSeconds ), GotoCheckpointArchive( nullptr ), GotoCheckpointSkipExtraTimeInMS( -1 )
	{
	}

	virtual void StartTask() override
	{		
		check( !Driver->IsFastForwarding() );

		OldTimeInSeconds		= Driver->DemoCurrentTime;	// Rember current time, so we can restore on failure
		Driver->DemoCurrentTime	= TimeInSeconds;			// Also, update current time so HUD reflects desired scrub time now

		// Tell the streamer to start going to this time
		Driver->ReplayStreamer->GotoTimeInMS( TimeInSeconds * 1000, FOnCheckpointReadyDelegate::CreateRaw( this, &FGotoTimeInSecondsTask::CheckpointReady ) );

		// Pause channels while we wait (so the world is paused while we wait for the new stream location to load)
		Driver->PauseChannels( true );
	}

	virtual bool Tick() override
	{
		if ( GotoCheckpointSkipExtraTimeInMS == -2 )
		{
			// Detect failure case
			return true;
		}

		if ( GotoCheckpointArchive != nullptr )
		{
			if ( GotoCheckpointSkipExtraTimeInMS > 0 && !Driver->ReplayStreamer->IsDataAvailable() )
			{
				// Wait for rest of stream before loading checkpoint
				// We do this so we can load the checkpoint and fastforward the stream all at once
				// We do this so that the OnReps don't stay queued up outside of this frame
				return false;
			}

			// We're done
			Driver->LoadCheckpoint( GotoCheckpointArchive, GotoCheckpointSkipExtraTimeInMS );
			return true;
		}

		return false;
	}

	virtual FString GetName() override
	{
		return TEXT( "FGotoTimeInSecondsTask" );
	}

	void CheckpointReady( const bool bSuccess, const int64 SkipExtraTimeInMS )
	{
		check( GotoCheckpointArchive == NULL );
		check( GotoCheckpointSkipExtraTimeInMS == -1 );

		if ( !bSuccess )
		{
			UE_LOG( LogDemo, Warning, TEXT( "FGotoTimeInSecondsTask::CheckpointReady: Failed to go to checkpoint." ) );

			// Restore old demo time
			Driver->DemoCurrentTime = OldTimeInSeconds;

			// Call delegate if any
			Driver->NotifyGotoTimeFinished(false);

			GotoCheckpointSkipExtraTimeInMS = -2;	// So tick can detect failure case
			return;
		}

		GotoCheckpointArchive			= Driver->ReplayStreamer->GetCheckpointArchive();
		GotoCheckpointSkipExtraTimeInMS = SkipExtraTimeInMS;
	}

	float				OldTimeInSeconds;		// So we can restore on failure
	float				TimeInSeconds;
	FArchive*			GotoCheckpointArchive;
	int64				GotoCheckpointSkipExtraTimeInMS;
};

class FSkipTimeInSecondsTask : public FQueuedReplayTask
{
public:
	FSkipTimeInSecondsTask( UDemoNetDriver* InDriver, const float InSecondsToSkip ) : FQueuedReplayTask( InDriver ), SecondsToSkip( InSecondsToSkip )
	{
	}

	virtual void StartTask() override
	{
		check( !Driver->IsFastForwarding() );

		const uint32 TimeInMSToCheck = FMath::Clamp( Driver->GetDemoCurrentTimeInMS() + ( uint32 )( SecondsToSkip * 1000 ), ( uint32 )0, Driver->ReplayStreamer->GetTotalDemoTime() );

		Driver->ReplayStreamer->SetHighPriorityTimeRange( Driver->GetDemoCurrentTimeInMS(), TimeInMSToCheck );

		Driver->SkipTimeInternal( SecondsToSkip, true, false );
	}

	virtual bool Tick() override
	{
		// The real work was done in StartTask, so we're done
		return true;
	}

	virtual FString GetName() override
	{
		return TEXT( "FSkipTimeInSecondsTask" );
	}

	float SecondsToSkip;
};

/*-----------------------------------------------------------------------------
	UDemoNetDriver.
-----------------------------------------------------------------------------*/

UDemoNetDriver::UDemoNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDemoNetDriver::AddReplayTask( FQueuedReplayTask* NewTask )
{
	UE_LOG( LogDemo, Verbose, TEXT( "UDemoNetDriver::AddReplayTask. Name: %s" ), *NewTask->GetName() );

	QueuedReplayTasks.Add( TSharedPtr< FQueuedReplayTask >( NewTask ) );

	// Give this task a chance to immediately start if nothing else is happening
	if ( !IsAnyTaskPending() )
	{
		ProcessReplayTasks();	
	}
}

bool UDemoNetDriver::IsAnyTaskPending()
{
	return ( QueuedReplayTasks.Num() > 0 ) || ActiveReplayTask.IsValid();
}

void UDemoNetDriver::ClearReplayTasks()
{
	QueuedReplayTasks.Empty();

	ActiveReplayTask = nullptr;
}

bool UDemoNetDriver::ProcessReplayTasks()
{
	if ( !ActiveReplayTask.IsValid() && QueuedReplayTasks.Num() > 0 )
	{
		// If we don't have an active task, pull one off now
		ActiveReplayTask = QueuedReplayTasks[0];
		QueuedReplayTasks.RemoveAt( 0 );

		UE_LOG( LogDemo, Verbose, TEXT( "UDemoNetDriver::ProcessReplayTasks. Name: %s" ), *ActiveReplayTask->GetName() );

		// Start the task
		ActiveReplayTask->StartTask();
	}

	// Tick the currently active task
	if ( ActiveReplayTask.IsValid() )
	{
		if ( !ActiveReplayTask->Tick() )
		{
			// Task isn't done, we can return
			return false;
		}

		// This task is now done
		ActiveReplayTask = nullptr;
	}

	return true;	// No tasks to process
}

bool UDemoNetDriver::IsNamedTaskInQueue( const FString& Name )
{
	if ( ActiveReplayTask.IsValid() && ActiveReplayTask->GetName() == Name )
	{
		return true;
	}

	for ( int32 i = 0; i < QueuedReplayTasks.Num(); i++ )
	{
		if ( QueuedReplayTasks[0]->GetName() == Name )
		{
			return true;
		}
	}

	return false;
}

bool UDemoNetDriver::InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error )
{
	if ( Super::InitBase( bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error ) )
	{
		DemoFilename					= URL.Map;
		Time							= 0;
		bDemoPlaybackDone				= false;
		bChannelsArePaused				= false;
		bIsFastForwarding				= false;
		bIsFastForwardingForCheckpoint	= false;
		bWasStartStreamingSuccessful	= true;
		SavedReplicatedWorldTimeSeconds	= 0.0f;
		SavedSecondsToSkip				= 0.0f;
		bIsLoadingCheckpoint			= false;

		ResetDemoState();

		const TCHAR* const StreamerOverride = URL.GetOption(TEXT("ReplayStreamerOverride="), nullptr);
		ReplayStreamer = FNetworkReplayStreaming::Get().GetFactory(StreamerOverride).CreateReplayStreamer();

		return true;
	}

	return false;
}

void UDemoNetDriver::FinishDestroy()
{
	if ( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		// Make sure we stop any recording/playing that might be going on
		if ( IsRecording() || IsPlaying() )
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
	HISTORY_SAVE_ENGINE_VERSION = 4,			// Now saving engine net version + InternalProtocolVersion
	HISTORY_ADD_GAME_SPECIFIC_DATA = 5,			// add field for game specific data that subclasses can process
};

static const uint32 NETWORK_DEMO_MAGIC				= 0x2CF5A13D;
static const uint32 NETWORK_DEMO_VERSION			= HISTORY_ADD_GAME_SPECIFIC_DATA;

static const uint32 NETWORK_DEMO_METADATA_MAGIC		= 0x3D06B24E;
static const uint32 NETWORK_DEMO_METADATA_VERSION	= 0;

struct FNetworkDemoHeader
{
	uint32	Magic;						// Magic to ensure we're opening the right file.
	uint32	Version;					// Version number to detect version mismatches.
	uint32	InternalProtocolVersion;	// Version of the engine internal network format
	uint32	EngineNetVersion;			// Version of engine networking format
	FString LevelName;					// Name of level loaded for demo
	TArray<FString> GameSpecificData; // area for subclasses to write stuff
	
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

		Ar << Header.GameSpecificData;

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
	DemoFrameNum		= 0;
	LastCheckpointTime	= 0.0f;
	DemoTotalTime		= 0;
	DemoCurrentTime		= 0;
	DemoTotalFrames		= 0;

	ExternalDataToObjectMap.Empty();
	PlaybackPackets.Empty();
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

	TArray< FString > UserNames;

	if ( GetWorld()->GetGameInstance()->GetFirstGamePlayer() != nullptr )
	{
		TSharedPtr<const FUniqueNetId> ViewerId = GetWorld()->GetGameInstance()->GetFirstGamePlayer()->GetPreferredUniqueNetId();

		if ( ViewerId.IsValid() )
		{ 
			UserNames.Add( ViewerId->ToString() );
		}
	}

	bWasStartStreamingSuccessful = true;

	ReplayStreamer->StartStreaming( 
		DemoFilename, 
		FString(),		// Friendly name isn't important for loading an existing replay.
		UserNames, 
		false, 
		FNetworkVersion::GetReplayVersion(), 
		FOnStreamReadyDelegate::CreateUObject( this, &UDemoNetDriver::ReplayStreamingReady ) );

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

	if (!ProcessGameSpecificDemoHeader(DemoHeader.GameSpecificData, Error))
	{
		UE_LOG(LogDemo, Error, TEXT("UDemoNetDriver::InitConnect: (Game Specific) %s"), *Error);
		GameInstance->HandleDemoPlaybackFailure(EDemoPlayFailure::Generic, Error);
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
	
	if ( CVarDemoAsyncLoadWorld.GetValueOnGameThread() > 0 )
	{
		TArray<AController*> Controllers;
		for ( FConstControllerIterator Iterator = World->GetControllerIterator(); Iterator; ++Iterator )
		{
			Controllers.Add( *Iterator );
		}

		for ( int i = 0; i < Controllers.Num(); i++ )
		{
			if ( Controllers[i] )
			{
				Controllers[i]->Destroy();
			}
		}

		// FIXME: Test for failure!!!
		World->SeamlessTravel( DemoHeader.LevelName, true );
	}
	else
	{
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

		bool bSuccess = GEngine->LoadMap( *WorldContext, DemoURL, NewPendingNetGame, LoadMapError );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( CVarDemoForceFailure.GetValueOnGameThread() == 2 )
		{
			bSuccess = false;
		}
#endif

		if ( !bSuccess )
		{
			StopDemo();

			// If we don't have a world that means we failed loading the new world.
			// Since there is no world, we must free the net driver ourselves
			// Technically the pending net game should handle it, but things aren't quite setup properly to handle that either
			if ( WorldContext->World() == NULL )
			{
				GEngine->DestroyNamedNetDriver( WorldContext->PendingNetGame, NetDriverName );
			}

			WorldContext->PendingNetGame = NULL;

			GEngine->BrowseToDefaultMap( *WorldContext );

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
			ULevelStreamingKismet* StreamingLevel = NewObject<ULevelStreamingKismet>( GetWorld(), NAME_None, RF_NoFlags, NULL );

			StreamingLevel->bShouldBeLoaded		= true;
			StreamingLevel->bShouldBeVisible	= true;
			StreamingLevel->bShouldBlockOnLoad	= false;
			StreamingLevel->bInitiallyLoaded	= true;
			StreamingLevel->bInitiallyVisible	= true;

			FString PackageName;
			FString PackageNameToLoad;

			( *MetadataAr ) << PackageName;
			( *MetadataAr ) << PackageNameToLoad;
			( *MetadataAr ) << StreamingLevel->LevelTransform;

			StreamingLevel->PackageNameToLoad = FName( *PackageNameToLoad );
			StreamingLevel->SetWorldAssetByPackageName( FName( *PackageName ) );

			GetWorld()->StreamingLevels.Add( StreamingLevel );

			UE_LOG( LogDemo, Log, TEXT( "  Loading streamingLevel: %s, %s" ), *PackageName, *PackageNameToLoad );
		}
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

	TArray< FString > UserNames;

	// If a client is recording a replay, GameState may not have replicated yet
	if ( GetWorld()->GameState != nullptr )
	{
		for ( int32 i = 0; i < GetWorld()->GameState->PlayerArray.Num(); i++ )
		{
			APlayerState* PlayerState = GetWorld()->GameState->PlayerArray[i];

			if ( !PlayerState->bIsABot && !PlayerState->bIsSpectator )
			{
				UserNames.Add( PlayerState->UniqueId.ToString() );
			}
		}
	}

	ReplayStreamer->StartStreaming(
		DemoFilename,
		FriendlyNameOption != nullptr ? FString(FriendlyNameOption) : World->GetMapName(),
		UserNames,
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

	WriteGameSpecificDemoHeader(DemoHeader.GameSpecificData);

	// Write the header
	(*FileAr) << DemoHeader;
	FileAr->Flush();

	// Spawn the demo recording spectator.
	SpawnDemoRecSpectator( Connection, ListenURL );
	
	for (auto It = GetWorld()->GetNetDriver()->DestroyedStartupOrDormantActors.CreateIterator(); It; ++It)
	{
		UActorChannel* Channel = (UActorChannel*)Connection->CreateChannel(CHTYPE_Actor, 1);
		if (Channel)
		{
			FActorDestructionInfo DestructionInfo = It.Value();

			GuidCache->GetOrAssignNetGUID(DestructionInfo.ObjOuter.Get());

#define COMPOSE_NET_GUID( Index, IsStatic )	( ( ( Index ) << 1 ) | ( IsStatic ) )
#define ALLOC_NEW_NET_GUID( IsStatic )		( COMPOSE_NET_GUID( ++GuidCache->UniqueNetIDs[ IsStatic ], IsStatic ) )

			const int32 IsStatic = 1;
			
			DestructionInfo.NetGUID = FNetworkGUID(ALLOC_NEW_NET_GUID(IsStatic));
			
			Channel->SetChannelActorForDestroy(&DestructionInfo);
		}
	}

	return true;
}

bool UDemoNetDriver::IsRecording() const
{
	return ClientConnections.Num() > 0 && ClientConnections[0] != nullptr && ClientConnections[0]->State != USOCK_Closed;
}

bool UDemoNetDriver::IsPlaying() const
{
	return ServerConnection != nullptr && ServerConnection->State != USOCK_Closed;
}

void UDemoNetDriver::TickFlush( float DeltaSeconds )
{
	Super::TickFlush( DeltaSeconds );

	if ( !IsRecording() )
	{
		// Nothing to do
		return;
	}

	if ( ReplayStreamer->GetLastError() != ENetworkReplayError::None )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickFlush: ReplayStreamer ERROR: %s" ), ENetworkReplayError::ToString( ReplayStreamer->GetLastError() ) );
		const bool bIsPlaying = IsPlaying();
		StopDemo();
		if ( bIsPlaying )
		{
			World->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( EDemoPlayFailure::ToString( EDemoPlayFailure::Generic ) ) );
		}
		return;
	}

	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickFlush: FileAr == nullptr" ) );
		StopDemo();
		return;
	}

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "Net replay record time" ), STAT_ReplayRecordTime, STATGROUP_Net );

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

		if ( AvgTimeMS > 8.0f )//|| MaxRecordTimeMS > 6.0f )
		{
			UE_LOG( LogDemo, Warning, TEXT( "UDemoNetDriver::TickFlush: SLOW FRAME. Avg: %2.2f, Max: %2.2f, Actors: %i" ), AvgTimeMS, MaxRecordTimeMS, GetNetworkObjectList().GetObjects().Num() );
		}

		LastRecordAvgFlush		= EndTime;
		AccumulatedRecordTime	= 0;
		MaxRecordTime			= 0;
		RecordCountSinceFlush	= 0;
	}
}

static float GetClampedDeltaSeconds( UWorld* World, const float DeltaSeconds )
{
	check( World != nullptr );

	const float RealDeltaSeconds = DeltaSeconds;

	// Clamp delta seconds
	const float ClampedDeltaSeconds = World->GetWorldSettings()->FixupDeltaSeconds( DeltaSeconds * World->GetWorldSettings()->GetEffectiveTimeDilation(), RealDeltaSeconds );
	check( ClampedDeltaSeconds >= 0.0f );

	return ClampedDeltaSeconds;
}

void UDemoNetDriver::TickDispatch( float DeltaSeconds )
{
	Super::TickDispatch( DeltaSeconds );

	if ( !IsPlaying() )
	{
		// Nothing to do
		return;
	}

	if ( ReplayStreamer->GetLastError() != ENetworkReplayError::None )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickDispatch: ReplayStreamer ERROR: %s" ), ENetworkReplayError::ToString( ReplayStreamer->GetLastError() ) );
		const bool bIsPlaying = IsPlaying();
		StopDemo();
		if ( bIsPlaying )
		{
			World->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( EDemoPlayFailure::ToString( EDemoPlayFailure::Generic ) ) );
		}
		return;
	}

	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr == nullptr )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::TickDispatch: FileAr == nullptr" ) );
		StopDemo();
		return;
	}

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

	// DeltaSeconds that is padded in, is unclampped and not time dilated
	DeltaSeconds = GetClampedDeltaSeconds( World, DeltaSeconds );

	// Update time dilation on spectator pawn to compensate for any demo dilation 
	//	(we want to continue to fly around in real-time)
	if ( SpectatorController != NULL )
	{
		if ( World->GetWorldSettings()->DemoPlayTimeDilation > KINDA_SMALL_NUMBER )
		{
			SpectatorController->CustomTimeDilation = 1.0f / World->GetWorldSettings()->DemoPlayTimeDilation;
		}
		else
		{
			SpectatorController->CustomTimeDilation = 1.0f;
		}

		if ( SpectatorController->GetSpectatorPawn() != NULL )
		{
			SpectatorController->GetSpectatorPawn()->CustomTimeDilation = SpectatorController->CustomTimeDilation;

			SpectatorController->GetSpectatorPawn()->PrimaryActorTick.bTickEvenWhenPaused = true;

			USpectatorPawnMovement* SpectatorMovement = Cast<USpectatorPawnMovement>(SpectatorController->GetSpectatorPawn()->GetMovementComponent());

			if ( SpectatorMovement )
			{
				//SpectatorMovement->bIgnoreTimeDilation = true;
				SpectatorMovement->PrimaryComponentTick.bTickEvenWhenPaused = true;
			}
		}
	}

	TickDemoPlayback( DeltaSeconds );
}

void UDemoNetDriver::ProcessRemoteFunction( class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject )
{
	if ( IsRecording() )
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
	if ( !IsRecording() && !IsPlaying() )
	{
		UE_LOG( LogDemo, Log, TEXT( "StopDemo: No demo is playing" ) );
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
		}
	}
	else
	{
		// flush out any pending network traffic
		ServerConnection->FlushNet();

		ServerConnection->State = USOCK_Closed;
		ServerConnection->Close();
	}

	ReplayStreamer->StopStreaming();
	ClearReplayTasks();

	check( !IsRecording() && !IsPlaying() );
}

/*-----------------------------------------------------------------------------
Demo Recording tick.
-----------------------------------------------------------------------------*/

static bool DemoReplicateActor( AActor* Actor, UNetConnection* Connection, APlayerController* SpectatorController, bool bMustReplicate )
{
	// RAII object to swap the Role and RemoteRole of an actor within a scope. Used for recording replays on a client.
	class FScopedActorRoleSwap
	{
	public:
		FScopedActorRoleSwap(AActor* InActor)
			: Actor(InActor)
		{
			if (Actor != nullptr)
			{
				Actor->SwapRolesForReplay();
			}
		}

		~FScopedActorRoleSwap()
		{
			if (Actor != nullptr)
			{
				Actor->SwapRolesForReplay();
			}
		}

	private:
		AActor* Actor;
	};

	const int32 OriginalOutBunches = Connection->Driver->OutBunches;
	
	bool bDidReplicateActor = false;

	if ( Actor != NULL )
	{
		// We need to swap roles if:
		//  1. We're recording a replay on a client that's connected to a live server, and
		//  2. the actor isn't bTearOff, and
		//  3. the actor isn't the replay spectator controller.
		//  3. the actor isn't the replay spectator controller or its PlayerState.
		// This is to ensure the roles appear correct when playing back this demo.
		UWorld* ActorWorld = Actor->GetWorld();
		bool bShouldSwapRoles =
			ActorWorld != nullptr && ActorWorld->IsRecordingClientReplay() &&
			!Actor->bTearOff &&
			Actor != SpectatorController &&
			Actor != SpectatorController->PlayerState;

		FScopedActorRoleSwap RoleSwap(bShouldSwapRoles ? Actor : nullptr);

		if ( (Actor->GetRemoteRole() != ROLE_None || Actor->bTearOff) && ( Actor == Connection->PlayerController || Cast< APlayerController >( Actor ) == NULL ) )
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
				bDidReplicateActor = Channel->ReplicateActor();

				// Close the channel if this actor shouldn't have one
				if (!bShouldHaveChannel)
				{
					Channel->Close();
				}
			}
		}
	}

	if ( bMustReplicate && Connection->Driver->OutBunches == OriginalOutBunches )
	{
		UE_LOG( LogDemo, Error, TEXT( "DemoReplicateActor: bMustReplicate is true but nothing was sent: %s" ), Actor ? *Actor->GetName() : TEXT( "NULL" ) );
	}

	return bDidReplicateActor;
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
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SaveCheckpoint time"), STAT_ReplayCheckpointSaveTime, STATGROUP_Net);

	FArchive* CheckpointArchive = ReplayStreamer->GetCheckpointArchive();

	if ( CheckpointArchive == nullptr )
	{
		// This doesn't mean error, it means the streamer isn't ready to save checkpoints
		return;
	}

	check( CheckpointArchive->TotalSize() == 0 );

	const double StartCheckpointTime = FPlatformTime::Seconds();

	// Save the replicated server time so we can restore it after the checkpoint has been serialized.
	// This preserves the existing behavior and prevents clients from receiving updated server time
	// more often than the normal update rate.
	float SavedReplicatedServerTimeSeconds = -1.0f;

	AGameState* const GameState = World != nullptr ? World->GetGameState() : nullptr;

	// Normally AGameState::ReplicatedWorldTimeSeconds is only updated periodically,
	// but we want to make sure it's accurate for the checkpoint.
	if (GameState != nullptr)
	{
		SavedReplicatedServerTimeSeconds = GameState->ReplicatedWorldTimeSeconds;
		GameState->UpdateServerTimeSeconds();
	}

	// First, save the current guid cache
	SerializeGuidCache( GuidCache, CheckpointArchive );

	const uint32 GuidCacheSize = CheckpointArchive->TotalSize();

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

	for ( TSharedPtr<FNetworkObjectInfo>& ObjectInfo : GetNetworkObjectList().GetObjects() )
	{
		AActor* Actor = ObjectInfo.Get()->Actor;

		if ( CheckpointConnection->ActorChannels.Contains( Actor ) )
		{
			Actor->CallPreReplication( this );
			DemoReplicateActor( Actor, CheckpointConnection, SpectatorController, true );
		}
	}

	CheckpointConnection->FlushNet();
	check( CheckpointConnection->SendBuffer.GetNumBits() == 0 );

	WriteDemoFrameFromQueuedDemoPackets( *CheckpointArchive, CastChecked< UDemoNetConnection>( CheckpointConnection ) );

	bSavingCheckpoint = false;

	// Undo hackery
	ClientConnections[0]->PlayerController->Player			= ClientConnections[0];
	ClientConnections[0]->PlayerController->NetConnection	= ClientConnections[0];

	CheckpointConnection->Close();
	CheckpointConnection->CleanUp();

	const uint32 CheckpointSize = CheckpointArchive->TotalSize() - GuidCacheSize;

	const int32 TotalSize = CheckpointArchive->TotalSize();

	if ( CheckpointArchive->TotalSize() > 0 )
	{
		ReplayStreamer->FlushCheckpoint( GetDemoCurrentTimeInMS() );
	}

	// Restore the game state's replicated world time
	if (GameState != nullptr)
	{
		GameState->ReplicatedWorldTimeSeconds = SavedReplicatedServerTimeSeconds;
	}

	const double EndCheckpointTime = FPlatformTime::Seconds();

	const float CheckpointTimeInMS = ( EndCheckpointTime - StartCheckpointTime ) * 1000.0f;

	UE_LOG( LogDemo, Log, TEXT( "Checkpoint. Total: %i, Rep size: %i, PackageMap: %u, Time: %2.2f" ), TotalSize, CheckpointSize, GuidCacheSize, CheckpointTimeInMS );
}

void UDemoNetDriver::SaveExternalData( FArchive& Ar )
{
	for ( auto It = RepChangedPropertyTrackerMap.CreateIterator(); It; ++It )
	{
		if ( It.Key().IsValid() )
		{
			if ( It.Value()->ExternalDataNumBits > 0 )
			{
				// Save payload size (in bits)
				Ar.SerializeIntPacked( It.Value()->ExternalDataNumBits );

				FNetworkGUID NetworkGUID = GuidCache->NetGUIDLookup.FindChecked( It.Key() );

				// Save GUID
				Ar << NetworkGUID;
			
				// Save payload
				Ar.Serialize( It.Value()->ExternalData.GetData(), It.Value()->ExternalData.Num() );

				It.Value()->ExternalData.Empty();
				It.Value()->ExternalDataNumBits = 0;
			}
		}
	}

	uint32 StopCount = 0;
	Ar.SerializeIntPacked( StopCount );
}

void UDemoNetDriver::LoadExternalData( FArchive& Ar, const float TimeSeconds )
{
	while ( true )
	{
		uint8 ExternalDataBuffer[1024];
		uint32 ExternalDataNumBits;

		// Read payload into payload/guid map
		Ar.SerializeIntPacked( ExternalDataNumBits );

		if ( ExternalDataNumBits == 0 )
		{
			return;
		}

		FNetworkGUID NetGUID;

		// Read net guid this payload belongs to
		Ar << NetGUID;

		int32 ExternalDataNumBytes = ( ExternalDataNumBits + 7 ) >> 3;

		Ar.Serialize( ExternalDataBuffer, ExternalDataNumBytes );

		FBitReader Reader( ExternalDataBuffer, ExternalDataNumBits );

		FReplayExternalDataArray& ExternalDataArray = ExternalDataToObjectMap.FindOrAdd( NetGUID );

		ExternalDataArray.Add( FReplayExternalData( Reader, TimeSeconds ) );
	}
}

void UDemoNetDriver::AddEvent(const FString& Group, const FString& Meta, const TArray<uint8>& Data)
{
	uint32 SavedTimeMS = GetDemoCurrentTimeInMS();
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->AddEvent(SavedTimeMS, Group, Meta, Data);
	}
	UE_LOG(LogDemo, Verbose, TEXT("Custom Event %s. Total: %i, Time: %2.2f"), *Group, Data.Num(), SavedTimeMS);
}

void UDemoNetDriver::EnumerateEvents(const FString& Group, FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate)
{
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->EnumerateEvents(Group, EnumerationCompleteDelegate);
	}
}

void UDemoNetDriver::RequestEventData(const FString& EventID, FOnRequestEventDataComplete& RequestEventDataCompleteDelegate)
{
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->RequestEventData(EventID, RequestEventDataCompleteDelegate);
	}
}

void UDemoNetDriver::TickDemoRecord( float DeltaSeconds )
{
	if ( !IsRecording() )
	{
		return;
	}

	FArchive* FileAr = ReplayStreamer->GetStreamingArchive();

	if ( FileAr == NULL )
	{
		return;
	}

	// DeltaSeconds that is padded in, is unclampped and not time dilated
	DemoCurrentTime += GetClampedDeltaSeconds( World, DeltaSeconds );

	ReplayStreamer->UpdateTotalDemoTime( GetDemoCurrentTimeInMS() );

	float MAX_RECORD_HZ	= CVarDemoRecordHz.GetValueOnGameThread();
	float MIN_RECORD_HZ = CVarDemoMinRecordHz.GetValueOnGameThread();
	if (MIN_RECORD_HZ > MAX_RECORD_HZ)
	{
		// make sure min and max are sane
		Swap(MIN_RECORD_HZ, MAX_RECORD_HZ);
	}

	// Save out a frame
	DemoFrameNum++;
	ReplicationFrame++;

	// flush out any pending network traffic
	ClientConnections[0]->FlushNet();

	// Make sure we don't have anything in the buffer for this new frame
	check( ClientConnections[0]->SendBuffer.GetNumBits() == 0 );

	float ServerTickTime = GEngine->GetMaxTickRate( DeltaSeconds );
	if ( ServerTickTime == 0.0 )
	{
		ServerTickTime = DeltaSeconds;
	}
	else
	{
		ServerTickTime	= 1.0 / ServerTickTime;
	}

	const bool bUseAdapativeNetFrequency = CVarUseAdaptiveReplayUpdateFrequency.GetValueOnGameThread() > 0;

	for ( auto ActorIt = GetNetworkObjectList().GetObjects().CreateIterator(); ActorIt; ++ActorIt)
	{
		FNetworkObjectInfo* ActorInfo = (*ActorIt).Get();
		AActor* Actor = ActorInfo->Actor;
		
		// Check NetUpdateFrequency for this actor, but clamp it to RECORD_HZ.
		if ( DemoCurrentTime > ActorInfo->NextUpdateTime )
		{
			if ( Actor->IsPendingKill() )
			{
				ActorIt.RemoveCurrent();
				continue;
			}

			// During client recording, a torn-off actor will already have its remote role set to None, but
			// we still need to replicate it one more time so that the recorded replay knows it's been torn-off as well.
			if ( Actor->GetRemoteRole() == ROLE_None && !Actor->bTearOff)
			{
				ActorIt.RemoveCurrent();
				continue;
			}

			const float ClampedNetUpdateFrequency = FMath::Clamp(Actor->NetUpdateFrequency, MIN_RECORD_HZ, MAX_RECORD_HZ);
			const double NetUpdateDelay = 1.0 / ClampedNetUpdateFrequency;

			// Set defaults if this actor is replicating for first time
			if ( ActorInfo->LastNetReplicateTime == 0 )
			{
				ActorInfo->LastNetReplicateTime		= DemoCurrentTime;
				ActorInfo->OptimalNetUpdateDelta	= NetUpdateDelay;
			}

			const float LastReplicateDelta = static_cast<float>( DemoCurrentTime - ActorInfo->LastNetReplicateTime );

			if ( Actor->MinNetUpdateFrequency == 0.0f )
			{
				Actor->MinNetUpdateFrequency = 2.0f;
			}

			// Calculate min delta (max rate actor will update), and max delta (slowest rate actor will update)
			const float MinOptimalDelta = NetUpdateDelay;										// Don't go faster than NetUpdateFrequency
			const float MaxOptimalDelta = FMath::Max( 1.0f / Actor->MinNetUpdateFrequency, MinOptimalDelta );	// Don't go slower than MinNetUpdateFrequency (or NetUpdateFrequency if it's slower)

			const float ScaleDownStartTime = 2.0f;
			const float ScaleDownTimeRange = 5.0f;				

			if ( LastReplicateDelta > ScaleDownStartTime )
			{
				// Interpolate between MinOptimalDelta/MaxOptimalDelta based on how long it's been since this actor actually sent anything
				const float Alpha = FMath::Clamp( ( LastReplicateDelta - ScaleDownStartTime ) / ScaleDownTimeRange, 0.0f, 1.0f );
				ActorInfo->OptimalNetUpdateDelta = FMath::Lerp( MinOptimalDelta, MaxOptimalDelta, Alpha );
			}

			const double NextUpdateDelta = bUseAdapativeNetFrequency ? ActorInfo->OptimalNetUpdateDelta : NetUpdateDelay;

			// Account for being fractionally into the next frame
			// But don't be more than a fraction of a frame behind either (we don't want to do catch-up frames when there is a long delay)
			const double ExtraTime = DemoCurrentTime - ActorInfo->NextUpdateTime;
			const double ClampedExtraTime = FMath::Clamp(ExtraTime, 0.0, NetUpdateDelay);

			// Try to spread the updates across multiple frames to smooth out spikes.
			ActorInfo->NextUpdateTime = ( DemoCurrentTime + NextUpdateDelta - ClampedExtraTime + ( ( FMath::SRand() - 0.5 ) * ServerTickTime ) );

			Actor->CallPreReplication( this );

			const bool bUpdatedExternalData = ( FindOrCreateRepChangedPropertyTracker( Actor ).Get()->ExternalData.Num() > 0 );

			if ( DemoReplicateActor( Actor, ClientConnections[0], SpectatorController, false ) || bUpdatedExternalData )
			{
				// Choose an optimal time, we choose 70% of the actual rate to allow frequency to go up if needed
				ActorInfo->OptimalNetUpdateDelta = FMath::Clamp( LastReplicateDelta * 0.7f, MinOptimalDelta, MaxOptimalDelta );
				ActorInfo->LastNetReplicateTime = DemoCurrentTime;
			}
		}
	}

	// Make sure nothing is left over
	ClientConnections[0]->FlushNet();
	check( ClientConnections[0]->SendBuffer.GetNumBits() == 0 );

	WriteDemoFrameFromQueuedDemoPackets( *FileAr, CastChecked< UDemoNetConnection >( ClientConnections[0] ) );

	// Save a checkpoint if it's time
	if ( CVarEnableCheckpoints.GetValueOnGameThread() == 1 )
	{
		const double CHECKPOINT_DELAY = 30.0;

		if ( DemoCurrentTime - LastCheckpointTime > CHECKPOINT_DELAY )
		{
			SaveCheckpoint();
			LastCheckpointTime = DemoCurrentTime;
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

bool UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets( FArchive& Ar )
{
	if ( Ar.IsError() )
	{
		StopDemo();
		return false;
	}

	if ( Ar.AtEnd() )
	{
		return false;
	}

	if ( ReplayStreamer->GetLastError() != ENetworkReplayError::None )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: ReplayStreamer ERROR: %s" ), ENetworkReplayError::ToString( ReplayStreamer->GetLastError() ) );
		StopDemo();
		return false;
	}

	if ( !ReplayStreamer->IsDataAvailable() )
	{
		return false;
	}

	float TimeSeconds = 0.0f;

	Ar << TimeSeconds;

#if DEMO_CHECKSUMS == 1
	{
		uint32 ServerDeltaTimeCheksum = 0;
		Ar << ServerDeltaTimeCheksum;

		const uint32 DeltaTimeChecksum = FCrc::MemCrc32( &TimeSeconds, sizeof( TimeSeconds ), 0 );

		if ( DeltaTimeChecksum != ServerDeltaTimeCheksum )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: DeltaTimeChecksum != ServerDeltaTimeCheksum" ) );
			StopDemo();
			return false;
		}
	}
#endif

	if ( Ar.IsError() )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: Failed to read demo ServerDeltaTime" ) );
		StopDemo();
		return false;
	}

	// Load any custom external data in this frame
	LoadExternalData( Ar, TimeSeconds );

	// Buffer any packets in this frame
	while ( true )
	{
		int32 PacketBytes = 0;
		uint8 ReadBuffer[MAX_DEMO_READ_WRITE_BUFFER];

		if ( !ReadPacket( Ar, ReadBuffer, PacketBytes, sizeof( ReadBuffer ) ) )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: ReadPacket failed." ) );

			StopDemo();

			if ( World != NULL && World->GetGameInstance() != NULL )
			{
				World->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( TEXT( "UDemoNetDriver::ReadDemoFrameIntoPlaybackPackets: PacketBytes > sizeof( ReadBuffer )" ) ) );
			}

			return false;
		}

		if ( PacketBytes == 0 )
		{
			break;
		}

		FPlaybackPacket& Packet = *( new( PlaybackPackets )FPlaybackPacket );
		Packet.Data.AddUninitialized( PacketBytes );
		Packet.TimeSeconds = TimeSeconds;
		FMemory::Memcpy( Packet.Data.GetData(), ReadBuffer, PacketBytes );
	}

	return true;
}

bool UDemoNetDriver::ConditionallyReadDemoFrameIntoPlaybackPackets( FArchive& Ar )
{
	if ( PlaybackPackets.Num() > 0 )
	{
		const float MAX_PLAYBACK_BUFFER_SECONDS = 5.0f;

		if ( PlaybackPackets[PlaybackPackets.Num() - 1].TimeSeconds > DemoCurrentTime && PlaybackPackets[PlaybackPackets.Num() - 1].TimeSeconds - DemoCurrentTime > MAX_PLAYBACK_BUFFER_SECONDS )
		{
			return false;	// Don't buffer more than MAX_PLAYBACK_BUFFER_SECONDS worth of frames
		}
	}

	if ( !ReadDemoFrameIntoPlaybackPackets( Ar ) )
	{
		return false;
	}

	return true;
}

bool UDemoNetDriver::ReadPacket( FArchive& Archive, uint8* OutReadBuffer, int32& OutBufferSize, const int32 MaxBufferSize )
{
	OutBufferSize = 0;

	Archive << OutBufferSize;

	if ( Archive.IsError() )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPacket: Failed to read demo OutBufferSize" ) );
		return false;
	}

	if ( OutBufferSize == 0 )
	{
		return true;		// Done
	}

	if ( OutBufferSize > MaxBufferSize )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPacket: OutBufferSize > sizeof( ReadBuffer )" ) );
		return false;
	}

	// Read data from file.
	Archive.Serialize( OutReadBuffer, OutBufferSize );

	if ( Archive.IsError() )
	{
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPacket: Failed to read demo file packet" ) );
		return false;
	}

#if DEMO_CHECKSUMS == 1
	{
		uint32 ServerChecksum = 0;
		Archive << ServerChecksum;

		const uint32 Checksum = FCrc::MemCrc32( OutReadBuffer, OutBufferSize, 0 );

		if ( Checksum != ServerChecksum )
		{
			UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ReadPacket: Checksum != ServerChecksum" ) );
			return false;
		}
	}
#endif

	return true;
}

bool UDemoNetDriver::ConditionallyProcessPlaybackPackets()
{
	if ( PlaybackPackets.Num() == 0 )
	{
		PauseChannels( true );
		return false;
	}

	if ( DemoCurrentTime < PlaybackPackets[0].TimeSeconds )
	{
		// Not enough time has passed to read another frame
		return false;
	}

	const bool Result = ProcessPacket( PlaybackPackets[0].Data.GetData(), PlaybackPackets[0].Data.Num() );

	PlaybackPackets.RemoveAt( 0 );

	return Result;
}

void UDemoNetDriver::ProcessAllPlaybackPackets()
{
	for ( int32 i = 0; i < PlaybackPackets.Num(); i++ )
	{
		ProcessPacket( PlaybackPackets[i].Data.GetData(), PlaybackPackets[i].Data.Num() );
	}

	PlaybackPackets.Empty();
}

bool UDemoNetDriver::ProcessPacket( uint8* Data, int32 Count )
{
	PauseChannels( false );

	if ( ServerConnection != NULL )
	{
		// Process incoming packet.
		ServerConnection->ReceivedRawPacket( Data, Count );
	}

	if ( ServerConnection == NULL || ServerConnection->State == USOCK_Closed )
	{
		// Something we received resulted in the demo being stopped
		UE_LOG( LogDemo, Error, TEXT( "UDemoNetDriver::ProcessPacket: ReceivedRawPacket closed connection" ) );

		StopDemo();

		if ( World != NULL && World->GetGameInstance() != NULL )
		{
			World->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( TEXT( "UDemoNetDriver::ProcessPacket: PacketBytes > sizeof( ReadBuffer )" ) ) );
		}

		return false;
	}

	return true;
}

void UDemoNetDriver::WriteDemoFrameFromQueuedDemoPackets( FArchive& Ar, UDemoNetConnection* Connection )
{
	check( Connection->SendBuffer.GetNumBits() == 0 );

	// Save total absolute demo time in seconds
	Ar << DemoCurrentTime;

	SaveExternalData( Ar );

	for ( int32 i = 0; i < Connection->QueuedDemoPackets.Num(); i++ )
	{
		WritePacket( Ar, Connection->QueuedDemoPackets[i].Data.GetData(), Connection->QueuedDemoPackets[i].Data.Num() );
	}

	Connection->QueuedDemoPackets.Empty();

	// Write a count of 0 to signal the end of the frame
	int32 EndCount = 0;
	Ar << EndCount;
}

void UDemoNetDriver::WritePacket( FArchive& Ar, uint8* Data, int32 Count )
{
	Ar << Count;
	Ar.Serialize( Data, Count );

#if DEMO_CHECKSUMS == 1
	uint32 Checksum = FCrc::MemCrc32( Data, Count, 0 );
	Ar << Checksum;
#endif
}

void UDemoNetDriver::SkipTime(const float InTimeToSkip)
{
	if ( IsNamedTaskInQueue( TEXT( "FSkipTimeInSecondsTask" ) ) )
	{
		return;		// Don't allow time skipping if we already are
	}

	AddReplayTask( new FSkipTimeInSecondsTask( this, InTimeToSkip ) );
}

void UDemoNetDriver::SkipTimeInternal( const float SecondsToSkip, const bool InFastForward, const bool InIsForCheckpoint )
{
	check( !bIsFastForwarding );				// Can only do one of these at a time (use tasks to gate this)
	check( !bIsFastForwardingForCheckpoint );	// Can only do one of these at a time (use tasks to gate this)

	SavedSecondsToSkip = SecondsToSkip;
	DemoCurrentTime += SecondsToSkip;

	bIsFastForwarding				= InFastForward;
	bIsFastForwardingForCheckpoint	= InIsForCheckpoint;
}

void UDemoNetDriver::GotoTimeInSeconds(const float TimeInSeconds, const FOnGotoTimeDelegate& InOnGotoTimeDelegate)
{
	OnGotoTimeDelegate_Transient = InOnGotoTimeDelegate;

	if ( IsNamedTaskInQueue( TEXT( "FGotoTimeInSecondsTask" ) ) )
	{
		NotifyGotoTimeFinished(false);
		return;		// Don't allow scrubbing if we already are
	}

	AddReplayTask( new FGotoTimeInSecondsTask( this, TimeInSeconds ) );
}

void UDemoNetDriver::JumpToEndOfLiveReplay()
{
	UE_LOG( LogDemo, Log, TEXT( "UDemoNetConnection::JumpToEndOfLiveReplay." ) );

	const uint32 TotalDemoTimeInMS = ReplayStreamer->GetTotalDemoTime();

	DemoTotalTime = (float)TotalDemoTimeInMS / 1000.0f;

	const uint32 BufferInMS = 5 * 1000;

	const uint32 JoinTimeInMS = FMath::Max( (uint32)0, ReplayStreamer->GetTotalDemoTime() - BufferInMS );

	if ( JoinTimeInMS > 0 )
	{
		GotoTimeInSeconds( (float)JoinTimeInMS / 1000.0f );
	}
}

void UDemoNetDriver::AddUserToReplay( const FString& UserString )
{
	if ( ReplayStreamer.IsValid() )
	{
		ReplayStreamer->AddUserToReplay( UserString );
	}
}

void UDemoNetDriver::TickDemoPlayback( float DeltaSeconds )
{
	if ( World && World->IsInSeamlessTravel() )
	{
		return;
	}

	if ( !IsPlaying() )
	{
		return;
	}

	if ( CVarGotoTimeInSeconds.GetValueOnGameThread() >= 0.0f )
	{
		GotoTimeInSeconds( CVarGotoTimeInSeconds.GetValueOnGameThread() );
		CVarGotoTimeInSeconds.AsVariable()->Set( TEXT( "-1" ), ECVF_SetByConsole );
	}

	if (FMath::Abs(CVarDemoSkipTime.GetValueOnGameThread()) > 0.0f)
	{
		// Just overwrite existing value, cvar wins in this case
		GotoTimeInSeconds(DemoCurrentTime + CVarDemoSkipTime.GetValueOnGameThread());
		CVarDemoSkipTime.AsVariable()->Set(TEXT("0"), ECVF_SetByConsole);
	}

	if ( !ProcessReplayTasks() )
	{
		// We're busy processing tasks, return
		return;
	}

	// Update total demo time
	if ( ReplayStreamer->GetTotalDemoTime() > 0 )
	{
		DemoTotalTime = ( float )ReplayStreamer->GetTotalDemoTime() / 1000.0f;
	}

	// Make sure there is data available to read
	// If we're at the end of the demo, just pause channels and return
	if ( bDemoPlaybackDone || ( !PlaybackPackets.Num() && !ReplayStreamer->IsDataAvailable() ) )
	{
		PauseChannels( true );
		return;
	}

	// Advance demo time by seconds passed if we're not paused
	if ( World->GetWorldSettings()->Pauser == NULL )
	{
		DemoCurrentTime += DeltaSeconds;
	}

	// Clamp time
	if ( DemoCurrentTime > DemoTotalTime )
	{
		DemoCurrentTime = DemoTotalTime;
	}

	// Speculatively grab seconds now in case we need it to get the time it took to fast forward
	const double FastForwardStartSeconds = FPlatformTime::Seconds();

	// Buffer up demo frames until we have enough time built-up
	while ( ConditionallyReadDemoFrameIntoPlaybackPackets( *ReplayStreamer->GetStreamingArchive() ) )
	{
	}

	// Process packets until we are caught up (this implicitly handles fast forward if DemoCurrentTime past many frames)
	while ( ConditionallyProcessPlaybackPackets() )
	{
		DemoFrameNum++;
	}

	// Finalize any fast forward stuff that needs to happen
	if ( bIsFastForwarding )
	{
		FinalizeFastForward( FastForwardStartSeconds );
	}
}

void UDemoNetDriver::FinalizeFastForward( const float StartTime )
{
	// This must be set before we CallRepNotifies or they might be skipped again
	bIsFastForwarding = false;

	AGameState* const GameState = World != nullptr ? World->GetGameState() : nullptr;

	// Correct server world time for fast-forwarding after a checkpoint
	if (GameState != nullptr)
	{
		if (bIsFastForwardingForCheckpoint)
		{
			const float PostCheckpointServerTime = SavedReplicatedWorldTimeSeconds + SavedSecondsToSkip;
			GameState->ReplicatedWorldTimeSeconds = PostCheckpointServerTime;
		}

		// Correct client world time for any fast-forward, checkpoint or not
		World->TimeSeconds += SavedSecondsToSkip;

		// Correct the ServerWorldTimeSecondsDelta
		GameState->OnRep_ReplicatedWorldTimeSeconds();
	}

	if (bIsFastForwardingForCheckpoint)
	{
		// Make a pass at OnReps for startup actors, since they were skipped during checkpoint loading.
		// At this point the shadow state of these actors should be the actual state from before the checkpoint,
		// and the current state is the CDO state evolved by any changes that occurred during checkpoint loading and fast-forwarding.
		for (UChannel* Channel : ServerConnection->OpenChannels)
		{
			UActorChannel* const ActorChannel = Cast<UActorChannel>(Channel);
			if (ActorChannel == nullptr)
			{
				continue;
			}

			const AActor* const Actor = ActorChannel->GetActor();
			if (Actor == nullptr)
			{
				continue;
			}

			const FObjectReplicator* const ActorReplicator = ActorChannel->ActorReplicator;
			if (Actor->IsNetStartupActor() && ActorReplicator)
			{
				ActorReplicator->RepLayout->DiffProperties(&(ActorReplicator->RepState->RepNotifies), ActorReplicator->RepState->StaticBuffer.GetData(), Actor, true);
			}
		}
	}

	// Flush all pending RepNotifies that were built up during the fast-forward.
	if ( ServerConnection != nullptr )
	{
		for ( auto& ChannelPair : ServerConnection->ActorChannels )
		{
			if ( ChannelPair.Value != NULL )
			{
				for ( auto& ReplicatorPair : ChannelPair.Value->ReplicationMap )
				{
					ReplicatorPair.Value->CallRepNotifies( true );
				}
			}
		}
	}

	// We may have been fast-forwarding immediately after loading a checkpoint
	// for fine-grained scrubbing. If so, at this point we are no longer loading a checkpoint.
	bIsFastForwardingForCheckpoint = false;

	// Reset the never-queue GUID list, we'll rebuild it
	NonQueuedGUIDsForScrubbing.Reset();

	const float FastForwardTotalSeconds = FPlatformTime::Seconds() - StartTime;

	NotifyGotoTimeFinished(true);

	UE_LOG( LogDemo, Log, TEXT( "Fast forward took %.2f seconds." ), FastForwardTotalSeconds );
}

void UDemoNetDriver::SpawnDemoRecSpectator( UNetConnection* Connection, const FURL& ListenURL )
{
	check( Connection != NULL );

	// Get the replay spectator controller class from the default game mode object,
	// since the game mode instance isn't replicated to clients of live games.
	AGameState* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr;
	TSubclassOf<AGameMode> DefaultGameModeClass = GameState != nullptr ? GameState->GameModeClass : nullptr;
	
	// If we don't have a game mode class from the world, try to get it from the URL option.
	// This may be true on clients who are recording a replay before the game mode class was replicated to them.
	if (DefaultGameModeClass == nullptr)
	{
		const TCHAR* URLGameModeClass = ListenURL.GetOption(TEXT("game="), nullptr);
		if (URLGameModeClass != nullptr)
		{
			UClass* GameModeFromURL = StaticLoadClass(AGameMode::StaticClass(), nullptr, URLGameModeClass);
			DefaultGameModeClass = GameModeFromURL;
		}
	}

	AGameMode* DefaultGameMode = Cast<AGameMode>(DefaultGameModeClass.GetDefaultObject());
	UClass* C = DefaultGameMode != nullptr ? DefaultGameMode->ReplaySpectatorPlayerControllerClass : nullptr;

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

	// Make sure SpectatorController->GetNetDriver returns this driver. Ensures functions that depend on it,
	// such as IsLocalController, work as expected.
	SpectatorController->SetNetDriverName(NetDriverName);

	// If the controller doesn't have a player state, we are probably recording on a client.
	// Spawn one manually.
	if ( SpectatorController->PlayerState == nullptr && GetWorld() != nullptr && GetWorld()->IsRecordingClientReplay())
	{
		SpectatorController->InitPlayerState();
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

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( CVarDemoForceFailure.GetValueOnGameThread() == 1 )
	{
		bSuccess = false;
	}
#endif

	if ( !bSuccess )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UDemoNetConnection::ReplayStreamingReady: Failed." ) );

		StopDemo();

		if ( !bRecord )
		{
			GetWorld()->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::DemoNotFound, FString( EDemoPlayFailure::ToString( EDemoPlayFailure::DemoNotFound ) ) );
		}
		return;
	}

	if ( !bRecord )
	{
		FString Error;
		
		const double StartTime = FPlatformTime::Seconds();

		if ( !InitConnectInternal( Error ) )
		{
			return;
		}

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
				AddReplayTask( new FJumpToLiveReplayTask( this ) );
			}
		}
	}
}

FReplayExternalDataArray* UDemoNetDriver::GetExternalDataArrayForObject( UObject* Object )
{
	FNetworkGUID NetworkGUID = GuidCache->NetGUIDLookup.FindRef( Object );

	if ( !NetworkGUID.IsValid() )
	{
		return nullptr;
	}

	return ExternalDataToObjectMap.Find( NetworkGUID );
}

void UDemoNetDriver::LoadCheckpoint( FArchive* GotoCheckpointArchive, int64 GotoCheckpointSkipExtraTimeInMS )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("LoadCheckpoint time"), STAT_ReplayCheckpointLoadTime, STATGROUP_Net);

	check( GotoCheckpointArchive != NULL );
	check( !bIsFastForwardingForCheckpoint );
	check( !bIsFastForwarding );

	// Save off the current spectator position
	// Check for NULL, which can be the case if we haven't played any of the demo yet but want to fast forward (joining live game for example)
	if ( SpectatorController != NULL )
	{
		// Save off the SpectatorController's GUID so that we know not to queue his bunches
		AddNonQueuedActorForScrubbing( SpectatorController );
	}

	// Remember the spectator controller's view target so we can restore it
	FNetworkGUID ViewTargetGUID;

	if ( SpectatorController && SpectatorController->GetViewTarget() )
	{
		ViewTargetGUID = GuidCache->NetGUIDLookup.FindRef( SpectatorController->GetViewTarget() );

		if ( ViewTargetGUID.IsValid() )
		{
			AddNonQueuedActorForScrubbing( SpectatorController->GetViewTarget() );
		}
	}

	PauseChannels( false );

	FNetworkReplayDelegates::OnPreScrub.Broadcast(GetWorld());

	bIsLoadingCheckpoint = true;

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
		
		if ( SpectatorController != nullptr )
		{
			if ( *It == SpectatorController || *It == SpectatorController->GetSpectatorPawn() )
			{
				continue;
			}

			if ( It->GetOwner() == SpectatorController && *It != SpectatorController->PlayerState )
			{
				continue;
			}
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

	ExternalDataToObjectMap.Empty();
	PlaybackPackets.Empty();

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
		bIsLoadingCheckpoint	= false;

		if ( GotoCheckpointSkipExtraTimeInMS != -1 )
		{
			SkipTimeInternal( ( float )GotoCheckpointSkipExtraTimeInMS / 1000.0f, true, true );
		}

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

	ReadDemoFrameIntoPlaybackPackets( *GotoCheckpointArchive );

	if ( PlaybackPackets.Num() > 0 )
	{
		DemoCurrentTime = PlaybackPackets[PlaybackPackets.Num() - 1].TimeSeconds;
	}
	else
	{
		DemoCurrentTime = 0.0f;
	}

	if ( GotoCheckpointSkipExtraTimeInMS != -1 )
	{
		// If we need to skip more time for fine scrubbing, set that up now
		SkipTimeInternal( ( float )GotoCheckpointSkipExtraTimeInMS / 1000.0f, true, true );
	}

	ProcessAllPlaybackPackets();

	bDemoPlaybackDone = false;
	bIsLoadingCheckpoint = false;

	// Save the replicated server time here
	if (World != nullptr)
	{
		const AGameState* const GameState = World->GetGameState();
		if (GameState != nullptr)
		{
			SavedReplicatedWorldTimeSeconds = GameState->ReplicatedWorldTimeSeconds;
		}
	}

	if ( SpectatorController && ViewTargetGUID.IsValid() )
	{
		AActor* ViewTarget = Cast< AActor >( GuidCache->GetObjectFromNetGUID( ViewTargetGUID, false ) );

		if ( ViewTarget )
		{
			SpectatorController->SetViewTarget( ViewTarget );
		}
	}
}

bool UDemoNetDriver::ShouldQueueBunchesForActorGUID(FNetworkGUID InGUID) const
{
	if ( CVarDemoQueueCheckpointChannels.GetValueOnGameThread() == 0)
	{
		return false;
	}

	// While loading a checkpoint, queue most bunches so that we don't process them all on one frame.
	if ( bIsFastForwardingForCheckpoint )
	{
		return !NonQueuedGUIDsForScrubbing.Contains(InGUID);
	}

	return false;
}

FNetworkGUID UDemoNetDriver::GetGUIDForActor(const AActor* InActor) const
{
	UNetConnection* Connection = ServerConnection;
	
	if ( ClientConnections.Num() > 0)
	{
		Connection = ClientConnections[0];
	}

	if ( !Connection )
	{
		return FNetworkGUID();
	}

	FNetworkGUID Guid = Connection->PackageMap->GetNetGUIDFromObject(InActor);
	return Guid;
}

AActor* UDemoNetDriver::GetActorForGUID(FNetworkGUID InGUID) const
{
	UNetConnection* Connection = ServerConnection;
	
	if ( ClientConnections.Num() > 0)
	{
		Connection = ClientConnections[0];
	}

	if ( !Connection )
	{
		return nullptr;
	}

	UObject* FoundObject = Connection->PackageMap->GetObjectFromNetGUID(InGUID, true);
	return Cast<AActor>(FoundObject);

}

bool UDemoNetDriver::ShouldReceiveRepNotifiesForObject(UObject* Object) const
{
	// Return false for startup actors during checkpoint loading, since they are
	// not destroyed and re-created like dynamic actors. Startup actors will
	// have their properties diffed and RepNotifies called after the checkpoint is loaded.

	if (!bIsLoadingCheckpoint && !bIsFastForwardingForCheckpoint)
	{
		return true;
	}

	const AActor* const Actor = Cast<AActor>(Object);
	const bool bIsStartupActor = Actor != nullptr && Actor->IsNetStartupActor();

	return !bIsStartupActor;
}

void UDemoNetDriver::AddNonQueuedActorForScrubbing(AActor const* Actor)
{
	UActorChannel const* const* const FoundChannel = ServerConnection->ActorChannels.Find(Actor);
	if (FoundChannel != nullptr && *FoundChannel != nullptr)
	{
		FNetworkGUID const ActorGUID = (*FoundChannel)->ActorNetGUID;
		NonQueuedGUIDsForScrubbing.Add(ActorGUID);
	}
}

void UDemoNetDriver::AddNonQueuedGUIDForScrubbing(FNetworkGUID InGUID)
{
	if (InGUID.IsValid())
	{
		NonQueuedGUIDsForScrubbing.Add(InGUID);
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

void UDemoNetConnection::InitConnection( UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed, int32 InMaxPacket)
{
	// default implementation
	Super::InitConnection( InDriver, InState, InURL, InConnectionSpeed );

	MaxPacket = (InMaxPacket == 0 || InMaxPacket > MAX_DEMO_READ_WRITE_BUFFER) ? MAX_DEMO_READ_WRITE_BUFFER : InMaxPacket;
	InternalAck = true;

	InitSendBuffer();

	// the driver must be a DemoRecording driver (GetDriver makes assumptions to avoid Cast'ing each time)
	check( InDriver->IsA( UDemoNetDriver::StaticClass() ) );
}

FString UDemoNetConnection::LowLevelGetRemoteAddress( bool bAppendPort )
{
	return TEXT( "UDemoNetConnection" );
}

void UDemoNetConnection::LowLevelSend(void* Data, int32 CountBytes, int32 CountBits)
{
	if ( CountBytes == 0 )
	{
		UE_LOG( LogDemo, Warning, TEXT( "UDemoNetConnection::LowLevelSend: Ignoring empty packet." ) );
		return;
	}

	if ( CountBytes > MAX_DEMO_READ_WRITE_BUFFER )
	{
		UE_LOG( LogDemo, Fatal, TEXT( "UDemoNetConnection::LowLevelSend: CountBytes > MAX_DEMO_READ_WRITE_BUFFER." ) );
	}

	TrackSendForProfiler( Data, CountBytes );

	new(QueuedDemoPackets)FQueuedDemoPacket((uint8*)Data, CountBytes, CountBits);
}

void UDemoNetConnection::TrackSendForProfiler(const void* Data, int32 NumBytes)
{
	NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(this));

	// Track "socket send" even though we're not technically sending to a socket, to get more accurate information in the profiler.
	NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendToCore(TEXT("Unreal"), Data, NumBytes, NumPacketIdBits, NumBunchBits, NumAckBits, NumPaddingBits, this));
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
		LastReceiveRealtime = FPlatformTime::Seconds();
		LastGoodPacketRealtime = FPlatformTime::Seconds();
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

TSharedPtr<FObjectReplicator> UDemoNetConnection::CreateReplicatorForNewActorChannel(UObject* Object)
{
	TSharedPtr<FObjectReplicator> NewReplicator = MakeShareable(new FObjectReplicator());

	// To handle rewinding net startup actors in replays properly, we need to
	// initialize the shadow state with the object's current state.
	// Afterwards, we will copy the CDO state to object's current state with repnotifies disabled.
	UDemoNetDriver* Driver = GetDriver();
	AActor* Actor = Cast<AActor>(Object);

	const bool bIsCheckpointStartupActor = Driver && Driver->IsLoadingCheckpoint() && Actor && Actor->IsNetStartupActor();
	const bool bUseDefaultState = !bIsCheckpointStartupActor;

	NewReplicator->InitWithObject(Object, this, bUseDefaultState);

	// Now that the shadow state is initialized, copy the CDO state into the actor state.
	if (bIsCheckpointStartupActor && NewReplicator->RepLayout.IsValid() && Object->GetClass())
	{
		NewReplicator->RepLayout->DiffProperties(nullptr, Object, Object->GetClass()->GetDefaultObject(), true);
	}

	return NewReplicator;
}

bool UDemoNetDriver::IsLevelInitializedForActor( const AActor* InActor, const UNetConnection* InConnection ) const
{
	return ( DemoFrameNum > 2 || Super::IsLevelInitializedForActor( InActor, InConnection ) );
}

void UDemoNetDriver::NotifyGotoTimeFinished(bool bWasSuccessful)
{
	// execute and clear the transient delegate
	OnGotoTimeDelegate_Transient.ExecuteIfBound(bWasSuccessful);
	OnGotoTimeDelegate_Transient.Unbind();

	// execute and keep the permanent delegate
	// call only when successful
	if (bWasSuccessful)
	{
		OnGotoTimeDelegate.Broadcast();
	}
}

/*-----------------------------------------------------------------------------
	UDemoPendingNetGame.
-----------------------------------------------------------------------------*/

UDemoPendingNetGame::UDemoPendingNetGame( const FObjectInitializer& ObjectInitializer ) : Super( ObjectInitializer )
{
}
