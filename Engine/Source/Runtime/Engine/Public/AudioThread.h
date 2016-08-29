// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioThread.h: Rendering thread definitions.
=============================================================================*/

#pragma once

#include "TaskGraphInterfaces.h"

////////////////////////////////////
// Audio thread API
////////////////////////////////////

DECLARE_STATS_GROUP(TEXT("Audio Thread Commands"), STATGROUP_AudioThreadCommands, STATCAT_Advanced);

class FAudioThread : public FRunnable
{
private:
	/**
	* Whether to run with an audio thread
	*/
	static bool bUseThreadedAudio;

	/** 
	* Sync event to make sure that audio thread is bound to the task graph before main thread queues work against it.
	*/
	FEvent* TaskGraphBoundSyncEvent;

	/**
	* Whether the audio thread is currently running
	* If this is false, then we have no audio thread and audio commands will be issued directly on the game thread
	*/
	static bool bIsAudioThreadRunning;

	static uint32 SuspensionCount;

	/**
	* Polled by the game thread to detect crashes in the audio thread.
	* If the audio thread crashes, it sets this variable to false.
	*/
	volatile static bool bIsAudioThreadHealthy;

	/** If the audio thread has been terminated by an unhandled exception, this contains the error message. */
	static FString AudioThreadError;

	/** The audio thread itself. */
	static FRunnable* AudioThreadRunnable;

	/** The stashed value of the audio thread as we clear it during GC */
	static uint32 CachedAudioThreadId;

	void OnPreGarbageCollect();
	void OnPostGarbageCollect();

public:

	FAudioThread();
	virtual ~FAudioThread();

	// FRunnable interface.
	virtual bool Init() override;
	virtual void Exit() override;
	virtual uint32 Run() override;

	/** Starts the audio thread. */
	static ENGINE_API void StartAudioThread();

	/** Stops the audio thread. */
	static ENGINE_API void StopAudioThread();

	/** Execute a command on the audio thread. If GIsAudioThreadRunning is false the command will execute immediately */
	static ENGINE_API void RunCommandOnAudioThread(TFunction<void()> InFunction, const TStatId InStatId = TStatId());

	/** Execute a (presumably audio) command on the game thread. If GIsAudioThreadRunning is false the command will execute immediately */
	static ENGINE_API void RunCommandOnGameThread(TFunction<void()> InFunction, const TStatId InStatId = TStatId());

	/**
	* Checks if the audio thread is healthy and running.
	* If it has crashed, UE_LOG is called with the exception information.
	*/
	static ENGINE_API void CheckAudioThreadHealth();

	/** Checks if the audio thread is healthy and running, without crashing */
	static ENGINE_API bool IsAudioThreadHealthy();

	static ENGINE_API void SetUseThreadedAudio(bool bInUseThreadedAudio);

	static ENGINE_API bool IsAudioThreadRunning() { return bIsAudioThreadRunning; }

	static ENGINE_API void SuspendAudioThread();
	static ENGINE_API void ResumeAudioThread();

};

/** Suspends the audio thread for the duration of the lifetime of the object */
struct FAudioThreadSuspendContext
{
	FAudioThreadSuspendContext()
	{
		FAudioThread::SuspendAudioThread();
	}

	~FAudioThreadSuspendContext()
	{
		FAudioThread::ResumeAudioThread();
	}
};

////////////////////////////////////
// Audio fences
////////////////////////////////////

/**
* Used to track pending audio commands from the game thread.
*/
class ENGINE_API FAudioCommandFence
{
public:

	/**
	* Adds a fence command to the audio command queue.
	* Conceptually, the pending fence count is incremented to reflect the pending fence command.
	* Once the rendering thread has executed the fence command, it decrements the pending fence count.
	*/
	void BeginFence();

	/**
	* Waits for pending fence commands to retire.
	* @param bProcessGameThreadTasks, if true we are on a short callstack where it is safe to process arbitrary game thread tasks while we wait
	*/
	void Wait(bool bProcessGameThreadTasks = false) const;

	// return true if the fence is complete
	bool IsFenceComplete() const;

private:
	/** Graph event that represents completion of this fence **/
	mutable FGraphEventRef CompletionEvent;
};

