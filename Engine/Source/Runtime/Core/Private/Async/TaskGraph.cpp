// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "CorePrivatePCH.h"
#include "TaskGraphInterfaces.h"

DEFINE_LOG_CATEGORY_STATIC(LogTaskGraph, Log, All);

DEFINE_STAT(STAT_FReturnGraphTask);
DEFINE_STAT(STAT_FTriggerEventGraphTask);
DEFINE_STAT(STAT_ParallelFor);
DEFINE_STAT(STAT_ParallelForTask);

namespace ENamedThreads
{
	CORE_API Type RenderThread = ENamedThreads::GameThread; // defaults to game and is set and reset by the render thread itself
	CORE_API Type RenderThread_Local = ENamedThreads::GameThread_Local; // defaults to game local and is set and reset by the render thread itself
}

#define PROFILE_TASKGRAPH (0)
#if PROFILE_TASKGRAPH
	struct FProfileRec
	{
		const TCHAR* Name;
		FThreadSafeCounter NumSamplesStarted;
		FThreadSafeCounter NumSamplesFinished;
		uint32 Samples[1000];

		FProfileRec()
		{
			Name = nullptr;
		}
	};
	static FThreadSafeCounter NumProfileSamples;
	static void DumpProfile();
	struct FProfileRecScope
	{
		FProfileRec* Target;
		int32 SampleIndex;
		uint32 StartCycles;
		FProfileRecScope(FProfileRec* InTarget, const TCHAR* InName)
			: Target(InTarget)
			, SampleIndex(InTarget->NumSamplesStarted.Increment() - 1)
			, StartCycles(FPlatformTime::Cycles())
		{
			if (SampleIndex == 0 && !Target->Name)
			{
				Target->Name = InName;
			}
		}
		~FProfileRecScope()
		{
			if (SampleIndex < 1000)
			{
				Target->Samples[SampleIndex] = FPlatformTime::Cycles() - StartCycles;
				if (Target->NumSamplesFinished.Increment() == 1000)
				{
					Target->NumSamplesFinished.Reset();
					FPlatformMisc::MemoryBarrier();
					uint64 Total = 0;
					for (int32 Index = 0; Index < 1000; Index++)
					{
						Total += Target->Samples[Index];
					}
					float MsPer = FPlatformTime::GetSecondsPerCycle() * double(Total);
					UE_LOG(LogTemp, Display, TEXT("%6.4f ms / scope %s"),MsPer, Target->Name);

					Target->NumSamplesStarted.Reset();
				}
			}
		}
	};
	static FProfileRec ProfileRecs[10];
	static void DumpProfile()
	{

	}

	#define TASKGRAPH_SCOPE_CYCLE_COUNTER(Index, Name) \
		FProfileRecScope ProfileRecScope##Index(&ProfileRecs[Index], TEXT(#Name));


#else
	#define TASKGRAPH_SCOPE_CYCLE_COUNTER(Index, Name)
#endif



/** 
 *	Pointer to the task graph implementation singleton.
 *	Because of the multithreaded nature of this system an ordinary singleton cannot be used.
 *	FTaskGraphImplementation::Startup() creates the singleton and the constructor actually sets this value.
**/
class FTaskGraphImplementation;
struct FWorkerThread;

static FTaskGraphImplementation* TaskGraphImplementationSingleton = NULL;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

static struct FChaosMode 
{
	enum 
	{
		NumSamples = 45771
	};
	FThreadSafeCounter Current;
	float DelayTimes[NumSamples + 1]; 
	int32 Enabled;

	FChaosMode()
		: Enabled(0)
	{
		FRandomStream Stream((int32)FPlatformTime::Cycles());
		for (int32 Index = 0; Index < NumSamples; Index++)
		{
			DelayTimes[Index] = Stream.GetFraction();
		}
		// ave = .5
		for (int32 Cube = 0; Cube < 2; Cube++)
		{
			for (int32 Index = 0; Index < NumSamples; Index++)
			{
				DelayTimes[Index] *= Stream.GetFraction();
			}
		}
		// ave = 1/8
		for (int32 Index = 0; Index < NumSamples; Index++)
		{
			DelayTimes[Index] *= 0.00001f;
		}
		// ave = 0.00000125s
		for (int32 Zeros = 0; Zeros < NumSamples / 20; Zeros++)
		{
			int32 Index = Stream.RandHelper(NumSamples);
			DelayTimes[Index] = 0.0f;
		}
		// 95% the samples are now zero
		for (int32 Zeros = 0; Zeros < NumSamples / 100; Zeros++)
		{
			int32 Index = Stream.RandHelper(NumSamples);
			DelayTimes[Index] = .00005f;
		}
		// .001% of the samples are 5ms
	}
	FORCEINLINE void Delay()
	{
		if (Enabled)
		{
			uint32 MyIndex = (uint32)Current.Increment();
			MyIndex %= NumSamples;
			float DelayS = DelayTimes[MyIndex];
			if (DelayS > 0.0f)
			{
				FPlatformProcess::Sleep(DelayS);
			}
		}
	}
} GChaosMode;

static void EnableRandomizedThreads(const TArray<FString>& Args)
{
	GChaosMode.Enabled = !GChaosMode.Enabled;
	if (GChaosMode.Enabled)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Random sleeps are enabled."));
	}
	else
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Random sleeps are disabled."));
	}
}

static FAutoConsoleCommand TestRandomizedThreadsCommand(
	TEXT("TaskGraph.Randomize"),
	TEXT("Useful for debugging, adds random sleeps throughout the task graph."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&EnableRandomizedThreads)
	);

FORCEINLINE void TestRandomizedThreads()
{
	GChaosMode.Delay();
}

#else

FORCEINLINE void TestRandomizedThreads()
{
}

#endif

static int32 GNumWorkerThreadsToIgnore = 0;
static FAutoConsoleVariableRef CVarNumWorkerThreadsToIgnore(
	TEXT("TaskGraph.NumWorkerThreadsToIgnore"),
	GNumWorkerThreadsToIgnore,
	TEXT("Used to tune the number of task threads. Generally once you have found the right value, PlatformMisc::NumberOfWorkerThreadsToSpawn() should be hardcoded."),
	ECVF_Cheat
	);

static int32 GFastScheduler = 0;
#if USE_NEW_LOCK_FREE_LISTS
static int32 GFastSchedulerLatched = 1;
#else
static int32 GFastSchedulerLatched = 0;
#endif
static FAutoConsoleVariableRef CVarFastScheduler(
	TEXT("TaskGraph.FastScheduler"),
	GFastScheduler,
	TEXT("If > 0, then use the new experimental anythread task scheduler."),
	ECVF_Cheat
	);

// move to platform abstraction
#if PLATFORM_XBOXONE || PLATFORM_PS4
	#define PLATFORM_OK_TO_BURN_CPU (1)
#else
	#define PLATFORM_OK_TO_BURN_CPU (0)
#endif

#define USE_INTRUSIVE_TASKQUEUES (USE_NEW_LOCK_FREE_LISTS)

static int32 GConsoleSpinMode = PLATFORM_OK_TO_BURN_CPU * 2;
static int32 GConsoleSpinModeLatched = 0;
static FAutoConsoleVariableRef CVarConsoleSpinMode(
	TEXT("TaskGraph.ConsoleSpinMode"),
	GConsoleSpinMode,
	TEXT("If > 0, then we never allow all task threads to idle; one will always be spinning looking for work (if mode == 2, then we sleep0 when spinning); this relieve the calling thread from the buden of starting threads. Only active with TaskGraph.FastScheduler 1."),
	ECVF_Cheat
	);

#if USE_NEW_LOCK_FREE_LISTS
static int32 GMaxTasksToStartOnDequeue = 1;
#else
static int32 GMaxTasksToStartOnDequeue = 2;
#endif

static FAutoConsoleVariableRef CVarMaxTasksToStartOnDequeue(
	TEXT("TaskGraph.MaxTasksToStartOnDequeue"),
	GMaxTasksToStartOnDequeue,
	TEXT("Performance tweak, controls how many additional task threads a task thread starts when it grabs a list of new tasks. This only applies in TaskGraph.ConsoleSpinMode"),
	ECVF_Cheat
	);

/** 
 *	FTaskQueue
 *	High performance, SINGLE threaded, FIFO task queue for the private queue on named threads.
**/
class FTaskQueue
{
public:
	/** Constructor, sets the queue to the empty state. **/
	FTaskQueue()
		: StartIndex(0)
		, EndIndex(0)
	{
	}
	/** Returns the number of non-NULL items in the queue. **/
	FORCEINLINE int32 Num() const
	{
		CheckInvariants();
		return EndIndex - StartIndex;
	}
	/** 
	 *	Adds a task to the queue.
	 *	@param Task; the task to add to the queue
	**/
	FORCEINLINE void Enqueue(FBaseGraphTask* Task)
	{
		CheckInvariants();
		if (EndIndex >= Tasks.Num())
		{
			if (StartIndex >= ARRAY_EXPAND)
			{
				checkThreadGraph(Tasks[StartIndex - 1]==NULL);
				checkThreadGraph(Tasks[0]==NULL);
				FMemory::Memmove(Tasks.GetData(), &Tasks[StartIndex], (EndIndex - StartIndex) * sizeof(FBaseGraphTask*));
				EndIndex -= StartIndex;
				FMemory::Memzero(&Tasks[EndIndex],StartIndex * sizeof(FBaseGraphTask*)); // not needed in final
				StartIndex = 0;
			}
			else
			{
				Tasks.AddZeroed(ARRAY_EXPAND);
			}
		}
		checkThreadGraph(EndIndex < Tasks.Num() && Tasks[EndIndex]==NULL);
		Tasks[EndIndex++] = Task;
	}
	/** 
	 *	Pops a task off the queue.
	 *	@return The oldest task in the queue or NULL if the queue is empty
	**/
	FORCEINLINE FBaseGraphTask* Dequeue()
	{
		if (!Num())
		{
			return NULL;
		}
		FBaseGraphTask* ReturnValue = Tasks[StartIndex];
		Tasks[StartIndex++] = NULL;
		if (StartIndex == EndIndex)
		{
			// Queue is empty, reset to start
			StartIndex = 0;
			EndIndex = 0;
		}
		return ReturnValue;
	}
private:
	enum
	{
		/** Number of tasks by which to expand and compact the queue on **/
		ARRAY_EXPAND=1024
	};

	/** Internal function to verify the state of the object is legal. **/
	void CheckInvariants() const
	{
		checkThreadGraph(StartIndex <= EndIndex);
		checkThreadGraph(EndIndex <= Tasks.Num());
		checkThreadGraph(StartIndex >= 0 && EndIndex >= 0);
	}

	/** Array to hold the tasks, only the [StartIndex,EndIndex) range contains non-NULL tasks. **/
	TArray<FBaseGraphTask*> Tasks;

	/** Index of first non-NULL task in the queue unless StartIndex == EndIndex (which means empty) **/
	int32 StartIndex;

	/** Index of first NULL task in the queue after the non-NULL tasks, unless StartIndex == EndIndex (which means empty) **/
	int32 EndIndex;

};

/** 
 *	FTaskThreadBase
 *	Base class for a thread that executes tasks
 *	This class implements the FRunnable API, but external threads don't use that because those threads are created elsewhere.
**/
class FTaskThreadBase : public FRunnable, FSingleThreadRunnable
{
public:
	// Calls meant to be called from a "main" or supervisor thread.

	/** Constructor, initializes everything to unusable values. Meant to be called from a "main" thread. **/
	FTaskThreadBase()
		: ThreadId(ENamedThreads::AnyThread)
		, PerThreadIDTLSSlot(0xffffffff)
		, OwnerWorker(nullptr)
	{
		NewTasks.Reset(128);
	}

	/** 
	 *	Sets up some basic information for a thread. Meant to be called from a "main" thread. Also creates the stall event.
	 *	@param InThreadId; Thread index for this thread.
	 *	@param InPerThreadIDTLSSlot; TLS slot to store the pointer to me into (later)
	 *	@param bInAllowsStealsFromMe; If true, this is a worker thread and any other thread can steal tasks from my incoming queue.
	 *	@param bInStealsFromOthers If true, this is a worker thread and I will attempt to steal tasks when I run out of work.
	**/
	void Setup(ENamedThreads::Type InThreadId, uint32 InPerThreadIDTLSSlot, FWorkerThread* InOwnerWorker)
	{
		ThreadId = InThreadId;
		check(ThreadId >= 0);
		PerThreadIDTLSSlot = InPerThreadIDTLSSlot;
		OwnerWorker = InOwnerWorker;
	}

	// Calls meant to be called from "this thread".

	/** A one-time call to set the TLS entry for this thread. **/
	void InitializeForCurrentThread()
	{
		FPlatformTLS::SetTlsValue(PerThreadIDTLSSlot,OwnerWorker);
	}

	/** Return the index of this thread. **/
	ENamedThreads::Type GetThreadId() const
	{
		checkThreadGraph(OwnerWorker); // make sure we are started up
		return ThreadId;
	}

	/** Used for named threads to start processing tasks until the thread is idle and RequestQuit has been called. **/
	virtual void ProcessTasksUntilQuit(int32 QueueIndex) = 0;

	/** Used for named threads to start processing tasks until the thread is idle and RequestQuit has been called. **/
	virtual void ProcessTasksUntilIdle(int32 QueueIndex)
	{
		check(0);
	}

	/** 
	 *	Queue a task, assuming that this thread is the same as the current thread.
	 *	For named threads, these go directly into the private queue.
	 *	@param QueueIndex, Queue to enqueue for
	 *	@param Task Task to queue.
	 **/
	virtual void EnqueueFromThisThread(int32 QueueIndex, FBaseGraphTask* Task)
	{
		check(0);
	}

	// Calls meant to be called from any thread.

	/** 
	 *	Will cause the thread to return to the caller when it becomes idle. Used to return from ProcessTasksUntilQuit for named threads or to shut down unnamed threads. 
	 *	CAUTION: This will not work under arbitrary circumstances. For example you should not attempt to stop unnamed threads unless they are known to be idle.
	 *	Return requests for named threads should be submitted from that named thread as FReturnGraphTask does.
	 *	@param QueueIndex, Queue to request quit from
	**/
	virtual void RequestQuit(int32 QueueIndex) = 0;

	/** 
	 *	Queue a task, assuming that this thread is not the same as the current thread.
	 *	@param QueueIndex, Queue to enqueue into
	 *	@param Task; Task to queue.
	 **/
	virtual bool EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task)
	{
		check(0);
		return false;
	}

	virtual void WakeUp()
	{
		check(0);
	}
	/** 
	 *Return true if this thread is processing tasks. This is only a "guess" if you ask for a thread other than yourself because that can change before the function returns.
	 *@param QueueIndex, Queue to request quit from
	 **/
	virtual bool IsProcessingTasks(int32 QueueIndex) = 0;

	// SingleThreaded API

	/** Tick single-threaded. */
	virtual void Tick() override
	{
		ProcessTasksUntilIdle(0);
	}


	// FRunnable API

	/**
	 * Allows per runnable object initialization. NOTE: This is called in the
	 * context of the thread object that aggregates this, not the thread that
	 * passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual bool Init() override
	{
		InitializeForCurrentThread();
		return true;
	}

	/**
	 * This is where all per object thread work is done. This is only called
	 * if the initialization was successful.
	 *
	 * @return The exit code of the runnable object
	 */
	virtual uint32 Run() override
	{
		ProcessTasksUntilQuit(0);
		return 0;
	}

	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop() override
	{
		RequestQuit(-1);
	}

	/**
	 * Called in the context of the aggregating thread to perform any cleanup.
	 */
	virtual void Exit() override
	{
	}

	/**
	 * Return single threaded interface when multithreading is disabled.
	 */
	virtual FSingleThreadRunnable* GetSingleThreadInterface() override
	{
		return this;
	}

protected:

	/** Id / Index of this thread. **/
	ENamedThreads::Type									ThreadId;
	/** TLS SLot that we store the FTaskThread* this pointer in. **/
	uint32												PerThreadIDTLSSlot;
	/** Used to signal stalling. Not safe for synchronization in most cases. **/
	FThreadSafeCounter									IsStalled;
	/** Array of tasks for this task thread. */
	TArray<FBaseGraphTask*> NewTasks;
	/** back pointer to the owning FWorkerThread **/
	FWorkerThread* OwnerWorker;
};

/** 
 *	FNamedTaskThread
 *	A class for managing a named thread. 
 */
class FNamedTaskThread : public FTaskThreadBase
{
public:

	virtual void ProcessTasksUntilQuit(int32 QueueIndex) override
	{
		Queue(QueueIndex).QuitWhenIdle.Reset();
		verify(++Queue(QueueIndex).RecursionGuard == 1);
		//checkThreadGraph(!Queue(QueueIndex).IncomingQueue.IsClosed()); // make sure we are started up
		while (Queue(QueueIndex).QuitWhenIdle.GetValue() == 0)
		{
			ProcessTasksNamedThread(QueueIndex, true);
			// @Hack - quit now when running with only one thread.
			if (!FPlatformProcess::SupportsMultithreading())
			{
				break;
			}
		}
		//checkThreadGraph(!Queue(QueueIndex).IncomingQueue.IsClosed()); // make sure we are started up
		verify(!--Queue(QueueIndex).RecursionGuard);
	}

	virtual void ProcessTasksUntilIdle(int32 QueueIndex) override
	{
		verify(++Queue(QueueIndex).RecursionGuard == 1);
		//checkThreadGraph(!Queue(QueueIndex).IncomingQueue.IsClosed()); // make sure we are started up
		ProcessTasksNamedThread(QueueIndex, false);
		//checkThreadGraph(!Queue(QueueIndex).IncomingQueue.IsClosed()); // make sure we are started up
		verify(!--Queue(QueueIndex).RecursionGuard);
	}
#if USE_NEW_LOCK_FREE_LISTS
	FORCEINLINE FBaseGraphTask* GetNextTask(int32 QueueIndex)
	{
		FBaseGraphTask* Task = NULL;
		Task = Queue(QueueIndex).PrivateQueueHiPri.Dequeue();
		if (!Task)
		{
			if (Queue(QueueIndex).OutstandingHiPriTasks.GetValue())
			{
				while (true)
				{
					FBaseGraphTask* NewTask = Queue(QueueIndex).IncomingQueue.Pop();
					if (!NewTask)
					{
						break;
					}
					if (ENamedThreads::GetPriority(NewTask->ThreadToExecuteOn))
					{
						Queue(QueueIndex).OutstandingHiPriTasks.Decrement();
						Task = NewTask;
						break;
					}
					else
					{
						Queue(QueueIndex).PrivateQueue.Enqueue(NewTask);
					}
				}
			}
			if (!Task)
			{
				Task = Queue(QueueIndex).PrivateQueue.Dequeue();
			}
		}
		if (!Task)
		{
			Task = Queue(QueueIndex).IncomingQueue.Pop();
		}
		return Task;
	}
	void ProcessTasksNamedThread(int32 QueueIndex, bool bAllowStall)
	{
		TStatId StallStatId;
		bool bCountAsStall = false;
#if STATS
		TStatId StatName;
		FCycleCounter ProcessingTasks;
		if (ThreadId == ENamedThreads::GameThread)
		{
			StatName = GET_STATID(STAT_TaskGraph_GameTasks);
			StallStatId = GET_STATID(STAT_TaskGraph_GameStalls);
			bCountAsStall = true;
		}
		else if (ThreadId == ENamedThreads::RenderThread)
		{
			if (QueueIndex > 0)
			{
				StallStatId = GET_STATID(STAT_TaskGraph_RenderStalls);
				bCountAsStall = true;
			}
			// else StatName = none, we need to let the scope empty so that the render thread submits tasks in a timely manner. 
		}
		else if (ThreadId != ENamedThreads::StatsThread)
		{
			StatName = GET_STATID(STAT_TaskGraph_OtherTasks);
			StallStatId = GET_STATID(STAT_TaskGraph_OtherStalls);
			bCountAsStall = true;
		}
		bool bTasksOpen = false;
		if (FThreadStats::IsCollectingData(StatName))
		{
			bTasksOpen = true;
			ProcessingTasks.Start(StatName);
		}
#endif
		while (1)
		{
			FBaseGraphTask* Task = GetNextTask(QueueIndex);
			if (!Task)
			{
#if STATS
				if(bTasksOpen)
				{
					ProcessingTasks.Stop();
					bTasksOpen = false;
				}
#endif
				if (bAllowStall)
				{
					bool bNewTaskReady = Stall(QueueIndex, StallStatId, bCountAsStall);
					if (!bNewTaskReady)
					{
						break; // we didn't really stall, rather we were asked to quit
					}
#if STATS
					if (!bTasksOpen && FThreadStats::IsCollectingData(StatName))
					{
						bTasksOpen = true;
						ProcessingTasks.Start(StatName);
					}
#endif
					continue;
				}
				else
				{
					break; // we were asked to quit
				}
			}
			else
			{
				TestRandomizedThreads();
				Task->Execute(NewTasks, ENamedThreads::Type(ThreadId | (QueueIndex << ENamedThreads::QueueIndexShift)));
				TestRandomizedThreads();
			}
		}
	}
#else
	void ProcessTasksNamedThread(int32 QueueIndex, bool bAllowStall)
	{
		//checkThreadGraph(!Queue(QueueIndex).IncomingQueue.IsClosed()); // make sure we are started up
		TStatId StallStatId;
		bool bCountAsStall = false;
#if STATS
		TStatId StatName;
		FCycleCounter ProcessingTasks;
		if (ThreadId == ENamedThreads::GameThread)
		{
			StatName = GET_STATID(STAT_TaskGraph_GameTasks);
			StallStatId = GET_STATID(STAT_TaskGraph_GameStalls);
			bCountAsStall = true;
		}
		else if (ThreadId == ENamedThreads::RenderThread)
		{
			if (QueueIndex > 0)
			{
				StallStatId = GET_STATID(STAT_TaskGraph_RenderStalls);
				bCountAsStall = true;
			}
			// else StatName = none, we need to let the scope empty so that the render thread submits tasks in a timely manner. 
		}
		else if (ThreadId != ENamedThreads::StatsThread)
		{
			StatName = GET_STATID(STAT_TaskGraph_OtherTasks);
			StallStatId = GET_STATID(STAT_TaskGraph_OtherStalls);
			bCountAsStall = true;
		}
		bool bTasksOpen = false;
#endif
		while (1)
		{
			FBaseGraphTask* Task = NULL;
			Task = Queue(QueueIndex).PrivateQueueHiPri.Dequeue();
			if (!Task)
			{
				if (Queue(QueueIndex).OutstandingHiPriTasks.GetValue())
				{
					Queue(QueueIndex).IncomingQueue.PopAll(NewTasks);
					if (NewTasks.Num())
					{
						for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
						{
							FBaseGraphTask* NewTask = NewTasks[Index];
							if (ENamedThreads::GetPriority(NewTask->ThreadToExecuteOn))
							{
								Queue(QueueIndex).PrivateQueueHiPri.Enqueue(NewTask);
								Queue(QueueIndex).OutstandingHiPriTasks.Decrement();
							}
							else
							{
								Queue(QueueIndex).PrivateQueue.Enqueue(NewTask);
							}
						}
						NewTasks.Reset();
						Task = Queue(QueueIndex).PrivateQueueHiPri.Dequeue();
					}
				}
				if (!Task)
				{
					Task = Queue(QueueIndex).PrivateQueue.Dequeue();
				}
			}
			if (!Task)
			{
				Queue(QueueIndex).IncomingQueue.PopAll(NewTasks);
				if (!NewTasks.Num())
				{
					if (bAllowStall)
					{
#if STATS
						if(bTasksOpen)
						{
							ProcessingTasks.Stop();
							bTasksOpen = false;
						}
#endif
						if (Stall(QueueIndex, StallStatId, bCountAsStall))
						{
							Queue(QueueIndex).IncomingQueue.PopAll(NewTasks);
						}
					}
				}
				if (NewTasks.Num())
				{
					for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
					{
						FBaseGraphTask* NewTask = NewTasks[Index];
						checkThreadGraph(NewTask);
						if (ENamedThreads::GetPriority(NewTask->ThreadToExecuteOn))
						{
							Queue(QueueIndex).PrivateQueueHiPri.Enqueue(NewTask);
							Queue(QueueIndex).OutstandingHiPriTasks.Decrement();
						}
						else
						{
							Queue(QueueIndex).PrivateQueue.Enqueue(NewTask);
						}
					}
					NewTasks.Reset();
					Task = Queue(QueueIndex).PrivateQueueHiPri.Dequeue();
					if (!Task)
					{
						Task = Queue(QueueIndex).PrivateQueue.Dequeue();
					}
				}
			}
			if (Task)
			{
				TestRandomizedThreads();
#if STATS
				if (!bTasksOpen && FThreadStats::IsCollectingData(StatName))
				{
					bTasksOpen = true;
					ProcessingTasks.Start(StatName);
				}
#endif
				Task->Execute(NewTasks, ENamedThreads::Type(ThreadId | (QueueIndex << ENamedThreads::QueueIndexShift)));

				TestRandomizedThreads();
			}
			else
			{
				break;
			}
		}
#if STATS
		if(bTasksOpen)
		{
			ProcessingTasks.Stop();
			bTasksOpen = false;
		}
#endif
	}
#endif
	virtual void EnqueueFromThisThread(int32 QueueIndex, FBaseGraphTask* Task) override
	{
		checkThreadGraph(Task && Queue(QueueIndex).StallRestartEvent); // make sure we are started up
		if (ENamedThreads::GetPriority(Task->ThreadToExecuteOn))
		{
			Queue(QueueIndex).PrivateQueueHiPri.Enqueue(Task);
		}
		else
		{
			Queue(QueueIndex).PrivateQueue.Enqueue(Task);
		}
	}

	virtual void RequestQuit(int32 QueueIndex) override
	{
		// this will not work under arbitrary circumstances. For example you should not attempt to stop threads unless they are known to be idle.
		if (!Queue(0).StallRestartEvent)
		{
			return;
		}
#if USE_NEW_LOCK_FREE_LISTS
		if (QueueIndex == -1)
		{
			// we are shutting down
			checkThreadGraph(Queue(0).StallRestartEvent); // make sure we are started up
			checkThreadGraph(Queue(1).StallRestartEvent); // make sure we are started up
			Queue(0).QuitWhenIdle.Increment();
			Queue(1).QuitWhenIdle.Increment();
			Queue(0).StallRestartEvent->Trigger(); 
			Queue(1).StallRestartEvent->Trigger(); 
		}
		else
		{
			checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
			Queue(QueueIndex).QuitWhenIdle.Increment();
		}
#else
		if (QueueIndex == -1)
		{
			QueueIndex = 0;
		}
		checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
		Queue(QueueIndex).QuitWhenIdle.Increment();
		Queue(QueueIndex).StallRestartEvent->Trigger(); 
#endif
	}

	virtual bool EnqueueFromOtherThread(int32 QueueIndex, FBaseGraphTask* Task) override
	{
		TestRandomizedThreads();
		checkThreadGraph(Task && Queue(QueueIndex).StallRestartEvent); // make sure we are started up

		bool bWasReopenedByMe;
		bool bHiPri = !!ENamedThreads::GetPriority(Task->ThreadToExecuteOn);
		{
			TASKGRAPH_SCOPE_CYCLE_COUNTER(0, STAT_TaskGraph_EnqueueFromOtherThread_ReopenIfClosedAndPush);
			bWasReopenedByMe = Queue(QueueIndex).IncomingQueue.ReopenIfClosedAndPush(Task);
		}
		if (bHiPri)
		{
			Queue(QueueIndex).OutstandingHiPriTasks.Increment();
		}
		if (bWasReopenedByMe)
		{
			TASKGRAPH_SCOPE_CYCLE_COUNTER(1, STAT_TaskGraph_EnqueueFromOtherThread_Trigger);
			Queue(QueueIndex).StallRestartEvent->Trigger();
		}
		return bWasReopenedByMe;
	}

	virtual bool IsProcessingTasks(int32 QueueIndex) override
	{
		return !!Queue(QueueIndex).RecursionGuard;
	}

private:

	/** Grouping of the data for an individual queue. **/
	struct FThreadTaskQueue
	{
		/** A non-threas safe queue for the thread locked tasks of a named thread **/
		FTaskQueue											PrivateQueue;
		/** A non-threas safe queue for the thread locked tasks of a named thread. These are high priority.**/
		FTaskQueue											PrivateQueueHiPri;
		/** Used to signal pending hi pri tasks. **/
		FThreadSafeCounter									OutstandingHiPriTasks;
		/** 
		 *	For named threads, this is a queue of thread locked tasks coming from other threads. They are not stealable.
		 *	For unnamed thread this is the public queue, subject to stealing.
		 *	In either case this queue is closely related to the stall event. Other threads that reopen the incoming queue must trigger the stall event to allow the thread to run.
		**/
#if USE_NEW_LOCK_FREE_LISTS && USE_INTRUSIVE_TASKQUEUES
		FCloseableLockFreePointerQueueBaseSingleBaseConsumerIntrusive<FBaseGraphTask>		IncomingQueue;
#elif USE_NEW_LOCK_FREE_LISTS
		TReopenableLockFreePointerListFIFOSingleConsumer<FBaseGraphTask>		IncomingQueue;
#else
		TReopenableLockFreePointerListLIFO<FBaseGraphTask>		IncomingQueue;
#endif
		/** Used to signal the thread to quit when idle. **/
		FThreadSafeCounter									QuitWhenIdle;
		/** We need to disallow reentry of the processing loop **/
		uint32												RecursionGuard;
		/** Event that this thread blocks on when it runs out of work. **/
		FEvent*												StallRestartEvent;

		FThreadTaskQueue()
			: RecursionGuard(0)
			, StallRestartEvent(FPlatformProcess::GetSynchEventFromPool(true))
		{

		}
		~FThreadTaskQueue()
		{
			FPlatformProcess::ReturnSynchEventToPool( StallRestartEvent );
			StallRestartEvent = nullptr;
		}
	};

	/**
	 *	Internal function to block on the stall wait event.
	 *  @param QueueIndex		- Queue to stall
	 *  @param StallStatId		- Stall stat id
	 *  @param bCountAsStall	- true if StallStatId is a valid stat id
	 *	@return true if the thread actually stalled; false in the case of a stop request or a task arrived while we were trying to stall.
	 */
	bool Stall(int32 QueueIndex, TStatId StallStatId, bool bCountAsStall)
	{
		TestRandomizedThreads();

		checkThreadGraph(Queue(QueueIndex).StallRestartEvent); // make sure we are started up
		if (Queue(QueueIndex).QuitWhenIdle.GetValue() == 0)
		{
			// @Hack - Only stall when multithreading is enabled.
			if (FPlatformProcess::SupportsMultithreading())
			{
				FPlatformMisc::MemoryBarrier();
				Queue(QueueIndex).StallRestartEvent->Reset();
				TestRandomizedThreads();
				if (Queue(QueueIndex).IncomingQueue.CloseIfEmpty())
				{
					FScopeCycleCounter Scope( StallStatId );

					TestRandomizedThreads();
					Queue(QueueIndex).StallRestartEvent->Wait(MAX_uint32, bCountAsStall);
					//checkThreadGraph(!Queue(QueueIndex).IncomingQueue.IsClosed()); // make sure we are started up
					return true;
				}
			}
			else 
			{
				return true;
			}
		}
		return false;
	}

	FORCEINLINE FThreadTaskQueue& Queue(int32 QueueIndex)
	{
		checkThreadGraph(QueueIndex >= 0 && QueueIndex < ENamedThreads::NumQueues);
		return Queues[QueueIndex];
	}
	FORCEINLINE const FThreadTaskQueue& Queue(int32 QueueIndex) const
	{
		checkThreadGraph(QueueIndex >= 0 && QueueIndex < ENamedThreads::NumQueues);
		return Queues[QueueIndex];
	}

	/** Array of queues, only the first one is used for unnamed threads. **/
	FThreadTaskQueue Queues[ENamedThreads::NumQueues];
};

/** 
 *	FTaskThreadAnyThread
 *	A class for managing a worker threads. 
**/
class FTaskThreadAnyThread : public FTaskThreadBase
{
public:
	/** Used for named threads to start processing tasks until the thread is idle and RequestQuit has been called. **/
	virtual void ProcessTasksUntilQuit(int32 QueueIndex) override
	{
		check(!QueueIndex);
		Queue.QuitWhenIdle.Reset();
		while (Queue.QuitWhenIdle.GetValue() == 0)
		{
			ProcessTasks();
			// @Hack - quit now when running with only one thread.
			if (!FPlatformProcess::SupportsMultithreading())
			{
				break;
			}
		}
	}

	virtual void ProcessTasksUntilIdle(int32 QueueIndex) override
	{
		if (!FPlatformProcess::SupportsMultithreading()) 
		{
			Queue.QuitWhenIdle.Set(1);
			ProcessTasks();
			Queue.QuitWhenIdle.Reset();
		}
		else
		{
			FTaskThreadBase::ProcessTasksUntilIdle(QueueIndex);
		}
	}
	// Calls meant to be called from any thread.

	/** 
	 *	Will cause the thread to return to the caller when it becomes idle. Used to return from ProcessTasksUntilQuit for named threads or to shut down unnamed threads. 
	 *	CAUTION: This will not work under arbitrary circumstances. For example you should not attempt to stop unnamed threads unless they are known to be idle.
	 *	Return requests for named threads should be submitted from that named thread as FReturnGraphTask does.
	 *	@param QueueIndex, Queue to request quit from
	**/
	virtual void RequestQuit(int32 QueueIndex) override
	{
		check(QueueIndex < 1);

		// this will not work under arbitrary circumstances. For example you should not attempt to stop threads unless they are known to be idle.
		checkThreadGraph(Queue.StallRestartEvent); // make sure we are started up
		Queue.QuitWhenIdle.Increment();
		Queue.StallRestartEvent->Trigger(); 
	}

	virtual void WakeUp() final override
	{
		TASKGRAPH_SCOPE_CYCLE_COUNTER(1, STAT_TaskGraph_Wakeup_Trigger);
		Queue.StallRestartEvent->Trigger();
	}

	/** 
	 *Return true if this thread is processing tasks. This is only a "guess" if you ask for a thread other than yourself because that can change before the function returns.
	 *@param QueueIndex, Queue to request quit from
	 **/
	virtual bool IsProcessingTasks(int32 QueueIndex) override
	{
		check(!QueueIndex);
		return !!Queue.RecursionGuard;
	}

private:

	/** 
	 *	Process tasks until idle. May block if bAllowStall is true
	 *	@param QueueIndex, Queue to process tasks from
	 *	@param bAllowStall,  if true, the thread will block on the stall event when it runs out of tasks.
	 **/
	void ProcessTasks()
	{
		TStatId StallStatId;
		bool bCountAsStall = true;
#if STATS
		TStatId StatName;
		FCycleCounter ProcessingTasks;
		StatName = GET_STATID(STAT_TaskGraph_OtherTasks);
		StallStatId = GET_STATID(STAT_TaskGraph_OtherStalls);
		bool bTasksOpen = false;
		if (FThreadStats::IsCollectingData(StatName))
		{
			bTasksOpen = true;
			ProcessingTasks.Start(StatName);
		}
#endif
		verify(++Queue.RecursionGuard == 1);
		while (1)
		{
			FBaseGraphTask* Task = FindWork();
			if (!Task)
			{
#if STATS
				if(bTasksOpen)
				{
					ProcessingTasks.Stop();
					bTasksOpen = false;
				}
#endif
				Stall(StallStatId, bCountAsStall);
				if (Queue.QuitWhenIdle.GetValue())
				{
					break;
				}
#if STATS
				if (FThreadStats::IsCollectingData(StatName))
				{
					bTasksOpen = true;
					ProcessingTasks.Start(StatName);
				}
#endif
				continue;
			}
			TestRandomizedThreads();
			Task->Execute(NewTasks, ENamedThreads::Type(ThreadId));
			TestRandomizedThreads();
		}
		verify(!--Queue.RecursionGuard);
	}

	/** Grouping of the data for an individual queue. **/
	struct FThreadTaskQueue
	{
		/** Used to signal the thread to quit when idle. **/
		FThreadSafeCounter									QuitWhenIdle;
		/** We need to disallow reentry of the processing loop **/
		uint32												RecursionGuard;
		/** Event that this thread blocks on when it runs out of work. **/
		FEvent*												StallRestartEvent;

		FThreadTaskQueue()
			: RecursionGuard(0)
			, StallRestartEvent(FPlatformProcess::GetSynchEventFromPool(true))
		{

		}
		~FThreadTaskQueue()
		{
			FPlatformProcess::ReturnSynchEventToPool( StallRestartEvent );
			StallRestartEvent = nullptr;
		}
	};

	/**
	 *	Internal function to block on the stall wait event.
	 *  @param QueueIndex		- Queue to stall
	 *  @param StallStatId		- Stall stat id
	 *  @param bCountAsStall	- true if StallStatId is a valid stat id
	 */
	void Stall(TStatId StallStatId, bool bCountAsStall)
	{
		TestRandomizedThreads();

		checkThreadGraph(Queue.StallRestartEvent); // make sure we are started up
		if (Queue.QuitWhenIdle.GetValue() == 0)
		{
			// @Hack - Only stall when multithreading is enabled.
			if (FPlatformProcess::SupportsMultithreading())
			{
				FPlatformMisc::MemoryBarrier();

				FScopeCycleCounter Scope( StallStatId );

				NotifyStalling();
				TestRandomizedThreads();
				Queue.StallRestartEvent->Wait(MAX_uint32, bCountAsStall);
				TestRandomizedThreads();
				Queue.StallRestartEvent->Reset();
			}
		}
	}

	/**
	 *	Internal function to call the system looking for work. Called from this thread.
	 *	@return New task to process.
	 */
	FBaseGraphTask* FindWork();

	/**
	 *	Internal function to notify the that system that I am stalling. This is a hint to give me a job asap.
	 */
	void NotifyStalling();

	/** Array of queues, only the first one is used for unnamed threads. **/
	FThreadTaskQueue Queue;
};

/** 
 *	FTaskGraphImplementation
 *	Implementation of the centralized part of the task graph system.
 *	These parts of the system have no knowledge of the dependency graph, they exclusively work on tasks.
**/
#define FAtomicStateBitfield_MAX_THREADS (13)

/** 
	*	FWorkerThread
	*	Helper structure to aggregate a few items related to the individual threads.
**/
struct FWorkerThread
{
	/** The actual FTaskThread that manager this task **/
	FTaskThreadBase*	TaskGraphWorker;
	/** For internal threads, the is non-NULL and holds the information about the runable thread that was created. **/
	FRunnableThread*	RunnableThread;
	/** For external threads, this determines if they have been "attached" yet. Attachment is mostly setting up TLS for this individual thread. **/
	bool				bAttached;

	/** Constructor to set reasonable defaults. **/
	FWorkerThread()
		: TaskGraphWorker(nullptr)
		, RunnableThread(nullptr)
		, bAttached(false)
	{
	}
};

class FTaskGraphImplementation : public FTaskGraphInterface  
{
public:

	// API related to life cycle of the system and singletons

	/** 
	 *	Singleton returning the one and only FTaskGraphImplementation.
	 *	Note that unlike most singletons, a manual call to FTaskGraphInterface::Startup is required before the singleton will return a valid reference.
	**/
	static FTaskGraphImplementation& Get()
	{		
		checkThreadGraph(TaskGraphImplementationSingleton);
		return *TaskGraphImplementationSingleton;
	}

	/** 
	 *	Constructor - initializes the data structures, sets the singleton pointer and creates the internal threads.
	 *	@param InNumThreads; total number of threads in the system, including named threads, unnamed threads, internal threads and external threads. Must be at least 1 + the number of named threads.
	**/
	FTaskGraphImplementation(int32 InNumThreads)
	{
		// if we don't want any performance-based threads, then force the task graph to not create any worker threads, and run in game thread
		if (!FPlatformProcess::SupportsMultithreading())
		{
			// this is the logic that used to be spread over a couple of places, that will make the rest of this function disable a worker thread
			// @todo: it could probably be made simpler/clearer
			InNumThreads = 1;
			// this - 1 tells the below code there is no rendering thread
			LastExternalThread = (ENamedThreads::Type)(ENamedThreads::ActualRenderingThread - 1);
		}
		else
		{
			LastExternalThread = ENamedThreads::ActualRenderingThread;
		}
		
		NumNamedThreads = LastExternalThread + 1;
		NumThreads = FMath::Max<int32>(FMath::Min<int32>(InNumThreads + NumNamedThreads,MAX_THREADS),NumNamedThreads + 1);
		// Cap number of extra threads to the platform worker thread count
		NumThreads = FMath::Min(NumThreads, NumNamedThreads + FPlatformMisc::NumberOfWorkerThreadsToSpawn());
		UE_LOG(LogTaskGraph, Log, TEXT("Started task graph with %d named threads and %d total threads."), NumNamedThreads, NumThreads);
		check(NumThreads - NumNamedThreads >= 1);  // need at least one pure worker thread
		check(NumThreads <= MAX_THREADS);
		check(!NextStealFromThread.GetValue()); // reentrant?
		NextStealFromThread.Increment(); // just checking for reentrancy
		PerThreadIDTLSSlot = FPlatformTLS::AllocTlsSlot();

		NextUnnamedThreadMod = NumThreads - NumNamedThreads;

		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			check(!WorkerThreads[ThreadIndex].bAttached); // reentrant?
			bool bAnyTaskThread = ThreadIndex >= NumNamedThreads;
			if (bAnyTaskThread)
			{
				WorkerThreads[ThreadIndex].TaskGraphWorker = new FTaskThreadAnyThread;
			}
			else
			{
				WorkerThreads[ThreadIndex].TaskGraphWorker = new FNamedTaskThread;
			}
			WorkerThreads[ThreadIndex].TaskGraphWorker->Setup(ENamedThreads::Type(ThreadIndex), PerThreadIDTLSSlot, &WorkerThreads[ThreadIndex]);
		}

		TaskGraphImplementationSingleton = this; // now reentrancy is ok

		for (int32 ThreadIndex = LastExternalThread + 1; ThreadIndex < NumThreads; ThreadIndex++)
		{
			FString Name = FString::Printf(TEXT("TaskGraphThread %d"), ThreadIndex - (LastExternalThread + 1));
			uint32 StackSize = 256 * 1024;
			WorkerThreads[ThreadIndex].RunnableThread = FRunnableThread::Create(&Thread(ThreadIndex), *Name, StackSize, TPri_BelowNormal, FPlatformAffinity::GetTaskGraphThreadMask()); // these are below normal threads? so that they sleep when the named threads are active
			WorkerThreads[ThreadIndex].bAttached = true;
		}
	}

	/** 
	 *	Destructor - probably only works reliably when the system is completely idle. The system has no idea if it is idle or not.
	**/
	virtual ~FTaskGraphImplementation()
	{
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			Thread(ThreadIndex).RequestQuit(-1);
		}
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			if (ThreadIndex > LastExternalThread)
			{
				WorkerThreads[ThreadIndex].RunnableThread->WaitForCompletion();
				delete WorkerThreads[ThreadIndex].RunnableThread;
				WorkerThreads[ThreadIndex].RunnableThread = NULL;
			}
			WorkerThreads[ThreadIndex].bAttached = false;
		}
		TaskGraphImplementationSingleton = NULL;
		NextUnnamedThreadMod = 0;
		TArray<FTaskThreadBase*> NotProperlyUnstalled;
		StalledUnnamedThreads.PopAll(NotProperlyUnstalled);
		FPlatformTLS::FreeTlsSlot(PerThreadIDTLSSlot);
	}

	// API inherited from FTaskGraphInterface

	/** 
	 *	Function to queue a task, called from a FBaseGraphTask
	 *	@param	Task; the task to queue
	 *	@param	ThreadToExecuteOn; Either a named thread for a threadlocked task or ENamedThreads::AnyThread for a task that is to run on a worker thread
	 *	@param	CurrentThreadIfKnown; This should be the current thread if it is known, or otherwise use ENamedThreads::AnyThread and the current thread will be determined.
	**/
	virtual void QueueTask(FBaseGraphTask* Task, ENamedThreads::Type ThreadToExecuteOn, ENamedThreads::Type InCurrentThreadIfKnown = ENamedThreads::AnyThread) final override
	{
		TASKGRAPH_SCOPE_CYCLE_COUNTER(2, STAT_TaskGraph_QueueTask);

		TestRandomizedThreads();
		checkThreadGraph(NextUnnamedThreadMod);
		if (GFastSchedulerLatched != GFastScheduler && IsInGameThread())
		{
#if USE_NEW_LOCK_FREE_LISTS
			GFastScheduler = !!FApp::ShouldUseThreadingForPerformance();
#endif
			if (GFastSchedulerLatched != GFastScheduler)
			{
				if (GFastScheduler)
				{
					AtomicForConsoleApproach = FAtomicStateBitfield();
					AtomicForConsoleApproach.Stalled = (1 << GetNumWorkerThreads()) - 1; // everyone is stalled
				}
				// this is all kinda of sketchy, but runtime switching just has to barely work.
				FPlatformProcess::Sleep(.1f); // hopefully all task threads are idle
				GFastSchedulerLatched = GFastScheduler;

				// wake up all threads to prevent deadlock on the transition
				if (GFastSchedulerLatched)
				{
					GConsoleSpinModeLatched = 0; // nobody is started now, this will re latch and start someone if we have that mode on
				}
				else
				{
					for (int32 Index = 0; Index < GetNumWorkerThreads(); Index++)
					{
						StartTaskThread(Index);
					}
				}
			}
		}
		if (GFastSchedulerLatched && GConsoleSpinModeLatched != GConsoleSpinMode && IsInGameThread())
		{
			bool bStartTask = GConsoleSpinMode && ((!!GConsoleSpinModeLatched) != !!GConsoleSpinMode);
			GConsoleSpinModeLatched = GConsoleSpinMode;
			if (bStartTask)
			{
				StartTaskThreadFastMode();
			}
		}

		if (ENamedThreads::GetThreadIndex(ThreadToExecuteOn) == ENamedThreads::AnyThread)
		{
			TASKGRAPH_SCOPE_CYCLE_COUNTER(3, STAT_TaskGraph_QueueTask_AnyThread);
			if (FPlatformProcess::SupportsMultithreading())
			{
				{
					TASKGRAPH_SCOPE_CYCLE_COUNTER(4, STAT_TaskGraph_QueueTask_IncomingAnyThreadTasks_Push);
					if (ENamedThreads::GetPriority(Task->ThreadToExecuteOn))
					{
						IncomingAnyThreadTasksHiPri.Push(Task);
					}
					else
					{
						IncomingAnyThreadTasks.Push(Task);
					}
				}
				if (GFastSchedulerLatched)
				{
					if (!GConsoleSpinModeLatched)
					{
						StartTaskThreadFastMode();
					}
				}
				else
				{
					ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread;
					if (ENamedThreads::GetThreadIndex(InCurrentThreadIfKnown) == ENamedThreads::AnyThread)
					{
						CurrentThreadIfKnown = GetCurrentThread();
					}
					else
					{
						CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(InCurrentThreadIfKnown);
						checkThreadGraph(CurrentThreadIfKnown == GetCurrentThread());
					}
					FTaskThreadBase* TempTarget;
					{
						TASKGRAPH_SCOPE_CYCLE_COUNTER(5, STAT_TaskGraph_QueueTask_StalledUnnamedThreads_Pop);
						TempTarget = StalledUnnamedThreads.Pop(); //@todo it is possible that a thread is in the process of stalling and we just missed it, non-fatal, but we could lose a whole task of potential parallelism.
						if (TempTarget && GNumWorkerThreadsToIgnore && (TempTarget->GetThreadId() - NumNamedThreads) >= GetNumWorkerThreads())
						{
							TempTarget = nullptr;
						}
					}
					if (TempTarget)
					{
						ThreadToExecuteOn = TempTarget->GetThreadId();
					}
					else
					{
						check(NextUnnamedThreadMod - GNumWorkerThreadsToIgnore > 0); // can't tune it to zero task threads
						ThreadToExecuteOn = ENamedThreads::Type((uint32(NextUnnamedThreadForTaskFromUnknownThread.Increment()) % uint32(NextUnnamedThreadMod - GNumWorkerThreadsToIgnore)) + NumNamedThreads);
					}
					FTaskThreadBase* Target = &Thread(ThreadToExecuteOn);
					if (ThreadToExecuteOn != CurrentThreadIfKnown)
					{
						Target->WakeUp();
					}
				}
				return;
			}
			else
			{
				ThreadToExecuteOn = ENamedThreads::GameThread;
			}
		}
		ENamedThreads::Type CurrentThreadIfKnown;
		if (ENamedThreads::GetThreadIndex(InCurrentThreadIfKnown) == ENamedThreads::AnyThread)
		{
			CurrentThreadIfKnown = GetCurrentThread();
		}
		else
		{
			CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(InCurrentThreadIfKnown);
			checkThreadGraph(CurrentThreadIfKnown == GetCurrentThread());
		}
		{
			int32 QueueToExecuteOn = ENamedThreads::GetQueueIndex(ThreadToExecuteOn);
			ThreadToExecuteOn = ENamedThreads::GetThreadIndex(ThreadToExecuteOn);
			FTaskThreadBase* Target = &Thread(ThreadToExecuteOn);
			if (ThreadToExecuteOn == CurrentThreadIfKnown)
			{
				Target->EnqueueFromThisThread(QueueToExecuteOn, Task);
			}
			else
			{
				Target->EnqueueFromOtherThread(QueueToExecuteOn, Task);
			}
		}
	}


	virtual	int32 GetNumWorkerThreads() final override
	{
		int32 Result = NumThreads - NumNamedThreads - GNumWorkerThreadsToIgnore;
		check(Result > 0); // can't tune it to zero task threads
		return Result;
	}

	virtual ENamedThreads::Type GetCurrentThreadIfKnown(bool bLocalQueue) final override
	{
		ENamedThreads::Type Result = GetCurrentThread();
		if (bLocalQueue && Result >= 0 && Result < NumNamedThreads)
		{
			Result = ENamedThreads::Type(int32(Result) | int32(ENamedThreads::LocalQueue));
		}
		return Result;
	}

	virtual bool IsThreadProcessingTasks(ENamedThreads::Type ThreadToCheck) final override
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(ThreadToCheck);
		ThreadToCheck = ENamedThreads::GetThreadIndex(ThreadToCheck);
		check(ThreadToCheck >= 0 && ThreadToCheck < NumNamedThreads);
		return Thread(ThreadToCheck).IsProcessingTasks(QueueIndex);
	}

	// External Thread API

	virtual void AttachToThread(ENamedThreads::Type CurrentThread) final override
	{
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(NextUnnamedThreadMod);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(!WorkerThreads[CurrentThread].bAttached);
		Thread(CurrentThread).InitializeForCurrentThread();
	}

	virtual void ProcessThreadUntilIdle(ENamedThreads::Type CurrentThread) final override
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(CurrentThread == GetCurrentThread());
		Thread(CurrentThread).ProcessTasksUntilIdle(QueueIndex);
	}

	virtual void ProcessThreadUntilRequestReturn(ENamedThreads::Type CurrentThread) final override
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread >= 0 && CurrentThread < NumNamedThreads);
		check(CurrentThread == GetCurrentThread());
		Thread(CurrentThread).ProcessTasksUntilQuit(QueueIndex);
	}

	virtual void RequestReturn(ENamedThreads::Type CurrentThread) final override
	{
		int32 QueueIndex = ENamedThreads::GetQueueIndex(CurrentThread);
		CurrentThread = ENamedThreads::GetThreadIndex(CurrentThread);
		check(CurrentThread != ENamedThreads::AnyThread);
		Thread(CurrentThread).RequestQuit(QueueIndex);
	}

	virtual void WaitUntilTasksComplete(const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) final override
	{
		ENamedThreads::Type CurrentThread = CurrentThreadIfKnown;
		if (ENamedThreads::GetThreadIndex(CurrentThreadIfKnown) == ENamedThreads::AnyThread)
		{
			bool bIsHiPri = !!ENamedThreads::GetPriority(CurrentThreadIfKnown);
			check(!ENamedThreads::GetQueueIndex(CurrentThreadIfKnown));
			CurrentThreadIfKnown = GetCurrentThread();
			CurrentThread = bIsHiPri ? ENamedThreads::HiPri(CurrentThreadIfKnown) : CurrentThreadIfKnown;
		}
		else
		{
			CurrentThreadIfKnown = ENamedThreads::GetThreadIndex(CurrentThreadIfKnown);
			check(CurrentThreadIfKnown == GetCurrentThread());
			// we don't modify CurrentThread here because it might be a local queue
		}

		if (CurrentThreadIfKnown != ENamedThreads::AnyThread && CurrentThreadIfKnown < NumNamedThreads && !IsThreadProcessingTasks(CurrentThread))
		{
			bool bAnyPending = false;
			for (int32 Index = 0; Index < Tasks.Num(); Index++)
			{
				if (!Tasks[Index]->IsComplete())
				{
					bAnyPending = true;
					break;
				}
			}
			if (!bAnyPending)
			{
				return;
			}
			// named thread process tasks while we wait
			TGraphTask<FReturnGraphTask>::CreateTask(&Tasks, CurrentThread).ConstructAndDispatchWhenReady(CurrentThread);
			ProcessThreadUntilRequestReturn(CurrentThread);
		}
		else
		{
			// We will just stall this thread on an event while we wait
			FScopedEvent Event;
			TriggerEventWhenTasksComplete(Event.Get(), Tasks, CurrentThreadIfKnown);
		}
	}

	virtual void TriggerEventWhenTasksComplete(FEvent* InEvent, const FGraphEventArray& Tasks, ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread) final override
	{
		check(InEvent);
		bool bAnyPending = false;
		for (int32 Index = 0; Index < Tasks.Num(); Index++)
		{
			if (!Tasks[Index]->IsComplete())
			{
				bAnyPending = true;
				break;
			}
		}
		if (!bAnyPending)
		{
			TestRandomizedThreads();
			InEvent->Trigger();
			return;
		}
		TGraphTask<FTriggerEventGraphTask>::CreateTask(&Tasks, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(InEvent);
	}

	// Scheduling utilities

	void StartTaskThread(int32 IndexToStart)
	{
		ENamedThreads::Type ThreadToWake = ENamedThreads::Type(IndexToStart + NumNamedThreads);
		((FTaskThreadAnyThread&)Thread(ThreadToWake)).FTaskThreadAnyThread::WakeUp();
	}

	bool StartTaskThreadFastMode(int32 MyIndex = -1)
	{
		while (true)
		{
			TestRandomizedThreads();
			FAtomicStateBitfield LocalAtomicForConsoleApproachInner = AtomicForConsoleApproach;
			check(MyIndex < 0 || !(LocalAtomicForConsoleApproachInner.Stalled & (1 << MyIndex))); // if I was stalled, I would not be here

			if (!LocalAtomicForConsoleApproachInner.Stalled)
			{
				break;
			}

			uint32 IndexToStart =  31 - FMath::CountLeadingZeros((uint32)LocalAtomicForConsoleApproachInner.Stalled);
			check(IndexToStart != MyIndex);
			check(((uint32)LocalAtomicForConsoleApproachInner.Stalled) & (1 << IndexToStart));

			FAtomicStateBitfield NewAtomicForConsoleApproachInner = LocalAtomicForConsoleApproachInner;
			NewAtomicForConsoleApproachInner.Stalled = LocalAtomicForConsoleApproachInner.Stalled & ~(1 << IndexToStart);
			check(!(((uint32)NewAtomicForConsoleApproachInner.Stalled) & (1 << IndexToStart)));
			check(LocalAtomicForConsoleApproachInner != NewAtomicForConsoleApproachInner);
			if (FAtomicStateBitfield::InterlockedCompareExchange(&AtomicForConsoleApproach, NewAtomicForConsoleApproachInner, LocalAtomicForConsoleApproachInner) == LocalAtomicForConsoleApproachInner)
			{
				TestRandomizedThreads();
				StartTaskThread(IndexToStart);
				return true;
			}
		}
		return false;
	}

	FORCEINLINE void SetWorking(uint32 MyIndex, int32 LocalNumWorkingThread)
	{
		if (!GConsoleSpinModeLatched)
		{
			// we don't need to track the number of threads working or start threads when we take a job unless we are in spin mode
			return;
		}
		while (true)
		{
			TestRandomizedThreads();
			FPlatformMisc::MemoryBarrier();
			FAtomicStateBitfield LocalAtomicForConsoleApproachInner = AtomicForConsoleApproach;
			check(!(LocalAtomicForConsoleApproachInner.Stalled & (1 << MyIndex))); // if I was stalled, I would not be here
			bool CurrentlyWorking = !!((LocalAtomicForConsoleApproachInner.Working) & (1 << MyIndex));
			if (CurrentlyWorking)
			{
				break;
			}

			FAtomicStateBitfield NewAtomicForConsoleApproachInner = LocalAtomicForConsoleApproachInner;
			NewAtomicForConsoleApproachInner.Working = LocalAtomicForConsoleApproachInner.Working | (1 << MyIndex);
			int32 NumStalled = LocalAtomicForConsoleApproachInner.NumberOfStalledThreads();

			// we are going to do work, so if everyone else is either working or stalled, we need to start a thread
			if (NumStalled && LocalAtomicForConsoleApproachInner.NumberOfStalledThreads() + LocalAtomicForConsoleApproachInner.NumberOfWorkingThreads() + 1 == LocalNumWorkingThread)
			{
				// start one while we flip ourselves to working.
				check(LocalAtomicForConsoleApproachInner.Stalled);

				uint32 IndexToStart =  31 - FMath::CountLeadingZeros((uint32)LocalAtomicForConsoleApproachInner.Stalled);
				check(IndexToStart != MyIndex);
				check(((uint32)LocalAtomicForConsoleApproachInner.Stalled) & (1 << IndexToStart));

				NewAtomicForConsoleApproachInner.Stalled = LocalAtomicForConsoleApproachInner.Stalled & ~(1 << IndexToStart);

				check(LocalAtomicForConsoleApproachInner != NewAtomicForConsoleApproachInner);
				if (FAtomicStateBitfield::InterlockedCompareExchange(&AtomicForConsoleApproach, NewAtomicForConsoleApproachInner, LocalAtomicForConsoleApproachInner) == LocalAtomicForConsoleApproachInner)
				{
					TestRandomizedThreads();
					StartTaskThread(IndexToStart);
					// if we have another stalled thread and it looks like there are more tasks, then start an extra thread to get fan-out on the thread starts
#if USE_NEW_LOCK_FREE_LISTS
					if (GMaxTasksToStartOnDequeue > 1 && NumStalled > 1 && (!IncomingAnyThreadTasks.IsEmptyFast() || !IncomingAnyThreadTasksHiPri.IsEmptyFast()))
					{
						StartTaskThreadFastMode(MyIndex);
					}
#endif
					return;
				}
			}
			else
			{
				// we don't need to wake anyone either because there are other unstalled idle threads or everyone is busy
				check(LocalAtomicForConsoleApproachInner != NewAtomicForConsoleApproachInner);
				if (FAtomicStateBitfield::InterlockedCompareExchange(&AtomicForConsoleApproach, NewAtomicForConsoleApproachInner, LocalAtomicForConsoleApproachInner) == LocalAtomicForConsoleApproachInner)
				{
					TestRandomizedThreads();
					return;
				}
			}
		}
	}
#if USE_NEW_LOCK_FREE_LISTS
	FBaseGraphTask* FindWork(ENamedThreads::Type ThreadInNeed)
	{
		int32 LocalNumWorkingThread = GetNumWorkerThreads();
		uint32 MyIndex = uint32(ThreadInNeed) - NumNamedThreads;
		check(MyIndex >= 0 && (int32)MyIndex < LocalNumWorkingThread && 
			LocalNumWorkingThread <= FAtomicStateBitfield_MAX_THREADS); // this is an atomic on 32 bits currently; we use two fields
		while (true)
		{
			if (!GFastSchedulerLatched)
			{
				return nullptr;
			}
			bool bOkWithFalseEmpties = !!GConsoleSpinModeLatched;
			{
				FBaseGraphTask* Task = IncomingAnyThreadTasksHiPri.Pop(bOkWithFalseEmpties);
				if (!Task)
				{
					Task = IncomingAnyThreadTasks.Pop(bOkWithFalseEmpties);
				}
				if (Task)
				{
					SetWorking(MyIndex, LocalNumWorkingThread);
					return Task;
				}
			}

			TestRandomizedThreads();
			FPlatformMisc::MemoryBarrier();
			FAtomicStateBitfield LocalAtomicForConsoleApproachInner = AtomicForConsoleApproach;
			check(!(LocalAtomicForConsoleApproachInner.Stalled & (1 << MyIndex))); // if I was stalled, I would not be here


			FAtomicStateBitfield NewAtomicForConsoleApproachInner = LocalAtomicForConsoleApproachInner;
			bool CurrentlyWorking = !!((LocalAtomicForConsoleApproachInner.Working) & (1 << MyIndex));
			NewAtomicForConsoleApproachInner.Working = NewAtomicForConsoleApproachInner.Working & ~(1 << MyIndex);
			int32 NumWorking = LocalAtomicForConsoleApproachInner.NumberOfWorkingThreads() - !!CurrentlyWorking;
			bool bAttemptStall = (!GConsoleSpinModeLatched || LocalAtomicForConsoleApproachInner.NumberOfStalledThreads() + NumWorking < LocalNumWorkingThread - 1);
			if (bAttemptStall)
			{
				NewAtomicForConsoleApproachInner.Stalled = LocalAtomicForConsoleApproachInner.Stalled | (1 << MyIndex);
				check((((uint32)NewAtomicForConsoleApproachInner.Stalled) & (1 << MyIndex)));

				// if we are going to attempt to stall, we better be sure there really are not tasks...unless we are already sure
				if (bOkWithFalseEmpties)
				{
					FBaseGraphTask* Task = IncomingAnyThreadTasksHiPri.Pop();
					if (!Task)
					{
						Task = IncomingAnyThreadTasks.Pop();
					}
					if (Task)
					{
						SetWorking(MyIndex, LocalNumWorkingThread);
						return Task;
					}
				}
			}
			if (bAttemptStall || CurrentlyWorking) // otherwise we have no reason to change the state
			{
				check(LocalAtomicForConsoleApproachInner != NewAtomicForConsoleApproachInner);
				if (FAtomicStateBitfield::InterlockedCompareExchange(&AtomicForConsoleApproach, NewAtomicForConsoleApproachInner, LocalAtomicForConsoleApproachInner) == LocalAtomicForConsoleApproachInner)
				{
					if (bAttemptStall)
					{
						TestRandomizedThreads();
						return nullptr; // we did stall
					}
				}
			}
			else if (GConsoleSpinModeLatched == 2)
			{
				FPlatformProcess::SleepNoStats(0.0f);
			}
		}
	}
#else
	FBaseGraphTask* FindWorkConsole(ENamedThreads::Type ThreadInNeed)
	{
		int32 LocalNumWorkingThread = GetNumWorkerThreads();
		uint32 MyIndex = uint32(ThreadInNeed) - NumNamedThreads;
		check(MyIndex >= 0 && (int32)MyIndex < LocalNumWorkingThread && 
			LocalNumWorkingThread <= FAtomicStateBitfield_MAX_THREADS); // this is an atomic on 32 bits currently; we use two fields
		while (true)
		{
			if (!GFastSchedulerLatched)
			{
				return nullptr;
			}
			{
				FBaseGraphTask* Task = SortedAnyThreadTasksHiPri.Pop();
				if (Task)
				{
					SetWorking(MyIndex, LocalNumWorkingThread);
					return Task;
				}
			}
			TestRandomizedThreads();
			FPlatformMisc::MemoryBarrier();
			FAtomicStateBitfield LocalAtomicForConsoleApproach = AtomicForConsoleApproach;
			check(!(LocalAtomicForConsoleApproach.Stalled & (1 << MyIndex))); // if I was stalled, I would not be here
			if (!LocalAtomicForConsoleApproach.QueueOwned)
			{
				if (IncomingAnyThreadTasksHiPri.IsEmpty())
				{
					FBaseGraphTask* Task = SortedAnyThreadTasks.Pop();
					if (Task)
					{
						SetWorking(MyIndex, LocalNumWorkingThread);
						return Task;
					}
				}
				FAtomicStateBitfield NewAtomicForConsoleApproach = LocalAtomicForConsoleApproach;
				// Queue is unowned, lets grab it and try to empty them
				NewAtomicForConsoleApproach.QueueOwned = 1;
				NewAtomicForConsoleApproach.QueueOwnerIndex = MyIndex;
				NewAtomicForConsoleApproach.Working = LocalAtomicForConsoleApproach.Working & ~(1 << MyIndex);

				check(LocalAtomicForConsoleApproach != NewAtomicForConsoleApproach);

				if (FAtomicStateBitfield::InterlockedCompareExchange(&AtomicForConsoleApproach, NewAtomicForConsoleApproach, LocalAtomicForConsoleApproach) == LocalAtomicForConsoleApproach)
				{
					TestRandomizedThreads();
					// we win
					// dequeue any pending things
					static TArray<FBaseGraphTask*> NewTasks;
					int32 TotalTasks = 0;
					bool bWeWereBeat = false;
					if (SortedAnyThreadTasksHiPri.IsEmpty()) // otherwise someone beat us to it, so we need to drain the queue first
					{
						NewTasks.Reset();
						IncomingAnyThreadTasksHiPri.PopAll(NewTasks);
						if (NewTasks.Num())
						{
							TotalTasks += NewTasks.Num();
							TLockFreePointerListLIFO<FBaseGraphTask> TempSortedAnyThreadTasks;
							for (int32 Index = 0 ; Index < NewTasks.Num(); Index++) // we are going to take the last one for ourselves
							{
								TempSortedAnyThreadTasks.Push(NewTasks[Index]);
							}
							verify(SortedAnyThreadTasksHiPri.ReplaceListIfEmpty(TempSortedAnyThreadTasks));
						}
					}
					else
					{
						bWeWereBeat = true;
					}
					if (SortedAnyThreadTasks.IsEmpty()) // otherwise someone beat us to it, so we need to drain the queue first
					{
						NewTasks.Reset();
						IncomingAnyThreadTasks.PopAll(NewTasks);
						if (NewTasks.Num())
						{
							TotalTasks += NewTasks.Num();
							TLockFreePointerListLIFO<FBaseGraphTask> TempSortedAnyThreadTasks;
							for (int32 Index = 0 ; Index < NewTasks.Num(); Index++) // we are going to take the last one for ourselves
							{
								TempSortedAnyThreadTasks.Push(NewTasks[Index]);
							}
							verify(SortedAnyThreadTasks.ReplaceListIfEmpty(TempSortedAnyThreadTasks));
						}
					}
					else
					{
						bWeWereBeat = true;
					}
					// release the ownership
					while (true)
					{
						TestRandomizedThreads();
						FAtomicStateBitfield LocalAtomicForConsoleApproachInner = AtomicForConsoleApproach;
						check(!(LocalAtomicForConsoleApproachInner.Stalled & (1 << MyIndex))); // if I was stalled, I would not be here
						check(!GConsoleSpinModeLatched || !(LocalAtomicForConsoleApproachInner.Working & (1 << MyIndex))); // if I was working, I would not be here


						FAtomicStateBitfield NewAtomicForConsoleApproachInner = LocalAtomicForConsoleApproachInner;
						check(LocalAtomicForConsoleApproachInner.QueueOwned && LocalAtomicForConsoleApproachInner.QueueOwnerIndex == MyIndex);
						NewAtomicForConsoleApproachInner.QueueOwned = false;
						NewAtomicForConsoleApproachInner.QueueOwnerIndex = 0;
						bool bAttemptStall = !TotalTasks && !bWeWereBeat &&
							(!GConsoleSpinModeLatched || LocalAtomicForConsoleApproachInner.NumberOfStalledThreads() + LocalAtomicForConsoleApproachInner.NumberOfWorkingThreads() < LocalNumWorkingThread - 1);
						if (bAttemptStall)
						{
							NewAtomicForConsoleApproachInner.Stalled = LocalAtomicForConsoleApproachInner.Stalled | (1 << MyIndex);
							NewAtomicForConsoleApproachInner.Working = LocalAtomicForConsoleApproachInner.Working & ~(1 << MyIndex); // this is redundant in most cases, but might as well track the correct state
							check((((uint32)NewAtomicForConsoleApproachInner.Stalled) & (1 << MyIndex)));
						}

						check(LocalAtomicForConsoleApproachInner != NewAtomicForConsoleApproachInner);
						if (FAtomicStateBitfield::InterlockedCompareExchange(&AtomicForConsoleApproach, NewAtomicForConsoleApproachInner, LocalAtomicForConsoleApproachInner) == LocalAtomicForConsoleApproachInner)
						{
							TestRandomizedThreads();
							if (bAttemptStall)
							{
								return nullptr; // we did stall
							}
							break;
						}
					}
					if (GConsoleSpinModeLatched)
					{
						// -- here because if we actually do one of these tasks, we will start another thread by transitioning into working
						TotalTasks--;
						int32 ThreadsToStart = FMath::Min<int32>(FMath::Min<int32>(TotalTasks, GMaxTasksToStartOnDequeue), LocalNumWorkingThread);
						while (ThreadsToStart > 0)
						{
							TestRandomizedThreads();
							if (!StartTaskThreadFastMode(MyIndex))
							{
								break;
							}
							ThreadsToStart--;
						}
					}
				}
			}
		}
	}
	// API used by FWorkerThread's

	/** 
	 *	Attempt to steal some work from another thread.
	 *	@param	ThreadInNeed; Id of the thread requesting work.
	 *	@return Task that was stolen if any was found.
	**/
	FBaseGraphTask* FindWork(ENamedThreads::Type ThreadInNeed)
	{
		TestRandomizedThreads();
		if (GFastSchedulerLatched)
		{
			return FindWorkConsole(ThreadInNeed);
		}
		{
			FBaseGraphTask* Task = SortedAnyThreadTasksHiPri.Pop();
			if (Task)
			{
				return Task;
			}
		}
		if (!IncomingAnyThreadTasksHiPri.IsEmpty())
		{
			do
			{
				FScopeLock ScopeLock(&CriticalSectionForSortingIncomingAnyThreadTasksHiPri);
				if (GFastSchedulerLatched)
				{
					return nullptr;
				}
				if (!IncomingAnyThreadTasksHiPri.IsEmpty() && SortedAnyThreadTasksHiPri.IsEmpty())
				{
					static TArray<FBaseGraphTask*> NewTasks;
					NewTasks.Reset();
					IncomingAnyThreadTasksHiPri.PopAll(NewTasks);
					check(NewTasks.Num());

					if (NewTasks.Num() > 1)
					{
						TLockFreePointerListLIFO<FBaseGraphTask> TempSortedAnyThreadTasks;
						for (int32 Index = 0 ; Index < NewTasks.Num() - 1; Index++) // we are going to take the last one for ourselves
						{
							TempSortedAnyThreadTasks.Push(NewTasks[Index]);
						}
						verify(SortedAnyThreadTasksHiPri.ReplaceListIfEmpty(TempSortedAnyThreadTasks));
					}
					return NewTasks[NewTasks.Num() - 1];
				}
				{
					FBaseGraphTask* Task = SortedAnyThreadTasksHiPri.Pop();
					if (Task)
					{
						return Task;
					}
				}
			} while (!IncomingAnyThreadTasksHiPri.IsEmpty() || !SortedAnyThreadTasksHiPri.IsEmpty());
		}
		{
			FBaseGraphTask* Task = SortedAnyThreadTasks.Pop();
			if (Task)
			{
				return Task;
			}
		}
		do
		{
			FScopeLock ScopeLock(&CriticalSectionForSortingIncomingAnyThreadTasks);
			if (GFastSchedulerLatched)
			{
				return nullptr;
			}
			if (!IncomingAnyThreadTasks.IsEmpty() && SortedAnyThreadTasks.IsEmpty())
			{
				static TArray<FBaseGraphTask*> NewTasks;
				NewTasks.Reset();
				IncomingAnyThreadTasks.PopAll(NewTasks);
				check(NewTasks.Num());

				if (NewTasks.Num() > 1)
				{
					TLockFreePointerListLIFO<FBaseGraphTask> TempSortedAnyThreadTasks;
					for (int32 Index = 0 ; Index < NewTasks.Num() - 1; Index++) // we are going to take the last one for ourselves
					{
						TempSortedAnyThreadTasks.Push(NewTasks[Index]);
					}
					verify(SortedAnyThreadTasks.ReplaceListIfEmpty(TempSortedAnyThreadTasks));
				}
				return NewTasks[NewTasks.Num() - 1];
			}
			{
				FBaseGraphTask* Task = SortedAnyThreadTasks.Pop();
				if (Task)
				{
					return Task;
				}
			}
		} while (!IncomingAnyThreadTasks.IsEmpty() || !SortedAnyThreadTasks.IsEmpty());
		return nullptr;
	}

#endif

	/** 
	 *	Hint from a worker thread that it is stalling.
	 *	@param	StallingThread; Id of the thread that is stalling.
	**/
	void NotifyStalling(ENamedThreads::Type StallingThread)
	{
		if (StallingThread >= NumNamedThreads && !GFastSchedulerLatched)
		{
			StalledUnnamedThreads.Push(&Thread(StallingThread));
		}
	}

	void SetTaskThreadPriorities(EThreadPriority Pri)
	{
		for (int32 ThreadIndex = 0; ThreadIndex < NumThreads; ThreadIndex++)
		{
			if (ThreadIndex > LastExternalThread)
			{
				WorkerThreads[ThreadIndex].RunnableThread->SetThreadPriority(Pri);
			}
		}
	}

private:

	// Internals

	/** 
	 *	Internal function to verify an index and return the corresponding FTaskThread
	 *	@param	Index; Id of the thread to retrieve.
	 *	@return	Reference to the corresponding thread.
	**/
	FTaskThreadBase& Thread(int32 Index)
	{
		checkThreadGraph(Index >= 0 && Index < NumThreads);
		checkThreadGraph(WorkerThreads[Index].TaskGraphWorker->GetThreadId() == Index);
		return *WorkerThreads[Index].TaskGraphWorker;
	}

	/** 
	 *	Examines the TLS to determine the identity of the current thread.
	 *	@return	Id of the thread that is this thread or ENamedThreads::AnyThread if this thread is unknown or is a named thread that has not attached yet.
	**/
	ENamedThreads::Type GetCurrentThread()
	{
		ENamedThreads::Type CurrentThreadIfKnown = ENamedThreads::AnyThread;
		FWorkerThread* TLSPointer = (FWorkerThread*)FPlatformTLS::GetTlsValue(PerThreadIDTLSSlot);
		if (TLSPointer)
		{
			checkThreadGraph(TLSPointer - WorkerThreads >= 0 && TLSPointer - WorkerThreads < NumThreads);
			CurrentThreadIfKnown = ENamedThreads::Type(TLSPointer - WorkerThreads);
			checkThreadGraph(Thread(CurrentThreadIfKnown).GetThreadId() == CurrentThreadIfKnown);
		}
		return CurrentThreadIfKnown;
	}


	enum
	{
		/** Compile time maximum number of threads. @todo Didn't really need to be a compile time constant. **/
		MAX_THREADS=12
	};

	/** Per thread data. **/
	FWorkerThread		WorkerThreads[MAX_THREADS];
	/** Number of threads actually in use. **/
	int32				NumThreads;
	/** Number of named threads actually in use. **/
	int32				NumNamedThreads;
	/** 
	 * "External Threads" are not created, the thread is created elsewhere and makes an explicit call to run 
	 * Here all of the named threads are external but that need not be the case.
	 * All unnamed threads must be internal
	**/
	ENamedThreads::Type LastExternalThread;
	/** Counter used to distribute new jobs to "any thread". **/
	FThreadSafeCounter	NextUnnamedThreadForTaskFromUnknownThread;
	/** Number of unnamed threads. **/
	int32					NextUnnamedThreadMod;
	/** Counter used to determine next thread to attempt to steal from. **/
	FThreadSafeCounter	NextStealFromThread;
	/** Index of TLS slot for FWorkerThread* pointer. **/
	uint32				PerThreadIDTLSSlot;
	/** Thread safe list of stalled thread "Hints". **/
	TLockFreePointerListUnordered<FTaskThreadBase>		StalledUnnamedThreads; 

#if USE_NEW_LOCK_FREE_LISTS
	#if USE_INTRUSIVE_TASKQUEUES
		FLockFreePointerListFIFOIntrusive<FBaseGraphTask>		IncomingAnyThreadTasks;
		FLockFreePointerListFIFOIntrusive<FBaseGraphTask>		IncomingAnyThreadTasksHiPri;
	#else
		TLockFreePointerListFIFO<FBaseGraphTask>		IncomingAnyThreadTasks;
		TLockFreePointerListFIFO<FBaseGraphTask>		IncomingAnyThreadTasksHiPri;
	#endif
#else
	TLockFreePointerListLIFO<FBaseGraphTask>		IncomingAnyThreadTasks;
	TLockFreePointerListLIFO<FBaseGraphTask>		IncomingAnyThreadTasksHiPri;
	TLockFreePointerListLIFO<FBaseGraphTask>		SortedAnyThreadTasks;
	TLockFreePointerListLIFO<FBaseGraphTask>		SortedAnyThreadTasksHiPri;
#endif
	FCriticalSection CriticalSectionForSortingIncomingAnyThreadTasks;
	FCriticalSection CriticalSectionForSortingIncomingAnyThreadTasksHiPri;

	// we use a single atomic to control the state of the anythread queues. This limits the maximum number of threads.
	struct FAtomicStateBitfield
	{
#if !USE_NEW_LOCK_FREE_LISTS
		uint32 QueueOwned: 1;
		uint32 QueueOwnerIndex: 4;
#endif
		uint32 Stalled: FAtomicStateBitfield_MAX_THREADS;
		uint32 Working: FAtomicStateBitfield_MAX_THREADS;

		FAtomicStateBitfield()
			: 
#if !USE_NEW_LOCK_FREE_LISTS
			QueueOwned(false)
			, QueueOwnerIndex(0) ,
#endif
			 Stalled(0)
			, Working(0)
		{
		}

		static FORCEINLINE FAtomicStateBitfield InterlockedCompareExchange(FAtomicStateBitfield* Dest, FAtomicStateBitfield Exchange, FAtomicStateBitfield Comparand)
		{
			FAtomicStateBitfield Result;
			*(int32*)&Result = FPlatformAtomics::InterlockedCompareExchange((volatile int32*)Dest, *(int32*)&Exchange, *(int32*)&Comparand);
			return Result;			
		}

		static int32 CountBits(uint32 Bits)
		{
			MS_ALIGN(64) static uint8 Table[64] GCC_ALIGN(64) = {0};
			if (!Table[63])
			{
				for (uint32 Index = 0; Index < 63; Index++)
				{
					uint32 LocalIndex = Index;
					uint8 Result = 0;
					while (LocalIndex)
					{
						if (LocalIndex & 1)
						{
							Result++;
						}
						LocalIndex >>= 1;
					}
					Table[Index] = Result;
				}
				FPlatformMisc::MemoryBarrier();
				Table[63] = 6;
			}
			int32 Count = 0;
			while (true)
			{
				Count += Table[Bits & 63];
				if (!(Bits & ~63))
				{
					break;
				}
				Bits >>= 6;
			}
			return Count;
		}

		FORCEINLINE int32 NumberOfStalledThreads()
		{
			return CountBits(Stalled);
		}

		FORCEINLINE int32 NumberOfWorkingThreads()
		{
			return CountBits(Working);
		}

		bool operator==(FAtomicStateBitfield Other) const
		{
			return 
#if !USE_NEW_LOCK_FREE_LISTS
				QueueOwned == Other.QueueOwned &&
				QueueOwnerIndex == Other.QueueOwnerIndex &&
#endif
				Stalled == Other.Stalled &&
				Working == Other.Working;
		}

		bool operator!=(FAtomicStateBitfield Other) const
		{
			return 
#if !USE_NEW_LOCK_FREE_LISTS
				QueueOwned != Other.QueueOwned ||
				QueueOwnerIndex != Other.QueueOwnerIndex ||
#endif
				Stalled != Other.Stalled ||
				Working != Other.Working;
		}
	};

	FAtomicStateBitfield AtomicForConsoleApproach;
};


// Implementations of FTaskThread function that require knowledge of FTaskGraphImplementation

FBaseGraphTask* FTaskThreadAnyThread::FindWork()
{
	return FTaskGraphImplementation::Get().FindWork(ThreadId);
}

void FTaskThreadAnyThread::NotifyStalling()
{
	return FTaskGraphImplementation::Get().NotifyStalling(ThreadId);
}



// Statics in FTaskGraphInterface

void FTaskGraphInterface::Startup(int32 NumThreads)
{
	// TaskGraphImplementationSingleton is actually set in the constructor because find work will be called before this returns.
	new FTaskGraphImplementation(NumThreads); 
}

void FTaskGraphInterface::Shutdown()
{
	delete TaskGraphImplementationSingleton;
	TaskGraphImplementationSingleton = NULL;
}

bool FTaskGraphInterface::IsRunning()
{
    return TaskGraphImplementationSingleton != NULL;
}

FTaskGraphInterface& FTaskGraphInterface::Get()
{
	checkThreadGraph(TaskGraphImplementationSingleton);
	return *TaskGraphImplementationSingleton;
}


// Statics and some implementations from FBaseGraphTask and FGraphEvent

static FBaseGraphTask::TSmallTaskAllocator TheSmallTaskAllocator;
FBaseGraphTask::TSmallTaskAllocator& FBaseGraphTask::GetSmallTaskAllocator()
{
	return TheSmallTaskAllocator;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void FBaseGraphTask::LogPossiblyInvalidSubsequentsTask(const TCHAR* TaskName)
{
	UE_LOG(LogTaskGraph, Warning, TEXT("Subsequents of %s look like they contain invalid pointer(s)."), TaskName);
}
#endif

#if USE_NEW_LOCK_FREE_LISTS

#define USE_TLS_FOR_EVENTS (1)

#define MONITOR_TASK_ALLOCATION (0)

#if	MONITOR_TASK_ALLOCATION==1
	typedef FThreadSafeCounter	FTaskLockFreeListCounter;
#else
	typedef FNoopCounter		FTaskLockFreeListCounter;
#endif

static FTaskLockFreeListCounter GraphEventsInUse;
static FTaskLockFreeListCounter GraphEventsWithInlineStorageInUse;

#if USE_TLS_FOR_EVENTS

static TPointerSet_TLSCacheBase<FGraphEvent, TLockFreePointerListUnordered<FGraphEvent>, FTaskLockFreeListCounter> TheGraphEventAllocator;
static TPointerSet_TLSCacheBase<FGraphEventAndSmallTaskStorage, TLockFreePointerListUnordered<FGraphEventAndSmallTaskStorage>, FTaskLockFreeListCounter> TheGraphEventWithInlineStorageAllocator;

static FThreadSafeCounter GraphEventsAllocated;
static FThreadSafeCounter GraphEventsWithInlineStorageAllocated;

FGraphEvent* FGraphEvent::GetBundle(int32 NumBundle)
{
	GraphEventsAllocated.Add(NumBundle);
	FGraphEvent* Root = nullptr;
	uint8* Block = (uint8*)FMemory::Malloc(NumBundle * sizeof(FGraphEvent));
	for (int32 Index = 0; Index < NumBundle; Index++)
	{
		FGraphEvent* Event = new ((void*)Block) FGraphEvent(false);
		Event->LockFreePointerQueueNext = Root;
		Root = Event;
		Block += sizeof(FGraphEvent);
	}
	return Root;
}

FGraphEventAndSmallTaskStorage* FGraphEventAndSmallTaskStorage::GetBundle(int32 NumBundle)
{
	GraphEventsWithInlineStorageAllocated.Add(NumBundle);
	FGraphEventAndSmallTaskStorage* Root = nullptr;
	uint8* Block = (uint8*)FMemory::Malloc(NumBundle * sizeof(FGraphEventAndSmallTaskStorage));
	for (int32 Index = 0; Index < NumBundle; Index++)
	{
		FGraphEventAndSmallTaskStorage* Event = new ((void*)Block) FGraphEventAndSmallTaskStorage();
		Event->LockFreePointerQueueNext = Root;
		Root = Event;
		Block += sizeof(FGraphEventAndSmallTaskStorage);
	}
	return Root;
}

#else
static FLockFreePointerListFIFOIntrusive<FGraphEvent> TheGraphEventAllocator;
static FLockFreePointerListFIFOIntrusive<FGraphEventAndSmallTaskStorage> TheGraphEventWithInlineStorageAllocator;

#endif

FGraphEventRef FGraphEvent::CreateGraphEvent()
{
	GraphEventsInUse.Increment();
	FGraphEvent* Used =  TheGraphEventAllocator.Pop();
	if (Used)
	{
		FPlatformMisc::MemoryBarrier();
		checkThreadGraph(!Used->SubsequentList.IsClosed());
		checkThreadGraph(Used->ReferenceCount.GetValue() == 0);
		checkThreadGraph(!Used->EventsToWaitFor.Num());
		checkThreadGraph(!Used->bComplete);
		checkThreadGraph(!Used->bInline);
//		Used->ReferenceCount.Reset();  // temp
//		Used->LockFreePointerQueueNext = nullptr; // temp
		return FGraphEventRef(Used);
	}
	return FGraphEventRef(new FGraphEvent(false));
}

FGraphEvent* FGraphEvent::CreateGraphEventWithInlineStorage()
{
	GraphEventsWithInlineStorageInUse.Increment();
	FGraphEventAndSmallTaskStorage* Used =  TheGraphEventWithInlineStorageAllocator.Pop();
	if (Used)
	{
		FPlatformMisc::MemoryBarrier();
		checkThreadGraph(!Used->SubsequentList.IsClosed());
		checkThreadGraph(Used->ReferenceCount.GetValue() == 0);
		checkThreadGraph(!Used->EventsToWaitFor.Num());
		checkThreadGraph(!Used->bComplete);
		checkThreadGraph(Used->bInline);
	}
	else
	{
		Used = new FGraphEventAndSmallTaskStorage();
	}
	checkThreadGraph(Used->bInline);
	return Used;
}


void FGraphEvent::Recycle(FGraphEvent* ToRecycle)
{
	ToRecycle->Reset();
	if (ToRecycle->bInline)
	{
		GraphEventsWithInlineStorageInUse.Decrement();
		TheGraphEventWithInlineStorageAllocator.Push((FGraphEventAndSmallTaskStorage*)ToRecycle);
	}
	else
	{
		GraphEventsInUse.Decrement();
		TheGraphEventAllocator.Push(ToRecycle);
	}
}


void FGraphEvent::Reset()
{
//	ReferenceCount.Set(-5); // temp
	SubsequentList.Reset();
	checkThreadGraph(!SubsequentList.Pop());
	EventsToWaitFor.Empty();  // we will let the memory block free here if there is one; these are not common
	// checkThreadGraph(!LockFreePointerQueueNext); // temp
	bComplete= false;
}

#else
static TLockFreeClassAllocator_TLSCache<FGraphEvent> TheGraphEventAllocator;

FGraphEventRef FGraphEvent::CreateGraphEvent()
{
	return TheGraphEventAllocator.New();
}

void FGraphEvent::Recycle(FGraphEvent* ToRecycle)
{
	TheGraphEventAllocator.Free(ToRecycle);
}
#endif


void FGraphEvent::DispatchSubsequents(TArray<FBaseGraphTask*>& NewTasks, ENamedThreads::Type CurrentThreadIfKnown)
{
	if (EventsToWaitFor.Num())
	{
		// need to save this first and empty the actual tail of the task might be recycled faster than it is cleared.
		FGraphEventArray TempEventsToWaitFor;
		Exchange(EventsToWaitFor, TempEventsToWaitFor);
		// create the Gather...this uses a special version of private CreateTask that "assumes" the subsequent list (which other threads might still be adding too).
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.DontCompleteUntil"),
			STAT_FNullGraphTask_DontCompleteUntil,
			STATGROUP_TaskGraphTasks);

		TGraphTask<FNullGraphTask>::CreateTask(FGraphEventRef(this), &TempEventsToWaitFor, CurrentThreadIfKnown).ConstructAndDispatchWhenReady(GET_STATID(STAT_FNullGraphTask_DontCompleteUntil), ENamedThreads::HiPri(ENamedThreads::AnyThread));
		return;
	}

#if USE_NEW_LOCK_FREE_LISTS
	bComplete = true;
	int32 SpinCount = 0;
	while (true)
	{
		FBaseGraphTask* NewTask = SubsequentList.Pop();
		if (!NewTask)
		{
			if (SubsequentList.CloseIfEmpty())
			{
				break;
			}
			LockFreeCriticalSpin(SpinCount);
			continue;
		}
		NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
	}
#else
	SubsequentList.PopAllAndClose(NewTasks);
	for (int32 Index = NewTasks.Num() - 1; Index >= 0 ; Index--) // reverse the order since PopAll is implicitly backwards
	{
		FBaseGraphTask* NewTask = NewTasks[Index];
		checkThreadGraph(NewTask);
		NewTask->ConditionalQueueTask(CurrentThreadIfKnown);
	}
	NewTasks.Reset();
#endif
}

FGraphEvent::~FGraphEvent()
{
#if DO_CHECK
	if (!IsComplete())
	{
#if USE_NEW_LOCK_FREE_LISTS
		checkThreadGraph(!SubsequentList.Pop());
#else
		// Verifies that the event is completed. We do not allow events to die before completion.
		TArray<FBaseGraphTask*> NewTasks;
		SubsequentList.PopAllAndClose(NewTasks);
		checkThreadGraph(!NewTasks.Num());
#endif
	}
#endif
	CheckDontCompleteUntilIsEmpty(); // We should not have any wait untils outstanding
}



// Benchmark

#include "ParallelFor.h"

static FORCEINLINE void DoWork(void* Hash, FThreadSafeCounter& Counter, FThreadSafeCounter& Cycles, int32 Work)
{
	if (Work > 0)
	{
		uint32 CyclesStart = FPlatformTime::Cycles();
		Counter.Increment();
		int32 Sum = 0;
		for (int32 Index = 0; Index < Work; Index++)
		{
			Sum += PointerHash(((uint64*)Hash) + Index);
		}
		Cycles.Add(FPlatformTime::Cycles() - CyclesStart + (Sum & 1));
	}
	else if (Work == 0)
	{
		Counter.Increment();
	}
}

class FIncGraphTask : public FCustomStatIDGraphTaskBase
{
public:
	FORCEINLINE FIncGraphTask(FThreadSafeCounter& InCounter, FThreadSafeCounter& InCycles, int32 InWork)
		: FCustomStatIDGraphTaskBase(TStatId())
		, Counter(InCounter)
		, Cycles(InCycles)
		, Work(InWork)
	{
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }
	void FORCEINLINE DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		DoWork(this, Counter, Cycles, Work);
	}
private:
	FThreadSafeCounter& Counter;
	FThreadSafeCounter& Cycles;
	int32 Work;
};

class FIncGraphTaskGT : public FIncGraphTask
{
public:
	FORCEINLINE FIncGraphTaskGT(FThreadSafeCounter& InCounter, FThreadSafeCounter& InCycles, int32 InWork)
		: FIncGraphTask(InCounter, InCycles, InWork)
	{
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread_Local;
	}
};

class FBoolGraphTask : public FCustomStatIDGraphTaskBase
{
public:
	FORCEINLINE FBoolGraphTask(bool *InOut)
		: FCustomStatIDGraphTaskBase(TStatId())
		, Out(InOut)
	{
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::FireAndForget; }
	void FORCEINLINE DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		*Out = true;
	}
private:
	bool *Out;
};


void PrintResult(double& StartTime, double& QueueTime, double& EndTime, double& JoinTime, FThreadSafeCounter& Counter, FThreadSafeCounter& Cycles, const TCHAR* Message)
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Total %6.3fms   %6.3fms queue   %6.3fms join   %6.3fms wait   %6.3fms work   : %s")
		, float(1000.0 * (EndTime-StartTime)), float(1000.0 * (QueueTime-StartTime)), float(1000.0 * (JoinTime-QueueTime)), float(1000.0 * (EndTime-JoinTime))
		, float(FPlatformTime::GetSecondsPerCycle() * double(Cycles.GetValue()) * 1000.0)
		, Message
		);

	Counter.Reset();
	Cycles.Reset();
	StartTime = 0.0;
	QueueTime = 0.0;
	EndTime = 0.0;
	JoinTime = 0.0;
}

static void TaskGraphBenchmark(const TArray<FString>& Args)
{
	double StartTime, QueueTime, EndTime, JoinTime;
	FThreadSafeCounter Counter;
	FThreadSafeCounter Cycles;
	
	if (Args.Num() == 1 && Args[0] == TEXT("infinite"))
	{
		while (true)
		{
			{
				StartTime = FPlatformTime::Seconds();

				ParallelFor(1000, 
					[&Counter, &Cycles](int32 Index)
				{
					TGraphTask<FIncGraphTaskGT>::CreateTask().ConstructAndDispatchWhenReady(Counter, Cycles, -1);
				}
				);
				QueueTime = FPlatformTime::Seconds();
				JoinTime = QueueTime;
				FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
				EndTime = FPlatformTime::Seconds();
			}
		}
	}
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.Reserve(1000);
		for (int32 Index = 0; Index < 1000; Index++)
		{
			Tasks.Emplace(TGraphTask<FNullGraphTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread));
		}
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ordinary GT start"));
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.AddZeroed(1000);

		ParallelFor(1000, 
			[&Tasks](int32 Index)
			{
				Tasks[Index] = TGraphTask<FNullGraphTask>::CreateTask().ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
			}
		);
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor start"));
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.AddZeroed(10);

		ParallelFor(10, 
			[&Tasks](int32 Index)
		{
			FGraphEventArray InnerTasks;
			InnerTasks.AddZeroed(100);
			ENamedThreads::Type CurrentThread = FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			for (int32 InnerIndex = 0; InnerIndex < 100; InnerIndex++)
			{
				InnerTasks[InnerIndex] = TGraphTask<FNullGraphTask>::CreateTask(nullptr, CurrentThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
			}
			// join the above tasks
			Tasks[Index] = TGraphTask<FNullGraphTask>::CreateTask(&InnerTasks, CurrentThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor start, batched completion 10x100"));
	{
		StartTime = FPlatformTime::Seconds();
		FGraphEventArray Tasks;
		Tasks.AddZeroed(100);

		ParallelFor(100, 
			[&Tasks](int32 Index)
		{
			FGraphEventArray InnerTasks;
			InnerTasks.AddZeroed(10);
			ENamedThreads::Type CurrentThread = FTaskGraphInterface::Get().GetCurrentThreadIfKnown();
			for (int32 InnerIndex = 0; InnerIndex < 10; InnerIndex++)
			{
				InnerTasks[InnerIndex] = TGraphTask<FNullGraphTask>::CreateTask(nullptr, CurrentThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
			}
			// join the above tasks
			Tasks[Index] = TGraphTask<FNullGraphTask>::CreateTask(&InnerTasks, CurrentThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		FGraphEventRef Join = TGraphTask<FNullGraphTask>::CreateTask(&Tasks, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(TStatId(), ENamedThreads::AnyThread);
		JoinTime = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Join, ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor start, batched completion 100x10"));

	{
		StartTime = FPlatformTime::Seconds();

		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
			{
				TGraphTask<FIncGraphTask>::CreateTask().ConstructAndDispatchWhenReady(Counter, Cycles, 0);
			}
		);
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		while (Counter.GetValue() < 1000)
		{
			FPlatformMisc::MemoryBarrier();
		}
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor, counter tracking"));

	{
		StartTime = FPlatformTime::Seconds();

		static bool Output[1000];
		FPlatformMemory::Memzero(Output, 1000);

		ParallelFor(1000, 
			[](int32 Index)
		{
			TGraphTask<FBoolGraphTask>::CreateTask().ConstructAndDispatchWhenReady(Output + Index);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		for (int32 Index = 0; Index < 1000; Index++)
		{
			while (!Output[Index])
			{
				FPlatformProcess::Sleep(0.0f);
			}
		}
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor, bool* tracking"));

	{
		StartTime = FPlatformTime::Seconds();

		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
		{
			TGraphTask<FIncGraphTask>::CreateTask().ConstructAndDispatchWhenReady(Counter, Cycles, 1000);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		while (Counter.GetValue() < 1000)
		{
			FPlatformProcess::Sleep(0.0f);
		}
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, ParallelFor, counter tracking, with work"));
	{
		StartTime = FPlatformTime::Seconds();
		for (int32 Index = 0; Index < 1000; Index++)
		{
			TGraphTask<FIncGraphTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(Counter, Cycles, 1000);
		}
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		while (Counter.GetValue() < 1000)
		{
			FPlatformProcess::Sleep(0.0f);
		}
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 tasks, GT submit, counter tracking, with work"));

	{
		StartTime = FPlatformTime::Seconds();

		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
		{
			TGraphTask<FIncGraphTaskGT>::CreateTask().ConstructAndDispatchWhenReady(Counter, Cycles, -1);
		}
		);
		QueueTime = FPlatformTime::Seconds();
		JoinTime = QueueTime;
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 GT tasks, ParallelFor, no tracking (none needed)"));


	{
		StartTime = FPlatformTime::Seconds();
		QueueTime = StartTime;
		JoinTime = QueueTime;
		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
			{
				DoWork(&Counter, Counter, Cycles, -1);
			}
		);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 element do-nothing ParallelFor"));
	{
		StartTime = FPlatformTime::Seconds();
		QueueTime = StartTime;
		JoinTime = QueueTime;
		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
			{
				DoWork(&Counter, Counter, Cycles, 1000);
			}
		);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 element ParallelFor, with work"));

	{
		StartTime = FPlatformTime::Seconds();
		QueueTime = StartTime;
		JoinTime = QueueTime;
		ParallelFor(1000, 
			[&Counter, &Cycles](int32 Index)
			{
				DoWork(&Counter, Counter, Cycles, 1000);
			},
			true
		);
		EndTime = FPlatformTime::Seconds();
	}
	PrintResult(StartTime, QueueTime, EndTime, JoinTime, Counter, Cycles, TEXT("1000 element ParallelFor, single threaded, with work"));
}

static FAutoConsoleCommand TaskGraphBenchmarkCmd(
	TEXT("TaskGraph.Benchmark"),
	TEXT("Prints the time to run 1000 no-op tasks."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&TaskGraphBenchmark)
	);

static void SetTaskThreadPriority(const TArray<FString>& Args)
{
	EThreadPriority Pri = TPri_Normal;
	if (Args.Num() && Args[0] == TEXT("abovenormal"))
	{
		Pri = TPri_AboveNormal;
		UE_LOG(LogConsoleResponse, Display, TEXT("Setting task thread priority to above normal."));
	}
	else if (Args.Num() && Args[0] == TEXT("belownormal"))
	{
		Pri = TPri_BelowNormal;
		UE_LOG(LogConsoleResponse, Display, TEXT("Setting task thread priority to below normal."));
	}
	else
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Setting task thread priority to normal."));
	}
	FTaskGraphImplementation::Get().SetTaskThreadPriorities(Pri);
}

static FAutoConsoleCommand TaskThreadPriorityCmd(
	TEXT("TaskGraph.TaskThreadPriority"),
	TEXT("Sets the priority of the task threads. Argument is one of belownormal, normal or abovenormal."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&SetTaskThreadPriority)
	);
