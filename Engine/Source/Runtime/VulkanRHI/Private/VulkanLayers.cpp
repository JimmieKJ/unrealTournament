// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanLayers.cpp: Vulkan device layers implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"

#if VULKAN_HAS_DEBUGGING_ENABLED
TAutoConsoleVariable<int32> GValidationCvar(
	TEXT("r.Vulkan.EnableValidation"),
	0,
	TEXT("1 to use validation layers")
	);

// List of validation layers which we want to activate for the (device)-instance (used in VulkanRHI.cpp)
static const ANSICHAR* GValidationLayersInstance[] =
{
#if UE_VK_API_VERSION >= VK_MAKE_VERSION(1, 0, 5)
	"VK_LAYER_GOOGLE_threading",
#else
	"VK_LAYER_LUNARG_threading",
#endif
	"VK_LAYER_LUNARG_param_checker",
	"VK_LAYER_LUNARG_device_limits",
	"VK_LAYER_LUNARG_object_tracker",	// The framebuffer is not registered for some reason by the object tracker... the steps are exactly the same as in the demo. For now ObjectTracker is disabled...
	"VK_LAYER_LUNARG_image",
	"VK_LAYER_LUNARG_mem_tracker",
	"VK_LAYER_LUNARG_draw_state",
	"VK_LAYER_LUNARG_swapchain",
	"VK_LAYER_GOOGLE_unique_objects",
#if VULKAN_ENABLE_API_DUMP
	"VK_LAYER_LUNARG_api_dump",
#endif
	//"VK_LAYER_LUNARG_vktrace",		// Useful for future
};

// List of validation layers which we want to activate for the device
static const ANSICHAR* GValidationLayersDevice[] =
{
#if UE_VK_API_VERSION >= VK_MAKE_VERSION(1, 0, 5)
	"VK_LAYER_GOOGLE_threading",
#else
	"VK_LAYER_LUNARG_threading",
#endif
	"VK_LAYER_LUNARG_param_checker",
	"VK_LAYER_LUNARG_device_limits",
	"VK_LAYER_LUNARG_object_tracker",	// The framebuffer is not registered for some reason by the object tracker... the steps are exactly the same as in the demo. For now ObjectTracker is disabled...
	"VK_LAYER_LUNARG_image",
	"VK_LAYER_LUNARG_mem_tracker",
	"VK_LAYER_LUNARG_draw_state",
	"VK_LAYER_LUNARG_swapchain",
	"VK_LAYER_GOOGLE_unique_objects",
#if VULKAN_ENABLE_API_DUMP
	"VK_LAYER_LUNARG_api_dump",
#endif
	//"VK_LAYER_LUNARG_vktrace",		// Useful for future
};
#endif // VULKAN_HAS_DEBUGGING_ENABLED

// Instance Extensions to enable
static const ANSICHAR* GInstanceExtensions[] =
{
	VK_KHR_SURFACE_EXTENSION_NAME,
#if PLATFORM_ANDROID
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#if !VULKAN_DISABLE_DEBUG_CALLBACK
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#endif
};

// Device Extensions to enable
static const ANSICHAR* GDeviceExtensions[] =
{
	//	VK_KHR_SURFACE_EXTENSION_NAME,			// Not supported, even if it's reported as a valid extension... (SDK/driver bug?)
#if PLATFORM_ANDROID
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
	//	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,	// Not supported, even if it's reported as a valid extension... (SDK/driver bug?)
#endif
#if VULKAN_ENABLE_DRAW_MARKERS
	DEBUG_MARKER_EXTENSION_NAME,
#endif
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};


#define RENDERDOC_LAYER_NAME	"VK_LAYER_RENDERDOC_Capture"


struct FLayerExtension
{
	FLayerExtension()
	{
		FMemory::Memzero(LayerProps);
	}

	VkLayerProperties LayerProps;
	TArray<VkExtensionProperties> ExtensionProps;
};

static inline void GetInstanceLayerExtensions(const ANSICHAR* LayerName, FLayerExtension& OutLayer)
{
	VkResult Result;
	do
	{
		//@TODO: Currently unsupported on device, so just make sure it doesn't cause problems
		uint32 Count = 0;
		Result = vkEnumerateInstanceExtensionProperties(LayerName, &Count, nullptr);
		check(Result >= VK_SUCCESS);

		if (Count > 0)
		{
			OutLayer.ExtensionProps.Empty(Count);
			OutLayer.ExtensionProps.AddUninitialized(Count);
			Result = vkEnumerateInstanceExtensionProperties(LayerName, &Count, OutLayer.ExtensionProps.GetData());
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);
}

static inline void GetDeviceLayerExtensions(VkPhysicalDevice Device, const ANSICHAR* LayerName, FLayerExtension& OutLayer)
{
	VkResult Result;
	do
	{
		uint32 Count = 0;
		Result = vkEnumerateDeviceExtensionProperties(Device, LayerName, &Count, nullptr);
		check(Result >= VK_SUCCESS);

		if (Count > 0)
		{
			OutLayer.ExtensionProps.Empty(Count);
			OutLayer.ExtensionProps.AddUninitialized(Count);
			Result = vkEnumerateDeviceExtensionProperties(Device, LayerName, &Count, OutLayer.ExtensionProps.GetData());
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);
}


void FVulkanDynamicRHI::GetInstanceLayersList(TArray<const ANSICHAR*>& OutInstanceExtensions, TArray<const ANSICHAR*>& OutInstanceLayers)
{
	TArray<FLayerExtension> GlobalLayers;
	FLayerExtension GlobalExtensions;

	VkResult Result;

	// Global extensions
	FMemory::Memzero(GlobalExtensions.LayerProps);
	GetInstanceLayerExtensions(nullptr, GlobalExtensions);

	// Now per layer
	TArray<VkLayerProperties> GlobalLayerProperties;
	do
	{
		uint32 InstanceLayerCount = 0;
		Result = vkEnumerateInstanceLayerProperties(&InstanceLayerCount, nullptr);
		check(Result >= VK_SUCCESS);

		if (InstanceLayerCount > 0)
		{
			GlobalLayers.Empty(InstanceLayerCount);
			GlobalLayerProperties.AddZeroed(InstanceLayerCount);
			Result = vkEnumerateInstanceLayerProperties(&InstanceLayerCount, &GlobalLayerProperties[GlobalLayerProperties.Num() - InstanceLayerCount]);
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);

	bool bRenderDocFound = false;
	for (int32 Index = 0; Index < GlobalLayerProperties.Num(); ++Index)
	{
		auto* Layer = new(GlobalLayers) FLayerExtension;
		Layer->LayerProps = GlobalLayerProperties[Index];
		GetInstanceLayerExtensions(GlobalLayerProperties[Index].layerName, *Layer);
		if (!bRenderDocFound)
		{
			if (!FCStringAnsi::Strcmp(GlobalLayerProperties[Index].layerName, RENDERDOC_LAYER_NAME))
			{
				bRenderDocFound  = true;
			}
			UE_LOG(LogVulkanRHI, Display, TEXT("- Found Global Layer %s"), ANSI_TO_TCHAR(GlobalLayerProperties[Index].layerName));
		}
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	if (GValidationCvar.GetValueOnAnyThread() > 0)
	{
		// Verify that all requested debugging device-layers are available
		for (uint32 LayerIndex = 0; LayerIndex < ARRAY_COUNT(GValidationLayersInstance); ++LayerIndex)
		{
			bool bValidationFound = false;
			const ANSICHAR* CurrValidationLayer = GValidationLayersInstance[LayerIndex];
			for (int32 Index = 0; Index < GlobalLayers.Num(); ++Index)
			{
				if (!FCStringAnsi::Strcmp(GlobalLayers[Index].LayerProps.layerName, CurrValidationLayer))
				{
					bValidationFound = true;
					OutInstanceLayers.Add(CurrValidationLayer);
					break;
				}
			}

			if (!bValidationFound)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan instance validation layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
			}
		}
	}
#endif	// VULKAN_HAS_DEBUGGING_ENABLED

	//@TODO: Android driver hasn't implemented extensions yet
#if !PLATFORM_ANDROID
	for (int32 i = 0; i < GlobalExtensions.ExtensionProps.Num(); i++)
	{
		for (int32 j = 0; j < ARRAY_COUNT(GInstanceExtensions); j++)
		{
			if (!FCStringAnsi::Strcmp(GlobalExtensions.ExtensionProps[i].extensionName, GInstanceExtensions[j]))
			{
				OutInstanceExtensions.Add(GInstanceExtensions[j]);
				break;
			}
		}
	}
#endif	// !PLATFORM_ANDROID

	if (OutInstanceExtensions.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using instance extensions"));
		for (auto* Extension : OutInstanceExtensions)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
		}
	}

	if (OutInstanceLayers.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using instance layers"));
		for (auto* Layer : OutInstanceLayers)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
		}
	}
}


void FVulkanDevice::GetDeviceExtensions(TArray<const ANSICHAR*>& OutDeviceExtensions, TArray<const ANSICHAR*>& OutDeviceLayers)
{
	// Setup device layer properties
	TArray<VkLayerProperties> LayerProperties;
	{
		uint32 Count = 0;
		VERIFYVULKANRESULT(vkEnumerateDeviceLayerProperties(Gpu, &Count, nullptr));
		LayerProperties.AddZeroed(Count);
		VERIFYVULKANRESULT(vkEnumerateDeviceLayerProperties(Gpu, &Count, (VkLayerProperties*)LayerProperties.GetData()));
		check(Count == LayerProperties.Num());
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	// Verify that all requested debugging device-layers are available
	if (GValidationCvar.GetValueOnAnyThread() > 0)
	{
		for (uint32 LayerIndex = 0; LayerIndex < ARRAY_COUNT(GValidationLayersDevice); ++LayerIndex)
		{
			bool bValidationFound = false;
			const ANSICHAR* CurrValidationLayer = GValidationLayersDevice[LayerIndex];
			for (int32 Index = 0; Index < LayerProperties.Num(); ++Index)
			{
				if (!FCStringAnsi::Strcmp(LayerProperties[Index].layerName, CurrValidationLayer))
				{
					bValidationFound = true;
					OutDeviceLayers.Add(CurrValidationLayer);
					break;
				}
			}

			if (!bValidationFound)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan device validation layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
			}
		}

	}
#endif	// VULKAN_HAS_DEBUGGING_ENABLED

	//@TODO: Extensions mechanisms are currently unavailable
#if !PLATFORM_ANDROID
	FLayerExtension Extensions;
	FMemory::Memzero(Extensions.LayerProps);
	GetDeviceLayerExtensions(Gpu, nullptr, Extensions);

	for (uint32 Index = 0; Index < ARRAY_COUNT(GDeviceExtensions); ++Index)
	{
		for (int32 i = 0; i < Extensions.ExtensionProps.Num(); i++)
		{
			if (!FCStringAnsi::Strcmp(GDeviceExtensions[Index], Extensions.ExtensionProps[i].extensionName))
			{
				OutDeviceExtensions.Add(GDeviceExtensions[Index]);
				break;
			}
		}
	}

#if VULKAN_ENABLE_DRAW_MARKERS
#endif	// VULKAN_ENABLE_DRAW_MARKERS
#endif	// !PLATFORM_ANDROID

	if (OutDeviceExtensions.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using device extensions"));
		for (auto* Extension : OutDeviceExtensions)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
		}
	}

	if (OutDeviceLayers.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using device layers"));
		for (auto* Layer : OutDeviceLayers)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
		}
	}
}
