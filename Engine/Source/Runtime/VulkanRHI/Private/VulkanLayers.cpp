// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanLayers.cpp: Vulkan device layers implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"

#if VULKAN_HAS_DEBUGGING_ENABLED

	#if VULKAN_ENABLE_DRAW_MARKERS
		#define RENDERDOC_LAYER_NAME	"VK_LAYER_RENDERDOC_Capture"
	#endif

TAutoConsoleVariable<int32> GValidationCvar(
	TEXT("r.Vulkan.EnableValidation"),
	0,
	TEXT("1 to use validation layers"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
	);

// List of validation layers which we want to activate for the instance
static const ANSICHAR* GRequiredLayersInstance[] =
{
	"VK_LAYER_LUNARG_swapchain",
};

#define VULKAN_ENABLE_STANDARD_VALIDATION	1

// List of validation layers which we want to activate for the instance
static const ANSICHAR* GValidationLayersInstance[] =
{
#if VULKAN_ENABLE_API_DUMP
	"VK_LAYER_LUNARG_api_dump",
#endif

#if VULKAN_ENABLE_STANDARD_VALIDATION
	"VK_LAYER_LUNARG_standard_validation",
#else
	"VK_LAYER_GOOGLE_threading",
	"VK_LAYER_LUNARG_parameter_validation",
	"VK_LAYER_LUNARG_object_tracker",
	"VK_LAYER_LUNARG_image",
	"VK_LAYER_LUNARG_core_validation",
	"VK_LAYER_LUNARG_swapchain",
	"VK_LAYER_GOOGLE_unique_objects",
#endif

	"VK_LAYER_LUNARG_device_limits",
	//"VK_LAYER_LUNARG_screenshot",
	//"VK_LAYER_NV_optimus",
	//"VK_LAYER_LUNARG_vktrace",		// Useful for future
};

// List of validation layers which we want to activate for the device
static const ANSICHAR* GRequiredLayersDevice[] =
{
	"VK_LAYER_LUNARG_swapchain",
};

// List of validation layers which we want to activate for the device
static const ANSICHAR* GValidationLayersDevice[] =
{
#if VULKAN_ENABLE_API_DUMP
	"VK_LAYER_LUNARG_api_dump",
#endif

#if VULKAN_ENABLE_STANDARD_VALIDATION
	"VK_LAYER_LUNARG_standard_validation",
#else
	"VK_LAYER_GOOGLE_threading",
	"VK_LAYER_LUNARG_parameter_validation",
	"VK_LAYER_LUNARG_object_tracker",
	"VK_LAYER_LUNARG_image",
	"VK_LAYER_LUNARG_core_validation",
	"VK_LAYER_LUNARG_swapchain",
	"VK_LAYER_GOOGLE_unique_objects",
#endif

	"VK_LAYER_LUNARG_device_limits",
	//"VK_LAYER_LUNARG_screenshot",
	//"VK_LAYER_NV_optimus",
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
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
	//	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,	// Not supported, even if it's reported as a valid extension... (SDK/driver bug?)
#endif
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};


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
		Result = VulkanRHI::vkEnumerateInstanceExtensionProperties(LayerName, &Count, nullptr);
		check(Result >= VK_SUCCESS);

		if (Count > 0)
		{
			OutLayer.ExtensionProps.Empty(Count);
			OutLayer.ExtensionProps.AddUninitialized(Count);
			Result = VulkanRHI::vkEnumerateInstanceExtensionProperties(LayerName, &Count, OutLayer.ExtensionProps.GetData());
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
		Result = VulkanRHI::vkEnumerateDeviceExtensionProperties(Device, LayerName, &Count, nullptr);
		check(Result >= VK_SUCCESS);

		if (Count > 0)
		{
			OutLayer.ExtensionProps.Empty(Count);
			OutLayer.ExtensionProps.AddUninitialized(Count);
			Result = VulkanRHI::vkEnumerateDeviceExtensionProperties(Device, LayerName, &Count, OutLayer.ExtensionProps.GetData());
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);
}


void FVulkanDynamicRHI::GetInstanceLayersAndExtensions(TArray<const ANSICHAR*>& OutInstanceExtensions, TArray<const ANSICHAR*>& OutInstanceLayers)
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
		Result = VulkanRHI::vkEnumerateInstanceLayerProperties(&InstanceLayerCount, nullptr);
		check(Result >= VK_SUCCESS);

		if (InstanceLayerCount > 0)
		{
			GlobalLayers.Empty(InstanceLayerCount);
			GlobalLayerProperties.AddZeroed(InstanceLayerCount);
			Result = VulkanRHI::vkEnumerateInstanceLayerProperties(&InstanceLayerCount, &GlobalLayerProperties[GlobalLayerProperties.Num() - InstanceLayerCount]);
			check(Result >= VK_SUCCESS);
		}
	}
	while (Result == VK_INCOMPLETE);

	for (int32 Index = 0; Index < GlobalLayerProperties.Num(); ++Index)
	{
		FLayerExtension* Layer = new(GlobalLayers) FLayerExtension;
		Layer->LayerProps = GlobalLayerProperties[Index];
		GetInstanceLayerExtensions(GlobalLayerProperties[Index].layerName, *Layer);
		UE_LOG(LogVulkanRHI, Display, TEXT("- Found Global Layer %s"), ANSI_TO_TCHAR(GlobalLayerProperties[Index].layerName));
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	// Verify that all required instance layers are available
	for (uint32 LayerIndex = 0; LayerIndex < ARRAY_COUNT(GRequiredLayersInstance); ++LayerIndex)
	{
		bool bValidationFound = false;
		const ANSICHAR* CurrValidationLayer = GRequiredLayersInstance[LayerIndex];
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
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan required instance layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
		}
	}

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

	if (OutInstanceExtensions.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using instance extensions"));
		for (const ANSICHAR* Extension : OutInstanceExtensions)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
		}
	}

	if (OutInstanceLayers.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using instance layers"));
		for (const ANSICHAR* Layer : OutInstanceLayers)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
		}
	}
}


void FVulkanDevice::GetDeviceExtensions(TArray<const ANSICHAR*>& OutDeviceExtensions, TArray<const ANSICHAR*>& OutDeviceLayers, bool& bOutDebugMarkers)
{
	bOutDebugMarkers = false;

	// Setup device layer properties
	TArray<VkLayerProperties> LayerProperties;
	{
		uint32 Count = 0;
		VERIFYVULKANRESULT(VulkanRHI::vkEnumerateDeviceLayerProperties(Gpu, &Count, nullptr));
		LayerProperties.AddZeroed(Count);
		VERIFYVULKANRESULT(VulkanRHI::vkEnumerateDeviceLayerProperties(Gpu, &Count, (VkLayerProperties*)LayerProperties.GetData()));
		check(Count == LayerProperties.Num());
	}

#if VULKAN_HAS_DEBUGGING_ENABLED
	
	bool bRenderDocFound = false;
	#if VULKAN_ENABLE_DRAW_MARKERS
		bool bDebugExtMarkerFound = false;
		for (int32 Index = 0; Index < LayerProperties.Num(); ++Index)
		{
			if (!FCStringAnsi::Strcmp(LayerProperties[Index].layerName, RENDERDOC_LAYER_NAME))
			{
				bRenderDocFound = true;
				break;
			}
			else if (!FCStringAnsi::Strcmp(LayerProperties[Index].layerName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
			{
				bDebugExtMarkerFound = true;
				break;
			}
		}
	#endif

	// Verify that all required device layers are available
	for (uint32 LayerIndex = 0; LayerIndex < ARRAY_COUNT(GRequiredLayersDevice); ++LayerIndex)
	{
		bool bValidationFound = false;
		const ANSICHAR* CurrValidationLayer = GRequiredLayersDevice[LayerIndex];
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
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to find Vulkan required device layer '%s'"), ANSI_TO_TCHAR(CurrValidationLayer));
		}
	}

	// Verify that all requested debugging device-layers are available. Skip validation layers under RenderDoc
	if (!bRenderDocFound && GValidationCvar.GetValueOnAnyThread() > 0)
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
	{
		if (bRenderDocFound)
		{
			for (int32 i = 0; i < Extensions.ExtensionProps.Num(); i++)
			{
				if (!FCStringAnsi::Strcmp(Extensions.ExtensionProps[i].extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
				{
					OutDeviceExtensions.Add(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
					bOutDebugMarkers = true;
					break;
				}
			}
		}
	}
	#endif

	if (OutDeviceExtensions.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using device extensions"));
		for (const ANSICHAR* Extension : OutDeviceExtensions)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Extension));
		}
	}

	if (OutDeviceLayers.Num() > 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Using device layers"));
		for (const ANSICHAR* Layer : OutDeviceLayers)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("* %s"), ANSI_TO_TCHAR(Layer));
		}
	}
}
