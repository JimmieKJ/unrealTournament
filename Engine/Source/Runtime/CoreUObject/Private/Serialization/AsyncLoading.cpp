// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code.
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "Serialization/AsyncLoading.h"
#include "Serialization/DeferredMessageLog.h"
#include "Serialization/AsyncPackage.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/LinkerManager.h"
#include "Serialization/AsyncLoadingThread.h"
#include "ExclusiveLoadPackageTimeTracker.h"
#include "AssetRegistryInterface.h"
#include "BlueprintSupport.h"
#include "LoadTimeTracker.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/ExceptionHandling.h"
#include "AsyncLoadingPrivate.h"

#define FIND_MEMORY_STOMPS (1 && (PLATFORM_WINDOWS || PLATFORM_LINUX) && !WITH_EDITORONLY_DATA)

//#pragma clang optimize off

/*-----------------------------------------------------------------------------
	Async loading stats.
-----------------------------------------------------------------------------*/

DECLARE_MEMORY_STAT(TEXT("Streaming Memory Used"),STAT_StreamingAllocSize,STATGROUP_Memory);

DECLARE_STATS_GROUP_VERBOSE(TEXT("Async Load"), STATGROUP_AsyncLoad, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("Tick AsyncPackage"),STAT_FAsyncPackage_Tick,STATGROUP_AsyncLoad);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Tick AsyncPackage Time"), STAT_FAsyncPackage_TickTime, STATGROUP_AsyncLoad);

DECLARE_CYCLE_STAT(TEXT("CreateLinker AsyncPackage"),STAT_FAsyncPackage_CreateLinker,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("FinishLinker AsyncPackage"),STAT_FAsyncPackage_FinishLinker,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("LoadImports AsyncPackage"),STAT_FAsyncPackage_LoadImports,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("CreateImports AsyncPackage"),STAT_FAsyncPackage_CreateImports,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("FinishTextureAllocations AsyncPackage"),STAT_FAsyncPackage_FinishTextureAllocations,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("CreateMetaData AsyncPackage"),STAT_FAsyncPackage_CreateMetaData,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("CreateExports AsyncPackage"),STAT_FAsyncPackage_CreateExports,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("FreeReferencedImports AsyncPackage"), STAT_FAsyncPackage_FreeReferencedImports, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("Precache ArchiveAsync"), STAT_FArchiveAsync_Precache, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("PreLoadObjects AsyncPackage"),STAT_FAsyncPackage_PreLoadObjects,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("PostLoadObjects AsyncPackage"),STAT_FAsyncPackage_PostLoadObjects,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("FinishObjects AsyncPackage"),STAT_FAsyncPackage_FinishObjects,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("CreateAsyncPackagesFromQueue"), STAT_FAsyncPackage_CreateAsyncPackagesFromQueue, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("ProcessAsyncLoading AsyncLoadingThread"), STAT_FAsyncLoadingThread_ProcessAsyncLoading, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("Async Loading Time"),STAT_AsyncLoadingTime,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("Async Loading Time Detailed"), STAT_AsyncLoadingTimeDetailed, STATGROUP_AsyncLoad);

DECLARE_STATS_GROUP(TEXT("Async Load Game Thread"), STATGROUP_AsyncLoadGameThread, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("PostLoadObjects GT"), STAT_FAsyncPackage_PostLoadObjectsGameThread, STATGROUP_AsyncLoadGameThread);
DECLARE_CYCLE_STAT(TEXT("TickAsyncLoading GT"), STAT_FAsyncPackage_TickAsyncLoadingGameThread, STATGROUP_AsyncLoadGameThread);
DECLARE_CYCLE_STAT(TEXT("Flush Async Loading GT"), STAT_FAsyncPackage_FlushAsyncLoadingGameThread, STATGROUP_AsyncLoadGameThread);

DECLARE_FLOAT_ACCUMULATOR_STAT( TEXT( "Async loading block time" ), STAT_AsyncIO_AsyncLoadingBlockingTime, STATGROUP_AsyncIO );
DECLARE_FLOAT_ACCUMULATOR_STAT( TEXT( "Async package precache wait time" ), STAT_AsyncIO_AsyncPackagePrecacheWaitTime, STATGROUP_AsyncIO );

/** Returns true if we're inside a FGCScopeLock */
bool IsGarbageCollectionLocked();

/** Global request ID counter */
static FThreadSafeCounter GPackageRequestID;

/** 
 * Keeps a reference to all objects created during async load until streaming has finished 
 *
 * ASSUMPTION: AddObject can't be called while GC is running and we don't want to lock when calling AddReferencedObjects
 */
class FAsyncObjectsReferencer : FGCObject
{
	/** Private constructor */
	FAsyncObjectsReferencer() {}

	/** List of referenced objects */
	TSet<UObject*> ReferencedObjects;
#if THREADSAFE_UOBJECTS
	/** Critical section for referenced objects list */
	FCriticalSection ReferencedObjectsCritical;
#endif

public:
#if !UE_BUILD_SHIPPING
	bool Contains(UObject* InObj)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
#endif
		return ReferencedObjects.Contains(InObj);
	}
#endif

	/** Returns the one and only instance of this object */
	static FAsyncObjectsReferencer& Get();
	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		// Note we don't lock here as we're guaranteed that AddObject can only be called from
		// within FGCScopeGuard scope where GC does not run
		Collector.AllowEliminatingReferences(false);
		{
			Collector.AddReferencedObjects(ReferencedObjects);
		}
		Collector.AllowEliminatingReferences(true);
	}
	/** 
	 * Adds an object to be referenced 
	 * The assumption here is that this can only happen from inside of FGCScopeGuard (@see IsGarbageCollectionLocked()) where we're sure GC is not currently running,
	 * unless we're on the game thread where atm GC can run simultaneously with async loading.
	 */
	FORCEINLINE void AddObject(UObject* InObject)
	{
		if (InObject)
		{
			UE_CLOG(!IsInGameThread() && !IsGarbageCollectionLocked(), LogStreaming, Fatal, TEXT("Trying to add an object %s to FAsyncObjectsReferencer outside of a FGCScopeLock."), *InObject->GetFullName());
			{
#if THREADSAFE_UOBJECTS
				// Still want to lock as AddObject may be called on the game thread and async loading thread,
				// but in any case it may not happen when GC runs.
				FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
#else
				check(IsInGameThread());
#endif
				if (!ReferencedObjects.Contains(InObject))
				{
					ReferencedObjects.Add(InObject);
				}
			}
			InObject->ThisThreadAtomicallyClearedRFUnreachable();
		}
	}
	/** Removes all objects from the list and clears async loading flags */
	void EmptyReferencedObjects()
	{
		check(IsInGameThread());
#if THREADSAFE_UOBJECTS
		FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
#endif
		const EInternalObjectFlags AsyncFlags = EInternalObjectFlags::Async | EInternalObjectFlags::AsyncLoading;
		for (UObject* Obj : ReferencedObjects)
		{
			// Temporary fatal messages instead of checks to find the cause for a one-time crash in shipping config
			UE_CLOG(Obj == nullptr, LogStreaming, Fatal, TEXT("NULL object in Async Objects Referencer"));
			UE_CLOG(!Obj->IsValidLowLevelFast(), LogStreaming, Fatal, TEXT("Invalid object in Async Objects Referencer"));
			Obj->AtomicallyClearInternalFlags(AsyncFlags);
			check(!Obj->HasAnyInternalFlags(AsyncFlags))
		}
		ReferencedObjects.Reset();
	}
	/** Removes all referenced objects and markes them for GC */
	void EmptyReferencedObjectsAndCancelLoading()
	{
		const EObjectFlags LoadFlags = RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects;
		const EInternalObjectFlags AsyncFlags = EInternalObjectFlags::Async | EInternalObjectFlags::AsyncLoading;

#if THREADSAFE_UOBJECTS
		FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
#endif

		// All of the referenced objects have been created by async loading code and may be in an invalid state so mark them for GC
		for (auto Object : ReferencedObjects)
		{
			Object->ClearInternalFlags(AsyncFlags);
			if (Object->HasAnyFlags(LoadFlags))
			{
				Object->AtomicallyClearFlags(LoadFlags);
				Object->MarkPendingKill();
			}
			check(!Object->HasAnyInternalFlags(AsyncFlags) && !Object->HasAnyFlags(LoadFlags));
		}
		ReferencedObjects.Reset();
	}

#if !UE_BUILD_SHIPPING
	/** Verifies that no object exists that has either EInternalObjectFlags::AsyncLoading and EInternalObjectFlags::Async set and is NOT being referenced by FAsyncObjectsReferencer */
	FORCENOINLINE void VerifyAssumptions()
	{
		const EInternalObjectFlags AsyncFlags = EInternalObjectFlags::Async | EInternalObjectFlags::AsyncLoading;
		for (FRawObjectIterator It; It; ++It)
		{
			FUObjectItem* ObjItem = *It;
			checkSlow(ObjItem);
			UObject* Object = static_cast<UObject*>(ObjItem->Object);
			if (Object->HasAnyInternalFlags(AsyncFlags))
			{
				if (!Contains(Object))
				{
					UE_LOG(LogStreaming, Error, TEXT("%s has AsyncLoading|Async set but is not referenced by FAsyncObjectsReferencer"), *Object->GetPathName());
				}
			}
		}
	}
#endif
};
FAsyncObjectsReferencer& FAsyncObjectsReferencer::Get()
{
	static FAsyncObjectsReferencer Singleton;
	return Singleton;
}

#if !UE_BUILD_SHIPPING
class FAsyncLoadingExec : private FSelfRegisteringExec
{
public:

	FAsyncLoadingExec()
	{}

	/** Console commands **/
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("VerifyAsyncLoadAssumptions")))
		{
			if (!IsAsyncLoading())
			{
				FAsyncObjectsReferencer::Get().VerifyAssumptions();
			}
			else
			{
				Ar.Logf(TEXT("Unable to verify async loading assumptions while streaming."));
			}
			return true;
		}
		return false;
	}
};
static TAutoPtr<FAsyncLoadingExec> GAsyncLoadingExec;
#endif

static int32 GAsyncLoadingThreadEnabled;
static FAutoConsoleVariableRef CVarAsyncLoadingThreadEnabledg(
	TEXT("s.AsyncLoadingThreadEnabled"),
	GAsyncLoadingThreadEnabled,
	TEXT("Placeholder console variable, currently not used in runtime."),
	ECVF_Default
	);


static int32 GWarnIfTimeLimitExceeded = 0;
static FAutoConsoleVariableRef CVarWarnIfTimeLimitExceeded(
	TEXT("s.WarnIfTimeLimitExceeded"),
	GWarnIfTimeLimitExceeded,
	TEXT("Enables log warning if time limit for time-sliced package streaming has been exceeded."),
	ECVF_Default
	);

static float GTimeLimitExceededMultiplier = 1.5f;
static FAutoConsoleVariableRef CVarTimeLimitExceededMultiplier(
	TEXT("s.TimeLimitExceededMultiplier"),
	GTimeLimitExceededMultiplier,
	TEXT("Multiplier for time limit exceeded warning time threshold."),
	ECVF_Default
	);

static float GTimeLimitExceededMinTime = 0.005f;
static FAutoConsoleVariableRef CVarTimeLimitExceededMinTime(
	TEXT("s.TimeLimitExceededMinTime"),
	GTimeLimitExceededMinTime,
	TEXT("Minimum time the time limit exceeded warning will be triggered by."),
	ECVF_Default
	);

static int32 GPreloadPackageDependencies = 0;
static FAutoConsoleVariableRef CVarPreloadPackageDependencies(
	TEXT("s.PreloadPackageDependencies"),
	GPreloadPackageDependencies,
	TEXT("Enables preloading of package dependencies based on data from the asset registry\n") \
	TEXT("0 - Do not preload dependencies. Can cause more seeks but uses less memory [default].\n") \
	TEXT("1 - Preload package dependencies. Faster but requires asset registry data to be loaded into memory\n"),
	ECVF_Default
	);

uint32 FAsyncLoadingThread::AsyncLoadingThreadID = 0;

static void IsTimeLimitExceededPrint(double InTickStartTime, double CurrentTime, float InTimeLimit, const TCHAR* InLastTypeOfWorkPerformed = nullptr, UObject* InLastObjectWorkWasPerformedOn = nullptr)
{
	// Log single operations that take longer than time limit (but only in cooked builds)
	if (
		(CurrentTime - InTickStartTime) > GTimeLimitExceededMinTime &&
		(CurrentTime - InTickStartTime) > (GTimeLimitExceededMultiplier * InTimeLimit))
	{
		UE_LOG(LogStreaming, Warning, TEXT("IsTimeLimitExceeded: %s %s took (less than) %5.2f ms"),
			InLastTypeOfWorkPerformed ? InLastTypeOfWorkPerformed : TEXT("unknown"),
			InLastObjectWorkWasPerformedOn ? *InLastObjectWorkWasPerformedOn->GetFullName() : TEXT("nullptr"),
			(CurrentTime - InTickStartTime) * 1000);
	}
}

static FORCEINLINE bool IsTimeLimitExceeded(double InTickStartTime, bool bUseTimeLimit, float InTimeLimit, const TCHAR* InLastTypeOfWorkPerformed = nullptr, UObject* InLastObjectWorkWasPerformedOn = nullptr)
{
	bool bTimeLimitExceeded = false;
	if (bUseTimeLimit)
	{
		double CurrentTime = FPlatformTime::Seconds();
		bTimeLimitExceeded = CurrentTime - InTickStartTime > InTimeLimit;

		if (GWarnIfTimeLimitExceeded)
		{
			IsTimeLimitExceededPrint(InTickStartTime, CurrentTime, InTimeLimit, InLastTypeOfWorkPerformed, InLastObjectWorkWasPerformedOn);
		}
	}
	return bTimeLimitExceeded;
}
FORCEINLINE bool FAsyncPackage::IsTimeLimitExceeded()
{
	return AsyncLoadingThread.IsAsyncLoadingSuspended() || ::IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn);
}

#if LOOKING_FOR_PERF_ISSUES
FThreadSafeCounter FAsyncLoadingThread::BlockingCycles = 0;
#endif

FAsyncLoadingThread& FAsyncLoadingThread::Get()
{
	static FAsyncLoadingThread GAsyncLoader;
	return GAsyncLoader;
}

/** Just like TGuardValue for FAsyncLoadingThread::bIsInAsyncLoadingTick but only works for the game thread */
struct FAsyncLoadingTickScope
{
	bool bWasInTick;
	FAsyncLoadingTickScope()
		: bWasInTick(false)
	{
		if (IsInGameThread())
		{
			FAsyncLoadingThread& AsyncLoadingThread = FAsyncLoadingThread::Get();
			bWasInTick = AsyncLoadingThread.GetIsInAsyncLoadingTick();
			AsyncLoadingThread.SetIsInAsyncLoadingTick(true);
		}
	}
	~FAsyncLoadingTickScope()
	{
		if (IsInGameThread())
		{
			FAsyncLoadingThread::Get().SetIsInAsyncLoadingTick(bWasInTick);
		}
	}
};

void FAsyncLoadingThread::InitializeAsyncThread()
{
	AsyncThreadReady.Increment();
}

void FAsyncLoadingThread::CancelAsyncLoadingInternal()
{
	{
		FGCScopeGuard GCGuard;
		FAsyncObjectsReferencer::Get().EmptyReferencedObjectsAndCancelLoading();
	}

	{
		// Packages we haven't yet started processing.
#if THREADSAFE_UOBJECTS
		FScopeLock QueueLock(&QueueCritical);
#endif
		const EAsyncLoadingResult::Type Result = EAsyncLoadingResult::Canceled;
		for (FAsyncPackageDesc* PackageDesc : QueuedPackages)
		{
			PackageDesc->PackageLoadedDelegate.ExecuteIfBound(PackageDesc->Name, nullptr, Result);
			delete PackageDesc;
		}
		QueuedPackages.Reset();
	}

	{
		// Packages we started processing, need to be canceled.
		// Accessed only in async thread, no need to protect region.
		for (FAsyncPackage* AsyncPackage : AsyncPackages)
		{
			AsyncPackage->Cancel();
			delete AsyncPackage;
		}

		AsyncPackages.Reset();
		AsyncPackageNameLookup.Reset();
	}

	{
		// Packages that are already loaded. May be halfway through PostLoad
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesCritical);
#endif
		for (FAsyncPackage* LoadedPackage : LoadedPackages)
		{
			LoadedPackage->Cancel();
			delete LoadedPackage;
		}
		LoadedPackages.Reset();
		LoadedPackagesNameLookup.Reset();
	}
	{
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesToProcessCritical);
#endif
		for (FAsyncPackage* LoadedPackage : LoadedPackagesToProcess)
		{
			LoadedPackage->Cancel();
			delete LoadedPackage;
		}
		LoadedPackagesToProcess.Reset();
		LoadedPackagesToProcessNameLookup.Reset();
	}

	ExistingAsyncPackagesCounter.Reset();
	QueuedPackagesCounter.Reset();

	FUObjectThreadContext::Get().ObjLoaded.Empty();

	// Notify everyone streaming is canceled.
	CancelLoadingEvent->Trigger();
}

void FAsyncLoadingThread::QueuePackage(const FAsyncPackageDesc& Package)
{
	{
#if THREADSAFE_UOBJECTS
		FScopeLock QueueLock(&QueueCritical);
#endif
		QueuedPackagesCounter.Increment();
		QueuedPackages.Add(new FAsyncPackageDesc(Package));
	}
	QueuedRequestsEvent->Trigger();
}

FAsyncPackage* FAsyncLoadingThread::FindExistingPackageAndAddCompletionCallback(FAsyncPackageDesc* PackageRequest, TMap<FName, FAsyncPackage*>& PackageList)
{
	checkSlow(IsInAsyncLoadThread());
	FAsyncPackage* Result = PackageList.FindRef(PackageRequest->Name);
	if (Result)
	{
		if (PackageRequest->PackageLoadedDelegate.IsBound())
		{
			const bool bInternalCallback = false;
			Result->AddCompletionCallback(PackageRequest->PackageLoadedDelegate, bInternalCallback);
			Result->AddRequestID(PackageRequest->RequestID);
		}
		const int32 QueuedPackagesCount = QueuedPackagesCounter.Decrement();
		check(QueuedPackagesCount >= 0);
	}
	return Result;
}

void FAsyncLoadingThread::UpdateExistingPackagePriorities(FAsyncPackage* InPackage, TAsyncLoadPriority InNewPriority, IAssetRegistryInterface* InAssetRegistry)
{

	if (InNewPriority > InPackage->GetPriority())
	{
		AsyncPackages.Remove(InPackage);
		// always inserted anyway AsyncPackageNameLookup.Remove(InPackage->GetPackageName());
		InPackage->SetPriority(InNewPriority);

		InsertPackage(InPackage, EAsyncPackageInsertMode::InsertBeforeMatchingPriorities);

		// Reduce loading counters as InsertPackage incremented them again
		ExistingAsyncPackagesCounter.Decrement();
	}

#if !WITH_EDITOR
	if (InAssetRegistry)
	{
		TMap<FName, int32> OrderTracker;
		int32 Order = 0;
		ScanPackageDependenciesForLoadOrder(*InPackage->GetPackageName().ToString(), OrderTracker, Order, InAssetRegistry);

		OrderTracker.ValueSort(TLess<int32>());

		// should adjust the low level IO priorities here!

		for (auto& Dependency : OrderTracker)
		{
			if (Dependency.Value < OrderTracker.Num() - 1) // we are already handling this one
			{
				FAsyncPackage* DependencyPackage = AsyncPackageNameLookup.FindRef(Dependency.Key);
				if (DependencyPackage)
				{
					UpdateExistingPackagePriorities(DependencyPackage, InNewPriority, nullptr);
				}
			}
		}
	}
#endif
}

void FAsyncLoadingThread::ProcessAsyncPackageRequest(FAsyncPackageDesc* InRequest, FAsyncPackage* InRootPackage, IAssetRegistryInterface* InAssetRegistry)
{
	FAsyncPackage* Package = FindExistingPackageAndAddCompletionCallback(InRequest, AsyncPackageNameLookup);

	if (Package)
	{
		// The package is already sitting in the queue. Make sure the its priority, and the priority of all its
		// dependencies is at least as high as the priority of this request
		UpdateExistingPackagePriorities(Package, InRequest->Priority, InAssetRegistry);
	}
	else
	{
		// [BLOCKING] LoadedPackages are accessed on the main thread too, so lock to be able to add a completion callback
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesCritical);
#endif
		Package = FindExistingPackageAndAddCompletionCallback(InRequest, LoadedPackagesNameLookup);
	}

	if (!Package)
	{
		// [BLOCKING] LoadedPackagesToProcess are modified on the main thread, so lock to be able to add a completion callback
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesToProcessCritical);
#endif
		Package = FindExistingPackageAndAddCompletionCallback(InRequest, LoadedPackagesToProcessNameLookup);
	}

	if (!Package)
	{
		// we probably don't need this bit (we can't cover much latency anyway) but it prevents false positives
		// probably would be better to start this on the game thread when the request is initiated.
#if 0 // left here as a note that this is probably not prestreamed and maybe could be
		if (InAssetRegistry && !InRootPackage)
		{
			FString PrestreamFilename = GetPrestreamPackageLinkerName(*InRequest->Name.ToString());
			if (PrestreamFilename.Len() > 0)
			{
				HintFutureRead(*PrestreamFilename); // should set the priority on the IO requests here
			}
		}
#endif
		// New package that needs to be loaded or a package has already been loaded long time ago
		Package = new FAsyncPackage(*InRequest);
		if (InRequest->PackageLoadedDelegate.IsBound())
		{
			const bool bInternalCallback = false;
			Package->AddCompletionCallback(InRequest->PackageLoadedDelegate, bInternalCallback);
		}
		Package->SetDependencyRootPackage(InRootPackage);

#if !WITH_EDITOR
		if (InAssetRegistry && !InRootPackage)
		{
			InRootPackage = Package;
			TMap<FName, int32> OrderTracker;
			int32 Order = 0;
			ScanPackageDependenciesForLoadOrder(*InRequest->Name.ToString(), OrderTracker, Order, InAssetRegistry);

			OrderTracker.ValueSort(TLess<int32>());
#if USE_NEW_ASYNC_IO
			/*for (auto& Dependency : OrderTracker)
			{
				FString PrestreamFilename = GetPrestreamPackageLinkerName(*Dependency.Key.ToString());
				if (PrestreamFilename.Len() > 0)
				{
					HintFutureRead(*PrestreamFilename); // should set the priority on the IO requests here
			}
			}*/
#endif
			for (auto& Dependency : OrderTracker)
			{
				if (Dependency.Value < OrderTracker.Num() - 1) // we are already handling this one
						{
							QueuedPackagesCounter.Increment();
							const int32 RequestID = GPackageRequestID.Increment();
							FAsyncLoadingThread::Get().AddPendingRequest(RequestID);
					FAsyncPackageDesc DependencyPackageRequest(RequestID, Dependency.Key, NAME_None, FGuid(), FLoadPackageAsyncDelegate(), InRequest->PackageFlags, INDEX_NONE, InRequest->Priority);
					ProcessAsyncPackageRequest(&DependencyPackageRequest, InRootPackage, nullptr);
				}
			}
		}
#endif
		// Add to queue according to priority.
		InsertPackage(Package, EAsyncPackageInsertMode::InsertAfterMatchingPriorities);

		// For all other cases this is handled in FindExistingPackageAndAddCompletionCallback
		const int32 QueuedPackagesCount = QueuedPackagesCounter.Decrement();
		check(QueuedPackagesCount >= 0);
	}
}

int32 FAsyncLoadingThread::CreateAsyncPackagesFromQueue()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateAsyncPackagesFromQueue);
	SCOPED_LOADTIMER(CreateAsyncPackagesFromQueueTime);

	FAsyncLoadingTickScope InAsyncLoadingTick;

	int32 NumCreated = 0;
	checkSlow(IsInAsyncLoadThread());

	TArray<FAsyncPackageDesc*> QueueCopy;
	{
#if THREADSAFE_UOBJECTS
		FScopeLock QueueLock(&QueueCritical);
#endif
		QueueCopy = QueuedPackages;
		QueuedPackages.Reset();
	}

	if (QueueCopy.Num() > 0)
	{
		IAssetRegistryInterface* AssetRegistry = nullptr;
		
		if (GPreloadPackageDependencies && IsPlatformFileCompatibleWithDependencyPreloading())
		{
			AssetRegistry = IAssetRegistryInterface::GetPtr();
		}
		
		double Timer = 0;
		{
			SCOPE_SECONDS_COUNTER(Timer);
			for (FAsyncPackageDesc* PackageRequest : QueueCopy)
			{
				ProcessAsyncPackageRequest(PackageRequest, nullptr, AssetRegistry);
				delete PackageRequest;
			}
		}
		UE_LOG(LogStreaming, Verbose, TEXT("Async package requests inserted in %fms"), Timer * 1000.0);
	}

	NumCreated = QueueCopy.Num();

	return NumCreated;
}

void FAsyncLoadingThread::InsertPackage(FAsyncPackage* Package, EAsyncPackageInsertMode InsertMode)
{
	checkSlow(IsInAsyncLoadThread());

	// Incremented on the Async Thread, decremented on the game thread
	ExistingAsyncPackagesCounter.Increment();

	{
#if THREADSAFE_UOBJECTS
		FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
#endif
		int32 InsertIndex = -1;

		switch (InsertMode)
		{
		case EAsyncPackageInsertMode::InsertAfterMatchingPriorities:
			{
				InsertIndex = AsyncPackages.IndexOfByPredicate([Package](const FAsyncPackage* Element)
				{
					return Element->GetPriority() < Package->GetPriority();
				});

				break;
			}

		case EAsyncPackageInsertMode::InsertBeforeMatchingPriorities:
			{
		// Insert new package keeping descending priority order in AsyncPackages
				InsertIndex = AsyncPackages.IndexOfByPredicate([Package](const FAsyncPackage* Element)
		{
			return Element->GetPriority() <= Package->GetPriority();
		});

				break;
			}
		};

		InsertIndex = InsertIndex == INDEX_NONE ? AsyncPackages.Num() : InsertIndex;

		AsyncPackages.InsertUninitialized(InsertIndex);
		AsyncPackages[InsertIndex] = Package;
		AsyncPackageNameLookup.Add(Package->GetPackageName(), Package);
	}
}

void FAsyncLoadingThread::JustInTimeHinting()
{
#if USE_NEW_ASYNC_IO
	const int32 NumHints = 1000;

	for (int32 PackageIdx = 0; PackageIdx < AsyncPackages.Num() && PendingPackageHints.Num() < NumHints; ++PackageIdx)
	{
		FAsyncPackage* Package = AsyncPackages[PackageIdx];
		if (!PendingPackageHints.Contains(Package))
		{
			const FString PrestreamName = GetPrestreamPackageLinkerName(*Package->GetPackageNameToLoad().ToString(), false);
			if(PrestreamName.Len())
			{
				HintFutureRead(*PrestreamName);
				PendingPackageHints.Add(Package);
			}			
		}

	}
#endif
}

EAsyncPackageState::Type FAsyncLoadingThread::ProcessAsyncLoading(int32& OutPackagesProcessed, bool bUseTimeLimit /*= false*/, bool bUseFullTimeLimit /*= false*/, float TimeLimit /*= 0.0f*/)
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncLoadingThread_ProcessAsyncLoading);
	SCOPED_LOADTIMER(AsyncLoadingTime);
	
	// If we're not multithreaded and flushing async loading, update the thread heartbeat
	const bool bNeedsHeartbeatTick = !bUseTimeLimit && !FAsyncLoadingThread::IsMultithreaded();

	EAsyncPackageState::Type LoadingState = EAsyncPackageState::Complete;
	OutPackagesProcessed = 0;

	// We need to loop as the function has to handle finish loading everything given no time limit
	// like e.g. when called from FlushAsyncLoading.
	for (int32 PackageIndex = 0; LoadingState != EAsyncPackageState::TimeOut && PackageIndex < AsyncPackages.Num(); ++PackageIndex)
	{
		JustInTimeHinting();

		SCOPED_LOADTIMER(ProcessAsyncLoadingTime);
		OutPackagesProcessed++;

		// Package to be loaded.
		FAsyncPackage* Package = AsyncPackages[PackageIndex];

		if (Package->HasFinishedLoading() == false)
		{
			    // Package tick returns EAsyncPackageState::Complete on completion.
			    // We only tick packages that have not yet been loaded.
			    LoadingState = Package->Tick(bUseTimeLimit, bUseFullTimeLimit, TimeLimit);
		}
		else
		{
			// This package has finished loading but some other package is still holding
			// a reference to it because it has this package in its dependency list.
			LoadingState = EAsyncPackageState::Complete;
		}
		bool bPackageFullyLoaded = false;
		if (LoadingState == EAsyncPackageState::Complete)
		{

#if USE_NEW_ASYNC_IO
			if (PendingPackageHints.Contains(Package))
			{
				PendingPackageHints.Remove(Package);
				HintFutureReadDone(*Package->GetPackageNameToLoad().ToString());
			}
#endif			


			// We're done, at least on this thread, so we can remove the package now.
			AddToLoadedPackages(Package);
			{
#if THREADSAFE_UOBJECTS
				FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
#endif
				AsyncPackageNameLookup.Remove(Package->GetPackageName());
				AsyncPackages.RemoveAt(PackageIndex);
			}
					
			// Need to process this index again as we just removed an item
			PackageIndex--;
			bPackageFullyLoaded = true;
		}
		else if (!bUseTimeLimit && !FPlatformProcess::SupportsMultithreading())
		{
			// Tick async loading when multithreading is disabled.
			FIOSystem::Get().TickSingleThreaded();
		}

		// Check if there's any new packages in the queue.
		CreateAsyncPackagesFromQueue();

		if (bNeedsHeartbeatTick)
		{
			// Update heartbeat after each package has been processed
			FThreadHeartBeat::Get().HeartBeat();
		}
	}

	return LoadingState;
}


EAsyncPackageState::Type FAsyncLoadingThread::ProcessLoadedPackages(bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, int32 WaitForRequestID /*= INDEX_NONE*/)
{
	SCOPED_LOADTIMER(TickAsyncLoading_ProcessLoadedPackages);

	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;


	double TickStartTime = FPlatformTime::Seconds();
	{
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedPackagesLock(&LoadedPackagesCritical);
		FScopeLock LoadedPackagesToProcessLock(&LoadedPackagesToProcessCritical);
#endif
		LoadedPackagesToProcess.Append(LoadedPackages);
		LoadedPackagesToProcessNameLookup.Append(LoadedPackagesNameLookup);
		LoadedPackages.Reset();
		LoadedPackagesNameLookup.Reset();
	}
		
	for (int32 PackageIndex = 0; PackageIndex < LoadedPackagesToProcess.Num() && !IsAsyncLoadingSuspended(); ++PackageIndex)
	{
		SCOPED_LOADTIMER(ProcessLoadedPackagesTime);

		if (PackageIndex % 20 == 0 && IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("ProcessLoadedPackages")))
		{
			break;
		}

		FAsyncPackage* Package = LoadedPackagesToProcess[PackageIndex];

		Result = Package->PostLoadDeferredObjects(TickStartTime, bUseTimeLimit, TimeLimit);
		if (Result == EAsyncPackageState::Complete)
		{
			// Remove the package from the list before we trigger the callbacks, 
			// this is to ensure we can re-enter FlushAsyncLoading from any of the callbacks
			{
				FScopeLock LoadedLock(&LoadedPackagesToProcessCritical);					
				LoadedPackagesToProcess.RemoveAt(PackageIndex--);
				LoadedPackagesToProcessNameLookup.Remove(Package->GetPackageName());
					
				if (FPlatformProperties::RequiresCookedData())
				{
					// Emulates ResetLoaders on the package linker's linkerroot.
					Package->ResetLoader();
				}
				else
				{
					// Detach linker in mutex scope to make sure that if something requests this package
					// before it's been deleted does not try to associate the new async package with the old linker
					// while this async package is still bound to it.
					Package->DetachLinker();
				}
					
			}

			// Incremented on the Async Thread, now decrement as we're done with this package				
			const int32 NewExistingAsyncPackagesCounterValue = ExistingAsyncPackagesCounter.Decrement();
			UE_CLOG(NewExistingAsyncPackagesCounterValue < 0, LogStreaming, Fatal, TEXT("ExistingAsyncPackagesCounter is negative, this means we loaded more packages then requested so there must be a bug in async loading code."));

			// Call external callbacks
			const bool bInternalCallbacks = false;
			const EAsyncLoadingResult::Type LoadingResult = Package->HasLoadFailed() ? EAsyncLoadingResult::Failed : EAsyncLoadingResult::Succeeded;
			Package->CallCompletionCallbacks(bInternalCallbacks, LoadingResult);

			// We don't need the package anymore
			PackagesToDelete.Add(Package);

			if (WaitForRequestID != INDEX_NONE && !ContainsRequestID(WaitForRequestID))
			{
				// The only package we care about has finished loading, so we're good to exit
				break;
			}
		}
		else
		{
			break;
		}
	}

	// Delete packages we're done processing and are no longer dependencies of anything else
	for (int32 PackageIndex = 0; PackageIndex < PackagesToDelete.Num(); ++PackageIndex)
	{
		FAsyncPackage* Package = PackagesToDelete[PackageIndex];
		if (Package->GetDependencyRefCount() == 0)
		{
			delete Package;
			PackagesToDelete.RemoveAtSwap(PackageIndex--);
		}
	}

	if (Result == EAsyncPackageState::Complete)
	{
		// We're not done until all packages have been deleted
		Result = PackagesToDelete.Num() ? EAsyncPackageState::PendingImports : EAsyncPackageState::Complete;
	}
	

	return Result;
}



EAsyncPackageState::Type FAsyncLoadingThread::TickAsyncLoading(bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, int32 WaitForRequestID /*= INDEX_NONE*/)
{
	const bool bLoadingSuspended = IsAsyncLoadingSuspended();
	const bool bIsMultithreaded = FAsyncLoadingThread::IsMultithreaded();
	EAsyncPackageState::Type Result = bLoadingSuspended ? EAsyncPackageState::PendingImports : EAsyncPackageState::Complete;

	if (!bLoadingSuspended)
	{
		double TickStartTime = FPlatformTime::Seconds();
		double TimeLimitUsedForProcessLoaded = 0;

		{
			Result = ProcessLoadedPackages(bUseTimeLimit, bUseFullTimeLimit, TimeLimit, WaitForRequestID);
			TimeLimitUsedForProcessLoaded = FPlatformTime::Seconds() - TickStartTime;
		}

		if (!bIsMultithreaded && Result != EAsyncPackageState::TimeOut && !IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("Pre-TickAsyncThread")))
		{
			double RemainingTimeLimit = FMath::Max(0.0, TimeLimit - TimeLimitUsedForProcessLoaded);
			Result = TickAsyncThread(bUseTimeLimit, bUseFullTimeLimit, RemainingTimeLimit);			
		}

		if (Result != EAsyncPackageState::TimeOut && !IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("Pre-EmptyReferencedObjects")))
		{
#if THREADSAFE_UOBJECTS
			FScopeLock QueueLock(&QueueCritical);
			FScopeLock LoadedLock(&LoadedPackagesCritical);
#endif
			// Release references we acquired when async loading when there are no more FAsyncPackages in existence
			if (ExistingAsyncPackagesCounter.GetValue() == 0)
			{
				FDeferredMessageLog::Flush();
				FAsyncObjectsReferencer::Get().EmptyReferencedObjects();
			}
		}
	}

	return Result;
}

FAsyncLoadingThread::FAsyncLoadingThread()
{
#if !UE_BUILD_SHIPPING
	GAsyncLoadingExec = new FAsyncLoadingExec();
#endif
	QueuedRequestsEvent = FPlatformProcess::GetSynchEventFromPool();
	CancelLoadingEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadSuspendedEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadResumedEvent = FPlatformProcess::GetSynchEventFromPool();
	if (FAsyncLoadingThread::IsMultithreaded())
	{
		UE_LOG(LogStreaming, Log, TEXT("Async loading is multithreaded."));
		Thread = FRunnableThread::Create(this, TEXT("FAsyncLoadingThread"), 0, TPri_Normal);
	}
	else
	{
		UE_LOG(LogStreaming, Log, TEXT("Async loading is time-sliced."));
		Thread = nullptr;
		Init();
	}
	bIsInAsyncLoadingTick = false;
}

FAsyncLoadingThread::~FAsyncLoadingThread()
{
	delete Thread;
	Thread = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(QueuedRequestsEvent);
	QueuedRequestsEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(CancelLoadingEvent);
	CancelLoadingEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(ThreadSuspendedEvent);
	ThreadSuspendedEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(ThreadResumedEvent);
	ThreadResumedEvent = nullptr;	
}

bool FAsyncLoadingThread::Init()
{
	return true;
}

uint32 FAsyncLoadingThread::Run()
{
	AsyncLoadingThreadID = FPlatformTLS::GetCurrentThreadId();

	bool bWasSuspendedLastFrame = false;
	while (StopTaskCounter.GetValue() == 0)
	{
		if (IsLoadingSuspended.GetValue() == 0)
		{
			if (bWasSuspendedLastFrame)
			{
				bWasSuspendedLastFrame = false;
				ThreadResumedEvent->Trigger();
			}			
			TickAsyncThread(false, true, 0.0f);
		}
		else if (!bWasSuspendedLastFrame)
		{
			bWasSuspendedLastFrame = true;
			ThreadSuspendedEvent->Trigger();			
		}
		else
		{
			FPlatformProcess::SleepNoStats(0.001f);
		}		
	}
	return 0;
}

EAsyncPackageState::Type  FAsyncLoadingThread::TickAsyncThread(bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit)
{
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	if (!bShouldCancelLoading)
	{
		int32 ProcessedRequests = 0;
		if (AsyncThreadReady.GetValue())
		{
			CreateAsyncPackagesFromQueue();
			Result = ProcessAsyncLoading(ProcessedRequests, bUseTimeLimit, bUseFullTimeLimit, TimeLimit);
		}
		if (ProcessedRequests == 0 && IsMultithreaded())
		{
			const bool bIgnoreThreadIdleStats = true;
			QueuedRequestsEvent->Wait(30, bIgnoreThreadIdleStats);
		}
	}
	else
	{
		// Blocks main thread
		CancelAsyncLoadingInternal();
		bShouldCancelLoading = false;
	}

#if LOOKING_FOR_PERF_ISSUES
	// Update stats
	SET_FLOAT_STAT( STAT_AsyncIO_AsyncLoadingBlockingTime, FPlatformTime::ToSeconds( BlockingCycles.GetValue() ) );
	BlockingCycles.Set( 0 );
#endif

	return Result;
}

void FAsyncLoadingThread::Stop()
{
	StopTaskCounter.Increment();
}

void FAsyncLoadingThread::CancelAsyncLoading()
{
	checkSlow(IsInGameThread());	

	bShouldCancelLoading = true;
	if (IsMultithreaded())
	{
		CancelLoadingEvent->Wait();
	}
	else
	{
		// This will immediately cancel async loading without waiting for packages to finish loading
		FlushAsyncLoading();
		// It's possible we haven't been async loading at all in which case the above call would not reset bShouldCancelLoading
		bShouldCancelLoading = false;
	}
}

void FAsyncLoadingThread::SuspendLoading()
{
	UE_CLOG(!IsInGameThread() || IsInSlateThread(), LogStreaming, Fatal, TEXT("Async loading can only be suspended from the main thread"));
	const int32 SuspendCount = IsLoadingSuspended.Increment();
#if !WITH_EDITORONLY_DATA
	UE_LOG(LogStreaming, Display, TEXT("Suspending async loading (%d)"), SuspendCount);
#endif
	if (IsMultithreaded() && SuspendCount == 1)
	{
		ThreadSuspendedEvent->Wait();
	}
}

void FAsyncLoadingThread::ResumeLoading()
{
	check(IsInGameThread() && !IsInSlateThread());
	const int32 SuspendCount = IsLoadingSuspended.Decrement();
#if !WITH_EDITORONLY_DATA
	UE_LOG(LogStreaming, Display, TEXT("Resuming async loading (%d)"), SuspendCount);
#endif
	UE_CLOG(SuspendCount < 0, LogStreaming, Fatal, TEXT("ResumeAsyncLoadingThread: Async loading was resumed more times than it was suspended."));
	if (IsMultithreaded() && SuspendCount == 0)
	{
		ThreadResumedEvent->Wait();
	}
}

float FAsyncLoadingThread::GetAsyncLoadPercentage(const FName& PackageName)
{
	float LoadPercentage = -1.0f;
	{
#if THREADSAFE_UOBJECTS
		FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
#endif
		FAsyncPackage* Package = AsyncPackageNameLookup.FindRef(PackageName);
		if (Package)
		{
			LoadPercentage = Package->GetLoadPercentage();
		}
	}
	if (LoadPercentage < 0.0f)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock LockLoadedPackages(&LoadedPackagesCritical);
#endif
		FAsyncPackage* Package = LoadedPackagesNameLookup.FindRef(PackageName);
		if (Package)
		{
			LoadPercentage = Package->GetLoadPercentage();
		}
	}
	if (LoadPercentage < 0.0f)
	{
		checkSlow(IsInGameThread());
		FAsyncPackage* Package = LoadedPackagesToProcessNameLookup.FindRef(PackageName);
		if (Package)
		{
			LoadPercentage = Package->GetLoadPercentage();
		}
	}

	return LoadPercentage;
}

/**
 * Call back into the async loading code to inform of the creation of a new object
 * @param Object		Object created
 * @param bSubObject	Object created as a sub-object of a loaded object
 */
void NotifyConstructedDuringAsyncLoading(UObject* Object, bool bSubObject)
{
	// Mark objects created during async loading process (e.g. from within PostLoad or CreateExport) as async loaded so they 
	// cannot be found. This requires also keeping track of them so we can remove the async loading flag later one when we 
	// finished routing PostLoad to all objects.
	if (!bSubObject)
	{
		Object->SetInternalFlags(EInternalObjectFlags::AsyncLoading);
	}
	FAsyncObjectsReferencer::Get().AddObject(Object);
}

/*-----------------------------------------------------------------------------
	FAsyncPackage implementation.
-----------------------------------------------------------------------------*/


int32 FAsyncPackage::PreLoadIndex = 0;
int32 FAsyncPackage::PreLoadSortIndex = 0;
int32 FAsyncPackage::PostLoadIndex = 0;

/**
* Constructor
*/
FAsyncPackage::FAsyncPackage(const FAsyncPackageDesc& InDesc)
: Desc(InDesc)
, Linker(nullptr)
, LinkerRoot(nullptr)
, DependencyRootPackage(nullptr)
, DependencyRefCount(0)
, LoadImportIndex(0)
, ImportIndex(0)
, ExportIndex(0)
, DeferredPostLoadIndex(0)
, TimeLimit(FLT_MAX)
, bUseTimeLimit(false)
, bUseFullTimeLimit(false)
, bTimeLimitExceeded(false)
, bLoadHasFailed(false)
, bLoadHasFinished(false)
, TickStartTime(0)
, LastObjectWorkWasPerformedOn(nullptr)
, LastTypeOfWorkPerformed(nullptr)
, LoadStartTime(0.0)
, LoadPercentage(0)
, AsyncLoadingThread(FAsyncLoadingThread::Get())
#if PERF_TRACK_DETAILED_ASYNC_STATS
, TickCount(0)
, TickLoopCount(0)
, CreateLinkerCount(0)
, FinishLinkerCount(0)
, CreateImportsCount(0)
, CreateExportsCount(0)
, PreLoadObjectsCount(0)
, PostLoadObjectsCount(0)
, FinishObjectsCount(0)
, TickTime(0.0)
, CreateLinkerTime(0.0)
, FinishLinkerTime(0.0)
, CreateImportsTime(0.0)
, CreateExportsTime(0.0)
, PreLoadObjectsTime(0.0)
, PostLoadObjectsTime(0.0)
, FinishObjectsTime(0.0)
#endif // PERF_TRACK_DETAILED_ASYNC_STATS
{
	AddRequestID(InDesc.RequestID);
}

FAsyncPackage::~FAsyncPackage()
{
	AsyncLoadingThread.RemovePendingRequests(RequestIDs);
	DetachLinker();
}

void FAsyncPackage::AddRequestID(int32 Id)
{
	if (Id > 0)
	{
		RequestIDs.Add(Id);
		AsyncLoadingThread.AddPendingRequest(Id);
	}
}

/**
 * @return Time load begun. This is NOT the time the load was requested in the case of other pending requests.
 */
double FAsyncPackage::GetLoadStartTime() const
{
	return LoadStartTime;
}

/**
 * Emulates ResetLoaders for the package's Linker objects, hence deleting it. 
 */
void FAsyncPackage::ResetLoader()
{
	// Reset loader.
	if (Linker)
	{
		check(Linker->AsyncRoot == this || Linker->AsyncRoot == nullptr);
		Linker->AsyncRoot = nullptr;
		// Flush cache and queue for delete, the linker will be detached later when it or its root is destroyed.
		Linker->FlushCache();
		FLinkerManager::Get().RemoveLinker(Linker);
		Linker = nullptr;
	}
}

void FAsyncPackage::DetachLinker()
{	
	if (Linker)
	{
		check(bLoadHasFinished || bLoadHasFailed);
		check(Linker->AsyncRoot == this || Linker->AsyncRoot == nullptr);
		Linker->AsyncRoot = nullptr;
		Linker = nullptr;
	}
}

/**
 * Gives up time slice if time limit is enabled.
 *
 * @return true if time slice can be given up, false otherwise
 */
bool FAsyncPackage::GiveUpTimeSlice()
{
	static const bool bPlatformIsSingleThreaded = !FPlatformProcess::SupportsMultithreading();
	if (bPlatformIsSingleThreaded)
	{
		FIOSystem::Get().TickSingleThreaded();
	}

	if (bUseTimeLimit && !bUseFullTimeLimit)
	{
		bTimeLimitExceeded = true;
	}
	return bTimeLimitExceeded;
}

/**
 * Begin async loading process. Simulates parts of BeginLoad.
 *
 * Objects created during BeginAsyncLoad and EndAsyncLoad will have EInternalObjectFlags::AsyncLoading set
 */
void FAsyncPackage::BeginAsyncLoad()
{
	if (IsInGameThread())
	{
		FAsyncLoadingThread::Get().SetIsInAsyncLoadingTick(true);
	}

	// this won't do much during async loading except increase the load count which causes IsLoading to return true
	BeginLoad();
}

/**
 * End async loading process. Simulates parts of EndLoad(). FinishObjects 
 * simulates some further parts once we're fully done loading the package.
 */
void FAsyncPackage::EndAsyncLoad()
{
	check(IsAsyncLoading());

	// this won't do much during async loading except decrease the load count which causes IsLoading to return false
	EndLoad();

	if (IsInGameThread())
	{
		FAsyncLoadingThread::Get().SetIsInAsyncLoadingTick(false);
	}

	if (!bLoadHasFailed)
	{
		// Mark the package as loaded, if we succeeded
		LinkerRoot->SetFlags(RF_WasLoaded);
	}
}

/**
 * Ticks the async loading code.
 *
 * @param	InbUseTimeLimit		Whether to use a time limit
 * @param	InbUseFullTimeLimit	If true use the entire time limit, even if you have to block on IO
 * @param	InOutTimeLimit			Soft limit to time this function may take
 *
 * @return	true if package has finished loading, false otherwise
 */

EAsyncPackageState::Type FAsyncPackage::Tick(bool InbUseTimeLimit, bool InbUseFullTimeLimit, float& InOutTimeLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_Tick);
	SCOPED_LOADTIMER(Package_Tick);

	// Whether we should execute the next step.
	EAsyncPackageState::Type LoadingState = EAsyncPackageState::Complete;

	check(LastObjectWorkWasPerformedOn == nullptr);
	check(LastTypeOfWorkPerformed == nullptr);

	// Set up tick relevant variables.
	bUseTimeLimit = InbUseTimeLimit;
	bUseFullTimeLimit = InbUseFullTimeLimit;
	bTimeLimitExceeded = false;
	TimeLimit = InOutTimeLimit;
	TickStartTime = FPlatformTime::Seconds();

	// Keep track of time when we start loading.
	if (LoadStartTime == 0.0)
	{
		LoadStartTime = TickStartTime;

		// If we are a dependency of another package, we need to tell that package when its first dependent started loading,
		// otherwise because that package loads last it'll not include the entire load time of all its dependencies
		if (DependencyRootPackage)
		{
			// Only the first dependent needs to register the start time
			if (DependencyRootPackage->GetLoadStartTime() == 0.0)
			{
				DependencyRootPackage->LoadStartTime = TickStartTime;
			}
		}
	}

	// Make sure we finish our work if there's no time limit. The loop is required as PostLoad
	// might cause more objects to be loaded in which case we need to Preload them again.
	do
	{
		// Reset value to true at beginning of loop.
		LoadingState = EAsyncPackageState::Complete;

		// Begin async loading, simulates BeginLoad
		BeginAsyncLoad();

		// We have begun loading a package that we know the name of. Let the package time tracker know.
		FExclusiveLoadPackageTimeTracker::PushLoadPackage(Desc.NameToLoad);

		// Create raw linker. Needs to be async created via ticking before it can be used.
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_CreateLinker);
			LoadingState = CreateLinker();
		}

		// Async create linker.
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_FinishLinker);
			LoadingState = FinishLinker();
		}

		// Load imports from linker import table asynchronously.
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_LoadImports);
			LoadingState = LoadImports();
		}

		// Create imports from linker import table.
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_CreateImports);
			LoadingState = CreateImports();
		}

		// Finish all async texture allocations.
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_FinishTextureAllocations);
			LoadingState = FinishTextureAllocations();
		}

#if WITH_EDITORONLY_DATA
		// Create and preload the package meta-data
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_CreateMetaData);
			LoadingState = CreateMetaData();
		}
#endif // WITH_EDITORONLY_DATA

		// Create exports from linker export table and also preload them.
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_CreateExports);
			LoadingState = CreateExports();
		}

		// Call Preload on the linker for all loaded objects which causes actual serialization.
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_PreLoadObjects);
			LoadingState = PreLoadObjects();
		}

		// Call PostLoad on objects, this could cause new objects to be loaded that require
		// another iteration of the PreLoad loop.
		if (LoadingState == EAsyncPackageState::Complete)
		{
			SCOPED_LOADTIMER(Package_PostLoadObjects);
			LoadingState = PostLoadObjects();
		}

		// We are done loading the package for now. Whether it is done or not, let the package time tracker know.
		FExclusiveLoadPackageTimeTracker::PopLoadPackage(Linker ? Linker->LinkerRoot : nullptr);

		// End async loading, simulates EndLoad
		EndAsyncLoad();

		// Finish objects (removing EInternalObjectFlags::AsyncLoading, dissociate imports and forced exports, 
		// call completion callback, ...
		// If the load has failed, perform completion callbacks and then quit
		if (LoadingState == EAsyncPackageState::Complete || bLoadHasFailed)
		{
			LoadingState = FinishObjects();
		}
	} while (!IsTimeLimitExceeded() && LoadingState == EAsyncPackageState::TimeOut);

	check(bUseTimeLimit || LoadingState != EAsyncPackageState::TimeOut || AsyncLoadingThread.IsAsyncLoadingSuspended());

	// We can't have a reference to a UObject.
	LastObjectWorkWasPerformedOn = nullptr;
	// Reset type of work performed.
	LastTypeOfWorkPerformed = nullptr;
	// Mark this package as loaded if everything completed.
	bLoadHasFinished = (LoadingState == EAsyncPackageState::Complete);
	// Subtract the time it took to load this package from the global limit.
	InOutTimeLimit = (float)FMath::Max(0.0, InOutTimeLimit - (FPlatformTime::Seconds() - TickStartTime));

	// true means that we're done loading this package.
	return LoadingState;
}

/**
 * Create linker async. Linker is not finalized at this point.
 *
 * @return true
 */
EAsyncPackageState::Type FAsyncPackage::CreateLinker()
{
	SCOPED_LOADTIMER(CreateLinkerTime);
	if (Linker == nullptr)
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateLinker);

		LastObjectWorkWasPerformedOn	= nullptr;
		LastTypeOfWorkPerformed			= TEXT("creating Linker");

		// Try to find existing package or create it if not already present.
		UPackage* Package = nullptr;
		{
			FGCScopeGuard GCGuard;
			Package = CreatePackage(nullptr, *Desc.Name.ToString());
			FAsyncObjectsReferencer::Get().AddObject(Package);
			LinkerRoot = Package;
		}
		FScopeCycleCounterUObject ConstructorScope(Package, GET_STATID(STAT_FAsyncPackage_CreateLinker));

		// Set package specific data 
		Package->SetPackageFlags(Desc.PackageFlags);
#if WITH_EDITOR
		Package->PIEInstanceID = Desc.PIEInstanceID;
#endif

		// Always store package filename we loading from
		Package->FileName = Desc.NameToLoad;
#if WITH_EDITORONLY_DATA
		// Assume all packages loaded through async loading are required by runtime
		Package->SetLoadedByEditorPropertiesOnly(false);
#endif

		// if the linker already exists, we don't need to lookup the file (it may have been pre-created with
		// a different filename)
		Linker = FLinkerLoad::FindExistingLinkerForPackage(Package);

		if (!Linker)
		{
			// Allow delegates to resolve this path
			FString NameToLoad = FPackageName::GetDelegateResolvedPackagePath(Desc.NameToLoad.ToString());

			const FGuid* const Guid = Desc.Guid.IsValid() ? &Desc.Guid : nullptr;

			FString PackageFileName;
			const bool DoesPackageExist = FPackageName::DoesPackageExist(NameToLoad, Guid, &PackageFileName);

			if (Desc.NameToLoad == NAME_None ||
				(!GetConvertedDynamicPackageNameToTypeName().Contains(Desc.Name) &&
				!DoesPackageExist))
			{
				UE_LOG(LogStreaming, Error, TEXT("Couldn't find file for package %s requested by async loading code. NameToLoad: %s"), *Desc.Name.ToString(), *Desc.NameToLoad.ToString());
				bLoadHasFailed = true;
				return EAsyncPackageState::TimeOut;
			}

			// Create raw async linker, requiring to be ticked till finished creating.
			uint32 LinkerFlags = LOAD_None;
			if (FApp::IsGame() && !GIsEditor)
			{
				LinkerFlags |= (LOAD_SeekFree | LOAD_NoVerify);
			}
#if WITH_EDITOR
			else if ((Desc.PackageFlags & PKG_PlayInEditor) != 0)
			{
				LinkerFlags |= LOAD_PackageForPIE;
			}
#endif
			Linker = FLinkerLoad::CreateLinkerAsync(Package, *PackageFileName, LinkerFlags);
		}

		// Associate this async package with the linker
		check(Linker->AsyncRoot == nullptr || Linker->AsyncRoot == this);
		Linker->AsyncRoot = this;

		UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::CreateLinker for %s finished."), *Desc.NameToLoad.ToString());
	}
	return EAsyncPackageState::Complete;
}

/**
 * Finalizes linker creation till time limit is exceeded.
 *
 * @return true if linker is finished being created, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::FinishLinker()
{
	SCOPED_LOADTIMER(FinishLinkerTime);
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	if (Linker && !Linker->HasFinishedInitialization())
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FinishLinker);
		LastObjectWorkWasPerformedOn	= LinkerRoot;
		LastTypeOfWorkPerformed			= TEXT("ticking linker");
	
		const float RemainingTimeLimit = TimeLimit - (float)(FPlatformTime::Seconds() - TickStartTime);

		// Operation still pending if Tick returns false
		FLinkerLoad::ELinkerStatus LinkerResult = Linker->Tick(RemainingTimeLimit, bUseTimeLimit, bUseFullTimeLimit);
		if (LinkerResult != FLinkerLoad::LINKER_Loaded)
		{
			// Give up remainder of timeslice if there is one to give up.
			GiveUpTimeSlice();
			Result = EAsyncPackageState::TimeOut;
			if (LinkerResult == FLinkerLoad::LINKER_Failed)
			{
				// If linker failed we exit with EAsyncPackageState::TimeOut to skip all the remaining steps.
				// The error will be handled as bLoadHasFailed will be true.
				bLoadHasFailed = true;
			}
		}
	}

	return Result;
}

/**
 * Find a package by name.
 * 
 * @param Dependencies package list.
 * @param PackageName long package name.
 * @return Index into the array if the package was found, otherwise INDEX_NONE
 */
FORCEINLINE int32 ContainsDependencyPackage(TArray<FAsyncPackage*>& Dependencies, const FName& PackageName)
{
	for (int32 Index = 0; Index < Dependencies.Num(); ++Index)
	{
		if (Dependencies[Index]->GetPackageName() == PackageName)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}


/**
	* Adds a package to the list of pending import packages.
	*
	* @param PendingImport Name of the package imported either directly or by one of the imported packages
	*/
void FAsyncPackage::AddImportDependency(const FName& PendingImport)
{
	FAsyncPackage* PackageToStream = FAsyncLoadingThread::Get().FindAsyncPackage(PendingImport);
	if (!PackageToStream)
	{
		const FAsyncPackageDesc Info(INDEX_NONE, PendingImport);
		PackageToStream = new FAsyncPackage(Info);

		// If priority of the dependency is not set, inherit from parent.
		if (PackageToStream->Desc.Priority == 0)
		{
			PackageToStream->Desc.Priority = Desc.Priority;
		}
		FAsyncLoadingThread::Get().InsertPackage(PackageToStream);
	}
	
	if (!PackageToStream->HasFinishedLoading() && 
		!PackageToStream->bLoadHasFailed)
	{
		const bool bInternalCallback = true;
		PackageToStream->AddCompletionCallback(FLoadPackageAsyncDelegate::CreateRaw(this, &FAsyncPackage::ImportFullyLoadedCallback), bInternalCallback);
		PackageToStream->DependencyRefCount.Increment();
		PendingImportedPackages.Add(PackageToStream);
	}
	else
	{
		PackageToStream->DependencyRefCount.Increment();
		ReferencedImports.Add(PackageToStream);
	}
}


/**
 * Adds a unique package to the list of packages to wait for until their linkers have been created.
 *
 * @param PendingImport Package imported either directly or by one of the imported packages
 */
bool FAsyncPackage::AddUniqueLinkerDependencyPackage(FAsyncPackage& PendingImport)
{
	if (ContainsDependencyPackage(PendingImportedPackages, PendingImport.GetPackageName()) == INDEX_NONE)
	{
		FLinkerLoad* PendingImportLinker = PendingImport.Linker;
		if (PendingImportLinker == nullptr || !PendingImportLinker->HasFinishedInitialization())
		{
			AddImportDependency(PendingImport.GetPackageName());
			UE_LOG(LogStreaming, Verbose, TEXT("  Adding linker dependency %s"), *PendingImport.GetPackageName().ToString());
		}
		else if (this != &PendingImport)
		{
			return false;
		}
	}
	return true;
}

/**
 * Adds dependency tree to the list if packages to wait for until their linkers have been created.
 *
 * @param ImportedPackage Package imported either directly or by one of the imported packages
 */
void FAsyncPackage::AddDependencyTree(FAsyncPackage& ImportedPackage, TSet<FAsyncPackage*>& SearchedPackages)
{
	if (SearchedPackages.Contains(&ImportedPackage))
	{
		// we've already searched this package
		return;
	}
	for (int32 Index = 0; Index < ImportedPackage.PendingImportedPackages.Num(); ++Index)
	{
		FAsyncPackage& PendingImport = *ImportedPackage.PendingImportedPackages[Index];
		if (!AddUniqueLinkerDependencyPackage(PendingImport))
		{
			AddDependencyTree(PendingImport, SearchedPackages);
		}
	}
	// Mark this package as searched
	SearchedPackages.Add(&ImportedPackage);
}

/** 
 * Load imports till time limit is exceeded.
 *
 * @return true if we finished load all imports, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::LoadImports()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_LoadImports);
	LastObjectWorkWasPerformedOn	= LinkerRoot;
	LastTypeOfWorkPerformed			= TEXT("loading imports");
	
	// GC can't run in here
	FGCScopeGuard GCGuard;

	// Create imports.
	while (LoadImportIndex < Linker->ImportMap.Num() && !IsTimeLimitExceeded())
	{
		// Get the package for this import
		const FObjectImport* Import = &Linker->ImportMap[LoadImportIndex++];

		while (Import->OuterIndex.IsImport())
		{
			Import = &Linker->Imp(Import->OuterIndex);
		}
		check(Import->OuterIndex.IsNull());

		// @todo: why do we need this? some UFunctions have null outer in the linker.
		if (Import->ClassName != NAME_Package)
		{
			continue;
		}
			

		// Don't try to import a package that is in an import table that we know is an invalid entry
		if (FLinkerLoad::KnownMissingPackages.Contains(Import->ObjectName))
		{
			continue;
		}

		// Our import package name is the import name
		const FName ImportPackageFName(Import->ObjectName);

		// Handle circular dependencies - try to find existing packages.
		UPackage* ExistingPackage = dynamic_cast<UPackage*>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, ImportPackageFName, true));
		if (ExistingPackage  && !ExistingPackage->bHasBeenFullyLoaded 
			&& (!ExistingPackage->HasAnyPackageFlags(PKG_CompiledIn) || GetConvertedDynamicPackageNameToTypeName().Contains(ImportPackageFName)))//!ExistingPackage->HasAnyFlags(RF_WasLoaded))
		{
			// The import package already exists. Check if it's currently being streamed as well. If so, make sure
			// we add all dependencies that don't yet have linkers created otherwise we risk that if the current package
			// doesn't depend on any other packages that have not yet started streaming, creating imports is going
			// to load packages blocking the main thread.
			FAsyncPackage* PendingPackage = FAsyncLoadingThread::Get().FindAsyncPackage(ImportPackageFName);
			if (PendingPackage)
			{
				FLinkerLoad* PendingPackageLinker = PendingPackage->Linker;
				if (PendingPackageLinker == nullptr || !PendingPackageLinker->HasFinishedInitialization())
				{
					// Add this import to the dependency list.
					AddUniqueLinkerDependencyPackage(*PendingPackage);
				}
				else
				{
					UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Linker exists for %s"), *Desc.NameToLoad.ToString(), *ImportPackageFName.ToString());
					// Only keep a reference to this package so that its linker doesn't go away too soon
					PendingPackage->DependencyRefCount.Increment();
					ReferencedImports.Add(PendingPackage);
					// Check if we need to add its dependencies too.
					TSet<FAsyncPackage*> SearchedPackages;
					AddDependencyTree(*PendingPackage, SearchedPackages);
				}
			}
		}

		if (!ExistingPackage && ContainsDependencyPackage(PendingImportedPackages, ImportPackageFName) == INDEX_NONE)
		{
			const FString ImportPackageName(Import->ObjectName.ToString());
			// The package doesn't exist and this import is not in the dependency list so add it now.
			if (!FPackageName::IsShortPackageName(ImportPackageName))
			{
				UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Loading %s"), *Desc.NameToLoad.ToString(), *ImportPackageName);
				AddImportDependency(ImportPackageFName);
#if 0 && USE_NEW_ASYNC_IO
				LoadImportIndex--;
				return EAsyncPackageState::TimeOut; // lets go load that package now so we can do it in order.
#endif
			}
			else
			{
				// This usually means there's a reference to a script package from another project
				UE_LOG(LogStreaming, Warning, TEXT("FAsyncPackage::LoadImports for %s: Short package name in imports list: %s"), *Desc.NameToLoad.ToString(), *ImportPackageName);
			}
		}
		UpdateLoadPercentage();
	}
			
	
	if (PendingImportedPackages.Num())
	{
		GiveUpTimeSlice();
		return EAsyncPackageState::PendingImports;
	}
	return LoadImportIndex == Linker->ImportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

/**
 * Function called when pending import package has been fully loaded.
 */
void FAsyncPackage::ImportFullyLoadedCallback(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
	if (Result != EAsyncLoadingResult::Canceled)
	{
		UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Loaded %s"), *Desc.NameToLoad.ToString(), *InPackageName.ToString());
		int32 Index = ContainsDependencyPackage(PendingImportedPackages, InPackageName);
		check(Index != INDEX_NONE);
		// Keep a reference to this package so that its linker doesn't go away too soon
		ReferencedImports.Add(PendingImportedPackages[Index]);
		PendingImportedPackages.RemoveAt(Index);
	}
}

/** 
 * Create imports till time limit is exceeded.
 *
 * @return true if we finished creating all imports, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::CreateImports()
{
	SCOPED_LOADTIMER(CreateImportsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateImports);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	// Create imports.
	while( ImportIndex < Linker->ImportMap.Num() && !IsTimeLimitExceeded() )
	{
 		UObject* Object	= Linker->CreateImport( ImportIndex++ );
		LastObjectWorkWasPerformedOn	= Object;
		LastTypeOfWorkPerformed			= TEXT("creating imports for");

		// Make sure this object is not claimed by GC if it's triggered while streaming.
		FAsyncObjectsReferencer::Get().AddObject(Object);
	}

	return ImportIndex == Linker->ImportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

/**
 * Checks if all async texture allocations for this package have been completed.
 *
 * @return true if all texture allocations have been completed, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::FinishTextureAllocations()
{
	//@TODO: Cancel allocations if they take too long.
#if WITH_ENGINE
	bool bHasCompleted = Linker->Summary.TextureAllocations.HasCompleted();
	if ( !bHasCompleted )
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FinishTextureAllocations);
		if ( bUseTimeLimit && !bUseFullTimeLimit)
		{
			// Try again next Tick instead.
			GiveUpTimeSlice();
		}
		else
		{
			// Need to finish right now. Cancel async allocations that haven't finished yet.
			// Those will be allocated immediately by UTexture2D during serialization instead.
			Linker->Summary.TextureAllocations.CancelRemainingAllocations( false );
			bHasCompleted = true;
		}
	}
	return bHasCompleted ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
#else
	return EAsyncPackageState::Complete;
#endif		// WITH_ENGINE
}

#if WITH_EDITORONLY_DATA
/**
* Creates and loads meta-data for the package.
*
* @return true if we finished creating meta-data, false otherwise.
*/
EAsyncPackageState::Type FAsyncPackage::CreateMetaData()
{
	SCOPED_LOADTIMER(CreateMetaDataTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateMetaData);

	if (!MetaDataIndex.IsSet())
	{
		checkSlow(!FPlatformProperties::RequiresCookedData());
		MetaDataIndex = Linker->LoadMetaDataFromExportMap();
	}

	return EAsyncPackageState::Complete;
}
#endif // WITH_EDITORONLY_DATA

/**
 * Create exports till time limit is exceeded.
 *
 * @return true if we finished creating and preloading all exports, false otherwise.
 */
EAsyncPackageState::Type FAsyncPackage::CreateExports()
{
	SCOPED_LOADTIMER(CreateExportsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateExports);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	// Create exports.
	while( ExportIndex < Linker->ExportMap.Num() && !IsTimeLimitExceeded() )
	{
#if WITH_EDITORONLY_DATA
		checkf(MetaDataIndex.IsSet(), TEXT("FAsyncPackage::CreateExports called before FAsyncPackage::CreateMetaData!"));
		if (ExportIndex == MetaDataIndex.GetValue())
		{
			++ExportIndex;
			continue;
		}
#endif // WITH_EDITORONLY_DATA

		const FObjectExport& Export = Linker->ExportMap[ExportIndex];
		// Precache data and see whether it's already finished.
		bool bReady;
#if USE_NEW_ASYNC_IO
		FArchiveAsync2* FAA2 = Linker->GetFArchiveAsync2Loader();
		if (FAA2)
		{
			bReady = FAA2->Precache(Export.SerialOffset, Export.SerialSize, bUseTimeLimit, bUseFullTimeLimit, TickStartTime, TimeLimit, false);
		}
		else
#endif
		{
			bReady = Linker->Precache(Export.SerialOffset, Export.SerialSize);
		}
		if (bReady)
		{
			// Create the object...
			UObject* Object	= Linker->CreateExport( ExportIndex++ );
			// ... and preload it.
			if( Object )
			{				
				// This will cause the object to be serialized. We do this here for all objects and
				// not just UClass and template objects, for which this is required in order to ensure
				// seek free loading, to be able introduce async file I/O.
				Linker->Preload( Object );
			}
			LastObjectWorkWasPerformedOn	= Object;
			LastTypeOfWorkPerformed = TEXT("creating exports for");
				
			UpdateLoadPercentage();
		}
		// Data isn't ready yet. Give up remainder of time slice if we're not using a time limit.
		else if (GiveUpTimeSlice())
		{
			INC_FLOAT_STAT_BY(STAT_AsyncIO_AsyncPackagePrecacheWaitTime, (float)FApp::GetDeltaTime());
			return EAsyncPackageState::TimeOut;
		}
	}
	
	// We no longer need the referenced packages.
	FreeReferencedImports();

	return ExportIndex == Linker->ExportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

/**
 * Removes references to any imported packages.
 */
void FAsyncPackage::FreeReferencedImports()
{	
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FreeReferencedImports);	

	for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencedImports.Num(); ++ReferenceIndex)
	{
		FAsyncPackage& Ref = *ReferencedImports[ReferenceIndex];
		Ref.DependencyRefCount.Decrement();
		UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::FreeReferencedImports for %s: Releasing %s (%d)"), *Desc.NameToLoad.ToString(), *Ref.GetPackageName().ToString(), Ref.GetDependencyRefCount());
		check(Ref.DependencyRefCount.GetValue() >= 0);
	}
	ReferencedImports.Empty();
}

/**
 * Preloads aka serializes all loaded objects.
 *
 * @return true if we finished serializing all loaded objects, false otherwise.
 */

#if 0 && USE_NEW_ASYNC_IO

// this can sort exports before preloading them
struct FPreloadSort
{
	UObject* Object;
	FLinkerLoad* Linker;
	int32 SerialOffset;

	FPreloadSort(UObject* InObject)
		: Object(InObject)
	{
		if (Object)
		{
			Linker = Object->GetLinker();
			if (Linker)
			{
				SerialOffset = Linker->ExportMap[Object->GetLinkerIndex()].SerialOffset;
			}
			else
			{
				SerialOffset = 0;
			}
		}
		else
		{
			Linker = nullptr;
			SerialOffset = 0;
		}
	}
};

TArray<UObject*>* GilObjLoaded = nullptr;

FString GilFullName;

EAsyncPackageState::Type FAsyncPackage::PreLoadObjects()
{
	SCOPED_LOADTIMER(PreLoadObjectsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PreLoadObjects);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	TArray<UObject*>& ObjLoaded = FUObjectThreadContext::Get().ObjLoaded;
	GilObjLoaded = &ObjLoaded;

	// Preload (aka serialize) the objects.
	while (PreLoadIndex < ObjLoaded.Num() && !IsTimeLimitExceeded())
	{
		if ( //PreLoadSortIndex <= PreLoadIndex && 
			ObjLoaded.Num() > PreLoadSortIndex)
		{
			int32 SortFrom = PreLoadIndex;
			FLinkerLoad* LastLinker = PreLoadIndex > 0 ? ObjLoaded[PreLoadIndex - 1]->GetLinker() : nullptr;

			auto SortPredicate =
				[LastLinker](const FPreloadSort& LHS, const FPreloadSort& RHS) -> bool
			{
				if (LastLinker)
				{
					// if we have a last linker, then those are always first
					if (LHS.Linker == LastLinker && RHS.Linker != LastLinker)
					{
						return true;
					}
					if (LHS.Linker != LastLinker && RHS.Linker == LastLinker)
					{
						return false;
					}
				}
				if (LHS.Linker && !RHS.Linker)
				{
					return true; // null linkers are last, maybe they will be non-null later
				}
				if (!LHS.Linker && RHS.Linker)
				{
					return false; // null linkers are last, maybe they will be non-null later
				}
				if (LHS.Linker != RHS.Linker)
				{
					return UPTRINT(LHS.Linker) < UPTRINT(RHS.Linker); // arbitrary order, based on the pointer
				}
				check(LHS.Linker == RHS.Linker && LHS.Linker);
				return LHS.SerialOffset < RHS.SerialOffset; // in disk order
			};

			TArray<FPreloadSort> SortedArrayTail;
			SortedArrayTail.Reserve(ObjLoaded.Num() - SortFrom);
			for (int32 Index = SortFrom; Index < ObjLoaded.Num(); Index++)
			{
				SortedArrayTail.Emplace(ObjLoaded[Index]);
			}
			SortedArrayTail.Sort(SortPredicate);
			{
				int32 NumTail = SortedArrayTail.Num();
				for (int32 Index = 0; Index < NumTail; Index++)
				{
					FPreloadSort& Item = SortedArrayTail[Index];
					ObjLoaded[SortFrom + Index] = Item.Object;
				}
			}
			PreLoadSortIndex = ObjLoaded.Num();
		}
		UObject* Object = ObjLoaded[PreLoadIndex];
		if (Object && Object->HasAnyFlags(RF_NeedLoad))
		{
			FLinkerLoad* ObjectLinker = Object->GetLinker();
			if (ObjectLinker)
			{
				const FObjectExport& Export = ObjectLinker->ExportMap[Object->GetLinkerIndex()];
				GilFullName = Object->GetPathName();
				FArchiveAsync2* FAA2 = ObjectLinker->GetFArchiveAsync2Loader();
				bool bReady;
				if (FAA2)
				{
					bReady = FAA2->Precache(Export.SerialOffset, Export.SerialSize, bUseTimeLimit, bUseFullTimeLimit, TickStartTime, TimeLimit);
				}
				else
				{
					bReady = Linker->Precache(Export.SerialOffset, Export.SerialSize);
				}
				if (bReady)
				{
					ObjectLinker->Preload(Object);
					LastObjectWorkWasPerformedOn = Object;
					LastTypeOfWorkPerformed = TEXT("preloading");
				}
				// Data isn't ready yet. Give up remainder of time slice if we're not using a time limit.
				else if (GiveUpTimeSlice())
				{
					INC_FLOAT_STAT_BY(STAT_AsyncIO_AsyncPackagePrecacheWaitTime, (float)FApp::GetDeltaTime());
					return EAsyncPackageState::TimeOut;
				}
			}
		}
		PreLoadIndex++;
	}
	return PreLoadIndex == ObjLoaded.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

#elif USE_NEW_ASYNC_IO

EAsyncPackageState::Type FAsyncPackage::PreLoadObjects()
{
	SCOPED_LOADTIMER(PreLoadObjectsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PreLoadObjects);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	auto& ObjLoaded = FUObjectThreadContext::Get().ObjLoaded;
	// Preload (aka serialize) the objects.
	while (PreLoadIndex < ObjLoaded.Num() && !IsTimeLimitExceeded())
	{
		UObject* Object = ObjLoaded[PreLoadIndex];
		if (Object && Object->HasAnyFlags(RF_NeedLoad))
		{
			FLinkerLoad* ObjectLinker = Object->GetLinker();
			if (ObjectLinker)
			{
				const FObjectExport& Export = ObjectLinker->ExportMap[Object->GetLinkerIndex()];
				FArchiveAsync2* FAA2 = ObjectLinker->GetFArchiveAsync2Loader();
				bool bReady;
				if (FAA2)
				{
					bReady = FAA2->Precache(Export.SerialOffset, Export.SerialSize, bUseTimeLimit, bUseFullTimeLimit, TickStartTime, TimeLimit, false);
				}
				else
				{
					bReady = Linker->Precache(Export.SerialOffset, Export.SerialSize);
				}
				if (bReady)
				{
					ObjectLinker->Preload(Object);
					LastObjectWorkWasPerformedOn = Object;
					LastTypeOfWorkPerformed = TEXT("preloading");
				}
			}
		}
		PreLoadIndex++;
	}

	return PreLoadIndex == ObjLoaded.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

#else

EAsyncPackageState::Type FAsyncPackage::PreLoadObjects()
{
	SCOPED_LOADTIMER(PreLoadObjectsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PreLoadObjects);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	auto& ObjLoaded = FUObjectThreadContext::Get().ObjLoaded;
	// Preload (aka serialize) the objects.
	while (PreLoadIndex < ObjLoaded.Num() && !IsTimeLimitExceeded())
	{
		//@todo async: make this part async as well.
		UObject* Object = ObjLoaded[PreLoadIndex++];
		if (Object && Object->GetLinker())
		{
			Object->GetLinker()->Preload(Object);
			LastObjectWorkWasPerformedOn = Object;
			LastTypeOfWorkPerformed = TEXT("preloading");
		}
	}

	return PreLoadIndex == ObjLoaded.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}
#endif

/**
 * Route PostLoad to all loaded objects. This might load further objects!
 *
 * @return true if we finished calling PostLoad on all loaded objects and no new ones were created, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::PostLoadObjects()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PostLoadObjects);
	
	SCOPED_LOADTIMER(PostLoadObjectsTime);
	
	// GC can't run in here
	FGCScopeGuard GCGuard;

	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	TGuardValue<bool> GuardIsRoutingPostLoad(FUObjectThreadContext::Get().IsRoutingPostLoad, true);

	auto& ObjLoaded = FUObjectThreadContext::Get().ObjLoaded;
	// PostLoad objects.
	while (PostLoadIndex < ObjLoaded.Num() && PostLoadIndex < PreLoadIndex && !IsTimeLimitExceeded())
	{
		UObject* Object = ObjLoaded[PostLoadIndex++];
		check(Object);
		if (!FAsyncLoadingThread::Get().IsMultithreaded() || Object->IsPostLoadThreadSafe())
		{
			FScopeCycleCounterUObject ConstructorScope(Object, GET_STATID(STAT_FAsyncPackage_PostLoadObjects));

			Object->ConditionalPostLoad();

			LastObjectWorkWasPerformedOn = Object;
			LastTypeOfWorkPerformed = TEXT("postloading_async");
		}
		else
		{
			DeferredPostLoadObjects.Add(Object);
		}
		// All object must be finalized on the game thread
		DeferredFinalizeObjects.Add(Object);
		check(Object->IsValidLowLevelFast());
		// Make sure all objects in DeferredFinalizeObjects are referenced too
		FAsyncObjectsReferencer::Get().AddObject(Object);
	}

	// New objects might have been loaded during PostLoad.
	Result = (PreLoadIndex == ObjLoaded.Num()) && (PostLoadIndex == ObjLoaded.Num()) ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;

	return Result;
}

void CreateClustersFromPackage(FLinkerLoad* PackageLinker);

EAsyncPackageState::Type FAsyncPackage::PostLoadDeferredObjects(double InTickStartTime, bool bInUseTimeLimit, float& InOutTimeLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PostLoadObjectsGameThread);
	SCOPED_LOADTIMER(PostLoadDeferredObjectsTime);

	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	TGuardValue<bool> GuardIsRoutingPostLoad(FUObjectThreadContext::Get().IsRoutingPostLoad, true);
	FAsyncLoadingTickScope InAsyncLoadingTick;

	LastObjectWorkWasPerformedOn = nullptr;
	LastTypeOfWorkPerformed = TEXT("postloading_gamethread");

	TArray<UObject*>& ObjLoadedInPostLoad = FUObjectThreadContext::Get().ObjLoaded;
	TArray<UObject*> ObjLoadedInPostLoadLocal;

	while (DeferredPostLoadIndex < DeferredPostLoadObjects.Num() && 
		!AsyncLoadingThread.IsAsyncLoadingSuspended() &&
		!::IsTimeLimitExceeded(InTickStartTime, bInUseTimeLimit, InOutTimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn))
	{
		UObject* Object = DeferredPostLoadObjects[DeferredPostLoadIndex++];
		check(Object);

		FScopeCycleCounterUObject ConstructorScope(Object, GET_STATID(STAT_FAsyncPackage_PostLoadObjectsGameThread));

		Object->ConditionalPostLoad();

		if (ObjLoadedInPostLoad.Num())
		{
			// If there were any LoadObject calls inside of PostLoad, we need to pre-load those objects here. 
			// There's no going back to the async tick loop from here.
			UE_LOG(LogStreaming, Warning, TEXT("Detected %d objects loaded in PostLoad while streaming, this may cause hitches as we're blocking async loading to pre-load them."), ObjLoadedInPostLoad.Num());
			
			// Copy to local array because ObjLoadedInPostLoad can change while we're iterating over it
			ObjLoadedInPostLoadLocal.Append(ObjLoadedInPostLoad);
			ObjLoadedInPostLoad.Reset();

			while (ObjLoadedInPostLoadLocal.Num())
			{
				// Make sure all objects loaded in PostLoad get post-loaded too
				DeferredPostLoadObjects.Append(ObjLoadedInPostLoadLocal);

				// Preload (aka serialize) the objects loaded in PostLoad.
				for (UObject* PreLoadObject : ObjLoadedInPostLoadLocal)
				{
					if (PreLoadObject && PreLoadObject->GetLinker())
					{
						PreLoadObject->GetLinker()->Preload(PreLoadObject);
					}
				}

				// Other objects could've been loaded while we were preloading, continue until we've processed all of them.
				ObjLoadedInPostLoadLocal.Reset();
				ObjLoadedInPostLoadLocal.Append(ObjLoadedInPostLoad);
				ObjLoadedInPostLoad.Reset();
			}			
		}

		LastObjectWorkWasPerformedOn = Object;		

		UpdateLoadPercentage();
	}

	// New objects might have been loaded during PostLoad.
	Result = (DeferredPostLoadIndex == DeferredPostLoadObjects.Num()) ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
	if (Result == EAsyncPackageState::Complete)
	{
		TArray<UObject*> CDODefaultSubobjects;
		// Clear async loading flags (we still want RF_Async, but EInternalObjectFlags::AsyncLoading can be cleared)
		for (UObject* Object : DeferredFinalizeObjects)
		{
			if (Object)
			{
				Object->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading);
			}

			// CDO need special handling, no matter if it's listed in DeferredFinalizeObjects or created here for DynamicClass
			UObject* CDOToHandle = nullptr;

			// Dynamic Class doesn't require/use pre-loading (or post-loading). 
			// The CDO is created at this point, because now it's safe to solve cyclic dependencies.
			if (UDynamicClass* DynamicClass = Cast<UDynamicClass>(Object))
			{
				UObject* OldCDO = DynamicClass->GetDefaultObject(false);
				UObject* NewCDO = DynamicClass->GetDefaultObject(true);
				const bool bCDOWasJustCreated = (OldCDO != NewCDO);
				if (bCDOWasJustCreated && (NewCDO != nullptr))
				{
					NewCDO->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading);
					CDOToHandle = NewCDO;
				}
			}
			else
			{
				CDOToHandle = ((Object != nullptr) && Object->HasAnyFlags(RF_ClassDefaultObject)) ? Object : nullptr;
			}

			// Clear AsyncLoading in CDO's subobjects.
			if(CDOToHandle != nullptr)
			{
				CDOToHandle->GetDefaultSubobjects(CDODefaultSubobjects);
				for (UObject* SubObject : CDODefaultSubobjects)
				{
					if (SubObject && SubObject->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
					{
						SubObject->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading);
					}
				}
				CDODefaultSubobjects.Reset();
			}
		}

		// Mark package as having been fully loaded and update load time.
		if (LinkerRoot && !bLoadHasFailed)
		{
			LinkerRoot->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading);
			LinkerRoot->MarkAsFullyLoaded();			
			LinkerRoot->SetLoadTime(FPlatformTime::Seconds() - LoadStartTime);

			if (Linker)
			{
				CreateClustersFromPackage(Linker);

				// give a hint to the IO system that we are done with this file for now
				FIOSystem::Get().HintDoneWithFile(Linker->Filename);
			}
		}
	}

	return Result;
}

/**
 * Finish up objects and state, which means clearing the EInternalObjectFlags::AsyncLoading flag on newly created ones
 *
 * @return true
 */
EAsyncPackageState::Type FAsyncPackage::FinishObjects()
{
	SCOPED_LOADTIMER(FinishObjectsTime);

	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FinishObjects);
	LastObjectWorkWasPerformedOn	= nullptr;
	LastTypeOfWorkPerformed			= TEXT("finishing all objects");
		
	auto& LoadingGlobals = FUObjectThreadContext::Get();

	EAsyncLoadingResult::Type LoadingResult;
	if (!bLoadHasFailed)
	{
		LoadingGlobals.ObjLoaded.Empty();
		LoadingResult = EAsyncLoadingResult::Succeeded;
	}
	else
	{
		// Cleanup objects from this package only
		for (int32 ObjectIndex = LoadingGlobals.ObjLoaded.Num() - 1; ObjectIndex >= 0; --ObjectIndex)
		{
			UObject* Object = LoadingGlobals.ObjLoaded[ObjectIndex];
			if (Object->GetOutermost()->GetFName() == Desc.Name)
			{
				Object->ClearFlags(RF_NeedPostLoad | RF_NeedLoad | RF_NeedPostLoadSubobjects);
				Object->MarkPendingKill();
				LoadingGlobals.ObjLoaded.RemoveAt(ObjectIndex);
			}
		}
		LoadingResult = EAsyncLoadingResult::Failed;
	}

	// Simulate what EndLoad does.
	FLinkerManager::Get().DissociateImportsAndForcedExports(); //@todo: this should be avoidable
	PreLoadIndex = 0;
	PreLoadSortIndex = 0;
	PostLoadIndex = 0;
	
	// Close any linkers that have been open as a result of blocking load while async loading
	TArray<FLinkerLoad*> PackagesToClose = MoveTemp(FUObjectThreadContext::Get().DelayedLinkerClosePackages);
	for (FLinkerLoad* LinkerToClose : PackagesToClose)
	{
		check(LinkerToClose);
		check(LinkerToClose->LinkerRoot);
		FLinkerManager::Get().ResetLoaders(LinkerToClose->LinkerRoot);
		check(LinkerToClose->LinkerRoot == nullptr);
		check(LinkerToClose->AsyncRoot == nullptr);
	}

	// If we successfully loaded
	if (!bLoadHasFailed && Linker)
	{
	#if WITH_ENGINE
		// Cancel all texture allocations that haven't been claimed yet.
		Linker->Summary.TextureAllocations.CancelRemainingAllocations( true );
	#endif		// WITH_ENGINE
	}

	if (Linker)
	{
		// Flush linker cache now to reduce peak memory usage (5.5-10x)
		// We shouldn't need it anyway at this point and even if something attempts to read in PostLoad, 
		// we're just going to re-cache then.
#if USE_NEW_ASYNC_IO && !UE_BUILD_SHIPPING
		if (FPlatformProperties::RequiresCookedData())
		{
			int32 TotalJunk = 0;
			for (int32 Index = 0; Index < Linker->ExportMap.Num(); Index++)
			{
				if (!Linker->ExportMap[Index].Object)
				{
					UE_LOG(LogStreaming, Warning, TEXT("Export %d (%s) in %s was not created."), Index, *Linker->ExportMap[Index].ObjectName.ToString(), *Linker->Filename);
					TotalJunk += Linker->ExportMap[Index].SerialSize;
				}
				else if (Linker->ExportMap[Index].Object->HasAnyFlags(RF_NeedLoad))
				{
					UE_LOG(LogStreaming, Warning, TEXT("Export %d (%s) in %s still had RF_NeedLoad."), Index, *Linker->ExportMap[Index].ObjectName.ToString(), *Linker->Filename);
				}
			}
			if (TotalJunk)
			{
				UE_LOG(LogStreaming, Warning, TEXT("File %s had %dk of exports we never loaded."), *Linker->Filename, (TotalJunk + 1023) / 1024);
			}
		}
#endif
		Linker->FlushCache();
	}

	{
		const bool bInternalCallbacks = true;
		CallCompletionCallbacks(bInternalCallbacks, LoadingResult);
	}

	return EAsyncPackageState::Complete;
}

void FAsyncPackage::CallCompletionCallbacks(bool bInternal, EAsyncLoadingResult::Type LoadingResult)
{
	UPackage* LoadedPackage = (!bLoadHasFailed) ? LinkerRoot : nullptr;
	for (auto& CompletionCallback : CompletionCallbacks)
	{
		if (CompletionCallback.bIsInternal == bInternal)
		{
			CompletionCallback.Callback.ExecuteIfBound(Desc.Name, LoadedPackage, LoadingResult);
		}
	}
}

void FAsyncPackage::Cancel()
{
	// Call any completion callbacks specified.
	const EAsyncLoadingResult::Type Result = EAsyncLoadingResult::Canceled;
	for (int32 CallbackIndex = 0; CallbackIndex < CompletionCallbacks.Num(); CallbackIndex++)
	{
		CompletionCallbacks[CallbackIndex].Callback.ExecuteIfBound(Desc.Name, nullptr, Result);
	}
	bLoadHasFailed = true;
	if (LinkerRoot)
	{
		if (Linker)
		{
			// give a hint to the IO system that we are done with this file for now
			FIOSystem::Get().HintDoneWithFile(Linker->Filename);
			Linker->FlushCache();
		}
		LinkerRoot->ClearFlags(RF_WasLoaded);
		LinkerRoot->bHasBeenFullyLoaded = false;
		LinkerRoot->Rename(*MakeUniqueObjectName(GetTransientPackage(), UPackage::StaticClass()).ToString(), nullptr, REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders | REN_NonTransactional);
		DetachLinker();
	}
	PreLoadIndex = 0;
	PreLoadSortIndex = 0;
}

void FAsyncPackage::AddCompletionCallback(const FLoadPackageAsyncDelegate& Callback, bool bInternal)
{
	// This is to ensure that there is no one trying to subscribe to a already loaded package
	//check(!bLoadHasFinished && !bLoadHasFailed);
	CompletionCallbacks.Add(FCompletionCallback(bInternal, Callback));
}

void FAsyncPackage::UpdateLoadPercentage()
{
	// PostLoadCount is just an estimate to prevent packages to go to 100% too quickly
	// We may never reach 100% this way, but it's better than spending most of the load package time at 100%
	float NewLoadPercentage = 0.0f;
	if (Linker)
	{
		const int32 PostLoadCount = FMath::Max(DeferredPostLoadObjects.Num(), Linker->ImportMap.Num());
		NewLoadPercentage = 100.f * (LoadImportIndex + ExportIndex + DeferredPostLoadIndex) / (Linker->ExportMap.Num() + Linker->ImportMap.Num() + PostLoadCount);		
	}
	else if (DeferredPostLoadObjects.Num() > 0)
	{
		NewLoadPercentage = static_cast<float>(DeferredPostLoadIndex) / DeferredPostLoadObjects.Num();
	}
	// It's also possible that we got so many objects to PostLoad that LoadPercantage will actually drop
	LoadPercentage = FMath::Max(NewLoadPercentage, LoadPercentage);
}

int32 LoadPackageAsync(const FString& InName, const FGuid* InGuid /*= nullptr*/, const TCHAR* InPackageToLoadFrom /*= nullptr*/, FLoadPackageAsyncDelegate InCompletionDelegate /*= FLoadPackageAsyncDelegate()*/, EPackageFlags InPackageFlags /*= PKG_None*/, int32 InPIEInstanceID /*= INDEX_NONE*/, int32 InPackagePriority /*= 0*/)
{
#if !WITH_EDITOR
	if (GPreloadPackageDependencies)
	{
		// If dependency preloading is enabled, we need to force the asset registry module to be loaded on the game thread
		// as it will potentiall be used on the async loading thread, which isn't allowed to load modules.
		// We could do this at init time, but doing it here allows us to not load the module at all if preloading is
		// disabled.
		IAssetRegistryInterface::GetPtr();
	}
#endif

	// The comments clearly state that it should be a package name but we also handle it being a filename as this function is not perf critical
	// and LoadPackage handles having a filename being passed in as well.
	FString PackageName;
	if (FPackageName::IsValidLongPackageName(InName, /*bIncludeReadOnlyRoots*/true))
	{
		PackageName = InName;
	}
	// PackageName got populated by the conditional function
	else if (!(FPackageName::IsPackageFilename(InName) && FPackageName::TryConvertFilenameToLongPackageName(InName, PackageName)))
	{
		// PackageName will get populated by the conditional function
		FString ClassName;
		if (!FPackageName::ParseExportTextPath(PackageName, &ClassName, &PackageName))
		{
			UE_LOG(LogStreaming, Fatal, TEXT("LoadPackageAsync failed to begin to load a package because the supplied package name was neither a valid long package name nor a filename of a map within a content folder: '%s'"), *PackageName);
		}
	}

	FString PackageNameToLoad(InPackageToLoadFrom);
	if (PackageNameToLoad.IsEmpty())
	{
		PackageNameToLoad = PackageName;
	}
	// Make sure long package name is passed to FAsyncPackage so that it doesn't attempt to 
	// create a package with short name.
	if (FPackageName::IsShortPackageName(PackageNameToLoad))
	{
		UE_LOG(LogStreaming, Fatal, TEXT("Async loading code requires long package names (%s)."), *PackageNameToLoad);
	}

	// Generate new request ID and add it immediately to the global request list (it needs to be there before we exit
	// this function, otherwise it would be added when the packages are being processed on the async thread).
	const int32 RequestID = GPackageRequestID.Increment();
	FAsyncLoadingThread::Get().AddPendingRequest(RequestID);
	// Add new package request
	FAsyncPackageDesc PackageDesc(RequestID, *PackageName, *PackageNameToLoad, InGuid ? *InGuid : FGuid(), InCompletionDelegate, InPackageFlags, InPIEInstanceID, InPackagePriority);
	FAsyncLoadingThread::Get().QueuePackage(PackageDesc);

	return RequestID;
}

int32 LoadPackageAsync(const FString& PackageName, FLoadPackageAsyncDelegate CompletionDelegate, int32 InPackagePriority /*= 0*/, EPackageFlags InPackageFlags /*= PKG_None*/)
{
	const FGuid* Guid = nullptr;
	const TCHAR* PackageToLoadFrom = nullptr;
	return LoadPackageAsync(PackageName, Guid, PackageToLoadFrom, CompletionDelegate, InPackageFlags, -1, InPackagePriority );
}

int32 LoadPackageAsync(const FString& InName, const FGuid* InGuid, FName InType /* Unused, deprecated */, const TCHAR* InPackageToLoadFrom /*= nullptr*/, FLoadPackageAsyncDelegate InCompletionDelegate /*= FLoadPackageAsyncDelegate()*/, EPackageFlags InPackageFlags /*= PKG_None*/, int32 InPIEInstanceID /*= INDEX_NONE*/, int32 InPackagePriority /*= 0*/)
{
	return LoadPackageAsync(InName, InGuid, InPackageToLoadFrom, InCompletionDelegate, InPackageFlags, InPIEInstanceID, InPackagePriority);
}

void CancelAsyncLoading()
{
	// Cancelling async loading while loading is suspend will result in infinite stall
	UE_CLOG(FAsyncLoadingThread::Get().IsAsyncLoadingSuspended(), LogStreaming, Fatal, TEXT("Cannot Cancel Async Loading while async loading is suspended."));

	FAsyncLoadingThread::Get().CancelAsyncLoading();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, false);
}

float GetAsyncLoadPercentage(const FName& PackageName)
{
	return FAsyncLoadingThread::Get().GetAsyncLoadPercentage(PackageName);
}

void InitAsyncThread()
{
	FAsyncLoadingThread::Get().InitializeAsyncThread();
}

bool IsInAsyncLoadingThreadCoreUObjectInternal()
{
	return FAsyncLoadingThread::Get().IsInAsyncLoadThread();
}

void FlushAsyncLoading(int32 PackageID /* = INDEX_NONE */)
{
	CheckImageIntegrityAtRuntime();

	if (IsAsyncLoading())
	{
		FAsyncLoadingThread& AsyncThread = FAsyncLoadingThread::Get();
		// Flushing async loading while loading is suspend will result in infinite stall
		UE_CLOG(AsyncThread.IsAsyncLoadingSuspended(), LogStreaming, Fatal, TEXT("Cannot Flush Async Loading while async loading is suspended (%d)"), AsyncThread.GetAsyncLoadingSuspendedCount());

		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FlushAsyncLoadingGameThread);

		if (PackageID != INDEX_NONE && !AsyncThread.ContainsRequestID(PackageID))
		{
			return;
		}

		FCoreDelegates::OnAsyncLoadingFlush.Broadcast();

		// Disallow low priority requests like texture streaming while we are flushing streaming
		// in order to avoid excessive seeking.
		FIOSystem::Get().SetMinPriority( AIOP_Normal );

		// Flush async loaders by not using a time limit. Needed for e.g. garbage collection.
		UE_LOG(LogStreaming, Log,  TEXT("Flushing async loaders.") );
		{
			SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_TickAsyncLoadingGameThread);
			while (IsAsyncLoading())
			{
				EAsyncPackageState::Type Result = AsyncThread.TickAsyncLoading(false, false, 0, PackageID);
				if (PackageID != INDEX_NONE && Result == EAsyncPackageState::Complete)
				{
					break;
				}

				if (AsyncThread.IsMultithreaded())
				{
					// Update the heartbeat and sleep. If we're not multithreading, the heartbeat is updated after each package has been processed
					FThreadHeartBeat::Get().HeartBeat();
					FPlatformProcess::SleepNoStats(0.0001f);
				}
			}
		}

		check(PackageID != INDEX_NONE || !IsAsyncLoading());

		// Reset min priority again.
		FIOSystem::Get().SetMinPriority( AIOP_MIN );
		FMemory::Trim();
	}
}

void FlushAsyncLoading(FName ExcludeType)
{
	FlushAsyncLoading();
}

int32 GetNumAsyncPackages()
{
	return FAsyncLoadingThread::Get().GetAsyncPackagesCount();
}

EAsyncPackageState::Type ProcessAsyncLoading(bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_AsyncLoadingTime);

	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_TickAsyncLoadingGameThread);
		FAsyncLoadingThread::Get().TickAsyncLoading(bUseTimeLimit, bUseFullTimeLimit, TimeLimit);
	}

	return IsAsyncLoading() ? EAsyncPackageState::TimeOut : EAsyncPackageState::Complete;
}

bool IsAsyncLoadingCoreUObjectInternal()
{
	// GIsInitialLoad guards the async loading thread from being created too early
	return !GIsInitialLoad && FAsyncLoadingThread::Get().IsAsyncLoadingPackages();
}

bool IsAsyncLoadingMultithreadedCoreUObjectInternal()
{
	// GIsInitialLoad guards the async loading thread from being created too early
	return !GIsInitialLoad && FAsyncLoadingThread::Get().IsMultithreaded();
}

void SuspendAsyncLoadingInternal()
{
	check(IsInGameThread() && !IsInSlateThread());
	FAsyncLoadingThread::Get().SuspendLoading();
}

void ResumeAsyncLoadingInternal()
{
	check(IsInGameThread() && !IsInSlateThread());
	FAsyncLoadingThread::Get().ResumeLoading();
}

/*----------------------------------------------------------------------------
	FArchiveAsync.
----------------------------------------------------------------------------*/
#if !USE_NEW_ASYNC_IO

static uint8* MallocAsyncBuffer(const SIZE_T Size, SIZE_T& OutAllocatedSize)
{
	uint8* Result = nullptr;
#if FIND_MEMORY_STOMPS
	const SIZE_T PageSize = FPlatformMemory::GetConstants().PageSize;
	const SIZE_T Alignment = PageSize;
	const SIZE_T AlignedSize = (Size + Alignment - 1U) & -static_cast<int32>(Alignment);
	const SIZE_T AllocFullPageSize = AlignedSize + (PageSize - 1) & ~(PageSize - 1U);
	check(AllocFullPageSize >= Size);
	OutAllocatedSize = AllocFullPageSize;
	Result = (uint8*)FPlatformMemory::BinnedAllocFromOS(AllocFullPageSize);
#else
	OutAllocatedSize = Size;
	Result = (uint8*)FMemory::Malloc(Size);
#endif // FIND_MEMORY_STOMPS
	return Result;
}

static void FreeAsyncBuffer(uint8* Buffer, const SIZE_T AllocatedSize)
{
	if (Buffer)
	{
#if FIND_MEMORY_STOMPS
		FPlatformMemory::BinnedFreeToOS(Buffer, AllocatedSize);
#else
		FMemory::Free(Buffer);
#endif // FIND_MEMORY_STOMPS
	}
}

/**
 * Constructor, initializing all member variables.
 */
FArchiveAsync::FArchiveAsync( const TCHAR* InFileName )
	: FileName(InFileName)
	, FileSize(INDEX_NONE)
	, UncompressedFileSize(INDEX_NONE)
	, BulkDataAreaSize(0)
	, CurrentPos(0)
	, CompressedChunks(nullptr)
	, CurrentChunkIndex(0)
	, CompressionFlags(COMPRESS_None)
	, PlatformIsSinglethreaded(false)
{
	/** Cache FPlatformProcess::SupportsMultithreading() value as it shows up too often in profiles */
	PlatformIsSinglethreaded = !FPlatformProcess::SupportsMultithreading();

	ArIsLoading		= true;
	ArIsPersistent	= true;

	PrecacheStartPos[CURRENT]	= 0;
	PrecacheEndPos[CURRENT]		= 0;
	PrecacheBuffer[CURRENT]		= nullptr;
	PrecacheBufferSize[CURRENT] = 0;
	PrecacheBufferProtected[CURRENT] = false;

	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;
	PrecacheBuffer[NEXT]		= nullptr;
	PrecacheBufferSize[NEXT] = 0;
	PrecacheBufferProtected[NEXT] = false;

	// Relies on default constructor initializing to 0.
	check( PrecacheReadStatus[CURRENT].GetValue() == 0 );
	check( PrecacheReadStatus[NEXT].GetValue() == 0 );

	// Cache file size.
	FileSize = IFileManager::Get().FileSize( *FileName );
	// Check whether file existed.
	if( FileSize >= 0 )
	{
		// No error.
		ArIsError	= false;

		// Retrieved uncompressed file size.
		UncompressedFileSize = INDEX_NONE;

		// Package wasn't compressed so use regular file size.
		if( UncompressedFileSize == INDEX_NONE )
		{
		UncompressedFileSize = FileSize;
		}
	}
	else
	{
		// Couldn't open the file.
		ArIsError	= true;
	}
}

/**
 * Flushes cache and frees internal data.
 */
void FArchiveAsync::FlushCache()
{
	// Wait on all outstanding requests.
	if (PrecacheReadStatus[CURRENT].GetValue() || PrecacheReadStatus[NEXT].GetValue())
	{
		SCOPE_CYCLE_COUNTER(STAT_Sleep);
#if !( PLATFORM_WINDOWS && defined(__clang__) )	// @todo clang: Clang r231657 on Windows has bugs with inlining DLL imported functions
		FThreadIdleStats::FScopeIdle Scope;
#endif
		do
		{
			SHUTDOWN_IF_EXIT_REQUESTED;
			FPlatformProcess::SleepNoStats(0.0f);
		} while (PrecacheReadStatus[CURRENT].GetValue() || PrecacheReadStatus[NEXT].GetValue());
	}

	uint32 Delta = 0;

	// Invalidate any precached data and free memory for current buffer.
	Delta += PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT];
	FreeAsyncBuffer(PrecacheBuffer[CURRENT], PrecacheBufferSize[CURRENT]);
	PrecacheBuffer[CURRENT]		= nullptr;
	PrecacheStartPos[CURRENT]	= 0;
	PrecacheEndPos[CURRENT]		= 0;
	PrecacheBufferSize[CURRENT] = 0;
	PrecacheBufferProtected[CURRENT] = false;
	
	// Invalidate any precached data and free memory for next buffer.
	FreeAsyncBuffer(PrecacheBuffer[NEXT], PrecacheBufferSize[NEXT]);
	PrecacheBuffer[NEXT]		= nullptr;
	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;
	PrecacheBufferSize[NEXT] = 0;
	PrecacheBufferProtected[NEXT] = false;

	Delta += PrecacheEndPos[NEXT] - PrecacheStartPos[NEXT];
	DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, Delta);
}

/**
 * Virtual destructor cleaning up internal file reader.
 */
FArchiveAsync::~FArchiveAsync()
{
	// Invalidate any precached data and free memory.
	FlushCache();
}

/**
 * Close archive and return whether there has been an error.
 *
 * @return	true if there were NO errors, false otherwise
 */
bool FArchiveAsync::Close()
{
	// Invalidate any precached data and free memory.
	FlushCache();
	// Return true if there were NO errors, false otherwise.
	return !ArIsError;
}

/**
 * Sets mapping from offsets/ sizes that are going to be used for seeking and serialization to what
 * is actually stored on disk. If the archive supports dealing with compression in this way it is 
 * going to return true.
 *
 * @param	InCompressedChunks	Pointer to array containing information about [un]compressed chunks
 * @param	InCompressionFlags	Flags determining compression format associated with mapping
 *
 * @return true if archive supports translating offsets & uncompressing on read, false otherwise
 */
bool FArchiveAsync::SetCompressionMap( TArray<FCompressedChunk>* InCompressedChunks, ECompressionFlags InCompressionFlags )
{
	// Set chunks. A value of nullptr means to use direct reads again.
	CompressedChunks	= InCompressedChunks;
	CompressionFlags	= InCompressionFlags;
	CurrentChunkIndex	= 0;
	// Invalidate any precached data and free memory.
	FlushCache();

	// verify some assumptions
	check(UncompressedFileSize == FileSize);
	check(CompressedChunks->Num() > 0);

	// update the uncompressed filesize (which is the end of the uncompressed last chunk)
	FCompressedChunk& LastChunk = (*CompressedChunks)[CompressedChunks->Num() - 1];
	UncompressedFileSize = LastChunk.UncompressedOffset + LastChunk.UncompressedSize;

	BulkDataAreaSize = FileSize - (LastChunk.CompressedOffset + LastChunk.CompressedSize);

	// We support translation as requested.
	return true;
}

/**
 * Swaps current and next buffer. Relies on calling code to ensure that there are no outstanding
 * async read operations into the buffers.
 */
void FArchiveAsync::BufferSwitcheroo()
{
	check( PrecacheReadStatus[CURRENT].GetValue() == 0 );
	check( PrecacheReadStatus[NEXT].GetValue() == 0 );

	// Switcheroo.
	DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT]);
	FreeAsyncBuffer(PrecacheBuffer[CURRENT], PrecacheBufferSize[CURRENT]);
	PrecacheBuffer[CURRENT]		= PrecacheBuffer[NEXT];
	PrecacheStartPos[CURRENT]	= PrecacheStartPos[NEXT];
	PrecacheEndPos[CURRENT]		= PrecacheEndPos[NEXT];
	PrecacheBufferSize[CURRENT] = PrecacheBufferSize[NEXT];
	PrecacheBufferProtected[CURRENT] = PrecacheBufferProtected[NEXT];

	// Next buffer is unused/ free.
	PrecacheBuffer[NEXT]		= nullptr;
	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;
	PrecacheBufferSize[NEXT] = 0;
	PrecacheBufferProtected[NEXT] = 0;
}

/**
 * Whether the current precache buffer contains the passed in request.
 *
 * @param	RequestOffset	Offset in bytes from start of file
 * @param	RequestSize		Size in bytes requested
 *
 * @return true if buffer contains request, false othwerise
 */
bool FArchiveAsync::PrecacheBufferContainsRequest( int64 RequestOffset, int64 RequestSize )
{
	// true if request is part of precached buffer.
	if( (RequestOffset >= PrecacheStartPos[CURRENT]) 
	&&  (RequestOffset+RequestSize <= PrecacheEndPos[CURRENT]) )
	{
		return true;
	}
	// false if it doesn't fit 100%.
	else
	{
		return false;
	}
}

/**
 * Finds and returns the compressed chunk index associated with the passed in offset.
 *
 * @param	RequestOffset	Offset in file to find associated chunk index for
 *
 * @return Index into CompressedChunks array matching this offset
 */
int32 FArchiveAsync::FindCompressedChunkIndex( int64 RequestOffset )
{
	// Find base start point and size. @todo optimization: avoid full iteration
	CurrentChunkIndex = 0;
	while( CurrentChunkIndex < CompressedChunks->Num() )
	{
		const FCompressedChunk& Chunk = (*CompressedChunks)[CurrentChunkIndex];
		// Check whether request offset is encompassed by this chunk.
		if( Chunk.UncompressedOffset <= RequestOffset 
		&&  Chunk.UncompressedOffset + Chunk.UncompressedSize > RequestOffset )
		{
			break;
		}
		CurrentChunkIndex++;
	}
	check( CurrentChunkIndex < CompressedChunks->Num() );
	return CurrentChunkIndex;
}

/**
 * Precaches compressed chunk of passed in index using buffer at passed in index.
 *
 * @param	ChunkIndex	Index of compressed chunk
 * @param	BufferIndex	Index of buffer to precache into	
 */
void FArchiveAsync::PrecacheCompressedChunk( int64 ChunkIndex, int64 BufferIndex )
{
	// Compressed chunk to request.
	FCompressedChunk ChunkToRead = (*CompressedChunks)[ChunkIndex];

	// Update start and end position...
	{
		DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[BufferIndex] - PrecacheStartPos[BufferIndex]);
	}
	PrecacheStartPos[BufferIndex]	= ChunkToRead.UncompressedOffset;
	PrecacheEndPos[BufferIndex]		= ChunkToRead.UncompressedOffset + ChunkToRead.UncompressedSize;

	// In theory we could use FMemory::Realloc if it had a way to signal that we don't want to copy
	// the data (implicit realloc behavior).
	FreeAsyncBuffer(PrecacheBuffer[BufferIndex], PrecacheBufferSize[BufferIndex]);
	PrecacheBufferProtected[BufferIndex] = false;
	PrecacheBuffer[BufferIndex] = MallocAsyncBuffer(PrecacheEndPos[BufferIndex] - PrecacheStartPos[BufferIndex], PrecacheBufferSize[BufferIndex]);
	{
		INC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[BufferIndex] - PrecacheStartPos[BufferIndex]);
	}

	// Increment read status, request load and make sure that request was possible (e.g. filename was valid).
	check( PrecacheReadStatus[BufferIndex].GetValue() == 0 );
	PrecacheReadStatus[BufferIndex].Increment();
	uint64 RequestId = FIOSystem::Get().LoadCompressedData( 
							FileName, 
							ChunkToRead.CompressedOffset, 
							ChunkToRead.CompressedSize, 
							ChunkToRead.UncompressedSize, 
							PrecacheBuffer[BufferIndex], 
							CompressionFlags, 
							&PrecacheReadStatus[BufferIndex],
							AIOP_Normal);
	check(RequestId);
}

bool FArchiveAsync::Precache( int64 RequestOffset, int64 RequestSize )
{
	SCOPE_CYCLE_COUNTER(STAT_FArchiveAsync_Precache);

	// Check whether we're currently waiting for a read request to finish.
	bool bFinishedReadingCurrent	= PrecacheReadStatus[CURRENT].GetValue()==0 ? true : false;
	bool bFinishedReadingNext		= PrecacheReadStatus[NEXT].GetValue()==0 ? true : false;

	// Return read status if the current request fits entirely in the precached region.
	if( PrecacheBufferContainsRequest( RequestOffset, RequestSize ) )
	{
		if (!bFinishedReadingCurrent && PlatformIsSinglethreaded)
		{
			// Tick async loading when multithreading is disabled.
			FIOSystem::Get().TickSingleThreaded();
		}
		return bFinishedReadingCurrent;
	}
	// We're not fitting into the precached region and we have a current read request outstanding
	// so wait till we're done with that. This can happen if we're skipping over large blocks in
	// the file because the object has been found in memory.
	// @todo async: implement cancelation
	else if( !bFinishedReadingCurrent )
	{
		return false;
	}
	// We're still in the middle of fulfilling the next read request so wait till that is done.
	else if( !bFinishedReadingNext )
	{
		return false;
	}
	// We need to make a new read request.
	else
	{
		// Compressed read. The passed in offset and size were requests into the uncompressed file and
		// need to be translated via the CompressedChunks map first.
		if( CompressedChunks && RequestOffset < UncompressedFileSize)
		{
			// Switch to next buffer.
			BufferSwitcheroo();

			// Check whether region is precached after switcheroo.
			bool	bIsRequestCached	= PrecacheBufferContainsRequest( RequestOffset, RequestSize );
			// Find chunk associated with request.
			int32		RequestChunkIndex	= FindCompressedChunkIndex( RequestOffset );

			// Precache chunk if it isn't already.
			if( !bIsRequestCached )
			{
				PrecacheCompressedChunk( RequestChunkIndex, CURRENT );
			}

			// Precache next chunk if there is one.
			if( RequestChunkIndex + 1 < CompressedChunks->Num() )
			{
				PrecacheCompressedChunk( RequestChunkIndex + 1, NEXT );
			}
		}
		// Regular read.
		else
		{
			// Request generic async IO system.
			{
				DEC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT]);
			}
			PrecacheStartPos[CURRENT]	= RequestOffset;
			// We always request at least a few KByte to be read/ precached to avoid going to disk for
			// a lot of little reads.
			static int64 MinimumReadSize = FIOSystem::Get().MinimumReadSize();
			checkSlow(MinimumReadSize >= 2048 && MinimumReadSize <= 1024 * 1024); // not a hard limit, but we should be loading at least a reasonable amount of data
			PrecacheEndPos[CURRENT]		= RequestOffset + FMath::Max( RequestSize, MinimumReadSize );
			// Ensure that we're not trying to read beyond EOF.
			PrecacheEndPos[CURRENT]		= FMath::Min( PrecacheEndPos[CURRENT], FileSize );
			// In theory we could use FMemory::Realloc if it had a way to signal that we don't want to copy
			// the data (implicit realloc behavior).
			FreeAsyncBuffer(PrecacheBuffer[CURRENT], PrecacheBufferSize[CURRENT]);
			PrecacheBufferProtected[CURRENT] = false;
			PrecacheBuffer[CURRENT] = MallocAsyncBuffer(PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT], PrecacheBufferSize[CURRENT]);
			{
				INC_DWORD_STAT_BY(STAT_StreamingAllocSize, PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT]);
			}

			// Increment read status, request load and make sure that request was possible (e.g. filename was valid).
			PrecacheReadStatus[CURRENT].Increment();
			uint64 RequestId = FIOSystem::Get().LoadData( 
									FileName, 
									PrecacheStartPos[CURRENT], 
									PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT], 
									PrecacheBuffer[CURRENT], 
									&PrecacheReadStatus[CURRENT],
									AIOP_Normal);
			check(RequestId);
		}

		return false;
	}
}

/**
 * Serializes data from archive.
 *
 * @param	Data	Pointer to serialize to
 * @param	Count	Number of bytes to read
 */
void FArchiveAsync::Serialize(void* Data, int64 Count)
{
#if PLATFORM_DESKTOP
	// Show a message box indicating, possible, corrupt data (desktop platforms only)
	if (CurrentPos + Count > TotalSize())
	{
		FText ErrorMessage, ErrorCaption;
		GConfig->GetText(TEXT("/Script/Engine.Engine"),
			  			 TEXT("SerializationOutOfBoundsErrorMessage"),
						 ErrorMessage,
						 GEngineIni);
		GConfig->GetText(TEXT("/Script/Engine.Engine"),
			TEXT("SerializationOutOfBoundsErrorMessageCaption"),
			ErrorCaption,
			GEngineIni);

		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage.ToString(), *ErrorCaption.ToString());
	}
#endif
	// Ensure we aren't reading beyond the end of the file
	checkf( CurrentPos + Count <= TotalSize(), TEXT("Seeked past end of file %s (%lld / %lld)"), *FileName, CurrentPos + Count, TotalSize() );

#if LOOKING_FOR_PERF_ISSUES
	uint32 StartCycles = 0;
	bool	bIOBlocked	= false;
#endif

	// Make sure serialization request fits entirely in already precached region.
	if( !PrecacheBufferContainsRequest( CurrentPos, Count ) )
	{
		DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FArchiveAsync::Serialize.PrecacheBufferContainsRequest" ), STAT_ArchiveAsync_Serialize_PrecacheBufferContainsRequest, STATGROUP_AsyncLoad );

#if LOOKING_FOR_PERF_ISSUES
		// Keep track of time we started to block.
		StartCycles = FPlatformTime::Cycles();
		bIOBlocked = true;
#endif

		// Busy wait for region to be precached.
		if (!Precache(CurrentPos, Count))
		{
			SCOPE_CYCLE_COUNTER(STAT_Sleep);
#if !( PLATFORM_WINDOWS && defined(__clang__) )	// @todo clang: Clang r231657 on Windows has bugs with inlining DLL imported functions
			FThreadIdleStats::FScopeIdle Scope;
#endif
			do
			{
				SHUTDOWN_IF_EXIT_REQUESTED;
				if (PlatformIsSinglethreaded)
				{
					FIOSystem::Get().TickSingleThreaded();
				}
				FPlatformProcess::SleepNoStats(0.001f);
			} while (!Precache(CurrentPos, Count));
		}

		// There shouldn't be any outstanding read requests for the main buffer at this point.
		check( PrecacheReadStatus[CURRENT].GetValue() == 0 );
	}
	
	// Make sure to wait till read request has finished before progressing. This can happen if PreCache interface
	// is not being used for serialization.
	if (PrecacheReadStatus[CURRENT].GetValue())
	{
		SCOPE_CYCLE_COUNTER(STAT_Sleep);
#if !( PLATFORM_WINDOWS && defined(__clang__) )	// @todo clang: Clang r231657 on Windows has bugs with inlining DLL imported functions
		FThreadIdleStats::FScopeIdle Scope;
#endif
		do
		{
			SHUTDOWN_IF_EXIT_REQUESTED;
#if LOOKING_FOR_PERF_ISSUES
			// Only update StartTime if we haven't already started blocking I/O above.
			if (!bIOBlocked)
			{
				// Keep track of time we started to block.
				StartCycles = FPlatformTime::Cycles();
				bIOBlocked = true;
			}
#endif
			if (PlatformIsSinglethreaded)
			{
				FIOSystem::Get().TickSingleThreaded();
			}
			FPlatformProcess::SleepNoStats(0.001f);
		} while (PrecacheReadStatus[CURRENT].GetValue());
	}
#if FIND_MEMORY_STOMPS
	if (!PrecacheBufferProtected[CURRENT])
	{
		if (!FPlatformMemory::PageProtect(PrecacheBuffer[CURRENT], PrecacheBufferSize[CURRENT], true, false))
		{
			UE_LOG(LogStreaming, Warning, TEXT("Unable to write protect async buffer %d for %s"), int32(CURRENT), *FileName);
		}
		PrecacheBufferProtected[CURRENT] = true;
	}
#endif // FIND_MEMORY_STOMPS

	// Update stats if we were blocked.
#if LOOKING_FOR_PERF_ISSUES
	if( bIOBlocked )
	{
		const int32 BlockingCycles = int32(FPlatformTime::Cycles() - StartCycles);
		FAsyncLoadingThread::BlockingCycles.Add( BlockingCycles );

		UE_LOG(LogStreaming, Warning, TEXT("FArchiveAsync::Serialize: %5.2fms blocking on read from '%s' (Offset: %lld, Size: %lld)"), 
			FPlatformTime::ToMilliseconds(BlockingCycles), 
			*FileName, 
			CurrentPos, 
			Count );
	}
#endif

	// Copy memory to destination.
	FMemory::Memcpy( Data, PrecacheBuffer[CURRENT] + (CurrentPos - PrecacheStartPos[CURRENT]), Count );
	// Serialization implicitly increases position in file.
	CurrentPos += Count;
}

int64 FArchiveAsync::Tell()
{
	return CurrentPos;
}

int64 FArchiveAsync::TotalSize()
{
	return UncompressedFileSize + BulkDataAreaSize;
}

void FArchiveAsync::Seek( int64 InPos )
{
	check( InPos >= 0 && InPos <= TotalSize() );
	CurrentPos = InPos;
}


#endif // !USE_NEW_ASYNC_IO
/*----------------------------------------------------------------------------
	End.
----------------------------------------------------------------------------*/

#if USE_NEW_ASYNC_IO

DEFINE_LOG_CATEGORY_STATIC(LogAsyncArchive, Display, All);

struct FRequestPair
{
	IAsyncReadFileHandle* Handle;
	IAsyncReadRequest* Request;

	FRequestPair()
		: Handle(nullptr)
		, Request(nullptr)
	{
	}

	void Clear()
	{
		if (Request)
		{
			Request->Cancel();
			Request->WaitCompletion();
			delete Request;
			Request = nullptr;
		}
		if (Handle)
		{
			delete Handle;
			Handle = nullptr;
		}
	}
};

struct FHintHistory
{
	FCriticalSection Crit;
	TMap<FName, FRequestPair> Requests;

	bool Contains(const TCHAR* Filename)
	{
		FName Name(Filename);
		FScopeLock Lock(&Crit);
		return Requests.Contains(Name);
	}

	void Add(const TCHAR* Filename, FRequestPair& Pair)
	{
		FName Name(Filename);
		FScopeLock Lock(&Crit);
		Requests.Add(Name, Pair);
	}
	void Done(const TCHAR* Filename)
	{
		FName Name(Filename);
		FScopeLock Lock(&Crit);
		FRequestPair Pair;
		Requests.RemoveAndCopyValue(Name, Pair);
		if (!Pair.Handle)
		{
			//UE_LOG(LogAsyncArchive, Warning, TEXT("FPakPlatformFile::HintFutureReadDone, file was not found %s"), Filename);
		}
		else
		{
			Pair.Clear();
		}
	}

	void Dump()
	{
		FScopeLock Lock(&Crit);
		for (auto& Elem : Requests)
		{
			UE_LOG(LogAsyncArchive, Warning, TEXT("%s"), *Elem.Key.ToString());
		}
	}
	void Flush()
	{
		FScopeLock Lock(&Crit);
		if (Requests.Num())
		{
			for (auto& Elem : Requests)
			{
				Elem.Value.Clear();
			}
		}
		Requests.Empty();
	}
};

static FHintHistory GHintHistory;

static void DumpHistory(const TArray<FString>& Args)
{
	GHintHistory.Dump();
	//GHintHistory.Flush();
}

static FAutoConsoleCommand DumpRequestsCmd(
	TEXT("pak.DumpAsyncHintHistory"),
	TEXT("debug command to print the pak file debug history."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpHistory)
	);

#define DEFAULT_USE_PRECACHE_HINTS (1)

static int32 GPakCache_EnableHints = DEFAULT_USE_PRECACHE_HINTS;
static int32 GPakCache_EnableHintsLatched = DEFAULT_USE_PRECACHE_HINTS;
static FAutoConsoleVariableRef CVar_EnableHints(
	TEXT("pakcache.EnableHints"),
	GPakCache_EnableHints,
	TEXT("If > 0, then process precache hints.")
	);

static bool LatchEnableHints()
{
	if (GPakCache_EnableHints != GPakCache_EnableHintsLatched)
	{
		GPakCache_EnableHintsLatched = GPakCache_EnableHints;
		if (!GPakCache_EnableHintsLatched)
		{
			GHintHistory.Flush();
		}
	}
	return !!GPakCache_EnableHintsLatched;
}

void HintFutureReadDone(const TCHAR * FileName)
{
	if (LatchEnableHints())
	{
		GHintHistory.Done(FileName);
	}
}

#define PRECACHE_SUMMARY_ONLY (0)

void HintFutureRead(const TCHAR * FileName)
{
	if (LatchEnableHints() && !GHintHistory.Contains(FileName))
	{
		FRequestPair Pair;
		Pair.Handle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(FileName);
		if (Pair.Handle)
		{
			Pair.Request = Pair.Handle->ReadRequest(0, PRECACHE_SUMMARY_ONLY ? 1 : MAX_int64, AIOP_Precache);
		}
		GHintHistory.Add(FileName, Pair);
	}
}

static bool GRecordPakOpens = false;
static TArray<FName> GPakOpens;

static void AddPakOpen(const TCHAR* Filename)
{
	if (GRecordPakOpens)
	{
		FName Name(Filename);
		if (!GPakOpens.Contains(Name))
		{
			GPakOpens.Add(Name);
		}
	}
}

static void StartOpenLog(const TArray<FString>& Args)
{
	GRecordPakOpens = true;
}

static FAutoConsoleCommand StartOpenLogCmd(
	TEXT("pak.StartOpenLog"),
	TEXT("debug command to start recording open commands."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&StartOpenLog)
	);

static void EndOpenLog(const TArray<FString>& Args)
{
	GRecordPakOpens = false;

	FString Contents = TEXT("\r\nstatic const TCHAR *PrecacheList[] = {\r\n");

	bool bStarted = false;
	for (FName& Name : GPakOpens)
	{
		Contents += FString::Printf(TEXT("\t%cTEXT(\"%s\")\r\n"), bStarted ? ',' : ' ', *Name.ToString());
		bStarted = true;
	}

	Contents += TEXT("};\r\n");

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\r\n%s\r\n"), *Contents);

	GPakOpens.Empty();
}

static FAutoConsoleCommand EndOpenLogCmd(
	TEXT("pak.EndOpenLog"),
	TEXT("debug command to end recording open commands."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&EndOpenLog)
	);



FArchive* NewFArchiveAsync2(const TCHAR* InFileName)
{
	return new FArchiveAsync2(InFileName);
}

//#define ASYNC_WATCH_FILE "Meshes/work/TwinBlast_CylShadows.uasset"
#define USE_FArchiveAsync2_PRECACHING (1)
#define USE_FArchiveAsync2_PRECACHE_WHOLE_FILE (1)
#define TRACK_SERIALIZE (0)
#define MIN_REMAIN_TIME (0.00101f)   // wait(0) is very different than wait(tiny) so we cut things off well before roundoff error could cause us to block when we didn't intend to. Also the granularity of the event API is 1ms.

FORCEINLINE void FArchiveAsync2::LogItem(const TCHAR* Item, int64 Offset, int64 Size, double StartTime)
{
	if (UE_LOG_ACTIVE(LogAsyncArchive, Verbose)
#if defined(ASYNC_WATCH_FILE)
	|| FileName.Contains(TEXT(ASYNC_WATCH_FILE))
#endif
		)
	{
		static double OpenTime(FPlatformTime::Seconds());
		double Now(FPlatformTime::Seconds());

		float ThisTime = (StartTime != 0.0) ? float(1000.0 * (Now - StartTime)) : 0.0f;

		if (!UE_LOG_ACTIVE(LogAsyncArchive, VeryVerbose) && ThisTime < 1.0f 
#if defined(ASYNC_WATCH_FILE)
			&& !FileName.Contains(TEXT(ASYNC_WATCH_FILE))
#endif
			)
		{
			return;
		}

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%32s%3s    %12lld %12lld    %6.2fms    (+%9.2fms)      %s\r\n"),
			Item,
			(ThisTime > 1.0f) ? TEXT("***") : TEXT(""),
			Offset,
			((Size == MAX_int64) ? TotalSize() : Offset + Size),
			ThisTime,
			float(1000.0 * (Now - OpenTime)),
			*FileName);
	}
}

FArchiveAsync2::FArchiveAsync2(const TCHAR* InFileName)
	: Handle(nullptr)
	, SizeRequestPtr(nullptr)
	, SummaryRequestPtr(nullptr)
	, SummaryPrecacheRequestPtr(nullptr)
	, ReadRequestPtr(nullptr)
	, PrecacheRequestPtr(nullptr)
	, CanceledReadRequestPtr(nullptr)
	, PrecacheBuffer(nullptr)
	, NextPrecacheBuffer(nullptr)
	, FileSize(-1)
	, CurrentPos(0)
	, PrecacheStartPos(0)
	, PrecacheEndPos(0)
	, ReadRequestOffset(0)
	, ReadRequestSize(0)
	, PrecacheOffset(-1)
	, HeaderSize(0)
	, LoadPhase(ELoadPhase::WaitingForSize)
	, FileName(InFileName)
	, OpenTime(FPlatformTime::Seconds())
	, SummaryReadTime(0.0)
	, ExportReadTime(0.0)
	, PrecacheEOFTime(0.0)
{
	AddPakOpen(InFileName);
	LogItem(TEXT("Open"));
	Handle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(InFileName);
	check(Handle); // this generally cannot fail because it is async

	ReadCallbackFunction = [this](bool bWasCancelled, IAsyncReadRequest* Request)
	{
		ReadCallback(bWasCancelled, Request);
	};
	SizeRequestPtr = Handle->SizeRequest(&ReadCallbackFunction);	

}

FArchiveAsync2::~FArchiveAsync2()
{
	HintFutureReadDone(*FileName); // done
	// Invalidate any precached data and free memory.
	FlushCache();
}

void FArchiveAsync2::ReadCallback(bool bWasCancelled, IAsyncReadRequest* Request)
{
	if (bWasCancelled || ArIsError)
	{
		ArIsError = true;
		return; // we don't do much with this, the code on the other thread knows how to deal with my request
	}
	if (LoadPhase == ELoadPhase::WaitingForSize)
	{
		LoadPhase = ELoadPhase::WaitingForSummary;
		FileSize = Request->GetSizeResults();
		if (FileSize < 32)
		{
			ArIsError = true;
		}
		else
		{
			int64 Size = FMath::Min<int64>(MAX_SUMMARY_SIZE, FileSize);
			LogItem(TEXT("Starting Summary"), 0, Size);
			SummaryRequestPtr = Handle->ReadRequest(0, Size, AIOP_Normal, &ReadCallbackFunction);
			// I need a precache request here to keep the memory alive until I submit the header request
			SummaryPrecacheRequestPtr = Handle->ReadRequest(0, Size, AIOP_Precache);
		}
	}
	else if (LoadPhase == ELoadPhase::WaitingForSummary)
	{
		uint8* Mem = Request->GetReadResults();
		if (!Mem)
		{
			ArIsError = true;
		}
		else
		{
			FBufferReader Ar(Mem, FMath::Min<int64>(MAX_SUMMARY_SIZE, FileSize),/*bInFreeOnClose=*/ true, /*bIsPersistent=*/ true);
			FPackageFileSummary Sum;
			Ar << Sum;
			if (Ar.IsError() || Sum.TotalHeaderSize > FileSize)
			{
				ArIsError = true;
			}
			else
			{
				check(Ar.Tell() < MAX_SUMMARY_SIZE / 2); // we need to be sure that we can at least get the size from the initial request. This is an early warning that custom versions are starting to get too big, relocate the total size to be at offset 4!
				HeaderSize = Sum.TotalHeaderSize;
				LogItem(TEXT("Starting Header"), 0, HeaderSize);
				PrecacheInternal(0, HeaderSize);
			}
		}
		FPlatformMisc::MemoryBarrier();
		LoadPhase = ELoadPhase::WaitingForHeader;
	}
	else
	{
		check(0); // we don't use callbacks for other phases
	}
}

void FArchiveAsync2::DispatchPrecache(int64 Offset, bool bForceTrimPrecache)
{
	if (!USE_FArchiveAsync2_PRECACHING)
	{
		return;
	}
	bool bFirstOrSecondTime = PrecacheOffset < HeaderSize;
	if (!bFirstOrSecondTime && !bForceTrimPrecache)
	{
		return;
	}
	bool bFirstTime = PrecacheOffset < 0;
	bool bTrimToHeader = !USE_FArchiveAsync2_PRECACHE_WHOLE_FILE && bFirstTime;
	if (bTrimToHeader)
	{
		Offset = HeaderSize - 1; // we are not yet processing imports, so we want to put the precache request at the first import and we are only going to do one byte.
	}
	if (Offset > PrecacheOffset)
	{
		if (Offset < TotalSizeOrMaxInt64IfNotReady())
		{
			// important to get this in flight before we cancel the old one
			int64 Size = bTrimToHeader ? 1 : MAX_int64; // MAX_int64 is a legal size for a precache request, if we have finished the header, then we only save the first byte of the first export
			double StartTime = FPlatformTime::Seconds();
			IAsyncReadRequest* NewPrecacheRequestPtr = Handle->ReadRequest(Offset, Size, AIOP_Precache);
			LogItem(TEXT("Precache"), Offset, Size, StartTime);
			CompletePrecache();
			PrecacheRequestPtr = NewPrecacheRequestPtr;
#if defined(ASYNC_WATCH_FILE)
			if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)) && Offset == 894913)
			{
				UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint after Precache"));
			}
#endif
		}
		else
		{
			CompletePrecache(); // we have reached the end of the file, so we are done with precaching
			LogItem(TEXT("Precache EOF"));
#if defined(ASYNC_WATCH_FILE)
			if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)))
			{
				UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint after Precache EOF"));
			}
#endif
			check(PrecacheEOFTime = 0.0);
			if (PrecacheEOFTime == 0.0)
			{
				PrecacheEOFTime = FPlatformTime::Seconds();
			}

		}
		PrecacheOffset = Offset;
		if (bFirstTime)
		{
			HintFutureReadDone(*FileName); // we will take over precaching from here
		}
		return;
	}
	if (Offset < PrecacheOffset)
	{
		UE_LOG(LogAsyncArchive, Warning, TEXT("FArchiveAsync2::DispatchPrecache Backwards streaming in %s  NewOffset = %lld   PreviousPrecacheOffset = %lld"), *FileName, Offset, PrecacheOffset);
	}
#if defined(ASYNC_WATCH_FILE)
	if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)))
	{
		UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint after new precache."));
	}
#endif
}

void FArchiveAsync2::CompletePrecache()
{
	if (PrecacheRequestPtr)
	{
		double StartTime = FPlatformTime::Seconds();
		PrecacheRequestPtr->Cancel();
		CompleteCancel(); // this deals with the cancel request...which we are going to reuse for us
		CanceledReadRequestPtr = PrecacheRequestPtr;
		PrecacheRequestPtr = nullptr;
		LogItem(TEXT("CompletePrecache"), 0, 0, StartTime);
	}
}

void FArchiveAsync2::FlushCache()
{
	bool nNonRedundantFlush = PrecacheEndPos || PrecacheBuffer || PrecacheRequestPtr || ReadRequestPtr;
	LogItem(TEXT("Flush"));
	WaitForIntialPhases();
	WaitRead(); // this deals with the read request
	CompletePrecache(); // this deals with the precache request
	CompleteCancel(); // this deals with the cancel request, important this is last because completing other things leaves cancels to process
	if (PrecacheBuffer)
	{
		FMemory::Free(PrecacheBuffer);
	}
	PrecacheBuffer = nullptr;
	check(!NextPrecacheBuffer);
	PrecacheStartPos = 0;
	PrecacheEndPos = 0;

	if ((UE_LOG_ACTIVE(LogAsyncArchive, Verbose) 
#if defined(ASYNC_WATCH_FILE)
		|| FileName.Contains(TEXT(ASYNC_WATCH_FILE))
#endif
		) && nNonRedundantFlush)
	{
		double Now(FPlatformTime::Seconds());
		float TotalLifetime = float(1000.0 * (Now - OpenTime));

		if (!UE_LOG_ACTIVE(LogAsyncArchive, VeryVerbose) && TotalLifetime < 100.0f
#if defined(ASYNC_WATCH_FILE)
			&& !FileName.Contains(TEXT(ASYNC_WATCH_FILE))
#endif
			)
		{
			return;
		}

		if (PrecacheEOFTime == 0.0)
		{
			PrecacheEOFTime = Now;
		}
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Flush     Lifeitme %6.2fms   Open->Summary %6.2fms    Summary->Export1 %6.2fms    Export1->ExportN %6.2fms    ExportN->Now %6.2fms       %s\r\n"),
			TotalLifetime,
			float(1000.0 * (SummaryReadTime - OpenTime)),
			float(1000.0 * (ExportReadTime - SummaryReadTime)),
			float(1000.0 * (PrecacheEOFTime - ExportReadTime)),
			float(1000.0 * (Now - PrecacheEOFTime)),
			*FileName);
#if defined(ASYNC_WATCH_FILE)
		if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)))
		{
			UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint after flush."));
		}
#endif
	}

}

bool FArchiveAsync2::Close()
{
	// Invalidate any precached data and free memory.
	FlushCache();
	// Return true if there were NO errors, false otherwise.
	return !ArIsError;
}

bool FArchiveAsync2::SetCompressionMap(TArray<FCompressedChunk>* InCompressedChunks, ECompressionFlags InCompressionFlags)
{
	check(0); // no support for compression
	return false;
}

int64 FArchiveAsync2::TotalSize()
{
	if (SizeRequestPtr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FArchiveAsync2_TotalSize);
		SizeRequestPtr->WaitCompletion();
		delete SizeRequestPtr;
		SizeRequestPtr = nullptr;
	}
	return FileSize;
}

void FArchiveAsync2::Seek(int64 InPos)
{
	check(InPos >= 0 && InPos <= TotalSizeOrMaxInt64IfNotReady());
	CurrentPos = InPos;
}

bool FArchiveAsync2::WaitRead(float TimeLimit)
{
	if (ReadRequestPtr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FArchiveAsync2_WaitRead);
		int64 Offset = ReadRequestOffset;
		int64 Size = ReadRequestSize;
		double StartTime = FPlatformTime::Seconds();
		bool bResult = ReadRequestPtr->WaitCompletion(TimeLimit);
		LogItem(TEXT("Wait Read"), Offset, Size, StartTime);
		if (!bResult)
		{
			return false;
		}
		CompleteRead();
	}
	return true;
}

void FArchiveAsync2::CompleteRead()
{
	double StartTime = FPlatformTime::Seconds();
	check(LoadPhase != ELoadPhase::WaitingForSize && LoadPhase != ELoadPhase::WaitingForSummary);
	check(ReadRequestPtr && ReadRequestPtr->PollCompletion());
	if (!ArIsError)
	{
		DispatchPrecache(ReadRequestOffset + ReadRequestSize);
		if (PrecacheBuffer)
		{
			FMemory::Free(PrecacheBuffer);
			PrecacheBuffer = nullptr;
		}

	uint8* Mem = ReadRequestPtr->GetReadResults();
	if (!Mem)
	{
		ArIsError = true;
			PrecacheStartPos = 0;
			PrecacheEndPos = 0;
	}
	else
	{
		PrecacheBuffer = Mem;
		PrecacheStartPos = ReadRequestOffset;
		PrecacheEndPos = ReadRequestOffset + ReadRequestSize;
	}
	}
	delete ReadRequestPtr;
	ReadRequestPtr = nullptr;
	LogItem(TEXT("CompleteRead"), ReadRequestOffset, ReadRequestSize);
	ReadRequestOffset = 0;
	ReadRequestSize = 0;
}

void FArchiveAsync2::CompleteCancel()
{
	if (CanceledReadRequestPtr)
	{
		double StartTime = FPlatformTime::Seconds();
		CanceledReadRequestPtr->WaitCompletion();
		check(!CanceledReadRequestPtr->GetReadResults()); // this should have been canceled
		delete CanceledReadRequestPtr;
		CanceledReadRequestPtr = nullptr;
		LogItem(TEXT("Complete Cancel"), 0, 0, StartTime);
	}
}


void FArchiveAsync2::CancelRead()
{
	if (ReadRequestPtr)
	{
		ReadRequestPtr->Cancel();
		CompleteCancel();
		CanceledReadRequestPtr = ReadRequestPtr;
		ReadRequestPtr = nullptr;
	}
}

bool FArchiveAsync2::WaitForIntialPhases(float InTimeLimit)
{
	if (SizeRequestPtr || SummaryRequestPtr || SummaryPrecacheRequestPtr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FArchiveAsync2_WaitForIntialPhases);
		double StartTime = FPlatformTime::Seconds();
		if (SizeRequestPtr)
		{
			if (SizeRequestPtr->WaitCompletion(InTimeLimit))
			{
				delete SizeRequestPtr;
				SizeRequestPtr = nullptr;
			}
			else
			{
				check(InTimeLimit > 0.0f);
				return false;
			}
		}
		if (SummaryRequestPtr)
		{
			float TimeLimit = 0.0f;
			if (InTimeLimit > 0.0f)
			{
				TimeLimit = InTimeLimit - (FPlatformTime::Seconds() - StartTime);
				if (TimeLimit < MIN_REMAIN_TIME)
				{
					return false;
				}
			}
			if (SummaryRequestPtr->WaitCompletion(TimeLimit))
			{
				delete SummaryRequestPtr;
				SummaryRequestPtr = nullptr;
			}
			else
			{
				check(InTimeLimit > 0.0f);
				return false;
			}
		}
		if (SummaryPrecacheRequestPtr)
		{
			float TimeLimit = 0.0f;
			if (InTimeLimit > 0.0f)
			{
				TimeLimit = InTimeLimit - (FPlatformTime::Seconds() - StartTime);
				if (TimeLimit < MIN_REMAIN_TIME)
				{
					return false;
				}
			}
			if (SummaryPrecacheRequestPtr->WaitCompletion(TimeLimit))
			{
				delete SummaryPrecacheRequestPtr;
				SummaryPrecacheRequestPtr = nullptr;
			}
			else
			{
				check(InTimeLimit > 0.0f);
				return false;
			}
		}
		LogItem(TEXT("Wait Summary"), 0, HeaderSize, StartTime);
	}
	return true;
}

bool FArchiveAsync2::PrecacheInternal(int64 RequestOffset, int64 RequestSize)
{
	// CAUTION! This is possibly called the first time from a random IO thread.
	if (LoadPhase != ELoadPhase::WaitingForSummary)
	{
	if (RequestSize == 0 || (RequestOffset >= PrecacheStartPos && RequestOffset + RequestSize <= PrecacheEndPos))
	{
		// ready
		return true;
	}
	if (ReadRequestPtr && RequestOffset >= ReadRequestOffset && RequestOffset + RequestSize <= ReadRequestOffset + ReadRequestSize)
	{
		// current request contains request
		if (ReadRequestPtr->PollCompletion())
		{
			CompleteRead();
			check(RequestOffset >= PrecacheStartPos && RequestOffset + RequestSize <= PrecacheEndPos);
			return true;
		}
		return false;
	}
	if (ReadRequestPtr)
	{
		// this one does not have what we need
			UE_LOG(LogStreaming, Warning, TEXT("FArchiveAsync2::DispatchPrecache Canceled read for %s  Offset = %lld   Size = %lld"), *FileName, RequestOffset, ReadRequestSize);
		CancelRead();
	}
	}
	check(!ReadRequestPtr);
	ReadRequestOffset = RequestOffset;
	ReadRequestSize = RequestSize;


	if (LoadPhase == ELoadPhase::ProcessingExports)
	{
		static int64 MinimumReadSize = 65536;
		checkSlow(MinimumReadSize >= 2048 && MinimumReadSize <= 1024 * 1024); // not a hard limit, but we should be loading at least a reasonable amount of data
		if (ReadRequestSize < MinimumReadSize)
		{ 
			ReadRequestSize = MinimumReadSize;
			int64 LocalFileSize = TotalSize();
			ReadRequestSize = FMath::Min(ReadRequestOffset + ReadRequestSize, LocalFileSize) - ReadRequestOffset;
		}
	}
	if (ReadRequestSize <= 0)
	{
		ArIsError = true;
		return true;
	}
	double StartTime = FPlatformTime::Seconds();
#if defined(ASYNC_WATCH_FILE)
	if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)) && ReadRequestOffset == 80203)
	{
		UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint Read"));
	}
#endif

	ReadRequestPtr = Handle->ReadRequest(ReadRequestOffset, ReadRequestSize, AIOP_Normal);
	if (LoadPhase != ELoadPhase::WaitingForSummary && ReadRequestPtr->PollCompletion())
	{
		LogItem(TEXT("Read Start Hot"), ReadRequestOffset, ReadRequestSize, StartTime);
		CompleteRead();
		check(RequestOffset >= PrecacheStartPos && RequestOffset + RequestSize <= PrecacheEndPos);
		return true;
	}
	LogItem(TEXT("Read Start Cold"), ReadRequestOffset, ReadRequestSize, StartTime);
	return false;
}

bool FArchiveAsync2::Precache(int64 RequestOffset, int64 RequestSize, bool bUseTimeLimit, bool bUseFullTimeLimit, double TickStartTime, float TimeLimit, bool bTrimPrecache)
{
#if defined(ASYNC_WATCH_FILE)
	if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)) && RequestOffset == 878129)
	{
		UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint Raw Precache %lld"), RequestOffset);
	}
#endif
	if (LoadPhase == ELoadPhase::WaitingForSize || LoadPhase == ELoadPhase::WaitingForSummary || LoadPhase == ELoadPhase::WaitingForHeader)
	{
		check(0); // this is a precache for an export, why is the summary not read yet?
		return false;
	}
	if (LoadPhase == ELoadPhase::WaitingForFirstExport)
	{
		ExportReadTime = FPlatformTime::Seconds();
		LogItem(TEXT("Exports"));
		LoadPhase = ELoadPhase::ProcessingExports;
#if !USE_FArchiveAsync2_PRECACHE_WHOLE_FILE
		DispatchPrecache(RequestOffset, true); // start precacheing the exports
#endif
	}
	if (!bUseTimeLimit)
	{
		if (bTrimPrecache)
		{
			DispatchPrecache(RequestOffset, true); // discard anything before here
		}
		return true; // we will stream and do the blocking on the serialize calls
	}
	bool bResult = PrecacheInternal(RequestOffset, RequestSize);
	if (!bResult && bUseFullTimeLimit)
	{
		float RemainingTime = TimeLimit - float(FPlatformTime::Seconds() - TickStartTime);
		if (RemainingTime > MIN_REMAIN_TIME && WaitRead(RemainingTime))
		{
			bResult = true;
		}
	}
	if (bTrimPrecache)
	{
		DispatchPrecache(RequestOffset + RequestSize, true); // discard anything before and including this export....we already have the real request in flight
	}
	return bResult;
}

bool FArchiveAsync2::Precache(int64 RequestOffset, int64 RequestSize)
{
	if (LoadPhase == ELoadPhase::WaitingForSize || LoadPhase == ELoadPhase::WaitingForSummary)
	{
		return false;
	}
	if (LoadPhase == ELoadPhase::WaitingForHeader)
	{
		check(RequestOffset == 0 && RequestOffset + RequestSize <= HeaderSize);
	}
	return PrecacheInternal(RequestOffset, RequestSize);
}


void FArchiveAsync2::StartReadingHeader()
{
	//LogItem(TEXT("Start Header"));
	WaitForIntialPhases();
	check(LoadPhase == ELoadPhase::WaitingForHeader && ReadRequestPtr);
	WaitRead();
}

void FArchiveAsync2::EndReadingHeader()
{
	LogItem(TEXT("End Header"));
	check(LoadPhase == ELoadPhase::WaitingForHeader);
	LoadPhase = ELoadPhase::WaitingForFirstExport;
}

bool FArchiveAsync2::ReadyToStartReadingHeader(bool bUseTimeLimit, bool bUseFullTimeLimit, double TickStartTime, float TimeLimit)
{
	if (SummaryReadTime == 0.0)
	{
		SummaryReadTime = FPlatformTime::Seconds();
	}
	if (!bUseTimeLimit)
	{
		return true; // we will stream and do the blocking on the serialize calls
	}
	if (LoadPhase == ELoadPhase::WaitingForSize || LoadPhase == ELoadPhase::WaitingForSummary)
	{
		if (bUseFullTimeLimit)
		{
			float RemainingTime = TimeLimit - float(FPlatformTime::Seconds() - TickStartTime);
			if (RemainingTime < MIN_REMAIN_TIME || !WaitForIntialPhases(RemainingTime))
			{
				return false; // not ready
			}
		}
		else
		{
			return false; // not ready, not going to wait
		}
	}
	check(LoadPhase == ELoadPhase::WaitingForHeader);
	LogItem(TEXT("Ready For Header"));
	return true;
}

#if TRACK_SERIALIZE
void CallSerializeHook();
#endif

void FArchiveAsync2::Serialize(void* Data, int64 Count)
{
#if TRACK_SERIALIZE
	CallSerializeHook();
#endif

	if (!Count || ArIsError)
	{
		return;
	}
	check(Count > 0);
#if PLATFORM_DESKTOP
	// Show a message box indicating, possible, corrupt data (desktop platforms only)
	if (CurrentPos + Count > TotalSize())
	{
		FText ErrorMessage, ErrorCaption;
		GConfig->GetText(TEXT("/Script/Engine.Engine"),
			TEXT("SerializationOutOfBoundsErrorMessage"),
			ErrorMessage,
			GEngineIni);
		GConfig->GetText(TEXT("/Script/Engine.Engine"),
			TEXT("SerializationOutOfBoundsErrorMessageCaption"),
			ErrorCaption,
			GEngineIni);

		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage.ToString(), *ErrorCaption.ToString());
	}
#endif
	// Ensure we aren't reading beyond the end of the file
	checkf(CurrentPos + Count <= TotalSizeOrMaxInt64IfNotReady(), TEXT("Seeked past end of file %s (%lld / %lld)"), *FileName, CurrentPos + Count, TotalSize());

	int64 BeforeBlockOffset = 0;
	int64 BeforeBlockSize = 0;
	int64 AfterBlockOffset = 0;
	int64 AfterBlockSize = 0;

	if (CurrentPos + Count <= PrecacheStartPos || CurrentPos >= PrecacheEndPos)
	{
		// no overlap with current buffer
		AfterBlockOffset = CurrentPos;
		AfterBlockSize = Count;
	}
	else
	{
		if (CurrentPos >= PrecacheStartPos)
		{
			// no before block and head of desired block is in the cache
			int64 CopyLen = FMath::Min(PrecacheEndPos - CurrentPos, Count);
			check(CopyLen > 0);
			FMemory::Memcpy(Data, PrecacheBuffer + CurrentPos - PrecacheStartPos, CopyLen);
			AfterBlockSize = Count - CopyLen;
			check(AfterBlockSize >= 0);
			AfterBlockOffset = PrecacheEndPos;
		}
		else
		{
			// first part of the block is not in the cache
			BeforeBlockSize = PrecacheStartPos - CurrentPos;
			check(BeforeBlockSize > 0);
			BeforeBlockOffset = CurrentPos;
			if (CurrentPos + Count > PrecacheStartPos)
			{
				// tail of desired block is in the cache
				int64 CopyLen = FMath::Min(PrecacheEndPos - CurrentPos - BeforeBlockSize, Count - BeforeBlockSize);
				check(CopyLen > 0);
				FMemory::Memcpy(((uint8*)Data) + BeforeBlockSize, PrecacheBuffer, CopyLen);
				AfterBlockSize = Count - CopyLen - BeforeBlockSize;
				check(AfterBlockSize >= 0);
				AfterBlockOffset = PrecacheEndPos;
			}
		}
	}
	if (BeforeBlockSize)
	{
		UE_LOG(LogAsyncArchive, Warning, TEXT("FArchiveAsync2::DispatchPrecache Backwards streaming in %s  BeforeBlockOffset = %lld   BeforeBlockOffset = %lld"), *FileName, BeforeBlockOffset, BeforeBlockOffset);
		LogItem(TEXT("Sync Before Block"), BeforeBlockOffset, BeforeBlockSize);
		PrecacheInternal(BeforeBlockOffset, BeforeBlockSize);
		WaitRead();
		check(BeforeBlockOffset >= PrecacheStartPos && BeforeBlockOffset + BeforeBlockSize <= PrecacheEndPos);
		FMemory::Memcpy(Data, PrecacheBuffer + BeforeBlockOffset - PrecacheStartPos, BeforeBlockSize);
	}
	if (AfterBlockSize)
	{
#if defined(ASYNC_WATCH_FILE)
		if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)))
		{
			UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint AfterBlockSize"));
		}
#endif
		LogItem(TEXT("Sync After Block"), AfterBlockOffset, AfterBlockSize);
		PrecacheInternal(AfterBlockOffset, AfterBlockSize);
		WaitRead();
		check(AfterBlockOffset >= PrecacheStartPos && AfterBlockOffset + AfterBlockSize <= PrecacheEndPos);
		FMemory::Memcpy(((uint8*)Data) + Count - AfterBlockSize, PrecacheBuffer + AfterBlockOffset - PrecacheStartPos, AfterBlockSize);
	}
	CurrentPos += Count;
}

#if TRACK_SERIALIZE

#include "StackTracker.h"
static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn(
	TEXT("LogAsyncArchiveSerializeChurn.Enable"),
	0,
	TEXT("If > 0, then sample game thread FArchiveAsync2::Serialize calls, periodically print a report of the worst offenders."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_Threshhold(
	TEXT("LogAsyncArchiveSerializeChurn.Threshhold"),
	1000,
	TEXT("Minimum average number of FArchiveAsync2::Serialize calls to include in the report."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_SampleFrequency(
	TEXT("LogAsyncArchiveSerializeChurn.SampleFrequency"),
	1000,
	TEXT("Number of FArchiveAsync2::Serialize calls per sample. This is used to prevent sampling from slowing the game down too much."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_StackIgnore(
	TEXT("LogAsyncArchiveSerializeChurn.StackIgnore"),
	2,
	TEXT("Number of items to discard from the top of a stack frame."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_RemoveAliases(
	TEXT("LogAsyncArchiveSerializeChurn.RemoveAliases"),
	1,
	TEXT("If > 0 then remove aliases from the counting process. This essentialy merges addresses that have the same human readable string. It is slower."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_StackLen(
	TEXT("LogAsyncArchiveSerializeChurn.StackLen"),
	4,
	TEXT("Maximum number of stack frame items to keep. This improves aggregation because calls that originate from multiple places but end up in the same place will be accounted together."));


struct FSampleSerializeChurn
{
	FStackTracker GGameThreadFNameChurnTracker;
	bool bEnabled;
	int32 CountDown;

	FSampleSerializeChurn()
		: bEnabled(false)
		, CountDown(MAX_int32)
	{
	}

	void SerializeHook()
	{
		bool bNewEnabled = CVarLogAsyncArchiveSerializeChurn.GetValueOnGameThread() > 0;
		if (bNewEnabled != bEnabled)
		{
			check(IsInGameThread());
			bEnabled = bNewEnabled;
			if (bEnabled)
			{
				CountDown = CVarLogAsyncArchiveSerializeChurn_SampleFrequency.GetValueOnGameThread();
				GGameThreadFNameChurnTracker.ResetTracking();
				GGameThreadFNameChurnTracker.ToggleTracking(true, true);
			}
			else
			{
				GGameThreadFNameChurnTracker.ToggleTracking(false, true);
				GGameThreadFNameChurnTracker.ResetTracking();
			}
		}
		else if (bEnabled)
		{
			check(IsInGameThread());
			if (--CountDown <= 0)
	{
				CountDown = CVarLogAsyncArchiveSerializeChurn_SampleFrequency.GetValueOnGameThread();
				CollectSample();
			}
		}
	}

	void CollectSample()
	{
		check(IsInGameThread());
		GGameThreadFNameChurnTracker.CaptureStackTrace(CVarLogAsyncArchiveSerializeChurn_StackIgnore.GetValueOnGameThread(), nullptr, CVarLogAsyncArchiveSerializeChurn_StackLen.GetValueOnGameThread(), CVarLogAsyncArchiveSerializeChurn_RemoveAliases.GetValueOnGameThread() > 0);
	}
	void PrintResultsAndReset()
	{
		FOutputDeviceRedirector* Log = FOutputDeviceRedirector::Get();
		float SampleAndFrameCorrection = float(CVarLogAsyncArchiveSerializeChurn_SampleFrequency.GetValueOnGameThread());
		GGameThreadFNameChurnTracker.DumpStackTraces(CVarLogAsyncArchiveSerializeChurn_Threshhold.GetValueOnGameThread(), *Log, SampleAndFrameCorrection);
		GGameThreadFNameChurnTracker.ResetTracking();
	}
};

FSampleSerializeChurn GGameThreadSerializeTracker;

void CallSerializeHook()
{
	if (GIsRunning && IsInGameThread())
	{
		GGameThreadSerializeTracker.SerializeHook();
	}
}

static void DumpSerialize(const TArray<FString>& Args)
{
	if (IsInGameThread())
	{
		GGameThreadSerializeTracker.PrintResultsAndReset();
	}
}

static FAutoConsoleCommand DumpSerializeCmd(
	TEXT("LogAsyncArchiveSerializeChurn.Dump"),
	TEXT("debug command to dump the results of tracking the serialization calls."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpSerialize)
	);
#endif

#endif // USE_NEW_ASYNC_IO

//#pragma clang optimize on
