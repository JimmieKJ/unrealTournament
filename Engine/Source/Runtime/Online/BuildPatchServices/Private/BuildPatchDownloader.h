// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FBuildPatchDownloader.h: Declares the BuildPatchChunkDownloader
	that runs a thread to download chunks in it's queue.
=============================================================================*/

/**
 * FChunkDownloadRecord
 * This is a simple struct to record downloads
 */
struct FBuildPatchDownloadRecord
{
	// The start time of the download
	double StartTime;
	// The end time of the download
	double EndTime;
	// The size of the download
	int64 DownloadSize;
	/**
	 * Default constructor
	 */
	FBuildPatchDownloadRecord()
		: StartTime( 0 )
		, EndTime( 0 )
		, DownloadSize( 0 ) {}
	/**
	 * Less-than operator to allow sorting by start time.
	 * @param Other	 The record to compare against.
	 * @return true if this download started first, otherwise false.
	 */
	FORCEINLINE bool operator<( const FBuildPatchDownloadRecord& Other ) const
	{
		return StartTime < Other.StartTime;
	}
};

/**
 * FBuildPatchDownloader
 * This class controls a thread that downloads chunks from a static chunk list
 */
class FBuildPatchDownloader
	: public FRunnable
	, public TSharedFromThis< FBuildPatchDownloader, ESPMode::ThreadSafe >
{
private:
	// Enum to list download status flags
	struct EDownloadState
	{
		enum Type
		{
			// The download is initialising
			DownloadInit = 0,
			// The download is running
			DownloadRunning,
			// The download failed
			DownloadFail,
			// The download completed successfully
			DownloadSuccess
		};
	};

	// A struct to store a guid with it's data type
	struct FDownloadJob
	{
		// The data guid
		FGuid Guid;
		// The download url for this job
		FString DownloadUrl;
		// A thread safe counter that will count retries.
		FThreadSafeCounter RetryCount;
		// A thread safe counter that we will use a simple check for download completion
		FThreadSafeCounter StateFlag;
		// The downloaded data
		TArray< uint8 > DataArray;
		// Track download time
		FBuildPatchDownloadRecord DownloadRecord;
		// The response code, for HTTP downloads
		int32 ResponseCode;
		// The request ID, for HTTP downloads
		int32 HttpRequestId;

		/**
		 * Default constructor
		 */
		FDownloadJob()
			: DownloadUrl( TEXT( "" ) )
			, RetryCount( 0 )
			, StateFlag( EDownloadState::DownloadInit )
			, ResponseCode( 0 )
			, HttpRequestId( INDEX_NONE )
		{}
	};

private:

	// The directory to save data to
	const FString SaveDirectory;

	// The Manifest that is being installed
	const FBuildPatchAppManifestRef InstallManifest;

	// Hold a pointer to my thread for deleting
	FRunnableThread* Thread;

	// A flag marking that we a running
	bool bIsRunning;

	// A flag marking that we initialized correctly
	bool bIsInited;

	// A flag marking whether the thread is idle waiting for jobs
	bool bIsIdle;

	// A flag that says whether more chunks could still be queued
	bool bWaitingForJobs;

	// A critical section to protect the flags
	FCriticalSection FlagsLock;

	// Store a record of each download we made for info
	TArray< FBuildPatchDownloadRecord > DownloadRecords;

	// Critical section to protect the download list
	FCriticalSection DownloadRecordsLock;

	// List of download jobs to be processed
	TArray< FGuid > DataToDownload;

	// List of download jobs to be retried
	TArray< FGuid > DataToRetry;

	// Count of number of bytes total added to download queue
	int64 DataToDownloadTotalBytes;

	// Critical section to protect the download list
	FCriticalSection DataToDownloadLock;

	// List of download jobs retry counts
	TMap< FGuid, FDownloadJob > InFlightDownloads;

	// Critical section to protect the retry count list
	FCriticalSection InFlightDownloadsLock;

	// Counter for tracking number of bytes downloaded per delta
	FThreadSafeCounter ByteDownloadCount;

	// Pointer to the build progress tracker
	FBuildPatchProgress* BuildProgress;

public:

	/**
	 * Constructor
	 * @param	InSaveDirectory		The chunk save to directory
	 * @param	InInstallManifest	The manifest being installed so we can pull info from it
	 * @param	InBuildProgress		Pointer to the progress tracker, for setting progress and getting pause state
	 */
	FBuildPatchDownloader( const FString& InSaveDirectory, const FBuildPatchAppManifestRef& InInstallManifest, FBuildPatchProgress* InBuildProgress );

	/**
	 * Default Destructor, deletes allocated Thread
	 */
	~FBuildPatchDownloader();

	// Begin FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	// End FRunnable

	/**
	 * Get whether the thread has finished working
	 * @return	true if the thread completed
	 */
	bool IsComplete();

	/**
	 * Get whether the thread is idle waiting for chunks to download
	 * @return	true if the thread completed
	 */
	bool IsIdle();

	/**
	 * Gets the array of download recordings. Should not be polled, only call when the thread has finished to gather data
	 * @return	An array of download records
	 */
	TArray< FBuildPatchDownloadRecord > GetDownloadRecordings();

	/**
	 * Adds a chunk GUID to the download list
	 * @param Guid				The GUID for the data
	 * @param bInsertAtFront	If this should be queued at the front
	 */
	void AddChunkToDownload( const FGuid& Guid, const bool bInsertAtFront = false );

	/**
	 * Adds an array of chunk GUIDs to the download list
	 * @param Guid				The GUID array for the data
	 * @param bInsertAtFront	If this should be queued at the front
	 */
	void AddChunksToDownload( const TArray< FGuid >& Guids, const bool bInsertAtFront = false );

	/**
	 * Clear out the download list
	 */
	void ClearDownloadJobs();

	/**
	 * A notification for when there is no possibility of more jobs to be queued (so the thread can end)
	 */
	void NotifyNoMoreChunksToAdd();

	/**
	 * Get how many chunks are in the list
	 * @return The number of chunks in the list
	 */
	int32 GetNumChunksLeft();

	/**
	 * Get the number of bytes left to download.
	 * @return The number of byte left to download
	 */
	int64 GetNumBytesLeft();

	/**
	 * Get the number of bytes that have been queued
	 * @return The total number of bytes queued
	 */
	int64 GetNumBytesQueued();

	/**
	 * Get how many bytes have been downloaded, resetting the counter
	 * @return The number of bytes downloaded since the last call
	 */
	int32 GetByteDownloadCountReset();

	/**
	 * Add some bytes to the download counter
	 * @param	NumBytes The number of bytes to add
	 */
	void IncrementByteDownloadCount( const int32& NumBytes );

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
	 * Sets the bIsIdle flag
	 * @param bIdle	Whether the thread is waiting for jobs
	 */
	void SetIdle( bool bIdle );

	/**
	 * Gets whether the thread still has work to do
	 * @return	false if the thread can quit
	 */
	bool ShouldBeRunning();

	/**
	 * Receives the HTTP request bytes downloaded when downloading a chunk.
	 * @param Request               The request that was made
	 * @param BytesSentSoFar        The total number of bytes sent so far
	 * @param BytesReceivedSoFar    The total number of bytes downloaded so far
	 */
	void HttpRequestProgress( FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived );

	/**
	 * Receives the HTTP response when downloading a chunk.
	 * @param Request		The request that was made
	 * @param Response		The request's response
	 * @param bSucceeded	Whether the request successfully completed
	 */
	void HttpRequestComplete( FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded );

	/**
	 * Progress update for download of a chunk, from HTTP or Network/HDD.
	 * @param Guid			The guid for the data
	 * @param BytesSoFar	The number of bytes downloaded so far
	 */
	void OnDownloadProgress( const FGuid& Guid, const int32& BytesSoFar );

	/**
	 * Completes a download of a chunk, from HTTP or Network/HDD.
	 * @param Guid			The guid for the data
	 * @param DownloadUrl	The url used to download the data
	 * @param DataArray		The array of bytes for the data
	 * @param bSucceeded	Whether the download was successful
	 * @param ResponseCode	The HTTP response code
	 */
	void OnDownloadComplete( const FGuid& Guid, const FString& DownloadUrl, const TArray< uint8 >& DataArray, bool bSucceeded, int32 ResponseCode );

	/**
	 * Get the next GUID to download data with
	 * @param Guid		Receives the download data GUID
	 * @return true if there was a job in the list
	 */
	bool GetNextDownload( FGuid& Guid );

	/* Here we have static access for the singleton
	*****************************************************************************/
public:

	/**
	 * Creates the singleton downloader system. Takes the arguments required for construction.
	 * @param	InSaveDirectory		The chunk save to directory
	 * @param	InInstallManifest	The manifest being installed so we can pull info from it
	 * @param	InBuildProgress		Pointer to the progress tracker, for setting progress and getting pause state
	 */
	static void Create( const FString& InSaveDirectory, const FBuildPatchAppManifestRef& InInstallManifest, FBuildPatchProgress* InBuildProgress );

	/**
	 * Get the singleton class object. Must only be called between Create and Shutdown calls otherwise the code will assert.
	 * @return		reference the the downloader system
	 */
	static FBuildPatchDownloader& Get();

	/**
	 * Shuts down and cleans up the downloader system, deallocating immediately.
	 */
	static void Shutdown();

private:

	// The threadsafe shared pointer holding the singleton download system
	static TSharedPtr< FBuildPatchDownloader, ESPMode::ThreadSafe > SingletonInstance;
};
