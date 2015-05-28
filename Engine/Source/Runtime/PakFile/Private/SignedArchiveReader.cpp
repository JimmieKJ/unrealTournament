// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PakFilePrivatePCH.h"
#include "IPlatformFilePak.h"
#include "SecureHash.h"
#include "FileManagerGeneric.h"
#include "ModuleManager.h"
#include "IPlatformFileModule.h"
#include "IOBase.h"
#include "BigInt.h"
#include "SignedArchiveReader.h"
#include "PublicKey.inl"

DECLARE_CYCLE_STAT( TEXT( "FChunkCacheWorker.ProcessQueue" ), STAT_FChunkCacheWorker_ProcessQueue, STATGROUP_PakFile );
DECLARE_CYCLE_STAT( TEXT( "FChunkCacheWorker.CheckSignature" ), STAT_FChunkCacheWorker_CheckSignature, STATGROUP_PakFile );
DECLARE_CYCLE_STAT( TEXT( "FSignedArchiveReader.Serialize" ), STAT_SignedArchiveReader_Serialize, STATGROUP_PakFile );

FChunkCacheWorker::FChunkCacheWorker(FArchive* InReader)
	: Reader(InReader)
	, QueuedRequestsEvent(NULL)
{
	DecryptionKey.Exponent.Parse(DECRYPTION_KEY_EXPONENT);
	DecryptionKey.Modulus.Parse(DECYRPTION_KEY_MODULUS);
	// Public key should never be zero at this point. Check PublicKey.inl for more details.
	check(!DecryptionKey.Exponent.IsZero() && !DecryptionKey.Modulus.IsZero());

	QueuedRequestsEvent = FPlatformProcess::GetSynchEventFromPool();
	Thread = FRunnableThread::Create(this, TEXT("FChunkCacheWorker"), 0, TPri_BelowNormal);
}

FChunkCacheWorker::~FChunkCacheWorker()
{
	delete Thread;
	Thread = NULL;
	FPlatformProcess::ReturnSynchEventToPool(QueuedRequestsEvent);
	QueuedRequestsEvent = nullptr;
}

bool FChunkCacheWorker::Init()
{
	return true;
}

uint32 FChunkCacheWorker::Run()
{
	while (StopTaskCounter.GetValue() == 0)
	{
		int32 ProcessedRequests = ProcessQueue();		
		if (ProcessedRequests == 0)
		{
			QueuedRequestsEvent->Wait(500);
		}
	}
	return 0;
}

void FChunkCacheWorker::Stop()
{
	StopTaskCounter.Increment();
}

FChunkBuffer* FChunkCacheWorker::GetCachedChunkBuffer(int32 ChunkIndex)
{
	for (int32 BufferIndex = 0; BufferIndex < MaxCachedChunks; ++BufferIndex)
	{
		if (CachedChunks[BufferIndex].ChunkIndex == ChunkIndex)
		{
			// Update access info and lock
			CachedChunks[BufferIndex].LockCount++;
			CachedChunks[BufferIndex].LastAccessTime = FPlatformTime::Seconds();
			return &CachedChunks[BufferIndex];
		}
	}
	return NULL;
}

FChunkBuffer* FChunkCacheWorker::GetFreeBuffer()
{
	// Find the least recently accessed, free buffer.
	FChunkBuffer* LeastRecentFreeBuffer = NULL;
	for (int32 BufferIndex = 0; BufferIndex < MaxCachedChunks; ++BufferIndex)
	{
		if (CachedChunks[BufferIndex].LockCount == 0 && 
			 (LeastRecentFreeBuffer == NULL || LeastRecentFreeBuffer->LastAccessTime > CachedChunks[BufferIndex].LastAccessTime))
		{
			LeastRecentFreeBuffer = &CachedChunks[BufferIndex];
		}
	}
	if (LeastRecentFreeBuffer)
	{
		// Update access info and lock
		LeastRecentFreeBuffer->LockCount++;
		LeastRecentFreeBuffer->LastAccessTime = FPlatformTime::Seconds();
	}
	return LeastRecentFreeBuffer;
}

void FChunkCacheWorker::ReleaseBuffer(int32 ChunkIndex)
{
	for (int32 BufferIndex = 0; BufferIndex < MaxCachedChunks; ++BufferIndex)
	{
		if (CachedChunks[BufferIndex].ChunkIndex == ChunkIndex)
		{
			CachedChunks[BufferIndex].LockCount--;
			check(CachedChunks[BufferIndex].LockCount >= 0);
		}
	}
}

int32 FChunkCacheWorker::ProcessQueue()
{
	SCOPE_CYCLE_COUNTER( STAT_FChunkCacheWorker_ProcessQueue );

	// Add the queue to the active requests list
	{
		FScopeLock LockQueue(&QueueLock);	
		ActiveRequests.Append(RequestQueue);
		RequestQueue.Empty();
	}

	// Keep track how many request have been process this loop
	int32 ProcessedRequests = ActiveRequests.Num();

	for (int32 RequestIndex = 0; RequestIndex < ActiveRequests.Num(); ++RequestIndex)
	{
		FChunkRequest& Request = *ActiveRequests[RequestIndex];
		if (Request.RefCount.GetValue() == 0)
		{
			// ChunkRequest is no longer used by anything. Add it to the free requests lists
			// and release the associated buffer.
			ReleaseBuffer(Request.Index);
			ActiveRequests.RemoveAt(RequestIndex--);
			FreeChunkRequests.Push(&Request);			
		}
		else if (Request.Buffer == NULL)
		{
			// See if the requested chunk is already cached.
			FChunkBuffer* CachedBuffer = GetCachedChunkBuffer(Request.Index);
			if (!CachedBuffer)
			{
				// This chunk is not cached. Get a free buffer if possible.
				CachedBuffer = GetFreeBuffer();				
				if (!!CachedBuffer)
				{
					// Load and verify.
					CachedBuffer->ChunkIndex = Request.Index;
					Request.Buffer = CachedBuffer;
					CheckSignature(Request);					
				}
			}
			else
			{
				Request.Buffer = CachedBuffer;
			}
			
			if (!!CachedBuffer)
			{
				check(Request.Buffer == CachedBuffer);
				// Chunk is cached and trusted. We no longer need the request handle on this thread.
				// Let the other thread know the chunk is ready to read.
				Request.RefCount.Decrement();				
				Request.IsTrusted.Increment();
			}
		}
	}
	return ProcessedRequests;
}

void FChunkCacheWorker::Decrypt(uint8* DecryptedData, const int256* Data, const int64 DataLength)
{
	for (int64 Index = 0; Index < DataLength; ++Index)
	{
		int256 DecryptedByte = FEncryption::ModularPow(Data[Index], DecryptionKey.Exponent, DecryptionKey.Modulus);
		DecryptedData[Index] = (uint8)DecryptedByte.ToInt();
	}
}

bool FChunkCacheWorker::CheckSignature(const FChunkRequest& ChunkInfo)
{
	SCOPE_CYCLE_COUNTER( STAT_FChunkCacheWorker_CheckSignature );

	FSignature Signature;
	Reader->Seek(ChunkInfo.Offset);
	Reader->Serialize(ChunkInfo.Buffer->Data, ChunkInfo.Size);
	Signature.Serialize(*Reader);

	// Hash data
	uint8 Hash[20];
	FSHA1::HashBuffer(ChunkInfo.Buffer->Data, ChunkInfo.Size, Hash);

	// Decrypt serialized hash
	uint8 DecryptedHash[20];
	Decrypt(DecryptedHash, Signature.Data, ARRAY_COUNT(DecryptedHash));

	// Compare hashes
	if (FMemory::Memcmp(Hash, DecryptedHash, sizeof(DecryptedHash)) != 0)
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Pak file has been tampered with."));
	}

	return true;
}

FChunkRequest& FChunkCacheWorker::RequestChunk(int32 ChunkIndex, int64 StartOffset, int64 ChunkSize)
{
	FChunkRequest* NewChunk = FreeChunkRequests.Pop();
	if (NewChunk == NULL)
	{
		NewChunk = new FChunkRequest();
	}
	NewChunk->Index = ChunkIndex;
	NewChunk->Offset = StartOffset;
	NewChunk->Size = ChunkSize;
	NewChunk->Buffer = NULL;
	NewChunk->IsTrusted.Set(0);
	// At this point both worker and the archive use this chunk so increase ref count
	NewChunk->RefCount.Set(2);

	QueueLock.Lock();
	RequestQueue.Add(NewChunk);
	QueueLock.Unlock();
	QueuedRequestsEvent->Trigger();
	return *NewChunk;
}

void FChunkCacheWorker::ReleaseChunk(FChunkRequest& Chunk)
{
	Chunk.RefCount.Decrement();
}

FSignedArchiveReader::FSignedArchiveReader(FArchive* InPakReader, FChunkCacheWorker* InSignatureChecker)
	: SignatureSize(FSignature::Size())
	, ChunkCount(0)
	, PakReader(InPakReader)
	, SizeOnDisk(0)
	, PakSize(0)
	, PakOffset(0)
	, SignatureChecker(InSignatureChecker)
{
	// Cache global info about the archive
	const int64 MaxChunkSize = FPakInfo::MaxChunkDataSize + SignatureSize;
	SizeOnDisk = PakReader->TotalSize();
	ChunkCount = SizeOnDisk / MaxChunkSize + ((SizeOnDisk % MaxChunkSize) ? 1 : 0);		
	PakSize = SizeOnDisk - ChunkCount * SignatureSize;
}

FSignedArchiveReader::~FSignedArchiveReader()
{
	delete PakReader;
	PakReader = NULL;
}

int64 FSignedArchiveReader::CalculateChunkSize(int64 ChunkIndex) const
{
	if (ChunkIndex == (ChunkCount - 1))
	{
		const int64 MaxChunkSize = FPakInfo::MaxChunkDataSize + SignatureSize;
		int64 Slack = SizeOnDisk % MaxChunkSize;
		if (!Slack)
		{
			return FPakInfo::MaxChunkDataSize;
		}
		else
		{
			Slack -= SignatureSize;
			check(Slack > 0);
			return Slack;
		}
	}
	else
	{
		return FPakInfo::MaxChunkDataSize;
	}
}

void FSignedArchiveReader::PrecacheChunks(TArray<FSignedArchiveReader::FReadInfo>& Chunks, int64 Length)
{
	// Request all the chunks that are needed to complete this read
	int64 DataOffset;
	int64 DestOffset = 0;
	int32 FirstChunkIndex = CalculateChunkIndex(PakOffset);
	int64 ChunkStartOffset = CalculateChunkOffset(PakOffset, DataOffset);
	int32 NumChunks = (DataOffset - ChunkStartOffset + Length) / FPakInfo::MaxChunkDataSize + 1;
	int64 RemainingLength = Length;
	int64 ArchiveOffset = PakOffset;

	// And then try to precache 'PrecacheLength' more chunks because it's likely
	// we're going to try to read them next
	if ((NumChunks + FirstChunkIndex + PrecacheLength - 1) < ChunkCount)
	{
		NumChunks += PrecacheLength;
	}
	Chunks.Empty(NumChunks);
	for (int32 ChunkIndexOffset = 0; ChunkIndexOffset < NumChunks; ++ChunkIndexOffset)
	{
		ChunkStartOffset = CalculateChunkOffset(ArchiveOffset, DataOffset);
		int64 SizeToReadFromBuffer = RemainingLength;
		if (DataOffset + SizeToReadFromBuffer > ChunkStartOffset + FPakInfo::MaxChunkDataSize)
		{
			SizeToReadFromBuffer = ChunkStartOffset + FPakInfo::MaxChunkDataSize - DataOffset;
		}

		FReadInfo ChunkInfo;
		ChunkInfo.SourceOffset = DataOffset - ChunkStartOffset;
		ChunkInfo.DestOffset = DestOffset;
		ChunkInfo.Size = SizeToReadFromBuffer;

		const int32 ChunkIndex = ChunkIndexOffset + FirstChunkIndex;							
		if (LastCachedChunk.ChunkIndex == ChunkIndex)
		{
			ChunkInfo.Request = NULL;
			ChunkInfo.PreCachedChunk = &LastCachedChunk;
		}
		else
		{
			const int64 ChunkSize = CalculateChunkSize(ChunkIndex);	
			ChunkInfo.Request = &SignatureChecker->RequestChunk(ChunkIndex, ChunkStartOffset, ChunkSize);
			ChunkInfo.PreCachedChunk = NULL;
		}

		Chunks.Add(ChunkInfo);

		ArchiveOffset += SizeToReadFromBuffer;
		DestOffset += SizeToReadFromBuffer;
		RemainingLength -= SizeToReadFromBuffer;
	}
}

void FSignedArchiveReader::Serialize(void* Data, int64 Length)
{
	SCOPE_CYCLE_COUNTER( STAT_SignedArchiveReader_Serialize );

	// First make sure the chunks we're going to read are actually cached.
	TArray<FReadInfo> QueuedChunks;
	PrecacheChunks(QueuedChunks, Length);

	// Read data from chunks.
	int64 RemainingLength = Length;
	uint8* DestData = (uint8*)Data;
	int32 ChunksToRead = QueuedChunks.Num() - PrecacheLength;	
	const int32 LastRequestIndex = ChunksToRead - 1;
	do
	{
		int32 ChunksReadThisLoop = 0;
		// Try to read cached chunks. If a chunk is not yet ready, skip to the next chunk - it's possible
		// that it has already been precached in one of the previous reads.
		for (int32 QueueIndex = 0; QueueIndex <= LastRequestIndex; ++QueueIndex)
		{
			FReadInfo& ChunkInfo = QueuedChunks[QueueIndex];
			if (ChunkInfo.Request && ChunkInfo.Request->IsReady())
			{
				// Read
				FMemory::Memcpy(DestData + ChunkInfo.DestOffset, ChunkInfo.Request->Buffer->Data + ChunkInfo.SourceOffset, ChunkInfo.Size);
				// Is this the last chunk? if so, copy it to pre-cache
				if (LastRequestIndex == QueueIndex && ChunkInfo.Request->Index != LastCachedChunk.ChunkIndex)
				{
					LastCachedChunk.ChunkIndex = ChunkInfo.Request->Index;
					FMemory::Memcpy(LastCachedChunk.Data, ChunkInfo.Request->Buffer->Data, FPakInfo::MaxChunkDataSize);
				}
				// Let the worker know we're done with this chunk for now.
				SignatureChecker->ReleaseChunk(*ChunkInfo.Request);
				ChunkInfo.Request = NULL;
				// One less chunk remaining
				ChunksToRead--;
				ChunksReadThisLoop++;
			}
			else if (ChunkInfo.PreCachedChunk)
			{
				// Copy directly from the pre-cached chunk.
				FMemory::Memcpy(DestData + ChunkInfo.DestOffset, ChunkInfo.PreCachedChunk->Data + ChunkInfo.SourceOffset, ChunkInfo.Size);
				ChunkInfo.PreCachedChunk = NULL;
				// One less chunk remaining
				ChunksToRead--;
				ChunksReadThisLoop++;
			}
		}
		if (ChunksReadThisLoop == 0)
		{
			// No chunks read, avoid tight spinning loops and give up some time to the other threads
			FPlatformProcess::Sleep(0.0f);
		}
	}
	while (ChunksToRead > 0);

	PakOffset += Length;

	// Free precached chunks (they will still get precached but simply marked as not used by anything)
	for (int32 QueueIndex = QueuedChunks.Num() - PrecacheLength; QueueIndex < QueuedChunks.Num(); ++QueueIndex)
	{
		FReadInfo& CachedChunk = QueuedChunks[QueueIndex];
		SignatureChecker->ReleaseChunk(*CachedChunk.Request);
	}
}
