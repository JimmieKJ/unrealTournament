// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncLoading.h: Unreal async loading definitions.
=============================================================================*/

#pragma once

/**
 * Structure containing intermediate data required for async loading of all imports and exports of a
 * ULinkerLoad.
 */
struct FAsyncPackage : public FGCObject
{
	/**
	 * Constructor
	 */
	FAsyncPackage(const FName& InPackageName, const FGuid* InPackageGuid, FName InPackageType, const FName& InPackageNameToLoad);

	/**
	 * Ticks the async loading code.
	 *
	 * @param	InbUseTimeLimit		Whether to use a time limit
	 * @param	InbUseFullTimeLimit	If true use the entire time limit, even if you have to block on IO
	 * @param	InTimeLimit			Soft limit to time this function may take
	 *
	 * @return	true if package has finished loading, false otherwise
	 */
	EAsyncPackageState::Type Tick( bool bUseTimeLimit, bool bInbUseFullTimeLimit, float& InOutTimeLimit );

	/**
	 * @return Estimated load completion percentage.
	 */
	float GetLoadPercentage() const;

	/**
	 * @return Time load begun. This is NOT the time the load was requested in the case of other pending requests.
	 */
	double GetLoadStartTime() const;

	/**
	 * Emulates ResetLoaders for the package's Linker objects, hence deleting it. 
	 */
	void ResetLoader();

	/**
	 * Returns the name of the package to load.
	 */
	FORCEINLINE const FName& GetPackageName() const
	{
		return PackageName;
	}

	/**
	 * Returns the type name associated with this package
	 */
	const FName &GetPackageType() const
	{
		return PackageType;
	}

	void AddCompletionCallback(const FLoadPackageAsyncDelegate& Callback)
	{
		// This is to ensure that there is no one trying to subscribe to a already loaded package
		check(!bLoadHasFinished && !bLoadHasFailed);
		
		CompletionCallbacks.AddUnique(Callback);
	}

	/**
	 * Serialize the linke for garbage collection.
	 * 
	 * @param Ar The archive to serialize with
	 */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( Linker );
	}

	/**
	 *	Sets the flags and PIE ID for the package
	 */
	void SetPackageData(uint32 InPackageFlags, int32 InPIEInstanceID)
	{
		PackageFlags = InPackageFlags;
#if WITH_EDITOR
		PIEInstanceID = InPIEInstanceID;
#endif
	}

	/** Gets the number of references to this package from other packages in the dependency tree. */
	FORCEINLINE int32 GetDependencyRefCount() const
	{
		return DependencyRefCount;
	}

	/** Returns true if the package has finished loading. */
	FORCEINLINE int32 HasFinishedLoading() const
	{
		return bLoadHasFinished;
	}

private:
	/** Name of the UPackage to create.																	*/
	FName						PackageName;
	/** Name of the package to load.																	*/
	FName						PackageNameToLoad;
	/** GUID of the package to load, or the zeroed invalid GUID for "don't care" */
	FGuid						PackageGuid;
	/** An abstract type name associated with this package, for tagging use								*/
	FName						PackageType;
	/** Linker which is going to have its exports and imports loaded									*/
	ULinkerLoad*				Linker;
	/** Call backs called when we finished loading this package											*/
	TArray<FLoadPackageAsyncDelegate>	CompletionCallbacks;
	/** Pending Import packages - we wait until all of them have been fully loaded. */
	TArray<FAsyncPackage*> PendingImportedPackages;
	/** Referenced imports - list of packages we need until we finish loading this package. */
	TArray<FAsyncPackage*> ReferencedImports;
	/** Number of references to this package from other packages in the dependency tree. */
	int32							DependencyRefCount;
	/** Current index into linkers import table used to spread creation over several frames				*/
	int32							LoadImportIndex;
	/** Current index into linkers import table used to spread creation over several frames				*/
	int32							ImportIndex;
	/** Current index into linkers export table used to spread creation over several frames				*/
	int32							ExportIndex;
	/** Current index into GObjLoaded array used to spread routing PreLoad over several frames			*/
	static int32					PreLoadIndex;
	/** Current index into GObjLoaded array used to spread routing PostLoad over several frames			*/
	static int32					PostLoadIndex;
	/** Currently used time limit for this tick.														*/
	float						TimeLimit;
	/** Whether we are using a time limit for this tick.												*/
	bool						bUseTimeLimit;
	/** Whether we should use the entire time limit, even if we're blocked on I/O						*/
	bool						bUseFullTimeLimit;
	/** Whether we already exceed the time limit this tick.												*/
	bool						bTimeLimitExceeded;
	/** True if our load has failed */
	bool						bLoadHasFailed;
	/** True if our load has finished */
	bool						bLoadHasFinished;
	/** The time taken when we started the tick.														*/
	double						TickStartTime;
	/** Last object work was performed on. Used for debugging/ logging purposes.						*/
	UObject*					LastObjectWorkWasPerformedOn;
	/** Last type of work performed on object.															*/
	const TCHAR*				LastTypeOfWorkPerformed;
	/** Time load begun. This is NOT the time the load was requested in the case of pending requests.	*/
	double						LoadStartTime;
	/** Estimated load percentage.																		*/
	float						LoadPercentage;
	/** The flags that should be applied to the package */
	uint32						PackageFlags;
#if WITH_EDITOR
	/** Editor only: PIE instance ID this package belongs to, INDEX_NONE otherwise */
	int32						PIEInstanceID;
#endif


public:
#if PERF_TRACK_DETAILED_ASYNC_STATS
	/** Number of times Tick function has been called.													*/
	int32							TickCount;
	/** Number of iterations in loop inside Tick.														*/
	int32							TickLoopCount;

	/** Number of iterations for CreateLinker.															*/
	int32							CreateLinkerCount;
	/** Number of iterations for FinishLinker.															*/
	int32							FinishLinkerCount;
	/** Number of iterations for CreateImports.															*/
	int32							CreateImportsCount;
	/** Number of iterations for CreateExports.															*/
	int32							CreateExportsCount;
	/** Number of iterations for PreLoadObjects.														*/
	int32							PreLoadObjectsCount;
	/** Number of iterations for PostLoadObjects.														*/
	int32							PostLoadObjectsCount;
	/** Number of iterations for FinishObjects.															*/
	int32							FinishObjectsCount;

	/** Total time spent in Tick.																		*/
	double						TickTime;
	/** Total time spent in	CreateLinker.																*/
	double						CreateLinkerTime;
	/** Total time spent in FinishLinker.																*/
	double						FinishLinkerTime;
	/** Total time spent in CreateImports.																*/
	double						CreateImportsTime;
	/** Total time spent in CreateExports.																*/
	double						CreateExportsTime;
	/** Total time spent in PreLoadObjects.																*/
	double						PreLoadObjectsTime;
	/** Total time spent in PostLoadObjects.															*/
	double						PostLoadObjectsTime;
	/** Total time spent in FinishObjects.																*/
	double						FinishObjectsTime;

	/** Map of each class of object loaded to the total time spent calling CreateExport on those objects */
	TMap<const UClass*,FMapTimeEntry>		CreateExportTimePerClass;
	/** Map of each class of object loaded to the total time spent calling PostLoad on those objects */
	TMap<const UClass*,FMapTimeEntry>		PostLoadTimePerClass;

#endif

private:
	/**
	 * Gives up time slice if time limit is enabled.
	 *
	 * @return true if time slice can be given up, false otherwise
	 */
	bool GiveUpTimeSlice();
	/**
	 * Returns whether time limit has been exceeded.
	 *
	 * @return true if time limit has been exceeded (and is used), false otherwise
	 */
	bool IsTimeLimitExceeded();
	/**
	 * Begin async loading process. Simulates parts of BeginLoad.
	 *
	 * Objects created during BeginAsyncLoad and EndAsyncLoad will have RF_AsyncLoading set
	 */
	void BeginAsyncLoad();
	/**
	 * End async loading process. Simulates parts of EndLoad(). FinishObjects 
	 * simulates some further parts once we're fully done loading the package.
	 */
	void EndAsyncLoad();
	/**
	 * Create linker async. Linker is not finalized at this point.
	 *
	 * @return true
	 */
	EAsyncPackageState::Type CreateLinker();
	/**
	 * Finalizes linker creation till time limit is exceeded.
	 *
	 * @return true if linker is finished being created, false otherwise
	 */
	EAsyncPackageState::Type FinishLinker();
	/** 
	 * Loads imported packages..
	 *
	 * @return true if we finished loading all imports, false otherwise
	 */
	EAsyncPackageState::Type LoadImports();
	/** 
	 * Create imports till time limit is exceeded.
	 *
	 * @return true if we finished creating all imports, false otherwise
	 */
	EAsyncPackageState::Type CreateImports();
	/**
	 * Checks if all async texture allocations for this package have been completed.
	 *
	 * @return true if all texture allocations have been completed, false otherwise
	 */
	EAsyncPackageState::Type FinishTextureAllocations();
	/**
	 * Create exports till time limit is exceeded.
	 *
	 * @return true if we finished creating and preloading all exports, false otherwise.
	 */
	EAsyncPackageState::Type CreateExports();
	/**
	 * Preloads aka serializes all loaded objects.
	 *
	 * @return true if we finished serializing all loaded objects, false otherwise.
	 */
	EAsyncPackageState::Type PreLoadObjects();
	/**
	 * Route PostLoad to all loaded objects. This might load further objects!
	 *
	 * @return true if we finished calling PostLoad on all loaded objects and no new ones were created, false otherwise
	 */
	EAsyncPackageState::Type PostLoadObjects();
	/**
	 * Finish up objects and state, which means clearing the RF_AsyncLoading flag on newly created ones
	 *
	 * @return true
	 */
	EAsyncPackageState::Type FinishObjects();
	/**
	 * Function called when pending import package has been loaded.
	 */
	void ImportFullyLoadedCallback(const FName& PackageName, UPackage* LoadedPackage);
	/**
	 * Adds dependency tree to the list if packages to wait for until their linkers have been created.
	 *
	 * @param ImportedPackage Package imported either directly or by one of the imported packages
	 */
	void AddDependencyTree(int32 CurrentPackageIndex, FAsyncPackage& ImportedPackage, TSet<FAsyncPackage*>& SearchedPackages);
	/**
	 * Adds a unique package to the list of packages to wait for until their linkers have been created.
	 *
	 * @param PendingImport Package imported either directly or by one of the imported packages
	 */
	bool AddUniqueLinkerDependencyPackage(int32 CurrentPackageIndex, FAsyncPackage& PendingImport);
	/**
	 * Adds a package to the list of pending import packages.
	 *
	 * @param PendingImport Name of the package imported either directly or by one of the imported packages
	 */
	void AddImportDependency(int32 CurrentPackageIndex, const FName& PendingImport);
	/**
	 * Removes references to any imported packages.
	 */
	void FreeReferencedImports();

#if PERF_TRACK_DETAILED_ASYNC_STATS
	/** Add this time taken for object of class Class to have CreateExport called, to the stats we track. */
	void TrackCreateExportTimeForClass(const UClass* Class, double Time);

	/** Add this time taken for object of class Class to have PostLoad called, to the stats we track. */
	void TrackPostLoadTimeForClass(const UClass* Class, double Time);
#endif // PERF_TRACK_DETAILED_ASYNC_STATS
};


