// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanDebug.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#if UE_VK_API_VERSION >= VK_MAKE_VERSION(1, 0, 3)
	#define VK_DEBUG_REPORT_WARN_BIT_EXT VK_DEBUG_REPORT_WARNING_BIT_EXT
	#define VK_DEBUG_REPORT_INFO_BIT_EXT VK_DEBUG_REPORT_INFORMATION_BIT_EXT
#else
	#include <vulkan/vk_ext_debug_report.h>
#endif
#define CREATE_MSG_CALLBACK							"vkCreateDebugReportCallbackEXT"
#define DESTROY_MSG_CALLBACK						"vkDestroyDebugReportCallbackEXT"

DEFINE_LOG_CATEGORY(LogVulkanRHI);

#if VULKAN_HAS_DEBUGGING_ENABLED

static VkBool32 DebugReportFunction(
	VkDebugReportFlagsEXT			msgFlags,
	VkDebugReportObjectTypeEXT		objType,
    uint64_t						srcObject,
    size_t							location,
    int32							msgCode,
	const ANSICHAR*					pLayerPrefix,
	const ANSICHAR*					pMsg,
#if (UE_VK_API_VERSION < VK_MAKE_VERSION(1, 0, 2))
 	const void*						pUserData)
#else
    void*							pUserData)
#endif
{
	if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
	{
		// Reaching this line should trigger a break/assert. 
		// Check to see if this is a code we've seen before.
		FString LayerCode = FString::Printf(TEXT("%s%d"), ANSI_TO_TCHAR(pLayerPrefix), msgCode);
		static TSet<FString> SeenCodes;
		if (!SeenCodes.Contains(LayerCode))
		{
			FString Message = FString::Printf(TEXT("VulkanRHI: VK ERROR: [%s] Code %d : %s\n"), ANSI_TO_TCHAR(pLayerPrefix), msgCode, ANSI_TO_TCHAR(pMsg));
			FPlatformMisc::LowLevelOutputDebugStringf(*Message);
    
			// Break debugger on first instance of each message. 
			// Continuing will ignore the error and suppress this message in future.
			bool bIgnoreInFuture = true;
			ensureAlways(0);
			if (bIgnoreInFuture)
			{
				SeenCodes.Add(LayerCode);
			}
		}
		return VK_FALSE;
	}
	else if (msgFlags & VK_DEBUG_REPORT_WARN_BIT_EXT)
	{
		if (FCStringAnsi::Strstr(pMsg, "in output interface has no Location or Builtin decoration"))
		{
			return VK_FALSE;
		}

		#if VULKAN_HLSLCC_GEN_GLSL_IN_OUT_WITHOUT_STRUCTURES
			if(FCStringAnsi::Strstr(pLayerPrefix, "SC") && msgCode == 3 && msgFlags == 2 && objType == VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT)
			{
				// We are aware that some of the outputs may not used or are compiled out...
				return VK_FALSE;
			}
		#endif

		FString Message = FString::Printf(TEXT("VulkanRHI: VK WARNING: [%s] Code %d : %s\n"), ANSI_TO_TCHAR(pLayerPrefix), msgCode, ANSI_TO_TCHAR(pMsg));
		FPlatformMisc::LowLevelOutputDebugString(*Message);
	}
#if VULKAN_ENABLE_API_DUMP
	else if (msgFlags & VK_DEBUG_REPORT_INFO_BIT_EXT)
	{
		FString Message = FString::Printf(TEXT("VulkanRHI: VK INFO: [%s] Code %d : %s\n"), ANSI_TO_TCHAR(pLayerPrefix), msgCode, ANSI_TO_TCHAR(pMsg));
		FPlatformMisc::LowLevelOutputDebugString(*Message);
	}
#endif

	return VK_TRUE;
}

void FVulkanDynamicRHI::SetupDebugLayerCallback()
{
#if !VULKAN_DISABLE_DEBUG_CALLBACK
	auto CreateMsgCallback = (PFN_vkCreateDebugReportCallbackEXT)(void*)vkGetInstanceProcAddr(Instance, CREATE_MSG_CALLBACK);
	if (CreateMsgCallback)
	{
		VkDebugReportCallbackCreateInfoEXT CreateInfo;
		FMemory::Memzero(CreateInfo);
		CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		CreateInfo.pfnCallback = &DebugReportFunction;
		CreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARN_BIT_EXT;
#if VULKAN_ENABLE_API_DUMP
		CreateInfo.flags |= VK_DEBUG_REPORT_INFO_BIT_EXT;
#endif
		auto Result = CreateMsgCallback(Instance, &CreateInfo, nullptr, &MsgCallback);
		switch (Result)
		{
		case VK_SUCCESS:
			break;
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			UE_LOG(LogVulkanRHI, Warning, TEXT("CreateMsgCallback: out of host memory/CreateMsgCallback Failure; debug reporting skipped"));
			break;
		default:
			UE_LOG(LogVulkanRHI, Warning, TEXT("CreateMsgCallback: unknown failure %d/CreateMsgCallback Failure; debug reporting skipped"), (int32)Result);
			break;
		}
	}
	else
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("GetProcAddr: Unable to find vkDbgCreateMsgCallback/vkGetInstanceProcAddr; debug reporting skipped!"));
	}
#endif
}

void FVulkanDynamicRHI::RemoveDebugLayerCallback()
{
#if !VULKAN_DISABLE_DEBUG_CALLBACK
	if (MsgCallback != VK_NULL_HANDLE)
	{
		auto DestroyMsgCallback = (PFN_vkDestroyDebugReportCallbackEXT)(void*)vkGetInstanceProcAddr(Instance, DESTROY_MSG_CALLBACK);
		checkf(DestroyMsgCallback, TEXT("GetProcAddr: Unable to find vkDbgCreateMsgCallback\vkGetInstanceProcAddr Failure"));
		DestroyMsgCallback(Instance, MsgCallback, nullptr);
	}
#endif
}

#endif // VULKAN_HAS_DEBUGGING_ENABLED
