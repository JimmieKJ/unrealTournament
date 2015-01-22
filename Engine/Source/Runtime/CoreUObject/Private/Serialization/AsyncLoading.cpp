// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code.
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "Serialization/AsyncLoading.h"

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
DECLARE_CYCLE_STAT(TEXT("CreateExports AsyncPackage"),STAT_FAsyncPackage_CreateExports,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("PreLoadObjects AsyncPackage"),STAT_FAsyncPackage_PreLoadObjects,STATGROUP_AsyncLoad);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("PostLoadObjects AsyncPackage Time"), STAT_FAsyncPackage_PostLoadObjectsTime, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("PostLoadObjects AsyncPackage"),STAT_FAsyncPackage_PostLoadObjects,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("FinishObjects AsyncPackage"),STAT_FAsyncPackage_FinishObjects,STATGROUP_AsyncLoad);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Flush Async Loading Time"), STAT_FAsyncPackage_FlushAsyncLoadingTime, STATGROUP_AsyncLoad);

DECLARE_CYCLE_STAT(TEXT("Async Loading Time"),STAT_AsyncLoadingTime,STATGROUP_AsyncLoad);



/** Objects that have been constructed during async loading phase.						*/
static TArray<UObject*>			GObjConstructedDuringAsyncLoading;

/** Keeps a reference to all objects created during async load until streaming has finished */
class FAsyncObjectsReferencer : FGCObject
{
	/** Private constructor */
	FAsyncObjectsReferencer() {}

	/** List of referenced objects */
	TArray<UObject*> ReferencedObjects;
public:
	/** Returns the one and only instance of this object */
	static FAsyncObjectsReferencer& Get();
	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AllowEliminatingReferences(false);
		Collector.AddReferencedObjects(ReferencedObjects);
		Collector.AllowEliminatingReferences(true);
	}
	/** Adds an object to be referenced */
	FORCEINLINE void AddObject(UObject* InObject)
	{
		ReferencedObjects.Add(InObject);
	}
	/** Removes all objects from the list */
	FORCEINLINE void EmptyReferencedObjects()
	{
		ReferencedObjects.Empty(ReferencedObjects.Num());
	}
};
FAsyncObjectsReferencer& FAsyncObjectsReferencer::Get()
{
	static FAsyncObjectsReferencer Singleton;
	return Singleton;
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
		Object->SetFlags(RF_AsyncLoading);
		GObjConstructedDuringAsyncLoading.Add(Object);
	}
	FAsyncObjectsReferencer::Get().AddObject(Object);
}


/*-----------------------------------------------------------------------------
	FAsyncPackage implementation.
-----------------------------------------------------------------------------*/
/** Array of packages that are being preloaded							*/
static TIndirectArray<struct FAsyncPackage>	GObjAsyncPackages;

int32 FAsyncPackage::PreLoadIndex = 0;
int32 FAsyncPackage::PostLoadIndex = 0;

/**
* Constructor
*/
FAsyncPackage::FAsyncPackage(const FName& InPackageName, const FGuid* InPackageGuid, FName InPackageType, const FName& InPackageNameToLoad)
: PackageName(InPackageName)
, PackageNameToLoad(InPackageNameToLoad)
, PackageGuid(InPackageGuid != NULL ? *InPackageGuid : FGuid(0, 0, 0, 0))
, PackageType(InPackageType)
, Linker(NULL)
, DependencyRefCount(0)
, LoadImportIndex(0)
, ImportIndex(0)
, ExportIndex(0)
, TimeLimit(FLT_MAX)
, bUseTimeLimit(false)
, bUseFullTimeLimit(false)
, bTimeLimitExceeded(false)
, bLoadHasFailed(false)
, bLoadHasFinished(false)
, TickStartTime(0)
, LastObjectWorkWasPerformedOn(NULL)
, LastTypeOfWorkPerformed(NULL)
, LoadStartTime(0.0)
, LoadPercentage(0)
, PackageFlags(0)
#if WITH_EDITOR
, PIEInstanceID(INDEX_NONE)
#endif	
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
}

/**
 * Returns an estimated load completion percentage.
 */
float FAsyncPackage::GetLoadPercentage() const
{
	return LoadPercentage;
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
	if( Linker )
	{
		Linker->Detach(false);
		Linker = nullptr;
	}
}

/**
 * Returns whether time limit has been exceeded.
 *
 * @return true if time limit has been exceeded (and is used), false otherwise
 */
bool FAsyncPackage::IsTimeLimitExceeded()
{
	if( !bTimeLimitExceeded && bUseTimeLimit )
	{
		double CurrentTime = FPlatformTime::Seconds();
		bTimeLimitExceeded = CurrentTime - TickStartTime > TimeLimit;
//		if( GUseSeekFreeLoading )
		{
			// Log single operations that take longer than timelimit.
			if( (CurrentTime - TickStartTime) > (2.5 * TimeLimit) )
			{
 				UE_LOG(LogStreaming, Log, TEXT("FAsyncPackage: %s %s took (less than) %5.2f ms"), 
 					LastTypeOfWorkPerformed, 
 					LastObjectWorkWasPerformedOn ? *LastObjectWorkWasPerformedOn->GetFullName() : TEXT(""), 
 					(CurrentTime - TickStartTime) * 1000);
			}
		}
	}
	return bTimeLimitExceeded;
}

/**
 * Gives up time slice if time limit is enabled.
 *
 * @return true if time slice can be given up, false otherwise
 */
bool FAsyncPackage::GiveUpTimeSlice()
{
	if (bUseTimeLimit && !bUseFullTimeLimit)
	{
		bTimeLimitExceeded = true;
	}
	return bTimeLimitExceeded;
}

/**
 * Begin async loading process. Simulates parts of BeginLoad.
 *
 * Objects created during BeginAsyncLoad and EndAsyncLoad will have RF_AsyncLoading set
 */
void FAsyncPackage::BeginAsyncLoad()
{
	// All objects created from now on should be flagged as RF_AsyncLoading so StaticFindObject doesn't return them.
	GIsAsyncLoading = true;

	// this won't do much during async loading except increase the load count which causes IsLoading to return true
	BeginLoad();
}

/**
 * End async loading process. Simulates parts of EndLoad(). FinishObjects 
 * simulates some further parts once we're fully done loading the package.
 */
void FAsyncPackage::EndAsyncLoad()
{
	check(GIsAsyncLoading);

	// this won't do much during async loading except decrease the load count which causes IsLoading to return false
	EndLoad();

	// We're no longer async loading.
	GIsAsyncLoading = false;

	if (!bLoadHasFailed)
	{
		// Mark the package as loaded, if we succeeded
		Linker->LinkerRoot->SetFlags(RF_WasLoaded);
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

EAsyncPackageState::Type FAsyncPackage::Tick( bool InbUseTimeLimit, bool InbUseFullTimeLimit, float& InOutTimeLimit )
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_Tick);

 	check(LastObjectWorkWasPerformedOn==nullptr);
	check(LastTypeOfWorkPerformed==nullptr);

	// Set up tick relevant variables.
	bUseTimeLimit			= InbUseTimeLimit;
	bUseFullTimeLimit		= InbUseFullTimeLimit;
	bTimeLimitExceeded		= false;
	TimeLimit				= InOutTimeLimit;
	TickStartTime			= FPlatformTime::Seconds();

	// Keep track of time when we start loading.
	if( LoadStartTime == 0.0 )
	{
		LoadStartTime = TickStartTime;
	}

	// Whether we should execute the next step.
	EAsyncPackageState::Type LoadingState	= EAsyncPackageState::Complete;

	STAT(double ThisTime = 0);
	{
		SCOPE_SECONDS_COUNTER(ThisTime);

		// Make sure we finish our work if there's no time limit. The loop is required as PostLoad
		// might cause more objects to be loaded in which case we need to Preload them again.
		do
		{
			// Reset value to true at beginning of loop.
			LoadingState	= EAsyncPackageState::Complete;

			// Begin async loading, simulates BeginLoad and sets GIsAsyncLoading to true.
			BeginAsyncLoad();

			// Create raw linker. Needs to be async created via ticking before it can be used.
			if( LoadingState == EAsyncPackageState::Complete )
			{
				LoadingState = CreateLinker();
			}

			// Async create linker.
			if( LoadingState == EAsyncPackageState::Complete )
			{
				LoadingState = FinishLinker();
			}

			// Load imports from linker import table asynchronously.
			if( LoadingState == EAsyncPackageState::Complete )
			{
				LoadingState = LoadImports();
			}

			// Create imports from linker import table.
			if( LoadingState == EAsyncPackageState::Complete )
			{
				LoadingState = CreateImports();
			}

			// Finish all async texture allocations.
			if( LoadingState == EAsyncPackageState::Complete )
			{
				LoadingState = FinishTextureAllocations();
			}

			// Create exports from linker export table and also preload them.
			if( LoadingState == EAsyncPackageState::Complete )
			{
				LoadingState = CreateExports();
			}

			// Call Preload on the linker for all loaded objects which causes actual serialization.
			if( LoadingState == EAsyncPackageState::Complete )
			{
				LoadingState = PreLoadObjects();
			}

			// Call PostLoad on objects, this could cause new objects to be loaded that require
			// another iteration of the PreLoad loop.
			if (LoadingState == EAsyncPackageState::Complete)
			{
				LoadingState = PostLoadObjects();
			}

			// End async loading, simulates EndLoad and sets GIsAsyncLoading to false.
			EndAsyncLoad();

			// Finish objects (removing RF_AsyncLoading, dissociate imports and forced exports, 
			// call completion callback, ...
			// If the load has failed, perform completion callbacks and then quit
			if( LoadingState == EAsyncPackageState::Complete || bLoadHasFailed )
			{
				LoadingState = FinishObjects();
			}
		// Only exit loop if we're either done or the time limit has been exceeded.
		} while( !IsTimeLimitExceeded() && LoadingState == EAsyncPackageState::TimeOut );	

		check( bUseTimeLimit || LoadingState != EAsyncPackageState::TimeOut );

		// We can't have a reference to a UObject.
		LastObjectWorkWasPerformedOn	= nullptr;
		// Reset type of work performed.
		LastTypeOfWorkPerformed			= nullptr;
		// Mark this package as loaded if everything completed.
		bLoadHasFinished = (LoadingState == EAsyncPackageState::Complete);
		// Subtract the time it took to load this package from the global limit.
		InOutTimeLimit = (float)FMath::Max(0.0, InOutTimeLimit - (FPlatformTime::Seconds() - TickStartTime));
	
	}
	INC_FLOAT_STAT_BY(STAT_FAsyncPackage_TickTime, (float)ThisTime);

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
	if (Linker == nullptr)
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateLinker);

		LastObjectWorkWasPerformedOn	= nullptr;
		LastTypeOfWorkPerformed			= TEXT("creating Linker");

		// Try to find existing package or create it if not already present.
		UPackage* Package = CreatePackage(nullptr, *PackageName.ToString());
		FScopeCycleCounterUObject ConstructorScope(Package, GET_STATID(STAT_FAsyncPackage_CreateLinker));

		// Set package specific data 
		Package->PackageFlags |= PackageFlags;
#if WITH_EDITOR
		Package->PIEInstanceID = PIEInstanceID;
#endif

		// Always store package filename we loading from
		Package->FileName = PackageNameToLoad;

		// if the linker already exists, we don't need to lookup the file (it may have been pre-created with
		// a different filename)
		Linker = ULinkerLoad::FindExistingLinkerForPackage(Package);

		if (!Linker)
		{
			FString PackageFileName;
			if (!FPackageName::DoesPackageExist(PackageNameToLoad.ToString(), PackageGuid.IsValid() ? &PackageGuid : nullptr, &PackageFileName))
			{
				UE_LOG(LogStreaming, Error, TEXT("Couldn't find file for package %s requested by async loading code."), *PackageName.ToString());
				bLoadHasFailed = true;
				return EAsyncPackageState::TimeOut;
			}

			// Create raw async linker, requiring to be ticked till finished creating.
			Linker = ULinkerLoad::CreateLinkerAsync( Package, *PackageFileName, (FApp::IsGame() && !GIsEditor) ? (LOAD_SeekFree | LOAD_NoVerify) : LOAD_None  );
		}

		UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::CreateLinker for %s finished."), *PackageNameToLoad.ToString());
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
	if( !Linker->HasFinishedInitialization() )
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FinishLinker);
		LastObjectWorkWasPerformedOn	= Linker->LinkerRoot;
		LastTypeOfWorkPerformed			= TEXT("ticking linker");
	
		// Operation still pending if Tick returns false
		if( Linker->Tick( TimeLimit, bUseTimeLimit, bUseFullTimeLimit ) != ULinkerLoad::LINKER_Loaded)
		{
			// Give up remainder of timeslice if there is one to give up.
			GiveUpTimeSlice();
			return EAsyncPackageState::TimeOut;
		}
	}

	return EAsyncPackageState::Complete;
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
 * Finds an existing async package in the GObjAsyncPackages by its name.
 *
 * @param PackageName async package name.
 * @return Index of the async package in GObjAsyncPackages array or INDEX_NONE if not found.
 */
FORCEINLINE int32 FindAsyncPackage(const FName& PackageName)
{
	for (int32 PackageIndex = 0; PackageIndex < GObjAsyncPackages.Num(); ++PackageIndex)
	{
		FAsyncPackage& PendingPackage = GObjAsyncPackages[PackageIndex];
		if (PendingPackage.GetPackageName() == PackageName)
		{
			return PackageIndex;
		}
	}
	return INDEX_NONE;
}
/**
	* Adds a package to the list of pending import packages.
	*
	* @param PendingImport Name of the package imported either directly or by one of the imported packages
	*/
void FAsyncPackage::AddImportDependency(int32 CurrentPackageIndex, const FName& PendingImport)
{
	FAsyncPackage* PackageToStream = nullptr;
	int32 ExistingAsyncPackageIndex = FindAsyncPackage(PendingImport);
	if (ExistingAsyncPackageIndex == INDEX_NONE)
	{
		PackageToStream = new FAsyncPackage(PendingImport, nullptr, NAME_None, PendingImport);
		GObjAsyncPackages.Insert(PackageToStream, CurrentPackageIndex);
	}
	else
	{
		PackageToStream = &GObjAsyncPackages[ExistingAsyncPackageIndex];
	}
	
	if (!PackageToStream->HasFinishedLoading() && 
		!PackageToStream->bLoadHasFailed)
	{
		PackageToStream->AddCompletionCallback(FLoadPackageAsyncDelegate::CreateRaw(this, &FAsyncPackage::ImportFullyLoadedCallback));
		PackageToStream->DependencyRefCount++;
		PendingImportedPackages.Add(PackageToStream);
	}
	else
	{
		PackageToStream->DependencyRefCount++;
		ReferencedImports.Add(PackageToStream);
	}
}

/**
 * Adds a unique package to the list of packages to wait for until their linkers have been created.
 *
 * @param PendingImport Package imported either directly or by one of the imported packages
 */
bool FAsyncPackage::AddUniqueLinkerDependencyPackage(int32 CurrentPackageIndex, FAsyncPackage& PendingImport)
{
	if (ContainsDependencyPackage(PendingImportedPackages, PendingImport.GetPackageName()) == INDEX_NONE)
	{
		if (PendingImport.Linker == nullptr || !PendingImport.Linker->HasFinishedInitialization())
		{
			AddImportDependency(CurrentPackageIndex, PendingImport.GetPackageName());
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
void FAsyncPackage::AddDependencyTree(int32 CurrentPackageIndex, FAsyncPackage& ImportedPackage, TSet<FAsyncPackage*>& SearchedPackages)
{
	if (SearchedPackages.Contains(&ImportedPackage))
	{
		// we've already searched this package
		return;
	}
	for (int32 Index = 0; Index < ImportedPackage.PendingImportedPackages.Num(); ++Index)
	{
		FAsyncPackage& PendingImport = *ImportedPackage.PendingImportedPackages[Index];
		if (!AddUniqueLinkerDependencyPackage(CurrentPackageIndex, PendingImport))
		{
			AddDependencyTree(CurrentPackageIndex, PendingImport, SearchedPackages);
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
	LastObjectWorkWasPerformedOn	= Linker->LinkerRoot;
	LastTypeOfWorkPerformed			= TEXT("loading imports");

	// Index of this package in the async queue.
	const int32 AsyncQueueIndex = FindAsyncPackage(GetPackageName());

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

		// Our import package name is the import name
		const FName ImportPackageFName(Import->ObjectName);

		// Handle circular dependencies - try to find existing packages.
		UPackage* ExistingPackage = dynamic_cast<UPackage*>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, ImportPackageFName, true));
		if (ExistingPackage && !(ExistingPackage->PackageFlags & PKG_CompiledIn) && !ExistingPackage->bHasBeenFullyLoaded)//!ExistingPackage->HasAnyFlags(RF_WasLoaded))
		{
			// The import package already exists. Check if it's currently being streamed as well. If so, make sure
			// we add all dependencies that don't yet have linkers created otherwise we risk that if the current package
			// doesn't depend on any other packages that have not yet started streaming, creating imports is going
			// to load packages blocking the main thread.
			int32 PendingAsyncPackageIndex = FindAsyncPackage(ImportPackageFName);
			if (PendingAsyncPackageIndex != INDEX_NONE)
			{
				FAsyncPackage& PendingPackage = GObjAsyncPackages[PendingAsyncPackageIndex];
				if (PendingPackage.Linker == nullptr || !PendingPackage.Linker->HasFinishedInitialization())
				{
					// Add this import to the dependency list.
					AddUniqueLinkerDependencyPackage(AsyncQueueIndex, PendingPackage);
				}
				else
				{
					UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Linker exists for %s"), *PackageNameToLoad.ToString(), *ImportPackageFName.ToString());
					// Only keep a reference to this package so that its linker doesn't go away too soon
					PendingPackage.DependencyRefCount++;
					ReferencedImports.Add(&PendingPackage);
					// Check if we need to add its dependencies too.
					TSet<FAsyncPackage*> SearchedPackages;
					AddDependencyTree(AsyncQueueIndex, PendingPackage, SearchedPackages);
				}
			}
		}

		if (!ExistingPackage && ContainsDependencyPackage(PendingImportedPackages, ImportPackageFName) == INDEX_NONE)
		{			
			const FString ImportPackageName(Import->ObjectName.ToString());
			// The package doesn't exist and this import is not in the dependency list so add it now.
			if (!FPackageName::IsShortPackageName(ImportPackageName))
			{
				UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Loading %s"), *PackageNameToLoad.ToString(), *ImportPackageName);

				AddImportDependency(AsyncQueueIndex, ImportPackageFName);
			}
			else
			{
				// This usually means there's a reference to a script package from another project
				UE_LOG(LogStreaming, Warning, TEXT("FAsyncPackage::LoadImports for %s: Short package name in imports list: %s"), *PackageNameToLoad.ToString(), *ImportPackageName);
			}
		}
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
void FAsyncPackage::ImportFullyLoadedCallback(const FName& InPackageName, UPackage* LoadedPackage)
{
	UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Loaded %s"), *PackageNameToLoad.ToString(), *InPackageName.ToString());
	int32 Index = ContainsDependencyPackage(PendingImportedPackages, InPackageName);
	check(Index != INDEX_NONE);
	// Keep a reference to this package so that its linker doesn't go away too soon
	ReferencedImports.Add(PendingImportedPackages[Index]);
	PendingImportedPackages.RemoveAt(Index);
}

/** 
 * Create imports till time limit is exceeded.
 *
 * @return true if we finished creating all imports, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::CreateImports()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateImports);

	// Create imports.
	while( ImportIndex < Linker->ImportMap.Num() && !IsTimeLimitExceeded() )
	{
 		UObject* Object	= Linker->CreateImport( ImportIndex++ );
		LastObjectWorkWasPerformedOn	= Object;
		LastTypeOfWorkPerformed			= TEXT("creating imports for");
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

/**
 * Create exports till time limit is exceeded.
 *
 * @return true if we finished creating and preloading all exports, false otherwise.
 */
EAsyncPackageState::Type FAsyncPackage::CreateExports()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateExports);

	// Create exports.
	while( ExportIndex < Linker->ExportMap.Num() && !IsTimeLimitExceeded() )
	{
		const FObjectExport& Export = Linker->ExportMap[ExportIndex];
		
		// Precache data and see whether it's already finished.

		// We have sufficient data in the cache so we can load.
		if( Linker->Precache( Export.SerialOffset, Export.SerialSize ) )
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
			LastTypeOfWorkPerformed			= TEXT("creating exports for");
			// This assumes that only CreateExports is taking a significant amount of time.
			LoadPercentage = 100.f * ExportIndex / Linker->ExportMap.Num();
		}
		// Data isn't ready yet. Give up remainder of time slice if we're not using a time limit.
		else if( GiveUpTimeSlice() )
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
	for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencedImports.Num(); ++ReferenceIndex)
	{
		FAsyncPackage& Ref = *ReferencedImports[ReferenceIndex];
		Ref.DependencyRefCount--;
		UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::FreeReferencedImports for %s: Releasing %s (%d)"), *PackageNameToLoad.ToString(), *Ref.GetPackageName().ToString(), Ref.DependencyRefCount);
		check(Ref.DependencyRefCount >= 0);
	}
	ReferencedImports.Empty();
}

/**
 * Preloads aka serializes all loaded objects.
 *
 * @return true if we finished serializing all loaded objects, false otherwise.
 */
EAsyncPackageState::Type FAsyncPackage::PreLoadObjects()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PreLoadObjects);

	// Preload (aka serialize) the objects.
	while( PreLoadIndex < GObjLoaded.Num() && !IsTimeLimitExceeded() )
	{
		//@todo async: make this part async as well.
		UObject* Object = GObjLoaded[ PreLoadIndex++ ];
		if( Object && Object->GetLinker() )
		{
			Object->GetLinker()->Preload( Object );
			LastObjectWorkWasPerformedOn	= Object;
			LastTypeOfWorkPerformed			= TEXT("preloading");
		}
	}

	return PreLoadIndex == GObjLoaded.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

/**
 * Route PostLoad to all loaded objects. This might load further objects!
 *
 * @return true if we finished calling PostLoad on all loaded objects and no new ones were created, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::PostLoadObjects()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PostLoadObjects);
	
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	STAT(double ThisTime = 0);
	{
		SCOPE_SECONDS_COUNTER(ThisTime);
		TGuardValue<bool> GuardIsRoutingPostLoad(GIsRoutingPostLoad, true);

		// PostLoad objects.
		while( PostLoadIndex < GObjLoaded.Num() && PostLoadIndex < PreLoadIndex && !IsTimeLimitExceeded() )
		{
			UObject* Object	= GObjLoaded[ PostLoadIndex++ ];
			check(Object);
			FScopeCycleCounterUObject ConstructorScope(Object, GET_STATID(STAT_FAsyncPackage_PostLoadObjects));

			Object->ConditionalPostLoad();

			LastObjectWorkWasPerformedOn	= Object;
			LastTypeOfWorkPerformed			= TEXT("postloading");
		}

		// New objects might have been loaded during PostLoad.
		Result = (PreLoadIndex == GObjLoaded.Num()) && (PostLoadIndex == GObjLoaded.Num()) ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
	}
	INC_FLOAT_STAT_BY(STAT_FAsyncPackage_PostLoadObjectsTime, (float)ThisTime);

	return Result;
}

/**
 * Finish up objects and state, which means clearing the RF_AsyncLoading flag on newly created ones
 *
 * @return true
 */
EAsyncPackageState::Type FAsyncPackage::FinishObjects()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FinishObjects);
	LastObjectWorkWasPerformedOn	= nullptr;
	LastTypeOfWorkPerformed			= TEXT("finishing all objects");

	// All loaded objects are now finished loading so we can clear the RF_AsyncLoading flag so
	// StaticFindObject returns them by default.
	for( int32 ObjectIndex=0; ObjectIndex<GObjConstructedDuringAsyncLoading.Num(); ObjectIndex++ )
	{
		UObject* Object = GObjConstructedDuringAsyncLoading[ObjectIndex];
		Object->ClearFlags( RF_AsyncLoading );
	}
	GObjConstructedDuringAsyncLoading.Empty();
			
	// Simulate what EndLoad does.
	GObjLoaded.Empty();
	DissociateImportsAndForcedExports(); //@todo: this should be avoidable
	PreLoadIndex = 0;
	PostLoadIndex = 0;


	// If we successfully loaded
	if (!bLoadHasFailed)
	{
		// Mark package as having been fully loaded and update load time.
		if( Linker->LinkerRoot )
		{
			Linker->LinkerRoot->MarkAsFullyLoaded();
			Linker->LinkerRoot->SetLoadTime( FPlatformTime::Seconds() - LoadStartTime );
		}

		// Call any completion callbacks specified.
		for (int32 i = 0; i < CompletionCallbacks.Num(); i++)
		{
			CompletionCallbacks[i].ExecuteIfBound(PackageName, Linker->LinkerRoot);
		}

		// give a hint to the IO system that we are done with this file for now
		FIOSystem::Get().HintDoneWithFile(Linker->Filename);

	#if WITH_ENGINE
		// Cancel all texture allocations that haven't been claimed yet.
		Linker->Summary.TextureAllocations.CancelRemainingAllocations( true );
	#endif		// WITH_ENGINE

		// Flush cache to free memory
		if (Linker != nullptr)
		{
			Linker->FlushCache();
		}
	}
	else
	{
		// On failure, just call completion callbacks

		// Call any completion callbacks specified.
		for (int32 i = 0; i < CompletionCallbacks.Num(); i++)
		{
			CompletionCallbacks[i].ExecuteIfBound(PackageName, nullptr);
		}
	}

	// Flush cache of linkers that were requirested to be closed to free memory
	extern TArray<ULinkerLoad*>	GDelayedLinkerClosePackages;
	for (int32 Index = 0; Index < GDelayedLinkerClosePackages.Num(); Index++)
	{
		ULinkerLoad* DelayedLinker = GDelayedLinkerClosePackages[Index];
		if (DelayedLinker)
		{
			DelayedLinker->FlushCache();
		}
	}

	return EAsyncPackageState::Complete;
}


/*-----------------------------------------------------------------------------
	UObject async (pre)loading.
-----------------------------------------------------------------------------*/
FAsyncPackage& LoadPackageAsync( const FString& InPackageName, const FGuid* PackageGuid, FName PackageType, const TCHAR* InPackageToLoadFrom)
{
	// The comments clearly state that it should be a package name but we also handle it being a filename as this function is not perf critical
	// and LoadPackage handles having a filename being passed in as well.
	FString PackageName;
	
	if ( FPackageName::IsValidLongPackageName(InPackageName, /*bIncludeReadOnlyRoots*/true) )
	{
		PackageName = InPackageName;
	}
	else if ( FPackageName::IsPackageFilename(InPackageName) && FPackageName::TryConvertFilenameToLongPackageName(InPackageName, PackageName) )
	{
		// PackageName got populated by the conditional function
	}
	else
	{
		// PackageName will get populated by the conditional function
		FString ClassName;
		if (!FPackageName::ParseExportTextPath(InPackageName, &ClassName, &PackageName))
		{
			UE_LOG(LogStreaming, Fatal, TEXT("LoadPackageAsync failed to begin to load a level because the supplied package name was neither a valid long package name nor a filename of a map within a content folder: '%s'"), *InPackageName);
		}
	}

	// When constructing the FName from filename, we need to make sure that if the filename ends with _number, we're not splitting the FName
	FName PackageFName(/*ENAME_LinkerConstructor,*/ *PackageName);

	// Check whether the file is already in the queue.	
	for( int32 PackageIndex=0; PackageIndex<GObjAsyncPackages.Num(); PackageIndex++ )
	{
		FAsyncPackage& PendingPackage = GObjAsyncPackages[PackageIndex];
		if (PendingPackage.GetPackageName() == PackageFName)
		{
			// Early out as package is already being preloaded.
			return PendingPackage;
		}
	}
	FString PackageToLoadFrom = PackageName;
	if (InPackageToLoadFrom)
	{
		PackageToLoadFrom = FString(InPackageToLoadFrom);
	}
	// Make sure long package name is passed to FAsyncPackage so that it doesn't attempt to 
	// create a package with short name.
	if (FPackageName::IsShortPackageName(PackageToLoadFrom))
	{
		UE_LOG(LogStreaming, Fatal, TEXT("Async loading code requires long package names (%s)."), *InPackageName);
	}
	// Add to (FIFO) queue.
	FAsyncPackage *Package = new(GObjAsyncPackages)FAsyncPackage(PackageFName, PackageGuid, PackageType, FName(/*ENAME_LinkerConstructor,*/ *PackageToLoadFrom));
	return *Package;
}

FAsyncPackage& LoadPackageAsync( const FString& InPackageName, FLoadPackageAsyncDelegate CompletionDelegate, const FGuid* PackageGuid, FName PackageType, const TCHAR* InPackageToLoadFrom)
{
	FAsyncPackage& AsyncPackage = LoadPackageAsync(InPackageName, PackageGuid, PackageType, InPackageToLoadFrom);
	AsyncPackage.AddCompletionCallback(CompletionDelegate);
	return AsyncPackage;
}

/**
 * Returns the async load percentage for a package in flight with the passed in name or -1 if there isn't one.
 *
 * @param	PackageName			Name of package to query load percentage for
 * @return	Async load percentage if package is currently being loaded, -1 otherwise
 */
float GetAsyncLoadPercentage( const FName& PackageName )
{
	float LoadPercentage = -1.f;
	for( int32 PackageIndex=0; PackageIndex<GObjAsyncPackages.Num(); PackageIndex++ )
	{
		const FAsyncPackage& PendingPackage = GObjAsyncPackages[PackageIndex];
		if( PendingPackage.GetPackageName() == PackageName )
		{
			LoadPercentage = PendingPackage.GetLoadPercentage();
			break;
		}
	}
	return LoadPercentage;
}


/**
 * Blocks till all pending package/ linker requests are fulfilled.
 *
 * @param	ExcludeType					Do not flush packages associated with this specific type name
 */
void FlushAsyncLoading(FName ExcludeType/*=NAME_None*/)
{
	if( GObjAsyncPackages.Num() )
	{
		// Disallow low priority requests like texture streaming while we are flushing streaming
		// in order to avoid excessive seeking.
		FIOSystem::Get().SetMinPriority( AIOP_Normal );

		// Flush async loaders by not using a time limit. Needed for e.g. garbage collection.
		UE_LOG(LogStreaming, Log,  TEXT("Flushing async loaders.") );
		STAT(double ThisTime = 0);
		{
			SCOPE_SECONDS_COUNTER(ThisTime);
			while (ProcessAsyncLoading(false, false, 0, ExcludeType) == EAsyncPackageState::PendingImports) {}
		}
		INC_FLOAT_STAT_BY(STAT_FAsyncPackage_FlushAsyncLoadingTime, (float)ThisTime);
		UE_LOG(LogStreaming, Log,  TEXT("Flushed async loaders.") );

		if (ExcludeType == NAME_None)
		{
			// It's fine to have pending loads if we excluded some from the check
			check( !IsAsyncLoading() );
		}

		// Reset min priority again.
		FIOSystem::Get().SetMinPriority( AIOP_MIN );
	}
}

/**
 * Returns whether we are currently async loading a package.
 *
 * @return true if we are async loading a package, false otherwise
 */
bool IsAsyncLoading()
{
	return GObjAsyncPackages.Num() > 0 ? true : false;
}

/**
 * @return number of active async load package requests
 */
int32 GetNumAsyncPackages()
{
	return GObjAsyncPackages.Num() ;
}

/**
 * Serializes a bit of data each frame with a soft time limit. The function is designed to be able
 * to fully load a package in a single pass given sufficient time.
 *
 * @param	bUseTimeLimit	Whether to use a time limit
 * @param	bUseFullTimeLimit	If true, use the entire time limit even if blocked on I/O
 * @param	TimeLimit		Soft limit of time this function is allowed to consume
 * @param	ExcludeType		Do not process packages associated with this specific type name
 */
EAsyncPackageState::Type ProcessAsyncLoading( bool bUseTimeLimit, bool bUseFullTimeLimit, float TimeLimit, FName ExcludeType )
{
	SCOPE_CYCLE_COUNTER(STAT_AsyncLoadingTime);
	// Whether to continue execution.
	EAsyncPackageState::Type LoadingState = EAsyncPackageState::Complete;
	EAsyncPackageState::Type CompletionState = EAsyncPackageState::Complete;

	bool bWasLoading = GObjAsyncPackages.Num() > 0;
	// We need to loop as the function has to handle finish loading everything given no time limit
	// like e.g. when called from FlushAsyncLoading.
	for (int32 i = 0; LoadingState != EAsyncPackageState::TimeOut && i < GObjAsyncPackages.Num(); i++)
	{
		// Package to be loaded.
		FAsyncPackage& Package = GObjAsyncPackages[i];

		if (ExcludeType != NAME_None && ExcludeType == Package.GetPackageType())
		{
			// We should skip packages of this type
			continue;
		}

		
		if (Package.HasFinishedLoading() == false)
		{
			// Package tick returns EAsyncPackageState::Complete on completion.
			// We only tick packages that have not yet been loaded.
			LoadingState = Package.Tick( bUseTimeLimit, bUseFullTimeLimit, TimeLimit );
		}
		else
		{
			// This package has finished loading but some other package is still holding
			// a reference to it because it has this package in its dependency list.
			LoadingState = EAsyncPackageState::Complete;
		}
		if( LoadingState == EAsyncPackageState::Complete )
		{
			// Only remove packages that are not being referenced by anything (we still need their linkers
			// when the referencing packages create their imports and exports).
			if (Package.GetDependencyRefCount() == 0)
			{
				if( FPlatformProperties::RequiresCookedData() )
				{
					// Emulates ResetLoaders on the package linker's linkerroot.
					Package.ResetLoader();
				}

				// We're done so we can remove the package now. @warning invalidates local Package variable!.
				GObjAsyncPackages.RemoveAt( i );

				// Need to process this index again as we just removed an item
				i--;
			}
			else
			{
				// This package is being referenced by another package so we can't say streaming is complete yet.
				LoadingState = EAsyncPackageState::PendingImports;
			}
		}
		else if (!bUseTimeLimit && !FPlatformProcess::SupportsMultithreading())
		{
			// Tick async loading when multithreading is disabled.
			FIOSystem::Get().TickSingleThreaded();
		}
		CompletionState = FMath::Min(CompletionState, LoadingState);
		// We cannot access Package anymore!
	}

	// Not that streaming has finished, we can stop force-referencing all objects that were created during async load.
	if (CompletionState == EAsyncPackageState::Complete && bWasLoading)
	{
		FAsyncObjectsReferencer::Get().EmptyReferencedObjects();
	}

	return CompletionState;
}


/*----------------------------------------------------------------------------
	FArchiveAsync.
----------------------------------------------------------------------------*/

/**
 * Constructor, initializing all member variables.
 */
FArchiveAsync::FArchiveAsync( const TCHAR* InFileName )
:	FileName					( InFileName	)
,	FileSize					( INDEX_NONE	)
,	UncompressedFileSize		( INDEX_NONE	)
,	BulkDataAreaSize			( 0	)
,	CurrentPos					( 0				)
,	CompressedChunks			( nullptr			)
,	CurrentChunkIndex			( 0				)
,	CompressionFlags			( COMPRESS_None	)
{
	ArIsLoading		= true;
	ArIsPersistent	= true;

	PrecacheStartPos[CURRENT]	= 0;
	PrecacheEndPos[CURRENT]		= 0;
	PrecacheBuffer[CURRENT]		= nullptr;

	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;
	PrecacheBuffer[NEXT]		= nullptr;

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
	while( PrecacheReadStatus[CURRENT].GetValue() || PrecacheReadStatus[NEXT].GetValue() )
	{
		SHUTDOWN_IF_EXIT_REQUESTED;
		FPlatformProcess::Sleep(0.0001);
	}
	uint32 Delta = 0;

	// Invalidate any precached data and free memory for current buffer.
	Delta += PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT];
	FMemory::Free( PrecacheBuffer[CURRENT] );
	PrecacheBuffer[CURRENT]		= nullptr;
	PrecacheStartPos[CURRENT]	= 0;
	PrecacheEndPos[CURRENT]		= 0;
	
	// Invalidate any precached data and free memory for next buffer.
	FMemory::Free( PrecacheBuffer[NEXT] );
	PrecacheBuffer[NEXT]		= nullptr;
	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;

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
	FMemory::Free( PrecacheBuffer[CURRENT] );
	PrecacheBuffer[CURRENT]		= PrecacheBuffer[NEXT];
	PrecacheStartPos[CURRENT]	= PrecacheStartPos[NEXT];
	PrecacheEndPos[CURRENT]		= PrecacheEndPos[NEXT];

	// Next buffer is unused/ free.
	PrecacheBuffer[NEXT]		= nullptr;
	PrecacheStartPos[NEXT]		= 0;
	PrecacheEndPos[NEXT]		= 0;
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
	FMemory::Free( PrecacheBuffer[BufferIndex] );
	PrecacheBuffer[BufferIndex]		= (uint8*) FMemory::Malloc( PrecacheEndPos[BufferIndex] - PrecacheStartPos[BufferIndex] );
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

/**
 * Hint the archive that the region starting at passed in offset and spanning the passed in size
 * is going to be read soon and should be precached.
 *
 * The function returns whether the precache operation has completed or not which is an important
 * hint for code knowing that it deals with potential async I/O. The archive is free to either not 
 * implement this function or only partially precache so it is required that given sufficient time
 * the function will return true. Archives not based on async I/O should always return true.
 *
 * This function will not change the current archive position.
 *
 * @param	RequestOffset	Offset at which to begin precaching.
 * @param	RequestSize		Number of bytes to precache
 * @return	false if precache operation is still pending, true otherwise
 */
bool FArchiveAsync::Precache( int64 RequestOffset, int64 RequestSize )
{
	// Check whether we're currently waiting for a read request to finish.
	bool bFinishedReadingCurrent	= PrecacheReadStatus[CURRENT].GetValue()==0 ? true : false;
	bool bFinishedReadingNext		= PrecacheReadStatus[NEXT].GetValue()==0 ? true : false;

	// Return read status if the current request fits entirely in the precached region.
	if( PrecacheBufferContainsRequest( RequestOffset, RequestSize ) )
	{
		if (!bFinishedReadingCurrent && !FPlatformProcess::SupportsMultithreading())
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
			FMemory::Free( PrecacheBuffer[CURRENT] );

			PrecacheBuffer[CURRENT]		= (uint8*) FMemory::Malloc( PrecacheEndPos[CURRENT] - PrecacheStartPos[CURRENT] );
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
									AIOP_Normal );
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
void FArchiveAsync::Serialize( void* Data, int64 Count )
{
	// Ensure we aren't reading beyond the end of the file
	checkf( CurrentPos + Count <= TotalSize(), TEXT("Seeked past end of file %s (%lld / %lld)"), *FileName, CurrentPos + Count, TotalSize() );

	double	StartTime	= 0;
	bool	bIOBlocked	= false;

	// Make sure serialization request fits entirely in already precached region.
	if( !PrecacheBufferContainsRequest( CurrentPos, Count ) )
	{
		// Keep track of time we started to block.
		StartTime	= FPlatformTime::Seconds();
		bIOBlocked	= true;

		// Busy wait for region to be precached.
		while( !Precache( CurrentPos, Count ) )
		{
			SHUTDOWN_IF_EXIT_REQUESTED;
			if (FPlatformProcess::SupportsMultithreading())
			{
				FPlatformProcess::Sleep(0);
			}
			else
			{
				FIOSystem::Get().TickSingleThreaded();
			}	
		}

		// There shouldn't be any outstanding read requests for the main buffer at this point.
		check( PrecacheReadStatus[CURRENT].GetValue() == 0 );
	}
	
	// Make sure to wait till read request has finished before progressing. This can happen if PreCache interface
	// is not being used for serialization.
	while( PrecacheReadStatus[CURRENT].GetValue() != 0 )
	{
		SHUTDOWN_IF_EXIT_REQUESTED;
		// Only update StartTime if we haven't already started blocking I/O above.
		if( !bIOBlocked )
		{
			// Keep track of time we started to block.
			StartTime	= FPlatformTime::Seconds();
			bIOBlocked	= true;
		}
		if (FPlatformProcess::SupportsMultithreading())
		{
			FPlatformProcess::Sleep(0);
		}
		else
		{
			FIOSystem::Get().TickSingleThreaded();
		}		
	}

	// Update stats if we were blocked.
#if STATS
	if( bIOBlocked )
	{
		double BlockingTime = FPlatformTime::Seconds() - StartTime;
		INC_FLOAT_STAT_BY(STAT_AsyncIO_MainThreadBlockTime,(float)BlockingTime);
#if LOOKING_FOR_PERF_ISSUES
		UE_LOG(LogStreaming, Warning, TEXT("FArchiveAsync::Serialize: %5.2fms blocking on read from '%s' (Offset: %lld, Size: %lld)"), 
			1000 * BlockingTime, 
			*FileName, 
			CurrentPos, 
			Count );
#endif
	}
#endif

	// Copy memory to destination.
	FMemory::Memcpy( Data, PrecacheBuffer[CURRENT] + (CurrentPos - PrecacheStartPos[CURRENT]), Count );
	// Serialization implicitly increases position in file.
	CurrentPos += Count;
}

/**
 * Returns the current position in the archive as offset in bytes from the beginning.
 *
 * @return	Current position in the archive (offset in bytes from the beginning)
 */
int64 FArchiveAsync::Tell()
{
	return CurrentPos;
}

/**
 * Returns the total size of the archive in bytes.
 *
 * @return total size of the archive in bytes
 */
int64 FArchiveAsync::TotalSize()
{
	return UncompressedFileSize + BulkDataAreaSize;
}

/**
 * Sets the current position.
 *
 * @param InPos	New position (as offset from beginning in bytes)
 */
void FArchiveAsync::Seek( int64 InPos )
{
	check( InPos >= 0 && InPos <= TotalSize() );
	CurrentPos = InPos;
}


/*----------------------------------------------------------------------------
	End.
----------------------------------------------------------------------------*/
