// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetDriver.h"
#include "NetworkReplayStreaming.h"
#include "DemoNetDriver.generated.h"

DECLARE_LOG_CATEGORY_EXTERN( LogDemo, Log, All );

DECLARE_MULTICAST_DELEGATE(FOnGotoTimeMCDelegate);
DECLARE_DELEGATE_OneParam(FOnGotoTimeDelegate, const bool /* bWasSuccessful */);

class UDemoNetDriver;

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

/**
 * Simulated network driver for recording and playing back game sessions.
 */
UCLASS(transient, config=Engine)
class ENGINE_API UDemoNetDriver : public UNetDriver
{
	GENERATED_UCLASS_BODY()

	/** Name of the file to read/write from */
	FString DemoFilename;

	/** Current record/playback frame number */
	int32 DemoFrameNum;

	/** Last time (in real seconds) that we recorded a frame */
	double LastRecordTime;

	/** Total time of demo in seconds */
	float DemoTotalTime;

	/** Current record/playback position in seconds */
	float DemoCurrentTime;

	/** Old current record/playback position in seconds (so we can restore on checkpoint failure) */
	float OldDemoCurrentTime;

	/** Total number of frames in the demo */
	int32 DemoTotalFrames;

	/** True if we're in the middle of recording a frame */
	bool bIsRecordingDemoFrame;

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

	bool		bSavingCheckpoint;
	double		LastCheckpointTime;

	void		SaveCheckpoint();

	void		LoadCheckpoint( FArchive* GotoCheckpointArchive, int64 GotoCheckpointSkipExtraTimeInMS );

	/** Public delegate for external systems to be notified when scrubbing is complete. Only called for successful scrub. */
	FOnGotoTimeMCDelegate OnGotoTimeDelegate;

private:
	bool		bIsFastForwarding;
	bool		bIsFastForwardingForCheckpoint;
	bool		bWasStartStreamingSuccessful;

	TArray<FNetworkGUID> NonQueuedGUIDsForScrubbing;

	// Replay tasks
	TArray< TSharedPtr< FQueuedReplayTask > >	QueuedReplayTasks;
	TSharedPtr< FQueuedReplayTask >				ActiveReplayTask;
	TSharedPtr< FQueuedReplayTask >				ActiveScrubReplayTask;

	/** Set via GotoTimeInSeconds, only fired once (at most). Called for successful or failed scrub. */
	FOnGotoTimeDelegate OnGotoTimeDelegate_Transient;
	
public:

	// UNetDriver interface.

	virtual bool InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error ) override;
	virtual void FinishDestroy() override;
	virtual FString LowLevelGetNetworkNumber() override;
	virtual bool InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error ) override;
	virtual bool InitListen( FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error ) override;
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

	/** 
	 * Scrubs playback to the given time. 
	 * 
	 * @param TimeInSeconds
	 * @param InOnGotoTimeDelegate		Delegate to call when finished. Will be called only once at most.
	*/
	void GotoTimeInSeconds(const float TimeInSeconds, const FOnGotoTimeDelegate& InOnGotoTimeDelegate = FOnGotoTimeDelegate());

	bool IsRecording();
	bool IsPlaying();

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
	bool ConditionallyReadDemoFrame();
	bool ReadDemoFrame( FArchive* Archive );
	void TickDemoPlayback( float DeltaSeconds );
	void FinalizeFastForward( const float StartTime );
	void SpawnDemoRecSpectator( UNetConnection* Connection, const FURL& ListenURL );
	void ResetDemoState();
	void JumpToEndOfLiveReplay();
	void AddEvent(const FString& Group, const FString& Meta, const TArray<uint8>& Data);
	void EnumerateEvents(const FString& Group, FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate);
	void RequestEventData(const FString& EventID, FOnRequestEventDataComplete& RequestEventDataCompleteDelegate);
	virtual bool IsFastForwarding() { return bIsFastForwarding; }

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
