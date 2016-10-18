// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUtil.cpp: Vulkan Utility implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanUtil.h"
#include "VulkanPendingState.h"
#include "VulkanManager.h"
#include "VulkanContext.h"
#include "VulkanMemory.h"

static FVulkanTimestampQueryPool* GTimestampQueryPool = nullptr;

/**
 * Initializes the static variables, if necessary.
 */
void FVulkanGPUTiming::PlatformStaticInitialize(void* UserData)
{
	// Are the static variables initialized?
	check( !GAreGlobalsInitialized );

	if (GTimestampQueryPool)
	{
		GTimingFrequency = (uint64)(GTimestampQueryPool->GetTimeStampsPerSecond());
	}
}

/**
 * Initializes all Vulkan resources and if necessary, the static variables.
 */
void FVulkanGPUTiming::Initialize()
{
	StaticInitialize(nullptr, PlatformStaticInitialize);

	bIsTiming = false;

	// Now initialize the queries for this timing object.
	if ( GIsSupported )
	{
		StartTimestamp = GTimestampQueryPool->AllocateUserQuery();
		EndTimestamp = GTimestampQueryPool->AllocateUserQuery();

		//// Initialize to 0 so we can tell when the GPU has written to it (completed the query)
		//*((uint64*)StartTimestamp.GetPointer()) = 0;
		//*((uint64*)EndTimestamp.GetPointer()) = 0;
	}
}

/**
 * Releases all Vulkan resources.
 */
void FVulkanGPUTiming::Release()
{
	//FMemBlock::Free(StartTimestamp);
	//FMemBlock::Free(EndTimestamp);
}

/**
 * Start a GPU timing measurement.
 */
void FVulkanGPUTiming::StartTiming()
{
	// Issue a timestamp query for the 'start' time.
	if ( GIsSupported && !bIsTiming )
	{
		if (StartTimestamp >= 0 && EndTimestamp >= 0)
		{
#if 1//VULKAN_USE_NEW_COMMAND_BUFFERS
			check(0);
#else
			//FVulkanPendingState& State = GTimestampQueryPool->Device->GetPendingState();
			GTimestampQueryPool->WriteTimestamp(State.GetCommandBuffer(), StartTimestamp, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
#endif
		}
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FVulkanGPUTiming::EndTiming()
{
	// Issue a timestamp query for the 'end' time.
	if ( GIsSupported && bIsTiming )
	{
		if (StartTimestamp >= 0 && EndTimestamp >= 0)
		{
#if 1//VULKAN_USE_NEW_COMMAND_BUFFERS
			check(0);
#else
			//FVulkanPendingState& State = GTimestampQueryPool->Device->GetPendingState();
			GTimestampQueryPool->WriteTimestamp(State.GetCommandBuffer(), EndTimestamp, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
#endif
		}
		bIsTiming = false;
		bEndTimestampIssued = true;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FVulkanGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{
	if ( GIsSupported )
	{
		if (StartTimestamp >= 0 && EndTimestamp >= 0)
		{
			if (bGetCurrentResultsAndBlock)
			{
				GTimestampQueryPool->Device->WaitUntilIdle();

				// Block until the GPU has finished the last query
				while (!IsComplete())
				{
					FPlatformProcess::Sleep(0);
				}

				return GTimestampQueryPool->CalculateTimeFromUserQueries(StartTimestamp, EndTimestamp, true);
			}

			return GTimestampQueryPool->CalculateTimeFromUserQueries(StartTimestamp, EndTimestamp, false);
		}
	}

	return 0;
}

void FVulkanGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
	//GVulkanManager.GetImmediateContext().PushMarker(TCHAR_TO_ANSI(Name));
	FGPUProfiler::PushEvent(Name, Color);
}

void FVulkanGPUProfiler::PopEvent()
{
	if (!bCommandlistSubmitted)
	{
		//GVulkanManager.GetImmediateContext().PopMarker();
		FGPUProfiler::PopEvent();
	}
}

/** Start this frame of per tracking */
void FVulkanEventNodeFrame::StartFrame()
{
	EventTree.Reset();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FVulkanEventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
}

float FVulkanEventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming(true);
		const uint64 GPUFreq = RootEventTiming.GetTimingFrequency();

		RootResult = double(GPUTiming) / double(GPUFreq);
	}

	return (float)RootResult;
}

float FVulkanEventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		const uint64 GPUTiming = Timing.GetTiming(false);
		const uint64 GPUFreq = Timing.GetTimingFrequency();

		Result = double(GPUTiming) / double(GPUFreq);
	}

	return Result;
}


void FVulkanGPUProfiler::BeginFrame(FVulkanCommandListContext* InCmdList, FVulkanTimestampQueryPool* InTimestampQueryPool)
{
	GTimestampQueryPool = InTimestampQueryPool;

	bCommandlistSubmitted = false;
	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GTriggerGPUProfile && InTimestampQueryPool;
	bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile && InTimestampQueryPool;
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
	}

	if (bLatchedGProfilingGPU)
	{
		// Issue a bunch of GPU work at the beginning of the frame, to make sure that we are GPU bound
		// We can't isolate idle time from GPU timestamps
		//#todo-rco: Not working yet
		//InRHI->IssueLongGPUTask();
	}

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches))
	{
		bOriginalGEmitDrawEvents = GEmitDrawEvents;
	}

	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--; 
		}
		else
		{
			GEmitDrawEvents = true;  // thwart an attempt to turn this off on the game side
			bTrackingEvents = true;
			InTimestampQueryPool->ResetUserQueries();
			CurrentEventNodeFrame = new FVulkanEventNodeFrame();
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		GEmitDrawEvents = bOriginalGEmitDrawEvents;
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;

	if (GEmitDrawEvents)
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void FVulkanGPUProfiler::EndFrameBeforeSubmit()
{
	if (GEmitDrawEvents)
	{
		// Finish all open nodes
		// This is necessary because timestamps must be issued before SubmitDone(), and SubmitDone() happens in RHIEndDrawingViewport instead of RHIEndFrame
		while (CurrentEventNode)
		{
			PopEvent();
		}

		bCommandlistSubmitted = true;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}
}

void FVulkanGPUProfiler::EndFrame()
{
	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	check(!bTrackingEvents || CurrentEventNodeFrame);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			GEmitDrawEvents = bOriginalGEmitDrawEvents;
			UE_LOG(LogRHI, Warning, TEXT(""));
			UE_LOG(LogRHI, Warning, TEXT(""));
			CurrentEventNodeFrame->DumpEventTree();
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		UE_LOG(LogRHI, Warning, TEXT("GPU hitch tracking not implemented on PS4"));
	}
	bTrackingEvents = false;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;
}


#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "OneColorShader.h"
#include "VulkanRHI.h"

static FGlobalBoundShaderState LongGPUTaskBoundShaderState;

/** Vertex declaration for just one FVector4 position. */
class FVulkanVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4)));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVulkanVector4VertexDeclaration> GVulkanVector4VertexDeclaration;

void FVulkanDynamicRHI::IssueLongGPUTask()
{
	int32 LargestViewportIndex = INDEX_NONE;
	int32 LargestViewportPixels = 0;

	for (int32 ViewportIndex = 0; ViewportIndex < Viewports.Num(); ViewportIndex++)
	{
		FVulkanViewport* Viewport = Viewports[ViewportIndex];

		if (Viewport->GetSizeXY().X * Viewport->GetSizeXY().Y > LargestViewportPixels)
		{
			LargestViewportPixels = Viewport->GetSizeXY().X * Viewport->GetSizeXY().Y;
			LargestViewportIndex = ViewportIndex;
		}
	}

	if (LargestViewportIndex >= 0)
	{
		FVulkanViewport* Viewport = Viewports[LargestViewportIndex];

		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;

		FRHICommandList_RecursiveHazardous RHICmdList(&Device->GetImmediateContext());
		SetRenderTarget(RHICmdList, Viewport->GetBackBuffer(RHICmdList), FTextureRHIRef());
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One>::GetRHI(), FLinearColor::Black);
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI(), 0);
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

		TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
		TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);

		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, LongGPUTaskBoundShaderState, GVulkanVector4VertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, 0);

		// Draw a fullscreen quad
		FVector4 Vertices[4];
		Vertices[0].Set(-1.0f, 1.0f, 0, 1.0f);
		Vertices[1].Set(1.0f, 1.0f, 0, 1.0f);
		Vertices[2].Set(-1.0f, -1.0f, 0, 1.0f);
		Vertices[3].Set(1.0f, -1.0f, 0, 1.0f);
		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));

		// RHICmdList flushes on destruction
	}
}


namespace VulkanRHI
{
	VkBuffer CreateBuffer(FVulkanDevice* InDevice, VkDeviceSize Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryRequirements& OutMemoryRequirements)
	{
		VkDevice Device = InDevice->GetInstanceHandle();
		VkBuffer Buffer = VK_NULL_HANDLE;

		VkBufferCreateInfo BufferCreateInfo;
		FMemory::Memzero(BufferCreateInfo);
		BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		BufferCreateInfo.size = Size;
		BufferCreateInfo.usage = BufferUsageFlags;
		VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateBuffer(Device, &BufferCreateInfo, nullptr, &Buffer));

		VulkanRHI::vkGetBufferMemoryRequirements(Device, Buffer, &OutMemoryRequirements);

		return Buffer;
	}

	/**
	 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
	 * @param	Result - The result code to check
	 * @param	Code - The code which yielded the result.
	 * @param	VkFunction - Tested function name.
	 * @param	Filename - The filename of the source file containing Code.
	 * @param	Line - The line number of Code within Filename.
	 */
	void VerifyVulkanResult(VkResult Result, const ANSICHAR* VkFunction, const ANSICHAR* Filename, uint32 Line)
	{
		FString ErrorString;
		switch (Result)
		{
#define VKERRORCASE(x)	case x: ErrorString = TEXT(#x); break;
		VKERRORCASE(VK_NOT_READY)
		VKERRORCASE(VK_TIMEOUT)
		VKERRORCASE(VK_EVENT_SET)
		VKERRORCASE(VK_EVENT_RESET)
		VKERRORCASE(VK_INCOMPLETE)
		VKERRORCASE(VK_ERROR_OUT_OF_HOST_MEMORY)
		VKERRORCASE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
		VKERRORCASE(VK_ERROR_INITIALIZATION_FAILED)
		VKERRORCASE(VK_ERROR_DEVICE_LOST)
		VKERRORCASE(VK_ERROR_MEMORY_MAP_FAILED)
		VKERRORCASE(VK_ERROR_LAYER_NOT_PRESENT)
		VKERRORCASE(VK_ERROR_EXTENSION_NOT_PRESENT)
		VKERRORCASE(VK_ERROR_FEATURE_NOT_PRESENT)
		VKERRORCASE(VK_ERROR_INCOMPATIBLE_DRIVER)
		VKERRORCASE(VK_ERROR_TOO_MANY_OBJECTS)
		VKERRORCASE(VK_ERROR_FORMAT_NOT_SUPPORTED)
		VKERRORCASE(VK_ERROR_SURFACE_LOST_KHR)
		VKERRORCASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
		VKERRORCASE(VK_SUBOPTIMAL_KHR)
		VKERRORCASE(VK_ERROR_OUT_OF_DATE_KHR)
		VKERRORCASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
		VKERRORCASE(VK_ERROR_VALIDATION_FAILED_EXT)
#undef VKERRORCASE
		default:
			break;
		}

		UE_LOG(LogVulkanRHI, Error, TEXT("%s failed, VkResult=%d\n at %s:%u \n with error %s"),
			ANSI_TO_TCHAR(VkFunction), (int32)Result, ANSI_TO_TCHAR(Filename), Line, *ErrorString);

		//#todo-rco: Don't need this yet...
		//TerminateOnDeviceRemoved(Result);
		//TerminateOnOutOfMemory(Result, false);

		UE_LOG(LogVulkanRHI, Fatal, TEXT("%s failed, VkResult=%d\n at %s:%u \n with error %s"),
			ANSI_TO_TCHAR(VkFunction), (int32)Result, ANSI_TO_TCHAR(Filename), Line, *ErrorString);
	}
}

DEFINE_STAT(STAT_VulkanDrawCallTime);
DEFINE_STAT(STAT_VulkanDrawCallPrepareTime);
DEFINE_STAT(STAT_VulkanGetOrCreatePipeline);
DEFINE_STAT(STAT_VulkanGetDescriptorSet);
DEFINE_STAT(STAT_VulkanCreateUniformBufferTime);
//DEFINE_STAT(STAT_VulkanCreatePipeline);
DEFINE_STAT(STAT_VulkanPipelineBind);
DEFINE_STAT(STAT_VulkanNumBoundShaderState);
DEFINE_STAT(STAT_VulkanNumRenderPasses);
DEFINE_STAT(STAT_VulkanDynamicVBSize);
DEFINE_STAT(STAT_VulkanDynamicIBSize);
DEFINE_STAT(STAT_VulkanDynamicVBLockTime);
DEFINE_STAT(STAT_VulkanDynamicIBLockTime);
DEFINE_STAT(STAT_VulkanUPPrepTime);
DEFINE_STAT(STAT_VulkanUniformBufferCreateTime);
DEFINE_STAT(STAT_VulkanApplyDSUniformBuffers);
DEFINE_STAT(STAT_VulkanSRVUpdateTime);
DEFINE_STAT(STAT_VulkanDeletionQueue);
DEFINE_STAT(STAT_VulkanQueueSubmit);
DEFINE_STAT(STAT_VulkanQueuePresent);
#if VULKAN_ENABLE_AGGRESSIVE_STATS
DEFINE_STAT(STAT_VulkanApplyDSResources);
DEFINE_STAT(STAT_VulkanUpdateDescriptorSets);
DEFINE_STAT(STAT_VulkanNumUpdateDescriptors);
DEFINE_STAT(STAT_VulkanNumDescSets);
DEFINE_STAT(STAT_VulkanSetShaderParamTime);
DEFINE_STAT(STAT_VulkanSetUniformBufferTime);
DEFINE_STAT(STAT_VulkanVkUpdateDS);
DEFINE_STAT(STAT_VulkanClearDirtyDSState);
DEFINE_STAT(STAT_VulkanBindVertexStreamsTime);
#endif
