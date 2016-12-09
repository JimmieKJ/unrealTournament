// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanDevice.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanMemory.h"

class FVulkanDescriptorPool;
class FVulkanCommandListContext;

class FVulkanDevice
{
public:
	FVulkanDevice(VkPhysicalDevice Gpu);

	~FVulkanDevice();

	// Returns true if this is a viable candidate for main GPU
	bool QueryGPU(int32 DeviceIndex);

	void InitGPU(int32 DeviceIndex);

	void CreateDevice();

	void PrepareForDestroy();
	void Destroy();

	void WaitUntilIdle();

	inline FVulkanQueue* GetQueue()
	{
		return Queue;
	}

	inline VkPhysicalDevice GetPhysicalHandle() const
	{
		return Gpu;
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

	inline bool HasUnifiedMemory() const
	{
		return MemoryManager.HasUnifiedMemory();
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

	inline const VkFormatProperties* GetFormatProperties() const
	{
		return FormatProperties;
	}

	inline VulkanRHI::FDeviceMemoryManager& GetMemoryManager()
	{
		return MemoryManager;
	}

	inline VulkanRHI::FResourceHeapManager& GetResourceHeapManager()
	{
		return ResourceHeapManager;
	}

	inline VulkanRHI::FDeferredDeletionQueue& GetDeferredDeletionQueue()
	{
		return DeferredDeletionQueue;
	}

	inline VulkanRHI::FStagingManager& GetStagingManager()
	{
		return StagingManager;
	}

	inline VulkanRHI::FFenceManager& GetFenceManager()
	{
		return FenceManager;
	}

	inline FVulkanCommandListContext& GetImmediateContext()
	{
		return *ImmediateContext;
	}

	void NotifyDeletedRenderTarget(const FVulkanTextureBase* Texture);

#if VULKAN_ENABLE_DRAW_MARKERS
	PFN_vkCmdDebugMarkerBeginEXT GetCmdDbgMarkerBegin() const
	{
		return CmdDbgMarkerBegin;
	}

	PFN_vkCmdDebugMarkerEndEXT GetCmdDbgMarkerEnd() const
	{
		return CmdDbgMarkerEnd;
	}

	PFN_vkDebugMarkerSetObjectNameEXT GetDebugMarkerSetObjectName() const
	{
		return DebugMarkerSetObjectName;
	}
#endif

	void PrepareForCPURead();

	void SubmitCommandsAndFlushGPU();

	inline FVulkanOcclusionQueryPool& FindAvailableOcclusionQueryPool(FVulkanCmdBuffer* CmdBuffer)
	{
		// First try to find An available one
		for (int32 Index = 0; Index < OcclusionQueryPools.Num(); ++Index)
		{
			FVulkanOcclusionQueryPool* Pool = OcclusionQueryPools[Index];
			if (Pool->HasRoom())
			{
				return *Pool;
			}
		}

		// None found, so allocate new Pool
		FVulkanOcclusionQueryPool* Pool = new FVulkanOcclusionQueryPool(this, NUM_OCCLUSION_QUERIES_PER_POOL);
		OcclusionQueryPools.Add(Pool);
		return *Pool;
	}

	inline FVulkanTimestampPool* GetTimestampQueryPool()
	{
		return TimestampQueryPool;
	}

private:
	void MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat);
	void MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat, int32 BlockBytes);
	void SetComponentMapping(EPixelFormat UEFormat, VkComponentSwizzle r, VkComponentSwizzle g, VkComponentSwizzle b, VkComponentSwizzle a);

	VkPhysicalDevice Gpu;
	VkPhysicalDeviceProperties GpuProps;
	VkPhysicalDeviceFeatures Features;
	
	VkDevice Device;

	VulkanRHI::FDeviceMemoryManager MemoryManager;

	VulkanRHI::FResourceHeapManager ResourceHeapManager;

	VulkanRHI::FDeferredDeletionQueue DeferredDeletionQueue;

	VulkanRHI::FStagingManager StagingManager;

	VulkanRHI::FFenceManager FenceManager;

	FVulkanSamplerState* DefaultSampler;

	TArray<VkQueueFamilyProperties> QueueFamilyProps;
	VkFormatProperties FormatProperties[VK_FORMAT_RANGE_SIZE];
	// Info for formats that are not in the core Vulkan spec (i.e. extensions)
	mutable TMap<VkFormat, VkFormatProperties> ExtensionFormatProperties;

	TArray<FVulkanOcclusionQueryPool*> OcclusionQueryPools;
	FVulkanTimestampPool* TimestampQueryPool;

	FVulkanQueue* Queue;

	VkComponentMapping PixelFormatComponentMapping[PF_MAX];

	FVulkanCommandListContext* ImmediateContext;

	void GetDeviceExtensions(TArray<const ANSICHAR*>& OutDeviceExtensions, TArray<const ANSICHAR*>& OutDeviceLayers, bool& bOutDebugMarkers);
	void SetupFormats();

#if VULKAN_ENABLE_DRAW_MARKERS
	PFN_vkCmdDebugMarkerBeginEXT CmdDbgMarkerBegin;
	PFN_vkCmdDebugMarkerEndEXT CmdDbgMarkerEnd;
	PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectName;
	friend class FVulkanCommandListContext;
#endif

#if VULKAN_ENABLE_PIPELINE_CACHE
	class FVulkanPipelineStateCache* PipelineStateCache;
	friend class FVulkanDynamicRHI;
	friend class FVulkanBoundShaderState;
#endif
};
