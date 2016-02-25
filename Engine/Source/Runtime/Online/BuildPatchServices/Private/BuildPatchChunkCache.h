// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchChunkCache.h: Declares chunk cache which controls the flow of
	chunk data in and out of RAM, hopefully minimizing HDD usage
=============================================================================*/

#pragma once

// Forward decelerations
class FBuildPatchAppManifest;
class FBuildPatchInstallationInfo;

/**
 * Declares a static access singleton class that controls the flow of chunk 
 * acquisition and usage. It handles holding chunks in memory from downloads,
 * and also reading chunks from an existing installation where necessary.
 * There is a limited number of chunks that are allowed to be stored in RAM.
 *
 * This class is strictly for chunk data builds only, and not packaged file data.
 */
class FBuildPatchChunkCache
{
private:
	// Enum describing the origin for a chunk
	struct EChunkOrigin
	{
		enum Type
		{
			// The chunk will need downloading
			Download,
			// The chunk is in an existing installation
			Recycle,
			// The chunk has been saved to disk (usually forced out of a full cache)
			Harddisk
		};
	};

	// Class for storing chunk data in memory
	// Protects access to the lookup only, chunk data is not protected internally and has it's own locking system
	class FThreadSafeChunkCache
	{
	private:
		// Critical Section for lookup access
		FCriticalSection ThreadLock;
		// The chunk file lookup
		TMap< FGuid, FChunkFile* > ChunkStore;
		// A list of chunks that we are reserving memory for
		TArray< FGuid > ReservedChunks;
	public:
		/**
		 * Find out if a chunk is currently loaded into the cache
		 * @param	ChunkGuid			The chunk GUID
		 * @return	true if the chunk is in the cache
		 */
		bool Contains( const FGuid& ChunkGuid );

		/**
		 * Get the number of chunks slots used
		 * @return	How many chunk slots have been used up (or reserved)
		 */
		int32 Num();

		/**
		 * Add a new chunk to the cache
		 * @param	ChunkGuid	The chunk GUID
		 * @param	ChunkFile	The allocated and fully setup chunk file data structure
		 */
		void Add( const FGuid& ChunkGuid, FChunkFile* ChunkFile );

		/**
		 * Get chunk file from the cache
		 * @param	ChunkGuid			The chunk GUID
		 * @return		pointer to the chunk file
		 */
		FChunkFile* Get( const FGuid& ChunkGuid );

		/**
		 * Remove a chunk from the cache
		 * @param	ChunkGuid			The chunk GUID
		 */
		void Remove( const FGuid& ChunkGuid );

		/**
		 * Completely empty the cache
		 */
		void Empty();

		/**
		 * Remove all chunks that have a zero ref count
		 */
		void PurgeUnreferenced();

		/**
		 * Attempt to reserve space for a chunk
		 * @param	ChunkGuid			The chunk GUID
		 * @return		true if space was reserved, false if we cannot take this chunk.
		 */
		bool TryAddReservation( const FGuid& ChunkGuid );

		/**
		 * Remove the reservation for a chunk
		 * @param	ChunkGuid			The chunk GUID
		 */
		void RemoveReservation( const FGuid& ChunkGuid );

		/**
		 * See if a chunk has reserved space
		 * @param	ChunkGuid			The chunk GUID
		 * @return		true if the chunk has been reserved
		 */
		bool HasReservation( const FGuid& ChunkGuid );
	};

private:

	// Chunk disk cache for full cache resort
	FString ChunkCacheStage;

	// Directory of current installation
	FString CurrentInstallDir;

	// The manifest for the build being installed
	FBuildPatchAppManifestRef InstallManifet;

	// The manifest for the currently installed build, if applicable
	FBuildPatchAppManifestPtr CurrentManifest;

	// Pointer to the progress tracking class which can be used to get pause state
	FBuildPatchProgress* BuildProgress;

	// Whether we are recycling chunks from current build
	bool bEnableChunkRecycle;

	// Whether downloads have started
	FThreadSafeCounter bDownloadsStarted;

	// The chunk cache storage class
	FThreadSafeChunkCache ChunkCache;

	// A critical section to protect the chunk origin lookup
	FCriticalSection ChunkOriginLock;

	// Lookup for where a chunk will be acquired from
	TMap< FGuid, EChunkOrigin::Type > ChunkOrigins;

	// A critical section to protect the chunk information lists
	FCriticalSection ChunkInfoLock;

	// A list of chunk downloads that will be queued
	TArray< FGuid > ChunksToDownload;

	// The full list of chunk references
	TArray< FGuid > FullChunkRefList;

	// A stack of chunks to store their usage order (Last is needed sooner)
	TArray< FGuid > ChunkUseOrderStack;

	// Build Stat: The number of files being constructed
	uint32 NumFilesToConstruct;

	// Build Stat: The number of chunks that need downloading
	uint32 NumChunksToDownload;

	// Build Stat: The number of chunks that need recycling
	uint32 NumChunksToRecycle;

	// Build Stat: The total number of chunks required to produce the build
	uint32 NumRequiredChunks;

	// Build Stat: The initial download size for chunks we need to download
	int64 TotalChunkDownloadSize;

	// Build Stat: The skipped download size from resuming
	int64 SkippedChunkDownloadSize;

	// Build Counter: The number of chunks that were successfully recycled
	FThreadSafeCounter NumChunksRecycled;

	// Build Counter: The number of chunks that had to be booted from cache to disk
	FThreadSafeCounter NumChunksCacheBooted;

	// Build Counter: The number of chunks that were successfully loaded from disk cache
	FThreadSafeCounter NumDriveCacheChunkLoads;

	// Build Counter: The number of chunks that failed to recycle
	FThreadSafeCounter NumRecycleFailures;

	// Build Counter: The number of chunks that failed to load from disk cache
	FThreadSafeCounter NumDriveCacheLoadFailures;

	// Reference to the Module's installation info which has the information about installed builds that can pull chunks from
	FBuildPatchInstallationInfo& InstallationInfo;

public:

	/**
	 * Constructor takes required arguments
	 * @param	InInstallManifet		The manifest for build being installed
	 * @param	InCurrentManifest		The manifest for build currently installed
	 * @param	InChunkCacheStage		A stage where we can save chunks to if necessary
	 * @param	InCurrentInstallDir		The directory of the current install
	 * @param	InBuildProgress			Pointer to the progress tracking class, also used for pause.
	 * @param	FilesToConstruct		List of files that are going to be constructed
	 * @param	InstallationInfoRef		Reference to the module's installation info that keeps record of locally installed apps for use as chunk sources
	 */
	FBuildPatchChunkCache(const FBuildPatchAppManifestRef& InInstallManifet, const FBuildPatchAppManifestPtr& InCurrentManifest, const FString& InChunkCacheStage, const FString& InCurrentInstallDir, FBuildPatchProgress* InBuildProgress, TArray< FString >& FilesToConstruct, FBuildPatchInstallationInfo& InstallationInfoRef);

	/**
	 * Gets hold of a chunk file class to provide access to chunk data. It will be blocking until the chunk is available in memory
	 * via whatever origin the chunk comes from.
	 *
	 * NOTE: The caller must dereference this chunk when finished with, inside the data lock
	 *
	 * NOTE: It's important to only call this function from the single thread that is writing install data.
	 *		 Calling from other places and/or threads will have large impact on drive access performance as this function
	 *		 also internally handles shuttling of chunk files to and from disk when necessary and this needs to take turns
	 *		 with writing installation data.
	 *
	 * @param ChunkGuid		The chunk guid
	 * @return	the chunk file data with header
	 */
	FChunkFile* GetChunkFile( const FGuid& ChunkGuid );

	/**
	 * Add downloaded data to the cache. The data will be copied.
	 * @param ChunkGuid			The chunk guid for this data
	 * @param ChunkDataFile		The file data
	 */
	void AddDataToCache( const FGuid& ChunkGuid, const TArray< uint8 >& ChunkDataFile );

	/**
	 * Reserve space for a chunk. Must be called before attempting download of a chunk
	 * @param ChunkGuid			The chunk guid for this data
	 * @return true if there was space available and this chunk is allowed into the cache
	 */
	bool ReserveChunkInventorySlot( const FGuid& ChunkGuid );

	/**
	 * Notify the cache that a file is already fully constructed and so we can skip over chunks for it
	 * @param Filename		The Filename being skipped
	 */
	void SkipFile( const FString& Filename );

	/**
	 * Notify the cache that a chunk part for a partially complete file is to be skipped
	 * @param ChunkPart		The chunk part
	 */
	void SkipChunkPart(const FChunkPartData& ChunkPart);

	/**
	 * Should be after initialization and after all skippable parts have been registered to kick of the chunk downloader
	 * with the remaining list of downloadable chunks
	 */
	void BeginDownloads();

	/**
	 * Get whether we have been told to queue downloads yet
	 * @return True if downloads are queued
	 */
	const bool HaveDownloadsStarted() const;

	/**
	 * Get the statistic for the number of files needing construction
	 * @return Number of files to be constructed
	 */
	const uint32& GetStatNumFilesToConstruct() const;

	/**
	 * Get the statistic for the number of chunks that need downloading
	 * @return Number of chunks that need downloading
	 */
	const uint32& GetStatNumChunksToDownload() const;

	/**
	 * Get the statistic for the number of chunks that need recycling
	 * @return Number of chunks that need recycling
	 */
	const uint32& GetStatNumChunksToRecycle() const;

	/**
	 * Get the statistic for the number of chunks required for the build
	 * @return Number of chunks required for build
	 */
	const uint32& GetStatNumRequiredChunks() const;

	/**
	 * Get the total number of bytes for the chunks needing download
	 * @return Total bytes of initial download list
	 */
	const int64& GetStatTotalChunkDownloadSize() const;

	/**
	 * Get the counter for the number of chunks that were recycled
	 * @return Number of chunks that were recycled
	 */
	const int32 GetCounterChunksRecycled() const;

	/**
	 * Get the counter for the number of chunks that were booted from cache to disk
	 * @return Number of chunks that were booted from the cache
	 */
	const int32 GetCounterChunksCacheBooted() const;

	/**
	 * Get the counter for the number of chunks that were loaded from disk cache
	 * @return Number of chunks loaded from disk cache
	 */
	const int32 GetCounterDriveCacheChunkLoads() const;

	/**
	 * Get the counter for the number of chunks that failed to recycle
	 * @return Number of chunks that failed to recycle
	 */
	const int32 GetCounterRecycleFailures() const;

	/**
	 * Get the counter for the number of chunks that failed to load from disk cache
	 * @return Number of chunks that failed to load from disk cache
	 */
	const int32 GetCounterDriveCacheLoadFailures() const;

private:

	/**
	 * Get the index for this chunk in the order of requirement time
	 * @param	ChunkGuid	The chunk GUID
	 * @return Chunk order index
	 */
	int32 GetChunkOrderIndex( const FGuid& ChunkGuid );

	/**
	 * Calculate how many more times a chunk is required
	 * @param	ChunkGuid	The chunk GUID
	 * @return Number of remaining references to the chunk
	 */
	int32 GetRemainingReferenceCount( const FGuid& ChunkGuid );

	/**
	 * Block the calling thread until the given chunk is available in the cache
	 * @param	ChunkGuid	The chunk GUID
	 */
	void WaitForChunk( const FGuid& ChunkGuid );

	/**
	 * Loads a chunk from the drive cache
	 * @param	ChunkGuid	The chunk GUID
	 * @return true if there were no errors
	 */
	bool ReadChunkFromDriveCache( const FGuid& ChunkGuid );

	/**
	 * Recycle the chunk from the currently installed build
	 * @param	ChunkGuid	The chunk GUID
	 * @return true if there were no errors
	 */
	bool RecycleChunkFromBuild( const FGuid& ChunkGuid );

	/**
	 * Force there to be space in the cache by booting out the one that is not needed for the longest time
	 * @param	ChunkGuid	The chunk GUID
	 */
	void ReserveChunkInventorySlotForce( const FGuid& ChunkGuid );

/* Here we have static access for the singleton
*****************************************************************************/
public:
	static void Init(const FBuildPatchAppManifestRef& InInstallManifet, const FBuildPatchAppManifestPtr& InCurrentManifest, const FString& InChunkCacheStage, const FString& InCurrentInstallDir, FBuildPatchProgress* InBuildProgress, TArray<FString>& FilesToConstruct, FBuildPatchInstallationInfo& ChunkRecyclerRef);
	static FBuildPatchChunkCache& Get();
	static void Shutdown();
	static bool IsAvailable();
private:
	static TSharedPtr< FBuildPatchChunkCache, ESPMode::ThreadSafe > SingletonInstance;
};
