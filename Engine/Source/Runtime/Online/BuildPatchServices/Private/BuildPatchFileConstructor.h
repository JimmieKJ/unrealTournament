// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchFileConstructor.h: Declares the BuildPatchFileConstructor class
	that handles creating files in a manifest from the chunks that make it.
=============================================================================*/

// Forward declarations
class FBuildPatchAppManifest;
struct FChunkPart;

/**
 * FBuildPatchFileConstructor
 * This class controls a thread that constructs files from a file list, given install details, and chunk availability notifications
 */
class FBuildPatchFileConstructor
	: public FRunnable
{
private:
	// Hold a pointer to my thread for easier deleting
	FRunnableThread* Thread;

	// A flag marking that we a running
	bool bIsRunning;

	// A flag marking that we initialized correctly
	bool bIsInited;

	// A flag marking that our init returned a failure (true means failed)
	bool bInitFailed;

	// A flag marking that we told the chunk cache to queue required downloads
	bool bIsDownloadStarted;

	// A critical section to protect the flags and variables
	FCriticalSection ThreadLock;

	// The build manifest for the version already installed
	FBuildPatchAppManifestPtr InstalledManifest;

	// The build manifest for the app we are installing
	FBuildPatchAppManifestRef BuildManifest;

	// Pointer to the build progress tracker
	FBuildPatchProgress* BuildProgress;

	// The directory for the installed build
	FString InstallDirectory;

	// The directory for staging files
	FString StagingDirectory;

	// Total job size for tracking progress
	int64 TotalJobSize;

	// Byte processed so far for tracking progress
	int64 ByteProcessed;

	// A list of filenames for files that need to be constructed in this build
	TArray< FString > FilesToConstruct;

	// A list of filenames for files that have been constructed
	TArray< FString > FilesConstructed;

	// A static map of GUIDs to filenames, listing each piece of file data that has been downloaded
	static TMap< FGuid, FString > FileDataAvailability;

	// A static critical section to protect the file availability map
	static FCriticalSection FileDataAvailabilityLock;

public:

	/**
	 * Constructor
	 * @param InInstalledManifest	The Manifest for the build that is currently install. Invalid for fresh installs.
	 * @param InBuildManifest		The Manifest for the build we are installing
	 * @param InInstallDirectory	The location for the installation.
	 * @param InSaveDirectory		The location where we will store temporary files
	 * @param InConstructList		The list of files to be constructed, filename paths should match those contained in manifest.
	 * @param InBuildProgress		Pointer to the progress tracking class
	 */
	FBuildPatchFileConstructor( FBuildPatchAppManifestPtr InInstalledManifest, FBuildPatchAppManifestRef InBuildManifest, const FString& InInstallDirectory, const FString& InStageDirectory, const TArray< FString >& InConstructList, FBuildPatchProgress* InBuildProgress );

	/**
	 * Default Destructor, will delete the allocated Thread
	 */
	~FBuildPatchFileConstructor();

	// Begin FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	// End FRunnable

	/**
	 * Blocks the calling thread until this one has completed
	 */
	void Wait();

	/**
	 * Get whether the thread has finished working
	 * @return	true if the thread completed
	 */
	bool IsComplete();
	
	/**
	 * Get list of constructed files - only valid when complete
	 * @param		OUT		Populated with the list of files
	 */
	void GetFilesConstructed( TArray< FString >& ConstructedFiles );

	/**
	 * Static function for registering a file download that has been successfully acquired
	 * @param FileGuid		The GUID for the chunk
	 * @param Filename		The filename to the saved file data
	 */
	static void AddFileDataToInventory( const FGuid& FileGuid, const FString& Filename );

	/**
	 * Static function to clear the inventory of available chunks
	 */
	static void PurgeFileDataInventory();

private:

	/**
	 * Sets the bIsRunning flag
	 * @param bRunning	Whether the thread is running
	 */
	void SetRunning( bool bRunning );

	/**
	 * Sets the bIsInited flag
	 * @param bInited	Whether the thread successfully initialized
	 */
	void SetInited( bool bInited );

	/**
	 * Sets the bInitFailed flag
	 * @param bFailed	Whether the thread failed on init
	 */
	void SetInitFailed( bool bFailed );

	/**
	 * Count additional bytes processed, and set new install progress value
	 * @param ByteCount		Number of bytes to increment by
	 */
	void CountBytesProcessed( const int64& ByteCount );

	/**
	 * Function to fetch a chunk from the download list
	 * @param Filename		Receives the filename for the file to construct from the manifest
	 * @return true if there was a chunk guid in the list
	 */
	bool GetFileToConstruct( FString& Filename );

	/**
	 * Get whether a piece of file data is currently available
	 * @param FileGuid		The Guid of the file data in question
	 * @return	true if the file data can be loaded from disk
	 */
	const bool IsFileDataAvailable( const FGuid& FileGuid ) const;

	/**
	 * Gets the full path the the requested file data if available
	 * @param FileGuid		The Guid of the file data in question
	 * @return	the path to the file data, or empty string if unavailable
	 */
	const FString GetFileDataFilename( const FGuid& FileGuid ) const;

	/**
	 * Constructs a particular file referenced by the given BuildManifest. The function takes an interface to a class that can provide availability information of chunks so that this
	 * file construction process can be ran alongside chunk acquisition threads. It will Sleep while waiting for chunks that it needs.
	 * @param Filename			The Filename for the file to construct, that matches an entry in the BuildManifest.
	 * @param bResumeExisting	Whether we should resume from an existing file
	 * @return	true if no file errors occurred
	 */
	bool ConstructFileFromChunks( const FString& Filename, bool bResumeExisting );

	/**
	 * Loads the file data and inserts it into a destination file according to the chunk part info. The chunk part info is mostly the whole file, but is still used for splitting support
	 * in future.
	 * @param ChunkPart			The chunk part details.
	 * @param DestinationFile	The archive for the file being constructed.
	 * @param HashState			An FSHA1 hash state to update with the data going into the destination file.
	 * @return	true if no file errors occurred
	 */
	bool InsertFileData(const FChunkPartData& ChunkPart, FArchive& DestinationFile, FSHA1& HashState);

	/**
	 * Inserts the data data from a chunk into the destination file according to the chunk part info
	 * @param ChunkPart			The chunk part details.
	 * @param DestinationFile	The Filename for the file being constructed.
	 * @param HashState			An FSHA1 hash state to update with the data going into the destination file.
	 * @return true if no errors were detected
	 */
	bool InsertChunkData(const FChunkPartData& ChunkPart, FArchive& DestinationFile, FSHA1& HashState);

	/**
	 * Delete all contents of a directory
	 * @param RootDirectory	 	Directory to make empty
	 */
	void DeleteDirectoryContents(const FString& RootDirectory);
};
