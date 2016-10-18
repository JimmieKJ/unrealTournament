// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncLoadingThread.h: Unreal async loading code.
=============================================================================*/
#pragma once

class IAssetRegistryInterface;

/**
 * Async loading thread. Preloads/serializes packages on async loading thread. Postloads objects on the game thread.
 */
class FAsyncLoadingThread : public FRunnable
{
	friend class FArchiveAsync;

	/** Thread to run the worker FRunnable on */
	FRunnableThread* Thread;
	/** Stops this thread */
	FThreadSafeCounter StopTaskCounter;

	/** [ASYNC/GAME THREAD] Event used to signal there's queued packages to stream */
	FEvent* QueuedRequestsEvent;
	/** [ASYNC/GAME THREAD] Event used to signal loading should be cancelled */
	FEvent* CancelLoadingEvent;
	/** [ASYNC/GAME THREAD] Event used to signal that the async loading thread should be suspended */
	FEvent* ThreadSuspendedEvent;
	/** [ASYNC/GAME THREAD] Event used to signal that the async loading thread has resumed */
	FEvent* ThreadResumedEvent;
	/** [ASYNC/GAME THREAD] List of queued packages to stream */
	TArray<FAsyncPackageDesc*> QueuedPackages;
#if THREADSAFE_UOBJECTS
	/** [ASYNC/GAME THREAD] Package queue critical section */
	FCriticalSection QueueCritical;
#endif
	/** [ASYNC/GAME THREAD] True if the async loading thread received a request to cancel async loading **/
	FThreadSafeBool bShouldCancelLoading;
	/** [ASYNC/GAME THREAD] True if the async loading thread received a request to suspend **/
	FThreadSafeCounter IsLoadingSuspended;
	/** [ASYNC/GAME THREAD] Event used to signal there's queued packages to stream */
	TArray<FAsyncPackage*> LoadedPackages;	
	TMap<FName, FAsyncPackage*> LoadedPackagesNameLookup;
#if THREADSAFE_UOBJECTS
	/** [ASYNC/GAME THREAD] Critical section for LoadedPackages list */
	FCriticalSection LoadedPackagesCritical;
#endif
	/** [GAME THREAD] Event used to signal there's queued packages to stream */
	TArray<FAsyncPackage*> LoadedPackagesToProcess;
	TArray<FAsyncPackage*> PackagesToDelete;
	TMap<FName, FAsyncPackage*> LoadedPackagesToProcessNameLookup;
#if THREADSAFE_UOBJECTS
	/** [ASYNC/GAME THREAD] Critical section for LoadedPackagesToProcess list. 
	 * Note this is only required for looking up existing packages on the async loading thread 
	 */
	FCriticalSection LoadedPackagesToProcessCritical;
#endif

	/** [ASYNC THREAD] Array of packages that are being preloaded */
	TArray<FAsyncPackage*> AsyncPackages;

	/** Packages that we've already hinted for reading */
	TSet<FAsyncPackage*> PendingPackageHints;

	TMap<FName, FAsyncPackage*> AsyncPackageNameLookup;
#if THREADSAFE_UOBJECTS
	/** We only lock AsyncPackages array to make GetAsyncLoadPercentage thread safe, so we only care about locking Add/Remove operations on the async thread */
	FCriticalSection AsyncPackagesCritical;
#endif

	/** List of all pending package requests */
	TSet<int32> PendingRequests;
#if THREADSAFE_UOBJECTS
	/** Synchronization object for PendingRequests list */
	FCriticalSection PendingRequestsCritical;
#endif

	/** [ASYNC/GAME THREAD] Number of package load requests in the async loading queue */
	FThreadSafeCounter QueuedPackagesCounter;
	/** [ASYNC/GAME THREAD] Number of packages being loaded on the async thread and post loaded on the game thread */
	FThreadSafeCounter ExistingAsyncPackagesCounter;

	FThreadSafeCounter AsyncThreadReady;

	/** Async loading thread ID */
	static uint32 AsyncLoadingThreadID;

	/** Enum describing async package request insertion mode */
	enum class EAsyncPackageInsertMode
	{
		InsertBeforeMatchingPriorities,	// Insert this package before all other packages of the same priority
		InsertAfterMatchingPriorities	// Insert this package after all other packages of the same priority
	};

#if LOOKING_FOR_PERF_ISSUES
	/** Thread safe counter used to accumulate cycles spent on blocking. Using stats may generate to many stats messages. */
	static FThreadSafeCounter BlockingCycles;
#endif

	FAsyncLoadingThread();
	virtual ~FAsyncLoadingThread();

public:

	//~ Begin FRunnable Interface.
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	//~ End FRunnable Interface

	/** Returns the async loading thread singleton */
	static FAsyncLoadingThread& Get();

	/** True if multithreaded async loading should be used. */
	static FORCEINLINE bool IsMultithreaded()
	{
		static struct FAsyncLoadingThreadEnabled
		{
			bool Value;
			FAsyncLoadingThreadEnabled()
			{
#if THREADSAFE_UOBJECTS
				if (FPlatformProperties::RequiresCookedData())
				{
					check(GConfig);
					bool bConfigValue = true;
					GConfig->GetBool(TEXT("/Script/Engine.StreamingSettings"), TEXT("s.AsyncLoadingThreadEnabled"), bConfigValue, GEngineIni);
					bool bCommandLineNoAsyncThread = false;
					Value = bConfigValue && FApp::ShouldUseThreadingForPerformance() && !FParse::Param(FCommandLine::Get(), TEXT("NoAsyncLoadingThread"));
				}
				else
#endif
				{
					Value = false;
				}
			}
		} AsyncLoadingThreadEnabled;
		return AsyncLoadingThreadEnabled.Value;
	}

	/** Sets the current state of async loading */
	FORCEINLINE void SetIsInAsyncLoadingTick(bool InTick)
	{
		bIsInAsyncLoadingTick = InTick;
	}

	/** Gets the current state of async loading */
	FORCEINLINE bool GetIsInAsyncLoadingTick() const
	{
		return bIsInAsyncLoadingTick;
	}

	/** Returns true if packages are currently being loaded on the async thread */
	FORCEINLINE bool IsAsyncLoadingPackages()
	{
		FPlatformMisc::MemoryBarrier();
		return QueuedPackagesCounter.GetValue() != 0 || ExistingAsyncPackagesCounter.GetValue() != 0;
	}

	/** Returns true this codes runs on the async loading thread */
	static FORCEINLINE bool IsInAsyncLoadThread()
	{
		bool bResult = false;
		if (IsMultithreaded())
		{
			// We still need to report we're in async loading thread even if 
			// we're on game thread but inside of async loading code (PostLoad mostly)
			// to make it behave exactly like the non-threaded version
			bResult = FPlatformTLS::GetCurrentThreadId() == AsyncLoadingThreadID || 
				(IsInGameThread() && FAsyncLoadingThread::Get().bIsInAsyncLoadingTick);
		}
		else
		{
			bResult = IsInGameThread() && FAsyncLoadingThread::Get().bIsInAsyncLoadingTick;
		}
		return bResult;
	}

	/** Returns true if async loading is suspended */
	FORCEINLINE bool IsAsyncLoadingSuspended()
	{
		FPlatformMisc::MemoryBarrier();
		return IsLoadingSuspended.GetValue() != 0;
	}

	FORCEINLINE int32 GetAsyncLoadingSuspendedCount()
	{
		FPlatformMisc::MemoryBarrier();
		return IsLoadingSuspended.GetValue();
	}

	/** Returns the number of async packages that are currently being processed */
	FORCEINLINE int32 GetAsyncPackagesCount()
	{
		FPlatformMisc::MemoryBarrier();
		return ExistingAsyncPackagesCounter.GetValue();
	}

	/**
	* [ASYNC THREAD] Finds an existing async package in the AsyncPackages by its name.
	*
	* @param PackageName async package name.
	* @return Pointer to the package or nullptr if not found
	*/
	FORCEINLINE FAsyncPackage* FindAsyncPackage(const FName& PackageName)
	{
		checkSlow(IsInAsyncLoadThread());
		return AsyncPackageNameLookup.FindRef(PackageName);
	}

	/**
	* [ASYNC THREAD] Inserts package to queue according to priority.
	*
	* @param PackageName - async package name.
	* @param InsertMode - Insert mode, describing how we insert this package into the request list
	*/
	void InsertPackage(FAsyncPackage* Package, EAsyncPackageInsertMode InsertMode = EAsyncPackageInsertMode::InsertBeforeMatchingPriorities);

	/**
	* [ASYNC THREAD] Finds an existing async package in the LoadedPackages by its name.
	*
	* @param PackageName async package name.
	* @return Index of the async package in LoadedPackages array or INDEX_NONE if not found.
	*/
	FORCEINLINE FAsyncPackage* FindLoadedPackage(const FName& PackageName)
	{
		checkSlow(IsInAsyncLoadThread());
		return LoadedPackagesNameLookup.FindRef(PackageName);
	}

	/**
	* [ASYNC/GAME THREAD] Queues a package for streaming.
	*
	* @param Package package descriptor.
	*/
	void QueuePackage(const FAsyncPackageDesc& Package);

	/**
	* [GAME THREAD] Cancels streaming
	*/
	void CancelAsyncLoading();

	/**
	* [GAME THREAD] Suspends async loading thread
	*/
	void SuspendLoading();

	/**
	* [GAME THREAD] Resumes async loading thread
	*/
	void ResumeLoading();

	/**
	* [ASYNC/GAME THREAD] Queues a package for streaming.
	*
	* @param Package package descriptor.
	*/
	FORCEINLINE FAsyncPackage* GetPackage(int32 PackageIndex)
	{
		checkSlow(IsInAsyncLoadThread());
		return AsyncPackages[PackageIndex];
	}

	/**
	* [ASYNC* THREAD] Loads all packages
	*
	* @param OutPackagesProcessed Number of packages processed in this call.
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type ProcessAsyncLoading(int32& OutPackagesProcessed, bool bUseTimeLimit = false, bool bUseFullTimeLimit = false, float TimeLimit = 0.0f);

	void JustInTimeHinting();

	/**
	* [GAME THREAD] Ticks game thread side of async loading.
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type TickAsyncLoading(bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, int32 WaitForRequestID = INDEX_NONE);

	/**
	* [ASYNC THREAD] Main thread loop
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	*/
	EAsyncPackageState::Type TickAsyncThread(bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit);

	/** Initializes async loading thread */
	void InitializeAsyncThread();

	/** 
	 * [GAME THREAD] Gets the load percentage of the specified package
	 * @param PackageName Name of the package to return async load percentage for
	 * @return Percentage (0-100) of the async package load or -1 of package has not been found
	 */
	float GetAsyncLoadPercentage(const FName& PackageName);

	/** 
	 * [ASYNC/GAME THREAD] Checks if a request ID already is added to the loading queue
	 */
	bool ContainsRequestID(int32 RequestID)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		return PendingRequests.Contains(RequestID);
	}

	/** 
	 * [ASYNC/GAME THREAD] Adds a request ID to the list of pending requests
	 */
	void AddPendingRequest(int32 RequestID)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		if (!PendingRequests.Contains(RequestID))
		{
			PendingRequests.Add(RequestID);
		}
	}

	/** 
	 * [ASYNC/GAME THREAD] Removes a request ID from the list of pending requests
	 */
	void RemovePendingRequests(TArray<int32>& RequestIDs)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		for (int32 ID : RequestIDs)
		{
			PendingRequests.Remove(ID);
		}		
	}

private:

	/**
	* [GAME THREAD] Performs game-thread specific operations on loaded packages (not-thread-safe PostLoad, callbacks)
	*
	* @param bUseTimeLimit True if time limit should be used [time-slicing].
	* @param bUseFullTimeLimit True if full time limit should be used [time-slicing].
	* @param TimeLimit Maximum amount of time that can be spent in this call [time-slicing].
	* @param WaitForRequestID If the package request ID is valid, exits as soon as the request is no longer in the qeueue.
	* @return The current state of async loading
	*/
	EAsyncPackageState::Type ProcessLoadedPackages(bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, int32 WaitForRequestID = INDEX_NONE);

	/**
	* [ASYNC THREAD] Creates async packages from the queued requests
	*/
	int32 CreateAsyncPackagesFromQueue();

	/**
	* [ASYNC THREAD] Internal helper function for processing a package load request. If dependency preloading is enabled, 
	* it will call itself recursively for all the package dependencies
	*/
	void ProcessAsyncPackageRequest(FAsyncPackageDesc* InRequest, FAsyncPackage* InRootPackage, IAssetRegistryInterface* InAssetRegistry);

	/**
	* [ASYNC THREAD] Internal helper function for updating the priorities of an existing package and all its dependencies
	*/
	void UpdateExistingPackagePriorities(FAsyncPackage* InPackage, TAsyncLoadPriority InNewPriority, IAssetRegistryInterface* InAssetRegistry);

	/**
	* [ASYNC THREAD] Finds existing async package and adds the new request's completion callback to it.
	*/
	FAsyncPackage* FindExistingPackageAndAddCompletionCallback(FAsyncPackageDesc* PackageRequest, TMap<FName, FAsyncPackage*>& PackageList);

	/**
	* [ASYNC THREAD] Adds a package to a list of packages that have finished loading on the async thread
	*/
	FORCEINLINE void AddToLoadedPackages(FAsyncPackage* Package)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesCritical);
#endif
		LoadedPackages.Add(Package);
		LoadedPackagesNameLookup.Add(Package->GetPackageName(), Package);
	}

	/** Cancels async loading internally */
	void CancelAsyncLoadingInternal();

	/** True if async loading is currently being ticked, mostly used by singlethreaded ticking */
	bool bIsInAsyncLoadingTick;
};
