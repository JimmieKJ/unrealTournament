// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Function.h"

class FTlsAutoCleanup;
class FRunnableThread;

/**
 * The list of enumerated thread priorities we support
 */
enum EThreadPriority
{
	TPri_Normal,
	TPri_AboveNormal,
	TPri_BelowNormal,
	TPri_Highest,
	TPri_Lowest,
	TPri_SlightlyBelowNormal,
};


/**
 * Interface for waitable events.
 *
 * This interface has platform-specific implementations that are used to wait for another
 * thread to signal that it is ready for the waiting thread to do some work. It can also
 * be used for telling groups of threads to exit.
 */
class FEvent
{
public:

	/**
	 * Creates the event.
	 *
	 * Manually reset events stay triggered until reset.
	 * Named events share the same underlying event.
	 *
	 * @param bIsManualReset Whether the event requires manual reseting or not.
	 * @return true if the event was created, false otherwise.
	 */
	virtual bool Create( bool bIsManualReset = false ) = 0;

	/**
	 * Whether the signaled state of this event needs to be reset manually.
	 *
	 * @return true if the state requires manual resetting, false otherwise.
	 * @see Reset
	 */
	virtual bool IsManualReset() = 0;

	/**
	 * Triggers the event so any waiting threads are activated.
	 *
	 * @see IsManualReset, Reset
	 */
	virtual void Trigger() = 0;

	/**
	 * Resets the event to an untriggered (waitable) state.
	 *
	 * @see IsManualReset, Trigger
	 */
	virtual void Reset() = 0;

	/**
	 * Waits the specified amount of time for the event to be triggered.
	 *
	 * A wait time of MAX_uint32 is treated as infinite wait.
	 *
	 * @param WaitTime The time to wait (in milliseconds).
	 * @param bIgnoreThreadIdleStats If true, ignores ThreadIdleStats
	 * @return true if the event was triggered, false if the wait timed out.
	 */
	virtual bool Wait( uint32 WaitTime, const bool bIgnoreThreadIdleStats = false ) = 0;

	/**
	 * Waits an infinite amount of time for the event to be triggered.
	 *
	 * @return true if the event was triggered.
	 */
	bool Wait()
	{
		return Wait(MAX_uint32);
	}

	/**
	 * Waits the specified amount of time for the event to be triggered.
	 *
	 * @param WaitTime The time to wait.
	 * @param bIgnoreThreadIdleStats If true, ignores ThreadIdleStats
	 * @return true if the event was triggered, false if the wait timed out.
	 */
	bool Wait( const FTimespan& WaitTime, const bool bIgnoreThreadIdleStats = false )
	{
		return Wait(WaitTime.GetTotalMilliseconds(), bIgnoreThreadIdleStats);
	}

	/** Default constructor. */
	FEvent()
		: EventId( 0 )
		, EventStartCycles( 0 )
	{}

	/** Virtual destructor. */
	virtual ~FEvent() 
	{}

	// DO NOT MODIFY THESE

	/** Advances stats associated with this event. Used to monitor wait->trigger history. */
	void AdvanceStats();

protected:
	/** Sends to the stats a special messages which encodes a wait for the event. */
	void WaitForStats();

	/** Send to the stats a special message which encodes a trigger for the event. */
	void TriggerForStats();

	/** Resets start cycles to 0. */
	void ResetForStats();

	/** Counter used to generate an unique id for the events. */
	static uint32 EventUniqueId;

	/** An unique id of this event. */
	uint32 EventId;

	/** Greater than 0, if the event called wait. */
	uint32 EventStartCycles;
};


/**
 * This class is allows a simple one-shot scoped event.
 *
 * Usage:
 * {
 *		FScopedEvent MyEvent;
 *		SendReferenceOrPointerToSomeOtherThread(&MyEvent); // Other thread calls MyEvent->Trigger();
 *		// MyEvent destructor is here, we wait here.
 * }
 */
class FScopedEvent
{
public:

	/** Default constructor. */
	CORE_API FScopedEvent();

	/** Destructor. */
	CORE_API ~FScopedEvent();

	/** Triggers the event. */
	void Trigger()
	{
		Event->Trigger();
	}

	/**
	 * Retrieve the event, usually for passing around.
	 *
	 * @return The event.
	 */
	FEvent* Get()
	{
		return Event;
	}

private:

	/** Holds the event. */
	FEvent* Event;
};


/**
 * Interface for "runnable" objects.
 *
 * A runnable object is an object that is "run" on an arbitrary thread. The call usage pattern is
 * Init(), Run(), Exit(). The thread that is going to "run" this object always uses those calling
 * semantics. It does this on the thread that is created so that any thread specific uses (TLS, etc.)
 * are available in the contexts of those calls. A "runnable" does all initialization in Init().
 *
 * If initialization fails, the thread stops execution and returns an error code. If it succeeds,
 * Run() is called where the real threaded work is done. Upon completion, Exit() is called to allow
 * correct clean up.
 */
class CORE_API FRunnable
{
public:

	/**
	 * Initializes the runnable object.
	 *
	 * This method is called in the context of the thread object that aggregates this, not the
	 * thread that passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 * @see Run, Stop, Exit
	 */
	virtual bool Init()
	{
		return true;
	}

	/**
	 * Runs the runnable object.
	 *
	 * This is where all per object thread work is done. This is only called if the initialization was successful.
	 *
	 * @return The exit code of the runnable object
	 * @see Init, Stop, Exit
	 */
	virtual uint32 Run() = 0;

	/**
	 * Stops the runnable object.
	 *
	 * This is called if a thread is requested to terminate early.
	 * @see Init, Run, Exit
	 */
	virtual void Stop() { }

	/**
	 * Exits the runnable object.
	 *
	 * Called in the context of the aggregating thread to perform any cleanup.
	 * @see Init, Run, Stop
	 */
	virtual void Exit() { }

	/**
	 * Gets single thread interface pointer used for ticking this runnable when multi-threading is disabled.
	 * If the interface is not implemented, this runnable will not be ticked when FPlatformProcess::SupportsMultithreading() is false.
	 *
	 * @return Pointer to the single thread interface or nullptr if not implemented.
	 */
	virtual class FSingleThreadRunnable* GetSingleThreadInterface( )
	{
		return nullptr;
	}

	/** Virtual destructor */
	virtual ~FRunnable() { }
};


/**
 * Interface for runnable threads.
 *
 * This interface specifies the methods used to manage a thread's life cycle.
 */
class CORE_API FRunnableThread
{
	friend class FThreadSingletonInitializer;
	friend class FTlsAutoCleanup;
	friend class FThreadManager;

	/** Index of TLS slot for FRunnableThread pointer. */
	static uint32 RunnableTlsSlot;

public:

	/** Gets a new Tls slot for storing the runnable thread pointer. */
	static uint32 GetTlsSlot();

	/**
	* Factory method to create a thread with the specified stack size and thread priority.
	*
	* @param InRunnable The runnable object to execute
	* @param ThreadName Name of the thread
	* @param bAutoDeleteSelf Whether to delete this object on exit
	* @param bAutoDeleteRunnable Whether to delete the runnable object on exit
	* @param InStackSize The size of the stack to create. 0 means use the current thread's stack size
	* @param InThreadPri Tells the thread whether it needs to adjust its priority or not. Defaults to normal priority
	* @return The newly created thread or nullptr if it failed
	*/
	DEPRECATED(4.3, "Function deprecated. Use FRunnableThread::Create without bAutoDeleteSelf and bAutoDeleteRunnable params and delete thread and runnable manually.")
	static FRunnableThread* Create(
		class FRunnable* InRunnable,
		const TCHAR* ThreadName,
		bool bAutoDeleteSelf,
		bool bAutoDeleteRunnable = false,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal,
		uint64 InThreadAffinityMask = 0);

	/**
	 * Factory method to create a thread with the specified stack size and thread priority.
	 *
	 * @param InRunnable The runnable object to execute
	 * @param ThreadName Name of the thread
	 * @param InStackSize The size of the stack to create. 0 means use the current thread's stack size
	 * @param InThreadPri Tells the thread whether it needs to adjust its priority or not. Defaults to normal priority
	 * @return The newly created thread or nullptr if it failed
	 */
	static FRunnableThread* Create(
		class FRunnable* InRunnable,
		const TCHAR* ThreadName,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal,
		uint64 InThreadAffinityMask = FPlatformAffinity::GetNoAffinityMask() );

	/**
	 * Changes the thread priority of the currently running thread
	 *
	 * @param NewPriority The thread priority to change to
	 */
	virtual void SetThreadPriority( EThreadPriority NewPriority ) = 0;

	/**
	 * Tells the thread to either pause execution or resume depending on the
	 * passed in value.
	 *
	 * @param bShouldPause Whether to pause the thread (true) or resume (false)
	 */
	virtual void Suspend( bool bShouldPause = true ) = 0;

	/**
	 * Tells the thread to exit. If the caller needs to know when the thread has exited, it should use the bShouldWait value.
	 * It's highly recommended not to kill the thread without waiting for it.
	 * Having a thread forcibly destroyed can cause leaks and deadlocks.
	 * 
	 * The kill method is calling Stop() on the runnable to kill the thread gracefully.
	 * 
	 * @param bShouldWait	If true, the call will wait infinitely for the thread to exit.					
	 * @return Always true
	 */
	virtual bool Kill( bool bShouldWait = true ) = 0;

	/** Halts the caller until this thread is has completed its work. */
	virtual void WaitForCompletion() = 0;

	/**
	 * Thread ID for this thread 
	 *
	 * @return ID that was set by CreateThread
	 * @see GetThreadName
	 */
	const uint32 GetThreadID() const
	{
		return ThreadID;
	}

	/**
	 * Retrieves the given name of the thread
	 *
	 * @return Name that was set by CreateThread
	 * @see GetThreadID
	 */
	const FString& GetThreadName() const
	{
		return ThreadName;
	}

	/** Default constructor. */
	FRunnableThread();

	/** Virtual destructor */
	virtual ~FRunnableThread();

protected:

	/**
	 * Creates the thread with the specified stack size and thread priority.
	 *
	 * @param InRunnable The runnable object to execute
	 * @param ThreadName Name of the thread
	 * @param InStackSize The size of the stack to create. 0 means use the current thread's stack size
	 * @param InThreadPri Tells the thread whether it needs to adjust its priority or not. Defaults to normal priority
	 * @return True if the thread and all of its initialization was successful, false otherwise
	 */
	virtual bool CreateInternal( FRunnable* InRunnable, const TCHAR* InThreadName,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0 ) = 0;

	/** Stores this instance in the runnable thread TLS slot. */
	void SetTls();

	/** Deletes all FTlsAutoCleanup objects created for this thread. */
	void FreeTls();

	/**
	 * @return a runnable thread that is executing this runnable, if return value is nullptr, it means the running thread can be game thread or a thread created outside the runnable interface
	 */
	static FRunnableThread* GetRunnableThread()
	{
		FRunnableThread* RunnableThread = (FRunnableThread*)FPlatformTLS::GetTlsValue( RunnableTlsSlot );
		return RunnableThread;
	}

	/** Holds the name of the thread. */
	FString ThreadName;

	/** The runnable object to execute on this thread. */
	FRunnable* Runnable;

	/** Sync event to make sure that Init() has been completed before allowing the main thread to continue. */
	FEvent* ThreadInitSyncEvent;

	/** The Affinity to run the thread with. */
	uint64 ThreadAffinityMask;

	/** An array of FTlsAutoCleanup based instances that needs to be deleted before the thread will die. */
	TArray<FTlsAutoCleanup*> TlsInstances;

	/** The priority to run the thread at. */
	EThreadPriority ThreadPriority;

	/** ID set during thread creation. */
	uint32 ThreadID;

private:

	/** Used by the thread manager to tick threads in single-threaded mode */
	virtual void Tick() {}
};


/**
 * Fake event object used when running with only one thread.
 */
class FSingleThreadEvent
	: public FEvent
{
	/** Flag to know whether this event has been triggered. */
	bool bTriggered;

	/** Should this event reset automatically or not. */
	bool bManualReset;

public:

	/** Default constructor. */
	FSingleThreadEvent()
		: bTriggered(false)
		, bManualReset(false)
	{ }

public:

	// FEvent Interface

	virtual bool Create( bool bIsManualReset = false ) override
	{ 
		bManualReset = bIsManualReset;
		return true; 
	}

	virtual bool IsManualReset() override
	{
		return bManualReset;
	}

	virtual void Trigger() override
	{
		bTriggered = true;
	}

	virtual void Reset() override
	{
		bTriggered = false;
	}

	virtual bool Wait( uint32 WaitTime, const bool bIgnoreThreadIdleStats = false ) override
	{ 
		// With only one thread it's assumed the event has been triggered
		// before Wait is called, otherwise it would end up waiting forever or always fail.
		check(bTriggered);
		bTriggered = bManualReset;
		return true; 
	}
};


/** 
 * Interface for ticking runnables when there's only one thread available and 
 * multithreading is disabled.
 */
class CORE_API FSingleThreadRunnable
{
public:

	virtual ~FSingleThreadRunnable() { }

	/* Tick function. */
	virtual void Tick() = 0;
};


/**
 * Manages runnables and runnable threads.
 */
class CORE_API FThreadManager
{
	/** List of thread objects to be ticked. */
	TMap<uint32, class FRunnableThread*> Threads;
	/** Critical section for ThreadList */
	FCriticalSection ThreadsCritical;

public:

	/**
	* Used internally to add a new thread object.
	*
	* @param Thread thread object.
	* @see RemoveThread
	*/
	void AddThread(uint32 ThreadId, class FRunnableThread* Thread);

	/**
	* Used internally to remove thread object.
	*
	* @param Thread thread object to be removed.
	* @see AddThread
	*/
	void RemoveThread(class FRunnableThread* Thread);

	/** Ticks all fake threads and their runnable objects. */
	void Tick();

	/** Returns the name of a thread given its TLS id */
	const FString& GetThreadName(uint32 ThreadId);

	/**
	 * Access to the singleton object.
	 *
	 * @return Thread manager object.
	 */
	static FThreadManager& Get();
};


/**
 * Interface for queued work objects.
 *
 * This interface is a type of runnable object that requires no per thread
 * initialization. It is meant to be used with pools of threads in an
 * abstract way that prevents the pool from needing to know any details
 * about the object being run. This allows queuing of disparate tasks and
 * servicing those tasks with a generic thread pool.
 */
class IQueuedWork
{
public:

	/**
	 * This is where the real thread work is done. All work that is done for
	 * this queued object should be done from within the call to this function.
	 */
	virtual void DoThreadedWork() = 0;

	/**
	 * Tells the queued work that it is being abandoned so that it can do
	 * per object clean up as needed. This will only be called if it is being
	 * abandoned before completion. NOTE: This requires the object to delete
	 * itself using whatever heap it was allocated in.
	 */
	virtual void Abandon() = 0;

public:

	/**
	 * Virtual destructor so that child implementations are guaranteed a chance
	 * to clean up any resources they allocated.
	 */
	virtual ~IQueuedWork() { }
};


DEPRECATED(4.8, "FQueuedWork has been renamed to IQueuedWork")
typedef IQueuedWork FQueuedWork;


/**
 * Interface for queued thread pools.
 *
 * This interface is used by all queued thread pools. It used as a callback by
 * FQueuedThreads and is used to queue asynchronous work for callers.
 */
class CORE_API FQueuedThreadPool
{
public:

	/**
	 * Creates the thread pool with the specified number of threads
	 *
	 * @param InNumQueuedThreads Specifies the number of threads to use in the pool
	 * @param StackSize The size of stack the threads in the pool need (32K default)
	 * @param ThreadPriority priority of new pool thread
	 * @return Whether the pool creation was successful or not
	 */
	virtual bool Create( uint32 InNumQueuedThreads, uint32 StackSize = (32 * 1024), EThreadPriority ThreadPriority=TPri_Normal ) = 0;

	/** Tells the pool to clean up all background threads */
	virtual void Destroy() = 0;

	/**
	 * Checks to see if there is a thread available to perform the task. If not,
	 * it queues the work for later. Otherwise it is immediately dispatched.
	 *
	 * @param InQueuedWork The work that needs to be done asynchronously
	 * @see RetractQueuedWork
	 */
	virtual void AddQueuedWork( IQueuedWork* InQueuedWork ) = 0;

	/**
	 * Attempts to retract a previously queued task.
	 *
	 * @param InQueuedWork The work to try to retract
	 * @return true if the work was retracted
	 * @see AddQueuedWork
	 */
	virtual bool RetractQueuedWork( IQueuedWork* InQueuedWork ) = 0;

	/**
	 * Places a thread back into the available pool
	 *
	 * @param InQueuedThread The thread that is ready to be pooled
	 * @return next job or null if there is no job available now
	 */
	virtual IQueuedWork* ReturnToPoolOrGetNextJob( class FQueuedThread* InQueuedThread ) = 0;

public:

	/** Virtual destructor. */
	virtual ~FQueuedThreadPool() { }

public:

	/**
	 * Allocates a thread pool
	 *
	 * @return A new thread pool.
	 */
	static FQueuedThreadPool* Allocate();

	/**
	 *	Stack size for threads created for the thread pool. 
	 *	Can be overridden by other projects.
	 *	If 0 means to use the value passed in the Create method.
	 */
	static uint32 OverrideStackSize;
};


/**
 *  Global thread pool for shared async operations
 */
extern CORE_API FQueuedThreadPool* GThreadPool;

#if USE_NEW_ASYNC_IO
extern CORE_API FQueuedThreadPool* GIOThreadPool;
#endif // USE_NEW_ASYNC_IO

#if WITH_EDITOR
extern CORE_API FQueuedThreadPool* GLargeThreadPool;
#endif


/** Thread safe counter */
class FThreadSafeCounter
{
public:
	typedef int32 IntegerType;

	/**
	 * Default constructor.
	 *
	 * Initializes the counter to 0.
	 */
	FThreadSafeCounter()
	{
		Counter = 0;
	}

	/**
	 * Copy Constructor.
	 *
	 * If the counter in the Other parameter is changing from other threads, there are no
	 * guarantees as to which values you will get up to the caller to not care, synchronize
	 * or other way to make those guarantees.
	 *
	 * @param Other The other thread safe counter to copy
	 */
	FThreadSafeCounter( const FThreadSafeCounter& Other )
	{
		Counter = Other.GetValue();
	}

	/**
	 * Constructor, initializing counter to passed in value.
	 *
	 * @param Value	Value to initialize counter to
	 */
	FThreadSafeCounter( int32 Value )
	{
		Counter = Value;
	}

	/**
	 * Increment and return new value.	
	 *
	 * @return the new, incremented value
	 * @see Add, Decrement, Reset, Set, Subtract
	 */
	int32 Increment()
	{
		return FPlatformAtomics::InterlockedIncrement(&Counter);
	}

	/**
	 * Adds an amount and returns the old value.	
	 *
	 * @param Amount Amount to increase the counter by
	 * @return the old value
	 * @see Decrement, Increment, Reset, Set, Subtract
	 */
	int32 Add( int32 Amount )
	{
		return FPlatformAtomics::InterlockedAdd(&Counter, Amount);
	}

	/**
	 * Decrement and return new value.
	 *
	 * @return the new, decremented value
	 * @see Add, Increment, Reset, Set, Subtract
	 */
	int32 Decrement()
	{
		return FPlatformAtomics::InterlockedDecrement(&Counter);
	}

	/**
	 * Subtracts an amount and returns the old value.	
	 *
	 * @param Amount Amount to decrease the counter by
	 * @return the old value
	 * @see Add, Decrement, Increment, Reset, Set
	 */
	int32 Subtract( int32 Amount )
	{
		return FPlatformAtomics::InterlockedAdd(&Counter, -Amount);
	}

	/**
	 * Sets the counter to a specific value and returns the old value.
	 *
	 * @param Value	Value to set the counter to
	 * @return The old value
	 * @see Add, Decrement, Increment, Reset, Subtract
	 */
	int32 Set( int32 Value )
	{
		return FPlatformAtomics::InterlockedExchange(&Counter, Value);
	}

	/**
	 * Resets the counter's value to zero.
	 *
	 * @return the old value.
	 * @see Add, Decrement, Increment, Set, Subtract
	 */
	int32 Reset()
	{
		return FPlatformAtomics::InterlockedExchange(&Counter, 0);
	}

	/**
	 * Gets the current value.
	 *
	 * @return the current value
	 */
	int32 GetValue() const
	{
		return Counter;
	}

private:

	/** Hidden on purpose as usage wouldn't be thread safe. */
	void operator=( const FThreadSafeCounter& Other ){}

	/** Thread-safe counter */
	volatile int32 Counter;
};

// This class cannot be implemented on platforms that don't define 64bit atomic functions
#if PLATFORM_HAS_64BIT_ATOMICS
/** Thread safe counter for 64bit ints */
class FThreadSafeCounter64
{
public:
	typedef int64 IntegerType;

	/**
	* Default constructor.
	*
	* Initializes the counter to 0.
	*/
	FThreadSafeCounter64()
	{
		Counter = 0;
	}

	/**
	* Copy Constructor.
	*
	* If the counter in the Other parameter is changing from other threads, there are no
	* guarantees as to which values you will get up to the caller to not care, synchronize
	* or other way to make those guarantees.
	*
	* @param Other The other thread safe counter to copy
	*/
	FThreadSafeCounter64(const FThreadSafeCounter& Other)
	{
		Counter = Other.GetValue();
	}

	/** Assignment has the same caveats as the copy ctor. */
	FThreadSafeCounter64& operator=(const FThreadSafeCounter64& Other)
	{
		if (this == &Other) return *this;
		Set(Other.GetValue());
		return *this;
	}


	/**
	* Constructor, initializing counter to passed in value.
	*
	* @param Value	Value to initialize counter to
	*/
	FThreadSafeCounter64(int64 Value)
	{
		Counter = Value;
	}

	/**
	* Increment and return new value.
	*
	* @return the new, incremented value
	* @see Add, Decrement, Reset, Set, Subtract
	*/
	int64 Increment()
	{
		return FPlatformAtomics::InterlockedIncrement(&Counter);
	}

	/**
	* Adds an amount and returns the old value.
	*
	* @param Amount Amount to increase the counter by
	* @return the old value
	* @see Decrement, Increment, Reset, Set, Subtract
	*/
	int64 Add(int64 Amount)
	{
		return FPlatformAtomics::InterlockedAdd(&Counter, Amount);
	}

	/**
	* Decrement and return new value.
	*
	* @return the new, decremented value
	* @see Add, Increment, Reset, Set, Subtract
	*/
	int64 Decrement()
	{
		return FPlatformAtomics::InterlockedDecrement(&Counter);
	}

	/**
	* Subtracts an amount and returns the old value.
	*
	* @param Amount Amount to decrease the counter by
	* @return the old value
	* @see Add, Decrement, Increment, Reset, Set
	*/
	int64 Subtract(int64 Amount)
	{
		return FPlatformAtomics::InterlockedAdd(&Counter, -Amount);
	}

	/**
	* Sets the counter to a specific value and returns the old value.
	*
	* @param Value	Value to set the counter to
	* @return The old value
	* @see Add, Decrement, Increment, Reset, Subtract
	*/
	int64 Set(int64 Value)
	{
		return FPlatformAtomics::InterlockedExchange(&Counter, Value);
	}

	/**
	* Resets the counter's value to zero.
	*
	* @return the old value.
	* @see Add, Decrement, Increment, Set, Subtract
	*/
	int64 Reset()
	{
		return FPlatformAtomics::InterlockedExchange(&Counter, 0);
	}

	/**
	* Gets the current value.
	*
	* @return the current value
	*/
	int64 GetValue() const
	{
		return Counter;
	}

private:
	/** Thread-safe counter */
	volatile int64 Counter;
};
#endif

/**
 * Thread safe bool, wraps FThreadSafeCounter
 */
class FThreadSafeBool
	: private FThreadSafeCounter
{
public:
	/**
	 * Constructor optionally takes value to initialize with, otherwise initializes false
	 * @param bValue	Value to initialize with
	 */
	FThreadSafeBool(bool bValue = false)
		: FThreadSafeCounter(bValue ? 1 : 0)
	{}

	/**
	 * Operator to use this struct as a bool with thread safety
	 */
	FORCEINLINE operator bool() const
	{
		return GetValue() != 0;
	}

	/**
	 * Operator to set the bool value with thread safety
	 */
	FORCEINLINE bool operator=(bool bNewValue)
	{
		Set(bNewValue ? 1 : 0);
		return bNewValue;
	}

	/**
	 * Sets a new value atomically, and returns the old value.
	 *
	 * @param bNewValue   Value to set
	 * @return The old value
	 */
	FORCEINLINE bool AtomicSet(bool bNewValue)
	{
		return Set(bNewValue ? 1 : 0) != 0;
	}
};


/** Fake Thread safe counter, used to avoid cluttering code with ifdefs when counters are only used for debugging. */
class FNoopCounter
{
public:

	FNoopCounter() { }
	FNoopCounter( const FNoopCounter& Other ) { }
	FNoopCounter( int32 Value ) { }

	int32 Increment()
	{
		return 0;
	}

	int32 Add( int32 Amount )
	{
		return 0;
	}

	int32 Decrement()
	{
		return 0;
	}

	int32 Subtract( int32 Amount )
	{
		return 0;
	}

	int32 Set( int32 Value )
	{
		return 0;
	}

	int32 Reset()
	{
		return 0;
	}

	int32 GetValue() const
	{
		return 0;
	}
};

/**
 * Object synchronizing read access (from any thread) and write access (only from game thread).
 * Multiple simultaneous reads from different threads are allowed. Allows multiple writes from
 * reentrant calls on main thread. Locking write doesn't prevent reads from main thread.
 **/
class FMultiReaderSingleWriterGT
{
public:
	/** Protect data from modification while reading. If issued on game thread, doesn't wait for write to finish. */
	void LockRead();

	/** Ends protecting data from modification while reading. */
	void UnlockRead();

	/** Protect data from reading while modifying. Can be called only on game thread. Reentrant. */
	void LockWrite();

	/** Ends protecting data from reading while modifying. */
	void UnlockWrite();

private:
	struct FMRSWCriticalSection
	{
		FMRSWCriticalSection()
			: Action(0)
			, ReadCounter(0)
			, WriteCounter(0)
		{ }

		int32 Action;
		FThreadSafeCounter ReadCounter;
		FThreadSafeCounter WriteCounter;
	};

	FMRSWCriticalSection CriticalSection;

	static const int32 WritingAction = -1;
	static const int32 NoAction = 0;
	static const int32 ReadingAction = 1;

	bool CanRead()
	{
		return FPlatformAtomics::InterlockedCompareExchange(&CriticalSection.Action, ReadingAction, NoAction) == ReadingAction;
	}
	
	bool CanWrite()
	{
		return FPlatformAtomics::InterlockedCompareExchange(&CriticalSection.Action, WritingAction, NoAction) == WritingAction;
	}
};

class FReadScopeLock
{
public:

	/**
	* Constructor that performs a lock on the synchronization object
	*
	* @param InSynchObject The synchronization object to manage
	*/
	FReadScopeLock(FMultiReaderSingleWriterGT* InSynchObject)
		: SynchObject(InSynchObject)
	{
		check(SynchObject);
		SynchObject->LockRead();
	}

	/** Destructor that performs a release on the synchronization object. */
	~FReadScopeLock()
	{
		check(SynchObject);
		SynchObject->UnlockRead();
	}
private:

	/** Default constructor (hidden on purpose). */
	FReadScopeLock();

	/** Copy constructor( hidden on purpose). */
	FReadScopeLock(FReadScopeLock* InScopeLock);

	/** Assignment operator (hidden on purpose). */
	FReadScopeLock& operator=(FReadScopeLock& InScopeLock)
	{
		return *this;
	}

private:

	// Holds the synchronization object to aggregate and scope manage.
	FMultiReaderSingleWriterGT* SynchObject;
};

class FWriteScopeLock
{
public:

	/**
	* Constructor that performs a lock on the synchronization object
	*
	* @param InSynchObject The synchronization object to manage
	*/
	FWriteScopeLock(FMultiReaderSingleWriterGT* InSynchObject)
		: SynchObject(InSynchObject)
	{
		check(SynchObject);
		SynchObject->LockWrite();
	}

	/** Destructor that performs a release on the synchronization object. */
	~FWriteScopeLock()
	{
		check(SynchObject);
		SynchObject->UnlockWrite();
	}
private:

	/** Default constructor (hidden on purpose). */
	FWriteScopeLock();

	/** Copy constructor( hidden on purpose). */
	FWriteScopeLock(FWriteScopeLock* InScopeLock);

	/** Assignment operator (hidden on purpose). */
	FWriteScopeLock& operator=(FWriteScopeLock& InScopeLock)
	{
		return *this;
	}

private:

	// Holds the synchronization object to aggregate and scope manage.
	FMultiReaderSingleWriterGT* SynchObject;
};

/**
 * Implements a scope lock.
 *
 * This is a utility class that handles scope level locking. It's very useful
 * to keep from causing deadlocks due to exceptions being caught and knowing
 * about the number of locks a given thread has on a resource. Example:
 *
 * <code>
 *	{
 *		// Synchronize thread access to the following data
 *		FScopeLock ScopeLock(SynchObject);
 *		// Access data that is shared among multiple threads
 *		...
 *		// When ScopeLock goes out of scope, other threads can access data
 *	}
 * </code>
 */
class FScopeLock
{
public:

	/**
	 * Constructor that performs a lock on the synchronization object
	 *
	 * @param InSynchObject The synchronization object to manage
	 */
	FScopeLock( FCriticalSection* InSynchObject )
		: SynchObject(InSynchObject)
	{
		check(SynchObject);
		SynchObject->Lock();
	}

	/** Destructor that performs a release on the synchronization object. */
	~FScopeLock()
	{
		check(SynchObject);
		SynchObject->Unlock();
	}
private:

	/** Default constructor (hidden on purpose). */
	FScopeLock();

	/** Copy constructor( hidden on purpose). */
	FScopeLock(const FScopeLock& InScopeLock);

	/** Assignment operator (hidden on purpose). */
	FScopeLock& operator=( FScopeLock& InScopeLock )
	{
		return *this;
	}

private:

	// Holds the synchronization object to aggregate and scope manage.
	FCriticalSection* SynchObject;
};


/** @return True if called from the game thread. */
FORCEINLINE bool IsInGameThread()
{
	if(GIsGameThreadIdInitialized)
	{
		const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		return CurrentThreadId == GGameThreadId || CurrentThreadId == GSlateLoadingThreadId;
	}

	return true;
}

/** @return True if called from the audio thread, and not merely a thread calling audio functions. */
extern CORE_API bool IsInAudioThread();

/** Thread used for audio */
extern CORE_API FRunnableThread* GAudioThread;

/** @return True if called from the slate thread, and not merely a thread calling slate functions. */
extern CORE_API bool IsInSlateThread();

/** @return True if called from the rendering thread, or if called from ANY thread during single threaded rendering */
extern CORE_API bool IsInRenderingThread();

/** @return True if called from the rendering thread, or if called from ANY thread that isn't the game thread, except that during single threaded rendering the game thread is ok too.*/
extern CORE_API bool IsInParallelRenderingThread();

/** @return True if called from the rendering thread. */
// Unlike IsInRenderingThread, this will always return false if we are running single threaded. It only returns true if this is actually a separate rendering thread. Mostly useful for checks
extern CORE_API bool IsInActualRenderingThread();

/** @return True if called from the async loading thread if it's enabled, otherwise if called from game thread while is async loading code. */
extern CORE_API bool (*IsInAsyncLoadingThread)();

/** Thread used for rendering */
extern CORE_API FRunnableThread* GRenderingThread;

/** Whether the rendering thread is suspended (not even processing the tickables) */
extern CORE_API int32 GIsRenderingThreadSuspended;

/** @return True if called from the RHI thread, or if called from ANY thread during single threaded rendering */
extern CORE_API bool IsInRHIThread();

/** Thread used for RHI */
extern CORE_API FRunnableThread* GRHIThread;

/** Base class for objects in TLS that support auto-cleanup. */
class CORE_API FTlsAutoCleanup
{
public:
	/** Virtual destructor. */
	virtual ~FTlsAutoCleanup()
	{}

	/** Register this instance to be auto-cleanup. */
	void Register();
};

/** Wrapper for values to be stored in TLS that support auto-cleanup. */
template< class T >
class TTlsAutoCleanupValue
	: public FTlsAutoCleanup
{
public:

	/** Constructor. */
	TTlsAutoCleanupValue(const T& InValue)
		: Value(InValue)
	{ }

	/** Gets the value. */
	T Get() const
	{
		return Value;
	}

private:

	/** The value. */
	T Value;
};

/**
 * Thread singleton initializer.
 */
class FThreadSingletonInitializer
{
public:

	/**
	* @return an instance of a singleton for the current thread.
	*/
	static CORE_API FTlsAutoCleanup* Get( TFunctionRef<FTlsAutoCleanup*()> CreateInstance, uint32& TlsSlot );
};


/**
 * This a special version of singleton. It means that there is created only one instance for each thread.
 * Calling Get() method is thread-safe.
 */
template < class T >
class TThreadSingleton : public FTlsAutoCleanup
{
	/**
	 * @return TLS slot that holds a TThreadSingleton.
	 */
	static uint32& GetTlsSlot()
	{
		static uint32 TlsSlot = 0;
		return TlsSlot;
	}

protected:

	/** Default constructor. */
	TThreadSingleton()
		: ThreadId(FPlatformTLS::GetCurrentThreadId())
	{}

	/**
	 * @return a new instance of the thread singleton.
	 */
	static FTlsAutoCleanup* CreateInstance()
	{
		return new T();
	}

	/** Thread ID of this thread singleton. */
	const uint32 ThreadId;

public:

	/**
	 *	@return an instance of a singleton for the current thread.
	 */
	FORCEINLINE static T& Get()
	{
		return *(T*)FThreadSingletonInitializer::Get( [](){ return (FTlsAutoCleanup*)new T(); }, T::GetTlsSlot() ); //-V572
	}
};

#define DECLARE_THREAD_SINGLETON(ClassType) \
	EMIT_DEPRECATED_WARNING_MESSAGE("DECLARE_THREAD_SINGLETON is deprecated in 4.8. It's no longer needed and can be removed.")