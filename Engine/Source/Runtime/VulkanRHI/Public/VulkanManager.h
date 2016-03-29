// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanManager.h: Class to wrap around Vulkan API
=============================================================================*/

#pragma once 

#include "VulkanUtil.h"

namespace VulkanRHI
{
	class FManager
	{
	public:
		FVulkanGPUProfiler GPUProfilingData;
	};

	extern FManager GManager;
}
