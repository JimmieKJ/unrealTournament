// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkReplayStreaming.h"
#include "Core.h"
#include "ModuleManager.h"
#include "UniquePtr.h"
#include "OnlineJsonSerializer.h"

/* Class to hold metadata about an entire replay */
class FNullReplayInfo : public FOnlineJsonSerializable
{
public:
	FNullReplayInfo() : LengthInMS(0), NetworkVersion(0), Changelist(0) {}

	int32		LengthInMS;
	uint32		NetworkVersion;
	uint32		Changelist;
	FString		FriendlyName;

	// FOnlineJsonSerializable
	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE( "LengthInMS",		LengthInMS );
		ONLINE_JSON_SERIALIZE( "NetworkVersion",	NetworkVersion );
		ONLINE_JSON_SERIALIZE( "Changelist",		Changelist );
		ONLINE_JSON_SERIALIZE( "FriendlyName",		FriendlyName );
	END_ONLINE_JSON_SERIALIZER
};

/** Default streamer that goes straight to the HD */
class FNullNetworkReplayStreamer : public INetworkReplayStreamer
{
public:
	FNullNetworkReplayStreamer() :
		StreamerState( EStreamerState::Idle ),
		CurrentCheckpointIndex( 0 )
	{}
	
	/** INetworkReplayStreamer implementation */
	virtual void StartStreaming( const FString& CustomName, const FString& FriendlyName, const TArray< FString >& UserNames, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FOnStreamReadyDelegate& Delegate ) override;
	virtual void StopStreaming() override;
	virtual FArchive* GetHeaderArchive() override;
	virtual FArchive* GetStreamingArchive() override;
	virtual FArchive* GetCheckpointArchive() override;
	virtual void FlushCheckpoint( const uint32 TimeInMS ) override;
	virtual void GotoCheckpointIndex( const int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate ) override;
	virtual void GotoTimeInMS( const uint32 TimeInMS, const FOnCheckpointReadyDelegate& Delegate ) override;
	virtual FArchive* GetMetadataArchive() override;
	virtual void UpdateTotalDemoTime( uint32 TimeInMS ) override;
	virtual uint32 GetTotalDemoTime() const override { return ReplayInfo.LengthInMS; }
	virtual bool IsDataAvailable() const override { return true; }
	virtual void SetHighPriorityTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) override { }
	virtual bool IsDataAvailableForTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) override { return true; }
	virtual bool IsLoadingCheckpoint() const override { return false; }
	virtual bool IsLive() const override;
	virtual void DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate) const override;
	virtual void EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const FOnEnumerateStreamsComplete& Delegate ) override;
	virtual void EnumerateRecentStreams( const FNetworkReplayVersion& ReplayVersion, const FString& RecentViewer, const FOnEnumerateStreamsComplete& Delegate ) override {}
	virtual ENetworkReplayError::Type GetLastError() const override { return ENetworkReplayError::None; }
	virtual void AddUserToReplay( const FString& UserString );

private:
	bool IsNamedStreamLive( const FString& StreamName ) const;

	/** Handles the details of loading a checkpoint */
	void GotoCheckpointIndexInternal(int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate, int32 TimeInMS);

	/** Handle to the archive that will read/write the demo header */
	TUniquePtr<FArchive> HeaderAr;

	/** Handle to the archive that will read/write network packets */
	TUniquePtr<FArchive> FileAr;

	/* Handle to the archive that will read/write metadata */
	TUniquePtr<FArchive> MetadataFileAr;

	/* Handle to the archive that will read/write checkpoint files */
	TUniquePtr<FArchive> CheckpointAr;

	/** EStreamerState - Overall state of the streamer */
	enum class EStreamerState
	{
		Idle,					// The streamer is idle. Either we haven't started streaming yet, or we are done
		Recording,				// We are in the process of recording a replay to disk
		Playback,				// We are in the process of playing a replay from disk
	};

	/** Overall state of the streamer */
	EStreamerState StreamerState;

	/** Remember the name of the current stream, if any. */
	FString CurrentStreamName;

	/** Current number of checkpoints written. */
	int32 CurrentCheckpointIndex;

	/** Currently playing or recording replay metadata */
	FNullReplayInfo ReplayInfo;
};

class FNullNetworkReplayStreamingFactory : public INetworkReplayStreamingFactory
{
public:
	virtual TSharedPtr< INetworkReplayStreamer > CreateReplayStreamer();
};
