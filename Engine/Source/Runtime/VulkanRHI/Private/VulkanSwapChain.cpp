// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanSwapChain.h: Vulkan viewport RHI definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanSwapChain.h"

FVulkanSwapChain::FVulkanSwapChain(VkInstance Instance, FVulkanDevice& InDevice, void* WindowHandle, EPixelFormat& InOutPixelFormat, uint32 Width, uint32 Height, 
	uint32* InOutDesiredNumBackBuffers, TArray<VkImage>& OutImages)
	: SwapChain(VK_NULL_HANDLE)
	, Device(InDevice)
	, Surface(VK_NULL_HANDLE)
	, CurrentImageIndex(-1)
	, SemaphoreIndex(0)
{
#if PLATFORM_WINDOWS
	VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo;
	FMemory::Memzero(SurfaceCreateInfo);
	SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
	SurfaceCreateInfo.hwnd = (HWND)WindowHandle;
	VERIFYVULKANRESULT(vkCreateWin32SurfaceKHR(Instance, &SurfaceCreateInfo, nullptr, &Surface));
#elif PLATFORM_ANDROID
	VkAndroidSurfaceCreateInfoKHR SurfaceCreateInfo;
	FMemory::Memzero(SurfaceCreateInfo);
	SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	SurfaceCreateInfo.window = (ANativeWindow*)WindowHandle;

	VERIFYVULKANRESULT(vkCreateAndroidSurfaceKHR(Instance, &SurfaceCreateInfo, nullptr, &Surface));
#else
	static_assert(false, "Unsupported Vulkan platform!");
#endif

	// Find Pixel format for presentable images
	VkSurfaceFormatKHR CurrFormat;
	FMemory::Memzero(CurrFormat);
	{
		uint32 NumFormats;
		VERIFYVULKANRESULT_EXPANDED(vkGetPhysicalDeviceSurfaceFormatsKHR(Device.GetPhysicalHandle(), Surface, &NumFormats, nullptr));
		check(NumFormats > 0);

		TArray<VkSurfaceFormatKHR> Formats;
		Formats.AddZeroed(NumFormats);
		VERIFYVULKANRESULT_EXPANDED(vkGetPhysicalDeviceSurfaceFormatsKHR(Device.GetPhysicalHandle(), Surface, &NumFormats, Formats.GetData()));

		if (Formats.Num() == 1 && Formats[0].format == VK_FORMAT_UNDEFINED && InOutPixelFormat == PF_Unknown)
		{
			InOutPixelFormat = PF_B8G8R8A8;
		}
		else if (InOutPixelFormat == PF_Unknown)
		{
			// Reverse lookup
			check(Formats[0].format != VK_FORMAT_UNDEFINED);
			for (int32 Index = 0; Index < PF_MAX; ++Index)
			{
				if (Formats[0].format == GPixelFormats[Index].PlatformFormat)
				{
					InOutPixelFormat = (EPixelFormat)Index;
					CurrFormat = Formats[0];
					break;
				}
			}
		}
		else
		{
			VkFormat PlatformFormat = UEToVkFormat(InOutPixelFormat, false);
			bool bSupported = false;
			for (int32 Index = 0; Index < Formats.Num(); ++Index)
			{
				if (Formats[Index].format == PlatformFormat)
				{
					bSupported = true;
					CurrFormat = Formats[Index];
					break;
				}
			}

			check(bSupported);
		}
	}

	VkFormat PlatformFormat = UEToVkFormat(InOutPixelFormat, false);

	//#todo-rco: Check multiple Gfx Queues?
	VkBool32 bSupportsPresent = VK_FALSE;
	VERIFYVULKANRESULT(vkGetPhysicalDeviceSurfaceSupportKHR(Device.GetPhysicalHandle(), Device.GetQueue()->GetFamilyIndex(), Surface, &bSupportsPresent));
	//#todo-rco: Find separate present queue if the gfx one doesn't support presents
	check(bSupportsPresent);

	// Fetch present mode
	VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
#if !PLATFORM_ANDROID
	{
		uint32 NumFoundPresentModes = 0;
		VERIFYVULKANRESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(Device.GetPhysicalHandle(), Surface, &NumFoundPresentModes, nullptr));
		check(NumFoundPresentModes > 0);

		TArray<VkPresentModeKHR> FoundPresentModes;
		FoundPresentModes.AddZeroed(NumFoundPresentModes);
		VERIFYVULKANRESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(Device.GetPhysicalHandle(), Surface, &NumFoundPresentModes, FoundPresentModes.GetData()));

		bool bFoundDesiredMode = false;
		for (size_t i = 0; i < NumFoundPresentModes; i++)
		{
			if (FoundPresentModes[i] == PresentMode)
			{
				bFoundDesiredMode = true;
				break;
			}
		}
		if (!bFoundDesiredMode)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Couldn't find Present Mode %d!"), (int32)PresentMode);
			PresentMode = FoundPresentModes[0];
		}
	}
#endif

	// Check the surface properties and formats
	
	VkSurfaceCapabilitiesKHR SurfProperties;
	VERIFYVULKANRESULT_EXPANDED(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device.GetPhysicalHandle(),
		Surface,
		&SurfProperties));
	VkSurfaceTransformFlagBitsKHR PreTransform;
	if (SurfProperties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		PreTransform = SurfProperties.currentTransform;
	}
	uint32 DesiredNumBuffers = FMath::Clamp(*InOutDesiredNumBackBuffers, SurfProperties.minImageCount, SurfProperties.maxImageCount);
	
	VkSwapchainCreateInfoKHR SwapChainInfo;
	FMemory::Memzero(SwapChainInfo);
	SwapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	SwapChainInfo.surface = Surface;
	SwapChainInfo.minImageCount = DesiredNumBuffers;
	SwapChainInfo.imageFormat = CurrFormat.format;
	SwapChainInfo.imageColorSpace = CurrFormat.colorSpace;
	SwapChainInfo.imageExtent.width = PLATFORM_ANDROID ? Width : (SurfProperties.currentExtent.width == -1 ? Width : SurfProperties.currentExtent.width);
	SwapChainInfo.imageExtent.height = PLATFORM_ANDROID ? Height : (SurfProperties.currentExtent.height == -1 ? Height : SurfProperties.currentExtent.height);
	SwapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	SwapChainInfo.preTransform = PreTransform;
	SwapChainInfo.imageArrayLayers = 1;
	SwapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	SwapChainInfo.presentMode = PresentMode;
	SwapChainInfo.oldSwapchain = VK_NULL_HANDLE;
	SwapChainInfo.clipped = VK_TRUE;
	SwapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	*InOutDesiredNumBackBuffers = DesiredNumBuffers;

	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateSwapchainKHR(Device.GetInstanceHandle(), &SwapChainInfo, nullptr, &SwapChain));

	uint32 NumSwapChainImages;
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetSwapchainImagesKHR(Device.GetInstanceHandle(), SwapChain, &NumSwapChainImages, nullptr));

	OutImages.AddUninitialized(NumSwapChainImages);
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkGetSwapchainImagesKHR(Device.GetInstanceHandle(), SwapChain, &NumSwapChainImages, OutImages.GetData()));

	ImageAcquiredSemaphore.AddUninitialized(DesiredNumBuffers);
	for (uint32 BufferIndex = 0; BufferIndex < DesiredNumBuffers; ++BufferIndex)
	{
		ImageAcquiredSemaphore[BufferIndex] = new FVulkanSemaphore(Device);
	}
}

void FVulkanSwapChain::Destroy()
{
	VulkanRHI::vkDestroySwapchainKHR(Device.GetInstanceHandle(), SwapChain, nullptr);
	SwapChain = VK_NULL_HANDLE;

	//#todo-rco: Enqueue for deletion as we first need to destroy the cmd buffers and queues otherwise validation fails
	for (int BufferIndex = 0; BufferIndex < ImageAcquiredSemaphore.Num(); ++BufferIndex)
	{
		delete ImageAcquiredSemaphore[BufferIndex];
	}
}

int32 FVulkanSwapChain::AcquireImageIndex(FVulkanSemaphore** OutSemaphore)
{
	// Get the index of the next swapchain image we should render to.
	// We'll wait with an "infinite" timeout, the function will block until an image is ready.
	// The ImageAcquiredSemaphore[ImageAcquiredSemaphoreIndex] will get signaled when the image is ready (upon function return).
	uint32 ImageIndex = 0;
	SemaphoreIndex = (SemaphoreIndex + 1) % ImageAcquiredSemaphore.Num();
	*OutSemaphore = ImageAcquiredSemaphore[SemaphoreIndex];
	VkResult Result = VulkanRHI::vkAcquireNextImageKHR(
		Device.GetInstanceHandle(),
		SwapChain,
		UINT64_MAX,
		ImageAcquiredSemaphore[SemaphoreIndex]->GetHandle(),
		VK_NULL_HANDLE,	// Currently no fence needed
		&ImageIndex);
	checkf(Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR, TEXT("AcquireNextImageKHR failed Result = %d"), int32(Result));
	CurrentImageIndex = (int32)ImageIndex;
	check(CurrentImageIndex == ImageIndex);
	return CurrentImageIndex;
}

bool FVulkanSwapChain::Present(FVulkanQueue* Queue, FVulkanSemaphore* BackBufferRenderingDoneSemaphore)
{
	check(CurrentImageIndex != -1);

	VkPresentInfoKHR Info;
	FMemory::Memzero(Info);
	Info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	VkSemaphore Semaphore = VK_NULL_HANDLE;
	if (BackBufferRenderingDoneSemaphore)
	{
		Info.waitSemaphoreCount = 1;
		Semaphore = BackBufferRenderingDoneSemaphore->GetHandle();
		Info.pWaitSemaphores = &Semaphore;
	}
	Info.swapchainCount = 1;
	Info.pSwapchains = &SwapChain;
	Info.pImageIndices = (uint32*)&CurrentImageIndex;

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanQueuePresent);
		VERIFYVULKANRESULT(VulkanRHI::vkQueuePresentKHR(Queue->GetHandle(), &Info));
	}

	return true;
}
