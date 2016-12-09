// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/MemoryBase.h"
#include "HAL/UnrealMemory.h"
#include "Math/NumericLimits.h"
#include "Templates/AlignmentTemplates.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTLS.h"
#include "HAL/Allocators/CachedOSPageAllocator.h"
#include "HAL/PlatformMath.h"

#define BINNED2_MAX_CACHED_OS_FREES (64)
#if PLATFORM_64BITS
	#define BINNED2_MAX_CACHED_OS_FREES_BYTE_LIMIT (64*1024*1024)
#else
	#define BINNED2_MAX_CACHED_OS_FREES_BYTE_LIMIT (16*1024*1024)
#endif

#define BINNED2_LARGE_ALLOC					65536		// Alignment of OS-allocated pointer - pool-allocated pointers will have a non-aligned pointer
#define BINNED2_MINIMUM_ALIGNMENT_SHIFT		4			// Alignment of blocks, expressed as a shift
#define BINNED2_MINIMUM_ALIGNMENT			16			// Alignment of blocks
#define BINNED2_MAX_SMALL_POOL_SIZE			(32768-16)	// Maximum block size in GMallocBinned2SmallBlockSizes
#define BINNED2_SMALL_POOL_COUNT			45


#define DEFAULT_GMallocBinned2PerThreadCaches 1
#define DEFAULT_GMallocBinned2LockFreeCaches 0
#define DEFAULT_GMallocBinned2BundleSize BINNED2_LARGE_ALLOC
#define DEFAULT_GMallocBinned2BundleCount 64
#define DEFAULT_GMallocBinned2AllocExtra 32
#define BINNED2_MAX_GMallocBinned2MaxBundlesBeforeRecycle 8

#define BINNED2_ALLOW_RUNTIME_TWEAKING 0
#if BINNED2_ALLOW_RUNTIME_TWEAKING
	extern CORE_API int32 GMallocBinned2PerThreadCaches;
	extern CORE_API int32 GMallocBinned2BundleSize = DEFAULT_GMallocBinned2BundleSize;
	extern CORE_API int32 GMallocBinned2BundleCount = DEFAULT_GMallocBinned2BundleCount;
	extern CORE_API int32 GMallocBinned2MaxBundlesBeforeRecycle = BINNED2_MAX_GMallocBinned2MaxBundlesBeforeRecycle;
	extern CORE_API int32 GMallocBinned2AllocExtra = DEFAULT_GMallocBinned2AllocExtra;
#else
	#define GMallocBinned2PerThreadCaches DEFAULT_GMallocBinned2PerThreadCaches
	#define GMallocBinned2BundleSize DEFAULT_GMallocBinned2BundleSize
	#define GMallocBinned2BundleCount DEFAULT_GMallocBinned2BundleCount
	#define GMallocBinned2MaxBundlesBeforeRecycle BINNED2_MAX_GMallocBinned2MaxBundlesBeforeRecycle
	#define GMallocBinned2AllocExtra DEFAULT_GMallocBinned2AllocExtra
#endif



//
// Optimized virtual memory allocator.
//
class CORE_API FMallocBinned2 final : public FMalloc
{
	struct Private;

	// Forward declares.
	struct FPoolInfo;
	struct PoolHashBucket;

	/** Information about a piece of free memory. */
	struct FFreeBlock
	{
		enum
		{
			CANARY_VALUE = 0xe3
		};

		FORCEINLINE FFreeBlock(uint32 InPageSize, uint32 InBlockSize, uint32 InPoolIndex)
			: BlockSize(InBlockSize)
			, PoolIndex(InPoolIndex)
			, Canary(CANARY_VALUE)
			, NextFreeBlock(nullptr)
		{
			check(InPoolIndex < MAX_uint8 && InBlockSize <= MAX_uint16);
			NumFreeBlocks = InPageSize / InBlockSize;
			if (NumFreeBlocks * InBlockSize + BINNED2_MINIMUM_ALIGNMENT > InPageSize)
			{
				NumFreeBlocks--;
			}
			check(NumFreeBlocks * InBlockSize + BINNED2_MINIMUM_ALIGNMENT <= InPageSize);
		}

		FORCEINLINE uint32 GetNumFreeRegularBlocks() const
		{
			return NumFreeBlocks;
		}
		FORCEINLINE bool IsCanaryOk() const
		{
			return Canary == FFreeBlock::CANARY_VALUE;
		}

		FORCEINLINE void CanaryTest() const
		{
			if (!IsCanaryOk())
			{
				CanaryFail();
			}
			//checkSlow(PoolIndex == BoundSizeToPoolIndex(BlockSize));
		}
		void CanaryFail() const;

		FORCEINLINE void* AllocateRegularBlock()
		{
			--NumFreeBlocks;
			if (IsAligned(this, BINNED2_LARGE_ALLOC))
			{
				return (uint8*)this + BINNED2_LARGE_ALLOC - (NumFreeBlocks + 1) * BlockSize;
			}
			return (uint8*)this + (NumFreeBlocks)* BlockSize;
		}

		uint16 BlockSize;				// Size of the blocks that this list points to
		uint8 PoolIndex;				// Index of this pool
		uint8 Canary;					// Constant value of 0xe3
		uint32 NumFreeBlocks;          // Number of consecutive free blocks here, at least 1.
		void*  NextFreeBlock;          // Next free block in another pool
	};

	struct FPoolList
	{
		FPoolList();

		bool IsEmpty() const;

		      FPoolInfo& GetFrontPool();
		const FPoolInfo& GetFrontPool() const;

		void LinkToFront(FPoolInfo* Pool);

		FPoolInfo& PushNewPoolToFront(FMallocBinned2& Allocator, uint32 InBytes, uint32 InPoolIndex);

		void ValidateActivePools();
		void ValidateExhaustedPools();

	private:
		FPoolInfo* Front;
	};

	/** Pool table. */
	struct FPoolTable
	{
		FPoolList ActivePools;
		FPoolList ExhaustedPools;
		uint32    BlockSize;

		FPoolTable();
	};

	struct FPtrToPoolMapping
	{
		FPtrToPoolMapping()
			: PtrToPoolPageBitShift(0)
			, HashKeyShift(0)
			, PoolMask(0)
			, MaxHashBuckets(0)
		{
		}
		explicit FPtrToPoolMapping(uint32 InPageSize, uint64 InNumPoolsPerPage, uint64 AddressLimit)
		{
			Init(InPageSize, InNumPoolsPerPage, AddressLimit);
		}

		void Init(uint32 InPageSize, uint64 InNumPoolsPerPage, uint64 AddressLimit)
		{
			uint64 PoolPageToPoolBitShift = FPlatformMath::CeilLogTwo(InNumPoolsPerPage);

			PtrToPoolPageBitShift = FPlatformMath::CeilLogTwo(InPageSize);
			HashKeyShift          = PtrToPoolPageBitShift + PoolPageToPoolBitShift;
			PoolMask              = (1ull << PoolPageToPoolBitShift) - 1;
			MaxHashBuckets        = AddressLimit >> HashKeyShift;
		}

		FORCEINLINE void GetHashBucketAndPoolIndices(const void* InPtr, uint32& OutBucketIndex, UPTRINT& OutBucketCollision, uint32& OutPoolIndex) const
		{
			OutBucketCollision = (UPTRINT)InPtr >> HashKeyShift;
			OutBucketIndex = uint32(OutBucketCollision & (MaxHashBuckets - 1));
			OutPoolIndex   = ((UPTRINT)InPtr >> PtrToPoolPageBitShift) & PoolMask;
		}

		FORCEINLINE uint64 GetMaxHashBuckets() const
		{
			return MaxHashBuckets;
		}

	private:
		/** Shift to apply to a pointer to get the reference from the indirect tables */
		uint64 PtrToPoolPageBitShift;

		/** Shift required to get required hash table key. */
		uint64 HashKeyShift;

		/** Used to mask off the bits that have been used to lookup the indirect table */
		uint64 PoolMask;

		// PageSize dependent constants
		uint64 MaxHashBuckets;
	};

	FPtrToPoolMapping PtrToPoolMapping;

	// Pool tables for different pool sizes
	FPoolTable SmallPoolTables[BINNED2_SMALL_POOL_COUNT];

	PoolHashBucket* HashBuckets;
	PoolHashBucket* HashBucketFreeList;
	uint64 NumPoolsPerPage;

	TCachedOSPageAllocator<BINNED2_MAX_CACHED_OS_FREES, BINNED2_MAX_CACHED_OS_FREES_BYTE_LIMIT> CachedOSPageAllocator;

	FCriticalSection Mutex;

	FORCEINLINE static bool IsOSAllocation(const void* Ptr)
	{
		return IsAligned(Ptr, BINNED2_LARGE_ALLOC);
	}

	struct FBundleNode
	{
		FBundleNode* NextNodeInCurrentBundle;
		union
		{
			FBundleNode* NextBundle;
			int32 Count;
		};
	};

	struct FBundle
	{
		FORCEINLINE FBundle()
		{
			Reset();
		}

		FORCEINLINE void Reset()
		{
			Head = nullptr;
			Count = 0;
		}

		FORCEINLINE void PushHead(FBundleNode* Node)
		{
			Node->NextNodeInCurrentBundle = Head;
			Node->NextBundle = nullptr;
			Head = Node;
			Count++;
		}

		FORCEINLINE FBundleNode* PopHead()
		{
			FBundleNode* Result = Head;

			Count--;
			Head = Head->NextNodeInCurrentBundle;
			return Result;
		}

		FBundleNode* Head;
		uint32       Count;
	};
	static_assert(sizeof(FBundleNode) <= BINNED2_MINIMUM_ALIGNMENT, "Bundle nodes must fit into the smallest block size");

	struct FFreeBlockList
	{
		// return true if we actually pushed it
		FORCEINLINE bool PushToFront(void* InPtr, uint32 InPoolIndex, uint32 InBlockSize)
		{
			checkSlow(InPtr);

			if (PartialBundle.Count >= (uint32)GMallocBinned2BundleCount || PartialBundle.Count * InBlockSize >= (uint32)GMallocBinned2BundleSize)
			{
				if (FullBundle.Head)
				{
					return false;
				}
				FullBundle = PartialBundle;
				PartialBundle.Reset();
			}
			PartialBundle.PushHead((FBundleNode*)InPtr);
			return true;
		}
		FORCEINLINE bool CanPushToFront(uint32 InPoolIndex, uint32 InBlockSize)
		{
			if (FullBundle.Head && (PartialBundle.Count >= (uint32)GMallocBinned2BundleCount || PartialBundle.Count * InBlockSize >= (uint32)GMallocBinned2BundleSize))
			{
				return false;
			}
			return true;
		}
		FORCEINLINE void* PopFromFront(uint32 InPoolIndex)
		{
			if (!PartialBundle.Head)
			{
				if (FullBundle.Head)
				{
					PartialBundle = FullBundle;
					FullBundle.Reset();
				}
			}
			return PartialBundle.Head ? PartialBundle.PopHead() : nullptr;
		}

		// tries to recycle the full bundle, if that fails, it is returned for freeing
		FBundleNode* RecyleFull(uint32 InPoolIndex);
		bool ObtainPartial(uint32 InPoolIndex);
		FBundleNode* PopBundles(uint32 InPoolIndex);
	private:
		FBundle PartialBundle;
		FBundle FullBundle;
	};

	struct FPerThreadFreeBlockLists
	{
		FORCEINLINE static FPerThreadFreeBlockLists* Get()
		{
			return FMallocBinned2::Binned2TlsSlot ? (FPerThreadFreeBlockLists*)FPlatformTLS::GetTlsValue(FMallocBinned2::Binned2TlsSlot) : nullptr;
		}
		static void SetTLS();
		static void ClearTLS();

		FORCEINLINE void* Malloc(uint32 InPoolIndex)
		{
			return FreeLists[InPoolIndex].PopFromFront(InPoolIndex);
		}
		// return true if the pointer was pushed
		FORCEINLINE bool Free(void* InPtr, uint32 InPoolIndex, uint32 InBlockSize)
		{
			return FreeLists[InPoolIndex].PushToFront(InPtr, InPoolIndex, InBlockSize);
		}		
		// return true if a pointer can be pushed
		FORCEINLINE bool CanFree(uint32 InPoolIndex, uint32 InBlockSize)
		{
			return FreeLists[InPoolIndex].CanPushToFront(InPoolIndex, InBlockSize);
		}
		// returns a bundle that needs to be freed if it can't be recycled
		FBundleNode* RecycleFullBundle(uint32 InPoolIndex)
		{
			return FreeLists[InPoolIndex].RecyleFull(InPoolIndex);
		}
		// returns true if we have anything to pop
		bool ObtainRecycledPartial(uint32 InPoolIndex)
		{
			return FreeLists[InPoolIndex].ObtainPartial(InPoolIndex);
		}
		FBundleNode* PopBundles(uint32 InPoolIndex)
		{
			return FreeLists[InPoolIndex].PopBundles(InPoolIndex);
		}
	private:
		FFreeBlockList FreeLists[BINNED2_SMALL_POOL_COUNT];
	};

	static FORCEINLINE FFreeBlock* GetPoolHeaderFromPointer(void* Ptr)
	{
		return (FFreeBlock*)AlignDown(Ptr, BINNED2_LARGE_ALLOC);
	}

public:


	FMallocBinned2();

	virtual ~FMallocBinned2();

	// FMalloc interface.
	virtual bool IsInternallyThreadSafe() const override;
	FORCEINLINE virtual void* Malloc(SIZE_T Size, uint32 Alignment) override
	{
		// Only allocate from the small pools if the size is small enough and the alignment isn't crazy large.
		// With large alignments, we'll waste a lot of memory allocating an entire page, but such alignments are highly unlikely in practice.
		if ((Size <= BINNED2_MAX_SMALL_POOL_SIZE) & (Alignment <= BINNED2_MINIMUM_ALIGNMENT)) // one branch, not two
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned2PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists)
			{
				if (void* Result = Lists->Malloc(BoundSizeToPoolIndex(Size)))
				{
					return Result;
				}
			}
		}
		return MallocExternal(Size, Alignment);
	}

	FORCEINLINE virtual void* Realloc(void* Ptr, SIZE_T NewSize, uint32 Alignment) override
	{
		if (NewSize <= BINNED2_MAX_SMALL_POOL_SIZE && Alignment <= BINNED2_MINIMUM_ALIGNMENT) // one branch, not two
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned2PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists && (!Ptr || !IsOSAllocation(Ptr)))
			{
				uint32 BlockSize = 0;
				uint32 PoolIndex = 0;

				bool bCanFree = true; // the nullptr is always "freeable"
				if (Ptr)
				{
					// Reallocate to a smaller/bigger pool if necessary
					FFreeBlock* Free = GetPoolHeaderFromPointer(Ptr);
					BlockSize = Free->BlockSize;
					PoolIndex = Free->PoolIndex;
					bCanFree = Free->IsCanaryOk();
					if (NewSize && bCanFree && NewSize <= BlockSize && (PoolIndex == 0 || NewSize > PoolIndexToBlockSize(PoolIndex - 1)))
					{
						return Ptr;
					}
					bCanFree = bCanFree && Lists->CanFree(PoolIndex, BlockSize);
				}
				if (bCanFree)
				{
					void* Result = NewSize ? Lists->Malloc(BoundSizeToPoolIndex(NewSize)) : nullptr;
					if (Result || !NewSize)
					{
						if (Result && Ptr)
						{
							FMemory::Memcpy(Result, Ptr, FPlatformMath::Min<SIZE_T>(NewSize, BlockSize));
						}
						if (Ptr)
						{
							bool bDidPush = Lists->Free(Ptr, PoolIndex, BlockSize);
							checkSlow(bDidPush);
						}
						return Result;
					}
				}
			}
		}
		return ReallocExternal(Ptr, NewSize, Alignment);
	}

	FORCEINLINE virtual void Free(void* Ptr) override
	{
		if (!IsOSAllocation(Ptr))
		{
			FPerThreadFreeBlockLists* Lists = GMallocBinned2PerThreadCaches ? FPerThreadFreeBlockLists::Get() : nullptr;
			if (Lists)
			{
				FFreeBlock* BasePtr = GetPoolHeaderFromPointer(Ptr);
				if (BasePtr->IsCanaryOk() && Lists->Free(Ptr, BasePtr->PoolIndex, BasePtr->BlockSize))
				{
					return;
				}
			}
		}
		FreeExternal(Ptr);
	}

	FORCEINLINE virtual bool GetAllocationSize(void *Ptr, SIZE_T &SizeOut) override
	{
		if (!IsOSAllocation(Ptr))
		{
			const FFreeBlock* Free = GetPoolHeaderFromPointer(Ptr);
			if (Free->IsCanaryOk())
			{
				SizeOut = Free->BlockSize;
				return true;
			}
		}
		return GetAllocationSizeExternal(Ptr, SizeOut);
	}

	FORCEINLINE virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment) override
	{
		static_assert(DEFAULT_ALIGNMENT <= BINNED2_MINIMUM_ALIGNMENT, "DEFAULT_ALIGNMENT is assumed to be zero"); // used below
		checkSlow((Alignment & (Alignment - 1)) == 0); // Check the alignment is a power of two
		SIZE_T SizeOut;
		if ((Count <= BINNED2_MAX_SMALL_POOL_SIZE) & (Alignment <= BINNED2_MINIMUM_ALIGNMENT)) // one branch, not two
		{
			SizeOut = PoolIndexToBlockSize(BoundSizeToPoolIndex(Count));
		}
		else
		{
			Alignment = FPlatformMath::Max<uint32>(Alignment, OsAllocationGranularity);
			checkSlow(Alignment <= PageSize);
			SizeOut = Align(Count, Alignment);
		}
		check(SizeOut >= Count);
		return SizeOut;
	}

	virtual bool ValidateHeap() override;
	virtual void Trim() override;
	virtual void SetupTLSCachesOnCurrentThread() override;
	virtual void ClearAndDisableTLSCachesOnCurrentThread() override;
	virtual const TCHAR* GetDescriptiveName() override;
	// End FMalloc interface.

	void FlushCurrentThreadCache();
	void* MallocExternal(SIZE_T Size, uint32 Alignment);
	void* ReallocExternal(void* Ptr, SIZE_T NewSize, uint32 Alignment);
	void FreeExternal(void *Ptr);
	bool GetAllocationSizeExternal(void* Ptr, SIZE_T& SizeOut);

	static uint16 SmallBlockSizesReversed[BINNED2_SMALL_POOL_COUNT]; // this is reversed to get the smallest elements on our main cache line
	static FMallocBinned2* MallocBinned2;
	static uint32 Binned2TlsSlot;
	static uint32 PageSize;
	static uint32 OsAllocationGranularity;
	// Mapping of sizes to small table indices
	static uint8 MemSizeToIndex[1 + (BINNED2_MAX_SMALL_POOL_SIZE >> BINNED2_MINIMUM_ALIGNMENT_SHIFT)];

	FORCEINLINE uint32 BoundSizeToPoolIndex(SIZE_T Size) 
	{
		auto Index = ((Size + BINNED2_MINIMUM_ALIGNMENT - 1) >> BINNED2_MINIMUM_ALIGNMENT_SHIFT);
		checkSlow(Index >= 0 && Index <= (BINNED2_MAX_SMALL_POOL_SIZE >> BINNED2_MINIMUM_ALIGNMENT_SHIFT)); // and it should be in the table
		uint32 PoolIndex = uint32(MemSizeToIndex[Index]);
		checkSlow(PoolIndex >= 0 && PoolIndex < BINNED2_SMALL_POOL_COUNT);
		return PoolIndex;
	}
	FORCEINLINE uint32 PoolIndexToBlockSize(uint32 PoolIndex)
	{
		return SmallBlockSizesReversed[BINNED2_SMALL_POOL_COUNT - PoolIndex - 1];
	}
};

#define BINNED2_INLINE (1)
#if BINNED2_INLINE // during development, it helps with iteration time to not include these here, but rather in the .cpp
	#if PLATFORM_USES_FIXED_GMalloc_CLASS && !FORCE_ANSI_ALLOCATOR && USE_MALLOC_BINNED2
		#define FMEMORY_INLINE_FUNCTION_DECORATOR  FORCEINLINE
		#define FMEMORY_INLINE_GMalloc (FMallocBinned2::MallocBinned2)
		#include "FMemory.inl"
	#endif
#endif
