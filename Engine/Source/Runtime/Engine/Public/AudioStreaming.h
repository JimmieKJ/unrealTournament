// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
AudioStreaming.h: Definitions of classes used for audio streaming.
=============================================================================*/

#pragma once

/** Lists possible states used by Thread-safe counter. */
enum EAudioStreamingState
{
	// There are no pending requests/ all requests have been fulfilled.
	AudioState_ReadyFor_Requests = 0,
	// Initial request has completed and finalization needs to be kicked off.
	AudioState_ReadyFor_Finalization = 1,
	// We're currently loading in chunk data.
	AudioState_InProgress_Loading = 2,
	// ...
	// States InProgress_Loading+N-1 means we're currently loading in N chunks
	// ...
};

/**
 * Async worker to stream audio chunks from the derived data cache.
 */
class FAsyncStreamDerivedChunkWorker : public FNonAbandonableTask
{
public:
	/** Initialization constructor. */
	FAsyncStreamDerivedChunkWorker(
		const FString& InDerivedDataKey,
		void* InDestChunkData,
		int32 InChunkSize,
		FThreadSafeCounter* InThreadSafeCounter
		);
	
	/**
	 * Retrieves the derived chunk from the derived data cache.
	 */
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncStreamDerivedChunkWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	/**
	 * Returns true if the streaming mip request failed.
	 */
	bool DidRequestFail() const
	{
		return bRequestFailed;
	}

private:
	/** Key for retrieving chunk data from the derived data cache. */
	FString DerivedDataKey;
	/** The location to which the chunk data should be copied. */
	void* DestChunkData;
	/** The size of the chunk in bytes. */
	int32 ExpectedChunkSize;
	/** true if the chunk data was not present in the derived data cache. */
	bool bRequestFailed;
	/** Thread-safe counter to decrement when data has been copied. */
	FThreadSafeCounter* ThreadSafeCounter;
};

/** Async task to stream chunks from the derived data cache. */
typedef FAsyncTask<FAsyncStreamDerivedChunkWorker> FAsyncStreamDerivedChunkTask;

/**
 * Contains a request to load chunks of a sound wave
 */
struct FWaveRequest
{
	TArray<uint32>	RequiredIndices;
	bool			bPrioritiseRequest;
};

/**
 * Stores info about an audio chunk once it's been loaded
 */
struct FLoadedAudioChunk
{
	uint8*	Data;
	int32	DataSize;
	int32	MemorySize;
	uint32	Index;
};

/**
 * Contains everything that will be needed by a SoundWave that's streaming in data
 */
struct FStreamingWaveData final
{
	FStreamingWaveData();
	~FStreamingWaveData();

	/**
	 * Sets up the streaming wave data and loads the first chunk of audio for instant play
	 *
	 * @param SoundWave	The SoundWave we are managing
	 */
	void Initialize(USoundWave* SoundWave);

	/**
	 * Updates the streaming status of the sound wave and performs finalization when appropriate. The function returns
	 * true while there are pending requests in flight and updating needs to continue.
	 *
	 * @return					true if there are requests in flight, false otherwise
	 */
	bool UpdateStreamingStatus();

	/**
	 * Tells the SoundWave which chunks are currently required so that it can start loading any needed
	 *
	 * @param InChunkIndices The Chunk Indices that are currently needed by all sources using this sound
	 * @param bShouldPrioritizeAsyncIORequest Whether request should have higher priority than usual
	 */
	void UpdateChunkRequests(FWaveRequest& InWaveRequest);

		/**
	 * Checks whether the requested chunk indices differ from those loaded
	 *
	 * @param IndicesToLoad		List of chunk indices that should be loaded
	 * @param IndicesToFree		List of chunk indices that should be freed
	 * @return Whether any changes to loaded chunks are required
	 */
	bool HasPendingRequests(TArray<uint32>& IndicesToLoad, TArray<uint32>& IndicesToFree) const;

	/**
	 * Kicks off any pending requests
	 */
	void BeginPendingRequests(const TArray<uint32>& IndicesToLoad, const TArray<uint32>& IndicesToFree);

#if WITH_EDITORONLY_DATA
	/**
	 * Finishes any Derived Data Cache requests that may be in progress
	 *
	 * @return Whether any of the requests failed.
	 */
	bool FinishDDCRequests();
#endif //WITH_EDITORONLY_DATA

private:
	// Don't allow copy construction as it could free shared memory
	FStreamingWaveData(const FStreamingWaveData& that);
	FStreamingWaveData& operator=(FStreamingWaveData const&);

	FLoadedAudioChunk* AddNewLoadedChunk(int32 ChunkSize);
	void FreeLoadedChunk(FLoadedAudioChunk& LoadedChunk);

public:
	/** SoundWave this streaming data is for */
	USoundWave* SoundWave;

	/** Thread-safe counter indicating the audio streaming state. */
	mutable FThreadSafeCounter	PendingChunkChangeRequestStatus;

	/* Contains pointers to Chunks of audio data that have been streamed in */
	TArray<FLoadedAudioChunk> LoadedChunks;

	/** Potentially outstanding audio chunk I/O requests */
	TArray<uint64>	IORequestIndices;

	/** Indices of chunks that are currently loaded */
	TArray<uint32>	LoadedChunkIndices;

	/** Indices of chunks we want to have loaded */
	FWaveRequest	CurrentRequest;

#if WITH_EDITORONLY_DATA
	/** Pending async derived data streaming tasks */
	TIndirectArray<FAsyncStreamDerivedChunkTask> PendingAsyncStreamDerivedChunkTasks;
#endif // #if WITH_EDITORONLY_DATA
};

/**
* Streaming manager dealing with audio.
*/
struct FAudioStreamingManager : public IAudioStreamingManager
{
	/** Constructor, initializing all members */
	FAudioStreamingManager();

	virtual ~FAudioStreamingManager();

	// IStreamingManager interface
	virtual void UpdateResourceStreaming( float DeltaTime, bool bProcessEverything=false ) override;
	virtual int32 BlockTillAllRequestsFinished( float TimeLimit = 0.0f, bool bLogResults = false ) override;
	virtual void CancelForcedResources() override;
	virtual void NotifyLevelChange() override;
	virtual void SetDisregardWorldResourcesForFrames( int32 NumFrames ) override;
	virtual void AddPreparedLevel( class ULevel* Level ) override;
	virtual void RemoveLevel( class ULevel* Level ) override;
	// End IStreamingManager interface

	// IAudioStreamingManager interface
	virtual void AddStreamingSoundWave(USoundWave* SoundWave) override;
	virtual void RemoveStreamingSoundWave(USoundWave* SoundWave) override;
	virtual bool IsManagedStreamingSoundWave(const USoundWave* SoundWave) const override;
	virtual bool IsStreamingInProgress(const USoundWave* SoundWave) override;
	virtual bool CanCreateSoundSource(const FWaveInstance* WaveInstance) const override;
	virtual void AddStreamingSoundSource(FSoundSource* SoundSource) override;
	virtual void RemoveStreamingSoundSource(FSoundSource* SoundSource) override;
	virtual bool IsManagedStreamingSoundSource(const FSoundSource* SoundSource) const override;
	virtual const uint8* GetLoadedChunk(const USoundWave* SoundWave, uint32 ChunkIndex) const override;
	// End IAudioStreamingManager interface

protected:
	/**
	 * Gets Wave request associated with a specific wave
	 *
	 * @param SoundWave		SoundWave we want request for
	 * @return Existing or new request structure
	 */
	FWaveRequest& GetWaveRequest(USoundWave* SoundWave);

	/** Sound Waves being managed. */
	TMap<USoundWave*, FStreamingWaveData> StreamingSoundWaves;

	/** Sound Sources being managed. */
	TArray<FSoundSource*>	StreamingSoundSources;

	/** Map of requests to make next time sound waves are ready */
	TMap<USoundWave*, FWaveRequest> WaveRequests;
};
