// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Viewport.cpp: D3D viewport RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RenderCore.h"

#ifndef D3D12_WITH_DWMAPI
#if WINVER > 0x502		// Windows XP doesn't support DWM
#define D3D12_WITH_DWMAPI	0
#else
#define D3D12_WITH_DWMAPI	0
#endif
#endif

#if D3D12_WITH_DWMAPI
#include "AllowWindowsPlatformTypes.h"
#include <dwmapi.h>
#endif	//D3D12_WITH_DWMAPI

namespace D3D12RHI
{
	/**
	 * RHI console variables used by viewports.
	 */
	namespace RHIConsoleVariables
	{
		int32 bSyncWithDWM = 0;
		static FAutoConsoleVariableRef CVarSyncWithDWM(
			TEXT("D3D12.SyncWithDWM"),
			bSyncWithDWM,
			TEXT("If true, synchronize with the desktop window manager for vblank."),
			ECVF_RenderThreadSafe
			);

		float RefreshPercentageBeforePresent = 1.0f;
		static FAutoConsoleVariableRef CVarRefreshPercentageBeforePresent(
			TEXT("D3D12.RefreshPercentageBeforePresent"),
			RefreshPercentageBeforePresent,
			TEXT("The percentage of the refresh period to wait before presenting."),
			ECVF_RenderThreadSafe
			);

		int32 bForceThirtyHz = 1;
		static FAutoConsoleVariableRef CVarForceThirtyHz(
			TEXT("D3D12.ForceThirtyHz"),
			bForceThirtyHz,
			TEXT("If true, the display will never update more often than 30Hz."),
			ECVF_RenderThreadSafe
			);

		int32 SyncInterval = 1;
		static FAutoConsoleVariableRef CVarSyncInterval(
			TEXT("D3D12.SyncInterval"),
			SyncInterval,
			TEXT("When synchronizing with D3D, specifies the interval at which to refresh."),
			ECVF_RenderThreadSafe
			);

		float SyncRefreshThreshold = 1.05f;
		static FAutoConsoleVariableRef CVarSyncRefreshThreshold(
			TEXT("D3D12.SyncRefreshThreshold"),
			SyncRefreshThreshold,
			TEXT("Threshold for time above which vsync will be disabled as a percentage of the refresh rate."),
			ECVF_RenderThreadSafe
			);

		int32 MaxSyncCounter = 8;
		static FAutoConsoleVariableRef CVarMaxSyncCounter(
			TEXT("D3D12.MaxSyncCounter"),
			MaxSyncCounter,
			TEXT("Maximum sync counter to smooth out vsync transitions."),
			ECVF_RenderThreadSafe
			);

		int32 SyncThreshold = 7;
		static FAutoConsoleVariableRef CVarSyncThreshold(
			TEXT("D3D12.SyncThreshold"),
			SyncThreshold,
			TEXT("Number of consecutive 'fast' frames before vsync is enabled."),
			ECVF_RenderThreadSafe
			);

		int32 MaximumFrameLatency = 3;
		static FAutoConsoleVariableRef CVarMaximumFrameLatency(
			TEXT("D3D12.MaximumFrameLatency"),
			MaximumFrameLatency,
			TEXT("Number of frames that can be queued for render."),
			ECVF_RenderThreadSafe
			);

#if UE_BUILD_DEBUG
		int32 DumpStatsEveryNFrames = 0;
		static FAutoConsoleVariableRef CVarDumpStatsNFrames(
			TEXT("D3D12.DumpStatsEveryNFrames"),
			DumpStatsEveryNFrames,
			TEXT("Dumps D3D12 stats every N frames on Present; 0 means no information (default)."),
			ECVF_RenderThreadSafe
			);
#endif
	};

	extern void D3D12TextureAllocated2D(FD3D12Texture2D& Texture);
}
using namespace D3D12RHI;

/**
 * Creates a FD3D12Surface to represent a swap chain's back buffer.
 */
FD3D12Texture2D* GetSwapChainSurface(FD3D12Device* Parent, EPixelFormat PixelFormat, IDXGISwapChain3* SwapChain, const uint32 &backBufferIndex)
{
	// Grab the back buffer
	TRefCountPtr<ID3D12Resource> BackBufferResource;
	VERIFYD3D12RESULT_EX(SwapChain->GetBuffer(backBufferIndex, __uuidof(ID3D12Resource), (void**)BackBufferResource.GetInitReference()), Parent->GetDevice());

	D3D12_RESOURCE_DESC BackBufferDesc = BackBufferResource->GetDesc();

	const D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;
	TRefCountPtr<FD3D12Resource> BackBufferWrappedResource = new FD3D12Resource(Parent, BackBufferResource, State, BackBufferDesc);

	// create the render target view
	TRefCountPtr<FD3D12RenderTargetView> BackBufferRenderTargetView;
	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = BackBufferDesc.Format;
	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;
	BackBufferRenderTargetView = new FD3D12RenderTargetView(Parent, &RTVDesc, BackBufferWrappedResource);

	TArray<TRefCountPtr<FD3D12RenderTargetView> > RenderTargetViews;
	RenderTargetViews.Add(BackBufferRenderTargetView);

	// create a shader resource view to allow using the backbuffer as a texture
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Format = BackBufferDesc.Format;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	TRefCountPtr<FD3D12ResourceLocation> ResourceLocation = new FD3D12ResourceLocation(Parent);
	ResourceLocation->SetFromD3DResource(BackBufferWrappedResource, 0, 0);

	FD3D12Texture2D* NewTexture = new FD3D12Texture2D(
		Parent,
		ResourceLocation,
		false,
		1,
		RenderTargetViews,
		NULL,
		(uint32)BackBufferDesc.Width,
		BackBufferDesc.Height,
		1,
		1,
		1,
		PixelFormat,
		false,
		false,
		false,
		FClearValueBinding()
		);

	FD3D12ShaderResourceView* pWrappedShaderResourceView = new FD3D12ShaderResourceView(Parent, &SRVDesc, NewTexture->ResourceLocation);
	NewTexture->SetShaderResourceView(pWrappedShaderResourceView);

	D3D12TextureAllocated2D(*NewTexture);

	NewTexture->DoNoDeferDelete();
	pWrappedShaderResourceView->DoNoDeferDelete();
	BackBufferRenderTargetView->DoNoDeferDelete();

	return NewTexture;
}

FD3D12Viewport::~FD3D12Viewport()
{
	check(IsInRenderingThread());

	GetParentDevice()->GetViewports().Remove(this);
}

DXGI_MODE_DESC FD3D12Viewport::SetupDXGI_MODE_DESC() const
{
	DXGI_MODE_DESC Ret;

	Ret.Width = SizeX;
	Ret.Height = SizeY;
	Ret.RefreshRate.Numerator = 0;	// illamas: use 0 to avoid a potential mismatch with hw
	Ret.RefreshRate.Denominator = 0;	// illamas: ditto
	Ret.Format = GetRenderTargetFormat(PixelFormat);
	Ret.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	Ret.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	return Ret;
}

void FD3D12Viewport::Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen)
{
	// Unbind any dangling references to resources
	GetParentDevice()->GetDefaultCommandContext().ClearState();

	if (IsValidRef(CustomPresent))
	{
		CustomPresent->OnBackBufferResize();
	}

	// Release our backbuffer reference, as required by DXGI before calling ResizeBuffers.
	for (uint32 i = 0; i < NumBackBuffers; ++i)
	{
		if (IsValidRef(BackBuffers[i]))
		{
			check(BackBuffers[i]->GetRefCount() == 1);
			check(BackBuffers[i]->ResourceLocation->GetRefCount() == 2);
		}
		BackBuffers[i].SafeRelease();
		check(BackBuffers[i] == nullptr);
	}

	// Flush all pending deletes. The resize may not happen on the rendering thread so we need to
	// enqueue a command on the rendering thread to ensure it happens.
	ENQUEUE_UNIQUE_RENDER_COMMAND(FlushCommand,
	{
		FRHIResource::FlushPendingDeletes();
	}
	);
	FlushRenderingCommands();

	// Flush the outstanding GPU work
	GetParentDevice()->GetDefaultCommandContext().FlushCommands();

	// Indicate that we've flushed the outstanding work for the current frame and wait for the command queue to flush
	// This is needed so the deferred deletion queue empties out completely because the resources in the
	// queue use the frame fence values.
	GetParentDevice()->GetCommandListManager().SignalFrameComplete(true);

	// Flush the deferred deletion queue to bring backbuffer refcount to 0.
	// Note: Clear ensures that the queue is completely empty.
	GetParentDevice()->GetDeferredDeletionQueue().Clear();

	bool bResizeBuffers = false;
	if (SizeX != InSizeX || SizeY != InSizeY)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;

		check(SizeX > 0);
		check(SizeY > 0);

		if (bInIsFullscreen)
		{
			DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();

			if (FAILED(SwapChain3->ResizeTarget(&BufferDesc)))
			{
				ConditionalResetSwapChain(true);
			}
		}

		bResizeBuffers = true;
	}

	if (bIsFullscreen != bInIsFullscreen)
	{
		bIsFullscreen = bInIsFullscreen;
		bIsValid = false;

		// Use ConditionalResetSwapChain to call SetFullscreenState, to handle the failure case.
		// Ignore the viewport's focus state; since Resize is called as the result of a user action we assume authority without waiting for Focus.
		ConditionalResetSwapChain(true);

		bResizeBuffers = true;
	}

	if (bResizeBuffers)
	{
		DXGI_SWAP_CHAIN_FLAG swapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		// Resize the swap chain.
		VERIFYD3D12RESULT_EX(SwapChain3->ResizeBuffers(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), swapChainFlags), GetParentDevice()->GetDevice());
	}

	// Create a RHI surface to represent the viewport's back buffer.
	for (uint32 i = 0; i < NumBackBuffers; ++i)
	{
		check(BackBuffers[i].GetReference() == nullptr);
		BackBuffers[i] = GetSwapChainSurface(GetParentDevice(), PixelFormat, SwapChain3, i);
	}

	// Reset the viewport frame counter to get the correct back buffer after the resize.
	GetParentDevice()->GetOwningRHI()->ResetViewportFrameCounter();
}

/** Returns true if desktop composition is enabled. */
static bool IsCompositionEnabled()
{
	BOOL bDwmEnabled = false;
#if D3D12_WITH_DWMAPI
	DwmIsCompositionEnabled(&bDwmEnabled);
#endif	//D3D12_WITH_DWMAPI
	return !!bDwmEnabled;
}

/** Presents the swap chain checking the return result. */
bool FD3D12Viewport::PresentChecked(int32 SyncInterval)
{
	HRESULT Result = S_OK;
	bool bNeedNativePresent = true;
	if (IsValidRef(CustomPresent))
	{
		bNeedNativePresent = CustomPresent->Present(SyncInterval);
	}
	if (bNeedNativePresent)
	{
		// Present the back buffer to the viewport window.
		Result = SwapChain3->Present(SyncInterval, 0);

#if LOG_PRESENT
		UE_LOG(LogD3D12RHI, Log, TEXT("*** PRESENT: SyncInterval %u ***"), SyncInterval);
#endif

		// Signal the frame is complete.
		GetParentDevice()->GetCommandListManager().SignalFrameComplete();
	}

	// Detect a lost device.
	if (Result == DXGI_ERROR_DEVICE_REMOVED || Result == DXGI_ERROR_DEVICE_RESET || Result == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
	{
		// This variable is checked periodically by the main thread.
		GetParentDevice()->SetDeviceRemoved(true);
	}
	else
	{
		VERIFYD3D12RESULT(Result);
	}

	return bNeedNativePresent;
}

/** Blocks the CPU to synchronize with vblank by communicating with DWM. */
void FD3D12Viewport::PresentWithVsyncDWM()
{
#if D3D12_WITH_DWMAPI
	LARGE_INTEGER Cycles;
	DWM_TIMING_INFO TimingInfo;

	// Find out how long since we last flipped and query DWM for timing information.
	QueryPerformanceCounter(&Cycles);
	FMemory::Memzero(TimingInfo);
	TimingInfo.cbSize = sizeof(DWM_TIMING_INFO);
	DwmGetCompositionTimingInfo(WindowHandle, &TimingInfo);

	uint64 QpcAtFlip = Cycles.QuadPart;
	uint64 CyclesSinceLastFlip = Cycles.QuadPart - LastFlipTime;
	float CPUTime = FPlatformTime::ToMilliseconds(CyclesSinceLastFlip);
	float GPUTime = FPlatformTime::ToMilliseconds(TimingInfo.qpcFrameComplete - LastCompleteTime);
	float DisplayRefreshPeriod = FPlatformTime::ToMilliseconds(TimingInfo.qpcRefreshPeriod);

	// Find the smallest multiple of the refresh rate that is >= 33ms, our target frame rate.
	float RefreshPeriod = DisplayRefreshPeriod;
	if (RHIConsoleVariables::bForceThirtyHz && RefreshPeriod > 1.0f)
	{
		while (RefreshPeriod - (1000.0f / 30.0f) < -1.0f)
		{
			RefreshPeriod *= 2.0f;
		}
	}

	// If the last frame hasn't completed yet, we don't know how long the GPU took.
	bool bValidGPUTime = (TimingInfo.cFrameComplete > LastFrameComplete);
	if (bValidGPUTime)
	{
		GPUTime /= (float)(TimingInfo.cFrameComplete - LastFrameComplete);
	}

	// Update the sync counter depending on how much time it took to complete the previous frame.
	float FrameTime = FMath::Max<float>(CPUTime, GPUTime);
	if (FrameTime >= RHIConsoleVariables::SyncRefreshThreshold * RefreshPeriod)
	{
		SyncCounter--;
	}
	else if (bValidGPUTime)
	{
		SyncCounter++;
	}
	SyncCounter = FMath::Clamp<int32>(SyncCounter, 0, RHIConsoleVariables::MaxSyncCounter);

	// If frames are being completed quickly enough, block for vsync.
	bool bSync = (SyncCounter >= RHIConsoleVariables::SyncThreshold);
	if (bSync)
	{
		// This flushes the previous present call and blocks until it is made available to DWM.
		GetParentDevice()->GetDefaultCommandContext().FlushCommands();
		// MS: Might need to wait for the previous command list to finish

		DwmFlush();

		// We sleep a percentage of the remaining time. The trick is to get the
		// present call in after the vblank we just synced for but with time to
		// spare for the next vblank.
		float MinFrameTime = RefreshPeriod * RHIConsoleVariables::RefreshPercentageBeforePresent;
		float TimeToSleep;
		do
		{
			QueryPerformanceCounter(&Cycles);
			float TimeSinceFlip = FPlatformTime::ToMilliseconds(Cycles.QuadPart - LastFlipTime);
			TimeToSleep = (MinFrameTime - TimeSinceFlip);
			if (TimeToSleep > 0.0f)
			{
				FPlatformProcess::Sleep(TimeToSleep * 0.001f);
			}
		} while (TimeToSleep > 0.0f);
	}

	// Present.
	PresentChecked(/*SyncInterval=*/ 0);

	// If we are forcing <= 30Hz, block the CPU an additional amount of time if needed.
	// This second block is only needed when RefreshPercentageBeforePresent < 1.0.
	if (bSync)
	{
		LARGE_INTEGER LocalCycles;
		float TimeToSleep;
		bool bSaveCycles = false;
		do
		{
			QueryPerformanceCounter(&LocalCycles);
			float TimeSinceFlip = FPlatformTime::ToMilliseconds(LocalCycles.QuadPart - LastFlipTime);
			TimeToSleep = (RefreshPeriod - TimeSinceFlip);
			if (TimeToSleep > 0.0f)
			{
				bSaveCycles = true;
				FPlatformProcess::Sleep(TimeToSleep * 0.001f);
			}
		} while (TimeToSleep > 0.0f);

		if (bSaveCycles)
		{
			Cycles = LocalCycles;
		}
	}

	// If we are dropping vsync reset the counter. This provides a debounce time
	// before which we try to vsync again.
	if (!bSync && bSyncedLastFrame)
	{
		SyncCounter = 0;
	}

	if (bSync != bSyncedLastFrame || UE_LOG_ACTIVE(LogRHI, VeryVerbose))
	{
		UE_LOG(LogRHI, Verbose, TEXT("BlockForVsync[%d]: CPUTime:%.2fms GPUTime[%d]:%.2fms Blocked:%.2fms Pending/Complete:%d/%d"),
			bSync,
			CPUTime,
			bValidGPUTime,
			GPUTime,
			FPlatformTime::ToMilliseconds(Cycles.QuadPart - QpcAtFlip),
			TimingInfo.cFramePending,
			TimingInfo.cFrameComplete);
	}

	// Remember if we synced, when the frame completed, etc.
	bSyncedLastFrame = bSync;
	LastFlipTime = Cycles.QuadPart;
	LastFrameComplete = TimingInfo.cFrameComplete;
	LastCompleteTime = TimingInfo.qpcFrameComplete;
#endif	//D3D12_WITH_DWMAPI
}

bool FD3D12Viewport::Present(bool bLockToVsync)
{
	FD3D12CommandContext& DefaultContext = GetParentDevice()->GetDefaultCommandContext();
	
	bool bNativelyPresented = true;
	FD3D12DynamicRHI::TransitionResource(DefaultContext.CommandListHandle, GetBackBuffer()->GetShaderResourceView(), D3D12_RESOURCE_STATE_PRESENT);
	DefaultContext.CommandListHandle.FlushResourceBarriers();

	// Return the current command allocator to the pool, execute the current command list, and 
	// then open a new command list with a new command allocator.
	DefaultContext.ReleaseCommandAllocator();
	DefaultContext.FlushCommands();

	// Reset the default context state
	DefaultContext.ClearState();

	if (GEnableAsyncCompute)
	{
		FD3D12CommandContext& DefaultAsyncComputeContext = GetParentDevice()->GetDefaultAsyncComputeContext();
		DefaultAsyncComputeContext.ReleaseCommandAllocator();
		DefaultAsyncComputeContext.ClearState();
	}

	int32 syncInterval = bLockToVsync ? RHIConsoleVariables::SyncInterval : 0;

	bNativelyPresented = PresentChecked(syncInterval);

	return bNativelyPresented;
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FD3D12DynamicRHI::RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	check(IsInGameThread());

	// Use a default pixel format if none was specified	
	if (PreferredPixelFormat == EPixelFormat::PF_Unknown)
	{
		PreferredPixelFormat = EPixelFormat::PF_A2B10G10R10;
	}

	// MSFT: Seb: Remove the Init? Thats what XB1 does.
	FD3D12Viewport* RenderingViewport = new FD3D12Viewport(GetRHIDevice(), (HWND)WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	RenderingViewport->Init(GetFactory(), true);
	return RenderingViewport;
}

void FD3D12DynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
{
	FD3D12Viewport*  Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);

	check(IsInGameThread());
	Viewport->Resize(SizeX, SizeY, bIsFullscreen);
}

void FD3D12DynamicRHI::RHITick(float DeltaTime)
{
	check(IsInGameThread());

	// Check to see if the device has been removed.
	if (GetRHIDevice()->IsDeviceRemoved())
	{
		InitD3DDevices();
	}

	// Check if any swap chains have been invalidated.
	for (int32 ViewportIndex = 0; ViewportIndex < GetRHIDevice()->GetViewports().Num(); ViewportIndex++)
	{
		GetRHIDevice()->GetViewports()[ViewportIndex]->ConditionalResetSwapChain(false);
	}
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

//#if PLATFORM_SUPPORTS_RHI_THREAD
void FD3D12CommandContext::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTarget)
{
	FD3D12CommandContext& CmdContext = *this;
	FD3D12Device* Device = GetParentDevice();
//#else
//void FD3D12DynamicRHI::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTarget)
//{
//	FD3D12CommandContext& CmdContext = GetRHIDevice()->GetDefaultCommandContext();
//	FD3D12Device* Device = GetRHIDevice();
//#endif

	FD3D12Viewport*  Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);

	SCOPE_CYCLE_COUNTER(STAT_D3D12PresentTime);

	check(!Device->GetDrawingViewport());
	Device->SetDrawingViewport(Viewport);

	// Set the render target and viewport.
	// MSFT: Seb
	//check (RenderTarget);
	if (RenderTarget == NULL)
	{
		RenderTarget = Viewport->GetBackBuffer();
	}
	FRHIRenderTargetView View(RenderTarget);
	CmdContext.RHISetRenderTargets(1, &View, nullptr, 0, NULL);

	// Set an initially disabled scissor rect.
	CmdContext.RHISetScissorRect(false, 0, 0, 0, 0);

	// MSFT: Seb
	//CmdContext.FlushCommands();
}

//#if PLATFORM_SUPPORTS_RHI_THREAD
void FD3D12CommandContext::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI, bool bPresent, bool bLockToVsync)
{
	//check(IsDefaultContext());
	FD3D12Device* Device = GetParentDevice();
	FD3D12CommandContext& CmdContext = *this;
	FD3D12DynamicRHI& RHI = *Device->GetOwningRHI();
//#else
//void FD3D12DynamicRHI::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI, bool bPresent, bool bLockToVsync)
//{
//	FD3D12Device* Device = GetRHIDevice();
//	FD3D12CommandContext& CmdContext = Device->GetDefaultCommandContext();
//	FD3D12DynamicRHI& RHI = *this;
//#endif

	FD3D12Viewport*  Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);

	SCOPE_CYCLE_COUNTER(STAT_D3D12PresentTime);

	check(Device->GetDrawingViewport() == Viewport);
	Device->SetDrawingViewport(nullptr);

	// Clear references the device might have to resources.
	CmdContext.CurrentDepthTexture = NULL;
	CmdContext.CurrentDepthStencilTarget = NULL;
	CmdContext.CurrentDSVAccessType = FExclusiveDepthStencil::DepthWrite_StencilWrite;
	CmdContext.CurrentRenderTargets[0] = NULL;
	for (uint32 RenderTargetIndex = 1; RenderTargetIndex < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++RenderTargetIndex)
	{
		CmdContext.CurrentRenderTargets[RenderTargetIndex] = NULL;
	}

	CmdContext.ClearAllShaderResources();

	CmdContext.CommitRenderTargetsAndUAVs();

	CmdContext.StateCache.SetBoundShaderState(nullptr);

	for (uint32 StreamIndex = 0; StreamIndex < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; StreamIndex++)
	{
		CmdContext.StateCache.SetStreamSource(nullptr, StreamIndex, 0, 0);
	}

	CmdContext.StateCache.SetIndexBuffer((FD3D12ResourceLocation*)nullptr, DXGI_FORMAT_R16_UINT, 0);
	// Compute Shader is set to NULL after each Dispatch call, so no need to clear it here
#if STATS
	SET_CYCLE_COUNTER(STAT_D3D12CommitResourceTables, RHI.CommitResourceTableCycles);
	SET_CYCLE_COUNTER(STAT_D3D12CacheResourceTables, RHI.CacheResourceTableCycles);
	SET_CYCLE_COUNTER(STAT_D3D12SetShaderTextureTime, RHI.SetShaderTextureCycles);
	INC_DWORD_STAT_BY(STAT_D3D12SetShaderTextureCalls, RHI.SetShaderTextureCalls);
	INC_DWORD_STAT_BY(STAT_D3D12CacheResourceTableCalls, RHI.CacheResourceTableCalls);
	INC_DWORD_STAT_BY(STAT_D3D12SetTextureInTableCalls, RHI.SetTextureInTableCalls);
#endif

	RHI.CommitResourceTableCycles = 0;
	RHI.CacheResourceTableCalls = 0;
	RHI.CacheResourceTableCycles = 0;
	RHI.SetShaderTextureCycles = 0;
	RHI.SetShaderTextureCalls = 0;
	RHI.SetTextureInTableCalls = 0;

	bool bNativelyPresented = Viewport->Present(bLockToVsync);

	// Don't wait on the GPU when using SLI, let the driver determine how many frames behind the GPU should be allowed to get
	if (GNumActiveGPUsForRendering == 1)
	{
		if (bNativelyPresented)
		{
			static const auto CFinishFrameVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FinishCurrentFrame"));
			if (CFinishFrameVar->GetValueOnRenderThread())
			{
				// Wait for the GPU to finish rendering the previous frame before finishing this frame.
				CmdContext.FlushCommands(true);
			}
		}

		// If the input latency timer has been triggered, block until the GPU is completely
		// finished displaying this frame and calculate the delta time.
		if (GInputLatencyTimer.RenderThreadTrigger)
		{
			CmdContext.FlushCommands(true);

			uint32 EndTime = FPlatformTime::Cycles();
			GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
			GInputLatencyTimer.RenderThreadTrigger = false;
		}
	}

	Device->GetDeferredDeletionQueue().ReleaseResources();

	const uint32 NumContexts = Device->GetNumContexts();
	for (uint32 i = 0; i < NumContexts; ++i)
	{
		Device->GetCommandContext(i).EndFrame();
	}

	Device->GetDefaultUploadHeapAllocator().CleanUpAllocations();
	Device->GetTextureAllocator().CleanUpAllocations();
	Device->GetDefaultBufferAllocator().CleanupFreeBlocks();

	Device->GetDefaultFastAllocatorPool().CleanUpPages(10);
	{
		FScopeLock Lock(Device->GetBufferInitFastAllocator().GetCriticalSection());
		Device->GetBufferInitFastAllocatorPool().CleanUpPages(10);
	}

	// The Texture streaming threads share a pool
	{
		FD3D12ThreadSafeFastAllocator* pCurrentHelperThreadDynamicHeapAllocator = nullptr;
		for (uint32 i = 0; i < FD3D12DynamicRHI::GetD3DRHI()->NumThreadDynamicHeapAllocators; ++i)
		{
			pCurrentHelperThreadDynamicHeapAllocator = FD3D12DynamicRHI::GetD3DRHI()->ThreadDynamicHeapAllocatorArray[i];
			if (pCurrentHelperThreadDynamicHeapAllocator)
			{
				FScopeLock Lock(pCurrentHelperThreadDynamicHeapAllocator->GetCriticalSection());
				pCurrentHelperThreadDynamicHeapAllocator->GetPool()->CleanUpPages(10);
				break;
			}
		}
	}

	DXGI_QUERY_VIDEO_MEMORY_INFO LocalVideoMemoryInfo;
	FD3D12DynamicRHI::GetD3DRHI()->GetLocalVideoMemoryInfo(&LocalVideoMemoryInfo);

	int64 budget = LocalVideoMemoryInfo.Budget;
	int64 AvailableSpace = budget - int64(LocalVideoMemoryInfo.CurrentUsage);

	SET_MEMORY_STAT(STAT_D3D12UsedVideoMemory, LocalVideoMemoryInfo.CurrentUsage);
	SET_MEMORY_STAT(STAT_D3D12AvailableVideoMemory, AvailableSpace);
	SET_MEMORY_STAT(STAT_D3D12TotalVideoMemory, budget);

#if CHECK_SRV_TRANSITIONS
	UnresolvedTargets.Reset();
#endif
}

void FD3D12DynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer()
{
	ViewportFrameCounter++;
}

FTexture2DRHIRef FD3D12DynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	FD3D12Viewport*  Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);

	if (GRHISupportsRHIThread)
		return Viewport->GetBackBuffer(ViewportFrameCounter);
	else
		return Viewport->GetBackBuffer();
}

// MSFT: Seb
//FTexture2DRHIRef FD3D12DynamicRHI::RHIGetViewportForegroundBuffer(FViewportRHIParamRef ViewportRHI)
//{
//	FD3D12Viewport* Viewport = FD3D12DynamicRHI::ResourceCast(ViewportRHI);
//
//	return Viewport->GetBackBuffer(ViewportFrameCounter);
//}

#if D3D12_WITH_DWMAPI
#include "HideWindowsPlatformTypes.h"
#endif	//D3D12_WITH_DWMAPI

void* FD3D12Viewport::GetNativeBackBufferTexture() const
{ 
	return GetBackBuffer()->GetResource();
}

void* FD3D12Viewport::GetNativeBackBufferRT() const
{
	return GetBackBuffer()->GetRenderTargetView(0, 0);
}