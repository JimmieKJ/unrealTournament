// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandQueue.cpp: Metal command queue wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandQueue.h"
#if METAL_STATISTICS
#include "Runtime/Mac/NoRedist/MetalStatistics/Public/MetalStatistics.h"
#include "ModuleManager.h"
#endif

#pragma mark - Public C++ Boilerplate -

FMetalCommandQueue::FMetalCommandQueue(id<MTLDevice> Device, uint32 const MaxNumCommandBuffers /* = 0 */)
: CommandQueue(nil)
#if METAL_STATISTICS
, Statistics(nullptr)
#endif
{
	if(MaxNumCommandBuffers == 0)
	{
		CommandQueue = [Device newCommandQueue];
	}
	else
	{
		CommandQueue = [Device newCommandQueueWithMaxCommandBufferCount: MaxNumCommandBuffers];
	}
	check(CommandQueue);

#if METAL_STATISTICS
	IMetalStatisticsModule* StatsModule = FModuleManager::Get().LoadModulePtr<IMetalStatisticsModule>(TEXT("MetalStatistics"));
	
	if(StatsModule)
	{
		Statistics = StatsModule->CreateMetalStatistics(CommandQueue);
		if(!Statistics->SupportsStatistics())
		{
			delete Statistics;
			Statistics = nullptr;
		}
	}
#endif
}

FMetalCommandQueue::~FMetalCommandQueue(void)
{
#if METAL_STATISTICS
	delete Statistics;
#endif
	
	[CommandQueue release];
	CommandQueue = nil;
}
	
#pragma mark - Public Command Buffer Mutators -

id<MTLCommandBuffer> FMetalCommandQueue::CreateRetainedCommandBuffer(void)
{
	return [CommandQueue commandBuffer];
}

id<MTLCommandBuffer> FMetalCommandQueue::CreateUnretainedCommandBuffer(void)
{
	return [CommandQueue commandBufferWithUnretainedReferences];
}

#pragma mark - Public Command Queue Accessors -
	
id<MTLCommandQueue> FMetalCommandQueue::GetCommandQueue(void) const
{
	return CommandQueue;
}

#pragma mark - Public Debug Support -

void FMetalCommandQueue::InsertDebugCaptureBoundary(void)
{
	[CommandQueue insertDebugCaptureBoundary];
}

#if METAL_STATISTICS
#pragma mark - Public Statistics Extensions -

IMetalStatistics* FMetalCommandQueue::GetStatistics(void)
{
	return Statistics;
}
#endif
