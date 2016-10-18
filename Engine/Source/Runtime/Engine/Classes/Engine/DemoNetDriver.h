// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetDriver.h"
#include "NetworkReplayStreaming.h"
#include "PackageMapClient.h"
#include "DemoNetConnection.h"
#include "DemoNetDriver.generated.h"

DECLARE_LOG_CATEGORY_EXTERN( LogDemo, Log, All );

DECLARE_MULTICAST_DELEGATE(FOnGotoTimeMCDelegate);
DECLARE_DELEGATE_OneParam(FOnGotoTimeDelegate, const bool /* bWasSuccessful */);

class UDemoNetDriver;
class UDemoNetConnection;

class FQueuedReplayTask
{
public:
	FQueuedReplayTask( UDemoNetDriver* InDriver ) : Driver( InDriver )
	{
	}

	virtual ~FQueuedReplayTask()
	{
	}

	virtual void	StartTask() = 0;
	virtual bool	Tick() = 0;
	virtual FString	GetName() = 0;

	UDemoNetDriver* Driver;
};

class FReplayExternalData
{
public:
	FReplayExternalData() : TimeSeconds( 0.0f )
	{
	}

	FReplayExternalData( const FBitReader& InReader, const float InTimeSeconds ) : Reader( InReader ), TimeSeconds( InTimeSeconds )
	{
	}

	FBitReader	Reader;
	float		TimeSeconds;
};

typedef TArray< FReplayExternalData > FReplayExternalDataArray;

struct FPlaybackPacket
{
	TArray< uint8 > Data;
	float			TimeSeconds;
};

enum ENetworkVersionHistory
{
	HISTORY_INITIAL				= 1,
	HISTORY_SAVE_ABS_TIME_MS	= 2,			// We now save the abs demo time in ms for each frame (solves accumulation errors)
	HISTORY_INCREASE_BUFFER		= 3,			// Increased buffer size of packets, which invalidates old replays
	HISTORY_SAVE_ENGINE_VERSION	= 4,			// Now saving engine net version + InternalProtocolVersion
	HISTORY_EXTRA_VERSION		= 5				// We now save engine/game protocol version, checksum, and changelist
};

static const uint32 NETWORK_DEMO_MAGIC				= 0x2CF5A13D;
static const uint32 NETWORK_DEMO_VERSION			= HISTORY_EXTRA_VERSION;

static const uint32 NETWORK_DEMO_METADATA_MAGIC		= 0x3D06B24E;
static const uint32 NETWORK_DEMO_METADATA_VERSION	= 0;

struct FNetworkDemoHeader
{
	uint32	Magic;							// Magic to ensure we're opening the right file.
	uint32	Version;						// Version number to detect version mismatches.
	uint32	NetworkChecksum;				// Network checksum
	uint32	EngineNetworkProtocolVersion;	// Version of the engine internal network format
	uint32	GameNetworkProtocolVersion;		// Version of the game internal network format
	uint32	Changelist;						// Engine changelist built from
	FString LevelName;						// Name of level loaded for demo
	TArray<FString> GameSpecificData;		// Area for subclasses to write stuff

	FNetworkDemoHeader() :
		Magic( NETWORK_DEMO_MAGIC ),
		Version( NETWORK_DEMO_VERSION ),
		NetworkChecksum( FNetworkVersion::GetLocalNetworkVersion() ),
		EngineNetworkProtocolVersion( FNetworkVersion::GetEngineNetworkProtocolVersion() ),
		GameNetworkProtocolVersion( FNetworkVersion::GetGameNetworkProtocolVersion() ),
		Changelist( FEngineVersion::Current().GetChangelist() )
	{
	}

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

		Ar << Header.NetworkChecksum;
		Ar << Header.EngineNetworkProtocolVersion;
		Ar << Header.GameNetworkProtocolVersion;
		Ar << Header.Changelist;

		Ar << Header.LevelName;

		Ar << Header.GameSpecificData;

		return Ar;
	}
};

/**
 * Simulated network driver for recording and playing back game sessions.
 */
UCLASS(transient, config=Engine)
class ENGINE_API UDemoNetDriver : public UNetDriver
{
	GENERATED_UCLASS_BODY()

	/** Current record/playback frame number */
	int32 DemoFrameNum;

	/** Total time of demo in seconds */
	float DemoTotalTime;

	/** Current record/playback position in seconds */
	float DemoCurrentTime;

	/** Old current record/playback position in seconds (so we can restore on checkpoint failure) */
	float OldDemoCurrentTime;

	/** Total number of frames in the demo */
	int32 DemoTotalFrames;

	/** True if we are at the end of playing a demo */
	bool bDemoPlaybackDone;

	/** True if as have paused all of the channels */
	bool bChannelsArePaused;

	/** This is our spectator controller that is used to view the demo world from */
	APlayerController* SpectatorController;

	/** Our network replay streamer */
	TSharedPtr< class INetworkReplayStreamer >	ReplayStreamer;

	uint32 GetDemoCurrentTimeInMS() { return (uint32)( (double)DemoCurrentTime * 1000 ); }

	/** Internal debug timing/tracking */
	double		AccumulatedRecordTime;
	double		LastRecordAvgFlush;
	double		MaxRecordTime;
	int32		RecordCountSinceFlush;

	/** When actors are dormant in replays, we remove them from the master list, and add them here for tracking */
	TSet< TWeakObjectPtr< AActor > > FullyDormantActors;

	/** When we save a checkpoint, we remember all of the actors that need a checkpoint saved out by adding them to this list */
	TSet< TWeakObjectPtr< AActor > > PendingCheckpointActors;

	/** Checkpoint state */
	FPackageMapAckState CheckpointAckState;					// Current ack state of packagemap for the current checkpoint being saved
	double				TotalCheckpointSaveTimeSeconds;		// Total time it took to save checkpoint across all frames
	int32				TotalCheckpointSaveFrames;			// Total number of frames used to save a checkpoint
	double				LastCheckpointTime;					// Last time a checkpoint was saved

	void		SaveCheckpoint();
	void		TickCheckpoint();
	void		LoadCheckpoint( FArchive* GotoCheckpointArchive, int64 GotoCheckpointSkipExtraTimeInMS );

	void		SaveExternalData( FArchive& Ar );
	void		LoadExternalData( FArchive& Ar, const float TimeSeconds );

	/** Public delegate for external systems to be notified when scrubbing is complete. Only called for successful scrub. */
	FOnGotoTimeMCDelegate OnGotoTimeDelegate;

	bool		IsLoadingCheckpoint() const { return bIsLoadingCheckpoint; }

	/** ExternalDataToObjectMap is used to map a FNetworkGUID to the proper FReplayExternalDataArray */
	TMap< FNetworkGUID, FReplayExternalDataArray > ExternalDataToObjectMap;
		
	/** PlaybackPackets are used to buffer packets up when we read a demo frame, which we can then process when the time is right */
	TArray< FPlaybackPacket > PlaybackPackets;

	/** All unique streaming levels since recording started */
	TSet< TWeakObjectPtr< UObject > >	UniqueStreamingLevels;

	/** Streaming levels waiting to be saved next frame */
	TArray< UObject* >					NewStreamingLevelsThisFrame;

private:
	bool		bIsFastForwarding;
	bool		bIsFastForwardingForCheckpoint;
	bool		bWasStartStreamingSuccessful;
	bool		bIsLoadingCheckpoint;

	TArray<FNetworkGUID> NonQueuedGUIDsForScrubbing;

	// Replay tasks
	TArray< TSharedPtr< FQueuedReplayTask > >	QueuedReplayTasks;
	TSharedPtr< FQueuedReplayTask >				ActiveReplayTask;
	TSharedPtr< FQueuedReplayTask >				ActiveScrubReplayTask;

	/** Set via GotoTimeInSeconds, only fired once (at most). Called for successful or failed scrub. */
	FOnGotoTimeDelegate OnGotoTimeDelegate_Transient;
	
	/** Saved server time after loading a checkpoint, so that we can set the server time as accurately as possible after the fast-forward */
	float SavedReplicatedWorldTimeSeconds;

	/** Saved fast-forward time, used for correcting world time after the fast-forward is complete */
	float SavedSecondsToSkip;

	/** Cached replay URL, so that the driver can access the map name and any options later */
	FURL DemoURL;

	/** This header is valid during playback (so we know what version to pass into serializers, etc */
	FNetworkDemoHeader PlaybackDemoHeader;

	/** Optional time quota for actor replication during recording. Going over this limit effectively lowers the net update frequency of the remaining actors. Negative values are considered unlimited. */
	float MaxDesiredRecordTimeMS;

	/** A player controller that this driver should consider its viewpoint for actor prioritization purposes. */
	TWeakObjectPtr<APlayerController> ViewerOverride;

	/** Array of prioritized actors, used in TickDemoRecord. Stored as a member so that its storage doesn't have to be re-allocated each frame. */
	TArray<FActorPriority> PrioritizedActors;

	/** If true, recording will prioritize replicating actors based on the value that AActor::GetReplayPriority returns. */
	bool bPrioritizeActors;



	/** If true, will skip recording, but leaves the replay open so that recording can be resumed again. */
	bool bPauseRecording;

public:

	// UNetDriver interface.

	virtual bool InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error ) override;
	virtual void FinishDestroy() override;
	virtual FString LowLevelGetNetworkNumber() override;
	virtual bool InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error ) override;
	virtual bool InitListen( FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error ) override;
	virtual void TickFlush( float DeltaSeconds ) override;
	virtual void TickDispatch( float DeltaSeconds ) override;
	virtual void ProcessRemoteFunction( class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = nullptr ) override;
	virtual bool IsAvailable() const override { return true; }
	void SkipTime(const float InTimeToSkip);
	void SkipTimeInternal( const float SecondsToSkip, const bool InFastForward, const bool InIsForCheckpoint );
	bool InitConnectInternal(FString& Error);
	virtual bool ShouldClientDestroyTearOffActors() const override;
	virtual bool ShouldSkipRepNotifies() const override;
	virtual bool ShouldQueueBunchesForActorGUID(FNetworkGUID InGUID) const override;
	virtual FNetworkGUID GetGUIDForActor(const AActor* InActor) const override;
	virtual AActor* GetActorForGUID(FNetworkGUID InGUID) const override;
	virtual bool ShouldReceiveRepNotifiesForObject(UObject* Object) const override;

	/** 
	 * Scrubs playback to the given time. 
	 * 
	 * @param TimeInSeconds
	 * @param InOnGotoTimeDelegate		Delegate to call when finished. Will be called only once at most.
	*/
	void GotoTimeInSeconds(const float TimeInSeconds, const FOnGotoTimeDelegate& InOnGotoTimeDelegate = FOnGotoTimeDelegate());

	bool IsRecording() const;
	bool IsPlaying() const;

	FString GetDemoURL() { return DemoURL.ToString(); }

	/** Sets the desired maximum recording time in milliseconds. */
	void SetMaxDesiredRecordTimeMS(const float InMaxDesiredRecordTimeMS) { MaxDesiredRecordTimeMS = InMaxDesiredRecordTimeMS; }

	/** Sets the controller to use as the viewpoint for recording prioritization purposes. */
	void SetViewerOverride(APlayerController* const InViewerOverride ) { ViewerOverride = InViewerOverride; }

	/** Enable or disable prioritization of actors for recording. */
	void SetActorPrioritizationEnabled(const bool bInPrioritizeActors) { bPrioritizeActors = bInPrioritizeActors; }

public:

	// FExec interface

	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

public:

	/** @todo document */
	bool UpdateDemoTime( float* DeltaTime, float TimeDilation );

	/** Called when demo playback finishes, either because we reached the end of the file or because the demo spectator was destroyed */
	void DemoPlaybackEnded();

	/** @return true if the net resource is valid or false if it should not be used */
	virtual bool IsNetResourceValid(void) override { return true; }

	void TickDemoRecord( float DeltaSeconds );
	void PauseChannels( const bool bPause );
	void PauseRecording( const bool bInPauseRecording ) { bPauseRecording = bInPauseRecording; }
	bool IsRecordingPaused() const { return bPauseRecording; }

	bool ConditionallyProcessPlaybackPackets();
	void ProcessAllPlaybackPackets();
	bool ReadPacket( FArchive& Archive, uint8* OutReadBuffer, int32& OutBufferSize, const int32 MaxBufferSize );
	bool ReadDemoFrameIntoPlaybackPackets( FArchive& Ar );
	bool ConditionallyReadDemoFrameIntoPlaybackPackets( FArchive& Ar );
	bool ProcessPacket( uint8* Data, int32 Count );

	void WriteDemoFrameFromQueuedDemoPackets( FArchive& Ar, TArray<FQueuedDemoPacket>& QueuedPackets );
	void WritePacket( FArchive& Ar, uint8* Data, int32 Count );

	void TickDemoPlayback( float DeltaSeconds );
	void FinalizeFastForward( const float StartTime );
	void SpawnDemoRecSpectator( UNetConnection* Connection, const FURL& ListenURL );
	void ResetDemoState();
	void JumpToEndOfLiveReplay();
	void AddEvent(const FString& Group, const FString& Meta, const TArray<uint8>& Data);
	void EnumerateEvents(const FString& Group, FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate);
	void RequestEventData(const FString& EventID, FOnRequestEventDataComplete& RequestEventDataCompleteDelegate);
	virtual bool IsFastForwarding() { return bIsFastForwarding; }

	FReplayExternalDataArray* GetExternalDataArrayForObject( UObject* Object );

	/**
	 * Adds a join-in-progress user to the set of users associated with the currently recording replay (if any)
	 *
	 * @param UserString a string that uniquely identifies the user, usually his or her FUniqueNetId
	 */
	void AddUserToReplay(const FString& UserString);

	void StopDemo();

	void ReplayStreamingReady( bool bSuccess, bool bRecord );

	void AddReplayTask( FQueuedReplayTask* NewTask );
	bool IsAnyTaskPending();
	void ClearReplayTasks();
	bool ProcessReplayTasks();
	bool IsNamedTaskInQueue( const FString& Name );

	/** If a channel is associated with Actor, adds the channel's GUID to the list of GUIDs excluded from queuing bunches during scrubbing. */
	void AddNonQueuedActorForScrubbing(AActor const* Actor);
	/** Adds the channel's GUID to the list of GUIDs excluded from queuing bunches during scrubbing. */
	void AddNonQueuedGUIDForScrubbing(FNetworkGUID InGUID);

	virtual bool IsLevelInitializedForActor( const AActor* InActor, const UNetConnection* InConnection ) const override;

	/** Called when a "go to time" operation is completed. */
	void NotifyGotoTimeFinished(bool bWasSuccessful);

	/** Read the streaming level information from the metadata after the level is loaded */
	void PendingNetGameLoadMapCompleted();

protected:
	/** allows subclasses to write game specific data to demo header which is then handled by ProcessGameSpecificDemoHeader */
	virtual void WriteGameSpecificDemoHeader(TArray<FString>& GameSpecificData)
	{}
	/** allows subclasses to read game specific data from demo
	 * return false to cancel playback
	 */
	virtual bool ProcessGameSpecificDemoHeader(const TArray<FString>& GameSpecificData, FString& Error)
	{
		return true;
	}
};
