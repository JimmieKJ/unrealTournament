// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RHI.cpp: Unreal D3D RHI library implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RHIStaticStates.h"

#ifndef WITH_DX_PERF
#define WITH_DX_PERF	1
#endif

#if WITH_DX_PERF
// For perf events
#include "AllowWindowsPlatformTypes.h"
#include "d3d9.h"
#include "HideWindowsPlatformTypes.h"
#endif	//WITH_DX_PERF
#include "OneColorShader.h"

#if !UE_BUILD_SHIPPING
#include "STaskGraph.h"
#endif

DEFINE_LOG_CATEGORY(LogD3D12RHI);

int32 GEnableMultiEngine = 1;
static FAutoConsoleVariableRef CVarEnableMultiEngine(
	TEXT("D3D12.EnableMultiEngine"),
	GEnableMultiEngine,
	TEXT("Enables multi engine (3D, Copy, Compute) use."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

int32 GCommandListBatchingMode = CLB_NormalBatching;
static FAutoConsoleVariableRef CVarCommandListBatchingMode(
	TEXT("D3D12.CommandListBatchingMode"),
	GCommandListBatchingMode,
	TEXT("Changes how command lists are batched and submitted to the GPU."),
	ECVF_RenderThreadSafe
	);

namespace D3D12RHI
{
	extern void UniformBufferBeginFrame();
}
using namespace D3D12RHI;

FD3D12CommandListManager& FD3D12CommandContext::GetCommandListManager()
{
	return bIsAsyncComputeContext ? GetParentDevice()->GetAsyncCommandListManager() : GetParentDevice()->GetCommandListManager();
}

void FD3D12CommandContext::ConditionalObtainCommandAllocator()
{
	if (CommandAllocator == nullptr)
	{
		// Obtain a command allocator if the context doesn't already have one.
		// This will check necessary fence values to ensure the returned command allocator isn't being used by the GPU, then reset it.
		CommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
	}
}

void FD3D12CommandContext::ReleaseCommandAllocator()
{
	if (CommandAllocator != nullptr)
	{
		// Release the command allocator so it can be reused.
		CommandAllocatorManager.ReleaseCommandAllocator(CommandAllocator);
		CommandAllocator = nullptr;
	}
}

void FD3D12CommandContext::OpenCommandList(bool bRestoreState)
{
	FD3D12CommandListManager& CommandListManager = GetCommandListManager();

	// Conditionally get a new command allocator.
	// Each command context uses a new allocator for all command lists with a single frame.
	ConditionalObtainCommandAllocator();

	// Get a new command list
	CommandListHandle = CommandListManager.ObtainCommandList(*CommandAllocator);

	CommandListHandle->SetDescriptorHeaps(DescriptorHeaps.Num(), DescriptorHeaps.GetData());
	StateCache.ForceSetGraphicsRootSignature();
	StateCache.ForceSetComputeRootSignature();

	StateCache.GetDescriptorCache()->NotifyCurrentCommandList(CommandListHandle);

	if (bRestoreState)
	{
		// The next time ApplyState is called, it will set all state on this new command list
		StateCache.RestoreState();
	}

	numDraws = 0;
	numDispatches = 0;
	numClears = 0;
	numBarriers = 0;
	numCopies = 0;
	otherWorkCounter = 0;
	CommandListHandle.SetCurrentOwningContext(this);
}

void FD3D12CommandContext::CloseCommandList()
{
	CommandListHandle.Close();
}

void FD3D12CommandContext::ExecuteCommandList(bool WaitForCompletion)
{
	check(CommandListHandle.IsClosed());

	// Only submit a command list if it does meaningful work or is expected to wait for completion.
	if (WaitForCompletion || HasDoneWork())
	{
		CommandListHandle.Execute(WaitForCompletion);
	}
}

FD3D12CommandListHandle FD3D12CommandContext::FlushCommands(bool WaitForCompletion)
{
	// We should only be flushing the default context
	check(IsDefaultContext());

	FD3D12Device* Device = GetParentDevice();
	const bool bExecutePendingWork = GCommandListBatchingMode == CLB_AggressiveBatching || GetParentDevice()->IsGPUIdle();
	const bool bHasPendingWork = bExecutePendingWork && (Device->PendingCommandListsTotalWorkCommands > 0);
	const bool bHasDoneWork = HasDoneWork() || bHasPendingWork;

	// Only submit a command list if it does meaningful work or the flush is expected to wait for completion.
	if (WaitForCompletion || bHasDoneWork)
	{
		// Close the current command list
		CloseCommandList();

		if (bHasPendingWork)
		{
			// Submit all pending command lists and the current command list
			Device->PendingCommandLists.Add(CommandListHandle);
			GetCommandListManager().ExecuteCommandLists(Device->PendingCommandLists, WaitForCompletion);
			Device->PendingCommandLists.Reset();
			Device->PendingCommandListsTotalWorkCommands = 0;
		}
		else
		{
			// Just submit the current command list
			CommandListHandle.Execute(WaitForCompletion);
		}

		// Get a new command list to replace the one we submitted for execution. 
		// Restore the state from the previous command list.
		OpenCommandList(true);
	}

	return CommandListHandle;
}

void FD3D12CommandContext::Finish(TArray<FD3D12CommandListHandle>& CommandLists)
{
	CloseCommandList();

	if (HasDoneWork())
	{
		CommandLists.Add(CommandListHandle);
	}
	else
	{
		// Release the unused command list.
		GetCommandListManager().ReleaseCommandList(CommandListHandle);
	}

	// The context is done with this command list handle.
	CommandListHandle = nullptr;
}

void FD3D12CommandContext::RHIBeginFrame()
{
	check(IsDefaultContext());
	RHIPrivateBeginFrame();

	FD3D12Device* Device = GetParentDevice();
	FD3D12GlobalOnlineHeap& SamplerHeap = Device->GetGlobalSamplerHeap();
	const uint32 NumContexts = Device->GetNumContexts();

	if (SamplerHeap.DescriptorTablesDirty())
	{
		//Rearrange the set for better look-up performance
		SamplerHeap.GetUniqueDescriptorTables().Compact();
	}

	for (uint32 i = 0; i < NumContexts; ++i)
	{
		Device->GetCommandContext(i).StateCache.GetDescriptorCache()->BeginFrame();
	}

	Device->GetGlobalSamplerHeap().ToggleDescriptorTablesDirtyFlag(false);

	UniformBufferBeginFrame();
	OwningRHI.GPUProfilingData.BeginFrame(&OwningRHI);
}

void FD3D12CommandContext::ClearState()
{
	StateCache.ClearState();

	bDiscardSharedConstants = false;

	for (int32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
	{
		DirtyUniformBuffers[Frequency] = 0;
		for (int32 Index = 0; Index < MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE; ++Index)
		{
			BoundUniformBuffers[Frequency][Index] = nullptr;
		}
	}

	for (int32 Index = 0; Index < _countof(CurrentUAVs); ++Index)
	{
		CurrentUAVs[Index] = nullptr;
	}
	NumUAVs = 0;

	if (!bIsAsyncComputeContext)
	{
		for (int32 Index = 0; Index < _countof(CurrentRenderTargets); ++Index)
		{
			CurrentRenderTargets[Index] = nullptr;
		}
		NumSimultaneousRenderTargets = 0;

		CurrentDepthStencilTarget = nullptr;
		CurrentDepthTexture = nullptr;

		CurrentDSVAccessType = FExclusiveDepthStencil::DepthWrite_StencilWrite;

		bUsingTessellation = false;

		CurrentBoundShaderState = nullptr;
	}

	// MSFT: Need to do anything with the constant buffers?
	/*
	for (int32 BufferIndex = 0; BufferIndex < VSConstantBuffers.Num(); BufferIndex++)
	{
	VSConstantBuffers[BufferIndex]->ClearConstantBuffer();
	}
	for (int32 BufferIndex = 0; BufferIndex < PSConstantBuffers.Num(); BufferIndex++)
	{
	PSConstantBuffers[BufferIndex]->ClearConstantBuffer();
	}
	for (int32 BufferIndex = 0; BufferIndex < HSConstantBuffers.Num(); BufferIndex++)
	{
	HSConstantBuffers[BufferIndex]->ClearConstantBuffer();
	}
	for (int32 BufferIndex = 0; BufferIndex < DSConstantBuffers.Num(); BufferIndex++)
	{
	DSConstantBuffers[BufferIndex]->ClearConstantBuffer();
	}
	for (int32 BufferIndex = 0; BufferIndex < GSConstantBuffers.Num(); BufferIndex++)
	{
	GSConstantBuffers[BufferIndex]->ClearConstantBuffer();
	}
	for (int32 BufferIndex = 0; BufferIndex < CSConstantBuffers.Num(); BufferIndex++)
	{
		CSConstantBuffers[BufferIndex]->ClearConstantBuffer();
	}
	*/

	CurrentComputeShader = nullptr;
}

void FD3D12CommandContext::CheckIfSRVIsResolved(FD3D12ShaderResourceView* SRV)
{
	// MSFT: Seb: TODO: Make this work on 12
#if CHECK_SRV_TRANSITIONS
	if (GRHIThread || !SRV)
	{
		return;
	}

	static const auto CheckSRVCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.CheckSRVTransitions"));
	if (!CheckSRVCVar->GetValueOnRenderThread())
	{
		return;
	}

	ID3D11Resource* SRVResource = nullptr;
	SRV->GetResource(&SRVResource);

	int32 MipLevel;
	int32 NumMips;
	int32 ArraySlice;
	int32 NumSlices;
	GetMipAndSliceInfoFromSRV(SRV, MipLevel, NumMips, ArraySlice, NumSlices);

	//d3d uses -1 to mean 'all mips'
	int32 LastMip = MipLevel + NumMips - 1;
	int32 LastSlice = ArraySlice + NumSlices - 1;

	TArray<FUnresolvedRTInfo> RTInfoArray;
	UnresolvedTargets.MultiFind(SRVResource, RTInfoArray);

	for (int32 InfoIndex = 0; InfoIndex < RTInfoArray.Num(); ++InfoIndex)
	{
		const FUnresolvedRTInfo& RTInfo = RTInfoArray[InfoIndex];
		int32 RTLastMip = RTInfo.MipLevel + RTInfo.NumMips - 1;
		ensureMsgf((MipLevel == -1 || NumMips == -1) || (LastMip < RTInfo.MipLevel || MipLevel > RTLastMip), TEXT("SRV is set to read mips in range %i to %i.  Target %s is unresolved for mip %i"), MipLevel, LastMip, *RTInfo.ResourceName.ToString(), RTInfo.MipLevel);
		ensureMsgf(NumMips != -1, TEXT("SRV is set to read all mips.  Target %s is unresolved for mip %i"), *RTInfo.ResourceName.ToString(), RTInfo.MipLevel);

		int32 RTLastSlice = RTInfo.ArraySlice + RTInfo.ArraySize - 1;
		ensureMsgf((ArraySlice == -1 || LastSlice == -1) || (LastSlice < RTInfo.ArraySlice || ArraySlice > RTLastSlice), TEXT("SRV is set to read slices in range %i to %i.  Target %s is unresolved for mip %i"), ArraySlice, LastSlice, *RTInfo.ResourceName.ToString(), RTInfo.ArraySlice);
		ensureMsgf(ArraySlice == -1 || NumSlices != -1, TEXT("SRV is set to read all slices.  Target %s is unresolved for slice %i"));
	}
	SRVResource->Release();
#endif
}

void FD3D12CommandContext::ConditionalClearShaderResource(FD3D12ResourceLocation* Resource)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ClearShaderResourceTime);
	check(Resource);
	ClearShaderResourceViews<SF_Vertex>(Resource);
	ClearShaderResourceViews<SF_Hull>(Resource);
	ClearShaderResourceViews<SF_Domain>(Resource);
	ClearShaderResourceViews<SF_Pixel>(Resource);
	ClearShaderResourceViews<SF_Geometry>(Resource);
	ClearShaderResourceViews<SF_Compute>(Resource);
}

void FD3D12CommandContext::ClearAllShaderResources()
{
	StateCache.ClearSRVs();
}

void FD3D12DynamicRHI::IssueLongGPUTask()
{
	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		int32 LargestViewportIndex = INDEX_NONE;
		int32 LargestViewportPixels = 0;

		for (int32 ViewportIndex = 0; ViewportIndex < GetRHIDevice()->GetViewports().Num(); ViewportIndex++)
		{
			FD3D12Viewport* Viewport = GetRHIDevice()->GetViewports()[ViewportIndex];

			if (Viewport->GetSizeXY().X * Viewport->GetSizeXY().Y > LargestViewportPixels)
			{
				LargestViewportPixels = Viewport->GetSizeXY().X * Viewport->GetSizeXY().Y;
				LargestViewportIndex = ViewportIndex;
			}
		}

		if (LargestViewportIndex >= 0)
		{
			FD3D12Viewport* Viewport = GetRHIDevice()->GetViewports()[LargestViewportIndex];

			FRHICommandList_RecursiveHazardous RHICmdList(RHIGetDefaultContext());

			SetRenderTarget(RHICmdList, Viewport->GetBackBuffer(), FTextureRHIRef());
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One>::GetRHI(), FLinearColor::Black);
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI(), 0);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

			auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
			TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);

			RHICmdList.SetLocalBoundShaderState(RHICmdList.BuildLocalBoundShaderState(GD3D12Vector4VertexDeclaration.VertexDeclarationRHI, VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), PixelShader->GetPixelShader(), FGeometryShaderRHIRef()));

			// Draw a fullscreen quad
			FVector4 Vertices[4];
			Vertices[0].Set(-1.0f, 1.0f, 0, 1.0f);
			Vertices[1].Set(1.0f, 1.0f, 0, 1.0f);
			Vertices[2].Set(-1.0f, -1.0f, 0, 1.0f);
			Vertices[3].Set(1.0f, -1.0f, 0, 1.0f);
			DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));

			// Implicit flush. Always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.
		}
	}
}

namespace D3D12RHI
{
	void FD3DGPUProfiler::BeginFrame(FD3D12DynamicRHI* InRHI)
	{
		CurrentEventNode = NULL;
		check(!bTrackingEvents);
		check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

		// latch the bools from the game thread into our private copy
		bLatchedGProfilingGPU = GTriggerGPUProfile;
		bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile;
		if (bLatchedGProfilingGPUHitches)
		{
			bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
		}

		if (bLatchedGProfilingGPU)
		{
			// Issue a bunch of GPU work at the beginning of the frame, to make sure that we are GPU bound
			// We can't isolate idle time from GPU timestamps
			InRHI->IssueLongGPUTask();
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
				CurrentEventNodeFrame = new FD3D12EventNodeFrame(GetParentDevice());
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

		// Skip timing events when using SLI, they will not be accurate anyway
		if (GNumActiveGPUsForRendering == 1)
		{
			FrameTiming.StartTiming();
		}

		if (GEmitDrawEvents)
		{
			PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
		}
	}
}

void FD3D12CommandContext::RHIEndFrame()
{
	check(IsDefaultContext());
	OwningRHI.GPUProfilingData.EndFrame();

	GetParentDevice()->FirstFrameSeen = true;
}

void FD3DGPUProfiler::EndFrame()
{
	if (GEmitDrawEvents)
	{
		PopEvent();
	}

	// Skip timing events when using SLI, they will not be accurate anyway
	if (GNumActiveGPUsForRendering == 1)
	{
		FrameTiming.EndTiming();
	}

	// Skip timing events when using SLI, as they will block the GPU and we want maximum throughput
	// Stat unit GPU time is not accurate anyway with SLI
	if (FrameTiming.IsSupported() && GNumActiveGPUsForRendering == 1)
	{
		uint64 GPUTiming = FrameTiming.GetTiming();
		uint64 GPUFreq = FrameTiming.GetTimingFrequency();
		GGPUFrameTime = FMath::TruncToInt(double(GPUTiming) / double(GPUFreq) / FPlatformTime::GetSecondsPerCycle());
	}
	else
	{
		GGPUFrameTime = 0;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}

	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	check(!bTrackingEvents || CurrentEventNodeFrame);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			GEmitDrawEvents = bOriginalGEmitDrawEvents;
			UE_LOG(LogD3D12RHI, Warning, TEXT(""));
			UE_LOG(LogD3D12RHI, Warning, TEXT(""));
			CurrentEventNodeFrame->DumpEventTree();
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;

			if (RHIConfig::ShouldSaveScreenshotAfterProfilingGPU()
				&& GEngine->GameViewport)
			{
				GEngine->GameViewport->Exec(NULL, TEXT("SCREENSHOT"), *GLog);
			}
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		//@todo this really detects any hitch, even one on the game thread.
		// it would be nice to restrict the test to stalls on D3D, but for now...
		// this needs to be out here because bTrackingEvents is false during the hitch debounce
		static double LastTime = -1.0;
		double Now = FPlatformTime::Seconds();
		if (bTrackingEvents)
		{
			/** How long, in seconds a frame much be to be considered a hitch **/
			const float HitchThreshold = RHIConfig::GetGPUHitchThreshold();
			float ThisTime = Now - LastTime;
			bool bHitched = (ThisTime > HitchThreshold) && LastTime > 0.0 && CurrentEventNodeFrame;
			if (bHitched)
			{
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** Hitch detected on CPU, frametime = %6.1fms"), ThisTime * 1000.0f);
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));

				for (int32 Frame = 0; Frame < GPUHitchEventNodeFrames.Num(); Frame++)
				{
					UE_LOG(LogD3D12RHI, Warning, TEXT(""));
					UE_LOG(LogD3D12RHI, Warning, TEXT(""));
					UE_LOG(LogD3D12RHI, Warning, TEXT("********** GPU Frame: Current - %d"), GPUHitchEventNodeFrames.Num() - Frame);
					GPUHitchEventNodeFrames[Frame].DumpEventTree();
				}
				UE_LOG(LogD3D12RHI, Warning, TEXT(""));
				UE_LOG(LogD3D12RHI, Warning, TEXT(""));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** GPU Frame: Current"));
				CurrentEventNodeFrame->DumpEventTree();

				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** End Hitch GPU Profile"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				if (GEngine->GameViewport)
				{
					GEngine->GameViewport->Exec(NULL, TEXT("SCREENSHOT"), *GLog);
				}

				GPUHitchDebounce = 5; // don't trigger this again for a while
				GPUHitchEventNodeFrames.Empty(); // clear history
			}
			else if (CurrentEventNodeFrame) // this will be null for discarded frames while recovering from a recent hitch
			{
				/** How many old frames to buffer for hitch reports **/
				static const int32 HitchHistorySize = 4;

				if (GPUHitchEventNodeFrames.Num() >= HitchHistorySize)
				{
					GPUHitchEventNodeFrames.RemoveAt(0);
				}
				GPUHitchEventNodeFrames.Add((FD3D12EventNodeFrame*)CurrentEventNodeFrame);
				CurrentEventNodeFrame = NULL;  // prevent deletion of this below; ke kept it in the history
			}
		}
		LastTime = Now;
	}
	bTrackingEvents = false;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;
}

float FD3D12EventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		// Get the timing result and block the CPU until it is ready
		const uint64 GPUTiming = Timing.GetTiming(true);
		const uint64 GPUFreq = Timing.GetTimingFrequency();

		Result = double(GPUTiming) / double(GPUFreq);
	}

	return Result;
}

void FD3D12CommandContext::RHIBeginScene()
{
	check(IsDefaultContext());
	FD3D12DynamicRHI& RHI = OwningRHI;
	// Increment the frame counter. INDEX_NONE is a special value meaning "uninitialized", so if
	// we hit it just wrap around to zero.
	OwningRHI.SceneFrameCounter++;
	if (OwningRHI.SceneFrameCounter == INDEX_NONE)
	{
		OwningRHI.SceneFrameCounter++;
	}

	static auto* ResourceTableCachingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("rhi.ResourceTableCaching"));
	if (ResourceTableCachingCvar == NULL || ResourceTableCachingCvar->GetValueOnAnyThread() == 1)
	{
		OwningRHI.ResourceTableFrameCounter = OwningRHI.SceneFrameCounter;
	}
}

void FD3D12CommandContext::RHIEndScene()
{
	OwningRHI.ResourceTableFrameCounter = INDEX_NONE;
}

void FD3DGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
#if WITH_DX_PERF
	D3DPERF_BeginEvent(Color.DWColor(), Name);
#endif

	FGPUProfiler::PushEvent(Name, Color);
}

void FD3DGPUProfiler::PopEvent()
{
#if WITH_DX_PERF
	D3DPERF_EndEvent();
#endif

	FGPUProfiler::PopEvent();
}

/** Start this frame of per tracking */
void FD3D12EventNodeFrame::StartFrame()
{
	EventTree.Reset();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FD3D12EventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
}

float FD3D12EventNodeFrame::GetRootTimingResults()
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

void FD3D12EventNodeFrame::LogDisjointQuery()
{
}

void UpdateBufferStats(FD3D12ResourceLocation* ResourceLocation, bool bAllocating, uint32 BufferType)
{
	uint64 RequestedSize = ResourceLocation->GetEffectiveBufferSize();

	const bool bUniformBuffer = BufferType == D3D12_BUFFER_TYPE_CONSTANT;
	const bool bIndexBuffer = BufferType == D3D12_BUFFER_TYPE_INDEX;
	const bool bVertexBuffer = BufferType == D3D12_BUFFER_TYPE_VERTEX;

	if (bAllocating)
	{
		if (bUniformBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_UniformBufferMemory, RequestedSize);
		}
		else if (bIndexBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_IndexBufferMemory, RequestedSize);
		}
		else if (bVertexBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_VertexBufferMemory, RequestedSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_StructuredBufferMemory, RequestedSize);
		}
	}
	else
	{
		if (bUniformBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_UniformBufferMemory, RequestedSize);
		}
		else if (bIndexBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_IndexBufferMemory, RequestedSize);
		}
		else if (bVertexBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_VertexBufferMemory, RequestedSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_StructuredBufferMemory, RequestedSize);
		}
	}
}

FD3D12ResourceLocation::~FD3D12ResourceLocation()
{
	InternalReleaseResource();
}

void FD3D12ResourceLocation::InternalReleaseResource()
{
	if (IsSubAllocatedFastAlloc)
	{
		Resource = nullptr;
		return;
	}

	if (BlockInfo != nullptr)
	{
		FD3D12ResourceAllocator* Allocator = BlockInfo->Allocator;
		check(!!Allocator);

		Allocator->Deallocate(BlockInfo);
		BlockInfo = nullptr;
		Resource = nullptr;
	}
	else
	{
		FD3D12Resource* pResource = Resource.GetReference();
		if (pResource)
		{
			GetParentDevice()->GetDeferredDeletionQueue().EnqueueResource(pResource);

			Resource = nullptr;
		}
	}
}

void FD3D12ResourceLocation::SetFromD3DResource(FD3D12Resource* InResource, uint64 InOffset, uint32 InEffectiveBufferSize)
{
	InternalReleaseResource();

	EffectiveBufferSize = InEffectiveBufferSize;
	Resource = InResource;
	Offset = InOffset;

	UpdateGPUVirtualAddress();
}

void FD3D12ResourceLocation::Clear()
{
	InternalReleaseResource();

	ClearNoUpdate();
}

void FD3D12ResourceLocation::ClearNoUpdate()
{
	EffectiveBufferSize = 0;
	Resource = nullptr;
	Offset = 0;

	UpdateGPUVirtualAddress();
}

void FD3D12ResourceLocation::UpdateStateCache(FD3D12StateCache& StateCache)
{

	// Rename SRV?
	if (!StateCache.GetForceSetSRVs() && StateCache.IsShaderResource(this))
	{
		StateCache.ForceSetSRVs();
	}

	// Rename vertex buffer?
	if (!StateCache.GetForceSetVB() && StateCache.IsStreamSource(this))
	{
		StateCache.ForceSetVB();
	}

	// Rename index buffer?
	if (!StateCache.GetForceSetIB() && StateCache.IsIndexBuffer(this))
	{
		// If this resource matches the current index buffer then force the state 
		// cache to update the index buffer view
		StateCache.ForceSetIB();
	}
}

void FD3D12ResourceLocation::UpdateDefaultStateCache()
{
	UpdateStateCache(GetParentDevice()->GetDefaultCommandContext().StateCache);
}

void FD3D12DeferredDeletionQueue::EnqueueResource(FD3D12Resource* pResource)
{
	const uint64 CurrentFrameFence = GetParentDevice()->GetCommandListManager().GetFence(EFenceType::FT_Frame).GetCurrentFence();

	pResource->AddRef();

	FencedObjectType FencedObject;
	FencedObject.Key = pResource;
	FencedObject.Value = CurrentFrameFence;
	DeferredReleaseQueue.Enqueue(FencedObject);
}

LONGLONG D3D12ShaderResourceViewSequenceNumber = 1;

#ifndef PLATFORM_IMPLEMENTS_FASTVRAMALLOCATOR
#define PLATFORM_IMPLEMENTS_FASTVRAMALLOCATOR		0
#endif

#if !PLATFORM_IMPLEMENTS_FASTVRAMALLOCATOR
FFastVRAMAllocator* FFastVRAMAllocator::GetFastVRAMAllocator()
{
	static FFastVRAMAllocator FastVRAMAllocatorSingleton;
	return &FastVRAMAllocatorSingleton;
}
#endif

void FD3D12DynamicRHI::GetLocalVideoMemoryInfo(DXGI_QUERY_VIDEO_MEMORY_INFO* LocalVideoMemoryInfo)
{
	VERIFYD3D12RESULT(MainDevice->GetAdapter3()->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, LocalVideoMemoryInfo));
}

void  FD3D12DynamicRHI::UpdateBuffer(FD3D12Resource* Dest, uint32 DestOffset, FD3D12Resource* Source, uint32 SourceOffset, uint32 NumBytes)
{
	FD3D12CommandContext& defaultContext = FD3D12DynamicRHI::GetD3DRHI()->GetRHIDevice()->GetDefaultCommandContext();
	FD3D12CommandListHandle& hCommandList = defaultContext.CommandListHandle;

	FScopeResourceBarrier ScopeResourceBarrierDest(hCommandList, Dest, Dest->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_DEST, 0);
	// Don't need to transition upload heaps

	defaultContext.numCopies++;
	hCommandList->CopyBufferRegion(Dest->GetResource(), DestOffset, Source->GetResource(), SourceOffset, NumBytes);
	hCommandList.UpdateResidency(Dest);
	hCommandList.UpdateResidency(Source);

	DEBUG_RHI_EXECUTE_COMMAND_LIST(this);
}