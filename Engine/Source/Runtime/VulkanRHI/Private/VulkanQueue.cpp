// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanQueue.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanQueue.h"
#include "VulkanSwapChain.h"

#if !VULKAN_USE_FENCE_MANAGER
static inline void CreateFence(VkDevice Device, VkFence& Fence)
{
	VkFenceCreateInfo FenceCreateInfo;
	FMemory::Memzero(FenceCreateInfo);
	FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VERIFYVULKANRESULT(vkCreateFence(Device, &FenceCreateInfo, nullptr, &Fence));
}
#endif

FVulkanQueue::FVulkanQueue(FVulkanDevice* InDevice, uint32 InNodeIndex)
	: AcquireNextImageKHR(VK_NULL_HANDLE)
	, QueuePresentKHR(VK_NULL_HANDLE)
	, CurrentFenceIndex(0)
	, CurrentImageIndex(-1)
	, Queue(VK_NULL_HANDLE)
	, NodeIndex(InNodeIndex)
	, Device(InDevice)
{
	check(Device);

	for (int BufferIndex = 0; BufferIndex < VULKAN_NUM_IMAGE_BUFFERS; ++BufferIndex)
	{
		ImageAcquiredSemaphore[BufferIndex] = new FVulkanSemaphore(*Device);
		RenderingCompletedSemaphore[BufferIndex] = new FVulkanSemaphore(*Device);
#if VULKAN_USE_FENCE_MANAGER
		Fences[BufferIndex] = nullptr;
#else
		Fences[BufferIndex] = VK_NULL_HANDLE;
#endif
	}

	vkGetDeviceQueue(Device->GetInstanceHandle(), NodeIndex, 0, &Queue);

	AcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)(void*)vkGetDeviceProcAddr(Device->GetInstanceHandle(), "vkAcquireNextImageKHR");
	check(AcquireNextImageKHR);

	QueuePresentKHR = (PFN_vkQueuePresentKHR)(void*)vkGetDeviceProcAddr(Device->GetInstanceHandle(), "vkQueuePresentKHR");
	check(QueuePresentKHR);
}

FVulkanQueue::~FVulkanQueue()
{
	check(Device);

	if (Fences[CurrentFenceIndex] != VK_NULL_HANDLE)
	{
#if VULKAN_USE_FENCE_MANAGER
		Device->GetFenceManager().WaitForFence(Fences[CurrentFenceIndex], UINT32_MAX);
#else
		VERIFYVULKANRESULT(vkWaitForFences(Device->GetInstanceHandle(), 1, &Fences[CurrentFenceIndex], VK_TRUE, UINT64_MAX));
#endif
	}

	for (int BufferIndex = 0; BufferIndex < VULKAN_NUM_IMAGE_BUFFERS; ++BufferIndex)
	{
		delete ImageAcquiredSemaphore[BufferIndex];
		ImageAcquiredSemaphore[BufferIndex] = nullptr;
		delete RenderingCompletedSemaphore[BufferIndex];
		RenderingCompletedSemaphore[BufferIndex] = nullptr;

#if VULKAN_USE_FENCE_MANAGER
		if (Fences[BufferIndex])
		{
			Device->GetFenceManager().ReleaseFence(Fences[BufferIndex]);
			Fences[BufferIndex] = nullptr;
		}
#else
		if (Fences[BufferIndex] != VK_NULL_HANDLE)
		{
			vkDestroyFence(Device->GetInstanceHandle(), Fences[BufferIndex], nullptr);
			Fences[BufferIndex] = VK_NULL_HANDLE;
		}
#endif
	}
}

uint32 FVulkanQueue::AquireImageIndex(FVulkanSwapChain* Swapchain)
{	
	check(Device);
	check(Swapchain);

	//@TODO: Clean up code once the API and SDK is more robust.
	//@SMEDIS: Since we only have one queue, we shouldn't actually need any semaphores in this .cpp file.
	//@SMEDIS: I also don't think we need the Fences[] array.

	CurrentFenceIndex = (CurrentFenceIndex + 1) % VULKAN_NUM_IMAGE_BUFFERS;

	if (Fences[CurrentFenceIndex] != VK_NULL_HANDLE)
	{
		#if 0
			VkResult Result = vkGetFenceStatus(Device->GetInstanceHandle(), Fences[CurrentFenceIndex]);
			if(Result >= VK_SUCCESS)
		#endif
		{
			// Grab the next fence and re-use it.
#if VULKAN_USE_FENCE_MANAGER
			Device->GetFenceManager().WaitForFence(Fences[CurrentFenceIndex], UINT32_MAX);
			Device->GetFenceManager().ResetFence(Fences[CurrentFenceIndex]);
#else
			VERIFYVULKANRESULT(vkWaitForFences(Device->GetInstanceHandle(), 1, &Fences[CurrentFenceIndex], VK_TRUE, UINT64_MAX));
			VERIFYVULKANRESULT(vkResetFences(Device->GetInstanceHandle(), 1, &Fences[CurrentFenceIndex]));
#endif
		}
	}

	// Get the index of the next swapchain image we should render to.
	// We'll wait with an "infinite" timeout, the function will block until an image is ready.
	// The ImageAcquiredSemaphore[ImageAcquiredSemaphoreIndex] will get signaled when the image is ready (upon function return).
	// The Fences[CurrentFenceIndex] will also get signaled when the image is ready (upon function return).
	// Note: Queues can still be filled in on the CPU side, but won't execute until the semaphore is signaled.
	CurrentImageIndex = -1;
	VkResult Result = AcquireNextImageKHR(
		Device->GetInstanceHandle(),
		Swapchain->SwapChain,
		UINT64_MAX,
		ImageAcquiredSemaphore[CurrentFenceIndex]->GetHandle(),
		VK_NULL_HANDLE,
		&CurrentImageIndex);
	checkf(Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR, TEXT("AcquireNextImageKHR failed Result = %d"), int32(Result));

	return CurrentImageIndex;
}

void FVulkanQueue::Submit(FVulkanCmdBuffer* CmdBuffer)
{
	check(CmdBuffer);
	CmdBuffer->End();

	const VkCommandBuffer CmdBuffers[] = { CmdBuffer->GetHandle(false) };
	const VkPipelineStageFlags WaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo SubmitInfo;
	FMemory::Memzero(SubmitInfo);

	VkSemaphore Local[2] =
		{ 
			ImageAcquiredSemaphore[CurrentFenceIndex]->GetHandle(),
			RenderingCompletedSemaphore[CurrentFenceIndex]->GetHandle()
		};
	SubmitInfo.waitSemaphoreCount = 1;	// 0;	// 1;
	SubmitInfo.pWaitSemaphores = &Local[0];
	SubmitInfo.pWaitDstStageMask = &WaitDstStageMask;
	SubmitInfo.signalSemaphoreCount = 1;	// 0;	// 1;
	SubmitInfo.pSignalSemaphores = &Local[1];
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = CmdBuffers;

#if VULKAN_USE_FENCE_MANAGER
	if (!Fences[CurrentFenceIndex])
	{
		Fences[CurrentFenceIndex] = Device->GetFenceManager().AllocateFence();
	}
	VERIFYVULKANRESULT(vkQueueSubmit(Queue, 1, &SubmitInfo, Fences[CurrentFenceIndex]->GetHandle()));
#else
	if(Fences[CurrentFenceIndex] == VK_NULL_HANDLE)
	{
		CreateFence(Device->GetInstanceHandle(), Fences[CurrentFenceIndex]);
	}
	VERIFYVULKANRESULT(vkQueueSubmit(Queue, 1, &SubmitInfo, Fences[CurrentFenceIndex]));
#endif

#if VULKAN_FORCE_WAIT_FOR_QUEUE
	VERIFYVULKANRESULT(vkQueueWaitIdle(Queue));
#endif
}

void FVulkanQueue::SubmitBlocking(FVulkanCmdBuffer* CmdBuffer)
{
	check(CmdBuffer);
	CmdBuffer->End();

	const VkCommandBuffer CmdBufs[] = { CmdBuffer->GetHandle(false) };

	VkSubmitInfo SubmitInfo;
	FMemory::Memzero(SubmitInfo);
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = CmdBufs;

#if VULKAN_USE_FENCE_MANAGER
	auto* Fence = Device->GetFenceManager().AllocateFence();
	VERIFYVULKANRESULT(vkQueueSubmit(Queue, 1, &SubmitInfo, Fence->GetHandle()));

	bool bSuccess = Device->GetFenceManager().WaitForFence(Fence, 0xFFFFFFFF);
	check(bSuccess);

	Device->GetFenceManager().ReleaseFence(Fence);
#else
	VkFence Fence = { VK_NULL_HANDLE };
	VkFenceCreateInfo FenceCreateInfo;
	FMemory::Memzero(FenceCreateInfo);
	FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VERIFYVULKANRESULT(vkCreateFence(Device->GetInstanceHandle(), &FenceCreateInfo, nullptr, &Fence));

	VERIFYVULKANRESULT(vkQueueSubmit(Queue, 1, &SubmitInfo, Fence));

	VERIFYVULKANRESULT(vkWaitForFences(Device->GetInstanceHandle(), 1, &Fence, true, 0xFFFFFFFF));

	VERIFYVULKANRESULT(vkResetFences(Device->GetInstanceHandle(), 1, &Fence));

	vkDestroyFence(Device->GetInstanceHandle(), Fence, nullptr);
#endif
}

void FVulkanQueue::Present(FVulkanSwapChain* Swapchain, uint32 ImageIndex)
{
	check(Swapchain);

	VkPresentInfoKHR Info;
	FMemory::Memzero(Info);
	Info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	Info.pNext = nullptr;

	Info.waitSemaphoreCount = 1;	// 0;	// 1;
	VkSemaphore Local = RenderingCompletedSemaphore[CurrentFenceIndex]->GetHandle();
	Info.pWaitSemaphores = &Local;
	Info.swapchainCount = 1;
	Info.pSwapchains = &Swapchain->SwapChain;
	Info.pImageIndices = &ImageIndex;

	VERIFYVULKANRESULT(QueuePresentKHR(Queue, &Info));
}
