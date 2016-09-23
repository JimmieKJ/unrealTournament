// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanConfiguration.h: Vulkan resource RHI definitions.
=============================================================================*/

// Compiled with 1.0.17.0

#pragma once

#include "RHIDefinitions.h"

// API version we want to target.
#if PLATFORM_WINDOWS
	#define UE_VK_API_VERSION	VK_MAKE_VERSION(1, 0, 1)
#elif PLATFORM_ANDROID
	#define UE_VK_API_VERSION	VK_MAKE_VERSION(1, 0, 1)
#else
	#error Unsupported platform!
#endif

#if UE_BUILD_DEBUG || PLATFORM_WINDOWS
	#define VULKAN_HAS_DEBUGGING_ENABLED 1 //!!!
#else
	#define VULKAN_HAS_DEBUGGING_ENABLED 0
#endif

// constants we probably will change a few times
#define VULKAN_UB_RING_BUFFER_SIZE								(8 * 1024 * 1024)
#define VULKAN_TEMP_FRAME_ALLOCATOR_SIZE						(8 * 1024 * 1024)

enum class EDescriptorSetStage
{
	// Adjusting these requires a full shader rebuild
	Vertex		= 0,
	Pixel		= 1,
	Compute		= 2,
	Geometry	= 3,

	MaxMobileSets	= 4,

	//#todo-rco: Some devices only have 4 descriptor sets max...
	Hull		= 4,
	Domain		= 5,

	Invalid		= -1,
};

inline EDescriptorSetStage GetDescriptorSetForStage(EShaderFrequency Stage)
{
	switch (Stage)
	{
	case SF_Vertex:		return EDescriptorSetStage::Vertex;
	case SF_Hull:		return EDescriptorSetStage::Hull;
	case SF_Domain:		return EDescriptorSetStage::Domain;
	case SF_Pixel:		return EDescriptorSetStage::Pixel;
	case SF_Geometry:	return EDescriptorSetStage::Geometry;
	case SF_Compute:	return EDescriptorSetStage::Compute;
	default:
		checkf(0, TEXT("Invalid shader Stage %d"), Stage);
		break;
	}

	return EDescriptorSetStage::Invalid;
}

// Enables the VK_LAYER_LUNARG_api_dump layer and the report VK_DEBUG_REPORT_INFORMATION_BIT_EXT flag
#define VULKAN_ENABLE_API_DUMP									0
// Enables logging wrappers per Vulkan call
#define VULKAN_ENABLE_DUMP_LAYER								0
#define VULKAN_ENABLE_DRAW_MARKERS								PLATFORM_WINDOWS
#define VULKAN_ALLOW_MIDPASS_CLEAR								0

#define VULKAN_SINGLE_ALLOCATION_PER_RESOURCE					0

#define VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED					0
	

// This disables/overrides some if the graphics pipeline setup
// Please remove this after we are done with testing
#if PLATFORM_WINDOWS
	#define VULKAN_DISABLE_DEBUG_CALLBACK						0	/* Disable the DebugReportFunction() callback in VulkanDebug.cpp */
	#define VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS					0	/* 1 = use resolve attachments, 0 = Use a command buffer vkResolveImage for MSAA resolve */
	#define VULKAN_USE_RING_BUFFER_FOR_GLOBAL_UBS				1
#else
	#define VULKAN_DISABLE_DEBUG_CALLBACK						1	/* Disable the DebugReportFunction() callback in VulkanDebug.cpp */
	#define VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS					1
	#define VULKAN_USE_RING_BUFFER_FOR_GLOBAL_UBS				1
#endif

#define VULKAN_ENABLE_AGGRESSIVE_STATS							1

#define VULKAN_ENABLE_PIPELINE_CACHE							1

#define VULKAN_ENABLE_RHI_DEBUGGING								1

//#todo-rco: While validation is not fixed...
#define VULKAN_REUSE_FENCES										(VK_HEADER_VERSION < 17)

#if PLATFORM_ANDROID
	#define VULKAN_SIGNAL_UNIMPLEMENTED()
#else
	#define VULKAN_SIGNAL_UNIMPLEMENTED()				checkf(false, TEXT("Unimplemented vulkan functionality: %s"), TEXT(__FUNCTION__))
#endif

#if VULKAN_HAS_DEBUGGING_ENABLED
#else
	// Ensures all debug related defines are disabled
	#ifdef VULKAN_DISABLE_DEBUG_CALLBACK
		#undef VULKAN_DISABLE_DEBUG_CALLBACK
		#define VULKAN_DISABLE_DEBUG_CALLBACK 0
	#endif
#endif
