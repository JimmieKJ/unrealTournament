// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Allocation.h: A Collection of allocators
=============================================================================*/

#pragma once
#include "D3D12Resources.h"

class FD3D12ResourceAllocator : public FD3D12DeviceChild
{
public:

	FD3D12ResourceAllocator(FD3D12Device* ParentDevice,
		FString Name,
		D3D12_HEAP_TYPE heapType,
		D3D12_RESOURCE_FLAGS flags,
		uint32 MaxSizeForPooling);

	~FD3D12ResourceAllocator();

	virtual FD3D12ResourceBlockInfo* TryAllocate(uint32 SizeInBytes, uint32 Alignment) = 0;

	virtual void Deallocate(FD3D12ResourceBlockInfo* Block) = 0;

	virtual void Initialize() = 0;

	virtual void Destroy() = 0;

	virtual void Reset() = 0;

	virtual void CleanUpAllocations() = 0;

	virtual void DumpAllocatorStats(class FOutputDevice& Ar) = 0;

	virtual void ReleaseAllResources() = 0;

	inline bool IsOwner(const FD3D12ResourceBlockInfo &block)
	{
		return block.Allocator == this;
	}

	// Any allocation larger than this just gets straight up allocated (i.e. not pooled).
	// These large allocations should be infrequent so the CPU overhead should be minimal
	const uint32 MaximumAllocationSizeForPooling;
	D3D12_RESOURCE_FLAGS ResourceFlags;

	void InitializeDefaultBuffer(FD3D12Resource* Destination, uint64 DestinationOffset, const void* Data, uint64 DataSize);

protected:

	const FString DebugName;

	bool Initialized;

	const D3D12_HEAP_TYPE HeapType;

	FCriticalSection CS;

#if defined(UE_BUILD_DEBUG)
	uint32 SpaceUsed;
	uint32 InternalFragmentation;
	uint32 NumBlocksInDeferredDeletionQueue;
	uint32 PeakUsage;
	uint32 FailedAllocationSpace;
#endif
};

//-----------------------------------------------------------------------------
//	Buddy Allocator
//-----------------------------------------------------------------------------
// Allocates blocks from a fixed range using buddy allocation method.
// Buddy allocation allows reasonably fast allocation of arbitrary size blocks
// with minimal fragmentation and provides efficient reuse of freed ranges.
// When a block is de-allocated an attempt is made to merge it with it's 
// neighbour (buddy) if it is contiguous and free.
// Based on reference implementation by MSFT: billkris

// Unfortunately the api restricts the minimum size of a placed buffer resource to 64k
#define MIN_PLACED_BUFFER_SIZE (64 * 1024)
#define D3D_BUFFER_ALIGNMENT (64 * 1024)

#if defined(UE_BUILD_DEBUG)
#define INCREASE_ALLOC_COUNTER(A, B) (A = A + B);
#define DECREASE_ALLOC_COUNTER(A, B) (A = A - B);
#else
#define INCREASE_ALLOC_COUNTER(A, B)
#define DECREASE_ALLOC_COUNTER(A, B)
#endif

enum eBuddyAllocationStrategy
{
	// This strategy uses Placed Resources to sub-allocate a buffer out of an underlying ID3D12Heap.
	// The benefit of this is that each buffer can have it's own resource state and can be treated
	// as any other buffer. The downside of this strategy is the API limitiation which enforces
	// the minimum buffer size to 64k leading to large internal fragmentation in the allocator
	kPlacedResourceStrategy,
	// The alternative is to manualy sub-allocate out of a single large buffer which allows block
	// allocation granularity down to 1 byte. However, this strategy is only really valid for buffers which
	// will be treated as read-only after their creation (i.e. most Index and Vertex buffers). This 
	// is because the underlying resource can only have one state at a time.
	kManualSubAllocationStrategy
};

class FD3D12BuddyAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12BuddyAllocator(FD3D12Device* ParentDevice,
		FString Name,
		eBuddyAllocationStrategy allocationStrategy,
		D3D12_HEAP_TYPE heapType,
		D3D12_HEAP_FLAGS heapFlags,
		D3D12_RESOURCE_FLAGS flags,
		uint32 MaxSizeForPooling,
		uint32 maxBlockSize,
		uint32 minBlockSize = MIN_PLACED_BUFFER_SIZE);

	// Public Override Functions
	virtual FD3D12ResourceBlockInfo* TryAllocate(uint32 SizeInBytes, uint32 Alignment) final override;

	virtual void Deallocate(FD3D12ResourceBlockInfo* Block) final override;

	virtual void Initialize() final override;

	virtual void Destroy() final override;

	virtual void CleanUpAllocations() final override;

	virtual void DumpAllocatorStats(class FOutputDevice& Ar) final override;

	virtual void ReleaseAllResources() final override;

	virtual void Reset() final override;

	inline bool IsEmpty()
	{
		return FreeBlocks[MaxOrder].Num() == 1;
	}

	inline uint32 GetTotalSizeUsed() const { return TotalSizeUsed; }

protected:
	const uint32 MaxBlockSize;
	const uint32 MinBlockSize;
	const D3D12_HEAP_FLAGS HeapFlags;
	const eBuddyAllocationStrategy AllocationStrategy;

	TRefCountPtr<FD3D12Resource> BackingResource;
	TRefCountPtr<ID3D12Heap> BackingHeap;

	void* BaseAddress;

private:

	TArray<FD3D12ResourceBlockInfo*> DeferredDeletionQueue;
	TArray<TSet<uint32>> FreeBlocks;
	uint32 MaxOrder;
	uint32 TotalSizeUsed;

	bool HeapFullMessageDisplayed;

	inline uint32 SizeToUnitSize(uint32 size) const
	{
		return (size + (MinBlockSize - 1)) / MinBlockSize;
	}

	inline uint32 UnitSizeToOrder(uint32 size) const
	{
		return uint32(ceil(log2f(float(size))));
	}

	inline uint32 GetBuddyOffset(const uint32 &offset, const uint32 &size)
	{
		return offset ^ size;
	}

	uint32 OrderToUnitSize(uint32 order) const { return ((uint32)1) << order; }
	uint32 AllocateBlock(uint32 order);
	void DeallocateBlock(uint32 offset, uint32 order);

	bool CanAllocate(uint32 size);

	void DeallocateInternal(FD3D12ResourceBlockInfo* Block);

	FD3D12ResourceBlockInfo* Allocate(uint32 SizeInBytes, uint32 Alignment, const void* InitialData = nullptr);
};

//-----------------------------------------------------------------------------
//	Multi-Buddy Allocator
//-----------------------------------------------------------------------------
// Builds on top of the Buddy Allocator but covers some of it's deficiencies by
// managing multiple buddy allocator instances to better match memory usage over
// time.

class FD3D12MultiBuddyAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12MultiBuddyAllocator(FD3D12Device* ParentDevice,
		FString Name,
		eBuddyAllocationStrategy allocationStrategyIn,
		D3D12_HEAP_TYPE heapType,
		D3D12_HEAP_FLAGS heapFlagsIn,
		D3D12_RESOURCE_FLAGS flags,
		uint32 MaxSizeForPooling,
		uint32 maxBlockSizeIn,
		uint32 minBlockSizeIn = MIN_PLACED_BUFFER_SIZE);

	// Public Override Functions
	virtual FD3D12ResourceBlockInfo* TryAllocate(uint32 SizeInBytes, uint32 Alignment) final override;

	virtual void Deallocate(FD3D12ResourceBlockInfo* Block) final override;

	virtual void Initialize() final override;

	virtual void Destroy() final override;

	virtual void CleanUpAllocations() final override;

	virtual void DumpAllocatorStats(class FOutputDevice& Ar) final override;

	virtual void ReleaseAllResources() final override;

	virtual void Reset() final override;

private:
	const eBuddyAllocationStrategy AllocationStrategy;
	const D3D12_HEAP_FLAGS HeapFlags;
	const uint32 MaxBlockSize;
	const uint32 MinBlockSize;

	FD3D12BuddyAllocator* CreateNewAllocator();

	TArray<FD3D12BuddyAllocator*> Allocators;
};

//-----------------------------------------------------------------------------
//	Linear Allocator
//-----------------------------------------------------------------------------
// A very simple allocator which sub-allocates out of pages. Allocations are not re-used thus
// fragmentation can occur when allocating resources with different life-spans but packs data 
// well when they are i.e. Allocate all the static meshes for 1 level
struct FD3D12LinearAllocatorPage
{
	FD3D12LinearAllocatorPage() : 
		Resource(nullptr), DeallocatedSpace(0), CurrentOffset(0), BaseAddress(nullptr){};
	FD3D12Resource* Resource;

	uint32 CurrentOffset;
	uint32 DeallocatedSpace;
	void* BaseAddress;
};

class FD3D12LinearAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12LinearAllocator(FD3D12Device* ParentDevice,
		FString Name,
		D3D12_HEAP_TYPE heapType,
		D3D12_RESOURCE_FLAGS flags,
		uint32 MaxSizeForPooling,
		uint32 PageSize);

	// Public Override Functions

	virtual FD3D12ResourceBlockInfo* TryAllocate(uint32 SizeInBytes, uint32 Alignment) final override;

	virtual void Deallocate(FD3D12ResourceBlockInfo* Block)  final override;

	virtual void Initialize() final override;

	virtual void Destroy() final override;

	virtual void CleanUpAllocations() final override;

	virtual void DumpAllocatorStats(class FOutputDevice& Ar) final override;

	virtual void ReleaseAllResources() final override;

	virtual void Reset() final override;

protected:

	const uint32 AllocationPageSize;

private:

	FD3D12LinearAllocatorPage* FindPageWithBestFit(uint32 AllocationSize, uint32 Alignment);

	struct FD3D12PageSubAllocation
	{
		FD3D12LinearAllocatorPage* Parent;
	};
	
	FD3D12LinearAllocatorPage* AllocatePage();
	void DestroyPage(FD3D12LinearAllocatorPage& Page);

	TArray<FD3D12LinearAllocatorPage*> ActivePages;
};

//-----------------------------------------------------------------------------
//	Bucket Allocator
//-----------------------------------------------------------------------------
// Resources are allocated from buckets, which are just a collection of resources of a particular size.
// Blocks can be an entire resource or a sub allocation from a resource.
class FD3D12BucketAllocator : public FD3D12ResourceAllocator
{
public:

	FD3D12BucketAllocator(FD3D12Device* ParentDevice,
		FString Name,
		D3D12_HEAP_TYPE heapType,
		D3D12_RESOURCE_FLAGS flags,
		uint64 BlockRetentionFrameCountIn);

	// Public Override Functions

	virtual FD3D12ResourceBlockInfo* TryAllocate(uint32 SizeInBytes, uint32 Alignment) final override;

	virtual void Deallocate(FD3D12ResourceBlockInfo* Block)  final override;

	virtual void Initialize() final override;

	virtual void Destroy() final override;

	virtual void CleanUpAllocations() final override;

	virtual void DumpAllocatorStats(class FOutputDevice& Ar) final override;

	virtual void ReleaseAllResources() final override;

	virtual void Reset() final override;

private:

	static uint32 FORCEINLINE BucketFromSize(uint32 size, uint32 bucketShift)
	{
		uint32 bucket = FMath::CeilLogTwo(size);
		bucket = bucket < bucketShift ? 0 : bucket - bucketShift;
		return bucket;
	}

	static uint32 FORCEINLINE BlockSizeFromBufferSize(uint32 bufferSize, uint32 bucketShift)
	{
		const uint32 minSize = 1 << bucketShift;
		return bufferSize > minSize ? FMath::RoundUpToPowerOfTwo(bufferSize) : minSize;
	}

#if SUB_ALLOCATED_DEFAULT_ALLOCATIONS
	static const uint32 MIN_HEAP_SIZE = 256 * 1024;
#else
	static const uint32 MIN_HEAP_SIZE = 64 * 1024;
#endif

	static const uint32 BucketShift = 6;
	static const uint32 NumBuckets = 22; // bucket resource sizes range from 64 to 2^28 
	FThreadsafeQueue<FD3D12ResourceBlockInfo*> AvailableBlocks[NumBuckets];
	FThreadsafeQueue<FD3D12ResourceBlockInfo*> ExpiredBlocks;
	TArray<FD3D12Resource*> SubAllocatedResources;// keep a list of the sub-allocated resources so that they may be cleaned up

	// This frame count value helps makes sure that we don't delete resources too soon. If resources are deleted too soon,
	// we can get in a loop the heap allocator will be constantly deleting and creating resources every frame which
	// results in CPU stutters. DynamicRetentionFrameCount was tested and set to a value that appears to be adequate for
	// creating a stable state on the Infiltrator demo.
	const uint64 BlockRetentionFrameCount;
};


//-----------------------------------------------------------------------------
//	FD3D12DynamicHeapAllocator
//-----------------------------------------------------------------------------
// This is designed for allocation of scratch memory such as temporary staging buffers
// or shadow buffers for dynamic resources.
class FD3D12DynamicHeapAllocator : public FD3D12DeviceChild
{
public:

	FD3D12DynamicHeapAllocator(FD3D12Device* InParent, FString Name, eBuddyAllocationStrategy allocationStrategy,
		uint32 MaxSizeForPooling,
		uint32 maxBlockSize,
		uint32 minBlockSize);

	// These are mutually exclusive calls!
	inline void SetCurrentCommandContext(class FD3D12CommandContext* CmdContext)
	{
		CurrentCommandContext = CmdContext;
		check(CurrentCommandListHandle == nullptr);
	}

	inline void SetCurrentCommandListHandle(FD3D12CommandListHandle CommandListHandle)
	{
		check(CurrentCommandContext == nullptr);
		CurrentCommandListHandle = CommandListHandle;
	}

	// Allocates <size> bytes from the end of an available resource heap.
	void* AllocUploadResource(uint32 size, uint32 alignment, class FD3D12ResourceLocation* ResourceLocation);

	void CleanUpAllocations();

	void Destroy();

private:
	// The currently paired command context.  If none is available (as with async texture loads),
	// the current command list handle can be provided instead.  The command list handle is the
	// critical part, but the command context can always return the current list handle.
	// I.e. you need one but not both of these to be valid
	class FD3D12CommandContext* CurrentCommandContext;
	FD3D12CommandListHandle CurrentCommandListHandle;

	FD3D12MultiBuddyAllocator* Allocator;
};

//-----------------------------------------------------------------------------
//	FD3D12DefaultBufferPool
//-----------------------------------------------------------------------------
class FD3D12DefaultBufferPool : public FD3D12DeviceChild
{
public:
	FD3D12DefaultBufferPool(FD3D12Device* InParent, FD3D12ResourceAllocator* AllocatorIn);
	~FD3D12DefaultBufferPool() { delete Allocator; }

	// Grab a buffer from the available buffers or create a new buffer if none are available
	void AllocDefaultResource(const D3D12_RESOURCE_DESC& Desc, D3D12_SUBRESOURCE_DATA* pInitialData, class FD3D12ResourceLocation* ResourceLocation, uint32 Alignment);

	void CleanUpAllocations();

private:
	FD3D12ResourceAllocator* Allocator;
};

// FD3D12DefaultBufferAllocator
//
class FD3D12DefaultBufferAllocator : public FD3D12DeviceChild
{
public:
	FD3D12DefaultBufferAllocator(FD3D12Device* InParent) :
		FD3D12DeviceChild(InParent)
	{
		FMemory::Memset(DefaultBufferPools, 0);
	}

	// Grab a buffer from the available buffers or create a new buffer if none are available
	HRESULT AllocDefaultResource(const D3D12_RESOURCE_DESC& pDesc, D3D12_SUBRESOURCE_DATA* InitialData, class FD3D12ResourceLocation* ResourceLocation, uint32 Alignment);
	void FreeDefaultBufferPools();
	void CleanupFreeBlocks();

private:

	static const uint32 MAX_DEFAULT_POOLS = 16; // Should match the max D3D12_RESOURCE_FLAG_FLAG combinations.
	FD3D12DefaultBufferPool* DefaultBufferPools[MAX_DEFAULT_POOLS];

	bool BufferIsWriteable(const D3D12_RESOURCE_DESC& Desc)
	{
		const bool bDSV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;
		const bool bRTV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0;
		const bool bUAV = (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0;

		// Buffer Depth Stencils are invalid
		check(bDSV == false);
		const bool bWriteable = bDSV || bRTV || bUAV;
		return bWriteable;
	}
};

//-----------------------------------------------------------------------------
//	FD3D12TextureAllocator
//-----------------------------------------------------------------------------

class FD3D12TextureAllocator : public FD3D12BuddyAllocator
{
public:
	FD3D12TextureAllocator(FD3D12Device* Device, FString Name, uint32 HeapSize, D3D12_HEAP_FLAGS Flags);

	~FD3D12TextureAllocator();

	HRESULT AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, FD3D12ResourceLocation* TextureLocation);
};

#define TEXTURE_POOL_SIZE_READABLE (64 * 1024 * 1024)

class FD3D12TextureAllocatorPool : public FD3D12DeviceChild
{
public:
	FD3D12TextureAllocatorPool(FD3D12Device* Device) :
		ReadOnlyTexturePool(Device, FString(L"Small Read-Only Texture allocator"), TEXTURE_POOL_SIZE_READABLE, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES),
		FD3D12DeviceChild(Device)
	{};

	HRESULT AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, uint8 UEFormat, FD3D12ResourceLocation* TextureLocation);

	void CleanUpAllocations() { ReadOnlyTexturePool.CleanUpAllocations(); }

	void Destroy() { ReadOnlyTexturePool.Destroy(); }

private:
	FD3D12TextureAllocator ReadOnlyTexturePool;
};