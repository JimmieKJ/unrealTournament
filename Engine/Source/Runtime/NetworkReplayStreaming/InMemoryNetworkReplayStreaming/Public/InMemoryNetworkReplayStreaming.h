// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkReplayStreaming.h"
#include "Core.h"
#include "ModuleManager.h"
#include "UniquePtr.h"
#include "OnlineJsonSerializer.h"
#include "Tickable.h"

class FInMemoryNetworkReplayStreamingFactory;

/* Holds all data about an entire replay */
struct FInMemoryReplay
{
	struct FCheckpoint
	{
		FCheckpoint() :
			TimeInMS(0),
			StreamByteOffset(0) {}

		TArray<uint8> Data;
		uint32 TimeInMS;
		uint32 StreamByteOffset;
	};

	FInMemoryReplay() :
		NetworkVersion(0) {}

	TArray<uint8> Header;
	TArray<uint8> Stream;
	TArray<uint8> Metadata;
	TArray<FCheckpoint> Checkpoints;
	FNetworkReplayStreamInfo StreamInfo;
	uint32 NetworkVersion;
};

/** Streamer that keeps all data in memory only */
class FInMemoryNetworkReplayStreamer : public INetworkReplayStreamer, public FTickableGameObject
{
public:
	FInMemoryNetworkReplayStreamer(FInMemoryNetworkReplayStreamingFactory* InFactory) :
		OwningFactory( InFactory ),
		StreamerState( EStreamerState::Idle )
	{
		check(OwningFactory != nullptr);
	}
	
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
	virtual uint32 GetTotalDemoTime() const override;
	virtual bool IsDataAvailable() const override;
	virtual void SetHighPriorityTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) override { }
	virtual bool IsDataAvailableForTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS ) override { return true; }
	virtual bool IsLoadingCheckpoint() const override { return false; }
	virtual bool IsLive() const override;
	virtual void DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate) const override;
	virtual void EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const FOnEnumerateStreamsComplete& Delegate ) override;
	virtual void EnumerateStreams( const FNetworkReplayVersion& InReplayVersion, const FString& UserString, const FString& MetaString, const TArray< FString >& ExtraParms, const FOnEnumerateStreamsComplete& Delegate ) override;
	virtual void EnumerateRecentStreams( const FNetworkReplayVersion& ReplayVersion, const FString& RecentViewer, const FOnEnumerateStreamsComplete& Delegate ) override {}
	virtual ENetworkReplayError::Type GetLastError() const override { return ENetworkReplayError::None; }
	virtual void AddUserToReplay(const FString& UserString) override;
	virtual void AddEvent(const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data) override;
	virtual void AddOrUpdateEvent( const FString& Name, const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data ) override {}
	virtual void EnumerateEvents(const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate) override;
	virtual void EnumerateEvents( const FString& ReplayName, const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate ) override;
	virtual void RequestEventData(const FString& EventID, const FOnRequestEventDataComplete& RequestEventDataComplete) override;
	virtual void SearchEvents(const FString& EventGroup, const FOnEnumerateStreamsComplete& Delegate) override;
	virtual void KeepReplay( const FString& ReplayName, const bool bKeep ) override;

	/** FTickableObjectBase implementation */
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;

	/** FTickableGameObject implementation */
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	bool IsNamedStreamLive( const FString& StreamName ) const;

	/** Handles the details of loading a checkpoint */
	void GotoCheckpointIndexInternal(int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate, int32 TimeInMS);

	/**
	 * Returns a pointer to the currently active (recording or playback) replay in the owning factory's map.
	 * May return null if if the streamer state is idle.
	 */
	FInMemoryReplay* GetCurrentReplay();

	/**
	 * Returns a pointer to the currently active (recording or playback) replay in the owning factory's map.
	 * May return null if if the streamer state is idle.
	 */
	FInMemoryReplay* GetCurrentReplay() const;

	/**
	 * Returns a pointer to the currently active (recording or playback) replay in the owning factory's map.
	 * Asserts if the returned replay pointer would be null.
	 */
	FInMemoryReplay* GetCurrentReplayChecked();

	/**
	 * Returns a pointer to the currently active (recording or playback) replay in the owning factory's map.
	 * Asserts if the returned replay pointer would be null.
	 */
	FInMemoryReplay* GetCurrentReplayChecked() const;

	/** Pointer to the factory that owns this streamer instance */
	FInMemoryNetworkReplayStreamingFactory* OwningFactory;

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
};

class FInMemoryNetworkReplayStreamingFactory : public INetworkReplayStreamingFactory
{
public:
	virtual TSharedPtr< INetworkReplayStreamer > CreateReplayStreamer();

	friend class FInMemoryNetworkReplayStreamer;

private:
	TMap<FString, TUniquePtr<FInMemoryReplay>> Replays;
};
