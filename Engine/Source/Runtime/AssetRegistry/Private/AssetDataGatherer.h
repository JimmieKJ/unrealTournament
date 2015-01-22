// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Async task for gathering asset data from from the file list in FAssetRegistry
 */
class FAssetDataGatherer : public FRunnable
{
public:
	/** Constructor */
	FAssetDataGatherer(const TArray<FString>& Paths, bool bInIsSynchronous, bool bInLoadAndSaveCache = false);
	virtual ~FAssetDataGatherer();

	// FRunnable implementation
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	/** Signals to end the thread and waits for it to close before returning */
	void EnsureCompletion();

	/** Gets search results from the data gatherer */
	bool GetAndTrimSearchResults(TArray<FBackgroundAssetData*>& OutAssetResults, TArray<FString>& OutPathResults, TArray<FPackageDependencyData>& OutDependencyResults, TArray<double>& OutSearchTimes, int32& OutNumFilesToSearch, int32& OutNumPathsToSearch);

	/** Adds a root path to the search queue. Only works when searching asynchronously */
	void AddPathToSearch(const FString& Path);

	/** Adds specific files to the search queue. Only works when searching asynchronously */
	void AddFilesToSearch(const TArray<FString>& Files);

	/** If assets are currently being asynchronously scanned in the specified path, this will cause them to be scanned before other assets. */
	void PrioritizeSearchPath(const FString& PathToPrioritize);

private:
	
	/** This function is run on the gathering thread, unless synchronous */
	void DiscoverFilesToSearch();

	/** Returns true if this package file has only valid characters and can exist in a content root */
	bool IsValidPackageFileToRead(const FString& Filename) const;

	/**
	 * Reads FAssetData information out of a file
	 *
	 * @param AssetFilename the name of the file to read
	 * @param AssetDataList the FBackgroundAssetData for every asset found in the file
	 * @param DependencyData the FPackageDependencyData for every asset found in the file
	 *
	 * @return true if the file was successfully read
	 */
	bool ReadAssetFile(const FString& AssetFilename, TArray<FBackgroundAssetData*>& AssetDataList, FPackageDependencyData& DependencyData) const;

	/** Serializes the timestamped cache of discovered assets. Used for quick loading of data for assets that have not changed on disk */
	void SerializeCache(FArchive& Ar);

private:
	/** A critical section to protect data transfer to the main thread */
	FCriticalSection WorkerThreadCriticalSection;

	/** List of files that need to be processed by the search. It is not threadsafe to directly access this array */
	TArray<FString> FilesToSearch;

	/** > 0 if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter StopTaskCounter;

	/** True if this gather request is synchronous (i.e, IsRunningCommandlet()) */
	bool bIsSynchronous;

	/** True if in the process of discovering files in PathsToSearch */
	bool bIsDiscoveringFiles;

	/** The current search start time */
	double SearchStartTime;

	/** The input base paths in which to discover assets and paths */
	// IMPORTANT: This variable may be modified by from a different thread via a call to AddPathToSearch(), so access 
	//            to this array should be handled very carefully.
	TArray<FString> PathsToSearch;

	/** The asset data gathered from the searched files */
	TArray<FBackgroundAssetData*> AssetResults;

	/** Dependency data for scanned packages */
	TArray<FPackageDependencyData> DependencyResults;

	/** All the search times since the last main thread tick */
	TArray<double> SearchTimes;

	/** The paths found during the search */
	TArray<FString> DiscoveredPaths;

	/** True if dependency data should be gathered */
	bool bGatherDependsData;

	/** Set of characters that are invalid in packages, thus files containing them can not be loaded. */
	TSet<TCHAR> InvalidAssetFileCharacters;

	///////////////////////////////////////////////////////////////
	// Asset discovery caching
	///////////////////////////////////////////////////////////////

	/** True if this gather request should both load and save the asset cache. Only one gatherer should do this at a time! */
	bool bLoadAndSaveCache;

	/** True if we have finished discovering our first wave of files. Used to preemptively save the discovered assets cache once a full scan has completed */
	bool bSavedCacheAfterInitialDiscovery;

	/** The name of the file that contains the timestamped cache of discovered assets */
	FString CacheFilename;

	/** When loading a registry from disk, we can allocate all the FAssetData objects in one chunk, to save on 10s of thousands of heap allocations */
	FDiskCachedAssetData* DiskCachedAssetDataBuffer;

	/** An array of all cached data that was newly discovered this run. This array is just used to make sure they are all deleted at shutdown */
	TArray<FDiskCachedAssetData*> NewCachedAssetData;

	/** Map of PackageName to cached discovered assets that were loaded from disk */
	TMap<FName, FDiskCachedAssetData*> DiskCachedAssetDataMap;

	/** Map of PackageName to cached discovered assets that will be written to disk at shutdown */
	TMap<FName, FDiskCachedAssetData*> NewCachedAssetDataMap;

public:
	/** Thread to run the cleanup FRunnable on */
	FRunnableThread* Thread;
};