// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MoviePlayerPrivatePCH.h"
#include "MoviePlayer.h"

#include "Engine.h"
#include "SlateBasics.h"
#include "SpinLock.h"
#include "MoviePlayerThreading.h"
#include "DefaultGameMoviePlayer.h"



/**
 * The Slate thread is simply run on a worker thread.
 * Slate is run on another thread because the game thread (where Slate is usually run)
 * is blocked loading things. Slate is very modular, which makes it very easy to run on another
 * thread with no adverse effects.
 * It does not enqueue render commands, because the RHI is not thread safe. Thus, it waits to
 * enqueue render commands until the render thread tickables ticks, and then it calls them there.
 */
class FSlateLoadingThreadTask : public FRunnable
{
public:
	FSlateLoadingThreadTask(class FSlateLoadingSynchronizationMechanism& InSyncMechanism)
		: SyncMechanism(&InSyncMechanism)
	{
	}

	/** FRunnable interface */
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
private:
	/** Hold a handle to our parent sync mechanism which handles all of our threading locks */
	class FSlateLoadingSynchronizationMechanism* SyncMechanism;
};



FSlateLoadingSynchronizationMechanism::FSlateLoadingSynchronizationMechanism()
{
}

FSlateLoadingSynchronizationMechanism::~FSlateLoadingSynchronizationMechanism()
{
	DestroySlateThread();
}

void FSlateLoadingSynchronizationMechanism::Initialize()
{
	ResetSlateDrawPassEnqueued();
	SetSlateMainLoopRunning();

	MainLoop.Lock();

	SlateRunnableTask = new FSlateLoadingThreadTask( *this );
	SlateLoadingThread = FRunnableThread::Create(SlateRunnableTask, TEXT("SlateLoadingThread"));
}

void FSlateLoadingSynchronizationMechanism::DestroySlateThread()
{
	if (SlateLoadingThread)
	{
		IsRunningSlateMainLoop.Reset();

		MainLoop.BlockUntilUnlocked();

		delete SlateLoadingThread;
		delete SlateRunnableTask;
		SlateLoadingThread = NULL;
		SlateRunnableTask = NULL;
	}
}

bool FSlateLoadingSynchronizationMechanism::IsSlateDrawPassEnqueued()
{
	return IsSlateDrawEnqueued.GetValue() != 0;
}

void FSlateLoadingSynchronizationMechanism::SetSlateDrawPassEnqueued()
{
	IsSlateDrawEnqueued.Set(1);
}

void FSlateLoadingSynchronizationMechanism::ResetSlateDrawPassEnqueued()
{
	IsSlateDrawEnqueued.Reset();
}

bool FSlateLoadingSynchronizationMechanism::IsSlateMainLoopRunning()
{
	return IsRunningSlateMainLoop.GetValue() != 0;
}

void FSlateLoadingSynchronizationMechanism::SetSlateMainLoopRunning()
{
	IsRunningSlateMainLoop.Set(1);
}

void FSlateLoadingSynchronizationMechanism::ResetSlateMainLoopRunning()
{
	IsRunningSlateMainLoop.Reset();
}

void FSlateLoadingSynchronizationMechanism::SlateThreadRunMainLoop()
{
	double LastTime = FPlatformTime::Seconds();

	while (IsSlateMainLoopRunning())
	{
		const double CurrentTime = FPlatformTime::Seconds();
		const double DeltaTime = CurrentTime - LastTime;

		// 60 fps max
		const double MaxTickRate = 1.0/60.0f;

		const double TimeToWait = MaxTickRate - DeltaTime;

		if( TimeToWait > 0 )
		{
			FPlatformProcess::Sleep(TimeToWait);
		}

		if (FSlateApplication::IsInitialized() && !IsSlateDrawPassEnqueued())
		{
			TSharedPtr<FSlateRenderer> SlateRenderer = FSlateApplication::Get().GetRenderer();
			FScopeLock ScopeLock(SlateRenderer->GetResourceCriticalSection());

			// We can't pump messages because this is not the main thread
			// and that does not work at least in Windows
			// (HWNDs can only be pumped on the thread they're created on)
			// Thus, this function does nothing on the Slate thread
			//FSlateApplication::Get().PumpMessages();
			FSlateApplication::Get().Tick();
			SetSlateDrawPassEnqueued();
		}

		LastTime = CurrentTime;
	}
	
	while (IsSlateDrawPassEnqueued())
	{
		FPlatformProcess::Sleep(0.1f);
	}
	
	MainLoop.Unlock();
}


bool FSlateLoadingThreadTask::Init()
{
	// First thing to do is set the slate loading thread ID
	// This guarantees all systems know that a slate thread exists
	GSlateLoadingThreadId = FPlatformTLS::GetCurrentThreadId();

	return true;
}

uint32 FSlateLoadingThreadTask::Run()
{
	check( GSlateLoadingThreadId == FPlatformTLS::GetCurrentThreadId() );

	SyncMechanism->SlateThreadRunMainLoop();

	// Tear down the slate loading thread ID
	GSlateLoadingThreadId = 0;

	return 0;
}

void FSlateLoadingThreadTask::Stop()
{
	SyncMechanism->ResetSlateDrawPassEnqueued();
	SyncMechanism->ResetSlateMainLoopRunning();
}
