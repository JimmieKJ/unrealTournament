// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanQueue.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanRHIPrivate.h"

struct FVulkanDevice;
class FVulkanCmdBuffer;
struct FVulkanSemaphore;
struct FVulkanSwapChain;

struct FVulkanQueue
{
	FVulkanQueue(FVulkanDevice* InDevice, uint32 InNodeIndex);

	~FVulkanQueue();

	inline uint32 GetNodeIndex() const
	{
		return NodeIndex;
	}

	void Submit(FVulkanCmdBuffer* CmdBuffer);

	void SubmitBlocking(FVulkanCmdBuffer* CmdBuffer);

	uint32 AquireImageIndex(FVulkanSwapChain* Swapchain);
	
	void Present(FVulkanSwapChain* Swapchain, uint32 ImageIndex);

	inline VkQueue GetHandle() const
	{
		return Queue;
	}

private:
	PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
	PFN_vkQueuePresentKHR QueuePresentKHR;

	FVulkanSemaphore* ImageAcquiredSemaphore[VULKAN_NUM_IMAGE_BUFFERS];
	FVulkanSemaphore* RenderingCompletedSemaphore[VULKAN_NUM_IMAGE_BUFFERS];
#if VULKAN_USE_FENCE_MANAGER
	VulkanRHI::FFence* Fences[VULKAN_NUM_IMAGE_BUFFERS];
#else
	VkFence Fences[VULKAN_NUM_IMAGE_BUFFERS];
#endif
	uint32	CurrentFenceIndex;

	uint32 CurrentImageIndex;	// Perhaps move this to the FVulkanSwapChain?

	VkQueue Queue;
	uint32 NodeIndex;
	FVulkanDevice* Device;
};

