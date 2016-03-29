// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanSwapChain.h: Vulkan viewport RHI definitions.
=============================================================================*/

#pragma once

struct FVulkanSwapChain
{
#if !PLATFORM_WINDOWS
	//@HACK : maybe NUM_BUFFERS at VulkanViewport should be moved to here?
	enum { NUM_BUFFERS = 2 };
#endif

	FVulkanSwapChain(VkInstance Instance, FVulkanDevice& Device, void* WindowHandle, EPixelFormat& InOutPixelFormat, uint32 Width, uint32 Height,
		uint32* InOutDesiredNumBackBuffers, TArray<VkImage>& OutImages);

	void Destroy(FVulkanDevice& Device);

	VkSwapchainKHR SwapChain;

	PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
	PFN_vkQueuePresentKHR QueuePresentKHR;
	PFN_vkAcquireNextImageKHR AcquireNextImageKHR;

	VkSurfaceKHR Surface;
};