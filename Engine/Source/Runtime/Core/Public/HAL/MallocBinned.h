// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define MEM_TIME(st)

//#define USE_LOCKFREE_DELETE
#define USE_INTERNAL_LOCKS
#define CACHE_FREED_OS_ALLOCS

#ifdef USE_INTERNAL_LOCKS
//#	define USE_COARSE_GRAIN_LOCKS
#endif

#if defined USE_LOCKFREE_DELETE
#	define USE_INTERNAL_LOCKS
#	define USE_COARSE_GRAIN_LOCKS
#endif

#if defined CACHE_FREED_OS_ALLOCS
	#define MAX_CACHED_OS_FREES (64)
	#if PLATFORM_64BITS
		#define MAX_CACHED_OS_FREES_BYTE_LIMIT (64*1024*1024)
	#else
		#define MAX_CACHED_OS_FREES_BYTE_LIMIT (16*1024*1024)
	#endif
#endif

#if defined USE_INTERNAL_LOCKS && !defined USE_COARSE_GRAIN_LOCKS
#	define USE_FINE_GRAIN_LOCKS
#endif

#include "LockFreeList.h"
#include "Array.h"

/** Malloc binned allocator specific stats. */
DECLARE_MEMORY_STAT_EXTERN(TEXT("Binned Os Current"),		STAT_Binned_OsCurrent,STATGROUP_MemoryAllocator, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Binned Os Peak"),			STAT_Binned_OsPeak,STATGROUP_MemoryAllocator, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Binned Waste Current"),	STAT_Binned_WasteCurrent,STATGROUP_MemoryAllocator, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Binned Waste Peak"),		STAT_Binned_WastePeak,STATGROUP_MemoryAllocator, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Binned Used Current"),		STAT_Binned_UsedCurrent,STATGROUP_MemoryAllocator, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Binned Used Peak"),		STAT_Binned_UsedPeak,STATGROUP_MemoryAllocator, CORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Binned Current Allocs"),	STAT_Binned_CurrentAllocs,STATGROUP_MemoryAllocator, CORE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Binned Total Allocs"),		STAT_Binned_TotalAllocs,STATGROUP_MemoryAllocator, CORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Binned Slack Current"),	STAT_Binned_SlackCurrent,STATGROUP_MemoryAllocator, CORE_API);


//
// Optimized virtual memory allocator.
//
class FMallocBinned : public FMalloc
{
private:

	// Counts.
	enum { POOL_COUNT = 42 };

	/** Maximum allocation for the pooled allocator */
	enum { EXTENED_PAGE_POOL_ALLOCATION_COUNT = 2 };
	enum { MAX_POOLED_ALLOCATION_SIZE   = 32768+1 };
	enum { PAGE_SIZE_LIMIT = 65536 };
	// BINNED_ALLOC_POOL_SIZE can be increased beyond 64k to cause binned malloc to allocate
	// the small size bins in bigger chunks. If OS Allocation is slow, increasing
	// this number *may* help performance but YMMV.
	enum { BINNED_ALLOC_POOL_SIZE = 65536 }; 

	// Forward declares.
	struct FFreeMem;
	struct FPoolTable;

	// Memory pool info. 32 bytes.
	struct FPoolInfo
	{
		/** Number of allocated elements in this pool, when counts down to zero can free the entire pool. */
		uint16			Taken;		// 2
		/** Index of pool. Index into MemSizeToPoolTable[]. Valid when < MAX_POOLED_ALLOCATION_SIZE, MAX_POOLED_ALLOCATION_SIZE is OsTable.
			When AllocSize is 0, this is the number of pages to step back to find the base address of an allocation. See FindPoolInfoInternal()
		*/
		uint16			TableIndex; // 4		
		/** Number of bytes allocated */
		uint32			AllocSize;	// 8
		/** Pointer to first free memory in this pool or the OS Allocation Size in bytes if this allocation is not binned*/
		FFreeMem*		FirstMem;   // 12/16
		FPoolInfo*		Next;		// 16/24
		FPoolInfo**		PrevLink;	// 20/32
#if PLATFORM_32BITS
		/** Explicit padding for 32 bit builds */
		uint8 Padding[12]; // 32
#endif

		void SetAllocationSizes( uint32 InBytes, UPTRINT InOsBytes, uint32 InTableIndex, uint32 SmallAllocLimt )
		{
			TableIndex=InTableIndex;
			AllocSize=InBytes;
			if (TableIndex == SmallAllocLimt)
			{
				FirstMem=(FFreeMem*)InOsBytes;
			}
		}

		uint32 GetBytes() const
		{
			return AllocSize;
		}

		UPTRINT GetOsBytes( uint32 InPageSize, uint32 SmallAllocLimt ) const
		{
			if (TableIndex == SmallAllocLimt)
			{
				return (UPTRINT)FirstMem;
			}
			else
			{
				return Align(AllocSize, InPageSize);
			}
		}

		void Link( FPoolInfo*& Before )
		{
			if( Before )
			{
				Before->PrevLink = &Next;
			}
			Next     = Before;
			PrevLink = &Before;
			Before   = this;
		}

		void Unlink()
		{
			if( Next )
			{
				Next->PrevLink = PrevLink;
			}
			*PrevLink = Next;
		}
	};

	/** Information about a piece of free memory. 8 bytes */
	struct FFreeMem
	{
		/** Next or MemLastPool[], always in order by pool. */
		FFreeMem*	Next;				
		/** Number of consecutive free blocks here, at least 1. */
		uint32		NumFreeBlocks;
	};

	/** Default alignment for binned allocator */
	enum { DEFAULT_BINNED_ALLOCATOR_ALIGNMENT = sizeof(FFreeMem) };

#ifdef CACHE_FREED_OS_ALLOCS
	/**  */
	struct FFreePageBlock
	{
		void*				Ptr;
		SIZE_T				ByteSize;

		FFreePageBlock() 
		{
			Ptr = nullptr;
			ByteSize = 0;
		}
	};
#endif

	/** Pool table. */
	struct FPoolTable
	{
		FPoolInfo*			FirstPool;
		FPoolInfo*			ExhaustedPool;
		uint32				BlockSize;
#ifdef USE_FINE_GRAIN_LOCKS
		FCriticalSection	CriticalSection;
#endif
#if STATS
		/** Number of currently active pools */
		uint32				NumActivePools;

		/** Largest number of pools simultaneously active */
		uint32				MaxActivePools;

		/** Number of requests currently active */
		uint32				ActiveRequests;

		/** High watermark of requests simultaneously active */
		uint32				MaxActiveRequests;

		/** Minimum request size (in bytes) */
		uint32				MinRequest;

		/** Maximum request size (in bytes) */
		uint32				MaxRequest;

		/** Total number of requests ever */
		uint64				TotalRequests;

		/** Total waste from all allocs in this table */
		uint64				TotalWaste;
#endif
		FPoolTable()
			: FirstPool(nullptr)
			, ExhaustedPool(nullptr)
			, BlockSize(0)
#if STATS
			, NumActivePools(0)
			, MaxActivePools(0)
			, ActiveRequests(0)
			, MaxActiveRequests(0)
			, MinRequest(0)
			, MaxRequest(0)
			, TotalRequests(0)
			, TotalWaste(0)
#endif
		{

		}
	};

	/** Hash table struct for retrieving allocation book keeping information */
	struct PoolHashBucket
	{
		UPTRINT			Key;
		FPoolInfo*		FirstPool;
		PoolHashBucket* Prev;
		PoolHashBucket* Next;

		PoolHashBucket()
		{
			Key=0;
			FirstPool=nullptr;
			Prev=this;
			Next=this;
		}

		void Link( PoolHashBucket* After )
		{
			Link(After, Prev, this);
		}

		static void Link( PoolHashBucket* Node, PoolHashBucket* Before, PoolHashBucket* After )
		{
			Node->Prev=Before;
			Node->Next=After;
			Before->Next=Node;
			After->Prev=Node;
		}

		void Unlink()
		{
			Next->Prev = Prev;
			Prev->Next = Next;
			Prev=this;
			Next=this;
		}
	};

	uint64 TableAddressLimit;

#ifdef USE_LOCKFREE_DELETE
	/** We can't call the constructor to TLockFreePointerList in the BinnedMalloc constructor
	* as it attempts to allocate memory. We push this back and initialize it later but we 
	* set aside the memory before hand
	*/
	uint8							PendingFreeListMemory[sizeof(TLockFreePointerList<void>)];
	TLockFreePointerList<void>*		PendingFreeList;
	TArray<void*>					FlushedFrees;
	bool							bFlushingFrees;
	bool							bDoneFreeListInit;
#endif

	FCriticalSection	AccessGuard;

	// PageSize dependent constants
	uint64 MaxHashBuckets; 
	uint64 MaxHashBucketBits;
	uint64 MaxHashBucketWaste;
	uint64 MaxBookKeepingOverhead;
	/** Shift to get the reference from the indirect tables */
	uint64 PoolBitShift;
	uint64 IndirectPoolBitShift;
	uint64 IndirectPoolBlockSize;
	/** Shift required to get required hash table key. */
	uint64 HashKeyShift;
	/** Used to mask off the bits that have been used to lookup the indirect table */
	uint64 PoolMask;
	uint64 BinnedSizeLimit;
	uint64 BinnedOSTableIndex;

	// Variables.
	FPoolTable  PoolTable[POOL_COUNT];
	FPoolTable	OsTable;
	FPoolTable	PagePoolTable[EXTENED_PAGE_POOL_ALLOCATION_COUNT];
	FPoolTable* MemSizeToPoolTable[MAX_POOLED_ALLOCATION_SIZE+EXTENED_PAGE_POOL_ALLOCATION_COUNT];

	PoolHashBucket* HashBuckets;
	PoolHashBucket* HashBucketFreeList;

	uint32		PageSize;

#ifdef CACHE_FREED_OS_ALLOCS
	FFreePageBlock	FreedPageBlocks[MAX_CACHED_OS_FREES];
	uint32			FreedPageBlocksNum;
	uint32			CachedTotal;
#endif

#if STATS
	SIZE_T		OsCurrent;
	SIZE_T		OsPeak;
	SIZE_T		WasteCurrent;
	SIZE_T		WastePeak;
	SIZE_T		UsedCurrent;
	SIZE_T		UsedPeak;
	SIZE_T		CurrentAllocs;
	SIZE_T		TotalAllocs;
	/** OsCurrent - WasteCurrent - UsedCurrent. */
	SIZE_T		SlackCurrent;
	double		MemTime;
#endif

	// Implementation. 
	CA_NO_RETURN void OutOfMemory(uint64 Size, uint32 Alignment=0)
	{
		// this is expected not to return
		FPlatformMemory::OnOutOfMemory(Size, Alignment);
	}

	FORCEINLINE void TrackStats(FPoolTable* Table, SIZE_T Size)
	{
#if STATS
		// keep track of memory lost to padding
		Table->TotalWaste += Table->BlockSize - Size;
		Table->TotalRequests++;
		Table->ActiveRequests++;
		Table->MaxActiveRequests = FMath::Max(Table->MaxActiveRequests, Table->ActiveRequests);
		Table->MaxRequest = Size > Table->MaxRequest ? Size : Table->MaxRequest;
		Table->MinRequest = Size < Table->MinRequest ? Size : Table->MinRequest;
#endif
	}

	/** 
	 * Create a 64k page of FPoolInfo structures for tracking allocations
	 */
	FPoolInfo* CreateIndirect()
	{
		checkSlow(IndirectPoolBlockSize * sizeof(FPoolInfo) <= PageSize);
		FPoolInfo* Indirect = (FPoolInfo*)FPlatformMemory::BinnedAllocFromOS(IndirectPoolBlockSize * sizeof(FPoolInfo));
		if( !Indirect )
		{
			OutOfMemory(IndirectPoolBlockSize * sizeof(FPoolInfo));
		}
		FMemory::Memset(Indirect, 0, IndirectPoolBlockSize*sizeof(FPoolInfo));
		STAT(OsPeak = FMath::Max(OsPeak, OsCurrent += Align(IndirectPoolBlockSize * sizeof(FPoolInfo), PageSize)));
		STAT(WastePeak = FMath::Max(WastePeak, WasteCurrent += Align(IndirectPoolBlockSize * sizeof(FPoolInfo), PageSize)));
		return Indirect;
	}

	/** 
	* Gets the FPoolInfo for a memory address. If no valid info exists one is created. 
	* NOTE: This function requires a mutex across threads, but its is the callers responsibility to 
	* acquire the mutex before calling
	*/
	FORCEINLINE FPoolInfo* GetPoolInfo( UPTRINT Ptr )
	{
		if (!HashBuckets)
		{
			// Init tables.
			HashBuckets = (PoolHashBucket*)FPlatformMemory::BinnedAllocFromOS(Align(MaxHashBuckets*sizeof(PoolHashBucket), PageSize));

			for (uint32 i=0; i<MaxHashBuckets; ++i) 
			{
				new (HashBuckets+i) PoolHashBucket();
			}
		}

		UPTRINT Key=Ptr>>HashKeyShift;
		UPTRINT Hash=Key&(MaxHashBuckets-1);
		UPTRINT PoolIndex=((UPTRINT)Ptr >> PoolBitShift) & PoolMask;

		PoolHashBucket* collision=&HashBuckets[Hash];
		do
		{
			if (collision->Key==Key || !collision->FirstPool)
			{
				if (!collision->FirstPool)
				{
					collision->Key=Key;
					InitializeHashBucket(collision);
					CA_ASSUME(collision->FirstPool);
				}
				return &collision->FirstPool[PoolIndex];
			}
			collision=collision->Next;
		} while (collision!=&HashBuckets[Hash]);
		//Create a new hash bucket entry
		PoolHashBucket* NewBucket=CreateHashBucket();
		NewBucket->Key=Key;
		HashBuckets[Hash].Link(NewBucket);
		return &NewBucket->FirstPool[PoolIndex];
	}

	FORCEINLINE FPoolInfo* FindPoolInfo(UPTRINT Ptr1, UPTRINT& AllocationBase)
	{
		uint16 NextStep = 0;
		UPTRINT Ptr=Ptr1&~((UPTRINT)PageSize-1);
		for (uint32 i=0, n=(BINNED_ALLOC_POOL_SIZE/PageSize)+1; i<n; ++i)
		{
			FPoolInfo* Pool = FindPoolInfoInternal(Ptr, NextStep);
			if (Pool)
			{
				AllocationBase=Ptr;
				//checkSlow(Ptr1 >= AllocationBase && Ptr1 < AllocationBase+Pool->GetBytes());
				return Pool;
			}
			Ptr = ((Ptr-(PageSize*NextStep))-1)&~((UPTRINT)PageSize-1);
		}
		AllocationBase=0;
		return nullptr;
	}

	FORCEINLINE FPoolInfo* FindPoolInfoInternal(UPTRINT Ptr, uint16& JumpOffset)
	{
		checkSlow(HashBuckets);

		uint32 Key=Ptr>>HashKeyShift;
		uint32 Hash=Key&(MaxHashBuckets-1);
		uint32 PoolIndex=((UPTRINT)Ptr >> PoolBitShift) & PoolMask;
		JumpOffset=0;

		PoolHashBucket* collision=&HashBuckets[Hash];
		do
		{
			if (collision->Key==Key)
			{
				if (!collision->FirstPool[PoolIndex].AllocSize)
				{
					JumpOffset = collision->FirstPool[PoolIndex].TableIndex;
					return nullptr;
				}
				return &collision->FirstPool[PoolIndex];
			}
			collision=collision->Next;
		} while (collision!=&HashBuckets[Hash]);

		return nullptr;
	}

	/**
	 *	Returns a newly created and initialized PoolHashBucket for use.
	 */
	FORCEINLINE PoolHashBucket* CreateHashBucket() 
	{
		PoolHashBucket* bucket=AllocateHashBucket();
		InitializeHashBucket(bucket);
		return bucket;
	}

	/**
	 *	Initializes bucket with valid parameters
	 *	@param bucket pointer to be initialized
	 */
	FORCEINLINE void InitializeHashBucket(PoolHashBucket* bucket)
	{
		if (!bucket->FirstPool)
		{
			bucket->FirstPool=CreateIndirect();
		}
	}

	/**
	 * Allocates a hash bucket from the free list of hash buckets
	*/
	PoolHashBucket* AllocateHashBucket()
	{
		if (!HashBucketFreeList) 
		{
			HashBucketFreeList=(PoolHashBucket*)FPlatformMemory::BinnedAllocFromOS(PageSize);
			STAT(OsPeak = FMath::Max(OsPeak, OsCurrent += PageSize));
			STAT(WastePeak = FMath::Max(WastePeak, WasteCurrent += PageSize));
			for (UPTRINT i=0, n=(PageSize/sizeof(PoolHashBucket)); i<n; ++i)
			{
				HashBucketFreeList->Link(new (HashBucketFreeList+i) PoolHashBucket());
			}
		}
		PoolHashBucket* NextFree=HashBucketFreeList->Next;
		PoolHashBucket* Free=HashBucketFreeList;
		Free->Unlink();
		if (NextFree == Free) 
		{
			NextFree = nullptr;
		}
		HashBucketFreeList=NextFree;
		return Free;
	}

	FPoolInfo* AllocatePoolMemory(FPoolTable* Table, uint32 PoolSize, uint16 TableIndex)
	{
		// Must create a new pool.
		uint32 Blocks = PoolSize / Table->BlockSize;
		uint32 Bytes = Blocks * Table->BlockSize;
		UPTRINT OsBytes=Align(Bytes, PageSize);
		checkSlow(Blocks >= 1);
		checkSlow(Blocks * Table->BlockSize <= Bytes && PoolSize >= Bytes);

		// Allocate memory.
		FFreeMem* Free = nullptr;
		SIZE_T ActualPoolSize; //TODO: use this to reduce waste?
		Free = (FFreeMem*)OSAlloc(OsBytes, ActualPoolSize);

		checkSlow(!((UPTRINT)Free & (PageSize-1)));
		if( !Free )
		{
			OutOfMemory(OsBytes);
		}

		// Create pool in the indirect table.
		FPoolInfo* Pool;
		{
#ifdef USE_FINE_GRAIN_LOCKS
			FScopeLock PoolInfoLock(&AccessGuard);
#endif
			Pool = GetPoolInfo((UPTRINT)Free);
			for (UPTRINT i=(UPTRINT)PageSize, Offset=0; i<OsBytes; i+=PageSize, ++Offset)
			{
				FPoolInfo* TrailingPool = GetPoolInfo(((UPTRINT)Free)+i);
				check(TrailingPool);
				//Set trailing pools to point back to first pool
				TrailingPool->SetAllocationSizes(0, 0, Offset, BinnedOSTableIndex);
			}
		}

		// Init pool.
		Pool->Link( Table->FirstPool );
		Pool->SetAllocationSizes(Bytes, OsBytes, TableIndex, BinnedOSTableIndex);
		STAT(OsPeak = FMath::Max(OsPeak, OsCurrent += OsBytes));
		STAT(WastePeak = FMath::Max(WastePeak, WasteCurrent += OsBytes - Bytes));
		Pool->Taken		 = 0;
		Pool->FirstMem   = Free;

#if STATS
		Table->NumActivePools++;
		Table->MaxActivePools = FMath::Max(Table->MaxActivePools, Table->NumActivePools);
#endif
		// Create first free item.
		Free->NumFreeBlocks = Blocks;
		Free->Next       = nullptr;

		return Pool;
	}

	FORCEINLINE FFreeMem* AllocateBlockFromPool(FPoolTable* Table, FPoolInfo* Pool)
	{
		// Pick first available block and unlink it.
		Pool->Taken++;
		checkSlow(Pool->TableIndex < BinnedOSTableIndex); // if this is false, FirstMem is actually a size not a pointer
		checkSlow(Pool->FirstMem);
		checkSlow(Pool->FirstMem->NumFreeBlocks > 0);
		checkSlow(Pool->FirstMem->NumFreeBlocks < PAGE_SIZE_LIMIT);
		FFreeMem* Free = (FFreeMem*)((uint8*)Pool->FirstMem + --Pool->FirstMem->NumFreeBlocks * Table->BlockSize);
		if( !Pool->FirstMem->NumFreeBlocks )
		{
			Pool->FirstMem = Pool->FirstMem->Next;
			if( !Pool->FirstMem )
			{
				// Move to exhausted list.
				Pool->Unlink();
				Pool->Link( Table->ExhaustedPool );
			}
		}
		STAT(UsedPeak = FMath::Max(UsedPeak, UsedCurrent += Table->BlockSize));
		return Free;
	}

	/**
	* Releases memory back to the system. This is not protected from multi-threaded access and it's
	* the callers responsibility to Lock AccessGuard before calling this.
	*/
	void FreeInternal( void* Ptr )
	{
		MEM_TIME(MemTime -= FPlatformTime::Seconds());
		STAT(CurrentAllocs--);

		UPTRINT BasePtr;
		FPoolInfo* Pool = FindPoolInfo((UPTRINT)Ptr, BasePtr);
#if PLATFORM_IOS
        if (Pool == NULL)
        {
            UE_LOG(LogMemory, Warning, TEXT("Attempting to free a pointer we didn't allocate!"));
            return;
        }
#endif
		checkSlow(Pool);
		checkSlow(Pool->GetBytes() != 0);
		if( Pool->TableIndex < BinnedOSTableIndex )
		{
			FPoolTable* Table=MemSizeToPoolTable[Pool->TableIndex];
#ifdef USE_FINE_GRAIN_LOCKS
			FScopeLock TableLock(&Table->CriticalSection);
#endif
#if STATS
			Table->ActiveRequests--;
#endif
			// If this pool was exhausted, move to available list.
			if( !Pool->FirstMem )
			{
				Pool->Unlink();
				Pool->Link( Table->FirstPool );
			}

			// Free a pooled allocation.
			FFreeMem* Free		= (FFreeMem*)Ptr;
			Free->NumFreeBlocks	= 1;
			Free->Next			= Pool->FirstMem;
			Pool->FirstMem		= Free;
			STAT(UsedCurrent -= Table->BlockSize);

			// Free this pool.
			checkSlow(Pool->Taken >= 1);
			if( --Pool->Taken == 0 )
			{
#if STATS
				Table->NumActivePools--;
#endif
				// Free the OS memory.
				SIZE_T OsBytes = Pool->GetOsBytes(PageSize, BinnedOSTableIndex);
				STAT(OsCurrent -= OsBytes);
				STAT(WasteCurrent -= OsBytes - Pool->GetBytes());
				Pool->Unlink();
				Pool->SetAllocationSizes(0, 0, 0, BinnedOSTableIndex);
				OSFree((void*)BasePtr, OsBytes);
			}
		}
		else
		{
			// Free an OS allocation.
			checkSlow(!((UPTRINT)Ptr & (PageSize-1)));
			SIZE_T OsBytes = Pool->GetOsBytes(PageSize, BinnedOSTableIndex);
			STAT(UsedCurrent -= Pool->GetBytes());
			STAT(OsCurrent -= OsBytes);
			STAT(WasteCurrent -= OsBytes - Pool->GetBytes());
			OSFree(Ptr, OsBytes);
		}

		MEM_TIME(MemTime += FPlatformTime::Seconds());
	}

	void PushFreeLockless(void* Ptr)
	{
#ifdef USE_LOCKFREE_DELETE
		PendingFreeList->Push(Ptr);
#else
#ifdef USE_COARSE_GRAIN_LOCKS
		FScopeLock ScopedLock(&AccessGuard);
#endif
		FreeInternal(Ptr);
#endif
	}

	/**
	* Clear and Process the list of frees to be deallocated. It's the callers
	* responsibility to Lock AccessGuard before calling this
	*/
	void FlushPendingFrees()
	{
#ifdef USE_LOCKFREE_DELETE
		if (!PendingFreeList && !bDoneFreeListInit)
		{
			bDoneFreeListInit=true;
			PendingFreeList = new ((void*)PendingFreeListMemory) TLockFreePointerList<void>();
		}
		// Because a lockless list and TArray calls new/malloc internally, need to guard against re-entry
		if (bFlushingFrees || !PendingFreeList)
		{
			return;
		}
		bFlushingFrees=true;
		PendingFreeList->PopAll(FlushedFrees);
		for (uint32 i=0, n=FlushedFrees.Num(); i<n; ++i)
		{
			FreeInternal(FlushedFrees[i]);
		}
		FlushedFrees.Reset();
		bFlushingFrees=false;
#endif
	}

	FORCEINLINE void OSFree(void* Ptr, SIZE_T Size)
	{
#ifdef CACHE_FREED_OS_ALLOCS
#ifdef USE_FINE_GRAIN_LOCKS
		FScopeLock MainLock(&AccessGuard);
#endif
		if (Size > MAX_CACHED_OS_FREES_BYTE_LIMIT / 4)
		{
			FPlatformMemory::BinnedFreeToOS(Ptr);
			return;
		}
		while (FreedPageBlocksNum && (FreedPageBlocksNum >= MAX_CACHED_OS_FREES || CachedTotal + Size > MAX_CACHED_OS_FREES_BYTE_LIMIT)) 
		{
			//Remove the oldest one
			void* FreePtr = FreedPageBlocks[0].Ptr;
			CachedTotal -= FreedPageBlocks[0].ByteSize;
			FreedPageBlocksNum--;
			if (FreedPageBlocksNum)
			{
				FMemory::Memmove(&FreedPageBlocks[0], &FreedPageBlocks[1], sizeof(FFreePageBlock) * FreedPageBlocksNum);
			}
			FPlatformMemory::BinnedFreeToOS(FreePtr);
		}
		FreedPageBlocks[FreedPageBlocksNum].Ptr = Ptr;
		FreedPageBlocks[FreedPageBlocksNum].ByteSize = Size;
		CachedTotal += Size;
		++FreedPageBlocksNum;
#else
		(void)Size;
		FPlatformMemory::BinnedFreeToOS(Ptr);
#endif
	}

	FORCEINLINE void* OSAlloc(SIZE_T NewSize, SIZE_T& OutActualSize)
	{
#ifdef CACHE_FREED_OS_ALLOCS
		{
#ifdef USE_FINE_GRAIN_LOCKS
			// We want to hold the lock a little as possible so release it
			// before the big call to the OS
			FScopeLock MainLock(&AccessGuard);
#endif
			for (uint32 i=0; i < FreedPageBlocksNum; ++i)
			{
				// look for exact matches first, these are aligned to the page size, so it should be quite common to hit these on small pages sizes
				if (FreedPageBlocks[i].ByteSize == NewSize)
				{
					void* Ret=FreedPageBlocks[i].Ptr;
					OutActualSize=FreedPageBlocks[i].ByteSize;
					CachedTotal-=FreedPageBlocks[i].ByteSize;
					if (i < FreedPageBlocksNum - 1)
					{
						FMemory::Memmove(&FreedPageBlocks[i], &FreedPageBlocks[i + 1], sizeof(FFreePageBlock) * (FreedPageBlocksNum - i - 1));
					}
					FreedPageBlocksNum--;
					return Ret;
				}
			};
			for (uint32 i=0; i < FreedPageBlocksNum; ++i)
			{
				// is it possible (and worth i.e. <25% overhead) to use this block
				if (FreedPageBlocks[i].ByteSize >= NewSize && FreedPageBlocks[i].ByteSize * 3 <= NewSize * 4)
				{
					void* Ret=FreedPageBlocks[i].Ptr;
					OutActualSize=FreedPageBlocks[i].ByteSize;
					CachedTotal-=FreedPageBlocks[i].ByteSize;
					if (i < FreedPageBlocksNum - 1)
					{
						FMemory::Memmove(&FreedPageBlocks[i], &FreedPageBlocks[i + 1], sizeof(FFreePageBlock) * (FreedPageBlocksNum - i - 1));
					}
					FreedPageBlocksNum--;
					return Ret;
				}
			};
		}
		OutActualSize=NewSize;
		void* Ptr = FPlatformMemory::BinnedAllocFromOS(NewSize);
		if (!Ptr)
		{
			//Are we holding on to much mem? Release it all.
			FlushAllocCache();
			Ptr = FPlatformMemory::BinnedAllocFromOS(NewSize);
		}
		return Ptr;
#else
		(void)OutActualSize;
		return FPlatformMemory::BinnedAllocFromOS(NewSize);
#endif
	}

#ifdef CACHE_FREED_OS_ALLOCS
	void FlushAllocCache()
	{
#ifdef USE_FINE_GRAIN_LOCKS
		FScopeLock MainLock(&AccessGuard);
#endif
		for (int i=0, n=FreedPageBlocksNum; i<n; ++i) 
		{
			//Remove allocs
			FPlatformMemory::BinnedFreeToOS(FreedPageBlocks[i].Ptr);
			FreedPageBlocks[i].Ptr = nullptr;
			FreedPageBlocks[i].ByteSize = 0;
		}
		FreedPageBlocksNum = 0;
		CachedTotal = 0;
	}
#endif

public:

	// FMalloc interface.
	// InPageSize - First parameter is page size, all allocs from BinnedAllocFromOS() MUST be aligned to this size
	// AddressLimit - Second parameter is estimate of the range of addresses expected to be returns by BinnedAllocFromOS(). Binned
	// Malloc will adjust it's internal structures to make look ups for memory allocations O(1) for this range. 
	// It's is ok to go outside this range, look ups will just be a little slower
	FMallocBinned(uint32 InPageSize, uint64 AddressLimit)
		:	TableAddressLimit(AddressLimit)
#ifdef USE_LOCKFREE_DELETE
		,	PendingFreeList(nullptr)
		,	bFlushingFrees(false)
		,	bDoneFreeListInit(false)
#endif
		,	HashBuckets(nullptr)
		,	HashBucketFreeList(nullptr)
		,	PageSize		(InPageSize)
#ifdef CACHE_FREED_OS_ALLOCS
		,	FreedPageBlocksNum(0)
		,	CachedTotal(0)
#endif
#if STATS
		,	OsCurrent		( 0 )
		,	OsPeak			( 0 )
		,	WasteCurrent	( 0 )
		,	WastePeak		( 0 )
		,	UsedCurrent		( 0 )
		,	UsedPeak		( 0 )
		,	CurrentAllocs	( 0 )
		,	TotalAllocs		( 0 )
		,	SlackCurrent	( 0 )
		,	MemTime			( 0.0 )
#endif
	{
		check(!(PageSize & (PageSize - 1)));
		check(!(AddressLimit & (AddressLimit - 1)));
		check(PageSize <= 65536); // There is internal limit on page size of 64k
		check(AddressLimit > PageSize); // Check to catch 32 bit overflow in AddressLimit

		/** Shift to get the reference from the indirect tables */
		PoolBitShift = FPlatformMath::CeilLogTwo(PageSize);
		IndirectPoolBitShift = FPlatformMath::CeilLogTwo(PageSize/sizeof(FPoolInfo));
		IndirectPoolBlockSize = PageSize/sizeof(FPoolInfo);

		MaxHashBuckets = AddressLimit >> (IndirectPoolBitShift+PoolBitShift); 
		MaxHashBucketBits = FPlatformMath::CeilLogTwo(MaxHashBuckets);
		MaxHashBucketWaste = (MaxHashBuckets*sizeof(PoolHashBucket))/1024;
		MaxBookKeepingOverhead = ((AddressLimit/PageSize)*sizeof(PoolHashBucket))/(1024*1024);
		/** 
		* Shift required to get required hash table key.
		*/
		HashKeyShift = PoolBitShift+IndirectPoolBitShift;
		/** Used to mask off the bits that have been used to lookup the indirect table */
		PoolMask =  ( ( 1ull << ( HashKeyShift - PoolBitShift ) ) - 1 );
		BinnedSizeLimit = PAGE_SIZE_LIMIT/2;
		BinnedOSTableIndex = BinnedSizeLimit+EXTENED_PAGE_POOL_ALLOCATION_COUNT;

		check((BinnedSizeLimit & (BinnedSizeLimit-1)) == 0);


		// Init tables.
		OsTable.FirstPool = nullptr;
		OsTable.ExhaustedPool = nullptr;
		OsTable.BlockSize = 0;

		/** The following options are not valid for page sizes less than 64k. They are here to reduce waste*/
		PagePoolTable[0].FirstPool = nullptr;
		PagePoolTable[0].ExhaustedPool = nullptr;
		PagePoolTable[0].BlockSize = PageSize == PAGE_SIZE_LIMIT ? BinnedSizeLimit+(BinnedSizeLimit/2) : 0;

		PagePoolTable[1].FirstPool = nullptr;
		PagePoolTable[1].ExhaustedPool = nullptr;
		PagePoolTable[1].BlockSize = PageSize == PAGE_SIZE_LIMIT ? PageSize+BinnedSizeLimit : 0;

		// Block sizes are based around getting the maximum amount of allocations per pool, with as little alignment waste as possible.
		// Block sizes should be close to even divisors of the POOL_SIZE, and well distributed. They must be 16-byte aligned as well.
		static const uint32 BlockSizes[POOL_COUNT] =
		{
			8,		16,		32,		48,		64,		80,		96,		112,
			128,	160,	192,	224,	256,	288,	320,	384,
			448,	512,	576,	640,	704,	768,	896,	1024,
			1168,	1360,	1632,	2048,	2336,	2720,	3264,	4096,
			4672,	5456,	6544,	8192,	9360,	10912,	13104,	16384,
			21840,	32768
		};

		for( uint32 i = 0; i < POOL_COUNT; i++ )
		{
			PoolTable[i].FirstPool = nullptr;
			PoolTable[i].ExhaustedPool = nullptr;
			PoolTable[i].BlockSize = BlockSizes[i];
#if STATS
			PoolTable[i].MinRequest = PoolTable[i].BlockSize;
#endif
		}

		for( uint32 i=0; i<MAX_POOLED_ALLOCATION_SIZE; i++ )
		{
			uint32 Index = 0;
			while( PoolTable[Index].BlockSize < i )
			{
				++Index;
			}
			checkSlow(Index < POOL_COUNT);
			MemSizeToPoolTable[i] = &PoolTable[Index];
		}

		MemSizeToPoolTable[BinnedSizeLimit] = &PagePoolTable[0];
		MemSizeToPoolTable[BinnedSizeLimit+1] = &PagePoolTable[1];

		check(MAX_POOLED_ALLOCATION_SIZE - 1 == PoolTable[POOL_COUNT - 1].BlockSize);
	}
	
	virtual void InitializeStatsMetadata() override;

	virtual ~FMallocBinned()
	{}

	/**
	 * Returns if the allocator is guaranteed to be thread-safe and therefore
	 * doesn't need a unnecessary thread-safety wrapper around it.
	 */
	virtual bool IsInternallyThreadSafe() const override
	{ 
#ifdef USE_INTERNAL_LOCKS
		return true;
#else
		return false;
#endif
	}

	/** 
	 * Malloc
	 */
	virtual void* Malloc( SIZE_T Size, uint32 Alignment ) override
	{
#ifdef USE_COARSE_GRAIN_LOCKS
		FScopeLock ScopedLock(&AccessGuard);
#endif

		FlushPendingFrees();

		// Handle DEFAULT_ALIGNMENT for binned allocator.
		if (Alignment == DEFAULT_ALIGNMENT)
		{
			Alignment = DEFAULT_BINNED_ALLOCATOR_ALIGNMENT;
		}
		check(Alignment <= PageSize);
		Alignment = FMath::Max<uint32>(Alignment, DEFAULT_BINNED_ALLOCATOR_ALIGNMENT);
		Size = FMath::Max<SIZE_T>(Alignment, Align(Size, Alignment));
		MEM_TIME(MemTime -= FPlatformTime::Seconds());
		STAT(CurrentAllocs++);
		STAT(TotalAllocs++);
		FFreeMem* Free;
		if( Size < BinnedSizeLimit )
		{
			// Allocate from pool.
			FPoolTable* Table = MemSizeToPoolTable[Size];
#ifdef USE_FINE_GRAIN_LOCKS
			FScopeLock TableLock(&Table->CriticalSection);
#endif
			checkSlow(Size <= Table->BlockSize);

			TrackStats(Table, Size);

			FPoolInfo* Pool = Table->FirstPool;
			if( !Pool )
			{
				Pool = AllocatePoolMemory(Table, BINNED_ALLOC_POOL_SIZE/*PageSize*/, Size);
			}

			Free = AllocateBlockFromPool(Table, Pool);
		}
		else if ( ((Size >= BinnedSizeLimit && Size <= PagePoolTable[0].BlockSize) ||
				  (Size > PageSize && Size <= PagePoolTable[1].BlockSize))
				  && Alignment == DEFAULT_BINNED_ALLOCATOR_ALIGNMENT )
		{
			// Bucket in a pool of 3*PageSize or 6*PageSize
			uint32 BinType = Size < PageSize ? 0 : 1;
			uint32 PageCount = 3*BinType + 3;
			FPoolTable* Table = &PagePoolTable[BinType];
#ifdef USE_FINE_GRAIN_LOCKS
			FScopeLock TableLock(&Table->CriticalSection);
#endif
			checkSlow(Size <= Table->BlockSize);

			TrackStats(Table, Size);

			FPoolInfo* Pool = Table->FirstPool;
			if( !Pool )
			{
				Pool = AllocatePoolMemory(Table, PageCount*PageSize, BinnedSizeLimit+BinType);
			}

			Free = AllocateBlockFromPool(Table, Pool);
		}
		else
		{
			// Use OS for large allocations.
			UPTRINT AlignedSize = Align(Size,PageSize);
			SIZE_T ActualPoolSize; //TODO: use this to reduce waste?
			Free = (FFreeMem*)OSAlloc(AlignedSize, ActualPoolSize);
			if( !Free )
			{
				OutOfMemory(AlignedSize);
			}
			checkSlow(!((SIZE_T)Free & (PageSize-1)));

			// Create indirect.
			FPoolInfo* Pool;
			{
#ifdef USE_FINE_GRAIN_LOCKS
				FScopeLock PoolInfoLock(&AccessGuard);
#endif
				Pool = GetPoolInfo((UPTRINT)Free);
			}

			Pool->SetAllocationSizes(Size, AlignedSize, BinnedOSTableIndex, BinnedOSTableIndex);
			STAT(OsPeak = FMath::Max(OsPeak, OsCurrent += AlignedSize));
			STAT(UsedPeak = FMath::Max(UsedPeak, UsedCurrent += Size));
			STAT(WastePeak = FMath::Max(WastePeak, WasteCurrent += AlignedSize - Size));
		}

		MEM_TIME(MemTime += FPlatformTime::Seconds());
		return Free;
	}

	/** 
	 * Realloc
	 */
	virtual void* Realloc( void* Ptr, SIZE_T NewSize, uint32 Alignment ) override
	{
		// Handle DEFAULT_ALIGNMENT for binned allocator.
		if (Alignment == DEFAULT_ALIGNMENT)
		{
			Alignment = DEFAULT_BINNED_ALLOCATOR_ALIGNMENT;
		}
		check(Alignment <= PageSize);
		Alignment = FMath::Max<uint32>(Alignment, DEFAULT_BINNED_ALLOCATOR_ALIGNMENT);
		if (NewSize)
		{
			NewSize = FMath::Max<SIZE_T>(Alignment, Align(NewSize, Alignment));
		}
		MEM_TIME(MemTime -= FPlatformTime::Seconds());
		UPTRINT BasePtr;
		void* NewPtr = Ptr;
		if( Ptr && NewSize )
		{
			FPoolInfo* Pool = FindPoolInfo((UPTRINT)Ptr, BasePtr);

			if( Pool->TableIndex < BinnedOSTableIndex )
			{
				// Allocated from pool, so grow or shrink if necessary.
				check(Pool->TableIndex > 0); // it isn't possible to allocate a size of 0, Malloc will increase the size to DEFAULT_BINNED_ALLOCATOR_ALIGNMENT
				if( NewSize>MemSizeToPoolTable[Pool->TableIndex]->BlockSize || NewSize<=MemSizeToPoolTable[Pool->TableIndex-1]->BlockSize )
				{
					NewPtr = Malloc( NewSize, Alignment );
					FMemory::Memcpy( NewPtr, Ptr, FMath::Min<SIZE_T>( NewSize, MemSizeToPoolTable[Pool->TableIndex]->BlockSize ) );
					Free( Ptr );
				}
			}
			else
			{
				// Allocated from OS.
				checkSlow(!((UPTRINT)Ptr & (PageSize-1)));
				if( NewSize > Pool->GetOsBytes(PageSize, BinnedOSTableIndex) || NewSize * 3 < Pool->GetOsBytes(PageSize, BinnedOSTableIndex) * 2 )
				{
					// Grow or shrink.
					NewPtr = Malloc( NewSize, Alignment );
					FMemory::Memcpy( NewPtr, Ptr, FMath::Min<SIZE_T>(NewSize, Pool->GetBytes()) );
					Free( Ptr );
				}
				else
				{
					// Keep as-is, reallocation isn't worth the overhead.
					STAT(UsedCurrent += NewSize - Pool->GetBytes());
					STAT(UsedPeak = FMath::Max(UsedPeak, UsedCurrent));
					STAT(WasteCurrent += Pool->GetBytes() - NewSize);
					Pool->SetAllocationSizes(NewSize, Pool->GetOsBytes(PageSize, BinnedOSTableIndex), BinnedOSTableIndex, BinnedOSTableIndex);
				}
			}
		}
		else if( Ptr == nullptr )
		{
			NewPtr = Malloc( NewSize, Alignment );
		}
		else
		{
			Free( Ptr );
			NewPtr = nullptr;
		}

		MEM_TIME(MemTime += FPlatformTime::Seconds());
		return NewPtr;
	}
	
	/** 
	 * Free
	 */
	virtual void Free( void* Ptr ) override
	{
		if( !Ptr )
		{
			return;
		}

		PushFreeLockless(Ptr);
	}

	/**
	 * If possible determine the size of the memory allocated at the given address
	 *
	 * @param Original - Pointer to memory we are checking the size of
	 * @param SizeOut - If possible, this value is set to the size of the passed in pointer
	 * @return true if succeeded
	 */
	virtual bool GetAllocationSize(void *Original, SIZE_T &SizeOut) override
	{
		if (!Original)
		{
			return false;
		}
		UPTRINT BasePtr;
		FPoolInfo* Pool = FindPoolInfo((UPTRINT)Original, BasePtr);
		SizeOut = Pool->TableIndex < BinnedOSTableIndex ? MemSizeToPoolTable[Pool->TableIndex]->BlockSize : Pool->GetBytes();
		return true;
	}

	/**
	 * Validates the allocator's heap
	 */
	virtual bool ValidateHeap() override
	{
#ifdef USE_COARSE_GRAIN_LOCKS
		FScopeLock ScopedLock(&AccessGuard);
#endif
		for( int32 i = 0; i < POOL_COUNT; i++ )
		{
			FPoolTable* Table = &PoolTable[i];
#ifdef USE_FINE_GRAIN_LOCKS
			FScopeLock TableLock(&Table->CriticalSection);
#endif
			for( FPoolInfo** PoolPtr = &Table->FirstPool; *PoolPtr; PoolPtr = &(*PoolPtr)->Next )
			{
				FPoolInfo* Pool = *PoolPtr;
				check(Pool->PrevLink == PoolPtr);
				check(Pool->FirstMem);
				for( FFreeMem* Free = Pool->FirstMem; Free; Free = Free->Next )
				{
					check(Free->NumFreeBlocks > 0);
				}
			}
			for( FPoolInfo** PoolPtr = &Table->ExhaustedPool; *PoolPtr; PoolPtr = &(*PoolPtr)->Next )
			{
				FPoolInfo* Pool = *PoolPtr;
				check(Pool->PrevLink == PoolPtr);
				check(!Pool->FirstMem);
			}
		}

		return( true );
	}


	/** Called once per frame, gathers and sets all memory allocator statistics into the corresponding stats. */
	virtual void UpdateStats() override
	{
		FMalloc::UpdateStats();
#if STATS
		SIZE_T	LocalOsCurrent = 0;
		SIZE_T	LocalOsPeak = 0;
		SIZE_T	LocalWasteCurrent = 0;
		SIZE_T	LocalWastePeak = 0;
		SIZE_T	LocalUsedCurrent = 0;
		SIZE_T	LocalUsedPeak = 0;
		SIZE_T	LocalCurrentAllocs = 0;
		SIZE_T	LocalTotalAllocs = 0;
		SIZE_T	LocalSlackCurrent = 0;

		{
#ifdef USE_INTERNAL_LOCKS
			FScopeLock ScopedLock( &AccessGuard );
#endif
			UpdateSlackStat();

			// Copy memory stats.
			LocalOsCurrent = OsCurrent;
			LocalOsPeak = OsPeak;
			LocalWasteCurrent = WasteCurrent;
			LocalWastePeak = WastePeak;
			LocalUsedCurrent = UsedCurrent;
			LocalUsedPeak = UsedPeak;
			LocalCurrentAllocs = CurrentAllocs;
			LocalTotalAllocs = TotalAllocs;
			LocalSlackCurrent = SlackCurrent;
		}

		SET_MEMORY_STAT( STAT_Binned_OsCurrent, LocalOsCurrent );
		SET_MEMORY_STAT( STAT_Binned_OsPeak, LocalOsPeak );
		SET_MEMORY_STAT( STAT_Binned_WasteCurrent, LocalWasteCurrent );
		SET_MEMORY_STAT( STAT_Binned_WastePeak, LocalWastePeak );
		SET_MEMORY_STAT( STAT_Binned_UsedCurrent, LocalUsedCurrent );
		SET_MEMORY_STAT( STAT_Binned_UsedPeak, LocalUsedPeak );
		SET_DWORD_STAT( STAT_Binned_CurrentAllocs, LocalCurrentAllocs );
		SET_DWORD_STAT( STAT_Binned_TotalAllocs, LocalTotalAllocs );
		SET_MEMORY_STAT( STAT_Binned_SlackCurrent, LocalSlackCurrent );
#endif
	}

	/** Writes allocator stats from the last update into the specified destination. */
	virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats ) override;

	/**
	 * Dumps allocator stats to an output device. Subclasses should override to add additional info
	 *
	 * @param Ar	[in] Output device
	 */
	virtual void DumpAllocatorStats( class FOutputDevice& Ar ) override
	{
		FBufferedOutputDevice BufferedOutput;
		{
#ifdef USE_COARSE_GRAIN_LOCKS
			FScopeLock ScopedLock( &AccessGuard );
#endif
			ValidateHeap();
#if STATS
			UpdateSlackStat();
#if !NO_LOGGING
			// This is all of the memory including stuff too big for the pools
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Allocator Stats for %s:" ), GetDescriptiveName() );
			// Waste is the total overhead of the memory system
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current Memory %.2f MB used, plus %.2f MB waste" ), UsedCurrent / (1024.0f * 1024.0f), (OsCurrent - UsedCurrent) / (1024.0f * 1024.0f) );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Peak Memory %.2f MB used, plus %.2f MB waste" ), UsedPeak / (1024.0f * 1024.0f), (OsPeak - UsedPeak) / (1024.0f * 1024.0f) );

			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current OS Memory %.2f MB, peak %.2f MB" ), OsCurrent / (1024.0f * 1024.0f), OsPeak / (1024.0f * 1024.0f) );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current Waste %.2f MB, peak %.2f MB" ), WasteCurrent / (1024.0f * 1024.0f), WastePeak / (1024.0f * 1024.0f) );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current Used %.2f MB, peak %.2f MB" ), UsedCurrent / (1024.0f * 1024.0f), UsedPeak / (1024.0f * 1024.0f) );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Current Slack %.2f MB" ), SlackCurrent / (1024.0f * 1024.0f) );

			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Allocs      % 6i Current / % 6i Total" ), CurrentAllocs, TotalAllocs );
			MEM_TIME( BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Seconds     % 5.3f" ), MemTime ) );
			MEM_TIME( BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "MSec/Allc   % 5.5f" ), 1000.0 * MemTime / MemAllocs ) );

			// This is the memory tracked inside individual allocation pools
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "" ) );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Block Size Num Pools Max Pools Cur Allocs Total Allocs Min Req Max Req Mem Used Mem Slack Mem Waste Efficiency" ) );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "---------- --------- --------- ---------- ------------ ------- ------- -------- --------- --------- ----------" ) );

			uint32 TotalMemory = 0;
			uint32 TotalWaste = 0;
			uint32 TotalActiveRequests = 0;
			uint32 TotalTotalRequests = 0;
			uint32 TotalPools = 0;
			uint32 TotalSlack = 0;

			FPoolTable* Table = nullptr;
			for( int32 i = 0; i < BinnedSizeLimit + EXTENED_PAGE_POOL_ALLOCATION_COUNT; i++ )
			{
				if( Table == MemSizeToPoolTable[i] || MemSizeToPoolTable[i]->BlockSize == 0 )
					continue;

				Table = MemSizeToPoolTable[i];

#ifdef USE_FINE_GRAIN_LOCKS
				FScopeLock TableLock(&Table->CriticalSection);
#endif

				uint32 TableAllocSize = (Table->BlockSize > BinnedSizeLimit ? (((3 * (i - BinnedSizeLimit)) + 3)*BINNED_ALLOC_POOL_SIZE) : BINNED_ALLOC_POOL_SIZE);
				// The amount of memory allocated from the OS
				uint32 MemAllocated = (Table->NumActivePools * TableAllocSize) / 1024;
				// Amount of memory actually in use by allocations
				uint32 MemUsed = (Table->BlockSize * Table->ActiveRequests) / 1024;
				// Wasted memory due to pool size alignment
				uint32 PoolMemWaste = Table->NumActivePools * (TableAllocSize - ((TableAllocSize / Table->BlockSize) * Table->BlockSize)) / 1024;
				// Wasted memory due to individual allocation alignment. This is an estimate.
				uint32 MemWaste = (uint32)(((double)Table->TotalWaste / (double)Table->TotalRequests) * (double)Table->ActiveRequests) / 1024 + PoolMemWaste;
				// Memory that is reserved in active pools and ready for future use
				uint32 MemSlack = MemAllocated - MemUsed - PoolMemWaste;

				BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "% 10i % 9i % 9i % 10i % 12i % 7i % 7i % 7iK % 8iK % 8iK % 9.2f%%" ),
											  Table->BlockSize,
											  Table->NumActivePools,
											  Table->MaxActivePools,
											  Table->ActiveRequests,
											  (uint32)Table->TotalRequests,
											  Table->MinRequest,
											  Table->MaxRequest,
											  MemUsed,
											  MemSlack,
											  MemWaste,
											  MemAllocated ? 100.0f * (MemAllocated - MemWaste) / MemAllocated : 100.0f );

				TotalMemory += MemAllocated;
				TotalWaste += MemWaste;
				TotalSlack += MemSlack;
				TotalActiveRequests += Table->ActiveRequests;
				TotalTotalRequests += Table->TotalRequests;
				TotalPools += Table->NumActivePools;
			}

			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "" ) );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "%iK allocated in pools (with %iK slack and %iK waste). Efficiency %.2f%%" ), TotalMemory, TotalSlack, TotalWaste, TotalMemory ? 100.0f * (TotalMemory - TotalWaste) / TotalMemory : 100.0f );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "Allocations %i Current / %i Total (in %i pools)" ), TotalActiveRequests, TotalTotalRequests, TotalPools );
			BufferedOutput.CategorizedLogf( LogMemory.GetCategoryName(), ELogVerbosity::Log, TEXT( "" ) );
#endif
#endif
		}

		BufferedOutput.RedirectTo( Ar );
	}

	virtual const TCHAR* GetDescriptiveName() override { return TEXT("binned"); }

protected:

	void UpdateSlackStat()
	{
#if	STATS
		SIZE_T LocalWaste = WasteCurrent;
		double Waste = 0.0;
		for( int32 PoolIndex = 0; PoolIndex < POOL_COUNT; PoolIndex++ )
		{
			Waste += ( ( double )PoolTable[PoolIndex].TotalWaste / ( double )PoolTable[PoolIndex].TotalRequests ) * ( double )PoolTable[PoolIndex].ActiveRequests;
			Waste += PoolTable[PoolIndex].NumActivePools * ( BINNED_ALLOC_POOL_SIZE - ( ( BINNED_ALLOC_POOL_SIZE / PoolTable[PoolIndex].BlockSize ) * PoolTable[PoolIndex].BlockSize ) );
		}
		LocalWaste += ( uint32 )Waste;
		SlackCurrent = OsCurrent - LocalWaste - UsedCurrent;
#endif // STATS
	}

	///////////////////////////////////
	//// API platforms must implement
	///////////////////////////////////
	/**
	 * @return the page size the OS uses
	 *	NOTE: This MUST be implemented on each platform that uses this!
	 */
	uint32 BinnedGetPageSize();
};
