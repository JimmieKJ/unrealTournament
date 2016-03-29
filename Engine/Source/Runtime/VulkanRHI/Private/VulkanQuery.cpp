// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanQuery.cpp: Vulkan query RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"

FVulkanRenderQuery::FVulkanRenderQuery(ERenderQueryType InQueryType)
{
}

FVulkanRenderQuery::~FVulkanRenderQuery()
{
}

void FVulkanRenderQuery::Begin()
{
}

void FVulkanRenderQuery::End()
{
}


FVulkanQueryPool::FVulkanQueryPool(FVulkanDevice* InDevice, uint32 InNumQueries, VkQueryType InQueryType) :
	QueryPool(VK_NULL_HANDLE),
	NumQueries(InNumQueries),
	QueryType(InQueryType),
	Device(InDevice)
{
	check(InDevice);

	VkQueryPoolCreateInfo PoolCreateInfo;
	FMemory::Memzero(PoolCreateInfo);
	PoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	PoolCreateInfo.queryType = QueryType;
	PoolCreateInfo.queryCount = NumQueries;

	VERIFYVULKANRESULT(vkCreateQueryPool(Device->GetInstanceHandle(), &PoolCreateInfo, nullptr, &QueryPool));
}

void FVulkanQueryPool::Destroy()
{
	vkDestroyQueryPool(Device->GetInstanceHandle(), QueryPool, nullptr);
	QueryPool = VK_NULL_HANDLE;
}


FVulkanTimestampQueryPool::FVulkanTimestampQueryPool(FVulkanDevice* InDevice) :
	FVulkanQueryPool(InDevice, TotalQueries, VK_QUERY_TYPE_TIMESTAMP),
	TimeStampsPerSeconds(0),
	SecondsPerTimestamp(0),
	UsedUserQueries(0),
	bFirst(true)
{
	// The number of nanoseconds it takes for a timestamp value to be incremented by 1 can be obtained from VkPhysicalDeviceLimits::timestampPeriod after a call to vkGetPhysicalDeviceProperties.
	double NanoSecondsPerTimestamp = Device->GetDeviceProperties().limits.timestampPeriod;
	checkf(NanoSecondsPerTimestamp > 0, TEXT("Driver said it allowed timestamps but returned invalid period %f!"), NanoSecondsPerTimestamp);
	SecondsPerTimestamp = NanoSecondsPerTimestamp / 1e9;
	TimeStampsPerSeconds = 1e9 / NanoSecondsPerTimestamp;
}

void FVulkanTimestampQueryPool::WriteStartFrame(VkCommandBuffer CmdBuffer)
{
	if (bFirst)
	{
		bFirst = false;
	}
	static_assert(StartFrame + 1 == EndFrame, "Enums required to be contiguous!");

	vkCmdResetQueryPool(CmdBuffer, QueryPool, StartFrame, TotalQueries);

	// Start Frame Timestamp
	vkCmdWriteTimestamp(CmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, QueryPool, StartFrame);
}

void FVulkanTimestampQueryPool::WriteEndFrame(VkCommandBuffer CmdBuffer)
{
	if (!bFirst)
	{
		// End Frame Timestamp
		vkCmdWriteTimestamp(CmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, QueryPool, EndFrame);
	}
}

void FVulkanTimestampQueryPool::CalculateFrameTime()
{
	uint64_t Results[2] = { 0, 0 };
	if (!bFirst)
	{
		VkDevice DeviceHandle = Device->GetInstanceHandle();
		VkResult Result;
		Result = vkGetQueryPoolResults(DeviceHandle, QueryPool, StartFrame, 2, sizeof(Results), Results, sizeof(uint64), /*VK_QUERY_RESULT_WAIT_BIT | */VK_QUERY_RESULT_64_BIT);
		if (Result != VK_SUCCESS)
		{
			GGPUFrameTime = 0;
			return;
		}
	}

	double ValueInSeconds = (double)(Results[1] - Results[0]) * SecondsPerTimestamp;
	GGPUFrameTime = (uint32)(ValueInSeconds / FPlatformTime::GetSecondsPerCycle());
}

void FVulkanTimestampQueryPool::ResetUserQueries()
{
	UsedUserQueries = 0;
}

int32 FVulkanTimestampQueryPool::AllocateUserQuery()
{
	if (UsedUserQueries < MaxNumUser)
	{
		return UsedUserQueries++;
	}

	return -1;
}

void FVulkanTimestampQueryPool::WriteTimestamp(VkCommandBuffer CmdBuffer, int32 UserQuery, VkPipelineStageFlagBits PipelineStageBits)
{
	check(UserQuery != -1);
	vkCmdWriteTimestamp(CmdBuffer, PipelineStageBits, QueryPool, StartUser + UserQuery);
}

uint32 FVulkanTimestampQueryPool::CalculateTimeFromUserQueries(int32 UserBegin, int32 UserEnd, bool bWait)
{
	check(UserBegin >= 0 && UserEnd >= 0);
	uint64_t Begin = 0;
	uint64_t End = 0;
	VkDevice DeviceHandle = Device->GetInstanceHandle();
	VkResult Result;

	Result = vkGetQueryPoolResults(DeviceHandle, QueryPool, StartUser + UserBegin, 1, sizeof(uint64_t), &Begin, 0, /*(bWait ? VK_QUERY_RESULT_WAIT_BIT : 0) |*/ VK_QUERY_RESULT_64_BIT);
	if (Result != VK_SUCCESS)
	{
		return 0;
	}

	Result = vkGetQueryPoolResults(DeviceHandle, QueryPool, StartUser + UserEnd, 1, sizeof(uint64_t), &End, 0, (bWait ? VK_QUERY_RESULT_WAIT_BIT : 0) | VK_QUERY_RESULT_64_BIT);
	if (Result != VK_SUCCESS)
	{
		return 0;
	}

	return End > Begin ? (uint32)(End - Begin) : 0;
}

FRenderQueryRHIRef FVulkanDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	return new FVulkanRenderQuery(QueryType);
}

bool FVulkanDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutNumPixels,bool bWait)
{
	check(IsInRenderingThread());

	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);

	return false;
}

void FVulkanCommandListContext::RHIBeginOcclusionQueryBatch()
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHIEndOcclusionQueryBatch()
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISubmitCommandsHint()
{
	//#todo-rco: Submit at this point if possible
}
