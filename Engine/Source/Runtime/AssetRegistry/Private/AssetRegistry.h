// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

/**
 * The AssetRegistry singleton gathers information about .uasset files in the background so things
 * like the content browser don't have to work with the filesystem
 */
class FAssetRegistry : public IAssetRegistry
{
public:
	FAssetRegistry();
	virtual ~FAssetRegistry();

	// IAssetRegistry implementation
	virtual bool GetAssetsByPackageName(FName PackageName, TArray<FAssetData>& OutAssetData) const override;
	virtual bool GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive = false) const override;
	virtual bool GetAssetsByClass(FName ClassName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses = false) const override;
	virtual bool GetAssetsByTagValues(const TMultiMap<FName, FString>& AssetTagsAndValues, TArray<FAssetData>& OutAssetData) const override;
	virtual bool GetAssets(const FARFilter& Filter, TArray<FAssetData>& OutAssetData) const override;
	virtual FAssetData GetAssetByObjectPath( const FName ObjectPath ) const override;
	virtual bool GetAllAssets(TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false) const override;
	virtual bool GetDependencies(FName PackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType = EAssetRegistryDependencyType::All, bool bResolveIniStringReferences = false) const override;
	virtual bool GetReferencers(FName PackageName, TArray<FName>& OutReferencers, EAssetRegistryDependencyType::Type InReferenceType) const override;
	virtual bool GetAncestorClassNames(FName ClassName, TArray<FName>& OutAncestorClassNames) const override;
	virtual void GetDerivedClassNames(const TArray<FName>& ClassNames, const TSet<FName>& ExcludedClassNames, TSet<FName>& OutDerivedClassNames) const override;
	virtual void GetAllCachedPaths(TArray<FString>& OutPathList) const override;
	virtual void GetSubPaths(const FString& InBasePath, TArray<FString>& OutPathList, bool bInRecurse) const override;
	virtual void RunAssetsThroughFilter (TArray<FAssetData>& AssetDataList, const FARFilter& Filter) const override;
	virtual EAssetAvailability::Type GetAssetAvailability(const FAssetData& AssetData) const override;	
	virtual float GetAssetAvailabilityProgress(const FAssetData& AssetData, EAssetAvailabilityProgressReportingType::Type ReportType) const override;
	virtual bool GetAssetAvailabilityProgressTypeSupported(EAssetAvailabilityProgressReportingType::Type ReportType) const override;
	virtual void PrioritizeAssetInstall(const FAssetData& AssetData) const override;
	virtual bool AddPath(const FString& PathToAdd) override;
	virtual bool RemovePath(const FString& PathToRemove) override;
	virtual void SearchAllAssets(bool bSynchronousSearch) override;
	virtual void ScanPathsSynchronous(const TArray<FString>& InPaths, bool bForceRescan = false) override;
	virtual void ScanFilesSynchronous(const TArray<FString>& InFilePaths, bool bForceRescan = false) override;
	virtual void PrioritizeSearchPath(const FString& PathToPrioritize) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void SaveRegistryData(FArchive& Ar, TMap<FName, FAssetData*>& Data, TArray<FName>* InMaps = nullptr) override;
	virtual void LoadRegistryData(FArchive& Ar, TMap<FName, FAssetData*>& Data) override;
	virtual void LoadPackageRegistryData(FArchive& Ar, TArray<FAssetData*>& Data) const override;
	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FPathAddedEvent, FPathAddedEvent);
	virtual FPathAddedEvent& OnPathAdded() override { return PathAddedEvent; }

	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FPathRemovedEvent, FPathRemovedEvent);
	virtual FPathRemovedEvent& OnPathRemoved() override { return PathRemovedEvent; }

	virtual void AssetCreated(UObject* NewAsset) override;
	virtual void AssetDeleted(UObject* DeletedAsset) override;
	virtual void AssetRenamed(const UObject* RenamedAsset, const FString& OldObjectPath) override;

	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FAssetAddedEvent, FAssetAddedEvent);
	virtual FAssetAddedEvent& OnAssetAdded() override { return AssetAddedEvent; }

	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FAssetRemovedEvent, FAssetRemovedEvent);
	virtual FAssetRemovedEvent& OnAssetRemoved() override { return AssetRemovedEvent; }

	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FAssetRenamedEvent, FAssetRenamedEvent);
	virtual FAssetRenamedEvent& OnAssetRenamed() override { return AssetRenamedEvent; }

	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FInMemoryAssetCreatedEvent, FInMemoryAssetCreatedEvent );
	virtual FInMemoryAssetCreatedEvent& OnInMemoryAssetCreated() override { return InMemoryAssetCreatedEvent; }

	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FInMemoryAssetDeletedEvent, FInMemoryAssetDeletedEvent );
	virtual FInMemoryAssetDeletedEvent& OnInMemoryAssetDeleted() override { return InMemoryAssetDeletedEvent; }

	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FFilesLoadedEvent, FFilesLoadedEvent );
	virtual FFilesLoadedEvent& OnFilesLoaded() override { return FileLoadedEvent; }

	DECLARE_DERIVED_EVENT( FAssetRegistry, IAssetRegistry::FFileLoadProgressUpdatedEvent, FFileLoadProgressUpdatedEvent );
	virtual FFileLoadProgressUpdatedEvent& OnFileLoadProgressUpdated() override { return FileLoadProgressUpdatedEvent; }

	virtual bool IsLoadingAssets() const override;

	virtual void Tick (float DeltaTime) override;

	/** True if world assets are enabled */
	static bool IsUsingWorldAssets();

private:

	/** Internal handler for ScanPathsSynchronous */
	void ScanPathsAndFilesSynchronous(const TArray<FString>& InPaths, const TArray<FString>& InSpecificFiles, bool bForceRescan, EAssetDataCacheMode AssetDataCacheMode);

	/** Called every tick to when data is retrieved by the background asset search. If TickStartTime is < 0, the entire list of gathered assets will be cached. Also used in sychronous searches */
	void AssetSearchDataGathered(const double TickStartTime, TArray<FAssetData*>& AssetResults);

	/** Called every tick when data is retrieved by the background path search. If TickStartTime is < 0, the entire list of gathered assets will be cached. Also used in sychronous searches */
	void PathDataGathered(const double TickStartTime, TArray<FString>& PathResults);

	/** Called every tick when data is retrieved by the background dependency search */
	void DependencyDataGathered(const double TickStartTime, TArray<FPackageDependencyData>& DependsResults);

	/** Called every tick when data is retrieved by the background search for cooked packages that do not contain asset data */
	void CookedPackageNamesWithoutAssetDataGathered(const double TickStartTime, TArray<FString>& CookedPackageNamesWithoutAssetDataResults);

	/** Finds an existing node for the given package and returns it, or returns null if one isn't found */
	FDependsNode* FindDependsNode(FName ObjectName);

	/** Creates a node in the CachedDependsNodes map or finds the existing node and returns it */
	FDependsNode* CreateOrFindDependsNode(FName ObjectName);

	/** Removes the depends node and updates the dependencies to no longer contain it as as a referencer. */
	bool RemoveDependsNode( FName PackageName );

	/** Adds an asset to the empty package list which contains packages that have no assets left in them */
	void AddEmptyPackage(FName PackageName);

	/** Removes an asset from the empty package list because it is no longer empty */
	bool RemoveEmptyPackage(FName PackageName);

	/** Adds a path to the cached paths tree. Returns true if the path was added to the tree, as opposed to already existing in the tree */
	bool AddAssetPath(FName PathToAdd);

	/** Removes a path to the cached paths tree. Returns true if successful. */
	bool RemoveAssetPath(FName PathToRemove, bool bEvenIfAssetsStillExist = false);

	/** Helper function to return the name of an object, given the objects export text path */
	FString ExportTextPathToObjectName(const FString& InExportTextPath) const;

	/** Adds the asset data to the lookup maps */
	void AddAssetData(FAssetData* AssetData);

	/** Updates an existing asset data with the new value and updates lookup maps */
	void UpdateAssetData(FAssetData* AssetData, const FAssetData& NewAssetData);

	/** Removes the asset data from the lookup maps */
	bool RemoveAssetData(FAssetData* AssetData);

	/** Find the first non-redirector dependency node starting from InDependency. */
	FDependsNode* ResolveRedirector(FDependsNode* InDependency, TMap<FName, FAssetData*>& InAllowedAssets, TMap<FDependsNode*, FDependsNode*>& InCache);

	/**
	 * Adds a root path to be discover files in, when asynchronously scanning the disk for asset files
	 *
	 * @param	Path	The path on disk to scan
	 */
	void AddPathToSearch(const FString& Path);

	/** Adds a list of files which will be searched for asset data */
	void AddFilesToSearch (const TArray<FString>& Files);

#if WITH_EDITOR
	/** Called when a file in a content directory changes on disk */
	void OnDirectoryChanged (const TArray<struct FFileChangeData>& Files);
#endif // WITH_EDITOR

	/**
	 * Called by the engine core when a new content path is added dynamically at runtime.  This is wired to 
	 * FPackageName's static delegate.
	 *
	 * @param	AssetPath		The new content root asset path that was added (e.g. "/MyPlugin/")
	 * @param	FileSystemPath	The filesystem path that the AssetPath is mapped to
	 */
	void OnContentPathMounted( const FString& AssetPath, const FString& FileSystemPath );

	/**
	 * Called by the engine core when a content path is removed dynamically at runtime.  This is wired to 
	 * FPackageName's static delegate.
	 *
	 * @param	AssetPath		The new content root asset path that was added (e.g. "/MyPlugin/")
	 * @param	FileSystemPath	The filesystem path that the AssetPath is mapped to
	 */
	void OnContentPathDismounted( const FString& AssetPath, const FString& FileSystemPath );

	/** Checks a filter to make sure there are no illegal entries */
	bool IsFilterValid(const FARFilter& Filter) const;

	/** Returns the names of all subclasses of the class whose name is ClassName */
	void GetSubClasses(const TArray<FName>& InClassNames, const TSet<FName>& ExcludedClassNames, TSet<FName>& SubClassNames) const;
	void GetSubClasses_Recursive(FName InClassName, TSet<FName>& SubClassNames, const TMap<FName, TSet<FName>>& ReverseInheritanceMap, const TSet<FName>& ExcludedClassNames) const;

	/** Registers the configured cooked tags blacklist to prevent blacklisted tags from being added to cooked builds. If configured, we will configure a whitelist instead to prevent non-whitelist tags */
	void SetupCookedFilterlistTags();

	/** Finds all class names of classes capable of generating new UClasses */
	void CollectCodeGeneratorClasses();

private:
	/** The map of ObjectPath names to asset data for assets saved to disk */
	TMap<FName, FAssetData*> CachedAssetsByObjectPath;

	/** The map of package names to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByPackageName;

	/** The map of long package path to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByPath;

	/** The map of class name to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByClass;

	/** The map of asset tag to asset data for assets saved to disk */
	TMap<FName, TArray<FAssetData*> > CachedAssetsByTag;

	/** A map of object names to dependency data */
	TMap<FName, FDependsNode*> CachedDependsNodes;

	/** The set of empty package names (packages which contain no assets but have not yet been saved) */
	TSet<FName> CachedEmptyPackages;

	/** The map of classes to their parents, and a map of parents to their classes */
	TMap<FName, FName> CachedInheritanceMap;

	/** True if CookFilterlistTagsByClass is a whitelist. False if it is a blacklist. */
	bool bFilterlistIsWhitelist;

	/** The map of classname to tag set of tags that are allowed in cooked builds */
	TMap<FName, TSet<FName>> CookFilterlistTagsByClass;

	/** The tree of known cached paths that assets may reside within */
	FPathTree CachedPathTree;

	/** Async task that gathers asset information from disk */
	TSharedPtr< class FAssetDataGatherer > BackgroundAssetSearch;

	/** A list of results that were gathered from the background thread that are waiting to get processed by the main thread */
	TArray<class FAssetData*> BackgroundAssetResults;
	TArray<FString> BackgroundPathResults;
	TArray<class FPackageDependencyData> BackgroundDependencyResults;
	TArray<FString> BackgroundCookedPackageNamesWithoutAssetDataResults;

	/** The max number of results to process per tick */
	float MaxSecondsPerFrame;

	/** The delegate to execute when an asset path is added to the registry */
	FPathAddedEvent PathAddedEvent;

	/** The delegate to execute when an asset path is removed from the registry */
	FPathRemovedEvent PathRemovedEvent;

	/** The delegate to execute when an asset is added to the registry */
	FAssetAddedEvent AssetAddedEvent;

	/** The delegate to execute when an asset is removed from the registry */
	FAssetRemovedEvent AssetRemovedEvent;

	/** The delegate to execute when an asset is renamed in the registry */
	FAssetRenamedEvent AssetRenamedEvent;

	/** The delegate to execute when an in-memory asset was just created */
	FInMemoryAssetCreatedEvent InMemoryAssetCreatedEvent;

	/** The delegate to execute when an in-memory asset was just deleted */
	FInMemoryAssetDeletedEvent InMemoryAssetDeletedEvent;

	/** The delegate to execute when finished loading files */
	FFilesLoadedEvent FileLoadedEvent;

	/** The delegate to execute while loading files to update progress */
	FFileLoadProgressUpdatedEvent FileLoadProgressUpdatedEvent;

	/** Counters for asset/depends data memory allocation to ensure that every FAssetData and FDependsNode created is deleted */
	int32 NumAssets;
	int32 NumDependsNodes;

	/** The start time of the full asset search */
	double FullSearchStartTime;
	double AmortizeStartTime;
	double TotalAmortizeTime;

	/** Flag to enable/disable dependency gathering */
	bool bGatherDependsData;

	/** Flag to indicate if the initial background search has completed */
	bool bInitialSearchCompleted;

	/** When loading a registry from disk, we can allocate all the FAssetData objects in one chunk, to save on 10s of thousands of heap allocations */
	FAssetData* PreallocatedAssetDataBuffer;
	FDependsNode* PreallocatedDependsNodeDataBuffer;

	/** A set used to ignore repeated requests to synchronously scan the same folder or file multiple times */
	TSet<FString> SynchronouslyScannedPathsAndFiles;

	/** List of all class names derived from Blueprint (including Blueprint itself) */
	TSet<FName> ClassGeneratorNames;

	/** Handles to all registered OnDirectoryChanged delegates */
	TMap<FString, FDelegateHandle> OnDirectoryChangedDelegateHandles;

	/** Handle to the registered OnDirectoryChanged delegate for the OnContentPathMounted handler */
	FDelegateHandle OnContentPathMountedOnDirectoryChangedDelegateHandle;
};
