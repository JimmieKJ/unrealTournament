// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanMemory.h: Vulkan Memory RHI definitions.
=============================================================================*/

#pragma once 

#define VULKAN_TRACK_MEMORY_USAGE	1

namespace VulkanRHI
{
	// Custom ref counting
	class FVulkanRefCount
	{
	public:
		FVulkanRefCount() {}
		virtual ~FVulkanRefCount()
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

	class FVulkanDeviceChild
	{
	public:
		FVulkanDeviceChild(FVulkanDevice* InDevice = nullptr) :
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

	class FDeviceMemoryAllocation
	{
	public:
		FDeviceMemoryAllocation()
			: DeviceHandle(VK_NULL_HANDLE)
			, Handle(VK_NULL_HANDLE)
			, Size(0)
			, MemoryTypeIndex(0)
			, bCanBeMapped(0)
			, bIsMapped(0)
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

		bool CanBeMapped() const
		{
			return bCanBeMapped != 0;
		}

		bool IsMapped() const
		{
			return bIsMapped != 0;
		}

		bool IsCoherent() const
		{
			return bIsCoherent != 0;
		}

		void FlushMappedMemory()
		{
			check(!IsCoherent());
			check(IsMapped());
			VkMappedMemoryRange Range;
			FMemory::Memzero(Range);
			Range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			Range.memory = Handle;
			//Range.offset = 0;
			Range.size = Size;
			VERIFYVULKANRESULT(vkFlushMappedMemoryRanges(DeviceHandle, 1, &Range));
		}

		void InvalidateMappedMemory()
		{
			check(!IsCoherent());
			check(IsMapped());
			VkMappedMemoryRange Range;
			FMemory::Memzero(Range);
			Range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			Range.memory = Handle;
			//Range.offset = 0;
			Range.size = Size;
			VERIFYVULKANRESULT(vkInvalidateMappedMemoryRanges(DeviceHandle, 1, &Range));
		}

		inline VkDeviceMemory GetHandle() const
		{
			return Handle;
		}

		inline VkDeviceSize GetSize() const
		{
			return Size;
		}

	protected:
		VkDeviceSize Size;
		VkDevice DeviceHandle;
		VkDeviceMemory Handle;
		uint32 MemoryTypeIndex : 8;
		uint32 bCanBeMapped : 1;
		uint32 bIsMapped : 1;
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

	class FDeviceMemoryManager
	{
	public:
		FDeviceMemoryManager();
		~FDeviceMemoryManager();

		void Init(struct FVulkanDevice* InDevice);

		void SetDevice(VkDevice InDevice)
		{
			DeviceHandle = InDevice;
		}

		void Deinit();

		inline uint32 GetNumMemoryTypes() const
		{
			return MemoryProperties.memoryTypeCount;
		}

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

	protected:
		VkPhysicalDeviceMemoryProperties MemoryProperties;
		VkPhysicalDevice PhysicalDeviceHandle;
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

	class FResourceAllocationInfo
	{
	public:
		inline VkDeviceMemory GetHandle() const
		{
			return Allocation->GetHandle();
		}

		inline VkDeviceSize GetOffset() const
		{
			return Offset;
		}

		FResourceAllocationInfo()
			: Allocation(nullptr)
			, Offset(~(VkDeviceSize)0)
			, MemoryTypeIndex(~0)
			, bReleased(true)
		{
		}

		~FResourceAllocationInfo()
		{
			check(bReleased);
		}

		void* Map(VkDeviceSize Size, VkDeviceSize Offset);
		void Unmap();

	protected:
		FDeviceMemoryAllocation* Allocation;
		VkDeviceSize Offset;
		uint32 MemoryTypeIndex;
		bool bReleased;

		FResourceAllocationInfo(uint32 InMemoryTypeIndex,
			FDeviceMemoryAllocation* InAllocation,
			VkDeviceSize InOffset)
			: Allocation(InAllocation)
			, Offset(InOffset)
			, MemoryTypeIndex(InMemoryTypeIndex)
			, bReleased(false)
		{
		}

		friend class FResourceAllocationManager;
	};

	class FResourceAllocationManager
	{
	public:
		FResourceAllocationManager();
		~FResourceAllocationManager();

		void Init(FVulkanDevice* InDevice);
		void Deinit();

		FResourceAllocationInfo* AllocateBuffer(const VkMemoryRequirements& MemoryRequirements, VkMemoryPropertyFlags MemPropertyFlags);
		// Sets to null
		void FreeBuffer(FResourceAllocationInfo*& Info);

	protected:
		enum
		{
			/*Host*/PageSize = 1 * 1024 * 1024,
			//LocalPageSize = 1 * 1024 * 1024,
		};
		FVulkanDevice* Device;

		struct FResourceEntry
		{
			FResourceEntry(FDeviceMemoryAllocation* InAllocation)
				: Allocation(InAllocation)
				, FreeOffset(0)
			{
			}

			bool TryAllocate(VkDeviceSize Size, VkDeviceSize Alignment, VkDeviceSize* OutOffset);

			FDeviceMemoryAllocation* Allocation;
			VkDeviceSize FreeOffset;
		};
		// Array of entries per memory type
		TArray< TArray<FResourceEntry*> > /*Free*/Entries;
	};

	class FStagingResource
	{
	public:
		FStagingResource()
			: Allocation(nullptr)
		{
		}

		inline void* Map()
		{
			return Allocation->Map(Allocation->GetSize(), 0);
		}

		inline void Unmap()
		{
			Allocation->Unmap();
		}

	protected:
		FDeviceMemoryAllocation* Allocation;

		// Owner maintains lifetime
		virtual ~FStagingResource();

		virtual void Destroy(FDeviceMemoryManager& Memory, VkDevice DeviceHandle) = 0;
	};

	class FStagingImage : public FStagingResource
	{
	public:
		FStagingImage(VkFormat InFormat, uint32 InWidth, uint32 InHeight, uint32 InDepth) :
			Image(VK_NULL_HANDLE),
			Format(InFormat),
			Width(InWidth),
			Height(InHeight),
			Depth(InDepth)
		{
		}

		VkImage GetHandle() const
		{
			return Image;
		}

	protected:
		VkImage Image;

		VkFormat Format;
		uint32 Width;
		uint32 Height;
		uint32 Depth;

		// Owner maintains lifetime
		virtual ~FStagingImage() {}

		virtual void Destroy(FDeviceMemoryManager& Memory, VkDevice DeviceHandle) override;

		friend class FStagingManager;
	};

	class FStagingBuffer : public FStagingResource
	{
	public:
		FStagingBuffer() :
			Buffer(VK_NULL_HANDLE)
		{
		}

		VkBuffer GetHandle() const
		{
			return Buffer;
		}

	protected:
		VkBuffer Buffer;

		// Owner maintains lifetime
		virtual ~FStagingBuffer() {}

		virtual void Destroy(FDeviceMemoryManager& Memory, VkDevice DeviceHandle) override;

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

		void Init(FVulkanDevice* InDevice, FVulkanQueue* InQueue);
		void Deinit();

		FStagingImage* AcquireImage(VkFormat Format, uint32 Width, uint32 Height, uint32 Depth = 1);

		FStagingBuffer* AcquireBuffer(uint32 Size);

		// Sets pointer to nullptr
		void ReleaseImage(FStagingImage*& StagingImage);

		// Sets pointer to nullptr
		void ReleaseBuffer(FStagingBuffer*& StagingBuffer);

	protected:
		TArray<FStagingImage*> FreeStagingImages;
		TArray<FStagingImage*> UsedStagingImages;

		TArray<FStagingBuffer*> FreeStagingBuffers;
		TArray<FStagingBuffer*> UsedStagingBuffers;

		FVulkanDevice* Device;
		FVulkanQueue* Queue;
	};

	class FFence
	{
	public:
		FFence(FVulkanDevice* Device);

		inline VkFence GetHandle() const
		{
			return Handle;
		}

		enum class EState
		{
			// Initial state
			NotReady,

			// After GPU processed it
			Signaled,
		};

		inline EState GetState() const
		{
			return State;
		}

	protected:
		VkFence Handle;
		EState State;

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

		inline FFence::EState GetFenceState(FFence* Fence)
		{
			if (Fence->GetState() == FFence::EState::Signaled)
			{
				return FFence::EState::Signaled;
			}

			return CheckFenceState(Fence);
		}

		// Returns false if it timed out
		bool WaitForFence(FFence* Fence, uint64 TimeInNanoseconds);

		void ResetFence(FFence* Fence);

		// Sets it to nullptr
		void ReleaseFence(FFence*& Fence);

	protected:
		FVulkanDevice* Device;
		TArray<FFence*> FreeFences;
		TArray<FFence*> UsedFences;

		FFence::EState CheckFenceState(FFence* Fence);
	};

	struct FVulkanResource : public FVulkanRefCount
	{
	};

	//#todo-rco: Move to common RHI?
	/**
	 * The base class of threadsafe reference counted objects.
	 */
	template <class Type>
	struct FThreadsafeQueue
	{
	private:
		mutable FCriticalSection	SynchronizationObject; // made this mutable so this class can have const functions and still be thread safe
		TQueue<Type>				Items;
	public:
		void Enqueue(const Type& Item)
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			Items.Enqueue(Item);
		}

		bool Dequeue(Type& Result)
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			return Items.Dequeue(Result);
		}

		template <typename CompareFunc>
		bool Dequeue(Type& Result, CompareFunc Func)
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			if (Items.Peek(Result))
			{
				if (Func(Result))
				{
					Items.Dequeue(Result);

					return true;
				}
			}

			return false;
		}

		template <typename CompareFunc>
		bool BatchDequeue(TQueue<Type>* Result, CompareFunc Func, uint32 MaxItems)
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			uint32 i = 0;
			Type Item;
			while (Items.Peek(Item) && i <= MaxItems)
			{
				if (Func(Item))
				{
					Items.Dequeue(Item);

					Result->Enqueue(Item);

					i++;
				}
				else
				{
					break;
				}
			}

			return i > 0;
		}

		bool Peek(Type& Result) const
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			return Items.Peek(Result);
		}

		bool IsEmpty() const
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			return Items.IsEmpty();
		}

		void Empty()
		{
			FScopeLock ScopeLock(&SynchronizationObject);

			Type Result;
			while (Items.Dequeue(Result)) {}
		}
	};

	class FVulkanDeferredDeletionQueue : public FVulkanDeviceChild
	{
		typedef TPair<FVulkanResource*, uint64> FFencedObject;
		FThreadsafeQueue<FFencedObject> DeferredReleaseQueue;

	public:
		FVulkanDeferredDeletionQueue(FVulkanDevice* InDevice);
		~FVulkanDeferredDeletionQueue();

		void EnqueueResource(FVulkanResource* Resource);
		void ReleaseResources(bool bDeleteImmediately = false);

		void Clear()
		{
			ReleaseResources(true);
		}

		class FVulkanAsyncDeletionWorker : public FVulkanDeviceChild, FNonAbandonableTask
		{
		public:
			FVulkanAsyncDeletionWorker(FVulkanDevice* InDevice, FThreadsafeQueue<FFencedObject>* DeletionQueue);

			void DoWork();

		private:
			TQueue<FFencedObject> Queue;
		};

	private:
		TQueue<FAsyncTask<FVulkanAsyncDeletionWorker>*> DeleteTasks;
	};
}
