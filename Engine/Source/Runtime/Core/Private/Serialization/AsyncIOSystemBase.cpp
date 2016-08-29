// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncIOSystemBase.h: Base implementation of the async IO system
=============================================================================*/

#include "CorePrivatePCH.h"
#include "Serialization/AsyncIOSystemBase.h"

DECLARE_STATS_GROUP_VERBOSE(TEXT("AsyncIOSystem"),STATGROUP_AsyncIO_Verbose, STATCAT_Advanced);

DECLARE_CYCLE_STAT( TEXT( "Platform read time" ), STAT_AsyncIO_PlatformReadTime, STATGROUP_AsyncIO );

DECLARE_DWORD_ACCUMULATOR_STAT( TEXT( "Fulfilled read count" ), STAT_AsyncIO_FulfilledReadCount, STATGROUP_AsyncIO );
DECLARE_DWORD_ACCUMULATOR_STAT( TEXT( "Canceled read count" ), STAT_AsyncIO_CanceledReadCount, STATGROUP_AsyncIO );

DECLARE_MEMORY_STAT( TEXT( "Fulfilled read size" ), STAT_AsyncIO_FulfilledReadSize, STATGROUP_AsyncIO );
DECLARE_MEMORY_STAT( TEXT( "Canceled read size" ), STAT_AsyncIO_CanceledReadSize, STATGROUP_AsyncIO );
DECLARE_MEMORY_STAT( TEXT( "Outstanding read size" ), STAT_AsyncIO_OutstandingReadSize, STATGROUP_AsyncIO );

DECLARE_DWORD_ACCUMULATOR_STAT( TEXT( "Outstanding read count" ), STAT_AsyncIO_OutstandingReadCount, STATGROUP_AsyncIO );
DECLARE_FLOAT_ACCUMULATOR_STAT( TEXT( "Uncompressor wait time" ), STAT_AsyncIO_UncompressorWaitTime, STATGROUP_AsyncIO );

DECLARE_FLOAT_COUNTER_STAT( TEXT( "Bandwidth (MByte/ sec)" ), STAT_AsyncIO_Bandwidth, STATGROUP_AsyncIO );

/*-----------------------------------------------------------------------------
	FAsyncIOSystemBase implementation.
-----------------------------------------------------------------------------*/

#define BLOCK_ON_ASYNCIO 0

// Constrain bandwidth if wanted. Value is in MByte/ sec.
float GAsyncIOBandwidthLimit = 0.0f;
static FAutoConsoleVariableRef CVarAsyncIOBandwidthLimit(
	TEXT("s.AsyncIOBandwidthLimit"),
	GAsyncIOBandwidthLimit,
	TEXT("Constrain bandwidth if wanted. Value is in MByte/ sec."),
	ECVF_Default
	);

CORE_API bool GbLogAsyncLoading = false;
CORE_API bool GbLogAsyncTiming = false;

uint64 FAsyncIOSystemBase::QueueIORequest( 
	const FString& FileName, 
	int64 Offset, 
	int64 Size, 
	int64 UncompressedSize, 
	void* Dest, 
	ECompressionFlags CompressionFlags, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	check(Offset != INDEX_NONE);
	check(Dest != nullptr || Size == 0);

	static bool HasCheckedCommandline = false;
	if (!HasCheckedCommandline)
	{
		HasCheckedCommandline = true;
		if ( FParse::Param(FCommandLine::Get(), TEXT("logasynctiming")))
		{
#if !UE_BUILD_SHIPPING
			GbLogAsyncTiming = true;
			UE_LOG(LogStreaming, Warning, TEXT("*** ASYNC TIMING IS ENABLED"));
#endif
		}
		else if ( FParse::Param(FCommandLine::Get(), TEXT("logasync")))
		{
			GbLogAsyncLoading = true;
			UE_LOG(LogStreaming, Warning, TEXT("*** ASYNC LOGGING IS ENABLED"));
		}
	}

	FScopeLock ScopeLock(CriticalSection);

	// Create an IO request containing passed in information.
	FAsyncIORequest IORequest;
	IORequest.RequestIndex				= RequestIndex++;
	IORequest.FileSortKey				= INDEX_NONE;
	IORequest.FileName					= FileName;
	IORequest.FileNameHash				= FCrc::StrCrc32<TCHAR>(*FileName.ToLower());
	IORequest.Offset					= Offset;
	IORequest.Size						= Size;
	IORequest.UncompressedSize			= UncompressedSize;
	IORequest.Dest						= Dest;
	IORequest.CompressionFlags			= CompressionFlags;
	IORequest.Counter					= Counter;
	IORequest.Priority					= Priority;
#if !UE_BUILD_SHIPPING
	IORequest.TimeOfRequest				= GbLogAsyncTiming ? FPlatformTime::Seconds() : 0;
#endif
	if (GbLogAsyncLoading == true)
	{
		LogIORequest(TEXT("QueueIORequest"), IORequest);
	}

	INC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
	INC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );

	// Add to end of queue.
	OutstandingRequests.Add( IORequest );

	// Trigger event telling IO thread to wake up to perform work.
	OutstandingRequestsEvent->Trigger();

	// Return unique ID associated with request which can be used to cancel it.
	return IORequest.RequestIndex;
}

uint64 FAsyncIOSystemBase::QueueDestroyHandleRequest(const FString& FileName)
{
	FScopeLock ScopeLock( CriticalSection );
	FAsyncIORequest IORequest;
	IORequest.RequestIndex				= RequestIndex++;
	IORequest.FileName					= FileName;
	IORequest.FileNameHash				= FCrc::StrCrc32<TCHAR>(*FileName.ToLower());
	IORequest.Priority					= AIOP_MIN;
	IORequest.bIsDestroyHandleRequest	= true;

	if (GbLogAsyncLoading == true)
	{
		LogIORequest(TEXT("QueueDestroyHandleRequest"), IORequest);
	}

	// Add to end of queue.
	OutstandingRequests.Add( IORequest );

	// Trigger event telling IO thread to wake up to perform work.
	OutstandingRequestsEvent->Trigger();

	// Return unique ID associated with request which can be used to cancel it.
	return IORequest.RequestIndex;
}

void FAsyncIOSystemBase::LogIORequest(const FString& Message, const FAsyncIORequest& IORequest)
{
	// When logging timings, ignore destroy handle requests.
	if (!GbLogAsyncTiming || !IORequest.bIsDestroyHandleRequest)
	{
		FString OutputStr = FString::Printf(TEXT("ASYNC: %32s: %s\n"), *Message, *(IORequest.ToString(GbLogAsyncTiming)));
		FPlatformMisc::LowLevelOutputDebugString(*OutputStr);
	}
}

void FAsyncIOSystemBase::InternalRead( IFileHandle* FileHandle, int64 Offset, int64 Size, void* Dest )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FAsyncIOSystemBase::InternalRead"), STAT_AsyncIOSystemBase_InternalRead, STATGROUP_AsyncIO_Verbose);

	FScopeLock ScopeLock( ExclusiveReadCriticalSection );

	STAT(double ReadTime = 0);
	{	
		SCOPE_SECONDS_COUNTER(ReadTime);
		PlatformReadDoNotCallDirectly( FileHandle, Offset, Size, Dest );
	}	
	INC_FLOAT_STAT_BY(STAT_AsyncIO_PlatformReadTime,(float)ReadTime);

	// The platform might actually read more than Size due to aligning and internal min read sizes
	// though we only really care about throttling requested bandwidth as it's not very accurate
	// to begin with.
	STAT(ConstrainBandwidth(Size, ReadTime));
}

bool FAsyncIOSystemBase::PlatformReadDoNotCallDirectly( IFileHandle* FileHandle, int64 Offset, int64 Size, void* Dest )
{
	check(FileHandle);
	if (!FileHandle->Seek(Offset))
	{
		UE_LOG(LogStreaming, Error,TEXT("Seek failure."));
		return false;
	}
	if (!FileHandle->Read((uint8*)Dest, Size))
	{
		UE_LOG(LogStreaming, Error,TEXT("Read failure."));
		return false;
	}
	return true;
}

IFileHandle* FAsyncIOSystemBase::PlatformCreateHandle( const TCHAR* FileName )
{
	return LowLevel.OpenRead(FileName);
}


int32 FAsyncIOSystemBase::PlatformGetNextRequestIndex()
{
	// Find first index of highest priority request level. Basically FIFO per priority.
	int32 HighestPriorityIndex = INDEX_NONE;
	EAsyncIOPriority HighestPriority = static_cast<EAsyncIOPriority>(AIOP_MIN - 1);
	for( int32 CurrentRequestIndex=0; CurrentRequestIndex<OutstandingRequests.Num(); CurrentRequestIndex++ )
	{
		// Calling code already entered critical section so we can access OutstandingRequests.
		const FAsyncIORequest& IORequest = OutstandingRequests[CurrentRequestIndex];
		if( IORequest.Priority > HighestPriority )
		{
			HighestPriority = IORequest.Priority;
			HighestPriorityIndex = CurrentRequestIndex;
		}
	}
	return HighestPriorityIndex;
}

void FAsyncIOSystemBase::PlatformHandleHintDoneWithFile(const FString& Filename)
{
	QueueDestroyHandleRequest(Filename);
}

int64 FAsyncIOSystemBase::PlatformMinimumReadSize()
{
	return 32*1024;
}



// If enabled allows tracking down crashes in decompression as it avoids using the async work queue.
#define BLOCK_ON_DECOMPRESSION 0

void FAsyncIOSystemBase::FulfillCompressedRead( const FAsyncIORequest& IORequest, IFileHandle* FileHandle )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FAsyncIOSystemBase::FulfillCompressedRead"), STAT_AsyncIOSystemBase_FulfillCompressedRead, STATGROUP_AsyncIO_Verbose);

	if (GbLogAsyncLoading == true)
	{
		LogIORequest(TEXT("FulfillCompressedRead"), IORequest);
	}

	// Initialize variables.
	FAsyncUncompress*		Uncompressor			= NULL;
	uint8*					UncompressedBuffer		= (uint8*) IORequest.Dest;
	// First compression chunk contains information about total size so we skip that one.
	int32						CurrentChunkIndex		= 1;
	int32						CurrentBufferIndex		= 0;
	bool					bHasProcessedAllData	= false;

	// read the first two ints, which will contain the magic bytes (to detect byteswapping)
	// and the original size the chunks were compressed from
	int64						HeaderData[2];
	int32						HeaderSize = sizeof(HeaderData);

	InternalRead(FileHandle, IORequest.Offset, HeaderSize, HeaderData);
	RETURN_IF_EXIT_REQUESTED;

	// if the magic bytes don't match, then we are byteswapped (or corrupted)
	bool bIsByteswapped = HeaderData[0] != PACKAGE_FILE_TAG;
	// if its potentially byteswapped, make sure it's not just corrupted
	if (bIsByteswapped)
	{
		// if it doesn't equal the swapped version, then data is corrupted
		if (HeaderData[0] != PACKAGE_FILE_TAG_SWAPPED)
		{
			UE_LOG(LogStreaming, Warning, TEXT("Detected data corruption [header] trying to read %lld bytes at offset %lld from '%s'. Please delete file and recook."),
				IORequest.UncompressedSize, 
				IORequest.Offset ,
				*IORequest.FileName );
			check(0);
			FPlatformMisc::HandleIOFailure(*IORequest.FileName);
		}
		// otherwise, we have a valid byteswapped file, so swap the chunk size
		else
		{
			HeaderData[1] = BYTESWAP_ORDER64(HeaderData[1]);
		}
	}

	int32						CompressionChunkSize	= HeaderData[1];
	
	// handle old packages that don't have the chunk size in the header, in which case
	// we can use the old hardcoded size
	if (CompressionChunkSize == PACKAGE_FILE_TAG)
	{
		CompressionChunkSize = LOADING_COMPRESSION_CHUNK_SIZE;
	}

	// calculate the number of chunks based on the size they were compressed from
	int32						TotalChunkCount = (IORequest.UncompressedSize + CompressionChunkSize - 1) / CompressionChunkSize + 1;

	// allocate chunk info data based on number of chunks
	FCompressedChunkInfo*	CompressionChunks		= (FCompressedChunkInfo*)FMemory::Malloc(sizeof(FCompressedChunkInfo) * TotalChunkCount);
	int32						ChunkInfoSize			= (TotalChunkCount) * sizeof(FCompressedChunkInfo);
	void*					CompressedBuffer[2]		= { 0, 0 };
	
	// Read table of compression chunks after seeking to offset (after the initial header data)
	InternalRead( FileHandle, IORequest.Offset + HeaderSize, ChunkInfoSize, CompressionChunks );
	RETURN_IF_EXIT_REQUESTED;

	// Handle byte swapping. This is required for opening a cooked file on the PC.
	int64 CalculatedUncompressedSize = 0;
	if (bIsByteswapped)
	{
		for( int32 ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			CompressionChunks[ChunkIndex].CompressedSize	= BYTESWAP_ORDER64(CompressionChunks[ChunkIndex].CompressedSize);
			CompressionChunks[ChunkIndex].UncompressedSize	= BYTESWAP_ORDER64(CompressionChunks[ChunkIndex].UncompressedSize);
			if (ChunkIndex > 0)
			{
				CalculatedUncompressedSize += CompressionChunks[ChunkIndex].UncompressedSize;
			}
		}
	}
	else
	{
		for( int32 ChunkIndex=1; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			CalculatedUncompressedSize += CompressionChunks[ChunkIndex].UncompressedSize;
		}
	}

	if (CompressionChunks[0].UncompressedSize != CalculatedUncompressedSize)
	{
		UE_LOG(LogStreaming, Warning, TEXT("Detected data corruption [incorrect uncompressed size] calculated %i bytes, requested %i bytes at offset %i from '%s'. Please delete file and recook."),
			CalculatedUncompressedSize,
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		FPlatformMisc::HandleIOFailure(*IORequest.FileName);
	}

	if (ChunkInfoSize + HeaderSize + CompressionChunks[0].CompressedSize > IORequest.Size )
	{
		UE_LOG(LogStreaming, Warning, TEXT("Detected data corruption [undershoot] trying to read %lld bytes at offset %lld from '%s'. Please delete file and recook."),
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		FPlatformMisc::HandleIOFailure(*IORequest.FileName);
	}

	if (IORequest.UncompressedSize != CalculatedUncompressedSize)
	{
		UE_LOG(LogStreaming, Warning, TEXT("Detected data corruption [incorrect uncompressed size] calculated %lld bytes, requested %lld bytes at offset %lld from '%s'. Please delete file and recook."),
			CalculatedUncompressedSize,
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		FPlatformMisc::HandleIOFailure(*IORequest.FileName);
	}

	// Figure out maximum size of compressed data chunk.
	int64 MaxCompressedSize = 0;
	for (int32 ChunkIndex = 1; ChunkIndex < TotalChunkCount; ChunkIndex++)
	{
		MaxCompressedSize = FMath::Max(MaxCompressedSize, CompressionChunks[ChunkIndex].CompressedSize);
		// Verify the all chunks are 'full size' until the last one...
		if (CompressionChunks[ChunkIndex].UncompressedSize < CompressionChunkSize)
		{
			if (ChunkIndex != (TotalChunkCount - 1))
			{
				checkf(0, TEXT("Calculated too many chunks: %d should be last, there are %d from '%s'"), ChunkIndex, TotalChunkCount, *IORequest.FileName);
			}
		}
		check( CompressionChunks[ChunkIndex].UncompressedSize <= CompressionChunkSize );
	}

	int32 Padding = 0;

	// Allocate memory for compressed data.
	CompressedBuffer[0]	= FMemory::Malloc( MaxCompressedSize + Padding );
	CompressedBuffer[1] = FMemory::Malloc( MaxCompressedSize + Padding );

	// Initial read request.
	InternalRead( FileHandle, FileHandle->Tell(), CompressionChunks[CurrentChunkIndex].CompressedSize, CompressedBuffer[CurrentBufferIndex] );
	RETURN_IF_EXIT_REQUESTED;

	// Loop till we're done decompressing all data.
	while( !bHasProcessedAllData )
	{
		FAsyncTask<FAsyncUncompress> UncompressTask(
			IORequest.CompressionFlags,
			UncompressedBuffer,
			CompressionChunks[CurrentChunkIndex].UncompressedSize,
			CompressedBuffer[CurrentBufferIndex],
			CompressionChunks[CurrentChunkIndex].CompressedSize,
			(Padding > 0)
			);

#if BLOCK_ON_DECOMPRESSION
		UncompressTask.StartSynchronousTask();
#else
		UncompressTask.StartBackgroundTask();
#endif

		// Advance destination pointer.
		UncompressedBuffer += CompressionChunks[CurrentChunkIndex].UncompressedSize;
	
		// Check whether we are already done reading.
		if( CurrentChunkIndex < TotalChunkCount-1 )
		{
			// Can't postincrement in if statement as we need it to remain at valid value for one more loop iteration to finish
		// the decompression.
			CurrentChunkIndex++;
			// Swap compression buffers to read into.
			CurrentBufferIndex = 1 - CurrentBufferIndex;
			// Read more data.
			InternalRead( FileHandle, FileHandle->Tell(), CompressionChunks[CurrentChunkIndex].CompressedSize, CompressedBuffer[CurrentBufferIndex] );
			RETURN_IF_EXIT_REQUESTED;
		}
		// We were already done reading the last time around so we are done processing now.
		else
		{
			bHasProcessedAllData = true;
		}
		
		//@todo async loading: should use event for this
		STAT(double UncompressorWaitTime = 0);
		{
			SCOPE_SECONDS_COUNTER(UncompressorWaitTime);
			UncompressTask.EnsureCompletion(); // just decompress on this thread if it isn't started yet
		}
		INC_FLOAT_STAT_BY(STAT_AsyncIO_UncompressorWaitTime,(float)UncompressorWaitTime);
	}

	FMemory::Free(CompressionChunks);
	FMemory::Free(CompressedBuffer[0]);
	FMemory::Free(CompressedBuffer[1] );
}

IFileHandle* FAsyncIOSystemBase::GetCachedFileHandle( const FString& FileName )
{
	// We can't make any assumptions about NULL being an invalid handle value so we need to use the indirection.
	IFileHandle*	FileHandle = FindCachedFileHandle( FileName );

	// We have an already cached handle, let's use it.
	if( !FileHandle )
	{
		// So let the platform specific code create one.
		FileHandle = PlatformCreateHandle( *FileName );
		// Make sure it's valid before caching and using it.
		if( FileHandle )
		{
			NameHashToHandleMap.Add(FCrc::StrCrc32<TCHAR>(*FileName.ToLower()), FileHandle);
		}
	}

	return FileHandle;
}

IFileHandle* FAsyncIOSystemBase::FindCachedFileHandle( const FString& FileName )
{
	return FindCachedFileHandle(FCrc::StrCrc32<TCHAR>(*FileName.ToLower()));
}

IFileHandle* FAsyncIOSystemBase::FindCachedFileHandle(const uint32 FileNameHash)
{
	return NameHashToHandleMap.FindRef(FileNameHash);
}

uint64 FAsyncIOSystemBase::LoadData( 
	const FString& FileName, 
	int64 Offset, 
	int64 Size, 
	void* Dest, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	uint64 TheRequestIndex;
	{
		TheRequestIndex = QueueIORequest( FileName, Offset, Size, 0, Dest, COMPRESS_None, Counter, Priority );
	}
#if BLOCK_ON_ASYNCIO
	BlockTillAllRequestsFinished(); 
#endif
	return TheRequestIndex;
}

uint64 FAsyncIOSystemBase::LoadCompressedData( 
	const FString& FileName, 
	int64 Offset, 
	int64 Size, 
	int64 UncompressedSize, 
	void* Dest, 
	ECompressionFlags CompressionFlags, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	uint64 TheRequestIndex;
	{
		TheRequestIndex = QueueIORequest( FileName, Offset, Size, UncompressedSize, Dest, CompressionFlags, Counter, Priority );
	}
#if BLOCK_ON_ASYNCIO
	BlockTillAllRequestsFinished(); 
#endif
	return TheRequestIndex;
}

int32 FAsyncIOSystemBase::CancelRequests( uint64* RequestIndices, int32 NumIndices )
{
	FScopeLock ScopeLock( CriticalSection );

	// Iterate over all outstanding requests and cancel matching ones.
	int32 RequestsCanceled = 0;
	for( int32 OutstandingIndex=OutstandingRequests.Num()-1; OutstandingIndex>=0 && RequestsCanceled<NumIndices; OutstandingIndex-- )
	{
		// Iterate over all indices of requests to cancel
		for( int32 TheRequestIndex=0; TheRequestIndex<NumIndices; TheRequestIndex++ )
		{
			// Look for matching request index in queue.
			FAsyncIORequest& IORequest = OutstandingRequests[OutstandingIndex];
			if (IORequest.RequestIndex == RequestIndices[TheRequestIndex] && !IORequest.bIsDestroyHandleRequest)
			{
				INC_DWORD_STAT( STAT_AsyncIO_CanceledReadCount );
				INC_DWORD_STAT_BY( STAT_AsyncIO_CanceledReadSize, IORequest.Size );
				DEC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
				DEC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );				
				// Decrement thread-safe counter to indicate that request has been "completed".
				if (IORequest.Counter)
				{
					IORequest.Counter->Decrement();
					IORequest.Counter = nullptr;
				}
				IORequest.bIsDestroyHandleRequest = true; // delete the file handle on the next tick (unless there is another request)
				RequestsCanceled++;
				break;
			}
		}
	}
	return RequestsCanceled;
}

void FAsyncIOSystemBase::CancelAllOutstandingRequests()
{
	FScopeLock ScopeLock( CriticalSection );

	// simply toss all outstanding requests - the critical section will guarantee we aren't removing
	// while using elsewhere
	OutstandingRequests.Empty();
}

void FAsyncIOSystemBase::ConstrainBandwidth( int64 BytesRead, float ReadTime )
{
	// Constrain bandwidth if wanted. Value is in MByte/ sec.
	if( GAsyncIOBandwidthLimit > 0.0f )
	{
		// Figure out how long to wait to throttle bandwidth.
		float WaitTime = BytesRead / (GAsyncIOBandwidthLimit * 1024.f * 1024.f) - ReadTime;
		// Only wait if there is something worth waiting for.
		if( WaitTime > 0 )
		{
			// Time in seconds to wait.
			FPlatformProcess::Sleep(WaitTime);
		}
	}
}

bool FAsyncIOSystemBase::Init()
{
	CriticalSection				= new FCriticalSection();
	ExclusiveReadCriticalSection= new FCriticalSection();
	OutstandingRequestsEvent	= FPlatformProcess::GetSynchEventFromPool();
	RequestIndex				= 1;
	MinPriority					= AIOP_MIN;
	IsRunning.Increment();
	return true;
}

void FAsyncIOSystemBase::Suspend()
{
	SuspendCount.Increment();
	ExclusiveReadCriticalSection->Lock();
}

void FAsyncIOSystemBase::Resume()
{
	ExclusiveReadCriticalSection->Unlock();
	SuspendCount.Decrement();
}

void FAsyncIOSystemBase::SetMinPriority( EAsyncIOPriority InMinPriority )
{
	FScopeLock ScopeLock( CriticalSection );
	// Trigger event telling IO thread to wake up to perform work if we are lowering the min priority.
	if( InMinPriority < MinPriority )
	{
		OutstandingRequestsEvent->Trigger();
	}
	// Update priority.
	MinPriority = InMinPriority;
}

void FAsyncIOSystemBase::HintDoneWithFile(const FString& Filename)
{
	// let the platform handle it
	PlatformHandleHintDoneWithFile(Filename);
}

int64 FAsyncIOSystemBase::MinimumReadSize()
{
	// let the platform handle it
	return PlatformMinimumReadSize();
}

/**
* Flush the pending logs if any.
*/
void FAsyncIOSystemBase::FlushLog()
{
	if (GbLogAsyncTiming)
	{
		// Move to a temp array to minimize how long the lock is active.
		TArray<FAsyncIORequest> ProcessedLogs;
		{
			FScopeLock ScopeLock(CriticalSection);
			FMemory::Memswap(&ProcessedRequestForLogs, &ProcessedLogs, sizeof(ProcessedLogs));
		}

		for (int32 i = 0; i < ProcessedLogs.Num(); ++i)
		{
			LogIORequest(TEXT("IOTiming"), ProcessedLogs[i]);
		}
	}
}

void FAsyncIOSystemBase::Exit()
{
	FlushHandles();
	delete CriticalSection;
	FPlatformProcess::ReturnSynchEventToPool(OutstandingRequestsEvent);
	OutstandingRequestsEvent = nullptr;
}

void FAsyncIOSystemBase::Stop()
{
	// Tell the thread to quit.
	IsRunning.Decrement();

	// Make sure that the thread awakens even if there is no work currently outstanding.
	OutstandingRequestsEvent->Trigger();
}

uint32 FAsyncIOSystemBase::Run()
{
	// IsRunning gets decremented by Stop.
	while( IsRunning.GetValue() > 0 )
	{
		// Sit and spin if requested, unless we are shutting down, in which case make sure we don't deadlock.
		while( !GIsRequestingExit && SuspendCount.GetValue() > 0 )
		{
			FPlatformProcess::Sleep(0.005);
		}

		Tick();
	}

	return 0;
}

void FAsyncIOSystemBase::Tick()
{
	// Create file handles.
	{
		TSet<FString> FileNamesToCacheHandles; 
		TSet<FString> FileNamesToUnCacheHandles;
		// Only enter critical section for copying existing array over. We don't operate on the 
		// real array as creating file handles might take a while and we don't want to have other
		// threads stalling on submission of requests.
		{
			FScopeLock ScopeLock( CriticalSection );

			if (OutstandingRequests.Num() > 1000 || NameHashToHandleMap.Num() > 1000)
			{
				static double LastTime = 0.0;
				if (FPlatformTime::Seconds() - LastTime > 1.0)
				{
					LastTime = FPlatformTime::Seconds();
					UE_LOG(LogStreaming, Error, TEXT("FAsyncIOSystemBase::Tick queue is overflowing %d requests and %d files."), OutstandingRequests.Num(), NameHashToHandleMap.Num());
				}
			}

			for( int32 RequestIdx=0; RequestIdx<OutstandingRequests.Num(); RequestIdx++ )
			{
				// Early outs avoid unnecessary work and string copies with implicit allocator churn.
				FAsyncIORequest& OutstandingRequest = OutstandingRequests[RequestIdx];
				if (OutstandingRequest.bIsDestroyHandleRequest)
				{
					FileNamesToUnCacheHandles.Emplace(*OutstandingRequest.FileName);
					OutstandingRequests.RemoveAt(RequestIdx--);
				}
				else if (!OutstandingRequest.bHasAlreadyRequestedHandleToBeCached)
				{
					FileNamesToCacheHandles.Emplace(*OutstandingRequest.FileName);
					OutstandingRequest.bHasAlreadyRequestedHandleToBeCached = true;
				}
			}
		}
		// Create file handles for requests down the pipe. This is done here so we can later on
		// use the handles to figure out the sort keys.
		for (auto& Filename : FileNamesToCacheHandles)
		{
			GetCachedFileHandle(Filename);
			FileNamesToUnCacheHandles.Remove(Filename);
		}
		for (auto& Filename : FileNamesToUnCacheHandles)
		{
			unsigned int Hash = FCrc::StrCrc32<TCHAR>(*Filename.ToLower());
			IFileHandle* FileHandle = FindCachedFileHandle(Hash);
			if (FileHandle)
			{
				// destroy and remove the handle
				delete FileHandle;
				NameHashToHandleMap.Remove(Hash);
			}
		}
	}

	int32 NumMergedRequests = 1;

	// Copy of request.
	FAsyncIORequest IORequest;
	bool bIsRequestPending	= false;
	{
		FScopeLock ScopeLock( CriticalSection );
		if( OutstandingRequests.Num() )
		{
			// Gets next request index based on platform specific criteria like layout on disc.
			int32 TheRequestIndex = PlatformGetNextRequestIndex();
			if( TheRequestIndex != INDEX_NONE )
			{					
				// We need to copy as we're going to remove it...
				IORequest = OutstandingRequests[ TheRequestIndex ];

#if !UE_BUILD_SHIPPING
				// Now check if the next request follow on disk and in memory
				// This is useful to clean the timing log, while not changing anything to timings.
				int32 NextRequestIndex = TheRequestIndex + 1;
				while (GbLogAsyncTiming)
				{
					if (OutstandingRequests.IsValidIndex(NextRequestIndex))
					{
						const FAsyncIORequest& NextIORequest = OutstandingRequests[NextRequestIndex];
						if (NextIORequest.FileNameHash == IORequest.FileNameHash && 
							NextIORequest.CompressionFlags == IORequest.CompressionFlags && 
							NextIORequest.Offset == IORequest.Offset + IORequest.Size &&
							(uint8*)NextIORequest.Dest == (uint8*)IORequest.Dest + IORequest.Size &&
							NextIORequest.Counter == IORequest.Counter)
						{
							IORequest.Size += NextIORequest.Size;
							++NextRequestIndex;
							++NumMergedRequests;
							continue;
						}
					}
					break;
				}
#endif
				// ...right here.
				// NOTE: this needs to be a Remove, not a RemoveSwap because the base implementation
				// of PlatformGetNextRequestIndex is a FIFO taking priority into account
				OutstandingRequests.RemoveAt( TheRequestIndex, NumMergedRequests );		
				// We're busy. Updated inside scoped lock to ensure BlockTillAllRequestsFinished works correctly.
				BusyWithRequest.Increment();
				bIsRequestPending = true;
			}
		}
	}

	// We only have work to do if there's a request pending.
	if( bIsRequestPending )
	{
		double CurrentTime = GbLogAsyncTiming ? FPlatformTime::Seconds() : 0;

		// handle a destroy handle request from the queue
		if( IORequest.bIsDestroyHandleRequest )
		{
			IFileHandle*	FileHandle = FindCachedFileHandle( IORequest.FileNameHash );
			if( FileHandle )
			{
				// destroy and remove the handle
				delete FileHandle;
				NameHashToHandleMap.Remove(IORequest.FileNameHash);
			}
		}
		else
		{
			// Retrieve cached handle or create it if it wasn't cached. We purposefully don't look at currently
			// set value as it might be stale by now.
			IFileHandle* FileHandle = GetCachedFileHandle( IORequest.FileName );
			if( FileHandle )
			{
				if( IORequest.UncompressedSize )
				{
					// Data is compressed on disc so we need to also decompress.
					FulfillCompressedRead( IORequest, FileHandle );
				}
				else
				{
					// Read data after seeking.
					InternalRead( FileHandle, IORequest.Offset, IORequest.Size, IORequest.Dest );
				}
				INC_DWORD_STAT( STAT_AsyncIO_FulfilledReadCount );
				INC_DWORD_STAT_BY( STAT_AsyncIO_FulfilledReadSize, IORequest.Size );
			}
			else
			{
				//@todo streaming: add warning once we have thread safe logging.
			}

			DEC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
			DEC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );
		}

		// Request fulfilled.
		if( IORequest.Counter )
		{
			IORequest.Counter->Subtract(NumMergedRequests); 
		}
		// We're done reading for now.
		BusyWithRequest.Decrement();	

#if !UE_BUILD_SHIPPING
		if (GbLogAsyncTiming)
		{
			IORequest.WaitTime = CurrentTime - IORequest.TimeOfRequest;
			IORequest.ReadTime = FPlatformTime::Seconds() - CurrentTime;
			FScopeLock ScopeLock(CriticalSection);
			ProcessedRequestForLogs.Push(IORequest);
		}
#endif
	}
	else
	{
		if( !OutstandingRequests.Num() && FPlatformProcess::SupportsMultithreading() )
		{
			// We're really out of requests now, wait till the calling thread signals further work
			OutstandingRequestsEvent->Wait();
		}
	}
}

void FAsyncIOSystemBase::TickSingleThreaded()
{
	// This should only be used when multithreading is disabled.
	check(FPlatformProcess::SupportsMultithreading() == false);
	Tick();
}

void FAsyncIOSystemBase::BlockTillAllRequestsFinished()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FAsyncIOSystemBase::BlockTillAllRequestsFinished"), STAT_FAsyncIOSystemBase_BlockTillAllRequestsFinished, STATGROUP_AsyncIO_Verbose);

	// Block till all requests are fulfilled.
	while( true ) 
	{
		bool bHasFinishedRequests = false;
		{
			FScopeLock ScopeLock( CriticalSection );
			bHasFinishedRequests = (OutstandingRequests.Num() == 0) && (BusyWithRequest.GetValue() == 0);
		}	
		if( bHasFinishedRequests )
		{
			break;
		}
		else
		{
			SHUTDOWN_IF_EXIT_REQUESTED;

			//@todo streaming: this should be replaced by waiting for an event.
			FPlatformProcess::SleepNoStats( 0.001f );
		}
	}
}

void FAsyncIOSystemBase::BlockTillAllRequestsFinishedAndFlushHandles()
{
	// Block till all requests are fulfilled.
	BlockTillAllRequestsFinished();

	// Flush all file handles.
	FlushHandles();
}

void FAsyncIOSystemBase::FlushHandles()
{
	FScopeLock ScopeLock( CriticalSection );
	// Iterate over all file handles, destroy them and empty name to handle map.
	for (TMap<uint32, IFileHandle*>::TIterator It(NameHashToHandleMap); It; ++It)
	{
		delete It.Value();
	}
	NameHashToHandleMap.Empty();
}

/** Thread used for async IO manager */
static FRunnableThread*	AsyncIOThread = NULL;
static FAsyncIOSystemBase* AsyncIOSystem = NULL;

FIOSystem& FIOSystem::Get()
{
	if (!AsyncIOThread)
	{
		check(!AsyncIOSystem);
		GConfig->GetFloat( TEXT("Core.System"), TEXT("AsyncIOBandwidthLimit"), GAsyncIOBandwidthLimit, GEngineIni );
		AsyncIOSystem = FPlatformMisc::GetPlatformSpecificAsyncIOSystem();
		if (!AsyncIOSystem)
		{
			// the platform didn't have a specific need, so we just use the base class with the normal file system.
			AsyncIOSystem = new FAsyncIOSystemBase(FPlatformFileManager::Get().GetPlatformFile());
		}
		AsyncIOThread = FRunnableThread::Create(AsyncIOSystem, TEXT("AsyncIOSystem"), 16384, TPri_Highest, FPlatformAffinity::GetPoolThreadMask());
		check(AsyncIOThread);
	}
	check(AsyncIOSystem);
	return *AsyncIOSystem;
}

void FIOSystem::Shutdown()
{
	if (AsyncIOThread)
	{
		AsyncIOThread->Kill(true);
		delete AsyncIOThread;
	}
	if (AsyncIOSystem)
	{
		delete AsyncIOSystem;
		AsyncIOSystem = NULL;
	}
	AsyncIOThread = (FRunnableThread*)-1; // non null, we don't allow a restart after a shutdown as this is usually an error of some sort
}

bool FIOSystem::HasShutdown()
{
	return AsyncIOThread == nullptr || AsyncIOThread == (FRunnableThread*)-1;
}
