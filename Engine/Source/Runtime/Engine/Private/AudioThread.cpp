// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioThread.cpp: Audio thread implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "AudioThread.h"
#include "ExceptionHandling.h"
#include "TaskGraphInterfaces.h"

//
// Globals
//

int32 GCVarSuspendAudioThread = 0;
TAutoConsoleVariable<int32> CVarSuspendAudioThread(TEXT("AudioThread.SuspendAudioThread"), GCVarSuspendAudioThread, TEXT("0=Resume, 1=Suspend"), ECVF_Cheat);

struct FAudioThreadInteractor
{
	static void UseAudioThreadCVarSinkFunction()
	{
		static bool bLastSuspendAudioThread = false;
		const bool bSuspendAudioThread = (CVarSuspendAudioThread.GetValueOnGameThread() != 0);

		if (bLastSuspendAudioThread != bSuspendAudioThread)
		{
			bLastSuspendAudioThread = bSuspendAudioThread;
			if (GAudioThread)
			{
				if (bSuspendAudioThread)
				{
					FAudioThread::SuspendAudioThread();
				}
				else
				{
					FAudioThread::ResumeAudioThread();
				}
			}
			else if (GIsEditor)
			{
				UE_LOG(LogAudio, Warning, TEXT("Audio threading is disabled in the editor."));
			}
			else
			{
				UE_LOG(LogAudio, Warning, TEXT("Cannot manipulate audio thread when disabled by platform or ini."));
			}
		}
	}
};

static FAutoConsoleVariableSink CVarUseAudioThreadSink(FConsoleCommandDelegate::CreateStatic(&FAudioThreadInteractor::UseAudioThreadCVarSinkFunction));

bool FAudioThread::bIsAudioThreadRunning = false;
bool FAudioThread::bUseThreadedAudio = false;
uint32 FAudioThread::SuspensionCount = 0;
uint32 FAudioThread::CachedAudioThreadId = 0;
FRunnable* FAudioThread::AudioThreadRunnable = nullptr;
FString FAudioThread::AudioThreadError;
volatile bool FAudioThread::bIsAudioThreadHealthy = true;


/** The audio thread main loop */
void AudioThreadMain( FEvent* TaskGraphBoundSyncEvent )
{
	FTaskGraphInterface::Get().AttachToThread(ENamedThreads::AudioThread);
	FPlatformMisc::MemoryBarrier();

	// Inform main thread that the audio thread has been attached to the taskgraph and is ready to receive tasks
	if( TaskGraphBoundSyncEvent != nullptr )
	{
		TaskGraphBoundSyncEvent->Trigger();
	}

	// set the thread back to real time mode
	FPlatformProcess::SetRealTimeMode();

#if STATS
	if (FThreadStats::WillEverCollectData())
	{
		FThreadStats::ExplicitFlush(); // flush the stats and set update the scope so we don't flush again until a frame update, this helps prevent fragmentation
	}
#endif

	FTaskGraphInterface::Get().ProcessThreadUntilRequestReturn(ENamedThreads::AudioThread);
	FPlatformMisc::MemoryBarrier();
	
#if STATS
	if (FThreadStats::WillEverCollectData())
	{
		FThreadStats::ExplicitFlush(); // Another explicit flush to clean up the ScopeCount established above for any stats lingering since the last frame
	}
#endif
}

FAudioThread::FAudioThread()
{
	TaskGraphBoundSyncEvent	= FPlatformProcess::GetSynchEventFromPool(true);

	FCoreUObjectDelegates::PreGarbageCollect.AddRaw(this, &FAudioThread::OnPreGarbageCollect);
	FCoreUObjectDelegates::PostGarbageCollect.AddRaw(this, &FAudioThread::OnPostGarbageCollect);
}

FAudioThread::~FAudioThread()
{
	FCoreUObjectDelegates::PreGarbageCollect.RemoveAll(this);
	FCoreUObjectDelegates::PostGarbageCollect.RemoveAll(this);

	FPlatformProcess::ReturnSynchEventToPool(TaskGraphBoundSyncEvent);
	TaskGraphBoundSyncEvent = nullptr;
}

void FAudioThread::SuspendAudioThread()
{
	check(IsInGameThread());
	if (IsAudioThreadRunning())
	{
		++SuspensionCount;

		// Make GC wait on the audio thread finishing processing
		FAudioCommandFence AudioFence;
		AudioFence.BeginFence();
		AudioFence.Wait();

		CachedAudioThreadId = GAudioThreadId;
		GAudioThreadId = 0; // While we are suspended we will pretend we have no audio thread
		bIsAudioThreadRunning = false;

		FPlatformMisc::MemoryBarrier();
	}
}

void FAudioThread::ResumeAudioThread()
{
	check(IsInGameThread());
	if (SuspensionCount > 0)
	{
		--SuspensionCount;

		if (SuspensionCount == 0)
		{
			GAudioThreadId = CachedAudioThreadId;
			bIsAudioThreadRunning = true;

			FPlatformMisc::MemoryBarrier();
		}
	}
}

void FAudioThread::OnPreGarbageCollect()
{
	SuspendAudioThread();
}

void FAudioThread::OnPostGarbageCollect()
{
	ResumeAudioThread();
}

bool FAudioThread::Init()
{ 
	GAudioThreadId = FPlatformTLS::GetCurrentThreadId();
	return true; 
}

void FAudioThread::Exit()
{
	GAudioThreadId = 0;
}

uint32 FAudioThread::Run()
{
	FMemory::SetupTLSCachesOnCurrentThread();
	FPlatformProcess::SetupAudioThread();

#if PLATFORM_WINDOWS
	if ( !FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash )
	{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__try
#endif
		{
			AudioThreadMain( TaskGraphBoundSyncEvent );
		}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__except(ReportCrash(GetExceptionInformation()))
		{
			AudioThreadError = GErrorHist;

			// Use a memory barrier to ensure that the game thread sees the write to AudioThreadError before
			// the write to GIsAudioThreadHealthy.
			FPlatformMisc::MemoryBarrier();

			bIsAudioThreadHealthy = false;
		}
#endif
	}
	else
#endif // PLATFORM_WINDOWS
	{
		AudioThreadMain( TaskGraphBoundSyncEvent );
	}
	FMemory::ClearAndDisableTLSCachesOnCurrentThread();
	return 0;
}

void FAudioThread::SetUseThreadedAudio(const bool bInUseThreadedAudio)
{
	if (bIsAudioThreadRunning && !bInUseThreadedAudio)
	{
		UE_LOG(LogAudio, Error, TEXT("You cannot disable using threaded audio once the thread has already begun running."));
	}
	else
	{
		bUseThreadedAudio = bInUseThreadedAudio;
	}
}

void FAudioThread::RunCommandOnAudioThread(TFunction<void()> InFunction, const TStatId InStatId)
{
	if (bIsAudioThreadRunning)
	{
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(InFunction), InStatId, nullptr, ENamedThreads::AudioThread);
	}
	else
	{
		FScopeCycleCounter ScopeCycleCounter(InStatId);
		InFunction();
	}
}

void FAudioThread::RunCommandOnGameThread(TFunction<void()> InFunction, const TStatId InStatId)
{
	if (bIsAudioThreadRunning)
	{
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(InFunction), InStatId, nullptr, ENamedThreads::GameThread);
	}
	else
	{
		FScopeCycleCounter ScopeCycleCounter(InStatId);
		InFunction();
	}
}

static FString BuildAudioThreadName( uint32 ThreadIndex )
{
	return FString::Printf( TEXT( "%s %u" ), *FName( NAME_AudioThread ).GetPlainNameString(), ThreadIndex );
}

void FAudioThread::StartAudioThread()
{
	if (bUseThreadedAudio)
	{
		check(GAudioThread == nullptr);

		static uint32 ThreadCount = 0;

		bIsAudioThreadRunning = true;

		// Create the audio thread.
		AudioThreadRunnable = new FAudioThread();

		GAudioThread = FRunnableThread::Create(AudioThreadRunnable, *BuildAudioThreadName(ThreadCount), 0, TPri_BelowNormal, FPlatformAffinity::GetAudioThreadMask());

		// Wait for audio thread to have taskgraph bound before we dispatch any tasks for it.
		((FAudioThread*)AudioThreadRunnable)->TaskGraphBoundSyncEvent->Wait();

		// ensure the thread has actually started and is idling
		FAudioCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();

		ThreadCount++;
	}
}

void FAudioThread::StopAudioThread()
{
	// This function is not thread-safe. Ensure it is only called by the main game thread.
	check( IsInGameThread() );

	if (!bIsAudioThreadRunning)
	{
		return;
	}

	// unregister
	IConsoleManager::Get().RegisterThreadPropagation();

	FGraphEventRef QuitTask = TGraphTask<FReturnGraphTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(ENamedThreads::AudioThread);

	// Busy wait while BP debugging, to avoid opportunistic execution of game thread tasks
	// If the game thread is already executing tasks, then we have no choice but to spin
	if (GIntraFrameDebuggingGameThread || FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread) ) 
	{
		while ((QuitTask.GetReference() != nullptr) && !QuitTask->IsComplete())
		{
			FPlatformProcess::Sleep(0.0f);
		}
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_StopAudioThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(QuitTask, ENamedThreads::GameThread_Local);
	}

	// Wait for the audio thread to return.
	GAudioThread->WaitForCompletion();

	bIsAudioThreadRunning = false;

	// Destroy the audio thread objects.
	delete GAudioThread;
	GAudioThread = nullptr;
			
	delete AudioThreadRunnable;
	AudioThreadRunnable = nullptr;
}

void FAudioThread::CheckAudioThreadHealth()
{
	if(!bIsAudioThreadHealthy)
	{
		GErrorHist[0] = 0;
		GIsCriticalError = false;
		UE_LOG(LogAudio, Fatal, TEXT("Audio thread exception:\r\n%s"),*AudioThreadError);
	}

	if (IsInGameThread())
	{
		GLog->FlushThreadedLogs();
		SCOPE_CYCLE_COUNTER(STAT_PumpMessages);
		FPlatformMisc::PumpMessages(false);
	}
}

bool FAudioThread::IsAudioThreadHealthy()
{
	return bIsAudioThreadHealthy;
}

void FAudioCommandFence::BeginFence()
{
	if (FAudioThread::IsAudioThreadRunning())
	{
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.FenceAudioCommand"),
			STAT_FNullGraphTask_FenceAudioCommand,
			STATGROUP_TaskGraphTasks);

		CompletionEvent = TGraphTask<FNullGraphTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(
			GET_STATID(STAT_FNullGraphTask_FenceAudioCommand), ENamedThreads::AudioThread);
	}
}

bool FAudioCommandFence::IsFenceComplete() const
{
	if (!FAudioThread::IsAudioThreadRunning())
	{
		return true;
	}
	check(IsInGameThread() || IsInAsyncLoadingThread());
	FAudioThread::CheckAudioThreadHealth();
	if (!CompletionEvent.GetReference() || CompletionEvent->IsComplete())
	{
		CompletionEvent = NULL; // this frees the handle for other uses, the NULL state is considered completed
		return true;
	}
	return false;
}

static int32 GTimeToBlockOnAudioFence = 1;
static FAutoConsoleVariableRef CVarTimeToBlockOnAudioFence(
	TEXT("g.TimeToBlockOnAudioFence"),
	GTimeToBlockOnAudioFence,
	TEXT("Number of milliseconds the game thread should block when waiting on a audio thread fence.")
	);

/**
 * Block the game thread waiting for a task to finish on the audio thread.
 */
static void GameThreadWaitForTask(const FGraphEventRef& Task, bool bEmptyGameThreadTasks = false)
{
	check(IsInGameThread());
	check(IsValidRef(Task));

	if (!Task->IsComplete())
	{
		SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);
		{
			static int32 NumRecursiveCalls = 0;
		
			// Check for recursion. It's not completely safe but because we pump messages while 
			// blocked it is expected.
			NumRecursiveCalls++;
			if (NumRecursiveCalls > 1)
			{
				UE_LOG(LogAudio,Warning,TEXT("Wait on Audio Thread called recursively! %d calls on the stack."), NumRecursiveCalls);
			}
			if (NumRecursiveCalls > 1 || FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread))
			{
				bEmptyGameThreadTasks = false; // we don't do this on recursive calls or if we are at a blueprint breakpoint
			}

			// Grab an event from the pool and fire off a task to trigger it.
			FEvent* Event = FPlatformProcess::GetSynchEventFromPool();
			FTaskGraphInterface::Get().TriggerEventWhenTaskCompletes(Event, Task, ENamedThreads::GameThread);

			// Check audio thread health needs to be called from time to
			// time in order to make sure the audio thread hasn't crashed :)
			bool bDone;
			uint32 WaitTime = FMath::Clamp<uint32>(GTimeToBlockOnAudioFence, 0, 33);
			do
			{
				FAudioThread::CheckAudioThreadHealth();
				if (bEmptyGameThreadTasks)
				{
					// process gamethread tasks if there are any
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
				}
				bDone = Event->Wait(WaitTime);
			}
			while (!bDone);

			// Return the event to the pool and decrement the recursion counter.
			FPlatformProcess::ReturnSynchEventToPool(Event);
			Event = nullptr;
			NumRecursiveCalls--;
		}
	}
}

/**
 * Waits for pending fence commands to retire.
 */
void FAudioCommandFence::Wait(bool bProcessGameThreadTasks) const
{
	if (!IsFenceComplete())
	{
#if 0
		// on most platforms this is a better solution because it doesn't spin
		// windows needs to pump messages
		if (bProcessGameThreadTasks)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FAudioCommandFence_Wait);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompletionEvent, ENamedThreads::GameThread);
		}
#endif
		GameThreadWaitForTask(CompletionEvent, bProcessGameThreadTasks);
	}
}
