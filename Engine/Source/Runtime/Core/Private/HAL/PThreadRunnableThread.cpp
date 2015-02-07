// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#if	PLATFORM_USE_PTHREADS

#include "PThreadRunnableThread.h"

uint32 FRunnableThreadPThread::Run()
{
	ThreadIsRunning = true;
	// Assume we'll fail init
	uint32 ExitCode = 1;
	check(Runnable);

	// Initialize the runnable object
	if (Runnable->Init() == true)
	{
		// Initialization has completed, release the sync event
		ThreadInitSyncEvent->Trigger();
		// Now run the task that needs to be done
		ExitCode = Runnable->Run();
		// Allow any allocated resources to be cleaned up
		Runnable->Exit();
	}
	else
	{
		// Initialization has failed, release the sync event
		ThreadInitSyncEvent->Trigger();
	}

#if STATS
	FThreadStats::Shutdown();
#endif

	// Clean ourselves up without waiting
	ThreadIsRunning = false;


	return ExitCode;
}


#endif // PLATFORM_USE_PTHREADS