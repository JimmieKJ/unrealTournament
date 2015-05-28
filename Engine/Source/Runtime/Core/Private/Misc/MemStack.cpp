// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

DECLARE_MEMORY_STAT(TEXT("MemStack Large Block"), STAT_MemStackLargeBLock,STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("PageAllocator Free"), STAT_PageAllocatorFree, STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("PageAllocator Used"), STAT_PageAllocatorUsed, STATGROUP_Memory);

TLockFreeFixedSizeAllocator<FPageAllocator::PageSize, FThreadSafeCounter> FPageAllocator::TheAllocator;
TLockFreeFixedSizeAllocator<FPageAllocator::SmallPageSize, FThreadSafeCounter> FPageAllocator::TheSmallAllocator;

#if STATS
void FPageAllocator::UpdateStats()
{
	SET_MEMORY_STAT(STAT_PageAllocatorFree, BytesFree());
	SET_MEMORY_STAT(STAT_PageAllocatorUsed, BytesUsed());
}
#endif

/*-----------------------------------------------------------------------------
	FMemStack implementation.
-----------------------------------------------------------------------------*/

int32 FMemStackBase::GetByteCount() const
{
	int32 Count = 0;
	for( FTaggedMemory* Chunk=TopChunk; Chunk; Chunk=Chunk->Next )
	{
		if( Chunk!=TopChunk )
		{
			Count += Chunk->DataSize;
		}
		else
		{
			Count += Top - Chunk->Data();
		}
	}
	return Count;
}

void FMemStackBase::AllocateNewChunk(int32 MinSize)
{
	FTaggedMemory* Chunk=nullptr;
	// Create new chunk.
	int32 TotalSize = MinSize + (int32)sizeof(FTaggedMemory);
	uint32 AllocSize;
	if (TopChunk || TotalSize > FPageAllocator::SmallPageSize)
	{
		AllocSize = AlignArbitrary<int32>(TotalSize, FPageAllocator::PageSize);
		if (AllocSize == FPageAllocator::PageSize)
		{
			Chunk = (FTaggedMemory*)FPageAllocator::Alloc();
		}
		else
		{
			Chunk = (FTaggedMemory*)FMemory::Malloc(AllocSize);
			INC_MEMORY_STAT_BY(STAT_MemStackLargeBLock, AllocSize);
		}
		check(AllocSize != FPageAllocator::SmallPageSize);
	}
	else
	{
		AllocSize = FPageAllocator::SmallPageSize;
		Chunk = (FTaggedMemory*)FPageAllocator::AllocSmall();
	}
	Chunk->DataSize = AllocSize - sizeof(FTaggedMemory);

	Chunk->Next = TopChunk;
	TopChunk    = Chunk;
	Top         = Chunk->Data();
	End         = Top + Chunk->DataSize;
}

void FMemStackBase::FreeChunks(FTaggedMemory* NewTopChunk)
{
	while( TopChunk!=NewTopChunk )
	{
		FTaggedMemory* RemoveChunk = TopChunk;
		TopChunk                   = TopChunk->Next;
		if (RemoveChunk->DataSize + sizeof(FTaggedMemory) == FPageAllocator::PageSize)
		{
			FPageAllocator::Free(RemoveChunk);
		}
		else if (RemoveChunk->DataSize + sizeof(FTaggedMemory) == FPageAllocator::SmallPageSize)
		{
			FPageAllocator::FreeSmall(RemoveChunk);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_MemStackLargeBLock, RemoveChunk->DataSize + sizeof(FTaggedMemory));
			FMemory::Free(RemoveChunk);
		}
	}
	Top = nullptr;
	End = nullptr;
	if( TopChunk )
	{
		Top = TopChunk->Data();
		End = Top + TopChunk->DataSize;
	}
}

bool FMemStackBase::ContainsPointer(const void* Pointer) const
{
	const uint8* Ptr = (const uint8*)Pointer;
	for (const FTaggedMemory* Chunk = TopChunk; Chunk; Chunk = Chunk->Next)
	{
		if (Ptr >= Chunk->Data() && Ptr < Chunk->Data() + Chunk->DataSize)
		{
			return true;
		}
	}

	return false;
}
