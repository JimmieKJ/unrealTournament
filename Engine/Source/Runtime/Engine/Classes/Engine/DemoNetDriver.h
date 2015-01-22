// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//
// Simulated network driver for recording and playing back game sessions.
#pragma once
#include "DemoNetDriver.generated.h"

UCLASS(transient, config=Engine)
class UDemoNetDriver : public UNetDriver
{
	GENERATED_UCLASS_BODY()

	// Variables.

	/** Name of the file to read/write from */
	FString				DemoFilename;

	/** Handle to the archive that will read/write network packets */
	FArchive*			FileAr;

	/** Current record/playback frame number */
	int32				DemoFrameNum;

	/** Last time (in real seconds) that we recorded a frame */
	double				LastRecordTime;

	/** Time (in game seconds) that have elapsed between recorded frames */
	float				DemoDeltaTime;

	/** Total time of demo in seconds */
	float				DemoTotalTime;

	/** Current record/playback position in seconds */
	float				DemoCurrentTime;

	/** Total number of frames in the demo */
	int32				DemoTotalFrames;

	/** during playback, set to offset of where the stream ends (we don't want to continue reading into the meta data section) */
	int32				EndOfStreamOffset;

	/** True if we're in the middle of recording a frame */
	bool				bIsRecordingDemoFrame;

	/** True if we are at the end of playing a demo */
	bool				bDemoPlaybackDone;

	/** This is our spectator controller that is used to view the demo world from */
	APlayerController*	SpectatorController;

	UPROPERTY( config )
	FString				DemoSpectatorClass;

	// Begin UNetDriver interface.
	virtual bool InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error ) override;
	virtual void FinishDestroy() override;
	virtual FString LowLevelGetNetworkNumber() override;
	virtual bool InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error ) override;
	virtual bool InitListen( FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error ) override;
	virtual void TickDispatch( float DeltaSeconds ) override;
	virtual void TickFlush( float DeltaSeconds ) override;
	virtual void ProcessRemoteFunction( class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = NULL );
	virtual bool IsAvailable() const override { return true; }
	// End UNetDriver interface.

	// Begin FExec interface.
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	// End FExec interface.

	/** @todo document */
	bool UpdateDemoTime( float* DeltaTime, float TimeDilation );

	/** Called when demo playback finishes, either because we reached the end of the file or because the demo spectator was destroyed */
	void DemoPlaybackEnded();

	/** @return true if the net resource is valid or false if it should not be used */
	virtual bool IsNetResourceValid(void) { return true; }

	void TickDemoRecord( float DeltaSeconds );
	bool ReadDemoFrame();
	void TickDemoPlayback( float DeltaSeconds );
	void SpawnDemoRecSpectator( UNetConnection* Connection );
	void ResetDemoState();

	void StopDemo();
};
