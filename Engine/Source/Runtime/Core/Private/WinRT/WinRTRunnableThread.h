// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * This is the base interface for all runnable thread classes. It specifies the
 * methods used in managing its life cycle.
 */
class FRunnableThreadWinRT : public FRunnableThread
{
	/**
	 * The thread handle for the thread
	 */
	HANDLE Thread;

	/**
	 * Helper function to set thread names, visible by the debugger.
	 * @param ThreadID		Thread ID for the thread who's name is going to be set
	 * @param ThreadName	Name to set
	 */
	static void SetThreadName( uint32 ThreadID, LPCSTR ThreadName )
	{
		/**
		 * Code setting the thread name for use in the debugger.
		 *
		 * http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
		 */
		const uint32 MS_VC_EXCEPTION=0x406D1388;

		struct THREADNAME_INFO
		{
			uint32 dwType;		// Must be 0x1000.
			LPCSTR szName;		// Pointer to name (in user addr space).
			uint32 dwThreadID;	// Thread ID (-1=caller thread).
			uint32 dwFlags;		// Reserved for future use, must be zero.
		};

		// on the xbox setting thread names messes up the XDK COM API that UnrealConsole uses so check to see if they have been
		// explicitly enabled
		ThreadEmulation::Sleep(10);
		THREADNAME_INFO ThreadNameInfo;
		ThreadNameInfo.dwType		= 0x1000;
		ThreadNameInfo.szName		= ThreadName;
		ThreadNameInfo.dwThreadID	= ThreadID;
		ThreadNameInfo.dwFlags		= 0;

		__try
		{
			RaiseException( MS_VC_EXCEPTION, 0, sizeof(ThreadNameInfo)/sizeof(ULONG_PTR), (ULONG_PTR*)&ThreadNameInfo );
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	/**
	 * The thread entry point. Simply forwards the call on to the right
	 * thread main function
	 */
	static ::DWORD STDCALL _ThreadProc(LPVOID pThis)
	{
		check(pThis);
		return ((FRunnableThreadWinRT*)pThis)->Run();
	}

	/**
	 * The real thread entry point. It calls the Init/Run/Exit methods on
	 * the runnable object
	 */
	uint32 Run()
	{
		// Assume we'll fail init
		uint32 ExitCode = 1;
		check(Runnable);

		ThreadID = GetCurrentThreadId();

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

		return ExitCode;
	}

public:
	FRunnableThreadWinRT()
		: Thread(nullptr)
	{
	}

	~FRunnableThreadWinRT()
	{
		// Clean up our thread if it is still active
		if (Thread != NULL)
		{
			Kill(true);
		}
	}
	
	virtual void SetThreadPriority(EThreadPriority NewPriority) override
	{
		// Don't bother calling the OS if there is no need
		if (NewPriority != ThreadPriority)
		{
			ThreadPriority = NewPriority;
			// Change the priority on the thread
			ThreadEmulation::SetThreadPriority(Thread,
				ThreadPriority == TPri_AboveNormal ? THREAD_PRIORITY_ABOVE_NORMAL :
				ThreadPriority == TPri_BelowNormal ? THREAD_PRIORITY_BELOW_NORMAL :
				THREAD_PRIORITY_NORMAL);
		}
	}

	virtual void Suspend(bool bShouldPause = 1) override
	{
		check(Thread);
		if (bShouldPause == true)
		{
			//@todo.WinRT: How to handle this....
			//SuspendThread(Thread);
		}
		else
		{
			ThreadEmulation::ResumeThread(Thread);
		}
	}

	virtual bool Kill(bool bShouldWait = false) override
	{
		check(Thread && "Did you forget to call Create()?");
		bool bDidExitOK = true;
		// Let the runnable have a chance to stop without brute force killing
		if (Runnable)
		{
			Runnable->Stop();
		}
		// If waiting was specified, wait the amount of time. If that fails,
		// brute force kill that thread. Very bad as that might leak.
		if (bShouldWait == true)
		{
			// Wait indefinitely for the thread to finish.  IMPORTANT:  It's not safe to just go and
			// kill the thread with TerminateThread() as it could have a mutex lock that's shared
			// with a thread that's continuing to run, which would cause that other thread to
			// dead-lock.  (This can manifest itself in code as simple as the synchronization
			// object that is used by our logging output classes.  Trust us, we've seen it!)
			//WaitForSingleObject(Thread,INFINITE);
			WaitForSingleObjectEx(Thread, INFINITE, FALSE);
		}
		// Now clean up the thread handle so we don't leak
		CloseHandle(Thread);
		Thread = NULL;

		return bDidExitOK;
	}

	virtual void WaitForCompletion() override
	{
		// Block until this thread exits
		//WaitForSingleObject(Thread,INFINITE);
		WaitForSingleObjectEx(Thread, INFINITE, FALSE);
	}

	virtual uint32 GetThreadID() override
	{
		return ThreadID;
	}

	virtual FString GetThreadName() override
	{
		return ThreadName;
	}

protected:
	virtual bool CreateInternal(FRunnable* InRunnable, const TCHAR* InThreadName,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0) override
	{
		check(InRunnable);
		Runnable = InRunnable;
		ThreadAffinityMask = InThreadAffinityMask;

		// Create a sync event to guarantee the Init() function is called first
		ThreadInitSyncEvent	= FPlatformProcess::GetSynchEventFromPool(true);

		// Create the new thread
		Thread = ThreadEmulation::CreateThread(NULL,InStackSize,_ThreadProc,this,0,(::DWORD *)&ThreadID);
		// If it fails, clear all the vars
		if (Thread == NULL)
		{
			Runnable = NULL;
		}
		else
		{
			// Let the thread start up, then set the name for debug purposes.
			ThreadInitSyncEvent->Wait(INFINITE);
			// Cheat and set the threadID to the handle...
//			ThreadID = (uint32)Thread;
			ThreadName = InThreadName ? InThreadName : TEXT("Unnamed UE4");
			SetThreadName( ThreadID, TCHAR_TO_ANSI( *ThreadName ) );
			SetThreadPriority(InThreadPri);
		}

		// Cleanup the sync event
		FPlatformProcess::ReturnSynchEventToPool(ThreadInitSyncEvent);
		ThreadInitSyncEvent = nullptr;
		return Thread != NULL;
	}
};
