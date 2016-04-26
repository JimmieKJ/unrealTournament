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
		PhysicalDeviceHandle(VK_NULL_HANDLE),
		DeviceHandle(VK_NULL_HANDLE),
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

		PhysicalDeviceHandle = InDevice->GetPhysicalHandle();
		vkGetPhysicalDeviceMemoryProperties(PhysicalDeviceHandle, &MemoryProperties);

		const uint32 MaxAllocations = InDevice->GetLimits().maxMemoryAllocationCount;

		HeapInfos.AddDefaulted(MemoryProperties.memoryHeapCount);

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
						String += TEXT(" HVisible");
					}
					if ((Flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
					{
						String += TEXT(" HCoherent");
					}
					if ((Flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
					{
						String += TEXT(" HCached");
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
			checkf(HeapInfos[Index].Allocations.Num() == 0, TEXT("Found %d unfreed allocations!"), HeapInfos[Index].Allocations.Num());
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

		auto* NewAllocation = new FDeviceMemoryAllocation;
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
		if (NumAllocations == Device->GetLimits().maxMemoryAllocationCount)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Hit Maximum # of allocations (%d) reported by device!"), NumAllocations);
		}
		VERIFYVULKANRESULT(vkAllocateMemory(DeviceHandle, &Info, nullptr, &NewAllocation->Handle));

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
		vkFreeMemory(DeviceHandle, Allocation->Handle, nullptr);

		--NumAllocations;

		uint32 HeapIndex = MemoryProperties.memoryTypes[Allocation->MemoryTypeIndex].heapIndex;

		HeapInfos[HeapIndex].UsedSize -= Allocation->Size;
		HeapInfos[HeapIndex].Allocations.RemoveSwap(Allocation);
		Allocation->bFreedBySystem = true;
		delete Allocation;
		Allocation = nullptr;
	}

	FDeviceMemoryAllocation::~FDeviceMemoryAllocation()
	{
		checkf(bFreedBySystem, TEXT("Memory has to released calling FDeviceMemory::Free()!"));
	}

	void* FDeviceMemoryAllocation::Map(VkDeviceSize InSize, VkDeviceSize Offset)
	{
		check(bCanBeMapped);
		check(!bIsMapped);
		check(InSize + Offset <= Size);

		void* MappedMemory = nullptr;

		VERIFYVULKANRESULT(vkMapMemory(DeviceHandle, Handle, Offset, InSize, 0, &MappedMemory));
		bIsMapped = true;

		return MappedMemory;
	}

	void FDeviceMemoryAllocation::Unmap()
	{
		check(bIsMapped);
		vkUnmapMemory(DeviceHandle, Handle);
		bIsMapped = false;
	}


	void* FResourceAllocationInfo::Map(VkDeviceSize InSize, VkDeviceSize InOffset)
	{
		return Allocation->Map(InSize, InOffset + Offset);
	}

	void FResourceAllocationInfo::Unmap()
	{
		Allocation->Unmap();
	}

	bool FResourceAllocationManager::FResourceEntry::TryAllocate(VkDeviceSize Size, VkDeviceSize Alignment, VkDeviceSize* OutOffset)
	{
		VkDeviceSize RequiredOffset = Align(FreeOffset, (int32)Alignment);
		if (RequiredOffset + Size <= Allocation->GetSize())
		{
			*OutOffset = RequiredOffset;
			FreeOffset = RequiredOffset + Size;
			return true;
		}

		return false;
	}

	FResourceAllocationManager::FResourceAllocationManager()
		: Device(nullptr)
	{
	}

	FResourceAllocationManager::~FResourceAllocationManager()
	{
	}

	void FResourceAllocationManager::Init(FVulkanDevice* InDevice)
	{
		check(!Device);
		Device = InDevice;

		Entries.AddDefaulted(Device->GetMemoryManager().GetNumMemoryTypes());
	}

	void FResourceAllocationManager::Deinit()
	{
		check(0);
	}

	FResourceAllocationInfo* FResourceAllocationManager::AllocateBuffer(const VkMemoryRequirements& MemoryRequirements, VkMemoryPropertyFlags MemPropertyFlags)
	{
		auto& MemManager = Device->GetMemoryManager();
		uint32 MemoryTypeIndex = ~0;
		VERIFYVULKANRESULT(MemManager.GetMemoryTypeFromProperties(MemoryRequirements.memoryTypeBits, MemPropertyFlags, &MemoryTypeIndex));

		FScopeLock Lock(&GBufferAllocationLock);
		auto& EntriesPerMemoryType = Entries[MemoryTypeIndex];

		VkDeviceSize OutOffset = ~(VkDeviceSize)0;

		for (int32 Index = 0; Index < EntriesPerMemoryType.Num(); ++Index)
		{
			FResourceEntry* Entry = EntriesPerMemoryType[Index];
			if (Entry->TryAllocate(MemoryRequirements.size, MemoryRequirements.alignment, &OutOffset))
			{
				return new FResourceAllocationInfo(MemoryTypeIndex, Entry->Allocation, OutOffset);
			}
		}

		// Allocate a new area
		VkDeviceSize Size = FMath::Max(MemoryRequirements.size, (VkDeviceSize)PageSize);
		auto* Block = MemManager.Alloc(Size, MemoryRequirements.memoryTypeBits, MemPropertyFlags, __FILE__, __LINE__);
		check(Block);
		FResourceEntry* NewEntry = new FResourceEntry(Block);
		EntriesPerMemoryType.Add(NewEntry);

		bool bResult = NewEntry->TryAllocate(MemoryRequirements.size, MemoryRequirements.alignment, &OutOffset);
		check(bResult);
		return new FResourceAllocationInfo(MemoryTypeIndex, NewEntry->Allocation, OutOffset);
	}

	void FResourceAllocationManager::FreeBuffer(FResourceAllocationInfo*& Info)
	{
		//#todo-rco: Verify!
		check(0);
		delete Info;
		Info = nullptr;
	}

	FStagingResource::~FStagingResource()
	{
		checkf(!Allocation, TEXT("Staging Resource not released!"));
	}

	void FStagingImage::Destroy(FDeviceMemoryManager& Memory, VkDevice DeviceHandle)
	{
		check(Allocation);

		vkDestroyImage(DeviceHandle, Image, nullptr);

		Memory.Free(Allocation);
	}

	void FStagingBuffer::Destroy(FDeviceMemoryManager& Memory, VkDevice DeviceHandle)
	{
		check(Allocation);

		vkDestroyBuffer(DeviceHandle, Buffer, nullptr);

		Memory.Free(Allocation);
	}

	void FStagingManager::Init(FVulkanDevice* InDevice, FVulkanQueue* InQueue)
	{
		Device = InDevice;
		Queue = InQueue;
	}

	void FStagingManager::Deinit()
	{
		check(UsedStagingImages.Num() == 0);
		auto& DeviceMemory = Device->GetMemoryManager();
		for (int32 Index = 0; Index < FreeStagingImages.Num(); ++Index)
		{
			FreeStagingImages[Index]->Destroy(DeviceMemory, Device->GetInstanceHandle());
			delete FreeStagingImages[Index];
		}

		FreeStagingImages.Empty(0);

		check(UsedStagingBuffers.Num() == 0);
		for (int32 Index = 0; Index < FreeStagingBuffers.Num(); ++Index)
		{
			FreeStagingBuffers[Index]->Destroy(DeviceMemory, Device->GetInstanceHandle());
			delete FreeStagingBuffers[Index];
		}

		FreeStagingBuffers.Empty(0);
	}

	FStagingImage* FStagingManager::AcquireImage(VkFormat Format, uint32 Width, uint32 Height, uint32 Depth)
	{
		//#todo-rco: Better locking!
		{
			FScopeLock Lock(&GAllocationLock);

			for (int32 Index = 0; Index < FreeStagingImages.Num(); ++Index)
			{
				auto* FreeImage = FreeStagingImages[Index];
				if (FreeImage->Format == Format && FreeImage->Width == Width && FreeImage->Height == Height && FreeImage->Depth == Depth)
				{
					FreeStagingImages.RemoveAtSwap(Index, 1, false);
					UsedStagingImages.Add(FreeImage);
					return FreeImage;
				}
			}
		}

		auto* StagingImage = new FStagingImage(Format, Width, Height, Depth);

		VkExtent3D StagingExtent;
		FMemory::Memzero(StagingExtent);
		StagingExtent.width = Width;
		StagingExtent.height = Height;
		StagingExtent.depth = Depth;

		VkImageCreateInfo StagingImageCreateInfo;
		FMemory::Memzero(StagingImageCreateInfo);
		StagingImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		StagingImageCreateInfo.imageType = Depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
		StagingImageCreateInfo.format = Format;
		StagingImageCreateInfo.extent = StagingExtent;
		StagingImageCreateInfo.mipLevels = 1;
		StagingImageCreateInfo.arrayLayers = 1;
		StagingImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		StagingImageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
		StagingImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		VkDevice VulkanDevice = Device->GetInstanceHandle();

		VERIFYVULKANRESULT(vkCreateImage(VulkanDevice, &StagingImageCreateInfo, nullptr, &StagingImage->Image));

		VkMemoryRequirements MemReqs;
		vkGetImageMemoryRequirements(VulkanDevice, StagingImage->Image, &MemReqs);

		StagingImage->Allocation = Device->GetMemoryManager().Alloc(MemReqs.size, MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, __FILE__, __LINE__);

		VERIFYVULKANRESULT(vkBindImageMemory(VulkanDevice, StagingImage->Image, StagingImage->Allocation->GetHandle(), 0));

		{
			FScopeLock Lock(&GAllocationLock);
			UsedStagingImages.Add(StagingImage);
		}
		return StagingImage;
	}

	void FStagingManager::ReleaseImage(FStagingImage*& StagingImage)
	{
		FScopeLock Lock(&GAllocationLock);
		UsedStagingImages.RemoveSingleSwap(StagingImage);
		FreeStagingImages.Add(StagingImage);
		StagingImage = nullptr;
	}

	FStagingBuffer* FStagingManager::AcquireBuffer(uint32 Size)
	{
		//#todo-rco: Better locking!
		{
			FScopeLock Lock(&GAllocationLock);

			for (int32 Index = 0; Index < FreeStagingBuffers.Num(); ++Index)
			{
				auto* FreeBuffer = FreeStagingBuffers[Index];
				if (FreeBuffer->Allocation->GetSize() == Size)
				{
					FreeStagingBuffers.RemoveAtSwap(Index, 1, false);
					UsedStagingBuffers.Add(FreeBuffer);
					return FreeBuffer;
				}
			}
		}

		auto* StagingBuffer = new FStagingBuffer();

		VkBufferCreateInfo StagingBufferCreateInfo;
		FMemory::Memzero(StagingBufferCreateInfo);
		StagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		StagingBufferCreateInfo.size = Size;
		StagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VkDevice VulkanDevice = Device->GetInstanceHandle();

		VERIFYVULKANRESULT(vkCreateBuffer(VulkanDevice, &StagingBufferCreateInfo, nullptr, &StagingBuffer->Buffer));

		VkMemoryRequirements MemReqs;
		vkGetBufferMemoryRequirements(VulkanDevice, StagingBuffer->Buffer, &MemReqs);

		StagingBuffer->Allocation = Device->GetMemoryManager().Alloc(MemReqs.size, MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, __FILE__, __LINE__);

		VERIFYVULKANRESULT(vkBindBufferMemory(VulkanDevice, StagingBuffer->Buffer, StagingBuffer->Allocation->GetHandle(), 0));

		{
			FScopeLock Lock(&GAllocationLock);
			UsedStagingBuffers.Add(StagingBuffer);
		}
		return StagingBuffer;
	}

	void FStagingManager::ReleaseBuffer(FStagingBuffer*& StagingBuffer)
	{
		FScopeLock Lock(&GAllocationLock);
		UsedStagingBuffers.RemoveSingleSwap(StagingBuffer);
		FreeStagingBuffers.Add(StagingBuffer);
		StagingBuffer = nullptr;
	}

	FFence::FFence(FVulkanDevice* Device)
		: State(FFence::EState::NotReady)
	{
		VkFenceCreateInfo Info;
		FMemory::Memzero(Info);
		Info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VERIFYVULKANRESULT(vkCreateFence(Device->GetInstanceHandle(), &Info, nullptr, &Handle));
	}

	FFence::~FFence()
	{
		checkf(Handle == VK_NULL_HANDLE, TEXT("Didn't get properly destroyed by FFenceManager!"));
	}

	FFenceManager::~FFenceManager()
	{
		check(UsedFences.Num() == 0);
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
			vkDestroyFence(DeviceHandle, Fence->GetHandle(), nullptr);
			Fence->Handle = VK_NULL_HANDLE;
			delete Fence;
		}
	}

	FFence* FFenceManager::AllocateFence()
	{
		FScopeLock Lock(&GFenceLock);
		if (FreeFences.Num() != 0)
		{
			auto* Fence = FreeFences[0];
			FreeFences.RemoveAtSwap(0, 1, false);
			UsedFences.Add(Fence);
			return Fence;
		}

		auto* NewFence = new FFence(Device);
		UsedFences.Add(NewFence);
		return NewFence;
	}

	// Sets it to nullptr
	void FFenceManager::ReleaseFence(FFence*& Fence)
	{
		FScopeLock Lock(&GFenceLock);
		ResetFence(Fence);
		UsedFences.RemoveSingleSwap(Fence);
		FreeFences.Add(Fence);
		Fence = nullptr;
	}

	FFence::EState FFenceManager::CheckFenceState(FFence* Fence)
	{
		check(UsedFences.Contains(Fence));
		check(Fence->State == FFence::EState::NotReady);
		auto Result = vkGetFenceStatus(Device->GetInstanceHandle(), Fence->Handle);
		switch (Result)
		{
		case VK_SUCCESS:
			Fence->State = FFence::EState::Signaled;
			return FFence::EState::Signaled;

		case VK_NOT_READY:
			break;

		default:
			VERIFYVULKANRESULT(Result);
			break;
		}

		return FFence::EState::NotReady;
	}

	bool FFenceManager::WaitForFence(FFence* Fence, uint64 TimeInNanoseconds)
	{
		check(UsedFences.Contains(Fence));
		check(Fence->State == FFence::EState::NotReady);
		auto Result = vkWaitForFences(Device->GetInstanceHandle(), 1, &Fence->Handle, true, TimeInNanoseconds);
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
			VERIFYVULKANRESULT(vkResetFences(Device->GetInstanceHandle(), 1, &Fence->Handle));
			Fence->State = FFence::EState::NotReady;
		}
	}
}
