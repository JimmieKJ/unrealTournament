// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandQueue.cpp: Metal command queue wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandQueue.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandList.h"
#include "MetalProfiler.h"
#if METAL_STATISTICS
#include "MetalStatistics.h"
#include "ModuleManager.h"
#endif

#pragma mark - Public C++ Boilerplate -

FMetalCommandQueue::FMetalCommandQueue(id<MTLDevice> Device, uint32 const MaxNumCommandBuffers /* = 0 */)
: CommandQueue(nil)
#if METAL_STATISTICS
, Statistics(nullptr)
#endif
#if METAL_DEBUG_OPTIONS
, RuntimeDebuggingLevel(EMetalDebugLevelOff)
#endif
, Features(0)
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
	
	if(StatsModule && (![Device.name containsString:@"AMD"] || FParse::Param(FCommandLine::Get(),TEXT("metalstats"))))
	{
		Statistics = StatsModule->CreateMetalStatistics(CommandQueue);
		if(!Statistics->SupportsStatistics())
		{
			delete Statistics;
			Statistics = nullptr;
		}
	}
#endif

#if PLATFORM_IOS
	NSOperatingSystemVersion Vers = [[NSProcessInfo processInfo] operatingSystemVersion];
	if(Vers.majorVersion >= 9)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesResourceOptions | EMetalFeaturesDepthStencilBlitOptions;

#if PLATFORM_TVOS
		if(!FParse::Param(FCommandLine::Get(),TEXT("nometalv2")) && [Device supportsFeatureSet:MTLFeatureSet_tvOS_GPUFamily1_v2])
		{
			Features |= EMetalFeaturesStencilView;
		}
#else
		if ([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1])
		{
			Features |= EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer;
		}
		
		if(!FParse::Param(FCommandLine::Get(),TEXT("nometalv2")) && ([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v3] || [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v3]))
		{
			Features |= EMetalFeaturesStencilView;
		}
#endif
	}
	else if(Vers.majorVersion == 8 && Vers.minorVersion >= 3)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset;
	}
#else // Assume that Mac & other platforms all support these from the start. They can diverge later.
	Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesDepthClipMode | EMetalFeaturesResourceOptions | EMetalFeaturesDepthStencilBlitOptions | EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer | EMetalFeaturesLayeredRendering;
    if (!FParse::Param(FCommandLine::Get(),TEXT("nometalv2")) && [Device supportsFeatureSet:MTLFeatureSet_OSX_GPUFamily1_v2])
    {
        Features |= EMetalFeaturesStencilView | EMetalFeaturesDepth16;
        
        // Assume that set*Bytes only works on macOS Sierra and above as no-one has tested it anywhere else.
        Features |= EMetalFeaturesSetBytes;
    }
    else if ([Device.name rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location != NSNotFound)
    {
    	Features |= EMetalFeaturesSetBytes;
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
	@autoreleasepool
	{
		id<MTLCommandBuffer> CmdBuffer = [[CommandQueue commandBuffer] retain];
#if METAL_DEBUG_OPTIONS
		if (RuntimeDebuggingLevel >= EMetalDebugLevelLogDebugGroups)
		{
			CmdBuffer = [[FMetalDebugCommandBuffer alloc] initWithCommandBuffer:CmdBuffer];
		}
#endif
		INC_DWORD_STAT(STAT_MetalCommandBufferCreatedPerFrame);
		TRACK_OBJECT(STAT_MetalCommandBufferCount, CmdBuffer);
		return CmdBuffer;
	}
}

id<MTLCommandBuffer> FMetalCommandQueue::CreateUnretainedCommandBuffer(void)
{
	static bool bUnretainedRefs = !FParse::Param(FCommandLine::Get(),TEXT("metalretainrefs"));
	id<MTLCommandBuffer> CmdBuffer = nil;
	@autoreleasepool
	{
		CmdBuffer = bUnretainedRefs ? [[CommandQueue commandBufferWithUnretainedReferences] retain] : [[CommandQueue commandBuffer] retain];
#if METAL_DEBUG_OPTIONS
		if (RuntimeDebuggingLevel >= EMetalDebugLevelLogDebugGroups)
		{
			CmdBuffer = [[FMetalDebugCommandBuffer alloc] initWithCommandBuffer:CmdBuffer];
		}
#endif
	}
	INC_DWORD_STAT(STAT_MetalCommandBufferCreatedPerFrame);
	TRACK_OBJECT(STAT_MetalCommandBufferCount, CmdBuffer);
	return CmdBuffer;
}

void FMetalCommandQueue::CommitCommandBuffer(id<MTLCommandBuffer> const CommandBuffer)
{
	check(CommandBuffer);
	UNTRACK_OBJECT(STAT_MetalCommandBufferCount, CommandBuffer);
	
	INC_DWORD_STAT(STAT_MetalCommandBufferCommittedPerFrame);
	
	[CommandBuffer commit];
	
	// Wait for completion when debugging command-buffers.
#if METAL_DEBUG_OPTIONS
	if (RuntimeDebuggingLevel >= EMetalDebugLevelWaitForComplete)
	{
		[CommandBuffer waitUntilCompleted];
	}
#endif
	
	[CommandBuffer release];
}

void FMetalCommandQueue::SubmitCommandBuffers(NSArray<id<MTLCommandBuffer>>* BufferList, uint32 Index, uint32 Count)
{
	check(BufferList);
	CommandBuffers.SetNumZeroed(Count);
	CommandBuffers[Index] = BufferList;
	bool bComplete = true;
	for (uint32 i = 0; i < Count; i++)
	{
		bComplete &= (CommandBuffers[i] != nullptr);
	}
	if (bComplete)
	{
		GetMetalDeviceContext().SubmitCommandsHint();
		
		for (uint32 i = 0; i < Count; i++)
		{
			NSArray<id<MTLCommandBuffer>>* CmdBuffers = CommandBuffers[i];
			check(CmdBuffers);
			for (id<MTLCommandBuffer> Buffer in CmdBuffers)
			{
				check(Buffer);
				CommitCommandBuffer(Buffer);
			}
			[CommandBuffers[i] release];
			CommandBuffers[i] = nullptr;
		}
	}
}

#pragma mark - Public Command Queue Accessors -
	
id<MTLDevice> FMetalCommandQueue::GetDevice(void) const
{
	return CommandQueue.device;
}

#pragma mark - Public Debug Support -

void FMetalCommandQueue::InsertDebugCaptureBoundary(void)
{
	[CommandQueue insertDebugCaptureBoundary];
}

#if METAL_DEBUG_OPTIONS
void FMetalCommandQueue::SetRuntimeDebuggingLevel(int32 const Level)
{
	RuntimeDebuggingLevel = Level;
}

int32 FMetalCommandQueue::GetRuntimeDebuggingLevel(void) const
{
	return RuntimeDebuggingLevel;
}
#endif

#if METAL_STATISTICS
#pragma mark - Public Statistics Extensions -

IMetalStatistics* FMetalCommandQueue::GetStatistics(void)
{
	return Statistics;
}
#endif
