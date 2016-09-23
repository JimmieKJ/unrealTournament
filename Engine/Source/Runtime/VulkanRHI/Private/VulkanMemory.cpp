// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanMemory.cpp: Vulkan memory RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanMemory.h"

namespace VulkanRHI
{
	static FCriticalSection GBufferAllocationLock;
	static FCriticalSection GAllocationLock;
	static FCriticalSection GFenceLock;

	FDeviceMemoryManager::FDeviceMemoryManager() :
		DeviceHandle(VK_NULL_HANDLE),
		bHasUnifiedMemory(false),
		Device(nullptr),
		NumAllocations(0),
		PeakNumAllocations(0)
	{
		FMemory::Memzero(MemoryProperties);
	}

	FDeviceMemoryManager::~FDeviceMemoryManager()
	{
		Deinit();
	}

	void FDeviceMemoryManager::Init(FVulkanDevice* InDevice)
	{
		check(Device == nullptr);
		Device = InDevice;
		NumAllocations = 0;
		PeakNumAllocations = 0;

		DeviceHandle = Device->GetInstanceHandle();
		VulkanRHI::vkGetPhysicalDeviceMemoryProperties(InDevice->GetPhysicalHandle(), &MemoryProperties);

		HeapInfos.AddDefaulted(MemoryProperties.memoryHeapCount);

		PrintMemInfo();
	}

	void FDeviceMemoryManager::PrintMemInfo()
	{
		const uint32 MaxAllocations = Device->GetLimits().maxMemoryAllocationCount;
		UE_LOG(LogVulkanRHI, Display, TEXT("%d Device Memory Heaps; Max memory allocations %d"), MemoryProperties.memoryHeapCount, MaxAllocations);
		for (uint32 Index = 0; Index < MemoryProperties.memoryHeapCount; ++Index)
		{
			bool bIsGPUHeap = ((MemoryProperties.memoryHeaps[Index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
			UE_LOG(LogVulkanRHI, Display, TEXT("%d: Flags 0x%x Size %llu (%.2f MB) %s"), 
				Index,
				MemoryProperties.memoryHeaps[Index].flags,
				MemoryProperties.memoryHeaps[Index].size,
				(float)((double)MemoryProperties.memoryHeaps[Index].size / 1024.0 / 1024.0),
				bIsGPUHeap ? TEXT("GPU") : TEXT(""));
			HeapInfos[Index].TotalSize = MemoryProperties.memoryHeaps[Index].size;
		}

		bHasUnifiedMemory = (MemoryProperties.memoryHeapCount == 1);
		UE_LOG(LogVulkanRHI, Display, TEXT("%d Device Memory Types"), MemoryProperties.memoryTypeCount);
		for (uint32 Index = 0; Index < MemoryProperties.memoryTypeCount; ++Index)
		{
			auto GetFlagsString = [](VkMemoryPropertyFlags Flags)
				{
					FString String;
					if ((Flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
					{
						String += TEXT(" Local");
					}
					if ((Flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
					{
						String += TEXT(" HostVisible");
					}
					if ((Flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
					{
						String += TEXT(" HostCoherent");
					}
					if ((Flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
					{
						String += TEXT(" HostCached");
					}
					if ((Flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) == VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
					{
						String += TEXT(" Lazy");
					}
					return String;
				};
			UE_LOG(LogVulkanRHI, Display, TEXT("%d: Flags 0x%x Heap %d %s"),
				Index,
				MemoryProperties.memoryTypes[Index].propertyFlags,
				MemoryProperties.memoryTypes[Index].heapIndex,
				*GetFlagsString(MemoryProperties.memoryTypes[Index].propertyFlags));
		}
	}

	void FDeviceMemoryManager::Deinit()
	{
		for (int32 Index = 0; Index < HeapInfos.Num(); ++Index)
		{
			ensureMsgf(HeapInfos[Index].Allocations.Num() == 0, TEXT("Found %d unfreed allocations!"), HeapInfos[Index].Allocations.Num());
		}
		NumAllocations = 0;
	}

	FDeviceMemoryAllocation* FDeviceMemoryManager::Alloc(VkDeviceSize AllocationSize, uint32 MemoryTypeIndex, const char* File, uint32 Line)
	{
		FScopeLock Lock(&GAllocationLock);

		check(AllocationSize > 0);
		check(MemoryTypeIndex < MemoryProperties.memoryTypeCount);

		VkMemoryAllocateInfo Info;
		FMemory::Memzero(Info);
		Info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		Info.allocationSize = AllocationSize;
		Info.memoryTypeIndex = MemoryTypeIndex;

		FDeviceMemoryAllocation* NewAllocation = new FDeviceMemoryAllocation;
		NewAllocation->DeviceHandle = DeviceHandle;
		NewAllocation->Size = AllocationSize;
		NewAllocation->MemoryTypeIndex = MemoryTypeIndex;
		NewAllocation->bCanBeMapped = ((MemoryProperties.memoryTypes[MemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		NewAllocation->bIsCoherent = ((MemoryProperties.memoryTypes[MemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		NewAllocation->bIsCached = ((MemoryProperties.memoryTypes[MemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
#if VULKAN_TRACK_MEMORY_USAGE
		NewAllocation->File = File;
		NewAllocation->Line = Line;
#endif
		++NumAllocations;
		PeakNumAllocations = FMath::Max(NumAllocations, PeakNumAllocations);
#if !VULKAN_SINGLE_ALLOCATION_PER_RESOURCE
		if (NumAllocations == Device->GetLimits().maxMemoryAllocationCount)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Hit Maximum # of allocations (%d) reported by device!"), NumAllocations);
		}
#endif
		VERIFYVULKANRESULT(VulkanRHI::vkAllocateMemory(DeviceHandle, &Info, nullptr, &NewAllocation->Handle));

		uint32 HeapIndex = MemoryProperties.memoryTypes[MemoryTypeIndex].heapIndex;
		HeapInfos[HeapIndex].Allocations.Add(NewAllocation);
		HeapInfos[HeapIndex].UsedSize += AllocationSize;

		return NewAllocation;
	}

	void FDeviceMemoryManager::Free(FDeviceMemoryAllocation*& Allocation)
	{
		FScopeLock Lock(&GAllocationLock);

		check(Allocation);
		check(Allocation->Handle != VK_NULL_HANDLE);
		check(!Allocation->bFreedBySystem);
		VulkanRHI::vkFreeMemory(DeviceHandle, Allocation->Handle, nullptr);

		--NumAllocations;

		uint32 HeapIndex = MemoryProperties.memoryTypes[Allocation->MemoryTypeIndex].heapIndex;

		HeapInfos[HeapIndex].UsedSize -= Allocation->Size;
		HeapInfos[HeapIndex].Allocations.RemoveSwap(Allocation);
		Allocation->bFreedBySystem = true;
		delete Allocation;
		Allocation = nullptr;
	}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	void FDeviceMemoryManager::DumpMemory()
	{
		PrintMemInfo();
		UE_LOG(LogVulkanRHI, Display, TEXT("Device Memory: %d allocations on %d heaps"), NumAllocations, HeapInfos.Num());
		for (int32 Index = 0; Index < HeapInfos.Num(); ++Index)
		{
			FHeapInfo& HeapInfo = HeapInfos[Index];
			UE_LOG(LogVulkanRHI, Display, TEXT("\tHeap %d, %d allocations"), Index, HeapInfo.Allocations.Num());
			uint64 TotalSize = 0;
			for (int32 SubIndex = 0; SubIndex < HeapInfo.Allocations.Num(); ++SubIndex)
			{
				FDeviceMemoryAllocation* Allocation = HeapInfo.Allocations[SubIndex];
				UE_LOG(LogVulkanRHI, Display, TEXT("\t\t%d Size %d Handle %p"), SubIndex, Allocation->Size, (void*)Allocation->Handle);
				TotalSize += Allocation->Size;
			}
			UE_LOG(LogVulkanRHI, Display, TEXT("\t\tTotal Allocated %.2f MB"), TotalSize / 1024.0f / 1024.0f);
		}
	}
#endif
	FDeviceMemoryAllocation::~FDeviceMemoryAllocation()
	{
		checkf(bFreedBySystem, TEXT("Memory has to released calling FDeviceMemory::Free()!"));
	}

	void* FDeviceMemoryAllocation::Map(VkDeviceSize InSize, VkDeviceSize Offset)
	{
		check(bCanBeMapped);
		check(!MappedPointer);
		check(InSize + Offset <= Size);

		VERIFYVULKANRESULT(VulkanRHI::vkMapMemory(DeviceHandle, Handle, Offset, InSize, 0, &MappedPointer));
		return MappedPointer;
	}

	void FDeviceMemoryAllocation::Unmap()
	{
		check(MappedPointer);
		VulkanRHI::vkUnmapMemory(DeviceHandle, Handle);
		MappedPointer = nullptr;
	}

	void FDeviceMemoryAllocation::FlushMappedMemory()
	{
		check(!IsCoherent());
		check(IsMapped());
		VkMappedMemoryRange Range;
		FMemory::Memzero(Range);
		Range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		Range.memory = Handle;
		//Range.offset = 0;
		Range.size = Size;
		VERIFYVULKANRESULT(VulkanRHI::vkFlushMappedMemoryRanges(DeviceHandle, 1, &Range));
	}

	void FDeviceMemoryAllocation::InvalidateMappedMemory()
	{
		check(!IsCoherent());
		check(IsMapped());
		VkMappedMemoryRange Range;
		FMemory::Memzero(Range);
		Range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		Range.memory = Handle;
		//Range.offset = 0;
		Range.size = Size;
		VERIFYVULKANRESULT(VulkanRHI::vkInvalidateMappedMemoryRanges(DeviceHandle, 1, &Range));
	}


	FOldResourceAllocation::FOldResourceAllocation(FOldResourceHeapPage* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
		uint32 InRequestedSize, uint32 InAlignedOffset,
		uint32 InAllocationSize, uint32 InAllocationOffset, const char* InFile, uint32 InLine)
		: Owner(InOwner)
		, DeviceMemoryAllocation(InDeviceMemoryAllocation)
		, AllocationSize(InAllocationSize)
		, AllocationOffset(InAllocationOffset)
		, RequestedSize(InRequestedSize)
		, AlignedOffset(InAlignedOffset)
#if VULKAN_TRACK_MEMORY_USAGE
		, File(InFile)
		, Line(InLine)
#endif
	{
	}

	FOldResourceAllocation::~FOldResourceAllocation()
	{
		Owner->ReleaseAllocation(this);
	}

	void FOldResourceAllocation::BindBuffer(FVulkanDevice* Device, VkBuffer Buffer)
	{
		VkResult Result = VulkanRHI::vkBindBufferMemory(Device->GetInstanceHandle(), Buffer, GetHandle(), GetOffset());
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		if (Result == VK_ERROR_OUT_OF_DEVICE_MEMORY || Result == VK_ERROR_OUT_OF_HOST_MEMORY)
		{
			Device->GetMemoryManager().DumpMemory();
			Device->GetResourceHeapManager().DumpMemory();
		}
#endif
		VERIFYVULKANRESULT(Result);
	}

	void FOldResourceAllocation::BindImage(FVulkanDevice* Device, VkImage Image)
	{
		VkResult Result = VulkanRHI::vkBindImageMemory(Device->GetInstanceHandle(), Image, GetHandle(), GetOffset());
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		if (Result == VK_ERROR_OUT_OF_DEVICE_MEMORY || Result == VK_ERROR_OUT_OF_HOST_MEMORY)
		{
			Device->GetMemoryManager().DumpMemory();
			Device->GetResourceHeapManager().DumpMemory();
		}
#endif
		VERIFYVULKANRESULT(Result);
	}

	FOldResourceHeapPage::FOldResourceHeapPage(FOldResourceHeap* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation)
		: Owner(InOwner)
		, DeviceMemoryAllocation(InDeviceMemoryAllocation)
		, MaxSize(0)
		, UsedSize(0)
		, PeakNumAllocations(0)
		, FrameFreed(0)
	{
		MaxSize = InDeviceMemoryAllocation->GetSize();
		FPair Pair;
		Pair.Offset = 0;
		Pair.Size = MaxSize;
		FreeList.Add(Pair);
	}

	FOldResourceHeapPage::~FOldResourceHeapPage()
	{
		check(!DeviceMemoryAllocation);
	}

	FOldResourceAllocation* FOldResourceHeapPage::TryAllocate(uint32 Size, uint32 Alignment, const char* File, uint32 Line)
	{
		//const uint32 Granularity = Owner->GetOwner()->GetParent()->GetLimits().bufferImageGranularity;
		FScopeLock ScopeLock(&GAllocationLock);
		for (int32 Index = 0; Index < FreeList.Num(); ++Index)
		{
			FPair& Entry = FreeList[Index];
			uint32 AllocatedOffset = Entry.Offset;
			uint32 AlignedOffset = Align(Entry.Offset, Alignment);
			uint32 AlignmentAdjustment = AlignedOffset - Entry.Offset;
			uint32 AllocatedSize = AlignmentAdjustment + Size;
			if (AllocatedSize <= Entry.Size)
			{
				if (AllocatedSize < Entry.Size)
				{
					// Modify current free entry in-place
					Entry.Size -= AllocatedSize;
					Entry.Offset += AllocatedSize;
				}
				else
				{
					// Remove this free entry
					FreeList.RemoveAtSwap(Index, 1, false);
				}

				UsedSize += AllocatedSize;

				FOldResourceAllocation* NewResourceAllocation = new FOldResourceAllocation(this, DeviceMemoryAllocation, Size, AlignedOffset, AllocatedSize, AllocatedOffset, File, Line);
				ResourceAllocations.Add(NewResourceAllocation);

				PeakNumAllocations = FMath::Max(PeakNumAllocations, ResourceAllocations.Num());
				return NewResourceAllocation;
			}
		}

		return nullptr;
	}

	void FOldResourceHeapPage::ReleaseAllocation(FOldResourceAllocation* Allocation)
	{
		{
			FScopeLock ScopeLock(&GAllocationLock);
			ResourceAllocations.RemoveSingleSwap(Allocation, false);

			FPair NewFree;
			NewFree.Offset = Allocation->AllocationOffset;
			NewFree.Size = Allocation->AllocationSize;

			FreeList.Add(NewFree);
		}

		UsedSize -= Allocation->AllocationSize;
		check(UsedSize >= 0);

		if (JoinFreeBlocks())
		{
			Owner->FreePage(this);
		}
	}

	bool FOldResourceHeapPage::JoinFreeBlocks()
	{
		FScopeLock ScopeLock(&GAllocationLock);
		if (FreeList.Num() > 1)
		{
			FreeList.Sort();

			for (int32 Index = FreeList.Num() - 1; Index > 0; --Index)
			{
				FPair& Current = FreeList[Index];
				FPair& Prev = FreeList[Index - 1];
				if (Prev.Offset + Prev.Size == Current.Offset)
				{
					Prev.Size += Current.Size;
					FreeList.RemoveAt(Index, 1, false);
				}
			}
		}

		if (FreeList.Num() == 1)
		{
			if (ResourceAllocations.Num() == 0)
			{
				check(UsedSize == 0);
				checkf(FreeList[0].Offset == 0 && FreeList[0].Size == MaxSize, TEXT("Memory leak, should have %d free, only have %d; missing %d bytes"), MaxSize, FreeList[0].Size, MaxSize - FreeList[0].Size);
				return true;
			}
		}

		return false;
	}

	FCriticalSection FResourceHeapManager::CS;

	FOldResourceHeap::FOldResourceHeap(FResourceHeapManager* InOwner, uint32 InMemoryTypeIndex, uint32 InPageSize)
		: Owner(InOwner)
		, MemoryTypeIndex(InMemoryTypeIndex)
		, DefaultPageSize(InPageSize)
		, PeakPageSize(0)
		, UsedMemory(0)
	{
	}

	FOldResourceHeap::~FOldResourceHeap()
	{
		ReleaseFreedPages(true);
		auto DeletePages = [&](TArray<FOldResourceHeapPage*>& UsedPages)
		{
			for (int32 Index = UsedPages.Num() - 1; Index >= 0; --Index)
			{
				FOldResourceHeapPage* Page = UsedPages[Index];
				if (Page->JoinFreeBlocks())
				{
					Owner->GetParent()->GetMemoryManager().Free(Page->DeviceMemoryAllocation);
					delete Page;
				}
				else
				{
					check(0);
				}
			}

			check(UsedPages.Num() == 0);
		};
		DeletePages(UsedBufferPages);
		DeletePages(UsedImagePages);

		for (int32 Index = 0; Index < FreePages.Num(); ++Index)
		{
			FOldResourceHeapPage* Page = FreePages[Index];
			Owner->GetParent()->GetMemoryManager().Free(Page->DeviceMemoryAllocation);
			delete Page;
		}
	}

	void FOldResourceHeap::FreePage(FOldResourceHeapPage* InPage)
	{
		FScopeLock ScopeLock(&CriticalSection);
		check(InPage->JoinFreeBlocks());
		int32 Index = -1;
		if (UsedBufferPages.Find(InPage, Index))
		{
			UsedBufferPages.RemoveAtSwap(Index, 1, false);
		}
		else
		{
			int32 Removed = UsedImagePages.RemoveSingleSwap(InPage, false);
			check(Removed);
		}
		InPage->FrameFreed = GFrameNumberRenderThread;
		FreePages.Add(InPage);
	}

	void FOldResourceHeap::ReleaseFreedPages(bool bImmediately)
	{
		FOldResourceHeapPage* PageToRelease = nullptr;

		{
			FScopeLock ScopeLock(&CriticalSection);
			for (int32 Index = 0; Index < FreePages.Num(); ++Index)
			{
				FOldResourceHeapPage* Page = FreePages[Index];
				if (bImmediately || Page->FrameFreed + NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS < GFrameNumberRenderThread)
				{
					PageToRelease = Page;
					FreePages.RemoveAtSwap(Index, 1, false);
					break;
				}
			}
		}

		if (PageToRelease)
		{
			Owner->GetParent()->GetMemoryManager().Free(PageToRelease->DeviceMemoryAllocation);
			UsedMemory -= PageToRelease->MaxSize;
			delete PageToRelease;
		}
	}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	void FOldResourceHeap::DumpMemory()
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("%d Free Pages"), FreePages.Num());

		auto DumpPages = [&](TArray<FOldResourceHeapPage*>& UsedPages, const TCHAR* TypeName)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("\t%s Pages: %d Used, Peak Allocation Size on a Page %d"), TypeName, UsedPages.Num(), PeakPageSize);
			uint64 SubAllocUsedMemory = 0;
			uint32 NumSuballocations = 0;
			for (int32 Index = 0; Index < UsedPages.Num(); ++Index)
			{
				SubAllocUsedMemory += UsedPages[Index]->UsedSize;
				NumSuballocations += UsedPages[Index]->ResourceAllocations.Num();

				UE_LOG(LogVulkanRHI, Display, TEXT("\t\t%d: %4d suballocs, %4d free chunks (%d used/%d free/%d max) DeviceMemory %p"), Index, UsedPages[Index]->ResourceAllocations.Num(), UsedPages[Index]->FreeList.Num(), UsedPages[Index]->UsedSize, UsedPages[Index]->MaxSize - UsedPages[Index]->UsedSize, UsedPages[Index]->MaxSize, (void*)UsedPages[Index]->DeviceMemoryAllocation->GetHandle());
			}

			UE_LOG(LogVulkanRHI, Display, TEXT("\tUsed Memory %d in %d Suballocations"), SubAllocUsedMemory, NumSuballocations);
		};

		DumpPages(UsedBufferPages, TEXT("Buffer"));
		DumpPages(UsedImagePages, TEXT("Image"));
		//UE_LOG(LogVulkanRHI, Display, TEXT("\tUsed Memory %d in %d Suballocations, Free Memory in pages %d, Heap free memory %ull"), SubAllocUsedMemory, NumSuballocations, FreeMemory, TotalMemory - UsedMemory);
	}
#endif

	FOldResourceAllocation* FOldResourceHeap::AllocateResource(uint32 Size, uint32 Alignment, bool bIsImage, bool bMapAllocation, const char* File, uint32 Line)
	{
		FScopeLock ScopeLock(&CriticalSection);

		TArray<FOldResourceHeapPage*>& UsedPages = bIsImage ? UsedImagePages : UsedBufferPages;
#if VULKAN_SINGLE_ALLOCATION_PER_RESOURCE
		uint32 AllocationSize = Size;
#else
		if (Size < DefaultPageSize)
		{
			// Check Used pages to see if we can fit this in
			for (int32 Index = 0; Index < UsedPages.Num(); ++Index)
			{
				FOldResourceHeapPage* Page = UsedPages[Index];
				if (Page->DeviceMemoryAllocation->IsMapped() == bMapAllocation)
				{
					FOldResourceAllocation* ResourceAllocation = Page->TryAllocate(Size, Alignment, File, Line);
					if (ResourceAllocation)
					{
						return ResourceAllocation;
					}
				}
			}
		}

		for (int32 Index = 0; Index < FreePages.Num(); ++Index)
		{
			FOldResourceHeapPage* Page = FreePages[Index];
			if (Page->DeviceMemoryAllocation->IsMapped() == bMapAllocation)
			{
				FOldResourceAllocation* ResourceAllocation = Page->TryAllocate(Size, Alignment, File, Line);
				if (ResourceAllocation)
				{
					FreePages.RemoveSingleSwap(Page, false);
					UsedPages.Add(Page);
					return ResourceAllocation;
				}
			}
		}
		uint32 AllocationSize = FMath::Max(Size, DefaultPageSize);
#endif
		FDeviceMemoryAllocation* DeviceMemoryAllocation = Owner->GetParent()->GetMemoryManager().Alloc(AllocationSize, MemoryTypeIndex, File, Line);
		FOldResourceHeapPage* NewPage = new FOldResourceHeapPage(this, DeviceMemoryAllocation);
		UsedPages.Add(NewPage);

		UsedMemory += AllocationSize;

		PeakPageSize = FMath::Max(PeakPageSize, AllocationSize);

		if (bMapAllocation)
		{
			DeviceMemoryAllocation->Map(AllocationSize, 0);
		}

		return NewPage->Allocate(Size, Alignment, File, Line);
	}


	FResourceHeapManager::FResourceHeapManager(FVulkanDevice* InDevice)
		: FDeviceChild(InDevice)
		, DeviceMemoryManager(&InDevice->GetMemoryManager())
		, GPUHeap(nullptr)
		, UploadToGPUHeap(nullptr)
		, DownloadToCPUHeap(nullptr)
	{
	}

	FResourceHeapManager::~FResourceHeapManager()
	{
		Deinit();
	}

	void FResourceHeapManager::Init()
	{
		FDeviceMemoryManager& MemoryManager = Device->GetMemoryManager();
		const uint32 TypeBits = (1 << MemoryManager.GetNumMemoryTypes()) - 1;

		const VkPhysicalDeviceMemoryProperties& MemoryProperties = MemoryManager.GetMemoryProperties();

		ResourceTypeHeaps.AddZeroed(MemoryProperties.memoryTypeCount);

		TArray<uint64> RemainingHeapSizes;
		TArray<uint64> NumTypesPerHeap;
		for (uint32 Index = 0; Index < MemoryProperties.memoryHeapCount; ++Index)
		{
			RemainingHeapSizes.Add(MemoryProperties.memoryHeaps[Index].size);
			NumTypesPerHeap.Add(0);
		}

		for (uint32 Index = 0; Index < MemoryProperties.memoryTypeCount; ++Index)
		{
			++NumTypesPerHeap[MemoryProperties.memoryTypes[Index].heapIndex];
		}

		// Setup main GPU heap
		uint32 NumGPUAllocations = 0;
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(MemoryManager.GetMemoryTypeFromProperties(TypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &TypeIndex));
			uint64 HeapSize = MemoryProperties.memoryHeaps[MemoryProperties.memoryTypes[TypeIndex].heapIndex].size;
			GPUHeap = new FOldResourceHeap(this, TypeIndex, GPU_ONLY_HEAP_PAGE_SIZE);
			ResourceTypeHeaps[TypeIndex] = GPUHeap;
			RemainingHeapSizes[MemoryProperties.memoryTypes[TypeIndex].heapIndex] -= HeapSize;
			NumGPUAllocations = HeapSize / GPU_ONLY_HEAP_PAGE_SIZE;
		}

		// Upload heap
		uint32 NumUploadAllocations = 0;
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(MemoryManager.GetMemoryTypeFromProperties(TypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &TypeIndex));
			uint64 HeapSize = MemoryProperties.memoryHeaps[MemoryProperties.memoryTypes[TypeIndex].heapIndex].size;
			UploadToGPUHeap = new FOldResourceHeap(this, TypeIndex, STAGING_HEAP_PAGE_SIZE);
			ResourceTypeHeaps[TypeIndex] = UploadToGPUHeap;
			RemainingHeapSizes[MemoryProperties.memoryTypes[TypeIndex].heapIndex] -= HeapSize;
			NumUploadAllocations = HeapSize / STAGING_HEAP_PAGE_SIZE;
		}

		// Download heap
		uint32 NumDownloadAllocations = 0;
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(MemoryManager.GetMemoryTypeFromProperties(TypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, &TypeIndex));
			uint64 HeapSize = MemoryProperties.memoryHeaps[MemoryProperties.memoryTypes[TypeIndex].heapIndex].size;
			DownloadToCPUHeap = new FOldResourceHeap(this, TypeIndex, STAGING_HEAP_PAGE_SIZE);
			ResourceTypeHeaps[TypeIndex] = DownloadToCPUHeap;
			RemainingHeapSizes[MemoryProperties.memoryTypes[TypeIndex].heapIndex] -= HeapSize;
			NumDownloadAllocations = HeapSize / STAGING_HEAP_PAGE_SIZE;
		}

		uint32 NumMemoryAllocations = (uint64)Device->GetLimits().maxMemoryAllocationCount;
		if (NumGPUAllocations + NumDownloadAllocations + NumUploadAllocations > NumMemoryAllocations)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Too many allocations (%d) per heap size (G:%d U:%d D:%d), might run into slow path in the driver"), NumGPUAllocations + NumDownloadAllocations + NumUploadAllocations, NumGPUAllocations, NumUploadAllocations, NumDownloadAllocations);
		}
	}

	void FResourceHeapManager::Deinit()
	{
		DestroyResourceAllocations();

		for (int32 Index = 0; Index < ResourceTypeHeaps.Num(); ++Index)
		{
			delete ResourceTypeHeaps[Index];
		}
		ResourceTypeHeaps.Empty(0);
	}

	void FResourceHeapManager::DestroyResourceAllocations()
	{
		ReleaseFreedResources(true);

		for (int32 Index = UsedBufferAllocations.Num() - 1; Index >= 0; --Index)
		{
			FBufferAllocation* BufferAllocation = UsedBufferAllocations[Index];
			if (BufferAllocation->JoinFreeBlocks())
			{
				BufferAllocation->Destroy(GetParent());
				GetParent()->GetMemoryManager().Free(BufferAllocation->MemoryAllocation);
				delete BufferAllocation;
			}
			else
			{
				check(0);
			}
		}
		UsedBufferAllocations.Empty(0);

		for (int32 Index = 0; Index < FreeBufferAllocations.Num(); ++Index)
		{
			FBufferAllocation* BufferAllocation = FreeBufferAllocations[Index];
			BufferAllocation->Destroy(GetParent());
			GetParent()->GetMemoryManager().Free(BufferAllocation->MemoryAllocation);
			delete BufferAllocation;
		}
		FreeBufferAllocations.Empty(0);
	}

	void FResourceHeapManager::ReleaseFreedResources(bool bImmediately)
	{
		FBufferAllocation* BufferAllocationToRelease = nullptr;

		{
			FScopeLock ScopeLock(&CS);
			for (int32 Index = 0; Index < FreeBufferAllocations.Num(); ++Index)
			{
				FBufferAllocation* BufferAllocation = FreeBufferAllocations[Index];
				if (bImmediately || BufferAllocation->FrameFreed + NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS < GFrameNumberRenderThread)
				{
					BufferAllocationToRelease = BufferAllocation;
					FreeBufferAllocations.RemoveAtSwap(Index, 1, false);
					break;
				}
			}
		}

		if (BufferAllocationToRelease)
		{
			BufferAllocationToRelease->Destroy(GetParent());
			GetParent()->GetMemoryManager().Free(BufferAllocationToRelease->MemoryAllocation);
			//UsedMemory -= BufferAllocationToRelease->MaxSize;
			delete BufferAllocationToRelease;
		}
	}

	void FResourceHeapManager::ReleaseFreedPages()
	{
		FOldResourceHeap* Heap = ResourceTypeHeaps[GFrameNumberRenderThread % ResourceTypeHeaps.Num()];
		if (Heap)
		{
			Heap->ReleaseFreedPages(false);
		}

		ReleaseFreedResources(false);
	}

	FBufferSuballocation* FResourceHeapManager::AllocateBuffer(uint32 Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
	{
		const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
		uint32 Alignment = ((BufferUsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0) ? (uint32)Limits.minUniformBufferOffsetAlignment : 1;
		Alignment = FMath::Max(Alignment, ((BufferUsageFlags & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)) != 0) ? (uint32)Limits.minTexelBufferOffsetAlignment : 1u);
		Alignment = FMath::Max(Alignment, ((BufferUsageFlags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0) ? (uint32)Limits.minStorageBufferOffsetAlignment : 1u);

		FScopeLock ScopeLock(&CS);

		for (int32 Index = 0; Index < UsedBufferAllocations.Num(); ++Index)
		{
			FBufferAllocation* BufferAllocation = UsedBufferAllocations[Index];
			if ((BufferAllocation->BufferUsageFlags & BufferUsageFlags) == BufferUsageFlags &&
				(BufferAllocation->MemoryPropertyFlags & MemoryPropertyFlags) == MemoryPropertyFlags)
			{
				FBufferSuballocation* Suballocation = (FBufferSuballocation*)BufferAllocation->TryAllocateNoLocking(Size, Alignment, File, Line);
				if (Suballocation)
				{
					return Suballocation;
				}
			}
		}

		for (int32 Index = 0; Index < FreeBufferAllocations.Num(); ++Index)
		{
			FBufferAllocation* BufferAllocation = FreeBufferAllocations[Index];
			if ((BufferAllocation->BufferUsageFlags & BufferUsageFlags) == BufferUsageFlags &&
				(BufferAllocation->MemoryPropertyFlags & MemoryPropertyFlags) == MemoryPropertyFlags)
			{
				FBufferSuballocation* Suballocation = (FBufferSuballocation*)BufferAllocation->TryAllocateNoLocking(Size, Alignment, File, Line);
				if (Suballocation)
				{
					FreeBufferAllocations.RemoveAtSwap(Index, 1, false);
					UsedBufferAllocations.Add(BufferAllocation);
					return Suballocation;
				}
			}
		}

		// New Buffer
		uint32 BufferSize = FMath::Max(Size, (uint32)BufferAllocationSize);

		VkBuffer Buffer;
		VkBufferCreateInfo BufferCreateInfo;
		FMemory::Memzero(BufferCreateInfo);
		BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		BufferCreateInfo.size = BufferSize;
		BufferCreateInfo.usage = BufferUsageFlags;
		VERIFYVULKANRESULT(VulkanRHI::vkCreateBuffer(Device->GetInstanceHandle(), &BufferCreateInfo, nullptr, &Buffer));

		VkMemoryRequirements MemReqs;
		VulkanRHI::vkGetBufferMemoryRequirements(Device->GetInstanceHandle(), Buffer, &MemReqs);

		uint32 MemoryTypeIndex;
		VERIFYVULKANRESULT(Device->GetMemoryManager().GetMemoryTypeFromProperties(MemReqs.memoryTypeBits, MemoryPropertyFlags, &MemoryTypeIndex));

		FDeviceMemoryAllocation* DeviceMemoryAllocation = Device->GetMemoryManager().Alloc(BufferSize, MemoryTypeIndex, File, Line);
		VERIFYVULKANRESULT(VulkanRHI::vkBindBufferMemory(Device->GetInstanceHandle(), Buffer, DeviceMemoryAllocation->GetHandle(), 0));
		if (DeviceMemoryAllocation->CanBeMapped())
		{
			DeviceMemoryAllocation->Map(BufferSize, 0);
		}

		FBufferAllocation* BufferAllocation = new FBufferAllocation(this, DeviceMemoryAllocation, MemoryTypeIndex, MemoryPropertyFlags, MemReqs.alignment, Buffer, BufferUsageFlags);
		UsedBufferAllocations.Add(BufferAllocation);

		return (FBufferSuballocation*)BufferAllocation->TryAllocateNoLocking(Size, Alignment, File, Line);
	}

	void FResourceHeapManager::ReleaseBuffer(FBufferAllocation* BufferAllocation)
	{
		FScopeLock ScopeLock(&CS);
		check(BufferAllocation->JoinFreeBlocks());
		UsedBufferAllocations.RemoveSingleSwap(BufferAllocation, false);
		BufferAllocation->FrameFreed = GFrameNumberRenderThread;
		FreeBufferAllocations.Add(BufferAllocation);
	}
#if 0
	FImageSuballocation* FResourceHeapManager::AllocateImage(uint32 Size, VkImageUsageFlags ImageUsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
	{
		FScopeLock ScopeLock(&CS);

		for (int32 Index = 0; Index < UsedImageAllocations.Num(); ++Index)
		{
			FImageAllocation* ImageAllocation = UsedImageAllocations[Index];
			if ((ImageAllocation->ImageUsageFlags & ImageUsageFlags) == ImageUsageFlags &&
				(ImageAllocation->MemoryPropertyFlags & MemoryPropertyFlags) == MemoryPropertyFlags)
			{
				FImageSuballocation* Suballocation = (FImageSuballocation*)ImageAllocation->TryAllocateNoLocking(Size, File, Line);
				if (Suballocation)
				{
					return Suballocation;
				}
			}
		}

		for (int32 Index = 0; Index < FreeImageAllocations.Num(); ++Index)
		{
			FImageAllocation* ImageAllocation = FreeImageAllocations[Index];
			if ((ImageAllocation->ImageUsageFlags & ImageUsageFlags) == ImageUsageFlags &&
				(ImageAllocation->MemoryPropertyFlags & MemoryPropertyFlags) == MemoryPropertyFlags)
			{
				FImageSuballocation* Suballocation = (FImageSuballocation*)ImageAllocation->TryAllocateNoLocking(Size, File, Line);
				if (Suballocation)
				{
					FreeImageAllocations.RemoveAtSwap(Index, 1, false);
					UsedImageAllocations.Add(ImageAllocation);
					return Suballocation;
				}
			}
		}

		// New Image
		uint32 ImageSize = FMath::Max(Size, (uint32)ImageAllocationSize);

		VkImage Image;
		VkImageCreateInfo ImageCreateInfo;
		FMemory::Memzero(ImageCreateInfo);
		ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
		ImageCreateInfo.size = ImageSize;
		ImageCreateInfo.usage = ImageUsageFlags;
		VERIFYVULKANRESULT(VulkanRHI::vkCreateImage(Device->GetInstanceHandle(), &ImageCreateInfo, nullptr, &Image));

		VkMemoryRequirements MemReqs;
		VulkanRHI::vkGetImageMemoryRequirements(Device->GetInstanceHandle(), Image, &MemReqs);

		uint32 MemoryTypeIndex;
		VERIFYVULKANRESULT(Device->GetMemoryManager().GetMemoryTypeFromProperties(MemReqs.memoryTypeBits, MemoryPropertyFlags, &MemoryTypeIndex));

		FDeviceMemoryAllocation* DeviceMemoryAllocation = Device->GetMemoryManager().Alloc(ImageSize, MemoryTypeIndex, File, Line);
		VERIFYVULKANRESULT(VulkanRHI::vkBindImageMemory(Device->GetInstanceHandle(), Image, DeviceMemoryAllocation->GetHandle(), 0));
		if (DeviceMemoryAllocation->CanBeMapped())
		{
			DeviceMemoryAllocation->Map(ImageSize, 0);
		}

		FImageAllocation* ImageAllocation = new FImageAllocation(this, DeviceMemoryAllocation, MemoryTypeIndex, MemoryPropertyFlags, MemReqs.alignment, Image, ImageUsageFlags);
		UsedImageAllocations.Add(ImageAllocation);

		return (FImageSuballocation*)ImageAllocation->TryAllocateNoLocking(Size, File, Line);
	}

	void FResourceHeapManager::ReleaseImage(FImageAllocation* ImageAllocation)
	{
		FScopeLock ScopeLock(&CS);
		check(ImageAllocation->JoinFreeBlocks());
		UsedImageAllocations.RemoveSingleSwap(ImageAllocation, false);
		ImageAllocation->FrameFreed = GFrameNumberRenderThread;
		FreeImageAllocations.Add(ImageAllocation);
	}
#endif

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	void FResourceHeapManager::DumpMemory()
	{
		FScopeLock ScopeLock(&CS);

		for (int32 Index = 0; Index < ResourceTypeHeaps.Num(); ++Index)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("Heap %d, Memory Type Index %d"), Index, ResourceTypeHeaps[Index]->MemoryTypeIndex);
			ResourceTypeHeaps[Index]->DumpMemory();
		}

		UE_LOG(LogVulkanRHI, Display, TEXT("Buffer Allocations: %d Used / %d Free"), UsedBufferAllocations.Num(), FreeBufferAllocations.Num());
		if (UsedBufferAllocations.Num() > 0)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("Index  BufferHandle   DeviceMemoryHandle MemFlags BufferFlags #Suballocs #FreeChunks UsedSize/MaxSize"));
			for (int32 Index = 0; Index < UsedBufferAllocations.Num(); ++Index)
			{
				FBufferAllocation* BA = UsedBufferAllocations[Index];
				UE_LOG(LogVulkanRHI, Display, TEXT("%6d %p %p 0x%06x 0x%08x %6d   %6d    %d/%d"), Index, (void*)BA->Buffer, (void*)BA->MemoryAllocation->GetHandle(), BA->MemoryPropertyFlags, BA->BufferUsageFlags, BA->Suballocations.Num(), BA->FreeList.Num(), BA->UsedSize, BA->MaxSize);
			}
		}
	}
#endif


	FBufferSuballocation::~FBufferSuballocation()
	{
		Owner->Release(this);
	}


	FCriticalSection FSubresourceAllocator::CS;

	bool FSubresourceAllocator::JoinFreeBlocks()
	{
		FScopeLock ScopeLock(&CS);
		if (FreeList.Num() > 1)
		{
			FreeList.Sort();

			for (int32 Index = FreeList.Num() - 1; Index > 0; --Index)
			{
				FPair& Current = FreeList[Index];
				FPair& Prev = FreeList[Index - 1];
				if (Prev.Offset + Prev.Size == Current.Offset)
				{
					Prev.Size += Current.Size;
					FreeList.RemoveAt(Index, 1, false);
				}
			}
		}

		if (FreeList.Num() == 1)
		{
			if (Suballocations.Num() == 0)
			{
				check(UsedSize == 0);
				checkf(FreeList[0].Offset == 0 && FreeList[0].Size == MaxSize, TEXT("Resource Suballocation leak, should have %d free, only have %d; missing %d bytes"), MaxSize, FreeList[0].Size, MaxSize - FreeList[0].Size);
				return true;
			}
		}

		return false;
	}
	
	FResourceSuballocation* FSubresourceAllocator::TryAllocateNoLocking(uint32 InSize, uint32 InAlignment, const char* File, uint32 Line)
	{
		InAlignment = FMath::Max(InAlignment, Alignment);
		for (int32 Index = 0; Index < FreeList.Num(); ++Index)
		{
			FPair& Entry = FreeList[Index];
			uint32 AllocatedOffset = Entry.Offset;
			uint32 AlignedOffset = Align(Entry.Offset, InAlignment);
			uint32 AlignmentAdjustment = AlignedOffset - Entry.Offset;
			uint32 AllocatedSize = AlignmentAdjustment + InSize;
			if (AllocatedSize <= Entry.Size)
			{
				if (AllocatedSize < Entry.Size)
				{
					// Modify current free entry in-place
					Entry.Size -= AllocatedSize;
					Entry.Offset += AllocatedSize;
				}
				else
				{
					// Remove this free entry
					FreeList.RemoveAtSwap(Index, 1, false);
				}

				UsedSize += AllocatedSize;

				FResourceSuballocation* NewSuballocation = CreateSubAllocation(InSize, AlignedOffset, AllocatedSize, AllocatedOffset);// , File, Line);
				Suballocations.Add(NewSuballocation);

				//PeakNumAllocations = FMath::Max(PeakNumAllocations, ResourceAllocations.Num());
				return NewSuballocation;
			}
		}

		return nullptr;
	}

	void FBufferAllocation::Release(FBufferSuballocation* Suballocation)
	{
		{
			FScopeLock ScopeLock(&CS);
			Suballocations.RemoveSingleSwap(Suballocation, false);

			FPair NewFree;
			NewFree.Offset = Suballocation->AllocationOffset;
			NewFree.Size = Suballocation->AllocationSize;

			FreeList.Add(NewFree);
		}

		UsedSize -= Suballocation->AllocationSize;
		check(UsedSize >= 0);

		if (JoinFreeBlocks())
		{
			Owner->ReleaseBuffer(this);
		}
	}

	void FBufferAllocation::Destroy(FVulkanDevice* Device)
	{
		// Does not need to go in the deferred deletion queue
		VulkanRHI::vkDestroyBuffer(Device->GetInstanceHandle(), Buffer, nullptr);
		Buffer = VK_NULL_HANDLE;
	}

	FStagingBuffer::~FStagingBuffer()
	{
		checkf(!ResourceAllocation, TEXT("Staging Buffer not released!"));
	}

	void FStagingBuffer::Destroy(FVulkanDevice* Device)
	{
		check(ResourceAllocation);

		// Does not need to go in the deferred deletion queue
		VulkanRHI::vkDestroyBuffer(Device->GetInstanceHandle(), Buffer, nullptr);
		Buffer = VK_NULL_HANDLE;
		ResourceAllocation = nullptr;
		//Memory.Free(Allocation);
	}

	FStagingManager::~FStagingManager()
	{
		check(UsedStagingBuffers.Num() == 0);
		check(PendingFreeStagingBuffers.Num() == 0);
		check(FreeStagingBuffers.Num() == 0);
	}
	
	void FStagingManager::Init(FVulkanDevice* InDevice, FVulkanQueue* InQueue)
	{
		Device = InDevice;
		Queue = InQueue;
	}

	void FStagingManager::Deinit()
	{
		ProcessPendingFree(true);

		check(UsedStagingBuffers.Num() == 0);
		check(PendingFreeStagingBuffers.Num() == 0);
		check(FreeStagingBuffers.Num() == 0);
	}

	FStagingBuffer* FStagingManager::AcquireBuffer(uint32 Size, VkBufferUsageFlags InUsageFlags, bool bCPURead)
	{
		//#todo-rco: Better locking!
		{
			FScopeLock Lock(&GAllocationLock);

			for (int32 Index = 0; Index < FreeStagingBuffers.Num(); ++Index)
			{
				FPendingItem& Item = FreeStagingBuffers[Index];
				FStagingBuffer* FreeBuffer = (FStagingBuffer*)Item.Resource;
				if (FreeBuffer->ResourceAllocation->GetSize() == Size && FreeBuffer->bCPURead == bCPURead)
				{
					FreeStagingBuffers.RemoveAtSwap(Index, 1, false);
					UsedStagingBuffers.Add((FStagingBuffer*)FreeBuffer);
					return FreeBuffer;
				}
			}
		}

		FStagingBuffer* StagingBuffer = new FStagingBuffer();

		VkBufferCreateInfo StagingBufferCreateInfo;
		FMemory::Memzero(StagingBufferCreateInfo);
		StagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		StagingBufferCreateInfo.size = Size;
		StagingBufferCreateInfo.usage = InUsageFlags;

		VkDevice VulkanDevice = Device->GetInstanceHandle();

		VERIFYVULKANRESULT(VulkanRHI::vkCreateBuffer(VulkanDevice, &StagingBufferCreateInfo, nullptr, &StagingBuffer->Buffer));

		VkMemoryRequirements MemReqs;
		VulkanRHI::vkGetBufferMemoryRequirements(VulkanDevice, StagingBuffer->Buffer, &MemReqs);

		StagingBuffer->ResourceAllocation = Device->GetResourceHeapManager().AllocateBufferMemory(MemReqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | (bCPURead ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), __FILE__, __LINE__);
		StagingBuffer->bCPURead = bCPURead;
		StagingBuffer->ResourceAllocation->BindBuffer(Device, StagingBuffer->Buffer);

		{
			FScopeLock Lock(&GAllocationLock);
			UsedStagingBuffers.Add(StagingBuffer);
		}
		return StagingBuffer;
	}

	void FStagingManager::ReleaseBuffer(FVulkanCmdBuffer* CmdBuffer, FStagingBuffer*& StagingBuffer)
	{
		FScopeLock Lock(&GAllocationLock);
		UsedStagingBuffers.RemoveSingleSwap(StagingBuffer);
		FPendingItem* Entry = new(PendingFreeStagingBuffers) FPendingItem;
		Entry->CmdBuffer = CmdBuffer;
		Entry->FenceCounter = CmdBuffer ? CmdBuffer->GetFenceSignaledCounter() : 0;
		Entry->Resource = StagingBuffer;
		StagingBuffer = nullptr;
	}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	void FStagingManager::DumpMemory()
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("StagingManager %d Used %d Pending Free %d Free"), UsedStagingBuffers.Num(), PendingFreeStagingBuffers.Num(), FreeStagingBuffers.Num());
		UE_LOG(LogVulkanRHI, Display, TEXT("Used   BufferHandle ResourceAllocation"));
		for (int32 Index = 0; Index < UsedStagingBuffers.Num(); ++Index)
		{
			FStagingBuffer* Buffer = UsedStagingBuffers[Index];
			UE_LOG(LogVulkanRHI, Display, TEXT("%6d %p %p"), Index, (void*)Buffer->GetHandle(), (void*)Buffer->ResourceAllocation->GetHandle());
		}

		UE_LOG(LogVulkanRHI, Display, TEXT("Pending CmdBuffer   Fence   BufferHandle ResourceAllocation"));
		for (int32 Index = 0; Index < PendingFreeStagingBuffers.Num(); ++Index)
		{
			FPendingItem& Item = PendingFreeStagingBuffers[Index];
			FStagingBuffer* Buffer = (FStagingBuffer*)Item.Resource;
			UE_LOG(LogVulkanRHI, Display, TEXT("%6d %p %p %p %p"), Index, (void*)Item.CmdBuffer->GetHandle(), (void*)Item.FenceCounter, (void*)Buffer->GetHandle(), (void*)Buffer->ResourceAllocation->GetHandle());
		}

		UE_LOG(LogVulkanRHI, Display, TEXT("Free   BufferHandle ResourceAllocation"));
		for (int32 Index = 0; Index < FreeStagingBuffers.Num(); ++Index)
		{
			FStagingBuffer* Buffer = (FStagingBuffer*)FreeStagingBuffers[Index].Resource;
			UE_LOG(LogVulkanRHI, Display, TEXT("%6d %p %p"), Index, (void*)Buffer->GetHandle(), (void*)Buffer->ResourceAllocation->GetHandle());
		}
	}
#endif

	void FStagingManager::ProcessPendingFree(bool bImmediately)
	{
		FScopeLock Lock(&GAllocationLock);
		int32 NumOriginalFreeBuffers = FreeStagingBuffers.Num();
		for (int32 Index = PendingFreeStagingBuffers.Num() - 1; Index >= 0; --Index)
		{
			FPendingItem& Entry = PendingFreeStagingBuffers[Index];
			if (bImmediately || !Entry.CmdBuffer || Entry.FenceCounter < Entry.CmdBuffer->GetFenceSignaledCounter())
			{
				FPendingItem NewEntry = Entry;
				NewEntry.FenceCounter = Entry.CmdBuffer ? Entry.CmdBuffer->GetFenceSignaledCounter() : 0;
				PendingFreeStagingBuffers.RemoveAtSwap(Index, 1, false);
				FreeStagingBuffers.Add(NewEntry);
			}
		}

		int32 NumFreeBuffers = bImmediately ? FreeStagingBuffers.Num() : NumOriginalFreeBuffers;
		for (int32 Index = NumFreeBuffers - 1; Index >= 0; --Index)
		{
			FPendingItem& Entry = FreeStagingBuffers[Index];
			if (bImmediately || !Entry.CmdBuffer || Entry.FenceCounter  + NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS < Entry.CmdBuffer->GetFenceSignaledCounter())
			{
				Entry.Resource->Destroy(Device);
				delete Entry.Resource;
				FreeStagingBuffers.RemoveAtSwap(Index, 1, false);
			}
		}
	}

	FFence::FFence(FVulkanDevice* InDevice, FFenceManager* InOwner)
		: State(FFence::EState::NotReady)
		, Owner(InOwner)
	{
		VkFenceCreateInfo Info;
		FMemory::Memzero(Info);
		Info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VERIFYVULKANRESULT(VulkanRHI::vkCreateFence(InDevice->GetInstanceHandle(), &Info, nullptr, &Handle));
	}

	FFence::~FFence()
	{
		checkf(Handle == VK_NULL_HANDLE, TEXT("Didn't get properly destroyed by FFenceManager!"));
	}

	FFenceManager::~FFenceManager()
	{
		check(UsedFences.Num() == 0);
	}

	inline void FFenceManager::DestroyFence(FFence* Fence)
	{
		// Does not need to go in the deferred deletion queue
		VulkanRHI::vkDestroyFence(Device->GetInstanceHandle(), Fence->GetHandle(), nullptr);
		Fence->Handle = VK_NULL_HANDLE;
		delete Fence;
	}

	void FFenceManager::Init(FVulkanDevice* InDevice)
	{
		Device = InDevice;
	}

	void FFenceManager::Deinit()
	{
		FScopeLock Lock(&GFenceLock);
		checkf(UsedFences.Num() == 0, TEXT("No all fences are done!"));
		VkDevice DeviceHandle = Device->GetInstanceHandle();
		for (FFence* Fence : FreeFences)
		{
			DestroyFence(Fence);
		}
	}

	FFence* FFenceManager::AllocateFence()
	{
		FScopeLock Lock(&GFenceLock);
		if (FreeFences.Num() != 0)
		{
			FFence* Fence = FreeFences[0];
			FreeFences.RemoveAtSwap(0, 1, false);
			UsedFences.Add(Fence);
			return Fence;
		}

		FFence* NewFence = new FFence(Device, this);
		UsedFences.Add(NewFence);
		return NewFence;
	}

	// Sets it to nullptr
	void FFenceManager::ReleaseFence(FFence*& Fence)
	{
		FScopeLock Lock(&GFenceLock);
		ResetFence(Fence);
		UsedFences.RemoveSingleSwap(Fence);
#if VULKAN_REUSE_FENCES
		FreeFences.Add(Fence);
#else
		DestroyFence(Fence);
#endif
		Fence = nullptr;
	}

	void FFenceManager::WaitAndReleaseFence(FFence*& Fence, uint64 TimeInNanoseconds)
	{
		FScopeLock Lock(&GFenceLock);
		if (!Fence->IsSignaled())
		{
			WaitForFence(Fence, TimeInNanoseconds);
		}

		ResetFence(Fence);
		UsedFences.RemoveSingleSwap(Fence);
		FreeFences.Add(Fence);
		Fence = nullptr;
	}

	bool FFenceManager::CheckFenceState(FFence* Fence)
	{
		check(UsedFences.Contains(Fence));
		check(Fence->State == FFence::EState::NotReady);
		VkResult Result = VulkanRHI::vkGetFenceStatus(Device->GetInstanceHandle(), Fence->Handle);
		switch (Result)
		{
		case VK_SUCCESS:
			Fence->State = FFence::EState::Signaled;
			return true;

		case VK_NOT_READY:
			break;

		default:
			VERIFYVULKANRESULT(Result);
			break;
		}

		return false;
	}

	bool FFenceManager::WaitForFence(FFence* Fence, uint64 TimeInNanoseconds)
	{
		check(UsedFences.Contains(Fence));
		check(Fence->State == FFence::EState::NotReady);
		VkResult Result = VulkanRHI::vkWaitForFences(Device->GetInstanceHandle(), 1, &Fence->Handle, true, TimeInNanoseconds);
		switch (Result)
		{
		case VK_SUCCESS:
			Fence->State = FFence::EState::Signaled;
			return true;
		case VK_TIMEOUT:
			break;
		default:
			VERIFYVULKANRESULT(Result);
			break;
		}

		return false;
	}

	void FFenceManager::ResetFence(FFence* Fence)
	{
		if (Fence->State != FFence::EState::NotReady)
		{
			VERIFYVULKANRESULT(VulkanRHI::vkResetFences(Device->GetInstanceHandle(), 1, &Fence->Handle));
			Fence->State = FFence::EState::NotReady;
		}
	}


	FDeferredDeletionQueue::FDeferredDeletionQueue(FVulkanDevice* InDevice)
		: FDeviceChild(InDevice)
	{
	}

	FDeferredDeletionQueue::~FDeferredDeletionQueue()
	{
		check(Entries.Num() == 0);
	}

	void FDeferredDeletionQueue::EnqueueGenericResource(EType Type, void* Handle)
	{
		FVulkanQueue* Queue = Device->GetQueue();

		FEntry Entry;
		Queue->GetLastSubmittedInfo(Entry.CmdBuffer, Entry.FenceCounter);
		Entry.Handle = Handle;
		Entry.StructureType = Type;

		{
			FScopeLock ScopeLock(&CS);
			Entries.Add(Entry);
		}
	}

	void FDeferredDeletionQueue::ReleaseResources(bool bDeleteImmediately)
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanDeletionQueue);
		FScopeLock ScopeLock(&CS);

		VkDevice DeviceHandle = Device->GetInstanceHandle();

		// Traverse list backwards so the swap switches to elements already tested
		for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
		{
			FEntry* Entry = &Entries[Index];
			if (bDeleteImmediately || Entry->FenceCounter < Entry->CmdBuffer->GetFenceSignaledCounter())
			{
				switch (Entry->StructureType)
				{
#define VKSWITCH(Type)	case EType::Type: VulkanRHI::vkDestroy##Type(DeviceHandle, (Vk##Type)Entry->Handle, nullptr); break
				VKSWITCH(RenderPass);
				VKSWITCH(Buffer);
				VKSWITCH(BufferView);
				VKSWITCH(Image);
				VKSWITCH(ImageView);
				VKSWITCH(Pipeline);
				VKSWITCH(PipelineLayout);
				VKSWITCH(Framebuffer);
				VKSWITCH(DescriptorSetLayout);
				VKSWITCH(Sampler);
				VKSWITCH(Semaphore);
				VKSWITCH(ShaderModule);
#undef VKSWTICH
				default:
					check(0);
					break;
				}
				Entries.RemoveAtSwap(Index, 1, false);
			}
		}
	}

	FTempFrameAllocationBuffer::FTempFrameAllocationBuffer(FVulkanDevice* InDevice, uint32 InSize)
		: FDeviceChild(InDevice)
		, Size(InSize)
		, BufferIndex(0)
		, PeakUsed(0)
	{
		for (int32 Index = 0; Index < NUM_RENDER_BUFFERS; ++Index)
		{
			BufferSuballocations[Index] = InDevice->GetResourceHeapManager().AllocateBuffer(InSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				__FILE__, __LINE__);
			MappedData[Index] = (uint8*)BufferSuballocations[Index]->GetMappedPointer();
			CurrentData[Index] = MappedData[Index];
		}
	}

	FTempFrameAllocationBuffer::~FTempFrameAllocationBuffer()
	{
		Destroy();
	}

	void FTempFrameAllocationBuffer::Destroy()
	{
		for (int32 Index = 0; Index < NUM_RENDER_BUFFERS; ++Index)
		{
			BufferSuballocations[Index] = nullptr;
		}
	}

	bool FTempFrameAllocationBuffer::Alloc(uint32 InSize, uint32 InAlignment, FTempAllocInfo& OutInfo)
	{
		uint8* AlignedData = (uint8*)Align((uintptr_t)CurrentData[BufferIndex], (uintptr_t)InAlignment);
		if (AlignedData + InSize <= MappedData[BufferIndex] + Size)
		{
			OutInfo.Data = AlignedData;
			OutInfo.BufferSuballocation = BufferSuballocations[BufferIndex];
			OutInfo.CurrentOffset = (uint32)(AlignedData - MappedData[BufferIndex]);
			CurrentData[BufferIndex] = AlignedData + InSize;
			PeakUsed = FMath::Max(PeakUsed, (uint32)(CurrentData[BufferIndex] - MappedData[BufferIndex]));
			return true;
		}

		return false;
	}

	void FTempFrameAllocationBuffer::Reset()
	{
		BufferIndex = (BufferIndex + 1) % NUM_RENDER_BUFFERS;
		CurrentData[BufferIndex] = MappedData[BufferIndex];
	}
}
