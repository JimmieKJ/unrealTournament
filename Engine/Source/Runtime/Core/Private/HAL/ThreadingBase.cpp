// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "CorePrivatePCH.h"
#include "EventPool.h"
#include "LockFreeList.h"
#include "StatsData.h"


/** The global thread pool */
FQueuedThreadPool* GThreadPool = nullptr;


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

CORE_API FRunnableThread* GRenderingThread = nullptr;

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
CORE_API FRunnableThread* GRHIThread = nullptr;
// Fake threads

// Core version of IsInAsyncLoadingThread
static bool IsInAsyncLoadingThreadCoreInternal()
{
	// No async loading in Core
	return false;
}
bool(*IsInAsyncLoadingThread)() = &IsInAsyncLoadingThreadCoreInternal;

/**
 * Fake thread created when multi-threading is disabled.
 */
class FFakeThread : public FRunnableThread
{
	/** Thread Id pool */
	static uint32 ThreadIdCounter;

	/** Thread is suspended. */
	bool bIsSuspended;

	/** Runnable object associated with this thread. */
	FSingleThreadRunnable* Runnable;

public:

	/** Constructor. */
	FFakeThread()
		: bIsSuspended(false)
		, Runnable(nullptr)
	{
		ThreadID = ThreadIdCounter++;
		// Auto register with single thread manager.
		FSingleThreadManager::Get().AddThread(this);
	}

	/** Virtual destructor. */
	virtual ~FFakeThread()
	{
		// Remove from the manager.
		FSingleThreadManager::Get().RemoveThread(this);
	}

	/** Tick one time per frame. */
	void Tick()
	{
		if (Runnable && !bIsSuspended)
		{
			Runnable->Tick();
		}
	}

public:

	// FRunnableThread interface

	virtual void SetThreadPriority(EThreadPriority NewPriority) override
	{
		// Not relevant.
	}

	virtual void Suspend(bool bShouldPause) override
	{
		bIsSuspended = bShouldPause;
	}

	virtual bool Kill(bool bShouldWait) override
	{
		FSingleThreadManager::Get().RemoveThread(this);
		return true;
	}

	virtual void WaitForCompletion() override
	{
		FSingleThreadManager::Get().RemoveThread(this);
	}

	virtual bool CreateInternal(FRunnable* InRunnable, const TCHAR* ThreadName,
		uint32 InStackSize,
		EThreadPriority InThreadPri, uint64 InThreadAffinityMask) override

	{
		Runnable = InRunnable->GetSingleThreadInterface();
		if (Runnable)
		{
			InRunnable->Init();
		}		
		return Runnable != nullptr;
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
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSingleThreadManager_Tick);

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

FScopedEvent::FScopedEvent()
	: Event(FEventPool<EEventPoolTypes::AutoReset>::Get().GetEventFromPool())
{ }

FScopedEvent::~FScopedEvent()
{
	Event->Wait();
	FEventPool<EEventPoolTypes::AutoReset>::Get().ReturnToPool(Event);
	Event = nullptr;
}

/*-----------------------------------------------------------------------------
	FRunnableThread
-----------------------------------------------------------------------------*/

uint32 FRunnableThread::RunnableTlsSlot = FRunnableThread::GetTlsSlot();

uint32 FRunnableThread::GetTlsSlot()
{
	check( IsInGameThread() );
	uint32 TlsSlot = FPlatformTLS::AllocTlsSlot();
	check( FPlatformTLS::IsValidTlsSlot( TlsSlot ) );
	return TlsSlot;
}

FRunnableThread::FRunnableThread()
	: Runnable(nullptr)
	, ThreadInitSyncEvent(nullptr)
	, ThreadAffinityMask(FPlatformAffinity::GetNoAffinityMask())
	, ThreadPriority(TPri_Normal)
	, ThreadID(0)
{
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
	FRunnableThread* NewThread = nullptr;
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
				NewThread = nullptr;
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
			NewThread = nullptr;
		}
	}

#if	STATS
	if( NewThread )
	{
		FStartupMessages::Get().AddThreadMetadata( FName( *NewThread->GetThreadName() ), NewThread->GetThreadID() );
	}
#endif // STATS

	return NewThread;
}

void FRunnableThread::SetTls()
{
	// Make sure it's called from the owning thread.
	check( ThreadID == FPlatformTLS::GetCurrentThreadId() );
	check( RunnableTlsSlot );
	FPlatformTLS::SetTlsValue( RunnableTlsSlot, this );
}

void FRunnableThread::FreeTls()
{
	// Make sure it's called from the owning thread.
	check( ThreadID == FPlatformTLS::GetCurrentThreadId() );
	check( RunnableTlsSlot );
	FPlatformTLS::SetTlsValue( RunnableTlsSlot, nullptr );

	// Delete all FTlsAutoCleanup objects created for this thread.
	for( auto& Instance : TlsInstances )
	{
		delete Instance;
		Instance = nullptr;
	}
}

/*-----------------------------------------------------------------------------
	FQueuedThread
-----------------------------------------------------------------------------*/

/**
 * This is the interface used for all poolable threads. The usage pattern for
 * a poolable thread is different from a regular thread and this interface
 * reflects that. Queued threads spend most of their life cycle idle, waiting
 * for work to do. When signaled they perform a job and then return themselves
 * to their owning pool via a callback and go back to an idle state.
 */
class FQueuedThread
	: public FRunnable
{
protected:

	/** The event that tells the thread there is work to do. */
	FEvent* DoWorkEvent;

	/** If true, the thread should exit. */
	volatile int32 TimeToDie;

	/** The work this thread is doing. */
	IQueuedWork* volatile QueuedWork;

	/** The pool this thread belongs to. */
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
			IQueuedWork* LocalQueuedWork = QueuedWork;
			QueuedWork = nullptr;
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

	/** Default constructor **/
	FQueuedThread()
		: DoWorkEvent(nullptr)
		, TimeToDie(0)
		, QueuedWork(nullptr)
		, OwningThreadPool(nullptr)
		, Thread(nullptr)
	{ }

	/**
	 * Creates the thread with the specified stack size and creates the various
	 * events to be able to communicate with it.
	 *
	 * @param InPool The thread pool interface used to place this thread back into the pool of available threads when its work is done
	 * @param InStackSize The size of the stack to create. 0 means use the current thread's stack size
	 * @param ThreadPriority priority of new thread
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual bool Create(class FQueuedThreadPool* InPool,uint32 InStackSize = 0,EThreadPriority ThreadPriority=TPri_Normal)
	{
		static int32 PoolThreadIndex = 0;
		const FString PoolThreadName = FString::Printf( TEXT( "PoolThread %d" ), PoolThreadIndex );
		PoolThreadIndex++;

		OwningThreadPool = InPool;
		DoWorkEvent = FPlatformProcess::GetSynchEventFromPool();
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
		FPlatformProcess::ReturnSynchEventToPool(DoWorkEvent);
		DoWorkEvent = nullptr;
		delete Thread;
		return bDidExitOK;
	}

	/**
	 * Tells the thread there is work to be done. Upon completion, the thread
	 * is responsible for adding itself back into the available pool.
	 *
	 * @param InQueuedWork The queued work to perform
	 */
	void DoWork(IQueuedWork* InQueuedWork)
	{
		check(QueuedWork == nullptr && "Can't do more than one task at a time");
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

	/** The work queue to pull from. */
	TArray<IQueuedWork*> QueuedWork;
	
	/** The thread pool to dole work out to. */
	TArray<FQueuedThread*> QueuedThreads;

	/** All threads in the pool. */
	TArray<FQueuedThread*> AllThreads;

	/** The synchronization object used to protect access to the queued work. */
	FCriticalSection* SynchQueue;

	/** If true, indicates the destruction process has taken place. */
	bool TimeToDie;

public:

	/** Default constructor. */
	FQueuedThreadPoolBase()
		: SynchQueue(nullptr)
		, TimeToDie(0)
	{ }

	/** Virtual destructor (cleans up the synchronization objects). */
	virtual ~FQueuedThreadPoolBase()
	{
		Destroy();
	}

	virtual bool Create(uint32 InNumQueuedThreads,uint32 StackSize = (32 * 1024),EThreadPriority ThreadPriority=TPri_Normal) override
	{
		// Make sure we have synch objects
		bool bWasSuccessful = true;
		check(SynchQueue == nullptr);
		SynchQueue = new FCriticalSection();
		FScopeLock Lock(SynchQueue);
		// Presize the array so there is no extra memory allocated
		QueuedThreads.Empty(InNumQueuedThreads);

		// Check for stack size override.
		if( OverrideStackSize > StackSize )
		{
			StackSize = OverrideStackSize;
		}

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
			SynchQueue = nullptr;
		}
	}

	void AddQueuedWork(IQueuedWork* InQueuedWork) override
	{
		if (TimeToDie)
		{
			InQueuedWork->Abandon();
			return;
		}
		check(InQueuedWork != nullptr);
		FQueuedThread* Thread = nullptr;
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
		if (Thread != nullptr)
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

	virtual bool RetractQueuedWork(IQueuedWork* InQueuedWork) override
	{
		if (TimeToDie)
		{
			return false; // no special consideration for this, refuse the retraction and let shutdown proceed
		}
		check(InQueuedWork != nullptr);
		check(SynchQueue);
		FScopeLock sl(SynchQueue);
		return !!QueuedWork.RemoveSingle(InQueuedWork);
	}

	virtual IQueuedWork* ReturnToPoolOrGetNextJob(FQueuedThread* InQueuedThread) override
	{
		check(InQueuedThread != nullptr);
		IQueuedWork* Work = nullptr;
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

uint32 FQueuedThreadPool::OverrideStackSize = 0;

FQueuedThreadPool* FQueuedThreadPool::Allocate()
{
	return new FQueuedThreadPoolBase;
}


/*-----------------------------------------------------------------------------
	FThreadSingletonInitializer
-----------------------------------------------------------------------------*/

FTlsAutoCleanup* FThreadSingletonInitializer::Get( const TFunctionRef<FTlsAutoCleanup*()>& CreateInstance, uint32& TlsSlot )
{
	if( TlsSlot == 0 )
	{
		const uint32 ThisTlsSlot = FPlatformTLS::AllocTlsSlot();
		check( FPlatformTLS::IsValidTlsSlot( ThisTlsSlot ) );
		const uint32 PrevTlsSlot = FPlatformAtomics::InterlockedCompareExchange( (int32*)&TlsSlot, (int32)ThisTlsSlot, 0 );
		if( PrevTlsSlot != 0 )
		{
			FPlatformTLS::FreeTlsSlot( ThisTlsSlot );
		}
	}
	FTlsAutoCleanup* ThreadSingleton = (FTlsAutoCleanup*)FPlatformTLS::GetTlsValue( TlsSlot );
	if( !ThreadSingleton )
	{
		ThreadSingleton = CreateInstance();
		ThreadSingleton->Register();
		FPlatformTLS::SetTlsValue( TlsSlot, ThreadSingleton );
	}
	return ThreadSingleton;
}

void FTlsAutoCleanup::Register()
{
	FRunnableThread* RunnableThread = FRunnableThread::GetRunnableThread();
	if( RunnableThread )
	{
		RunnableThread->TlsInstances.Add( this );
	}
}



FMultiReaderSingleWriterGT::FMultiReaderSingleWriterGT()
{
	CanRead = TFunction<bool()>([=]() { return FPlatformAtomics::InterlockedCompareExchange(&CriticalSection.Action, ReadingAction, NoAction) == ReadingAction; });
	CanWrite = TFunction<bool()>([=]() { return FPlatformAtomics::InterlockedCompareExchange(&CriticalSection.Action, WritingAction, NoAction) == WritingAction; });
}

void FMultiReaderSingleWriterGT::LockRead()
{
	if (!IsInGameThread())
	{
		FPlatformProcess::ConditionalSleep(CanRead);
	}
	CriticalSection.ReadCounter.Increment();
}

void FMultiReaderSingleWriterGT::UnlockRead()
{
	if (CriticalSection.ReadCounter.Decrement() != 0)
	{
		return;
	}

	if (!IsInGameThread())
	{
		FPlatformAtomics::InterlockedExchange(&CriticalSection.Action, NoAction);
	}
}

void FMultiReaderSingleWriterGT::LockWrite()
{
	check(IsInGameThread());
	FPlatformProcess::ConditionalSleep(CanWrite);
	CriticalSection.WriteCounter.Increment();
}

void FMultiReaderSingleWriterGT::UnlockWrite()
{
	if (CriticalSection.WriteCounter.Decrement() == 0)
	{
		FPlatformAtomics::InterlockedExchange(&CriticalSection.Action, NoAction);
	}
}
