// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FCachedOSPageAllocator
{
protected:
	struct FFreePageBlock
	{
		void*  Ptr;
		SIZE_T ByteSize;

		FFreePageBlock() 
		{
			Ptr      = nullptr;
			ByteSize = 0;
		}
	};

	void* AllocateImpl(SIZE_T Size, FFreePageBlock* First, FFreePageBlock* Last, uint32& FreedPageBlocksNum, uint32& CachedTotal);
	void FreeImpl(void* Ptr, SIZE_T Size, uint32 NumCacheBlocks, uint32 CachedByteLimit, FFreePageBlock* First, uint32& FreedPageBlocksNum, uint32& CachedTotal);
	void FreeAllImpl(FFreePageBlock* First, uint32& FreedPageBlocksNum, uint32& CachedTotal);
};

template <uint32 NumCacheBlocks, uint32 CachedByteLimit>
struct TCachedOSPageAllocator : private FCachedOSPageAllocator
{
	TCachedOSPageAllocator()
		: FreedPageBlocksNum(0)
		, CachedTotal       (0)
	{
	}

	FORCEINLINE void* Allocate(SIZE_T Size)
	{
		return AllocateImpl(Size, FreedPageBlocks, FreedPageBlocks + FreedPageBlocksNum, FreedPageBlocksNum, CachedTotal);
	}

	void Free(void* Ptr, SIZE_T Size)
	{
		return FreeImpl(Ptr, Size, NumCacheBlocks, CachedByteLimit, FreedPageBlocks, FreedPageBlocksNum, CachedTotal);
	}
	void FreeAll()
	{
		return FreeAllImpl(FreedPageBlocks, FreedPageBlocksNum, CachedTotal);
	}

private:
	FFreePageBlock FreedPageBlocks[NumCacheBlocks];
	uint32         FreedPageBlocksNum;
	uint32         CachedTotal;
};
