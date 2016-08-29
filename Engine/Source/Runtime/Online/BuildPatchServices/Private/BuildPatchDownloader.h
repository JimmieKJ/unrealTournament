// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

	// Config count allowed for retries
	const int32 MaxRetryCount;

	// Config array for retry times
	const TArray<float> RetryDelayTimes;

	// Config array for health percentages
	const TArray<float> HealthPercentages;

	// Config for disconnected delay
	const float DisconnectedDelay;

	// The directory to save data to
	const FString SaveDirectory;

	// The base urls for downloads
	const TArray<FString> CloudDirectories;

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

	// A flag marking whether the thread is failing all current requests
	bool bIsDisconnected;

	// A flag that says whether more chunks could still be queued
	bool bWaitingForJobs;

	// The current overall download success rate
	float ChunkSuccessRate;

	// Used to help time health states
	volatile int64 CyclesAtLastHealthState;

	// Timers in seconds for how long we were in each health state
	TArray<float> HealthStateTimes;

	// The current overall download health, determined by ChunkSuccessRate
	EBuildPatchDownloadHealth DownloadHealth;

	// A critical section to protect the flags and rate
	FCriticalSection FlagsLock;

	// The time in cycles since we received data
	volatile int64 CyclesAtLastData;

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

	// Counter for tracking how many chunks we did not successfully receive
	FThreadSafeCounter NumFailedDownloads;

	// Counter for tracking how many chunks we downloaded fine but were determined bad data
	FThreadSafeCounter NumBadDownloads;

	// Pointer to the build progress tracker
	FBuildPatchProgress* BuildProgress;

public:

	/**
	 * Constructor
	 * @param SaveDirectory         The chunk save to directory
	 * @param CloudDirectories      The base paths or urls for the downloads
	 * @param InstallManifest       The manifest being installed so we can pull info from it
	 * @param BuildProgress         Pointer to the progress tracker, for setting progress and getting pause state
	 */
	FBuildPatchDownloader(const FString& SaveDirectory, const TArray<FString>& CloudDirectories, const FBuildPatchAppManifestRef& InstallManifest, FBuildPatchProgress* BuildProgress);

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
	 * Get whether the thread is idle waiting for chunks to download.
	 * @return True if the thread completed.
	 */
	bool IsIdle();

	/**
	 * Get whether the thread is currently failing all downloads.
	 * @return True if the thread is not getting success responses.
	 */
	bool IsDisconnected();

	/**
	 * Gets the array of download recordings. Should not be polled, only call when the thread has finished to gather data.
	 * @return An array of download records.
	 */
	TArray< FBuildPatchDownloadRecord > GetDownloadRecordings();

	/**
	 * Adds a chunk GUID to the download list.
	 * @param Guid                  The GUID for the data.
	 * @param bInsertAtFront        If this should be queued at the front.
	 */
	void AddChunkToDownload( const FGuid& Guid, const bool bInsertAtFront = false );

	/**
	 * Adds an array of chunk GUIDs to the download list.
	 * @param Guid                  The GUID array for the data.
	 * @param bInsertAtFront        If this should be queued at the front.
	 */
	void AddChunksToDownload( const TArray< FGuid >& Guids, const bool bInsertAtFront = false );

	/**
	 * Clear out the download list.
	 */
	void ClearDownloadJobs();

	/**
	 * A notification for when there is no possibility of more jobs to be queued (so the thread can end).
	 */
	void NotifyNoMoreChunksToAdd();

	/**
	 * Get how many chunks are in the list.
	 * @return The number of chunks in the list.
	 */
	int32 GetNumChunksLeft();

	/**
	 * Get the number of bytes left to download.
	 * @return The number of byte left to download.
	 */
	int64 GetNumBytesLeft();

	/**
	 * Get the number of bytes that have been queued.
	 * @return The total number of bytes queued.
	 */
	int64 GetNumBytesQueued();

	/**
	 * Get how many bytes have been downloaded, resetting the counter.
	 * @return The number of bytes downloaded since the last call.
	 */
	int32 GetByteDownloadCountReset();

	/**
	 * Get the success rate of chunk downloads in range of 0 to 1.
	 * @return The current success rate.
	 */
	float GetDownloadSuccessRate();

	/**
	 * Get the current download health based on configured percentage bars of success rate.
	 * @return The currently reported download health.
	 */
	EBuildPatchDownloadHealth GetDownloadHealth();

	/**
	 * Get an array indexable by EBuildPatchDownloadHealth providing the time spent in each health state.
	 * @return The health timers.
	 */
	TArray<float> GetDownloadHealthTimers();

	/**
	 * Get how many chunks we did not successfully receive.
	 * @return The number of failed chunks.
	 */
	int32 GetNumFailedDownloads();

	/**
	 * Get how many chunks we downloaded fine but were determined bad data.
	 * @return The number of bad chunks.
	 */
	int32 GetNumBadDownloads();

private:

	/**
	 * Load from configuration, the number of time to retry each request before failing. Negative indicates always retry.
	 * @return The desired number of retries per request, from config or default.
	 */
	int32 LoadRetryCount() const;

	/**
	 * Load from configuration, the set of times to wait in between each retry up to the maximum delay.
	 * @return The desired retry times, from config or default.
	 */
	TArray<float> LoadRetryTimes() const;

	/**
	 * Load from configuration, the disconnection delay time.
	 * @return The amount of time with no data received to consider a disconnected state.
	 */
	float LoadDisconnectDelay() const;

	/**
	 * Load from configuration, the percentages of success rate that determine the download health status.
	 * @return The percentages used to determine the health status.
	 */
	TArray<float> LoadHealthPercentages() const;

	/**
	 * Get the desired cloud directory given the retry number that a chunk is on.
	 * @param RetryNum              The zero based retry number for the chunk to be downloaded.
	 * @return The base url to use for the download.
	 */
	const FString& GetCloudDirectory(int32 RetryNum) const;

	/**
	 * Sets the bIsRunning flag.
	 * @param bRunning              Whether the thread is running.
	 */
	void SetRunning( bool bRunning );

	/**
	 * Sets the bIsInited flag.
	 * @param bInited               Whether the thread successfully initialized.
	 */
	void SetInited( bool bInited );

	/**
	 * Sets the bIsIdle flag.
	 * @param bIdle                 Whether the thread is waiting for jobs.
	 */
	void SetIdle( bool bIdle );

	/**
	 * Sets the bIsDisconnected flag.
	 * @param bIsDisconnected       Whether the thread is stalling on all requests.
	 */
	void SetIsDisconnected(bool bIsDisconnected);

	/**
	 * Sets the current request success rate.
	 * @param SuccessRate           The rate of success from 0.0f to 1.0f.
	 */
	void SetSuccessRate(float SuccessRate);

	/**
	 * Update download health based on success rate and whether disconnected
	 * @param bFlushTimer           Whether to flush the current timer to the current state
	 */
	void UpdateDownloadHealth(bool bFlushTimer = false);

	/**
	 * Add some bytes to the download counter.
	 * @param NumBytes              The number of bytes to add.
	 */
	void IncrementByteDownloadCount( const int32& NumBytes );

	/**
	 * Gets whether the thread still has work to do.
	 * @return False if the thread can quit.
	 */
	bool ShouldBeRunning();

	/**
	 * Receives the HTTP request bytes downloaded when downloading a chunk.
	 * @param Request               The request that was made.
	 * @param BytesSentSoFar        The total number of bytes sent so far.
	 * @param BytesReceivedSoFar    The total number of bytes downloaded so far.
	 */
	void HttpRequestProgress( FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived );

	/**
	 * Receives the HTTP response when downloading a chunk.
	 * @param Request               The request that was made.
	 * @param Response              The request's response.
	 * @param bSucceeded            Whether the request successfully completed.
	 */
	void HttpRequestComplete( FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded );

	/**
	 * Progress update for download of a chunk, from HTTP or Network/HDD.
	 * @param Guid                  The guid for the data.
	 * @param BytesSoFar            The number of bytes downloaded so far.
	 */
	void OnDownloadProgress( const FGuid& Guid, const int32& BytesSoFar );

	/**
	 * Completes a download of a chunk, from HTTP or Network/HDD.
	 * @param Guid                  The guid for the data.
	 * @param DownloadUrl           The url used to download the data.
	 * @param DataArray             The array of bytes for the data.
	 * @param bSucceeded            Whether the download was successful.
	 * @param ResponseCode          The HTTP response code.
	 */
	void OnDownloadComplete( const FGuid& Guid, const FString& DownloadUrl, const TArray< uint8 >& DataArray, bool bSucceeded, int32 ResponseCode );

	/**
	 * Get the next GUID to download data with.
	 * @param Guid                  Receives the download data GUID.
	 * @return True if there was a job in the list.
	 */
	bool GetNextDownload( FGuid& Guid );

	/**
	 * Get the required retry delay for a chunk retry.
	 * @param RetryCount            The retry number.
	 * @return The required retry delay in seconds.
	 */
	float GetRetryDelay(int32 RetryCount);

	/* Here we have static access for the singleton
	*****************************************************************************/
public:

	/**
	 * Creates the singleton downloader system. Takes the arguments required for construction.
	 * @param SaveDirectory         The chunk save to directory.
	 * @param CloudDirectories      The base paths or urls for the downloads.
	 * @param InstallManifest       The manifest being installed so we can pull info from it.
	 * @param BuildProgress         Pointer to the progress tracker, for setting progress and getting pause state.
	 */
	static void Create(const FString& SaveDirectory, const TArray<FString>& CloudDirectories, const FBuildPatchAppManifestRef& InstallManifest, FBuildPatchProgress* BuildProgress);

	/**
	 * Get the singleton class object. Must only be called between Create and Shutdown calls otherwise the code will assert.
	 * @return Reference the the downloader system.
	 */
	static FBuildPatchDownloader& Get();

	/**
	 * Shuts down and cleans up the downloader system, deallocating immediately.
	 */
	static void Shutdown();

private:

	// The threadsafe shared pointer holding the singleton download system.
	static TSharedPtr< FBuildPatchDownloader, ESPMode::ThreadSafe > SingletonInstance;
};
