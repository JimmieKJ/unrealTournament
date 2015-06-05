// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Dependencies.
#include "ModuleManager.h"
#include "Core.h"

/** Struct to store information about a stream, returned from search results. */
struct FNetworkReplayStreamInfo
{
	FNetworkReplayStreamInfo() : SizeInBytes( 0 ), LengthInMS( 0 ), NumViewers( 0 ), bIsLive( false ), Changelist( 0 ) {}

	/** The name of the stream (generally this is auto generated, refer to friendly name for UI) */
	FString Name;

	/** The UI friendly name of the stream */
	FString FriendlyName;

	/** The date and time the stream was recorded */
	FDateTime Timestamp;

	/** The size of the stream */
	int64 SizeInBytes;
	
	/** The duration of the stream in MS */
	int32 LengthInMS;

	/** Number of viewers viewing this stream */
	int32 NumViewers;

	/** True if the stream is live and the game hasn't completed yet */
	bool bIsLive;

	/** The changelist of the replay */
	int32 Changelist;
};

namespace ENetworkReplayError
{
	enum Type
	{
		/** There are currently no issues */
		None,

		/** The backend service supplying the stream is unavailable, or connection interrupted */
		ServiceUnavailable,
	};

	inline const TCHAR* ToString( const ENetworkReplayError::Type FailureType )
	{
		switch ( FailureType )
		{
			case None:
				return TEXT( "None" );
			case ServiceUnavailable:
				return TEXT( "ServiceUnavailable" );
		}
		return TEXT( "Unknown ENetworkReplayError error" );
	}
}

/**
 * Delegate called when StartStreaming() completes.
 *
 * @param bWasSuccessful Whether streaming was started.
 * @param bRecord Whether streaming is recording or not (vs playing)
 */
DECLARE_DELEGATE_TwoParams( FOnStreamReadyDelegate, const bool, const bool );

/**
 * Delegate called when GotoCheckpointIndex() completes.
 *
 * @param bWasSuccessful Whether streaming was started.
 */
DECLARE_DELEGATE_TwoParams( FOnCheckpointReadyDelegate, const bool, const int64 );

/**
 * Delegate called when DeleteFinishedStream() completes.
 *
 * @param bWasSuccessful Whether the stream was deleted.
 */
DECLARE_DELEGATE_OneParam( FOnDeleteFinishedStreamComplete, const bool );

/**
 * Delegate called when EnumerateStreams() completes.
 *
 * @param Streams An array containing information about the streams that were found.
 */
DECLARE_DELEGATE_OneParam( FOnEnumerateStreamsComplete, const TArray<FNetworkReplayStreamInfo>& );

class FNetworkReplayVersion
{
public:
	FNetworkReplayVersion() : NetworkVersion( 0 ), Changelist( 0 ) {}
	FNetworkReplayVersion( const FString& InAppString, const uint32 InNetworkVersion, const uint32 InChangelist ) : AppString( InAppString ), NetworkVersion( InNetworkVersion ), Changelist( InChangelist ) {}

	FString		AppString;
	uint32		NetworkVersion;
	uint32		Changelist;
};

/** Generic interface for network replay streaming */
class INetworkReplayStreamer 
{
public:
	virtual ~INetworkReplayStreamer() {}

	virtual void StartStreaming( const FString& CustomName, const FString& FriendlyName, const TArray< FString >& UserNames, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FOnStreamReadyDelegate& Delegate ) = 0;
	virtual void StopStreaming() = 0;
	virtual FArchive* GetHeaderArchive() = 0;
	virtual FArchive* GetStreamingArchive() = 0;
	virtual FArchive* GetCheckpointArchive() = 0;
	virtual void FlushCheckpoint( const uint32 TimeInMS ) = 0;
	virtual void GotoCheckpointIndex( const int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate ) = 0;
	virtual void GotoTimeInMS( const uint32 TimeInMS, const FOnCheckpointReadyDelegate& Delegate ) = 0;
	virtual FArchive* GetMetadataArchive() = 0;
	virtual void UpdateTotalDemoTime( uint32 TimeInMS ) = 0;
	virtual uint32 GetTotalDemoTime() const = 0;
	virtual bool IsDataAvailable() const = 0;
	virtual void SetHighPriorityTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) = 0;
	virtual bool IsDataAvailableForTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) = 0;
	virtual bool IsLoadingCheckpoint() const = 0;
	
	/** Returns true if the playing stream is currently in progress */
	virtual bool IsLive() const = 0;

	/**
	 * Attempts to delete the stream with the specified name. May execute asynchronously.
	 *
	 * @param StreamName The name of the stream to delete
	 * @param Delegate A delegate that will be executed if bound when the delete operation completes
	 */
	virtual void DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate) const = 0;

	/**
	 * Retrieves the streams that are available for viewing. May execute asynchronously.
	 *
	 * @param Delegate A delegate that will be executed if bound when the list of streams is available
	 */
	virtual void EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const FOnEnumerateStreamsComplete& Delegate ) = 0;

	/**
	 * Retrieves the streams that have been recently viewed. May execute asynchronously.
	 *
	 * @param Delegate A delegate that will be executed if bound when the list of streams is available
	 */
	virtual void EnumerateRecentStreams( const FNetworkReplayVersion& ReplayVersion, const FString& RecentViewer, const FOnEnumerateStreamsComplete& Delegate ) = 0;

	/** Returns the last error that occurred while streaming replays */
	virtual ENetworkReplayError::Type GetLastError() const = 0;
};

/** Replay streamer factory */
class INetworkReplayStreamingFactory : public IModuleInterface
{
public:
	virtual TSharedPtr< INetworkReplayStreamer > CreateReplayStreamer() = 0;
};

/** Replay streaming factory manager */
class FNetworkReplayStreaming : public IModuleInterface
{
public:
	static inline FNetworkReplayStreaming& Get()
	{
		return FModuleManager::LoadModuleChecked< FNetworkReplayStreaming >( "NetworkReplayStreaming" );
	}

	virtual INetworkReplayStreamingFactory& GetFactory();
};
