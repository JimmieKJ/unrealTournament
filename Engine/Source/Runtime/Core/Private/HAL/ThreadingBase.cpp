// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "CorePrivatePCH.h"
#include "LockFreeList.h"
#include "StatsData.h"

/** The global thread pool */
FQueuedThreadPool*		GThreadPool						= NULL;

CORE_API bool IsInGameThread()
{
	// if the game thread is uninitialized, then we are calling this VERY early before other threads will have started up, so it will be the game thread
	return !GIsGameThreadIdInitialized || FPlatformTLS::GetCurrentThreadId() == GGameThreadId ||
		FPlatformTLS::GetCurrentThreadId() == GSlateLoadingThreadId;
}

CORE_API bool IsInSlateThread()
{
	// If this explicitly is a slate thread, not just the main thread running slate
	return GSlateLoadingThreadId != 0 && FPlatformTLS::GetCurrentThreadId() == GSlateLoadingThreadId;
}

CORE_API int32 GIsRenderingThreadSuspended = 0;

CORE_API FRunnableThread* GRenderingThread = NULL;

CORE_API bool IsInActualRenderingThread()
{
	return GRenderingThread && FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID();
}

CORE_API bool IsInRenderingThread()
{
	return !GRenderingThread || GIsRenderingThreadSuspended || (FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID());
}

CORE_API bool IsInParallelRenderingThread()
{
	return !GRenderingThread || GIsRenderingThreadSuspended || (FPlatformTLS::GetCurrentThreadId() != GGameThreadId);
}

CORE_API bool IsInRHIThread()
{
	return GRHIThread && FPlatformTLS::GetCurrentThreadId() == GRHIThread->GetThreadID();
}
CORE_API FRunnableThread* GRHIThread = NULL;
// Fake threads

/**
 * Fake thread created when multithreading is disabled.
 */
class FFakeThread : public FRunnableThread
{
	/** Thread Id pool */
	static uint32 ThreadIdCounter;

	/** Thread is suspended. */
	bool bIsSuspended;
	/** Name of this thread. */
	FString Name;
	/** Runnable object associated with this thread. */
	FSingleThreadRunnable* Runnable;
	/** Thread Id. */
	uint32 ThreadId;

public:

	/**
	 * Constructor.
	 */
	FFakeThread()
		: bIsSuspended(false)
		, Runnable(NULL)
		, ThreadId(ThreadIdCounter++)
	{
		// Auto register with single thread manager.
		FSingleThreadManager::Get().AddThread(this);
	}

	/**
	 * Destructor
	 */
	virtual ~FFakeThread()
	{
		// Remove from the manager.
		FSingleThreadManager::Get().RemoveThread(this);
	}

	/**
	 * Tick one time per frame
	 */
	void Tick()
	{
		if (Runnable && !bIsSuspended)
		{
			Runnable->Tick();
		}
	}

	// FRunnableThread interface
	virtual void SetThreadPriority(EThreadPriority NewPriority) override
	{
		// Not relevant.
	}

	virtual void Suspend(bool bShouldPause) override
	{
		bIsSuspended = bShouldPause;
	}

	virtual bool Kill(bool bShouldWait)
	{
		FSingleThreadManager::Get().RemoveThread(this);
		return true;
	}

	virtual void WaitForCompletion() override
	{
		FSingleThreadManager::Get().RemoveThread(this);
	}

	virtual uint32 GetThreadID() override
	{
		return ThreadId;
	}

	virtual FString GetThreadName() override
	{
		return Name;
	}

	virtual bool CreateInternal(FRunnable* InRunnable, const TCHAR* ThreadName,
		uint32 InStackSize,
		EThreadPriority InThreadPri, uint64 InThreadAffinityMask)

	{
		Runnable = InRunnable->GetSingleThreadInterface();
		if (Runnable)
		{
			InRunnable->Init();
		}		
		return Runnable != NULL;
	}
};
uint32 FFakeThread::ThreadIdCounter = 0xffff;

void FSingleThreadManager::AddThread(FFakeThread* Thread)
{
	ThreadList.Add(Thread);
}

void FSingleThreadManager::RemoveThread(FFakeThread* Thread)
{
	ThreadList.Remove(Thread);
}

void FSingleThreadManager::Tick()
{
	// Tick all registered threads.
	for (int32 RunnableIndex = 0; RunnableIndex < ThreadList.Num(); ++RunnableIndex)
	{
		ThreadList[RunnableIndex]->Tick();
	}
}

FSingleThreadManager& FSingleThreadManager::Get()
{
	static FSingleThreadManager Singleton;
	return Singleton;
}

class FEventPool
{
	TLockFreePointerList<FEvent> Pool;
public:
	static FEventPool& Get()
	{
		static FEventPool Singleton;
		return Singleton;
	}
	FEvent* GetEventFromPool()
	{
		FEvent* Result = Pool.Pop();
		if (!Result)
		{
			Result = FPlatformProcess::CreateSynchEvent();
		}
		check(Result);
		return Result;
	}
	void ReturnToPool(FEvent* Event)
	{
		check(Event);
		Pool.Push(Event);
	}
};

FEvent* FScopedEvent::GetEventFromPool()
{
	return FEventPool::Get().GetEventFromPool();
}

void FScopedEvent::ReturnToPool(FEvent* Event)
{
	FEventPool::Get().ReturnToPool(Event);
}

FRunnableThread::~FRunnableThread()
{
	ThreadDestroyedDelegate.Broadcast();
}

FRunnableThread* FRunnableThread::Create(
	class FRunnable* InRunnable,
	const TCHAR* ThreadName,
	bool bAutoDeleteSelf,
	bool bAutoDeleteRunnable,
	uint32 InStackSize,
	EThreadPriority InThreadPri,
	uint64 InThreadAffinityMask)
{
	return Create(InRunnable, ThreadName, InStackSize, InThreadPri, InThreadAffinityMask);
}

FRunnableThread* FRunnableThread::Create(
	class FRunnable* InRunnable, 
	const TCHAR* ThreadName,
	uint32 InStackSize,
	EThreadPriority InThreadPri, 
	uint64 InThreadAffinityMask)
{
	FRunnableThread* NewThread = NULL;
	if (FPlatformProcess::SupportsMultithreading())
	{
		check(InRunnable);
		// Create a new thread object
		NewThread = FPlatformProcess::CreateRunnableThread();
		if (NewThread)
		{
			// Call the thread's create method
			if (NewThread->CreateInternal(InRunnable,ThreadName,InStackSize,InThreadPri,InThreadAffinityMask) == false)
			{
				// We failed to start the thread correctly so clean up
				delete NewThread;
				NewThread = NULL;
			}
		}
	}
	else if (InRunnable->GetSingleThreadInterface())
	{
		// Create a fake thread when multithreading is disabled.
		NewThread = new FFakeThread();
		if (NewThread->CreateInternal(InRunnable,ThreadName,InStackSize,InThreadPri) == false)
		{
			// We failed to start the thread correctly so clean up
			delete NewThread;
			NewThread = NULL;
		}
	}

	if( NewThread )
	{
		FRunnableThread::GetThreadRegistry().Add( NewThread->GetThreadID(), NewThread );
#if	STATS
		FStartupMessages::Get().AddThreadMetadata( FName( *NewThread->GetThreadName() ), NewThread->GetThreadID() );
#endif // STATS
	}

	return NewThread;

}

/**
 * This is the interface used for all poolable threads. The usage pattern for
 * a poolable thread is different from a regular thread and this interface
 * reflects that. Queued threads spend most of their life cycle idle, waiting
 * for work to do. When signaled they perform a job and then return themselves
 * to their owning pool via a callback and go back to an idle state.
 */
class FQueuedThread : public FRunnable
{
protected:
	/**
	 * The event that tells the thread there is work to do
	 */
	FEvent* DoWorkEvent;

	/**
	 * If true, the thread should exit
	 */
	volatile int32 TimeToDie;

	/**
	 * The work this thread is doing
	 */
	FQueuedWork* volatile QueuedWork;

	/**
	 * The pool this thread belongs to
	 */
	class FQueuedThreadPool* OwningThreadPool;

	/** My Thread  */
	FRunnableThread* Thread;

	/**
	 * The real thread entry point. It waits for work events to be queued. Once
	 * an event is queued, it executes it and goes back to waiting.
	 */
	virtual uint32 Run() override
	{
		while (!TimeToDie)
		{
			// Wait for some work to do
			DoWorkEvent->Wait();
			FQueuedWork* LocalQueuedWork = QueuedWork;
			QueuedWork = NULL;
			FPlatformMisc::MemoryBarrier();
			check(LocalQueuedWork || TimeToDie); // well you woke me up, where is the job or termination request?
			while (LocalQueuedWork)
			{
				// Tell the object to do the work
				LocalQueuedWork->DoThreadedWork();
				// Let the object cleanup before we remove our ref to it
				LocalQueuedWork = OwningThreadPool->ReturnToPoolOrGetNextJob(this);
			} 
		}
		return 0;
	}

public:

	/** Constructor **/
	FQueuedThread()
		: DoWorkEvent(NULL)
		, TimeToDie(0)
		, QueuedWork(NULL)
		, OwningThreadPool(NULL)
		, Thread(NULL)
	{
	}

	/**
	 * Creates the thread with the specified stack size and creates the various
	 * events to be able to communicate with it.
	 *
	 * @param InPool The thread pool interface used to place this thread
	 * back into the pool of available threads when its work is done
	 * @param InStackSize The size of the stack to create. 0 means use the
	 * current thread's stack size
	 * @param ThreadPriority priority of new thread
	 *
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual bool Create(class FQueuedThreadPool* InPool,uint32 InStackSize = 0,EThreadPriority ThreadPriority=TPri_Normal)
	{
		static int32 PoolThreadIndex = 0;
		const FString PoolThreadName = FString::Printf( TEXT( "PoolThread %d" ), PoolThreadIndex );
		PoolThreadIndex++;

		OwningThreadPool = InPool;
		DoWorkEvent = FPlatformProcess::CreateSynchEvent();
		Thread = FRunnableThread::Create(this, *PoolThreadName, InStackSize, ThreadPriority, FPlatformAffinity::GetPoolThreadMask());
		check(Thread);
		return true;
	}
	
	/**
	 * Tells the thread to exit. If the caller needs to know when the thread
	 * has exited, it should use the bShouldWait value and tell it how long
	 * to wait before deciding that it is deadlocked and needs to be destroyed.
	 * NOTE: having a thread forcibly destroyed can cause leaks in TLS, etc.
	 *
	 * @return True if the thread exited graceful, false otherwise
	 */
	virtual bool KillThread()
	{
		bool bDidExitOK = true;
		// Tell the thread it needs to die
		FPlatformAtomics::InterlockedExchange(&TimeToDie,1);
		// Trigger the thread so that it will come out of the wait state if
		// it isn't actively doing work
		DoWorkEvent->Trigger();
		// If waiting was specified, wait the amount of time. If that fails,
		// brute force kill that thread. Very bad as that might leak.
		Thread->WaitForCompletion();
		// Clean up the event
		delete DoWorkEvent;
		DoWorkEvent = NULL;
		delete DoWorkEvent;
		delete Thread;
		return bDidExitOK;
	}

	/**
	 * Tells the thread there is work to be done. Upon completion, the thread
	 * is responsible for adding itself back into the available pool.
	 *
	 * @param InQueuedWork The queued work to perform
	 */
	void DoWork(FQueuedWork* InQueuedWork)
	{
		check(QueuedWork == NULL && "Can't do more than one task at a time");
		// Tell the thread the work to be done
		QueuedWork = InQueuedWork;
		FPlatformMisc::MemoryBarrier();
		// Tell the thread to wake up and do its job
		DoWorkEvent->Trigger();
	}

};

/**
 * Implementation of a queued thread pool.
 */
class FQueuedThreadPoolBase : public FQueuedThreadPool
{
protected:
	/**
	 * The work queue to pull from
	 */
	TArray<FQueuedWork*> QueuedWork;
	
	/**
	 * The thread pool to dole work out to
	 */
	TArray<FQueuedThread*> QueuedThreads;

	/**
	 * All threads in the pool
	 */
	TArray<FQueuedThread*> AllThreads;

	/**
	 * The synchronization object used to protect access to the queued work
	 */
	FCriticalSection* SynchQueue;

	/**
	 * If true, indicates the destruction process has taken place
	 */
	bool TimeToDie;

public:

	FQueuedThreadPoolBase()
		: SynchQueue(NULL)
		, TimeToDie(0)
	{
	}

	/**
	 * Clean up the synch objects
	 */
	virtual ~FQueuedThreadPoolBase()
	{
		Destroy();
	}

	virtual bool Create(uint32 InNumQueuedThreads,uint32 StackSize = (32 * 1024),EThreadPriority ThreadPriority=TPri_Normal) override
	{
		// Make sure we have synch objects
		bool bWasSuccessful = true;
		check(SynchQueue == NULL);
		SynchQueue = new FCriticalSection();
		FScopeLock Lock(SynchQueue);
		// Presize the array so there is no extra memory allocated
		QueuedThreads.Empty(InNumQueuedThreads);
		// Now create each thread and add it to the array
		for (uint32 Count = 0; Count < InNumQueuedThreads && bWasSuccessful == true; Count++)
		{
			// Create a new queued thread
			FQueuedThread* pThread = new FQueuedThread();
			// Now create the thread and add it if ok
			if (pThread->Create(this,StackSize,ThreadPriority) == true)
			{
				QueuedThreads.Add(pThread);
				AllThreads.Add(pThread);
			}
			else
			{
				// Failed to fully create so clean up
				bWasSuccessful = false;
				delete pThread;
			}
		}
		// Destroy any created threads if the full set was not successful
		if (bWasSuccessful == false)
		{
			Destroy();
		}
		return bWasSuccessful;
	}

	virtual void Destroy() override
	{
		if (SynchQueue)
		{
			{
				FScopeLock Lock(SynchQueue);
				TimeToDie = 1;
				FPlatformMisc::MemoryBarrier();
				// Clean up all queued objects
				for (int32 Index = 0; Index < QueuedWork.Num(); Index++)
				{
					QueuedWork[Index]->Abandon();
				}
				// Empty out the invalid pointers
				QueuedWork.Empty();
			}
			// wait for all threads to finish up
			while (1)
			{
				{
					FScopeLock Lock(SynchQueue);
					if (AllThreads.Num() == QueuedThreads.Num())
					{
						break;
					}
				}
				FPlatformProcess::Sleep(0.0f);
			}
			// Delete all threads
			{
				FScopeLock Lock(SynchQueue);
				// Now tell each thread to die and delete those
				for (int32 Index = 0; Index < AllThreads.Num(); Index++)
				{
					AllThreads[Index]->KillThread();
					delete AllThreads[Index];
				}
				QueuedThreads.Empty();
				AllThreads.Empty();
			}
			delete SynchQueue;
			SynchQueue = NULL;
		}
	}

	void AddQueuedWork(FQueuedWork* InQueuedWork) override
	{
		if (TimeToDie)
		{
			InQueuedWork->Abandon();
			return;
		}
		check(InQueuedWork != NULL);
		FQueuedThread* Thread = NULL;
		// Check to see if a thread is available. Make sure no other threads
		// can manipulate the thread pool while we do this.
		check(SynchQueue);
		FScopeLock sl(SynchQueue);
		if (QueuedThreads.Num() > 0)
		{
			// Figure out which thread is available
			int32 Index = QueuedThreads.Num() - 1;
			// Grab that thread to use
			Thread = QueuedThreads[Index];
			// Remove it from the list so no one else grabs it
			QueuedThreads.RemoveAt(Index);
		}
		// Was there a thread ready?
		if (Thread != NULL)
		{
			// We have a thread, so tell it to do the work
			Thread->DoWork(InQueuedWork);
		}
		else
		{
			// There were no threads available, queue the work to be done
			// as soon as one does become available
			QueuedWork.Add(InQueuedWork);
		}
	}

	virtual bool RetractQueuedWork(FQueuedWork* InQueuedWork) override
	{
		if (TimeToDie)
		{
			return false; // no special consideration for this, refuse the retraction and let shutdown proceed
		}
		check(InQueuedWork != NULL);
		check(SynchQueue);
		FScopeLock sl(SynchQueue);
		return !!QueuedWork.RemoveSingle(InQueuedWork);
	}

	virtual FQueuedWork* ReturnToPoolOrGetNextJob(FQueuedThread* InQueuedThread) override
	{
		check(InQueuedThread != NULL);
		FQueuedWork* Work = NULL;
		// Check to see if there is any work to be done
		FScopeLock sl(SynchQueue);
		if (TimeToDie)
		{
			check(!QueuedWork.Num());  // we better not have anything if we are dying
		}
		if (QueuedWork.Num() > 0)
		{
			// Grab the oldest work in the queue. This is slower than
			// getting the most recent but prevents work from being
			// queued and never done
			Work = QueuedWork[0];
			// Remove it from the list so no one else grabs it
			QueuedWork.RemoveAt(0);
		}
		if (!Work)
		{
			// There was no work to be done, so add the thread to the pool
			QueuedThreads.Add(InQueuedThread);
		}
		return Work;
	}
};

FQueuedThreadPool* FQueuedThreadPool::Allocate()
{
	return new FQueuedThreadPoolBase;
}

/*-----------------------------------------------------------------------------
	FThreadSingletonInitializer
-----------------------------------------------------------------------------*/

FThreadSingleton* FThreadSingletonInitializer::Get( const FThreadSingleton::TCreateSingletonFuncPtr CreateFunc, const FThreadSingleton::TDestroySingletonFuncPtr DestroyFunc, uint32& TlsSlot )
{
	check( CreateFunc != nullptr );
	check( DestroyFunc != nullptr );
	if( TlsSlot == 0 )
	{
		check( IsInGameThread() );
		TlsSlot = FPlatformTLS::AllocTlsSlot();
	}
	FThreadSingleton* ThreadSingleton = (FThreadSingleton*)FPlatformTLS::GetTlsValue( TlsSlot );
	if( !ThreadSingleton )
	{
		const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
		ThreadSingleton = (*CreateFunc)();
		FRunnableThread::GetThreadRegistry().Lock();
		FRunnableThread* RunnableThread = FRunnableThread::GetThreadRegistry().GetThread( ThreadId );
		if( RunnableThread )
		{
			RunnableThread->OnThreadDestroyed().AddRaw( ThreadSingleton, DestroyFunc );
		}
		FRunnableThread::GetThreadRegistry().Unlock();
		FPlatformTLS::SetTlsValue( TlsSlot, ThreadSingleton );
	}
	return ThreadSingleton;
}
