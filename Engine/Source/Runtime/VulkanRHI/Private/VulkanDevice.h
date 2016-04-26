// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanDevice.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanMemory.h"

class FVulkanDescriptorPool;

struct FVulkanDevice
{
	enum
	{
		NumTimestampPools = 3,	// Must be the same size as the number of the backbuffer images
	};

	FVulkanDevice(VkPhysicalDevice Gpu);

	~FVulkanDevice();

	// Returns true if this is a viable candidate for main GPU
	bool QueryGPU(int32 DeviceIndex);

	void InitGPU(int32 DeviceIndex);

	void InitDevice();

	void Destroy();

	void WaitUntilIdle();

	inline FVulkanPendingState& GetPendingState()
	{
		check(PendingState);
		return *PendingState;
	}

	void EndCommandBufferBlock(FVulkanCmdBuffer* CmdBuffer);

	inline FVulkanQueue* GetQueue()
	{
		check(Queue);
		return Queue;
	}

	inline VkPhysicalDevice GetPhysicalHandle()
	{
		check(Gpu != VK_NULL_HANDLE);
		return Gpu;
	}

	inline FVulkanRingBuffer* GetVBIBRingBuffer()
	{
		return VBIBRingBuffer;
	}

	inline FVulkanRingBuffer* GetUBRingBuffer()
	{
		return UBRingBuffer;
	}

	inline const VkPhysicalDeviceProperties& GetDeviceProperties() const
	{
		return GpuProps;
	}

	inline const VkPhysicalDeviceLimits& GetLimits() const
	{
		return GpuProps.limits;
	}

	inline const VkPhysicalDeviceFeatures& GetFeatures() const
	{
		return Features;
	}

	bool IsFormatSupported(VkFormat Format) const;

	const VkComponentMapping& GetFormatComponentMapping(EPixelFormat UEFormat) const;
	
	inline VkDevice GetInstanceHandle()
	{
		check(Device != VK_NULL_HANDLE);
		return Device;
	}

	inline VkSampler GetDefaultSampler() const
	{
		return DefaultSampler->Sampler;
	}

	inline FVulkanTimestampQueryPool* GetTimestampQueryPool(uint32 Index)
	{
		check(Index < NumTimestampPools);
		return TimestampQueryPool[Index];
	}

	inline const VkFormatProperties* GetFormatProperties() const
	{
		return FormatProperties;
	}

	void BindSRV(FVulkanShaderResourceView* SRV, uint32 TextureIndex, EShaderFrequency Stage);

	VulkanRHI::FDeviceMemoryManager& GetMemoryManager()
	{
		return MemoryManager;
	}

	VulkanRHI::FResourceAllocationManager& GetResourceAllocationManager()
	{
		return ResourceAllocationManager;
	}

	VulkanRHI::FStagingManager& GetStagingManager()
	{
		return StagingManager;
	}

	FVulkanDescriptorPool* GetDescriptorPool()
	{
		return DescriptorPool;
	}

#if VULKAN_USE_FENCE_MANAGER
	VulkanRHI::FFenceManager& GetFenceManager()
	{
		return FenceManager;
	}
#endif

	FVulkanCommandListContext& GetImmediateContext()
	{
		return *ImmediateContext;
	}

#if VULKAN_ENABLE_DRAW_MARKERS
	PFN_vkCmdDbgMarkerBegin GetCmdDbgMarkerBegin() const
	{
		return VkCmdDbgMarkerBegin;
	}

	PFN_vkCmdDbgMarkerEnd GetCmdDbgMarkerEnd() const
	{
		return VkCmdDbgMarkerEnd;
	}
#endif

private:
	void MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat);
	void MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat, int32 BlockBytes);
	void SetComponentMapping(EPixelFormat UEFormat, VkComponentSwizzle r, VkComponentSwizzle g, VkComponentSwizzle b, VkComponentSwizzle a);

	VkPhysicalDevice Gpu;
	VkPhysicalDeviceProperties GpuProps;
	VkPhysicalDeviceFeatures Features;
	
	VulkanRHI::FDeviceMemoryManager MemoryManager;
	VulkanRHI::FResourceAllocationManager ResourceAllocationManager;
	VulkanRHI::FStagingManager StagingManager;

#if VULKAN_USE_FENCE_MANAGER
	VulkanRHI::FFenceManager FenceManager;
#endif

	VkDevice Device;

	FVulkanDescriptorPool* DescriptorPool;

	FVulkanSamplerState* DefaultSampler;

	TArray<VkQueueFamilyProperties> QueueProps;
	VkFormatProperties FormatProperties[VK_FORMAT_RANGE_SIZE];

	// Nullptr if not supported
	FVulkanTimestampQueryPool* TimestampQueryPool[NumTimestampPools];

	FVulkanQueue* Queue;

	FVulkanPendingState* PendingState;
	VkComponentMapping PixelFormatComponentMapping[PF_MAX];

	class FVulkanCommandListContext* ImmediateContext;

	FVulkanRingBuffer* VBIBRingBuffer;
	FVulkanRingBuffer* UBRingBuffer;

	void GetDeviceExtensions(TArray<const ANSICHAR*>& OutDeviceExtensions, TArray<const ANSICHAR*>& OutDeviceLayers);

#if VULKAN_ENABLE_DRAW_MARKERS
	PFN_vkCmdDbgMarkerBegin VkCmdDbgMarkerBegin;
	PFN_vkCmdDbgMarkerEnd VkCmdDbgMarkerEnd;
	friend class FVulkanCommandListContext;
#endif

public:
	uint64 FrameCounter;

#if VULKAN_ENABLE_PIPELINE_CACHE
	class FVulkanPipelineStateCache* PipelineStateCache;
#endif
};
