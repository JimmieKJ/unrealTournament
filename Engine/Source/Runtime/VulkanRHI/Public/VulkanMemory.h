// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanMemory.h: Vulkan Memory RHI definitions.
=============================================================================*/

#pragma once 

#define VULKAN_TRACK_MEMORY_USAGE	1

class FVulkanQueue;
class FVulkanCmdBuffer;

namespace VulkanRHI
{
	class FFenceManager;

	enum
	{
		NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS = 20,
	};

	// Custom ref counting
	class FRefCount
	{
	public:
		FRefCount() {}
		virtual ~FRefCount()
		{
			check(NumRefs.GetValue() == 0);
		}

		inline uint32 AddRef()
		{
			int32 NewValue = NumRefs.Increment();
			check(NewValue > 0);
			return (uint32)NewValue;
		}

		inline uint32 Release()
		{
			int32 NewValue = NumRefs.Decrement();
			if (NewValue == 0)
			{
				delete this;
			}
			check(NewValue >= 0);
			return (uint32)NewValue;
		}

		inline uint32 GetRefCount() const
		{
			int32 Value = NumRefs.GetValue();
			check(Value >= 0);
			return (uint32)Value;
		}

	private:
		FThreadSafeCounter NumRefs;
	};

	class FDeviceChild
	{
	public:
		FDeviceChild(FVulkanDevice* InDevice = nullptr) :
			Device(InDevice)
		{
		}

		inline FVulkanDevice* GetParent() const
		{
			// Has to have one if we are asking for it...
			check(Device);
			return Device;
		}

		inline void SetParent(FVulkanDevice* InDevice)
		{
			check(!Device);
			Device = InDevice;
		}

	protected:
		FVulkanDevice* Device;
	};

	// An Allocation of a Device Heap. Lowest level of allocations and bounded by VkPhysicalDeviceLimits::maxMemoryAllocationCount.
	class FDeviceMemoryAllocation
	{
	public:
		FDeviceMemoryAllocation()
			: DeviceHandle(VK_NULL_HANDLE)
			, Handle(VK_NULL_HANDLE)
			, Size(0)
			, MemoryTypeIndex(0)
			, bCanBeMapped(0)
			, MappedPointer(nullptr)
			, bIsCoherent(0)
			, bIsCached(0)
			, bFreedBySystem(false)
#if VULKAN_TRACK_MEMORY_USAGE
			, File(nullptr)
			, Line(0)
#endif
		{
		}

		void* Map(VkDeviceSize Size, VkDeviceSize Offset);
		void Unmap();

		inline bool CanBeMapped() const
		{
			return bCanBeMapped != 0;
		}

		inline bool IsMapped() const
		{
			return !!MappedPointer;
		}

		inline void* GetMappedPointer()
		{
			check(IsMapped());
			return MappedPointer;
		}

		inline bool IsCoherent() const
		{
			return bIsCoherent != 0;
		}

		void FlushMappedMemory();
		void InvalidateMappedMemory();

		inline VkDeviceMemory GetHandle() const
		{
			return Handle;
		}

		inline VkDeviceSize GetSize() const
		{
			return Size;
		}

		inline uint32 GetMemoryTypeIndex() const
		{
			return MemoryTypeIndex;
		}

	protected:
		VkDeviceSize Size;
		VkDevice DeviceHandle;
		VkDeviceMemory Handle;
		void* MappedPointer;
		uint32 MemoryTypeIndex : 8;
		uint32 bCanBeMapped : 1;
		uint32 bIsCoherent : 1;
		uint32 bIsCached : 1;
		uint32 bFreedBySystem : 1;
		uint32 : 0;

#if VULKAN_TRACK_MEMORY_USAGE
		const char* File;
		uint32 Line;
#endif
		// Only owner can delete!
		~FDeviceMemoryAllocation();

		friend class FDeviceMemoryManager;
	};

	// Manager of Device Heap Allocations. Calling Alloc/Free is expensive!
	class FDeviceMemoryManager
	{
	public:
		FDeviceMemoryManager();
		~FDeviceMemoryManager();

		void Init(FVulkanDevice* InDevice);

		void Deinit();

		inline uint32 GetNumMemoryTypes() const
		{
			return MemoryProperties.memoryTypeCount;
		}

		//#todo-rco: Might need to revisit based on https://gitlab.khronos.org/vulkan/vulkan/merge_requests/1165
		inline VkResult GetMemoryTypeFromProperties(uint32 TypeBits, VkMemoryPropertyFlags Properties, uint32* OutTypeIndex)
		{
			// Search memtypes to find first index with those properties
			for (uint32 i = 0; i < MemoryProperties.memoryTypeCount && TypeBits; i++)
			{
				if ((TypeBits & 1) == 1)
				{
					// Type is available, does it match user properties?
					if ((MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties)
					{
						*OutTypeIndex = i;
						return VK_SUCCESS;
					}
				}
				TypeBits >>= 1;
			}

			// No memory types matched, return failure
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		inline const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const
		{
			return MemoryProperties;
		}

		FDeviceMemoryAllocation* Alloc(VkDeviceSize AllocationSize, uint32 MemoryTypeIndex, const char* File, uint32 Line);

		inline FDeviceMemoryAllocation* Alloc(VkDeviceSize AllocationSize, uint32 MemoryTypeBits, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
		{
			uint32 MemoryTypeIndex = ~0;
			VERIFYVULKANRESULT(this->GetMemoryTypeFromProperties(MemoryTypeBits, MemoryPropertyFlags, &MemoryTypeIndex));
			return Alloc(AllocationSize, MemoryTypeIndex, File, Line);
		}

		// Sets the Allocation to nullptr
		void Free(FDeviceMemoryAllocation*& Allocation);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		VkPhysicalDeviceMemoryProperties MemoryProperties;
		VkDevice DeviceHandle;
		FVulkanDevice* Device;
		uint32 NumAllocations;
		uint32 PeakNumAllocations;

		struct FHeapInfo
		{
			VkDeviceSize TotalSize;
			VkDeviceSize UsedSize;
			TArray<FDeviceMemoryAllocation*> Allocations;

			FHeapInfo() :
				TotalSize(0),
				UsedSize(0)
			{
			}
		};

		TArray<FHeapInfo> HeapInfos;
	};

	class FOldResourceHeap;
	class FOldResourceHeapPage;
	class FResourceHeapManager;

	// A sub allocation for a specific memory type
	class FOldResourceAllocation : public FRefCount
	{
	public:
		FOldResourceAllocation(FOldResourceHeapPage* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset, const char* InFile, uint32 InLine);

		virtual ~FOldResourceAllocation();

		inline uint32 GetSize() const
		{
			return RequestedSize;
		}

		inline uint32 GetOffset() const
		{
			return AlignedOffset;
		}

		inline VkDeviceMemory GetHandle() const
		{
			return DeviceMemoryAllocation->GetHandle();
		}

		inline void* GetMappedPointer()
		{
			check(DeviceMemoryAllocation->CanBeMapped());
			check(DeviceMemoryAllocation->IsMapped());
			return (uint8*)DeviceMemoryAllocation->GetMappedPointer() + AlignedOffset;
		}

		inline uint32 GetMemoryTypeIndex() const
		{
			return DeviceMemoryAllocation->GetMemoryTypeIndex();
		}

		void BindBuffer(FVulkanDevice* Device, VkBuffer Buffer);
		void BindImage(FVulkanDevice* Device, VkImage Image);

	private:
		FOldResourceHeapPage* Owner;

		// Total size of allocation
		uint32 AllocationSize;

		// Original offset of allocation
		uint32 AllocationOffset;

		// Requested size
		uint32 RequestedSize;

		// Requested alignment offset
		uint32 AlignedOffset;

		FDeviceMemoryAllocation* DeviceMemoryAllocation;

#if VULKAN_TRACK_MEMORY_USAGE
		const char* File;
		uint32 Line;
#endif

		friend class FOldResourceHeapPage;
	};

	// One device allocation that is shared amongst different resources
	class FOldResourceHeapPage
	{
	public:
		FOldResourceHeapPage(FOldResourceHeap* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation);
		~FOldResourceHeapPage();

		FOldResourceAllocation* TryAllocate(uint32 Size, uint32 Alignment, const char* File, uint32 Line);

		FOldResourceAllocation* Allocate(uint32 Size, uint32 Alignment, const char* File, uint32 Line)
		{
			FOldResourceAllocation* ResourceAllocation = TryAllocate(Size, Alignment, File, Line);
			check(ResourceAllocation);
			return ResourceAllocation;
		}

		void ReleaseAllocation(FOldResourceAllocation* Allocation);

		FOldResourceHeap* GetOwner()
		{
			return Owner;
		}

	protected:
		FOldResourceHeap* Owner;
		FDeviceMemoryAllocation* DeviceMemoryAllocation;
		TArray<FOldResourceAllocation*>  ResourceAllocations;
		uint32 MaxSize;
		uint32 UsedSize;
		int32 PeakNumAllocations;
		uint32 FrameFreed;

		bool JoinFreeBlocks();

		struct FPair
		{
			uint32 Offset;
			uint32 Size;

			bool operator<(const FPair& In) const
			{
				return Offset < In.Offset;
			}
		};
		TArray<FPair> FreeList;
		friend class FOldResourceHeap;
	};

	class FBufferAllocation;

	class FResourceSuballocation : public FRefCount
	{
	public:
		FResourceSuballocation(uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset)
			: RequestedSize(InRequestedSize)
			, AlignedOffset(InAlignedOffset)
			, AllocationSize(InAllocationSize)
			, AllocationOffset(InAllocationOffset)
		{
		}

		inline uint32 GetOffset() const
		{
			return AlignedOffset;
		}

		inline uint32 GetSize() const
		{
			return RequestedSize;
		}

	protected:
		uint32 RequestedSize;
		uint32 AlignedOffset;
		uint32 AllocationSize;
		uint32 AllocationOffset;
	};

	class FBufferSuballocation : public FResourceSuballocation
	{
	public:
		FBufferSuballocation(FBufferAllocation* InOwner, VkBuffer InHandle,
			uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset)
			: FResourceSuballocation(InRequestedSize, InAlignedOffset, InAllocationSize, InAllocationOffset)
			, Owner(InOwner)
			, Handle(InHandle)
		{
		}

		virtual ~FBufferSuballocation();

		inline VkBuffer GetHandle() const
		{
			return Handle;
		}

		FBufferAllocation* GetBufferAllocation()
		{
			return Owner;
		}

		void* GetMappedPointer();

	protected:
		friend class FBufferAllocation;
		FBufferAllocation* Owner;
		VkBuffer Handle;
	};

	class FSubresourceAllocator
	{
	public:
		FSubresourceAllocator(FResourceHeapManager* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InMemoryTypeIndex, VkMemoryPropertyFlags InMemoryPropertyFlags,
			uint32 InAlignment)
			: Owner(InOwner)
			, MemoryTypeIndex(InMemoryTypeIndex)
			, MemoryPropertyFlags(InMemoryPropertyFlags)
			, MemoryAllocation(InDeviceMemoryAllocation)
			, Alignment(InAlignment)
			, FrameFreed(0)
			, UsedSize(0)
		{
			MaxSize = InDeviceMemoryAllocation->GetSize();
			FPair Pair;
			Pair.Offset = 0;
			Pair.Size = MaxSize;
			FreeList.Add(Pair);
		}

		virtual ~FSubresourceAllocator() {}

		virtual FResourceSuballocation* CreateSubAllocation(uint32 Size, uint32 AlignedOffset, uint32 AllocatedSize, uint32 AllocatedOffset) = 0;
		virtual void Destroy(FVulkanDevice* Device) = 0;

		FResourceSuballocation* TryAllocateNoLocking(uint32 InSize, uint32 InAlignment, const char* File, uint32 Line);

		inline FResourceSuballocation* TryAllocateLocking(uint32 InSize, uint32 InAlignment, const char* File, uint32 Line)
		{
			FScopeLock ScopeLock(&CS);
			return TryAllocateNoLocking(InSize, InAlignment, File, Line);
		}

		uint32 GetAlignment() const
		{
			return Alignment;
		}

		void* GetMappedPointer()
		{
			return MemoryAllocation->GetMappedPointer();
		}

	protected:
		FResourceHeapManager* Owner;
		uint32 MemoryTypeIndex;
		VkMemoryPropertyFlags MemoryPropertyFlags;
		FDeviceMemoryAllocation* MemoryAllocation;
		uint32 MaxSize;
		uint32 Alignment;
		uint32 FrameFreed;
		int64 UsedSize;
		static FCriticalSection CS;

		struct FPair
		{
			uint32 Offset;
			uint32 Size;

			bool operator<(const FPair& In) const
			{
				return Offset < In.Offset;
			}
		};
		TArray<FPair> FreeList;

		TArray<FResourceSuballocation*> Suballocations;
		bool JoinFreeBlocks();
	};

	class FBufferAllocation : public FSubresourceAllocator
	{
	public:
		FBufferAllocation(FResourceHeapManager* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InMemoryTypeIndex, VkMemoryPropertyFlags InMemoryPropertyFlags,
			uint32 InAlignment,
			VkBuffer InBuffer, VkBufferUsageFlags InBufferUsageFlags)
			: FSubresourceAllocator(InOwner, InDeviceMemoryAllocation, InMemoryTypeIndex, InMemoryPropertyFlags, InAlignment)
			, BufferUsageFlags(InBufferUsageFlags)
			, Buffer(InBuffer)
		{
		}

		virtual ~FBufferAllocation()
		{
			check(Buffer == VK_NULL_HANDLE);
		}

		virtual FResourceSuballocation* CreateSubAllocation(uint32 Size, uint32 AlignedOffset, uint32 AllocatedSize, uint32 AllocatedOffset) override
		{
			return new FBufferSuballocation(this, Buffer, Size, AlignedOffset, AllocatedSize, AllocatedOffset);// , File, Line);
		}
		virtual void Destroy(FVulkanDevice* Device) override;

		void Release(FBufferSuballocation* Suballocation);

	protected:
		VkBufferUsageFlags BufferUsageFlags;
		VkBuffer Buffer;
		friend class FResourceHeapManager;
	};

#if 0
	class FImageAllocation;

	class FImageSuballocation : public FResourceSuballocation
	{
	public:
		FImageSuballocation(FImageAllocation* InOwner, VkImage InHandle,
			uint32 InRequestedSize, uint32 InAlignedOffset,
			uint32 InAllocationSize, uint32 InAllocationOffset)
			: FResourceSuballocation(InRequestedSize, InAlignedOffset, InAllocationSize, InAllocationOffset)
			, Owner(InOwner)
			, Handle(InHandle)
		{
		}

		virtual ~FImageSuballocation();

		inline VkImage GetHandle() const
		{
			return Handle;
		}

	protected:
		friend class FImageAllocation;
		FImageAllocation* Owner;
		VkImage Handle;
	};

	class FImageAllocation : public FSubresourceAllocator
	{
	public:
		FImageAllocation(FResourceHeapManager* InOwner, FDeviceMemoryAllocation* InDeviceMemoryAllocation,
			uint32 InMemoryTypeIndex, VkMemoryPropertyFlags InMemoryPropertyFlags,
			uint32 InAlignment,
			VkImage InImage, VkImageUsageFlags InImageUsageFlags)
			: FSubresourceAllocator(InOwner, InDeviceMemoryAllocation, InMemoryTypeIndex, InMemoryPropertyFlags, InAlignment)
			, ImageUsageFlags(InImageUsageFlags)
			, Image(InImage)
		{
		}

		virtual ~FImageAllocation()
		{
			check(Image == VK_NULL_HANDLE);
		}

		virtual FResourceSuballocation* CreateSubAllocation(uint32 Size, uint32 AlignedOffset, uint32 AllocatedSize, uint32 AllocatedOffset) override
		{
			return new FImageSuballocation(this, Image, Size, AlignedOffset, AllocatedSize, AllocatedOffset);// , File, Line);
		}
		virtual void Destroy(FVulkanDevice* Device) override;

		void Release(FImageSuballocation* Suballocation);

	protected:
		VkImageUsageFlags ImageUsageFlags;
		VkImage Image;
		friend class FResourceHeapManager;
	};
#endif

	// A set of Device Allocations (Heap Pages) for a specific memory type. This handles pooling allocations inside memory pages to avoid
	// doing allocations directly off the device's heaps
	class FOldResourceHeap
	{
	public:
		FOldResourceHeap(FResourceHeapManager* InOwner, uint32 InMemoryTypeIndex, uint32 InPageSize, uint64 InTotalMemory);
		~FOldResourceHeap();

		void FreePage(FOldResourceHeapPage* InPage);

		void ReleaseFreedPages(bool bImmediately);

		inline FResourceHeapManager* GetOwner()
		{
			return Owner;
		}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		FResourceHeapManager* Owner;
		uint32 MemoryTypeIndex;

		uint32 DefaultPageSize;
		uint32 PeakPageSize;
		uint64 TotalMemory;
		uint64 UsedMemory;

		TArray<FOldResourceHeapPage*> UsedBufferPages;
		TArray<FOldResourceHeapPage*> UsedImagePages;
		TArray<FOldResourceHeapPage*> FreePages;

		FOldResourceAllocation* AllocateResource(uint32 Size, uint32 Alignment, bool bIsImage, bool bMapAllocation, const char* File, uint32 Line);

		FCriticalSection CriticalSection;

		friend class FResourceHeapManager;
	};

	// Manages heaps and their interactions
	class FResourceHeapManager : public FDeviceChild
	{
	public:
		FResourceHeapManager(FVulkanDevice* InDevice);
		~FResourceHeapManager();

		void Init();
		void Deinit();

		FBufferSuballocation* AllocateBuffer(uint32 Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line);
		void ReleaseBuffer(FBufferAllocation* BufferAllocation);
#if 0
		FImageSuballocation* AllocateImage(uint32 Size, VkImageUsageFlags ImageUsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line);
		void ReleaseImage(FImageAllocation* ImageAllocation);
#endif
		inline FOldResourceAllocation* AllocateImageMemory(const VkMemoryRequirements& MemoryReqs, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromProperties(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, &TypeIndex));
			bool bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			return ResourceHeaps[TypeIndex]->AllocateResource(MemoryReqs.size, MemoryReqs.alignment, true, bMapped, File, Line);
		}

		inline FOldResourceAllocation* AllocateBufferMemory(const VkMemoryRequirements& MemoryReqs, VkMemoryPropertyFlags MemoryPropertyFlags, const char* File, uint32 Line)
		{
			uint32 TypeIndex = 0;
			VERIFYVULKANRESULT(DeviceMemoryManager->GetMemoryTypeFromProperties(MemoryReqs.memoryTypeBits, MemoryPropertyFlags, &TypeIndex));
			bool bMapped = (MemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			if (!ResourceHeaps[TypeIndex])
			{
				UE_LOG(LogVulkanRHI, Fatal, TEXT("Missing memory type index %d, MemSize %d, MemPropTypeBits %u, MemPropertyFlags %u, %s(%d)"), TypeIndex, (uint32)MemoryReqs.size, (uint32)MemoryReqs.memoryTypeBits, (uint32)MemoryPropertyFlags, ANSI_TO_TCHAR(File), Line);
			}
			return ResourceHeaps[TypeIndex]->AllocateResource(MemoryReqs.size, MemoryReqs.alignment, false, bMapped, File, Line);
		}

		void ReleaseFreedPages();

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		static FCriticalSection CS;
		FDeviceMemoryManager* DeviceMemoryManager;
		TArray<FOldResourceHeap*> ResourceHeaps;
		TArray<uint32> PageSizes;

		FOldResourceHeap* GPUOnlyHeap;
		FOldResourceHeap* CPUStagingHeap;

		enum
		{
			BufferAllocationSize = 1 * 1024 * 1024,
			ImageAllocationSize = 2 * 1024 * 1024,
		};

		TArray<FBufferAllocation*> UsedBufferAllocations;
		TArray<FBufferAllocation*> FreeBufferAllocations;
#if 0
		TArray<FImageAllocation*> UsedImageAllocations;
		TArray<FImageAllocation*> FreeImageAllocations;
#endif
		void ReleaseFreedResources(bool bImmediately);
		void DestroyResourceAllocations();
	};

	class FStagingBuffer : public FRefCount
	{
	public:
		FStagingBuffer()
			: ResourceAllocation(nullptr)
			, Buffer(VK_NULL_HANDLE)
		{
		}

		VkBuffer GetHandle() const
		{
			return Buffer;
		}

		inline void* GetMappedPointer()
		{
			return ResourceAllocation->GetMappedPointer();
		}

	protected:
		TRefCountPtr<FOldResourceAllocation> ResourceAllocation;
		VkBuffer Buffer;

		// Owner maintains lifetime
		virtual ~FStagingBuffer();

		void Destroy(FVulkanDevice* Device);

		friend class FStagingManager;
	};

	class FStagingManager
	{
	public:
		FStagingManager() :
			Device(nullptr),
			Queue(nullptr)
		{
		}
		~FStagingManager();

		void Init(FVulkanDevice* InDevice, FVulkanQueue* InQueue);
		void Deinit();

		FStagingBuffer* AcquireBuffer(uint32 Size, VkBufferUsageFlags InUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		// Sets pointer to nullptr
		void ReleaseBuffer(FVulkanCmdBuffer* CmdBuffer, FStagingBuffer*& StagingBuffer);

		void ProcessPendingFree(bool bImmediately = false);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		void DumpMemory();
#endif

	protected:
		struct FPendingItem
		{
			FVulkanCmdBuffer* CmdBuffer;
			uint64 FenceCounter;
			FStagingBuffer* Resource;
		};

		TArray<FStagingBuffer*> UsedStagingBuffers;
		TArray<FPendingItem> PendingFreeStagingBuffers;
		TArray<FPendingItem> FreeStagingBuffers;

		FVulkanDevice* Device;
		FVulkanQueue* Queue;
	};

	class FFence
	{
	public:
		FFence(FVulkanDevice* InDevice, FFenceManager* InOwner);

		inline VkFence GetHandle() const
		{
			return Handle;
		}

		inline bool IsSignaled() const
		{
			return State == EState::Signaled;
		}

		FFenceManager* GetOwner()
		{
			return Owner;
		}

	protected:
		VkFence Handle;

		enum class EState
		{
			// Initial state
			NotReady,

			// After GPU processed it
			Signaled,
		};

		EState State;

		FFenceManager* Owner;

		// Only owner can delete!
		~FFence();
		friend class FFenceManager;
	};

	class FFenceManager
	{
	public:
		FFenceManager()
			: Device(nullptr)
		{
		}
		~FFenceManager();

		void Init(FVulkanDevice* InDevice);
		void Deinit();

		FFence* AllocateFence();

		inline bool IsFenceSignaled(FFence* Fence)
		{
			if (Fence->IsSignaled())
			{
				return true;
			}

			return CheckFenceState(Fence);
		}

		// Returns false if it timed out
		bool WaitForFence(FFence* Fence, uint64 TimeInNanoseconds);

		void ResetFence(FFence* Fence);

		// Sets it to nullptr
		void ReleaseFence(FFence*& Fence);

		// Sets it to nullptr
		void WaitAndReleaseFence(FFence*& Fence, uint64 TimeInNanoseconds);

	protected:
		FVulkanDevice* Device;
		TArray<FFence*> FreeFences;
		TArray<FFence*> UsedFences;

		// Returns true if signaled
		bool CheckFenceState(FFence* Fence);

		void DestroyFence(FFence* Fence);
	};

	class FDeferredDeletionQueue : public FDeviceChild
	{
		//typedef TPair<FRefCountedObject*, uint64> FFencedObject;
		//FThreadsafeQueue<FFencedObject> DeferredReleaseQueue;

	public:
		FDeferredDeletionQueue(FVulkanDevice* InDevice);
		~FDeferredDeletionQueue();

		enum class EType
		{
			RenderPass,
			Buffer,
			BufferView,
			Image,
			ImageView,
			Pipeline,
			PipelineLayout,
			Framebuffer,
			DescriptorSetLayout,
			Sampler,
			Semaphore,
			ShaderModule,
		};

		template <typename T>
		inline void EnqueueResource(EType Type, T Handle)
		{
			EnqueueGenericResource(Type, (void*)Handle);
		}

		void ReleaseResources(bool bDeleteImmediately = false);

		inline void Clear()
		{
			ReleaseResources(true);
		}
/*
		class FVulkanAsyncDeletionWorker : public FVulkanDeviceChild, FNonAbandonableTask
		{
		public:
			FVulkanAsyncDeletionWorker(FVulkanDevice* InDevice, FThreadsafeQueue<FFencedObject>* DeletionQueue);

			void DoWork();

		private:
			TQueue<FFencedObject> Queue;
		};
*/
	private:
		//TQueue<FAsyncTask<FVulkanAsyncDeletionWorker>*> DeleteTasks;

		void EnqueueGenericResource(EType Type, void* Handle);

		struct FEntry
		{
			uint64 FenceCounter;
			FVulkanCmdBuffer* CmdBuffer;
			void* Handle;
			EType StructureType;

			//FRefCount* Resource;
		};
		FCriticalSection CS;
		TArray<FEntry> Entries;
	};

	// Simple tape allocation per frame for a VkBuffer, used for Volatile allocations
	class FTempFrameAllocationBuffer : public FDeviceChild
	{
	public:
		FTempFrameAllocationBuffer(FVulkanDevice* InDevice, uint32 InSize);
		virtual ~FTempFrameAllocationBuffer();
		void Destroy();

		struct FTempAllocInfo
		{
			void* Data;

			// Offset into the locked area
			uint32 CurrentOffset;

			FBufferSuballocation* BufferSuballocation;

			// Simple counter used for the SRVs to know a new one is required
			uint32 LockCounter;

			FTempAllocInfo()
				: Data(nullptr)
				, CurrentOffset(0)
				, BufferSuballocation(nullptr)
				, LockCounter(0)
			{
			}

			inline uint32 GetBindOffset() const
			{
				return BufferSuballocation->GetOffset() + CurrentOffset;
			}

			inline VkBuffer GetHandle() const
			{
				return BufferSuballocation->GetHandle();
			}
		};

		bool Alloc(uint32 InSize, uint32 InAlignment, FTempAllocInfo& OutInfo);

		void Reset();

	protected:
		uint8* MappedData[NUM_RENDER_BUFFERS];
		uint8* CurrentData[NUM_RENDER_BUFFERS];
		TRefCountPtr<FBufferSuballocation> BufferSuballocations[NUM_RENDER_BUFFERS];
		uint32 BufferIndex;
		uint32 Size;
		uint32 PeakUsed;

		friend class FVulkanCommandListContext;
	};

	inline void* FBufferSuballocation::GetMappedPointer()
	{
		return Owner->GetMappedPointer();
	}
}
