// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Implementation of Memory Allocation Strategies

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"
#include "D3D12Allocation.h"


//-----------------------------------------------------------------------------
//	Allocator Base
//-----------------------------------------------------------------------------
FD3D12ResourceAllocator::FD3D12ResourceAllocator(FD3D12Device* ParentDevice, FString Name, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_FLAGS flags, uint32 MaxSizeForPooling)
	: DebugName(Name)
	, HeapType(heapType)
	, ResourceFlags(flags)
	, Initialized(false)
	, MaximumAllocationSizeForPooling(MaxSizeForPooling)
#if defined(UE_BUILD_DEBUG)
	, PeakUsage(0)
	, SpaceUsed(0)
	, InternalFragmentation(0)
	, NumBlocksInDeferredDeletionQueue(0)
	, FailedAllocationSpace(0)
#endif
	, FD3D12DeviceChild(ParentDevice)
{
}

FD3D12ResourceAllocator::~FD3D12ResourceAllocator()
{
}

void FD3D12ResourceAllocator::InitializeDefaultBuffer(FD3D12Device* Device, FD3D12Resource* Destination, uint64 DestinationOffset, const void* Data, uint64 DataSize)
{
	FD3D12CommandListHandle& hCommandList = Device->GetDefaultCommandContext().CommandListHandle;
	// Queue up a CopySubresource() to handle initial data...

	// Get an upload heap and initialize data
	FD3D12ResourceLocation SrcResourceLoc(Device);
	void* pData = Device->GetBufferInitFastAllocator().Allocate(DataSize, 4UL, &SrcResourceLoc);
	check(pData);
	FMemory::Memcpy(pData, Data, DataSize);

	// Copy from the temporary upload heap to the default resource
	{
		// Writable structured bufferes are sometimes initialized with inital data which means they sometimes need tracking.
		FConditionalScopeResourceBarrier ConditionalScopeResourceBarrier(hCommandList, Destination, D3D12_RESOURCE_STATE_COPY_DEST, 0);

		Device->GetDefaultCommandContext().numCopies++;
		hCommandList->CopyBufferRegion(
			Destination->GetResource(),
			DestinationOffset,
			SrcResourceLoc.GetResource()->GetResource(),
			SrcResourceLoc.GetOffset(), DataSize);

		hCommandList.UpdateResidency(Destination);
	}
}

//-----------------------------------------------------------------------------
//	Buddy Allocator
//-----------------------------------------------------------------------------

FD3D12BuddyAllocator::FD3D12BuddyAllocator(FD3D12Device* ParentDevice, FString Name, eBuddyAllocationStrategy allocationStrategy, D3D12_HEAP_TYPE heapType,
	D3D12_HEAP_FLAGS heapFlags, D3D12_RESOURCE_FLAGS flags, uint32 MaxSizeForPooling, uint32 maxBlockSize, uint32 MinBlockSize)
	: AllocationStrategy(allocationStrategy)
	, MaxBlockSize(maxBlockSize)
	, MinBlockSize(MinBlockSize)
	, HeapFlags(heapFlags)
	, BackingHeap(nullptr)
	, BaseAddress(nullptr)
	, HeapFullMessageDisplayed(false)
	, TotalSizeUsed(0)
	, FD3D12ResourceAllocator(ParentDevice, Name, heapType, flags, MaxSizeForPooling)
{
	// maxBlockSize should be evenly dividable by MinBlockSize and  
	// maxBlockSize / MinBlockSize should be a power of two  
	check((maxBlockSize / MinBlockSize) * MinBlockSize == maxBlockSize); // Evenly dividable  
	check(0 == ((maxBlockSize / MinBlockSize) & ((maxBlockSize / MinBlockSize) - 1))); // Power of two  

	MaxOrder = UnitSizeToOrder(SizeToUnitSize(maxBlockSize));

	Reset();
}

void FD3D12BuddyAllocator::Initialize()
{
	if (AllocationStrategy == eBuddyAllocationStrategy::kPlacedResourceStrategy)
	{
		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(HeapType);

		D3D12_HEAP_DESC desc = {};
		desc.SizeInBytes = MaxBlockSize;
		desc.Properties = heapProps;
		desc.Alignment = 0;
		desc.Flags = HeapFlags;

		ID3D12Heap* Heap = nullptr;
		VERIFYD3D12RESULT(GetParentDevice()->GetDevice()->CreateHeap(&desc, IID_PPV_ARGS(&Heap)));
		SetName(Heap, L"Placed Resource Allocator Backing Heap");

		BackingHeap = new FD3D12Heap(GetParentDevice());
		BackingHeap->SetHeap(Heap);

		if (IsCPUWritable(HeapType) == false)
		{
			BackingHeap->BeginTrackingResidency(desc.SizeInBytes);
		}
	}
	else
	{
		VERIFYD3D12RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(HeapType, MaxBlockSize, BackingResource.GetInitReference(), ResourceFlags));
		SetName(BackingResource, L"Resource Allocator Underlying Buffer");

		if (IsCPUWritable(HeapType))
		{
			BackingResource->GetResource()->Map(0, nullptr, &BaseAddress);
		}
	}
}

void FD3D12BuddyAllocator::Destroy()
{
	ReleaseAllResources();
}

uint32 FD3D12BuddyAllocator::AllocateBlock(uint32 order)
{
	uint32 offset;

	if (order > MaxOrder)
	{
		check(false); // Can't allocate a block that large  
	}

	if (FreeBlocks[order].Num() == 0)
	{
		// No free nodes in the requested pool.  Try to find a higher-order block and split it.  
		uint32 left = AllocateBlock(order + 1);

		uint32 size = OrderToUnitSize(order);

		uint32 right = left + size;

		FreeBlocks[order].Add(right); // Add the right block to the free pool  

		offset = left; // Return the left block  
	}

	else
	{
		TSet<uint32>::TConstIterator it(FreeBlocks[order]);
		offset = *it;

		// Remove the block from the free list
		FreeBlocks[order].Remove(*it);
	}

	return offset;
}

void FD3D12BuddyAllocator::DeallocateBlock(uint32 offset, uint32 order)
{
	// See if the buddy block is free  
	uint32 size = OrderToUnitSize(order);

	uint32 buddy = GetBuddyOffset(offset, size);

	uint32* it = FreeBlocks[order].Find(buddy);

	if (it != nullptr)
	{
		// Deallocate merged blocks
		DeallocateBlock(FMath::Min(offset, buddy), order + 1);
		// Remove the buddy from the free list  
		FreeBlocks[order].Remove(*it);
	}
	else
	{
		// Add the block to the free list
		FreeBlocks[order].Add(offset);
	}
}

FD3D12ResourceBlockInfo* FD3D12BuddyAllocator::Allocate(uint32 SizeInBytes, uint32 Alignment, const void* InitialData)
{
	FScopeLock Lock(&CS);

	if (Initialized == false)
	{
		Initialize();
		Initialized = true;
	}

	uint32 sizeToAllocate = SizeInBytes;

	// If the alignment doesn't match the block size
	if (Alignment != 0 && MinBlockSize % Alignment != 0)
	{
		sizeToAllocate = SizeInBytes + Alignment;
	}

	// Work out what size block is needed and allocate one
	uint32 unitSize = SizeToUnitSize(sizeToAllocate);
	uint32 order = UnitSizeToOrder(unitSize);
	uint32 offset = AllocateBlock(order); // This is the offset in MinBlockSize units

	FD3D12ResourceBlockInfo::AllocationTrackingValues alloc = {};
	alloc.Size = uint32(OrderToUnitSize(order) * MinBlockSize);
	alloc.AllocationBlockOffset = uint32(offset * MinBlockSize);
	alloc.UnpaddedSize = SizeInBytes;

	if (Alignment != 0 && alloc.AllocationBlockOffset % Alignment != 0)
	{
		const uint64 originalAlignment = alloc.AllocationBlockOffset;
		alloc.AllocationBlockOffset = AlignArbitrary(alloc.AllocationBlockOffset, Alignment);
		check((alloc.AllocationBlockOffset - originalAlignment + SizeInBytes) <= alloc.Size)
	}

	check(Alignment == 0 || alloc.AllocationBlockOffset % Alignment == 0);

	INCREASE_ALLOC_COUNTER(SpaceUsed, alloc.Size);
	INCREASE_ALLOC_COUNTER(InternalFragmentation, (alloc.Size - alloc.UnpaddedSize));

	TotalSizeUsed += alloc.Size;

#if defined(UE_BUILD_DEBUG)
	if (SpaceUsed > PeakUsage)
	{
		PeakUsage = SpaceUsed;
	}
#endif
	FD3D12ResourceBlockInfo* Block = new FD3D12ResourceBlockInfo(this, alloc.AllocationBlockOffset, alloc);

	if (AllocationStrategy == eBuddyAllocationStrategy::kManualSubAllocationStrategy)
	{
		Block->ResourceHeap = BackingResource;

		if (IsCPUWritable(HeapType))
		{
			Block->Address = (byte*)BaseAddress + alloc.AllocationBlockOffset;
		}
	}

	if (InitialData)
	{
		InitializeDefaultBuffer(GetParentDevice(), Block->ResourceHeap, alloc.AllocationBlockOffset, InitialData, SizeInBytes);
	}

	return Block;
}

FD3D12ResourceBlockInfo * FD3D12BuddyAllocator::TryAllocate(uint32 SizeInBytes, uint32 Alignment)
{
	FScopeLock Lock(&CS);

	if (CanAllocate(SizeInBytes, Alignment))
	{
		return Allocate(SizeInBytes, Alignment);
	}
	else
	{
		INCREASE_ALLOC_COUNTER(FailedAllocationSpace, SizeInBytes);
		return nullptr;
	}
}

void FD3D12BuddyAllocator::Deallocate(FD3D12ResourceBlockInfo* Block)
{
	FScopeLock Lock(&CS);

	Block->FrameFence = GetParentDevice()->GetCommandListManager().GetFence(EFenceType::FT_Frame).GetCurrentFence();

	if (Block->IsPlacedResource)
	{
		// Blocks don't have ref counted pointers to their resources so add a ref for the deletion queue
		// (Otherwise the resource may be deleted before the GPU is finished with it)
		Block->ResourceHeap->AddRef();
	}
	DeferredDeletionQueue.Add(Block);
	INCREASE_ALLOC_COUNTER(NumBlocksInDeferredDeletionQueue, 1);
}

void FD3D12BuddyAllocator::DeallocateInternal(FD3D12ResourceBlockInfo* Block)
{
	check(IsOwner(*Block));

	uint32 offset = SizeToUnitSize(Block->GetOffset());

	uint32 size = SizeToUnitSize(Block->GetSize());

	uint32 order = UnitSizeToOrder(size);

	// Blocks are cleaned up async so need a lock

	DeallocateBlock(offset, order);

	DECREASE_ALLOC_COUNTER(SpaceUsed, Block->GetSize());
	DECREASE_ALLOC_COUNTER(InternalFragmentation, (Block->AllocatorValues.Size - Block->AllocatorValues.UnpaddedSize));

	TotalSizeUsed -= Block->AllocatorValues.Size;

	if (AllocationStrategy == eBuddyAllocationStrategy::kPlacedResourceStrategy)
	{
		// Release the resource
		Block->Destroy();
	}

	delete(Block);
};

void FD3D12BuddyAllocator::CleanUpAllocations()
{
	FScopeLock Lock(&CS);

	FD3D12CommandListManager& manager = GetParentDevice()->GetCommandListManager();

	uint32 popCount = 0;
	for (int32 i = 0; i < DeferredDeletionQueue.Num(); i++)
	{
		FD3D12ResourceBlockInfo* Block = DeferredDeletionQueue[i];

		if (manager.GetFence(FT_Frame).IsFenceComplete(Block->FrameFence))
		{
			DeallocateInternal(Block);
			DECREASE_ALLOC_COUNTER(NumBlocksInDeferredDeletionQueue, 1);
			popCount = i + 1;
		}
		else
		{
			break;
		}
	}

	if (popCount)
	{
		// clear out all of the released blocks, don't allow the array to shrink
		DeferredDeletionQueue.RemoveAt(0, popCount, false);
	}
}

void FD3D12BuddyAllocator::ReleaseAllResources()
{
	for (FD3D12ResourceBlockInfo*& Block : DeferredDeletionQueue)
	{
		DeallocateInternal(Block);
		DECREASE_ALLOC_COUNTER(NumBlocksInDeferredDeletionQueue, 1);
	}

	DeferredDeletionQueue.Empty();

	if (BackingResource)
	{
		check(BackingResource->GetRefCount() == 1);
		BackingResource = nullptr;
	}

	if (BackingHeap)
	{
		BackingHeap->Destroy();
	}
}

void FD3D12BuddyAllocator::DumpAllocatorStats(class FOutputDevice& Ar)
{
#if defined(UE_BUILD_DEBUG)
	FBufferedOutputDevice BufferedOutput;
	{
		// This is the memory tracked inside individual allocation pools
		FD3D12DynamicRHI* D3DRHI = FD3D12DynamicRHI::GetD3DRHI();
		FName categoryName(&DebugName.GetCharArray()[0]);

		BufferedOutput.CategorizedLogf(categoryName, ELogVerbosity::Log, TEXT(""));
		BufferedOutput.CategorizedLogf(categoryName, ELogVerbosity::Log, TEXT("Heap Size | MinBlock Size | Space Used | Peak Usage | Unpooled Allocations | Internal Fragmentation | Blocks in Deferred Delete Queue "));
		BufferedOutput.CategorizedLogf(categoryName, ELogVerbosity::Log, TEXT("----------"));

		BufferedOutput.CategorizedLogf(categoryName, ELogVerbosity::Log, TEXT("% 10i % 10i % 16i % 12i % 13i % 8i % 10I"),
			MaxBlockSize,
			MinBlockSize,
			SpaceUsed,
			PeakUsage,
			FailedAllocationSpace,
			InternalFragmentation,
			NumBlocksInDeferredDeletionQueue);
	}

	BufferedOutput.RedirectTo(Ar);
#endif
}

bool FD3D12BuddyAllocator::CanAllocate(uint32 size, uint32 alignment)
{
	if (TotalSizeUsed == MaxBlockSize)
	{
		return false;
	}

	uint32 sizeToAllocate = size;
	// If the alignment doesn't match the block size
	if (alignment != 0 && MinBlockSize % alignment != 0)
	{
		sizeToAllocate = size + alignment;
	}

	uint32 blockSize = MaxBlockSize;

	for (int32 i = FreeBlocks.Num() - 1; i >= 0; i--)
	{
		if (FreeBlocks[i].Num() && blockSize >= sizeToAllocate)
		{
			return true;
		}

		// Halve the block size;
		blockSize = blockSize >> 1;

		if (blockSize < sizeToAllocate) return false;
	}
	return false;
}

void FD3D12BuddyAllocator::Reset()
{
	// Clear the free blocks collection
	FreeBlocks.Empty();

	// Initialize the pool with a free inner block of max inner block size
	FreeBlocks.SetNum(MaxOrder + 1);
	FreeBlocks[MaxOrder].Add((uint32)0);
}

//-----------------------------------------------------------------------------
//	Multi-Buddy Allocator
//-----------------------------------------------------------------------------

FD3D12MultiBuddyAllocator::FD3D12MultiBuddyAllocator(FD3D12Device* ParentDevice,
	FString Name,
	eBuddyAllocationStrategy allocationStrategyIn,
	D3D12_HEAP_TYPE heapType,
	D3D12_HEAP_FLAGS heapFlagsIn,
	D3D12_RESOURCE_FLAGS flags,
	uint32 MaxSizeForPooling,
	uint32 maxBlockSizeIn,
	uint32 minBlockSizeIn) :
	AllocationStrategy(allocationStrategyIn)
	, HeapFlags(heapFlagsIn)
	, MaxBlockSize(maxBlockSizeIn)
	, MinBlockSize(minBlockSizeIn)
	, FD3D12ResourceAllocator(ParentDevice, Name, heapType, flags, MaxSizeForPooling)
{}


FD3D12ResourceBlockInfo* FD3D12MultiBuddyAllocator::TryAllocate(uint32 SizeInBytes, uint32 Alignment)
{
	FScopeLock Lock(&CS);

	for (int32 i = 0; i < Allocators.Num(); i++)
	{
		FD3D12ResourceBlockInfo* Block = Allocators[i]->TryAllocate(SizeInBytes, Alignment);
		if (Block)
		{
			return Block;
		}
	}

	Allocators.Add(CreateNewAllocator());
	return Allocators.Last()->TryAllocate(SizeInBytes, Alignment);
}

void FD3D12MultiBuddyAllocator::Deallocate(FD3D12ResourceBlockInfo* Block)
{
	//The sub-allocators should handle the deallocation
	check(false);
}

FD3D12BuddyAllocator* FD3D12MultiBuddyAllocator::CreateNewAllocator()
{
	return new FD3D12BuddyAllocator(GetParentDevice(),
		DebugName,
		AllocationStrategy,
		HeapType,
		HeapFlags,
		ResourceFlags,
		MaximumAllocationSizeForPooling,
		MaxBlockSize,
		MinBlockSize);
}

void FD3D12MultiBuddyAllocator::Initialize()
{
	Allocators.Add(CreateNewAllocator());
}

void FD3D12MultiBuddyAllocator::Destroy()
{
	ReleaseAllResources();
}

void FD3D12MultiBuddyAllocator::CleanUpAllocations()
{
	FScopeLock Lock(&CS);

	for (auto*& Allocator : Allocators)
	{
		Allocator->CleanUpAllocations();
	}

	// Trim empty allocators
	for (int32 i = (Allocators.Num() - 1); i >= 0; i--)
	{
		if (Allocators[i]->IsEmpty())
		{
			Allocators[i]->Destroy();
			delete(Allocators[i]);
			Allocators.RemoveAt(i);
		}
	}
}

void FD3D12MultiBuddyAllocator::DumpAllocatorStats(class FOutputDevice& Ar)
{
	//TODO
}

void FD3D12MultiBuddyAllocator::ReleaseAllResources()
{
	for (int32 i = (Allocators.Num() - 1); i >= 0; i--)
	{
		Allocators[i]->CleanUpAllocations();
		Allocators[i]->Destroy();
		delete(Allocators[i]);
	}

	Allocators.Empty();
}

void FD3D12MultiBuddyAllocator::Reset()
{

}

//-----------------------------------------------------------------------------
//	Linear Allocator
//-----------------------------------------------------------------------------

FD3D12LinearAllocator::FD3D12LinearAllocator(FD3D12Device* ParentDevice,
	FString Name,
	D3D12_HEAP_TYPE heapType,
	D3D12_RESOURCE_FLAGS flags,
	uint32 MaxSizeForPooling,
	uint32 PageSize) :
	AllocationPageSize(PageSize),
	FD3D12ResourceAllocator(ParentDevice, Name, heapType, flags, MaxSizeForPooling)
{
	Reset();
}

FD3D12ResourceBlockInfo* FD3D12LinearAllocator::TryAllocate(uint32 SizeInBytes, uint32 Alignment)
{
	FScopeLock Lock(&CS);

	if (Initialized == false)
	{
		Initialize();
		Initialized = true;
	}

	check(SizeInBytes <= AllocationPageSize);

	FD3D12LinearAllocatorPage* CurrentPage = FindPageWithBestFit(SizeInBytes, Alignment);

	uint32 AlignedOffset = AlignArbitrary(CurrentPage->CurrentOffset, Alignment);

	FD3D12ResourceBlockInfo::AllocationTrackingValues alloc;
	alloc.Size = SizeInBytes + (AlignedOffset - CurrentPage->CurrentOffset);
	alloc.AllocatorPrivateData = CurrentPage;

	FD3D12ResourceBlockInfo* Block = new FD3D12ResourceBlockInfo(this, AlignedOffset, alloc);

	Block->ResourceHeap = CurrentPage->Resource;

	if (IsCPUWritable(HeapType))
	{
		Block->Address = (byte*)CurrentPage->BaseAddress + AlignedOffset;
	}

	CurrentPage->CurrentOffset += alloc.Size;

	return Block;
}

FD3D12LinearAllocatorPage* FD3D12LinearAllocator::FindPageWithBestFit(uint32 AllocationSize, uint32 Alignment)
{
	// Search for a space that is a good fit
	// TODO: This could potentially be super slow! (O(n^2)), keeping the list sorted could help
	for (int32 i = 0; i < ActivePages.Num(); i++)
	{
		uint32 AlignedOffset = AlignArbitrary(ActivePages[i]->CurrentOffset, Alignment);

		if (AllocationSize < (AllocationPageSize - AlignedOffset))
		{
			return ActivePages[i];
		}
	}

	ActivePages.Add(AllocatePage());
	return ActivePages.Last();
}

void FD3D12LinearAllocator::Deallocate(FD3D12ResourceBlockInfo* Block)
{
	FScopeLock Lock(&CS);

	check(IsOwner(*Block));
	FD3D12LinearAllocatorPage* page = (FD3D12LinearAllocatorPage*)Block->AllocatorValues.AllocatorPrivateData;
	page->DeallocatedSpace += Block->AllocatorValues.Size;

	delete(Block);
}

void FD3D12LinearAllocator::Initialize()
{
	ActivePages.Reserve(32);

	ActivePages.Add(AllocatePage());
}

void FD3D12LinearAllocator::Destroy()
{
	for (FD3D12LinearAllocatorPage*& page : ActivePages)
	{
		DestroyPage(*page);
		delete(page);
	}

	ActivePages.Empty();
}

void FD3D12LinearAllocator::CleanUpAllocations()
{
	FScopeLock Lock(&CS);

	for (int32 i = (ActivePages.Num() - 1); i >= 0; i--)
	{
		// All the allcocations that came out of this page are emptied so it can be release;
		if (ActivePages[i]->DeallocatedSpace == AllocationPageSize)
		{
			DestroyPage(*ActivePages[i]);
			delete(ActivePages[i]);
			ActivePages.RemoveAt(i);
		}
	}
}

void FD3D12LinearAllocator::DumpAllocatorStats(class FOutputDevice& Ar)
{
	//TODO:
}

void FD3D12LinearAllocator::ReleaseAllResources()
{
	for (FD3D12LinearAllocatorPage*& page : ActivePages)
	{
		DestroyPage(*page);
		delete(page);
	}

	ActivePages.Empty();
}

void FD3D12LinearAllocator::Reset()
{
	ReleaseAllResources();
}

FD3D12LinearAllocatorPage* FD3D12LinearAllocator::AllocatePage()
{
	FD3D12LinearAllocatorPage* newPage = new FD3D12LinearAllocatorPage();

	VERIFYD3D12RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(HeapType, AllocationPageSize, &newPage->Resource, ResourceFlags));
	SetName(newPage->Resource, L"Linear Allocator Page");

	if (IsCPUWritable(HeapType))
	{
		VERIFYD3D12RESULT(newPage->Resource->GetResource()->Map(0, nullptr, &newPage->BaseAddress));
	}
	return newPage;
}

void FD3D12LinearAllocator::DestroyPage(FD3D12LinearAllocatorPage& Page)
{
	check(Page.Resource);
	delete(Page.Resource);
	Page.Resource = nullptr;
}

//-----------------------------------------------------------------------------
//	Bucket Allocator
//-----------------------------------------------------------------------------
FD3D12BucketAllocator::FD3D12BucketAllocator(FD3D12Device* ParentDevice,
	FString Name,
	D3D12_HEAP_TYPE heapType,
	D3D12_RESOURCE_FLAGS flags,
	uint64 BlockRetentionFrameCountIn) : 
	BlockRetentionFrameCount(BlockRetentionFrameCountIn),
	FD3D12ResourceAllocator(ParentDevice, Name, heapType, flags, 32 * 1024 * 1024)
{}

FD3D12ResourceBlockInfo* FD3D12BucketAllocator::TryAllocate(uint32 SizeInBytes, uint32 Alignment)
{
	FScopeLock Lock(&CS);

	// Size cannot be smaller than the requested alignment
	SizeInBytes = FMath::Max(SizeInBytes, Alignment);

	uint32 bucket = BucketFromSize(SizeInBytes, BucketShift);
	check(bucket < NumBuckets);
	FD3D12ResourceBlockInfo* pBlock = nullptr;

	uint32 BlockSize = BlockSizeFromBufferSize(SizeInBytes, BucketShift);

	// If some odd alignment is requested, make sure the block can fulfill it.
	if (BlockSize % Alignment != 0)
	{
		SizeInBytes += Alignment;
		bucket = BucketFromSize(SizeInBytes, BucketShift);
		BlockSize = BlockSizeFromBufferSize(SizeInBytes, BucketShift);
	}

	// See if a block is already available in the bucket
	if (AvailableBlocks[bucket].Dequeue(pBlock))
	{
		check(pBlock);
		if (IsCPUWritable(HeapType))
		{
			check(pBlock->Address);
		}
		check(pBlock->ResourceHeap);
	}
	else
	{
		// No blocks of the requested size are available so make one
		FD3D12Resource* Resource = nullptr;
		void *BaseAddress = nullptr;

		// Allocate a block
		check(BlockSize >= SizeInBytes);

		if (FAILED(GetParentDevice()->GetResourceHelper().CreateBuffer(HeapType, BlockSize < MIN_HEAP_SIZE ? MIN_HEAP_SIZE : SizeInBytes, &Resource, ResourceFlags)))
		{
			return nullptr;
		}

		if (IsCPUWritable(HeapType))
		{
			VERIFYD3D12RESULT(Resource->GetResource()->Map(0, nullptr, &BaseAddress));
			check(BaseAddress == (uint8*)(((uint64)BaseAddress + Alignment - 1) & ~((uint64)Alignment - 1)));
		}

		FD3D12ResourceBlockInfo::AllocationTrackingValues alloc = {};
		alloc.AllocatorPrivateData = (void*)size_t(bucket); // Stash the bucket index in the private data
		alloc.Size = BlockSize;

		pBlock = new FD3D12ResourceBlockInfo(this, 0, alloc);
		pBlock->Address = BaseAddress;
		pBlock->ResourceHeap = Resource;

		// If bucket blocks are less than 65536 bytes then use FDynamicResourcePool
		if (BlockSize < MIN_HEAP_SIZE)
		{
			// Create additional available blocks that can be sub-allocated from the same resource
			for (uint32 Offset = BlockSize; Offset <= MIN_HEAP_SIZE - BlockSize; Offset += BlockSize)
			{
				void *Start = (void *)((uint8 *)BaseAddress + Offset);
				FD3D12ResourceBlockInfo *NewBlock = new FD3D12ResourceBlockInfo(this, Offset, alloc);
				NewBlock->Address = (BaseAddress) ? Start : nullptr;
				NewBlock->ResourceHeap = Resource;
				NewBlock->ResourceHeap->AddRef();

				// Add the bucket to the available list
				AvailableBlocks[bucket].Enqueue(NewBlock);
			}
		}
	}

	if (Alignment != 0 && pBlock->Offset % Alignment != 0)
	{
		uint64 OriginalOffset = pBlock->Offset;
		pBlock->Offset = AlignArbitrary(pBlock->Offset, Alignment);

		// Check that when the offset is aligned that it doesn't go passed the end of the block
		check(pBlock->Offset + SizeInBytes <= OriginalOffset + pBlock->GetSize());

		if (pBlock->Address)
		{
			pBlock->Address = (void*)((SIZE_T)pBlock->Address + (SIZE_T)(pBlock->Offset - OriginalOffset));
		}
	}

	// Check the returned block matches the requested alignment
	check(Alignment == 0 || ((size_t)(pBlock->Offset) % Alignment) == 0);

	return pBlock;
}

void FD3D12BucketAllocator::Deallocate(FD3D12ResourceBlockInfo* Block)
{
	FScopeLock Lock(&CS);

	FD3D12CommandListManager& CommandListManager = GetParentDevice()->GetCommandListManager();
	Block->FrameFence = CommandListManager.GetFence(FT_Frame).GetCurrentFence();

	ExpiredBlocks.Enqueue(Block);
}

void FD3D12BucketAllocator::Initialize()
{

}

void FD3D12BucketAllocator::Destroy()
{
	ReleaseAllResources();
}
void FD3D12BucketAllocator::CleanUpAllocations()
{
	FScopeLock Lock(&CS);

	struct FDequeueAlloc
	{
		FDequeueAlloc(FD3D12CommandListManager* InCommandListManager, const uint64 InRetentionFrameCount)
			: CommandListManager(InCommandListManager)
			, RetentionFrameCount(InRetentionFrameCount)
		{
		}

		bool operator() (FD3D12ResourceBlockInfo* pBlockInfo)
		{
			return CommandListManager->GetFence(FT_Frame).IsFenceComplete(pBlockInfo->FrameFence + RetentionFrameCount);
		}

		FD3D12CommandListManager* CommandListManager;
		const uint64              RetentionFrameCount;
	};

	FD3D12CommandListManager& CommandListManager = GetParentDevice()->GetCommandListManager();

#if SUB_ALLOCATED_DEFAULT_ALLOCATIONS
	const static uint32 MinCleanupBucket = FMath::Max<uint32>(0, BucketFromSize(MIN_HEAP_SIZE, BucketShift) - 4);
#else
	const static uint32 MinCleanupBucket = 0;
#endif

	// Start at bucket 8 since smaller buckets are sub-allocated resources
	// and would be fragmented by deleting blocks
	for (uint32 bucket = MinCleanupBucket; bucket < NumBuckets; bucket++)
	{
		FDequeueAlloc DequeueAlloc(&CommandListManager, BlockRetentionFrameCount);
		FD3D12ResourceBlockInfo* pBlockInfo = nullptr;

		while (AvailableBlocks[bucket].Dequeue(pBlockInfo, DequeueAlloc))
		{
			pBlockInfo->ResourceHeap->Release();
			delete(pBlockInfo);
		}
	}

	FD3D12ResourceBlockInfo* pBlockInfo = nullptr;
	FDequeueAlloc DequeueAlloc(&CommandListManager, 0);

	while (ExpiredBlocks.Dequeue(pBlockInfo, DequeueAlloc))
	{
		check(pBlockInfo);
		// Get the bucket info from the allocator private data
		uint32 bucket = uint32((size_t)pBlockInfo->AllocatorValues.AllocatorPrivateData);
		// Add the bucket to the available list
		AvailableBlocks[bucket].Enqueue(pBlockInfo);
	}
}

void FD3D12BucketAllocator::DumpAllocatorStats(class FOutputDevice& Ar)
{
	//TODO:
}
void FD3D12BucketAllocator::ReleaseAllResources()
{
	const static uint32 MinCleanupBucket = 0;

	// Start at bucket 8 since smaller buckets are sub-allocated resources
	// and would be fragmented by deleting blocks
	for (uint32 bucket = MinCleanupBucket; bucket < NumBuckets; bucket++)
	{
		FD3D12ResourceBlockInfo* pBlockInfo = nullptr;
		while (AvailableBlocks[bucket].Dequeue(pBlockInfo))
		{
			pBlockInfo->ResourceHeap->Release();
			delete(pBlockInfo);
		}
	}

	FD3D12ResourceBlockInfo* pBlockInfo = nullptr;

	while (ExpiredBlocks.Dequeue(pBlockInfo))
	{
		// Get the bucket info from the allocator private data
		uint32 bucket = *(uint32*)pBlockInfo->AllocatorValues.AllocatorPrivateData;

		if (bucket >= MinCleanupBucket)
		{
			pBlockInfo->ResourceHeap->Release();
		}
		delete(pBlockInfo);
	}

	for (FD3D12Resource*& Resource : SubAllocatedResources)
	{
		Resource->Release();
		delete(Resource);
	}
}

void FD3D12BucketAllocator::Reset()
{

}

//-----------------------------------------------------------------------------
//	Dynamic Buffer Allocator
//-----------------------------------------------------------------------------

FD3D12DynamicHeapAllocator::FD3D12DynamicHeapAllocator(FD3D12Device* InParent, FString Name, eBuddyAllocationStrategy allocationStrategy,
	uint32 MaxSizeForPooling,
	uint32 maxBlockSize,
	uint32 minBlockSize)
	: CurrentCommandContext(nullptr)
	, Allocator(nullptr)
	, FD3D12DeviceChild(InParent)
{
	Allocator = new FD3D12MultiBuddyAllocator(InParent, Name,
		allocationStrategy,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
		D3D12_RESOURCE_FLAG_NONE,
		MaxSizeForPooling,
		maxBlockSize,
		minBlockSize);
}

void* FD3D12DynamicHeapAllocator::AllocUploadResource(uint32 size, uint32 alignment, FD3D12ResourceLocation* ResourceLocation)
{
	ResourceLocation->Clear();

	//TODO: For some reason 0 sized buffers are being created and then expected to have a resource
	if (size == 0)
	{
		size = 16;
	}

	//Work loads like infiltrator create enourmous amounts of buffer space in setup
	//clean up as we go as it can even run out of memory before the first frame.
	if (GetParentDevice()->FirstFrameSeen == false)
	{
		GetParentDevice()->GetDeferredDeletionQueue().ReleaseResources(true);
		Allocator->CleanUpAllocations();
	}

	if (size <= Allocator->MaximumAllocationSizeForPooling)
	{
		FD3D12ResourceBlockInfo* Block = Allocator->TryAllocate(size, alignment);

		if (Block)
		{
			FD3D12CommandListManager& CommandListManager = GetParentDevice()->GetCommandListManager();
			Block->FrameFence = CommandListManager.GetFence(FT_Frame).GetCurrentFence();
			check(Block->FrameFence);

			FD3D12StateCache* pStateCache = CurrentCommandContext ? &CurrentCommandContext->StateCache : nullptr;
			ResourceLocation->SetBlockInfo(Block, pStateCache);
			ResourceLocation->EffectiveBufferSize = size;

			check(Block->Address);
			check(Block->ResourceHeap);

			if (alignment > 0)
			{
				uint32 AlignmentMismatch = ResourceLocation->GetOffset() % alignment;

				if (AlignmentMismatch != 0)
				{
					ResourceLocation->SetPadding(alignment - AlignmentMismatch);

					check(ResourceLocation->GetOffset() % alignment == 0);
					check(ResourceLocation->GetOffset() % 4 == 0);
				}
			}

			return Block->Address;
		}
	}

	//Allocate Standalone
	VERIFYD3D12RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, size, ResourceLocation->Resource.GetInitReference()));
	SetName(ResourceLocation->Resource, L"Stand Alone Upload Buffer");

	void* Data = nullptr;
	VERIFYD3D12RESULT(ResourceLocation->Resource->GetResource()->Map(0, nullptr, &Data));

	ResourceLocation->EffectiveBufferSize = size;
	ResourceLocation->Offset = alignment;

	ResourceLocation->UpdateGPUVirtualAddress();

	return Data;
}

void FD3D12DynamicHeapAllocator::CleanUpAllocations()
{
	Allocator->CleanUpAllocations();
}

void FD3D12DynamicHeapAllocator::Destroy()
{
	Allocator->Destroy();
	delete(Allocator);
	Allocator = nullptr;
}

//-----------------------------------------------------------------------------
//	Default Buffer Allocator
//-----------------------------------------------------------------------------

FD3D12DefaultBufferPool::FD3D12DefaultBufferPool(FD3D12Device* InParent, FD3D12ResourceAllocator* AllocatorIn) :
	Allocator(AllocatorIn),
	FD3D12DeviceChild(InParent)
{
}

void FD3D12DefaultBufferPool::CleanUpAllocations()
{
	Allocator->CleanUpAllocations();
}


// Grab a buffer from the available buffers or create a new buffer if none are available

void FD3D12DefaultBufferPool::AllocDefaultResource(const D3D12_RESOURCE_DESC& Desc, D3D12_SUBRESOURCE_DATA* InitialData, FD3D12ResourceLocation* ResourceLocation, uint32 Alignment)
{
	// If the resource location owns a block, this will deallocate it.
	ResourceLocation->Clear();

	if (Desc.Width == 0) return;

	const void* Data = (InitialData) ? InitialData->pData : nullptr;

	const bool PoolResource = Desc.Width < Allocator->MaximumAllocationSizeForPooling &&
		((Desc.Width % (1024 * 64)) != 0);

	if (PoolResource)
	{
		// Ensure we're allocating from the correct pool
		check(Desc.Flags == Allocator->ResourceFlags);

		FD3D12ResourceBlockInfo* Block = Allocator->TryAllocate(Desc.Width, Alignment);

		if (Block)
		{
			ResourceLocation->SetBlockInfo(Block, &GetParentDevice()->GetDefaultCommandContext().StateCache);

			ResourceLocation->EffectiveBufferSize = Desc.Width;

			if (Alignment > 0)
			{
				uint32 AlignmentMismatch = ResourceLocation->GetOffset() % Alignment;

				if (AlignmentMismatch != 0)
				{
					ResourceLocation->SetPadding(Alignment - AlignmentMismatch);

					check(ResourceLocation->GetOffset() % Alignment == 0);
					check(ResourceLocation->GetOffset() % 4 == 0);
				}
			}

			if (InitialData)
			{
				Allocator->InitializeDefaultBuffer(GetParentDevice(), Block->ResourceHeap, Block->Offset, InitialData->pData, Desc.Width);
			}

			return;
		}
	}

	//Allocate Standalone
	VERIFYD3D12RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, Desc.Width, ResourceLocation->Resource.GetInitReference(), Allocator->ResourceFlags));
	SetName(ResourceLocation->Resource, L"Stand Alone Default Buffer");

	ResourceLocation->EffectiveBufferSize = Desc.Width;
	ResourceLocation->Offset = 0;

	ResourceLocation->UpdateGPUVirtualAddress();

	if (Data)
	{
		Allocator->InitializeDefaultBuffer(GetParentDevice(), ResourceLocation->Resource.GetReference(), ResourceLocation->Offset, Data, InitialData->RowPitch);
	}
}

#if 0
#define USE_BUCKET_ALLOCATOR
#endif

// Grab a buffer from the available buffers or create a new buffer if none are available
HRESULT FD3D12DefaultBufferAllocator::AllocDefaultResource(const D3D12_RESOURCE_DESC& Desc, D3D12_SUBRESOURCE_DATA* InitialData, FD3D12ResourceLocation* ResourceLocation, uint32 Alignment)
{
	if (BufferIsWriteable(Desc))
	{
		VERIFYD3D12RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, Desc.Width, ResourceLocation->Resource.GetInitReference(), Desc.Flags));
		SetName(ResourceLocation->Resource, L"Stand Alone Default Buffer");

		ResourceLocation->EffectiveBufferSize = Desc.Width;
		ResourceLocation->Offset = 0;

		ResourceLocation->UpdateGPUVirtualAddress();

		if (InitialData)
		{
			FD3D12ResourceAllocator::InitializeDefaultBuffer(GetParentDevice(), ResourceLocation->Resource.GetReference(), ResourceLocation->Offset, InitialData->pData, InitialData->RowPitch);
		}
	}
	else
	{
		//NOTE: Indexing based on the resource flags looks weird but is necessary e.g. the flags dictate if the resource
		//      can be used as a UAV. So each type of buffer has to come from a separate pool.

		check((uint32)Desc.Flags < MAX_DEFAULT_POOLS);
		if (DefaultBufferPools[Desc.Flags] == nullptr)
		{
			FD3D12ResourceAllocator* Allocator = nullptr;

#ifdef USE_BUCKET_ALLOCATOR
			Allocator = new FD3D12BucketAllocator(GetParentDevice(), FString(L"Default Buffer Bucket Allocator"), D3D12_HEAP_TYPE_DEFAULT, Desc.Flags, 5);
#else
			Allocator = new FD3D12MultiBuddyAllocator(GetParentDevice(),
				FString(L"Default Buffer Multi Buddy Allocator"),
				kManualSubAllocationStrategy,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS,
				Desc.Flags,
				DEFAULT_BUFFER_POOL_MAX_ALLOC_SIZE,
				DEFAULT_BUFFER_POOL_SIZE,
				16);
#endif

			DefaultBufferPools[Desc.Flags] = new FD3D12DefaultBufferPool(GetParentDevice(), Allocator);
		}

		DefaultBufferPools[Desc.Flags]->AllocDefaultResource(Desc, InitialData, ResourceLocation, Alignment);
	}
	return S_OK;
}

void FD3D12DefaultBufferAllocator::FreeDefaultBufferPools()
{
	for (uint32 i = 0; i < MAX_DEFAULT_POOLS; ++i)
	{
		if (DefaultBufferPools[i])
		{
			DefaultBufferPools[i]->CleanUpAllocations();
		}
		delete DefaultBufferPools[i];
		DefaultBufferPools[i] = nullptr;
	}
}

void FD3D12DefaultBufferAllocator::CleanupFreeBlocks()
{
	for (uint32 i = 0; i < MAX_DEFAULT_POOLS; ++i)
	{
		if (DefaultBufferPools[i])
		{
			DefaultBufferPools[i]->CleanUpAllocations();
		}
	}
}

//-----------------------------------------------------------------------------
//	Texture Allocator
//-----------------------------------------------------------------------------

FD3D12TextureAllocator::FD3D12TextureAllocator(FD3D12Device* Device, FString Name, uint32 HeapSize, D3D12_HEAP_FLAGS Flags) :
	FD3D12MultiBuddyAllocator(Device, Name,
		kPlacedResourceStrategy,
		D3D12_HEAP_TYPE_DEFAULT,
		Flags | D3D12_HEAP_FLAG_DENY_BUFFERS,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		HeapSize,
		D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT)
{
	// Inform the texture streaming system of this heap so that it correctly accounts for placed textures
	FD3D12DynamicRHI::GetD3DRHI()->UpdataTextureMemorySize((MaxBlockSize / 1024));
}

FD3D12TextureAllocator::~FD3D12TextureAllocator()
{
	FD3D12DynamicRHI::GetD3DRHI()->UpdataTextureMemorySize(-int32(MaxBlockSize / 1024));
}

HRESULT FD3D12TextureAllocator::AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, FD3D12ResourceLocation* TextureLocation, const D3D12_RESOURCE_STATES InitialState)
{
	FD3D12Device* Device = GetParentDevice();
	FD3D12ResourceHelper& ResourceHelper = Device->GetResourceHelper();
	TRefCountPtr<FD3D12Resource> Resource;
	HRESULT hr = S_OK;

	TextureLocation->Clear();

	D3D12_RESOURCE_ALLOCATION_INFO Info = Device->GetDevice()->GetResourceAllocationInfo(0, 1, &Desc);

	if (Info.SizeInBytes < D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
	{
		FD3D12ResourceBlockInfo* Block = TryAllocate(Info.SizeInBytes, Info.Alignment);
		if (Block)
		{
			FD3D12Heap* BackingHeap = ((FD3D12BuddyAllocator*)Block->Allocator)->GetBackingHeap();
			hr = GetParentDevice()->GetResourceHelper().CreatePlacedResource(Desc, BackingHeap, Block->Offset, InitialState, ClearValue, Resource.GetInitReference());

			Block->ResourceHeap = Resource.GetReference();
			Block->IsPlacedResource = true;
			TextureLocation->SetBlockInfo(Block, &Device->GetDefaultCommandContext().StateCache);

			return hr;
		}
	}

	// Request default alignment for stand alone textures
	Desc.Alignment = 0;
	hr = ResourceHelper.CreateDefaultResource(Desc, ClearValue, Resource.GetInitReference(), InitialState);

	TextureLocation->SetFromD3DResource(Resource, 0, 0);

	return hr;
}

FD3D12TextureAllocatorPool::FD3D12TextureAllocatorPool(FD3D12Device* Device) :
	ReadOnlyTexturePool(Device, FString(L"Small Read-Only Texture allocator"), TEXTURE_POOL_SIZE, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES),
	FD3D12DeviceChild(Device)
{};

HRESULT FD3D12TextureAllocatorPool::AllocateTexture(D3D12_RESOURCE_DESC Desc, const D3D12_CLEAR_VALUE* ClearValue, uint8 UEFormat, FD3D12ResourceLocation* TextureLocation, const D3D12_RESOURCE_STATES InitialState)
{
	// 4KB alignment is only available for read only textures
	if ((Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ||
		Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL ||
		Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == false &&
		Desc.SampleDesc.Count == 1)// Multi-Sample texures have much larger alignment requirements (4MB vs 64KB)
	{
		// The top mip level must be less than 64k
		if (TextureCanBe4KAligned(Desc, UEFormat))
		{
			Desc.Alignment = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT; // request 4k alignment
			return ReadOnlyTexturePool.AllocateTexture(Desc, ClearValue, TextureLocation, InitialState);
		}
	}

	TRefCountPtr<FD3D12Resource> Resource = nullptr;

	HRESULT hr = GetParentDevice()->GetResourceHelper().CreateDefaultResource(Desc, ClearValue, Resource.GetInitReference(), InitialState);
	TextureLocation->SetFromD3DResource(Resource, 0, 0);

	return hr;
}

//-----------------------------------------------------------------------------
//	Fast Allocation
//-----------------------------------------------------------------------------

void* FD3D12FastAllocator::Allocate(uint32 size, uint32 alignment, class FD3D12ResourceLocation* ResourceLocation)
{
	return AllocateInternal(size, alignment, ResourceLocation);
}

void FD3D12FastAllocator::Destroy()
{
	if (CurrentAllocatorPage)
	{
		PagePool->ReturnFastAllocatorPage(CurrentAllocatorPage);
		CurrentAllocatorPage = nullptr;
	}
}

void* FD3D12FastAllocator::AllocateInternal(uint32 size, uint32 alignment, class FD3D12ResourceLocation* ResourceLocation)
{
	if (size > PagePool->GetPageSize())
	{
		ResourceLocation->Clear();

		//Allocations are 64k aligned
		if (alignment)
			alignment = (D3D_BUFFER_ALIGNMENT % alignment) == 0 ? 0 : alignment;

		VERIFYD3D12RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(PagePool->GetHeapType(), size + alignment, ResourceLocation->Resource.GetInitReference()));
		SetName(ResourceLocation->Resource, L"Stand Alone Fast Allocation");

		void* Data = nullptr;
		if (PagePool->IsCPUWritable())
		{
			VERIFYD3D12RESULT(ResourceLocation->Resource->GetResource()->Map(0, nullptr, &Data));
		}

		ResourceLocation->EffectiveBufferSize = size;
		ResourceLocation->Offset = alignment;

		ResourceLocation->UpdateGPUVirtualAddress();

		return Data;
	}
	else
	{
		void* Data = nullptr;

		const uint32 Offset = (CurrentAllocatorPage) ? CurrentAllocatorPage->NextFastAllocOffset : 0;
		uint32 CurrentOffset = AlignArbitrary(Offset, alignment);

		// See if there is room in the current pool
		if (CurrentAllocatorPage == nullptr || PagePool->GetPageSize() < CurrentOffset + size)
		{
			if (CurrentAllocatorPage)
			{
				PagePool->ReturnFastAllocatorPage(CurrentAllocatorPage);
			}
			CurrentAllocatorPage = PagePool->RequestFastAllocatorPage();

			CurrentOffset = AlignArbitrary(CurrentAllocatorPage->NextFastAllocOffset, alignment);
		}

		check(PagePool->GetPageSize() - size >= CurrentOffset);

		// Create a FD3D12ResourceLocation representing a sub-section of the pool resource
		ResourceLocation->SetFromD3DResource(CurrentAllocatorPage->FastAllocBuffer.GetReference(), CurrentOffset, size);
		ResourceLocation->SetAsFastAllocatedSubresource();

		CurrentAllocatorPage->NextFastAllocOffset = CurrentOffset + size;

		if (PagePool->IsCPUWritable())
		{
			Data = (void*)((uint8*)CurrentAllocatorPage->FastAllocData + CurrentOffset);
		}

		check(Data);
		return Data;
	}
}

void* FD3D12ThreadSafeFastAllocator::Allocate(uint32 size, uint32 alignment, class FD3D12ResourceLocation* ResourceLocation)
{
	FScopeLock Lock(&CS);
	return AllocateInternal(size, alignment, ResourceLocation);
}

FD3D12FastAllocatorPage* FD3D12FastAllocatorPagePool::RequestFastAllocatorPage()
{
	return RequestFastAllocatorPageInternal();
}

void FD3D12FastAllocatorPagePool::ReturnFastAllocatorPage(FD3D12FastAllocatorPage* Page)
{
	return ReturnFastAllocatorPageInternal(Page);
}

void FD3D12FastAllocatorPagePool::CleanUpPages(uint64 frameLag, bool force)
{
	return CleanUpPagesInternal(frameLag, force);
}

FD3D12FastAllocatorPage* FD3D12ThreadSafeFastAllocatorPagePool::RequestFastAllocatorPage()
{
	FScopeLock Lock(&CS);
	return RequestFastAllocatorPageInternal();
}

void FD3D12ThreadSafeFastAllocatorPagePool::ReturnFastAllocatorPage(FD3D12FastAllocatorPage* Page)
{
	FScopeLock Lock(&CS);
	return ReturnFastAllocatorPageInternal(Page);
}

void FD3D12ThreadSafeFastAllocatorPagePool::CleanUpPages(uint64 frameLag, bool force)
{
	FScopeLock Lock(&CS);
	return CleanUpPagesInternal(frameLag, force);
}

void FD3D12FastAllocatorPagePool::Destroy()
{
	for (int32 i = 0; i < Pool.Num(); i++)
	{
		check(Pool[i]->FastAllocBuffer->GetRefCount() == 1);
		{
			FD3D12FastAllocatorPage *Page = Pool[i];
			delete(Page);
		}
	}

	Pool.Empty();
}

FD3D12FastAllocatorPage* FD3D12FastAllocatorPagePool::RequestFastAllocatorPageInternal()
{
	FD3D12FastAllocatorPage* Page = nullptr;

	uint64 currentFence = GetParentDevice()->GetCommandListManager().GetFence(EFenceType::FT_Frame).GetLastCompletedFence();

	for (int32 Index = 0; Index < Pool.Num(); Index++)
	{
		//If the GPU is done with it and no-one has a lock on it
		if (Pool[Index]->FastAllocBuffer->GetRefCount() == 1 &&
			Pool[Index]->FrameFence <= currentFence)
		{
			Page = Pool[Index];
			Page->Reset();
			Pool.RemoveAt(Index);
			return Page;
		}
	}

	check(Page == nullptr);
	Page = new FD3D12FastAllocatorPage(PageSize);

	VERIFYD3D12RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(HeapType, PageSize, Page->FastAllocBuffer.GetInitReference(), D3D12_RESOURCE_FLAG_NONE, &HeapProperties));
	SetName(Page->FastAllocBuffer, L"Fast Allocator Page");

	VERIFYD3D12RESULT(Page->FastAllocBuffer->GetResource()->Map(0, nullptr, &Page->FastAllocData));
	return Page;
}

void FD3D12FastAllocatorPagePool::ReturnFastAllocatorPageInternal(FD3D12FastAllocatorPage* Page)
{
	Page->FrameFence = GetParentDevice()->GetCommandListManager().GetFence(EFenceType::FT_Frame).GetCurrentFence();
	Pool.Add(Page);
}

void FD3D12FastAllocatorPagePool::CleanUpPagesInternal(uint64 frameLag, bool force)
{
	uint64 currentFence = GetParentDevice()->GetCommandListManager().GetFence(EFenceType::FT_Frame).GetLastCompletedFence();

	FD3D12FastAllocatorPage* Page = nullptr;

	int32 Index = Pool.Num() - 1;
	while (Index >= 0)
	{
		//If the GPU is done with it and no-one has a lock on it
		if (Pool[Index]->FastAllocBuffer->GetRefCount() == 1 &&
			Pool[Index]->FrameFence + frameLag <= currentFence)
		{
			Page = Pool[Index];
			Pool.RemoveAt(Index);
			delete(Page);
		}

		Index--;
	}
}