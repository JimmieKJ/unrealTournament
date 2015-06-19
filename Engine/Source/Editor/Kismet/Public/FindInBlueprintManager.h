// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 *Const values for Find-in-Blueprints to tag searchable data
 */
struct KISMET_API FFindInBlueprintSearchTags
{
	/** Properties tag, for Blueprint variables */
	static const FText FiB_Properties;

	/** Components tags */
	static const FText FiB_Components;
	static const FText FiB_IsSCSComponent;
	/** End Components tags */

	/** Nodes tag */
	static const FText FiB_Nodes;

	/** Schema Name tag, to identify the schema that a graph uses */
	static const FText FiB_SchemaName;
	/** Uber graphs tag */
	static const FText FiB_UberGraphs;
	/** Function graph tag */
	static const FText FiB_Functions;
	/** Macro graph tag */
	static const FText FiB_Macros;
	/** Sub graph tag, for any sub-graphs in a Blueprint */
	static const FText FiB_SubGraphs;

	/** Name tag */
	static const FText FiB_Name;
	/** Class Name tag */
	static const FText FiB_ClassName;
	/** NodeGuid tag */
	static const FText FiB_NodeGuid;
	/** Default value */
	static const FText FiB_DefaultValue;
	/** Tooltip tag */
	static const FText FiB_Tooltip;
	/** Description tag */
	static const FText FiB_Description;
	/** Comment tag */
	static const FText FiB_Comment;

	/** Pin type tags */

	/** Pins tag */
	static const FText FiB_Pins;

	/** Pin Category tag */
	static const FText FiB_PinCategory;
	/** Pin Sub-Category tag */
	static const FText FiB_PinSubCategory;
	/** Pin object class tag */
	static const FText FiB_ObjectClass;
	/** Pin IsArray tag */
	static const FText FiB_IsArray;
	/** Pin IsReference tag */
	static const FText FiB_IsReference;
	/** Glyph icon tag */
	static const FText FiB_Glyph;
	/** Glyph icon color tag */
	static const FText FiB_GlyphColor;
	/** End const values for Find-in-Blueprint */
};

/** Tracks data relevant to a Blueprint for searches */
struct FSearchData
{
	/** The Blueprint this search data points to, if available */
	TWeakObjectPtr<UBlueprint> Blueprint;

	/** The full Blueprint path this search data is associated with */
	FString BlueprintPath;

	/** Search data block for the Blueprint */
	FString Value;

	/** Cached to determine if the Blueprint is seen as no longer valid, allows it to be cleared out next save to disk */
	bool bMarkedForDeletion;

	FSearchData()
		: Blueprint(nullptr)
		, bMarkedForDeletion(false)
	{

	}
};

/** Used for external gather functions to add Key/Value pairs to be placed into Json */
struct FSearchTagDataPair
{
	FSearchTagDataPair(FText InKey, FText InValue)
		: Key(InKey)
		, Value(InValue)
	{}

	FText Key;
	FText Value;
};

/** Singleton manager for handling all Blueprint searches, helps to manage the going progress of Blueprints, and is thread-safe. */
class KISMET_API FFindInBlueprintSearchManager
{
public:
	static FFindInBlueprintSearchManager* Instance;
	static FFindInBlueprintSearchManager& Get();

	FFindInBlueprintSearchManager();
	~FFindInBlueprintSearchManager();

	/**
	 * Gathers the Blueprint's search metadata and adds or updates it in the cache
	 *
	 * @param InBlueprint		Blueprint to cache the searchable data for
	 * @param bInForceReCache	Forces the Blueprint to be recache'd, regardless of what data it believes exists
	 */
	void AddOrUpdateBlueprintSearchMetadata(UBlueprint* InBlueprint, bool bInForceReCache = false);

	/**
	 * Starts a search query, the FiB manager handles where the thread is at in the search query at all times so that post-save of the cache to disk it can correct the index
	 *
	 * @param InSearchOriginator		Pointer to the thread object that the query originates from
	 */
	void BeginSearchQuery(const class FStreamSearch* InSearchOriginator);

	/**
	 * Continues a search query, returning a single piece of search data
	 *
	 * @param InSearchOriginator		Pointer to the thread object that the query originates from
	 * @param OutSearchData				Result of the search, if there is any Blueprint search data still available to query
	 * @return							TRUE if the search was successful, FALSE if the search is complete
	 */
	bool ContinueSearchQuery(const class FStreamSearch* InSearchOriginator, FSearchData& OutSearchData);

	/**
	 * This function ensures that the passed in search query ends in a safe manner. The search will no longer be valid to this manager, though it does not destroy any objects.
	 * Use this whenever the search is finished or canceled.
	 *
	 * @param InSearchOriginator	Identifying search stream to be stopped
	 */
	void EnsureSearchQueryEnds(const class FStreamSearch* InSearchOriginator);

	/**
	 * Query how far along a search thread is
	 *
	 * @param OutSearchData				Result of the search, if there is any Blueprint search data still available to query
	 * @return							Percent along the search thread is
	 */
	float GetPercentComplete(const class FStreamSearch* InSearchOriginator) const;

	/**
	 * Query for a single, specific Blueprint's search block
	 *
	 * @param InBlueprint				The Blueprint to search for
	 * @param bInRebuildSearchData		When TRUE the search data will be freshly collected
	 * @return							The search block
	 */
	FString QuerySingleBlueprint(UBlueprint* InBlueprint, bool bInRebuildSearchData = true);

	/** Converts a string of hex characters, previously converted by ConvertFTextToHexString, to an FText. */
	static FText ConvertHexStringToFText(FString InHexString);

	/** Serializes an FText to memory and converts the memory into a string of hex characters */
	static FString ConvertFTextToHexString(FText InValue);

	/** Returns the number of uncached Blueprints */
	int32 GetNumberUncachedBlueprints() const { return UncachedBlueprints.Num(); };

	/**
	 * Starts caching all uncached Blueprints at a rate of 1 per tick
	 *
	 * @param InSourceWidget		The source FindInBlueprints widget, this widget will be informed when caching is complete
	 */
	void CacheAllUncachedBlueprints(TWeakPtr< class SFindInBlueprints > InSourceWidgetm, FWidgetActiveTimerDelegate& OutActiveTimerDelegate);
	void OnCacheAllUncachedBlueprints(bool bInSourceControlActive);

	/** Stops the caching process where it currently is at, the rest can be continued later */
	void CancelCacheAll(SFindInBlueprints* InFindInBlueprintWidget);

	/** Returns the current index in the caching */
	int32 GetCurrentCacheIndex() const;

	/** Returns the name of the current Blueprint being cached */
	FString GetCurrentCacheBlueprintName() const;

	/** Returns the progress complete on the caching */
	float GetCacheProgress() const;

	/** Returns the list of Blueprint paths that failed to cache */
	TArray<FString> GetFailedToCachePathList() const { return FailedToCachePaths; }

	/** Returns the number of Blueprints that failed to cache */
	int32 GetFailedToCacheCount() const { return FailedToCachePaths.Num(); }

	/** Returns TRUE if caching failed */
	bool HasCachingFailed() const { return FailedToCachePaths.Num() > 0; };
	/**
	 * Callback to note that Blueprint caching is complete
	 *
	 * @param InNumberCached		The number of Blueprints cached, to be chopped off the existing array so the rest (if any) can be finished later
	 */
	void FinishedCachingBlueprints(int32 InNumberCached, TArray<FString>& InFailedToCacheList);

	/** Returns TRUE if Blueprints are being cached. */
	bool IsCacheInProgress() const;

	/** Serializes an FString to memory and converts the memory into a string of hex characters */
	static FString ConvertFStringToHexString(FString InValue);

	/** Given a fully constructed Find-in-Blueprint FString of searchable data, will parse and construct a JsonObject */
	static TSharedPtr< class FJsonObject > ConvertJsonStringToObject(FString InJsonString);

private:
	/** Initializes the FiB manager */
	void Initialize();

	/** Callback hook during pre-garbage collection, pauses all processing searches so that the GC can do it's job */ 
	void PauseFindInBlueprintSearch();

	/** Callback hook during post-garbage collection, saves the cache to disk and unpauses all processing searches */
	void UnpauseFindInBlueprintSearch();

	/** Callback hook from the Asset Registry when an asset is added */
	void OnAssetAdded(const class FAssetData& InAssetData);

	/** Callback hook from the Asset Registry, marks the asset for deletion from the cache */
	void OnAssetRemoved(const class FAssetData& InAssetData);

	/** Callback hook from the Asset Registry, marks the asset for deletion from the cache */
	void OnAssetRenamed(const class FAssetData& InAssetData, const FString& InOldName);

	/** Callback hook from the Asset Registry when an asset is loaded */
	void OnAssetLoaded(class UObject* InAsset);

	/** Helper to gathers the Blueprint's search metadata */
	FString GatherBlueprintSearchMetadata(const UBlueprint* Blueprint);

	/** Cleans the cache of any excess data from Blueprints that have been moved, renamed, or deleted. Occurs during post-garbage collection */
	void CleanCache();

	/** Builds the cache from all available Blueprint assets that the asset registry has discovered at the time of this function. Occurs on startup */
	void BuildCache();

	/**
	 * Helper to properly add a Blueprint's SearchData to the database
	 *
	 * @param InSearchData		Data to add to the database
	 * @return					Index into the SearchArray for looking up the added item
	 */
	int32 AddSearchDataToDatabase(FSearchData& InSearchData);

	/** Removes a Blueprint from being managed by the FiB system by passing in the UBlueprint's path */
	void RemoveBlueprintByPath(FString InPath);

	/** Removes a World Blueprint with compound FiB searchable data from being managed by the FiB system by passing in the World's path */
	void RemoveWorldByPath(FString InPath);
protected:
	/** Maps the Blueprint paths to their index in the SearchArray */
	TMap<FString, int> SearchMap;

	/** Stores the Blueprint search data and is used to iterate over in small chunks */
	TArray<FSearchData> SearchArray;

	/** Counter of active searches */
	FThreadSafeCounter ActiveSearchCounter;

	/** A mapping of active search queries and where they are currently at in the search data */
	TMap< const class FStreamSearch*, int32 > ActiveSearchQueries;

	/** Critical section to safely add, remove, and find data in ActiveSearchQueries */
	mutable FCriticalSection SafeQueryModifyCriticalSection;

	/** Critical section to lock threads during the pausing procedure */
	FCriticalSection PauseThreadsCriticalSection;

	/** Critical section to safely modify cached data */
	FCriticalSection SafeModifyCacheCriticalSection;

	/** TRUE when the the FiB manager wants to pause all searches, helps manage the pausing procedure */
	volatile bool bIsPausing;

	/** Because we are unable to query for the module on another thread, cache it for use later */
	class FAssetRegistryModule* AssetRegistryModule;

	/** FindInBlueprints widget that started the cache process */
	TWeakPtr<SFindInBlueprints> SourceCachingWidget;

	/** Blueprint paths that have not been cached for searching due to lack of data, this means that they are either older Blueprints, or the DDC cannot find the data */
	TArray<FString> UncachedBlueprints;

	/** List of paths for Blueprints that failed to cache */
	TArray<FString> FailedToCachePaths;

	/** Tickable object that does the caching of uncached Blueprints at a rate of once per tick */
	class FCacheAllBlueprintsTickableObject* CachingObject;
};
