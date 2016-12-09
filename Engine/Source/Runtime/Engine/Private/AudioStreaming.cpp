// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
AudioStreaming.cpp: Implementation of audio streaming classes.
=============================================================================*/

#include "AudioStreaming.h"
#include "Misc/CoreStats.h"
#include "HAL/IOBase.h"
#include "Sound/SoundWave.h"
#include "Sound/AudioSettings.h"
#include "DerivedDataCacheInterface.h"
#include "Serialization/MemoryReader.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"

#if USE_NEW_ASYNC_IO
#include "AsyncFileHandle.h"
#endif
/*------------------------------------------------------------------------------
	Streaming chunks from the derived data cache.
------------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA

/** Initialization constructor. */
FAsyncStreamDerivedChunkWorker::FAsyncStreamDerivedChunkWorker(
	const FString& InDerivedDataKey,
	void* InDestChunkData,
	int32 InChunkSize,
	FThreadSafeCounter* InThreadSafeCounter
	)
	: DerivedDataKey(InDerivedDataKey)
	, DestChunkData(InDestChunkData)
	, ExpectedChunkSize(InChunkSize)
	, bRequestFailed(false)
	, ThreadSafeCounter(InThreadSafeCounter)
{
}

/** Retrieves the derived chunk from the derived data cache. */
void FAsyncStreamDerivedChunkWorker::DoWork()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FAsyncStreamDerivedChunkWorker::DoWork"), STAT_AsyncStreamDerivedChunkWorker_DoWork, STATGROUP_StreamingDetails);

	UE_LOG(LogAudio, Verbose, TEXT("Start of ASync DDC Chunk read for key: %s"), *DerivedDataKey);

	TArray<uint8> DerivedChunkData;

	if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedChunkData))
	{
		FMemoryReader Ar(DerivedChunkData, true);
		int32 ChunkSize = 0;
		Ar << ChunkSize;
		checkf(ChunkSize == ExpectedChunkSize, TEXT("ChunkSize(%d) != ExpectedSize(%d)"), ChunkSize, ExpectedChunkSize);
		Ar.Serialize(DestChunkData, ChunkSize);
	}
	else
	{
		bRequestFailed = true;
	}
	FPlatformMisc::MemoryBarrier();
	ThreadSafeCounter->Decrement();

	UE_LOG(LogAudio, Verbose, TEXT("End of ASync DDC Chunk read for key: %s"), *DerivedDataKey);
}

#endif // #if WITH_EDITORONLY_DATA

////////////////////////
// FStreamingWaveData //
////////////////////////

FStreamingWaveData::FStreamingWaveData()
	: SoundWave(NULL)
#if USE_NEW_ASYNC_IO
	, IORequestHandle(nullptr)
#endif
{
#if USE_NEW_ASYNC_IO
	AsyncFileCallBack =
		[this](bool bWasCancelled, IAsyncReadRequest* Req)
	{
		uint8* Mem = Req->GetReadResults();
		if (Mem)
		{
			bool bFound = false;
			for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
			{
				if (LoadedChunk.IORequest == Req)
				{
					check(!LoadedChunk.Data);
					LoadedChunk.Data = Mem;
					DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, LoadedChunk.DataSize);
					INC_DWORD_STAT_BY(STAT_AudioMemorySize, LoadedChunk.DataSize);
					INC_DWORD_STAT_BY(STAT_AudioMemory, LoadedChunk.DataSize);
					bFound = true;
					break;
				}
			}
			check(bFound);
		}
		// else was canceled
		PendingChunkChangeRequestStatus.Decrement();
	};
#endif
}

FStreamingWaveData::~FStreamingWaveData()
{
	// Make sure there are no pending requests in flight.
#if USE_NEW_ASYNC_IO
	{
		for (int32 Pass = 0; Pass < 3; Pass++)
		{
			BlockTillAllRequestsFinished();
			if (!UpdateStreamingStatus())
			{
				break;
			}
			check(Pass < 2); // we should be done after two passes. Pass 0 will start anything we need and pass 1 will complete those requests
		}
	}
#else
	{
		QUICK_SCOPE_CYCLE_COUNTER(FStreamingWaveData_Destructor_Spin);
		while (UpdateStreamingStatus() == true)
		{
			// Give up timeslice.
			check(!USE_NEW_ASYNC_IO); // should be done because we block on it above
			FPlatformProcess::SleepNoStats(0.001f);
		}
	}
#endif

	for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
	{
		FreeLoadedChunk(LoadedChunk);
	}
#if USE_NEW_ASYNC_IO
	if (IORequestHandle)
	{
		delete IORequestHandle;
		IORequestHandle = nullptr;
	}
#endif
}

void FStreamingWaveData::Initialize(USoundWave* InSoundWave)
{
#if USE_NEW_ASYNC_IO
	check(!IORequestHandle);
#endif
	SoundWave = InSoundWave;

	// Always get the first chunk of data so we can play immediately
	check(LoadedChunks.Num() == 0);
	check(LoadedChunkIndices.Num() == 0);
	FLoadedAudioChunk* FirstChunk = AddNewLoadedChunk(SoundWave->RunningPlatformData->Chunks[0].DataSize);
	FirstChunk->Index = 0;
	SoundWave->GetChunkData(0, &FirstChunk->Data);

	// Set up the loaded/requested indices to be identical
	LoadedChunkIndices.Add(0);
	CurrentRequest.RequiredIndices.Add(0);
}

bool FStreamingWaveData::UpdateStreamingStatus()
{
	bool	bHasPendingRequestInFlight = true;
	int32	RequestStatus = PendingChunkChangeRequestStatus.GetValue();
	TArray<uint32> IndicesToLoad;
	TArray<uint32> IndicesToFree;

	if (!HasPendingRequests(IndicesToLoad, IndicesToFree))
	{
		check(RequestStatus == AudioState_ReadyFor_Requests);
		bHasPendingRequestInFlight = false;
	}
	// Pending request in flight, though we might be able to finish it.
	else
	{
		if (RequestStatus == AudioState_ReadyFor_Finalization)
		{
			if (UE_LOG_ACTIVE(LogAudio, Log) && IndicesToLoad.Num() > 0)
			{
				FString LogString = FString::Printf(TEXT("Finalised loading of chunk(s) %d"), IndicesToLoad[0]);
				for (int32 Index = 1; Index < IndicesToLoad.Num(); ++ Index)
				{
					LogString += FString::Printf(TEXT(", %d"), IndicesToLoad[Index]);
				}
				LogString += FString::Printf(TEXT(" from SoundWave'%s'"), *SoundWave->GetName());
				UE_LOG(LogAudio, Log, TEXT("%s"), *LogString);
			}

			bool bFailedRequests = false;
#if WITH_EDITORONLY_DATA
			bFailedRequests = FinishDDCRequests();
#endif //WITH_EDITORONLY_DATA

#if USE_NEW_ASYNC_IO
			// could maybe iterate over the things we know are done, but I couldn't tell if that was IndicesToLoad or not.
			for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
			{
				if (LoadedChunk.IORequest && LoadedChunk.IORequest->PollCompletion())
				{
					LoadedChunk.IORequest->WaitCompletion();
					delete LoadedChunk.IORequest;
					LoadedChunk.IORequest = nullptr;
				}
			}
#endif

			PendingChunkChangeRequestStatus.Decrement();
			bHasPendingRequestInFlight = false;
			LoadedChunkIndices = CurrentRequest.RequiredIndices;
		}
		else if (RequestStatus == AudioState_ReadyFor_Requests) // odd that this is an else, probably we should start requests right now
		{
			BeginPendingRequests(IndicesToLoad, IndicesToFree);
		}
	}

	return bHasPendingRequestInFlight;
}

void FStreamingWaveData::UpdateChunkRequests(FWaveRequest& InWaveRequest)
{
	// Might change this but ensures chunk 0 stays loaded for now
	check(InWaveRequest.RequiredIndices.Contains(0));
	check(PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests);

	CurrentRequest = InWaveRequest;

#if !USE_NEW_ASYNC_IO
	// Clear last batch of request indices
	IORequestIndices.Empty();
#endif
}

bool FStreamingWaveData::HasPendingRequests(TArray<uint32>& IndicesToLoad, TArray<uint32>& IndicesToFree) const
{
	IndicesToLoad.Empty();
	IndicesToFree.Empty();

	// Find indices that aren't loaded
	for (auto NeededIndex : CurrentRequest.RequiredIndices)
	{
		if (!LoadedChunkIndices.Contains(NeededIndex))
		{
			IndicesToLoad.AddUnique(NeededIndex);
		}
	}

	// Find indices that aren't needed anymore
	for (auto CurrentIndex : LoadedChunkIndices)
	{
		if (!CurrentRequest.RequiredIndices.Contains(CurrentIndex))
		{
			IndicesToFree.AddUnique(CurrentIndex);
		}
	}

	return IndicesToLoad.Num() > 0 || IndicesToFree.Num() > 0;
}

void FStreamingWaveData::BeginPendingRequests(const TArray<uint32>& IndicesToLoad, const TArray<uint32>& IndicesToFree)
{
	if (UE_LOG_ACTIVE(LogAudio, Log) && IndicesToLoad.Num() > 0)
	{
		FString LogString = FString::Printf(TEXT("Requesting ASync load of chunk(s) %d"), IndicesToLoad[0]);
		for (int32 Index = 1; Index < IndicesToLoad.Num(); ++Index)
		{
			LogString += FString::Printf(TEXT(", %d"), IndicesToLoad[Index]);
		}
		LogString += FString::Printf(TEXT(" from SoundWave'%s'"), *SoundWave->GetName());
		UE_LOG(LogAudio, Log, TEXT("%s"), *LogString);
	}

	// Mark Chunks for removal in case they can be reused
#if USE_NEW_ASYNC_IO
	for (auto Index : IndicesToFree)
	{
		for (int32 ChunkIndex = 0; ChunkIndex < LoadedChunks.Num(); ++ChunkIndex)
		{
			if (LoadedChunks[ChunkIndex].Index == Index)
			{
				FreeLoadedChunk(LoadedChunks[ChunkIndex]);
				LoadedChunks.RemoveAt(ChunkIndex);
				break;
			}
		}
	}
#else
	TArray<uint32> FreeChunkIndices;
	for (auto Index : IndicesToFree)
	{
		for (int32 ChunkIndex = 0; ChunkIndex < LoadedChunks.Num(); ++ChunkIndex)
		{
			if (LoadedChunks[ChunkIndex].Index == Index)
			{
				check(!FreeChunkIndices.Contains(ChunkIndex));
				FreeChunkIndices.Add(ChunkIndex);
				break;
			}
		}
	}
#endif

	if (IndicesToLoad.Num() > 0)
	{
		PendingChunkChangeRequestStatus.Set(AudioState_InProgress_Loading);

		// Set off all IO Requests
		for (auto Index : IndicesToLoad)
		{
			const FStreamedAudioChunk& Chunk = SoundWave->RunningPlatformData->Chunks[Index];
			int32 ChunkSize = Chunk.DataSize;

			FLoadedAudioChunk* ChunkStorage = NULL;

#if !USE_NEW_ASYNC_IO
			for (auto FreeIndex : FreeChunkIndices)
			{
				if (LoadedChunks[FreeIndex].MemorySize >= ChunkSize)
				{
					FreeChunkIndices.Remove(FreeIndex);
					ChunkStorage = &LoadedChunks[FreeIndex];
					ChunkStorage->DataSize = ChunkSize;
					ChunkStorage->Index = Index;
					break;
				}
			}
#endif
			if (ChunkStorage == NULL)
			{
				ChunkStorage = AddNewLoadedChunk(ChunkSize);
				ChunkStorage->Index = Index;
			}

			// Pass the request on to the async io manager after increasing the request count. The request count 
			// has been pre-incremented before fielding the update request so we don't have to worry about file
			// I/O immediately completing and the game thread kicking off again before this function
			// returns.
			PendingChunkChangeRequestStatus.Increment();

			EAsyncIOPriority AsyncIOPriority = CurrentRequest.bPrioritiseRequest ? AIOP_BelowNormal : AIOP_Low;

			// Load and decompress async.
#if WITH_EDITORONLY_DATA
			if (Chunk.DerivedDataKey.IsEmpty() == false)
			{
				FAsyncStreamDerivedChunkTask* Task = new(PendingAsyncStreamDerivedChunkTasks)FAsyncStreamDerivedChunkTask(
					Chunk.DerivedDataKey,
					ChunkStorage->Data,
					ChunkSize,
					&PendingChunkChangeRequestStatus
					);
				Task->StartBackgroundTask();
			}
			else
#endif // #if WITH_EDITORONLY_DATA
			{
				check(Chunk.BulkData.GetFilename().Len());
#if USE_NEW_ASYNC_IO
				if (Chunk.BulkData.IsStoredCompressedOnDisk())
				{
					check(!"USE_NEW_ASYNC_IO does not support compression at the package level.");
				}
				else
				{
					check(!ChunkStorage->IORequest);
					if (!IORequestHandle)
					{
						IORequestHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*Chunk.BulkData.GetFilename());
						check(IORequestHandle); // this generally cannot fail because it is async
					}
					check(Chunk.BulkData.GetBulkDataSize() == ChunkStorage->DataSize);
					ChunkStorage->IORequest = IORequestHandle->ReadRequest(Chunk.BulkData.GetBulkDataOffsetInFile(), ChunkStorage->DataSize, AsyncIOPriority, &AsyncFileCallBack);
					if (!ChunkStorage->IORequest)
					{
						// we failed for some reason; file not found I guess.
						PendingChunkChangeRequestStatus.Decrement();
					}

				}
#else
				if (Chunk.BulkData.IsStoredCompressedOnDisk())
				{
					IORequestIndices.AddUnique(FIOSystem::Get().LoadCompressedData(
						Chunk.BulkData.GetFilename(),						// filename
						Chunk.BulkData.GetBulkDataOffsetInFile(),			// offset
						Chunk.BulkData.GetBulkDataSizeOnDisk(),				// compressed size
						Chunk.BulkData.GetBulkDataSize(),					// uncompressed size
						ChunkStorage->Data,									// dest pointer
						Chunk.BulkData.GetDecompressionFlags(),				// compressed data format
						&PendingChunkChangeRequestStatus,					// counter to decrement
						AsyncIOPriority										// priority
						)
						);
				}
				// Load async.
				else
				{
					IORequestIndices.AddUnique(FIOSystem::Get().LoadData(
						Chunk.BulkData.GetFilename(),						// filename
						Chunk.BulkData.GetBulkDataOffsetInFile(),			// offset
						Chunk.BulkData.GetBulkDataSize(),					// size
						ChunkStorage->Data,									// dest pointer
						&PendingChunkChangeRequestStatus,					// counter to decrement
						AsyncIOPriority										// priority
						)
						);
				}
				check(IORequestIndices[IORequestIndices.Num() - 1]);
#endif
			}
		}

		// Decrement the state to AudioState_InProgress_Loading + NumChunksCurrentLoading - 1.
		PendingChunkChangeRequestStatus.Decrement();
	}
	else
	{
		// Skip straight to finalisation
		PendingChunkChangeRequestStatus.Set(AudioState_ReadyFor_Finalization);
	}

#if !USE_NEW_ASYNC_IO
	// Ensure indices are in order so we can step through backwards
	FreeChunkIndices.Sort();

	for (int32 FreeIndex = FreeChunkIndices.Num() - 1; FreeIndex >= 0; --FreeIndex)
	{
		FreeLoadedChunk(LoadedChunks[FreeChunkIndices[FreeIndex]]);
		LoadedChunks.RemoveAt(FreeChunkIndices[FreeIndex]);
	}
#endif
}

#if USE_NEW_ASYNC_IO

bool FStreamingWaveData::BlockTillAllRequestsFinished(float TimeLimit)
{
	QUICK_SCOPE_CYCLE_COUNTER(FStreamingWaveData_BlockTillAllRequestsFinished);
	if (TimeLimit == 0.0f)
	{
		for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
		{
			if (LoadedChunk.IORequest)
			{
				LoadedChunk.IORequest->WaitCompletion();
				delete LoadedChunk.IORequest;
				LoadedChunk.IORequest = nullptr;
			}
		}
	}
	else
	{
		double EndTime = FPlatformTime::Seconds() + TimeLimit;
		for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
		{
			if (LoadedChunk.IORequest)
			{
				float ThisTimeLimit = EndTime - FPlatformTime::Seconds();
				if (ThisTimeLimit < .001f || // one ms is the granularity of the platform event system
					!LoadedChunk.IORequest->WaitCompletion(ThisTimeLimit))
				{
					return false;
				}
				delete LoadedChunk.IORequest;
				LoadedChunk.IORequest = nullptr;
			}
		}
	}
	return true;
}
#endif

#if WITH_EDITORONLY_DATA
bool FStreamingWaveData::FinishDDCRequests()
{
	bool bRequestFailed = false;
	if (PendingAsyncStreamDerivedChunkTasks.Num())
	{
		for (int32 TaskIndex = 0; TaskIndex < PendingAsyncStreamDerivedChunkTasks.Num(); ++TaskIndex)
		{
			FAsyncStreamDerivedChunkTask& Task = PendingAsyncStreamDerivedChunkTasks[TaskIndex];
			Task.EnsureCompletion();
			bRequestFailed |= Task.GetTask().DidRequestFail();
		}
		PendingAsyncStreamDerivedChunkTasks.Empty();
	}
	return bRequestFailed;
}
#endif //WITH_EDITORONLY_DATA

FLoadedAudioChunk* FStreamingWaveData::AddNewLoadedChunk(int32 ChunkSize)
{
	int32 NewIndex = LoadedChunks.Num();
	LoadedChunks.AddDefaulted();
#if !USE_NEW_ASYNC_IO
	LoadedChunks[NewIndex].MemorySize = ChunkSize;
	LoadedChunks[NewIndex].Data = static_cast<uint8*>(FMemory::Malloc(ChunkSize));
	INC_DWORD_STAT_BY(STAT_AudioMemorySize, ChunkSize);
	INC_DWORD_STAT_BY(STAT_AudioMemory, ChunkSize);
#endif
	LoadedChunks[NewIndex].DataSize = ChunkSize;

	return &LoadedChunks[NewIndex];
}

void FStreamingWaveData::FreeLoadedChunk(FLoadedAudioChunk& LoadedChunk)
{
#if USE_NEW_ASYNC_IO
	if (LoadedChunk.IORequest)
	{
		LoadedChunk.IORequest->Cancel();
		LoadedChunk.IORequest->WaitCompletion();
		delete LoadedChunk.IORequest;
		LoadedChunk.IORequest = nullptr;
	}
#endif
	if (LoadedChunk.Data != NULL)
	{
		FMemory::Free(LoadedChunk.Data);

		// Stat housekeeping
#if USE_NEW_ASYNC_IO
		DEC_DWORD_STAT_BY(STAT_AudioMemorySize, LoadedChunk.DataSize);
		DEC_DWORD_STAT_BY(STAT_AudioMemory, LoadedChunk.DataSize);
#else
		DEC_DWORD_STAT_BY(STAT_AudioMemorySize, LoadedChunk.MemorySize);
		DEC_DWORD_STAT_BY(STAT_AudioMemory, LoadedChunk.MemorySize);
#endif

	}
	LoadedChunk.Data = NULL;
	LoadedChunk.DataSize = 0;
#if !USE_NEW_ASYNC_IO
	LoadedChunk.MemorySize = 0;
#endif
	LoadedChunk.Index = 0;
}

////////////////////////////
// FAudioStreamingManager //
////////////////////////////

FAudioStreamingManager::FAudioStreamingManager()
{
}

FAudioStreamingManager::~FAudioStreamingManager()
{
}

void FAudioStreamingManager::UpdateResourceStreaming(float DeltaTime, bool bProcessEverything /*= false*/)
{
	for (auto& WavePair : StreamingSoundWaves)
	{
		WavePair.Value->UpdateStreamingStatus();
	}

	for (auto Source : StreamingSoundSources)
	{
		const FWaveInstance* WaveInstance = Source->GetWaveInstance();
		USoundWave* Wave = WaveInstance ? WaveInstance->WaveData : nullptr;
		if (Wave)
		{
			FStreamingWaveData** WaveDataPtr = StreamingSoundWaves.Find(Wave);
	
			if (WaveDataPtr && (*WaveDataPtr)->PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests)
			{
				FStreamingWaveData* WaveData = *WaveDataPtr;
				// Request the chunk the source is using and the one after that
				FWaveRequest& WaveRequest = GetWaveRequest(Wave);
				int32 SourceChunk = Source->GetBuffer()->GetCurrentChunkIndex();
				if (SourceChunk >= 0 && SourceChunk < Wave->RunningPlatformData->NumChunks)
				{
					WaveRequest.RequiredIndices.AddUnique(SourceChunk);
					WaveRequest.RequiredIndices.AddUnique((SourceChunk + 1) % Wave->RunningPlatformData->NumChunks);
					if (!WaveData->LoadedChunkIndices.Contains(SourceChunk)
					|| Source->GetBuffer()->GetCurrentChunkOffset() > Wave->RunningPlatformData->Chunks[SourceChunk].DataSize / 2)
					{
						// currently not loaded or already read over half, request is high priority
						WaveRequest.bPrioritiseRequest = true;
					}
				}
				else
				{
					UE_LOG(LogAudio, Log, TEXT("Invalid chunk request curIndex=%d numChunks=%d\n"), SourceChunk, Wave->RunningPlatformData->NumChunks);
				}
			}
		}
	}
	for (auto Iter = WaveRequests.CreateIterator(); Iter; ++Iter)
	{
		USoundWave* Wave = Iter.Key();
		FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(Wave);

		if (WaveData && WaveData->PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests)
		{
			WaveData->UpdateChunkRequests(Iter.Value());
			WaveData->UpdateStreamingStatus();
			Iter.RemoveCurrent();
		}
	}
}

int32 FAudioStreamingManager::BlockTillAllRequestsFinished(float TimeLimit, bool)
{
#if USE_NEW_ASYNC_IO
	{
		QUICK_SCOPE_CYCLE_COUNTER(FAudioStreamingManager_BlockTillAllRequestsFinished);
		if (TimeLimit == 0.0f)
		{
			for (auto& WavePair : StreamingSoundWaves)
			{
				WavePair.Value->BlockTillAllRequestsFinished();
			}
		}
		else
		{
			double EndTime = FPlatformTime::Seconds() + TimeLimit;
			for (auto& WavePair : StreamingSoundWaves)
			{
				float ThisTimeLimit = EndTime - FPlatformTime::Seconds();
				if (ThisTimeLimit < .001f || // one ms is the granularity of the platform event system
					!WavePair.Value->BlockTillAllRequestsFinished(ThisTimeLimit))
				{
					return 1; // we don't report the actual number, just 1 for any number of outstanding requests
				}
			}
		}
	}

#endif
	// Not sure yet whether this will work the same as textures - aside from just before destroying
	return 0;
}

void FAudioStreamingManager::CancelForcedResources()
{
}

void FAudioStreamingManager::NotifyLevelChange()
{
}

void FAudioStreamingManager::SetDisregardWorldResourcesForFrames(int32 NumFrames)
{
}

void FAudioStreamingManager::AddLevel(class ULevel* Level)
{
}

void FAudioStreamingManager::RemoveLevel(class ULevel* Level)
{
}

void FAudioStreamingManager::AddStreamingSoundWave(USoundWave* SoundWave)
{
	if (FPlatformProperties::SupportsAudioStreaming() && SoundWave->IsStreaming()
	&& StreamingSoundWaves.FindRef(SoundWave) == NULL)
	{
		FStreamingWaveData& WaveData = *StreamingSoundWaves.Add(SoundWave, new FStreamingWaveData);
		WaveData.Initialize(SoundWave);
	}
}

void FAudioStreamingManager::RemoveStreamingSoundWave(USoundWave* SoundWave)
{
	FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave);
	if (WaveData)
	{
		StreamingSoundWaves.Remove(SoundWave);
		delete WaveData;
	}
	WaveRequests.Remove(SoundWave);
}

bool FAudioStreamingManager::IsManagedStreamingSoundWave(const USoundWave* SoundWave) const
{
	return StreamingSoundWaves.FindRef(SoundWave) != NULL;
}

bool FAudioStreamingManager::IsStreamingInProgress(const USoundWave* SoundWave)
{
	FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave);
	if (WaveData)
	{
		return WaveData->UpdateStreamingStatus();
	}
	return false;
}

bool FAudioStreamingManager::CanCreateSoundSource(const FWaveInstance* WaveInstance) const
{
	if (WaveInstance && WaveInstance->IsStreaming())
	{
		int32 MaxStreams = GetDefault<UAudioSettings>()->MaximumConcurrentStreams;

		if ( StreamingSoundSources.Num() < MaxStreams )
		{
			return true;
		}
		else
		{
			for (int32 Index = 0; Index < StreamingSoundSources.Num(); ++Index)
			{
				const FSoundSource* ExistingSource = StreamingSoundSources[Index];
				const FWaveInstance* ExistingWaveInst = ExistingSource->GetWaveInstance();
				if (!ExistingWaveInst || !ExistingWaveInst->WaveData
					|| ExistingWaveInst->WaveData->StreamingPriority < WaveInstance->WaveData->StreamingPriority)
				{
					return Index < MaxStreams;
				}
			}

			return false;
		}
	}
	return true;
}

void FAudioStreamingManager::AddStreamingSoundSource(FSoundSource* SoundSource)
{
	const FWaveInstance* WaveInstance = SoundSource->GetWaveInstance();
	if (WaveInstance && WaveInstance->IsStreaming())
	{
		int32 MaxStreams = GetDefault<UAudioSettings>()->MaximumConcurrentStreams;

		// Add source sorted by priority so we can easily iterate over the amount of streams
		// that are allowed
		int32 OrderedIndex = -1;
		for (int32 Index = 0; Index < StreamingSoundSources.Num() && Index < MaxStreams; ++Index)
		{
			const FSoundSource* ExistingSource = StreamingSoundSources[Index];
			const FWaveInstance* ExistingWaveInst = ExistingSource->GetWaveInstance();
			if (!ExistingWaveInst || !ExistingWaveInst->WaveData
				|| ExistingWaveInst->WaveData->StreamingPriority < WaveInstance->WaveData->StreamingPriority)
			{
				OrderedIndex = Index;
				break;
			}
		}
		if (OrderedIndex != -1)
		{
			StreamingSoundSources.Insert(SoundSource, OrderedIndex);
		}
		else if (StreamingSoundSources.Num() < MaxStreams)
		{
			StreamingSoundSources.AddUnique(SoundSource);
		}

		for (int32 Index = StreamingSoundSources.Num()-1; Index >= MaxStreams; --Index)
		{
			StreamingSoundSources[Index]->Stop();
		}
	}
}

void FAudioStreamingManager::RemoveStreamingSoundSource(FSoundSource* SoundSource)
{
	const FWaveInstance* WaveInstance = SoundSource->GetWaveInstance();
	if (WaveInstance && WaveInstance->WaveData && WaveInstance->WaveData->IsStreaming())
	{
		// Make sure there is a request so that unused chunks
		// can be cleared if this was the last playing instance
		GetWaveRequest(WaveInstance->WaveData);
		StreamingSoundSources.Remove(SoundSource);
	}
}

bool FAudioStreamingManager::IsManagedStreamingSoundSource(const FSoundSource* SoundSource) const
{
	return StreamingSoundSources.FindByKey(SoundSource) != NULL;
}

const uint8* FAudioStreamingManager::GetLoadedChunk(const USoundWave* SoundWave, uint32 ChunkIndex, uint32* OutChunkSize) const
{
	const FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave);
	if (WaveData)
	{
		if (WaveData->LoadedChunkIndices.Contains(ChunkIndex))
		{
			for (int32 Index = 0; Index < WaveData->LoadedChunks.Num(); ++Index)
			{
				if (WaveData->LoadedChunks[Index].Index == ChunkIndex)
				{
					if(OutChunkSize != NULL)
					{
						*OutChunkSize = WaveData->LoadedChunks[Index].DataSize;
					}
					
					return WaveData->LoadedChunks[Index].Data;
				}
			}
		}
	}
	return NULL;
}

FWaveRequest& FAudioStreamingManager::GetWaveRequest(USoundWave* SoundWave)
{
	FWaveRequest* WaveRequest = WaveRequests.Find(SoundWave);
	if (!WaveRequest)
	{
		// Setup the new request so it always asks for chunk 0
		WaveRequest = &WaveRequests.Add(SoundWave);
		WaveRequest->RequiredIndices.AddUnique(0);
		WaveRequest->bPrioritiseRequest = false;
	}
	return *WaveRequest;
}
