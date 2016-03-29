// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanConfiguration.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"

#if PLATFORM_WINDOWS
	#define UE_VK_API_VERSION	VK_MAKE_VERSION(1, 0, 3)
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
#define VULKAN_VBIB_RING_BUFFER_SIZE							(4 * 1024 * 1024)
#define VULKAN_UB_RING_BUFFER_SIZE								(8 * 1024 * 1024)

//@NOTE: VULKAN_NUM_IMAGE_BUFFERS should be smaller than VULKAN_NUM_COMMAND_BUFFERS, to make sure that we wait for the fence before we reset the cmd buffer
//@TODO: Clean up VULKAN_NUM_IMAGE_BUFFERS and VULKAN_NUM_COMMAND_BUFFERS once the Vulkan API and SDK stabilizes.
#define VULKAN_NUM_IMAGE_BUFFERS								3

//@NOTE: VULKAN_NUM_COMMAND_BUFFERS should be larger than NUM_QUEUES_IN_FLIGHT VulkanQueue.h, to make sure that we wait for the fence before we reset the cmd buffer
#define VULKAN_NUM_COMMAND_BUFFERS								(VULKAN_NUM_IMAGE_BUFFERS + 1)

enum class EDescriptorSetStage
{
	// Adjusting these requires a full shader rebuild
	Vertex		= 0,
	Pixel		= 1,
	Compute		= 2,
	Geometry	= 3,

	MaxMobileSets	= 4,

	//#todo-rco: ES2 devices only have 4 descriptor sets max...
	Hull		= 4,
	Domain		= 5,

	Invalid		= -1,
};

// VULKAN_HLSLCC_GEN_GLSL_IN_OUT_WITHOUT_STRUCTURES:
//	0:		"layout(location=%d) struct { vec4 Data; } varname;"
//	1:		"layout(location=%d) vec4 varname;"
//
//	Originally, in/outs are generated with structures, which is fine. However,
//	when the next shader stage has a compiled-out location and is first location is not starting with "0",
//	the input is getting messed up. Therefore, we generate in/outs without structures.
//
//	NOTE: Changing the flag requires a full shader-rebuild.
#define VULKAN_HLSLCC_GEN_GLSL_IN_OUT_WITHOUT_STRUCTURES		1

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

#define VULKAN_ENABLE_API_DUMP									0
#define VULKAN_ENABLE_DRAW_MARKERS								(PLATFORM_WINDOWS && (VK_API_VERSION == VK_MAKE_VERSION(1, 0, 3)))
#define VULKAN_ALLOW_MIDPASS_CLEAR								0

#define VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED					0
	

// This disables/overrides some if the graphics pipeline setup
// Please remove this after we are done with testing
#if PLATFORM_WINDOWS
	#define VULKAN_DISABLE_DEBUG_CALLBACK						0	/* Disable the DebugReportFunction() callback in VulkanDebug.cpp */
	#define VULKAN_CLEAR_SURFACE_ON_CREATE						1
	#define VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS					1	/* 1 = use resolve attachments, 0 = Use a command buffer vkResolveImage for MSAA resolve */
	#define VULKAN_USE_RING_BUFFER_FOR_GLOBAL_UBS				0	/* For some reason, using this on PC renders black - it may need lock/unlock all the time */
	#define VULKAN_STRICT_TEXTURE_FLAGS							0	/* Checks the format feature flags when determining usage & flags for surfaces */
	#define VULKAN_FORCE_WAIT_FOR_QUEUE							0	/* Ensures all work is finished inside the queue, before proceeding to the next frame. This should be only used to test for stability issues. The performance will be garbage, since we are not making use of any double/tripple buffering. */
#else
	#define VULKAN_DISABLE_DEBUG_CALLBACK						1	/* Disable the DebugReportFunction() callback in VulkanDebug.cpp */
	#define VULKAN_CLEAR_SURFACE_ON_CREATE						0
	#define VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS					1
	#define VULKAN_USE_RING_BUFFER_FOR_GLOBAL_UBS				1
	#define VULKAN_STRICT_TEXTURE_FLAGS							0
	#define VULKAN_FORCE_WAIT_FOR_QUEUE							0
#endif

#define VULKAN_ENABLE_AGGRESSIVE_STATS							1

#define VULKAN_ENABLE_PIPELINE_CACHE							1

//#todo-rco: Test on Android
#define VULKAN_USE_MEMORY_SYSTEM								PLATFORM_WINDOWS	// Enables all vkAllocateMemory/vkFreeMemory to go through one system

//#todo-rco: Test on Android
#define VULKAN_USE_RESOURCE_ALLOCATIONS							0//PLATFORM_WINDOWS	// Does not directly allocate memory for buffers, instead partitions buffer ranges without reallocating memory

//#todo-rco: Test on Android
#define VULKAN_USE_NEW_STAGING_BUFFERS							PLATFORM_WINDOWS	// Enables reusing staging buffer memory

//#todo-rco: Test on Android
#define VULKAN_USE_FENCE_MANAGER								PLATFORM_WINDOWS

#define VULKAN_ENABLE_RHI_DEBUGGING								1

#define VULKAN_IGNORE_EXTENSIONS								(PLATFORM_ANDROID && 1)

#define VULKAN_INVERT_VERTEX_SHADER_Y_AXIS						1	/* Compiles vertex shaders with inverted Y axis 'y=-y'. Note: If changed, requires full shader rebuild */

#define VULKAN_DX11_TO_GLSL_DEPTH								0
#define VULKAN_GLSL_TO_VULKAN_DEPTH								0	/* Compiles shaders with 'z=(z+w)/2' vertex output. Note: If changed, requires full shader rebuild */


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


#if !VULKAN_USE_MEMORY_SYSTEM
	#if VULKAN_USE_NEW_STAGING_BUFFERS
		#error VULKAN_USE_NEW_STAGING_BUFFERS requires VULKAN_USE_MEMORY_SYSTEM enabled!
	#endif
	#if VULKAN_USE_BUFFER_HEAPS
		#error VULKAN_USE_BUFFER_HEAPS requires VULKAN_USE_MEMORY_SYSTEM enabled!
	#endif
#endif
