// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchAnalytics.h: Declares static helper class for sending analytics events
	for the module.
	An IAnalyticsProvider should be provided by the code making use of this module.
=============================================================================*/

#pragma once

class FHttpServiceTracker;

/**
 * A struct to hold static fatal error flagging
 */
struct FBuildPatchAnalytics
{
private:
	// The analytics provider interface
	static TSharedPtr< IAnalyticsProvider > Analytics;

	// The analytics provider interface
	static TSharedPtr< FHttpServiceTracker > HttpTracker;

	// Avoid spamming download errors
	static FThreadSafeCounter DownloadErrors;

	// Avoid spamming cache errors
	static FThreadSafeCounter CacheErrors;

	// Avoid spamming construction errors
	static FThreadSafeCounter ConstructionErrors;

public:
	/**
	 * Set the analytics provider to be used when recording events
	 * @param AnalyticsProvider		Shared ptr to an analytics interface to use. If NULL analytics will be disabled.
	 */
	static void SetAnalyticsProvider( TSharedPtr< IAnalyticsProvider > AnalyticsProvider );

	/**
	 * Records an analytics event, must be called from the main thread only
	 * @param EventName		The name of the event being sent
	 * @param Attributes	The attributes for the event
	 */
	static void RecordEvent( const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes );

	/**
	 * Resets the event counters that monitor for spamming and limit the number of events that are sent
	 */
	static void ResetCounters();

	/**
	 * Record a chunk download error event
	 * @param ChunkUrl		The url for the chunk being downloaded
	 * @param ResponseCode	The http response code, should pass INDEX_NONE if no response or not http.
	 * @param ErrorString	The point at which the download has failed
	 */
	static void RecordChunkDownloadError( const FString& ChunkUrl, const int32& ResponseCode, const FString& ErrorString );

	/**
	 * Record a chunk download aborted event
	 * @param ChunkUrl		The url for the chunk being downloaded
	 * @param ChunkTime		The current running time for this chunk
	 * @param ChunkMean		The recorded average chunk download time
	 * @param ChunkStd		The recorded standard deviation for chunk download time
	 * @param BreakingPoint	The value used as the breaking point for chunk download time
	 */
	static void RecordChunkDownloadAborted( const FString& ChunkUrl, const double& ChunkTime, const double& ChunkMean, const double& ChunkStd, const double& BreakingPoint );

	/**
	 * Record a chunk cache error event
	 * @param ChunkGuid		The chunk guid that the error occurred for
	 * @param Filename		The filename for the related event
	 * @param LastError		The OS error code if appropriate
	 * @param SystemName	The cache system that the error comes from
	 * @param ErrorString	The type of error that has occurred
	 */
	static void RecordChunkCacheError( const FGuid& ChunkGuid, const FString& Filename, const int32 LastError, const FString& SystemName, const FString& ErrorString );

	/**
	 * Record a file construction error event
	 * @param Filename		The filename for the related event
	 * @param LastError		The OS error code if appropriate
	 * @param ErrorString	The type of error that has occurred
	 */
	static void RecordConstructionError( const FString& Filename, const int32 LastError, const FString& ErrorString );

	/**
	 * Record a file construction error event
	 * @param Filename		The installer file
	 * @param CommandLine	The command line passed to the installer
	 * @param ErrorCode		The OS error code if appropriate
	 * @param ErrorString	The type of error that has occurred
	 */
	static void RecordPrereqInstallnError( const FString& Filename, const FString& CommandLine, const int32 ErrorCode, const FString& ErrorString );

	/**
	 * Set the Http Service Tracker to be used for tracking Http Service responsiveness.
	 * Will only track HTTP requests, not file requests.
	 * @param HttpTracker	Shared ptr to an Http service tracker interface to use. If NULL tracking will be disabled.
	 */
	static void SetHttpTracker( TSharedPtr< FHttpServiceTracker > HttpTracker );

	/**
	 * Tracks a specific Http request. Currently there is only one endpoint, CDN.Chunk. 
	 * In the future we may decide to break up, say, file from chunk downloads, but not now.
	 * 
	 * @param const FHttpRequestPtr & Request
	 */
	static void TrackRequest( const FHttpRequestPtr& Request );

};
