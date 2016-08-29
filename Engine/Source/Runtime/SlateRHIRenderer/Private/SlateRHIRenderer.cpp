// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "ElementBatcher.h"
#include "StereoRendering.h"
#include "Runtime/Engine/Public/Features/ILiveStreamingService.h"
#include "SlateNativeTextureResource.h"
#include "SceneUtils.h"

DECLARE_CYCLE_STAT(TEXT("Map Staging Buffer"),STAT_MapStagingBuffer,STATGROUP_CrashTracker);
DECLARE_CYCLE_STAT(TEXT("Generate Capture Buffer"),STAT_GenerateCaptureBuffer,STATGROUP_CrashTracker);
DECLARE_CYCLE_STAT(TEXT("Unmap Staging Buffer"),STAT_UnmapStagingBuffer,STATGROUP_CrashTracker);

DECLARE_CYCLE_STAT(TEXT("Slate RT: Rendering"), STAT_SlateRenderingRTTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Slate RT: Create Batches"), STAT_SlateRTCreateBatches, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Slate RT: Fill Vertex & Index Buffers"), STAT_SlateRTFillVertexIndexBuffers, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Slate RT: Draw Batches"), STAT_SlateRTDrawBatches, STATGROUP_Slate);

DECLARE_FLOAT_COUNTER_STAT(TEXT("Slate UI/present"), Stat_GPU_SlateUI, STATGROUP_GPU); 

// Defines the maximum size that a slate viewport will create
#define MAX_VIEWPORT_SIZE 16384


void FSlateCrashReportResource::InitDynamicRHI()
{
	int32 Width = VirtualScreen.Width();
	int32 Height = VirtualScreen.Height();

	FRHIResourceCreateInfo CreateInfo;
	CrashReportBuffer = RHICreateTexture2D(
		Width,
		Height,
		PF_R8G8B8A8,
		1,
		1,
		TexCreate_RenderTargetable,
		CreateInfo
		);

	for (int32 i = 0; i < 2; ++i)
	{
		ReadbackBuffer[i] = RHICreateTexture2D(
			Width,
			Height,
			PF_R8G8B8A8,
			1,
			1,
			TexCreate_CPUReadback,
			CreateInfo
			);
	}
	
	ReadbackBufferIndex = 0;
}

void FSlateCrashReportResource::ReleaseDynamicRHI()
{
	ReadbackBuffer[0].SafeRelease();
	ReadbackBuffer[1].SafeRelease();
	CrashReportBuffer.SafeRelease();
}

FSlateWindowElementList* FSlateCrashReportResource::GetNextElementList()
{
	ElementListIndex = (ElementListIndex + 1) % 2;
	return &ElementList[ElementListIndex];
}


void FSlateRHIRenderer::FViewportInfo::InitRHI()
{
	// Viewport RHI is created on the game thread
	// Create the depth-stencil surface if needed.
	RecreateDepthBuffer_RenderThread();
}

void FSlateRHIRenderer::FViewportInfo::ReleaseRHI()
{
	DepthStencil.SafeRelease();
	ViewportRHI.SafeRelease();	
}

void FSlateRHIRenderer::FViewportInfo::ConditionallyUpdateDepthBuffer(bool bInRequiresStencilTest)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER( UpdateDepthBufferCommand, 
		FViewportInfo*, ViewportInfo, this,
		bool, bNewRequiresStencilTest, bInRequiresStencilTest,
	{
		// Allocate a stencil buffer if needed and not already allocated
		if (bNewRequiresStencilTest && !ViewportInfo->bRequiresStencilTest)
		{
			ViewportInfo->bRequiresStencilTest = bNewRequiresStencilTest;
			ViewportInfo->RecreateDepthBuffer_RenderThread();
		}
	});
}

void FSlateRHIRenderer::FViewportInfo::RecreateDepthBuffer_RenderThread()
{
	check(IsInRenderingThread());
	DepthStencil.SafeRelease();
	if (bRequiresStencilTest)
	{		
		FTexture2DRHIRef ShaderResourceUnused;
		FRHIResourceCreateInfo CreateInfo(FClearValueBinding::DepthZero);
		RHICreateTargetableShaderResource2D( Width, Height, PF_DepthStencil, 1, TexCreate_None, TexCreate_DepthStencilTargetable, false, CreateInfo, DepthStencil, ShaderResourceUnused );
		check( IsValidRef(DepthStencil) );
	}
}



FSlateRHIRenderer::FSlateRHIRenderer( TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager )
	: FSlateRenderer(InSlateFontServices)
#if USE_MAX_DRAWBUFFERS
	, EnqueuedWindowDrawBuffer(NULL)
	, FreeBufferIndex(1)
#endif
{
	ResourceManager = InResourceManager;

	CrashTrackerResource = NULL;
	ViewMatrix = FMatrix(	FPlane(1,	0,	0,	0),
							FPlane(0,	1,	0,	0),
							FPlane(0,	0,	1,  0),
							FPlane(0,	0,	0,	1));

	bTakingAScreenShot = false;
	OutScreenshotData = NULL;
}

FSlateRHIRenderer::~FSlateRHIRenderer()
{
}

FMatrix FSlateRHIRenderer::CreateProjectionMatrix(uint32 Width, uint32 Height)
{
	// Create ortho projection matrix
	const float Left = 0;
	const float Right = Left + Width;
	const float Top = 0;
	const float Bottom = Top + Height;
	const float ZNear = -100.0f;
	const float ZFar = 100.0f;
	return AdjustProjectionMatrixForRHI(
		FMatrix(
		FPlane(2.0f / (Right - Left), 0, 0, 0),
		FPlane(0, 2.0f / (Top - Bottom), 0, 0),
		FPlane(0, 0, 1 / (ZNear - ZFar), 0),
		FPlane((Left + Right) / (Left - Right), (Top + Bottom) / (Bottom - Top), ZNear / (ZNear - ZFar), 1)
		)
		);
}

bool FSlateRHIRenderer::Initialize()
{
	LoadUsedTextures();

	RenderingPolicy = MakeShareable( new FSlateRHIRenderingPolicy( SlateFontServices.ToSharedRef(), ResourceManager.ToSharedRef() ) );

	ElementBatcher = MakeShareable( new FSlateElementBatcher( RenderingPolicy.ToSharedRef() ) );

	return true;
}

void FSlateRHIRenderer::Destroy()
{
	RenderingPolicy->ReleaseResources();
	ResourceManager->ReleaseResources();
	SlateFontServices->ReleaseResources();

	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		BeginReleaseResource( It.Value() );
	}
	
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	if (CrashTrackerResource != nullptr)
	{
		BeginReleaseResource(CrashTrackerResource);
	}
#endif

	FlushRenderingCommands();
	
	check( ElementBatcher.IsUnique() );
	ElementBatcher.Reset();
	RenderingPolicy.Reset();
	ResourceManager.Reset();
	SlateFontServices.Reset();

	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		FViewportInfo* ViewportInfo = It.Value();
		delete ViewportInfo;
	}
	
	if (CrashTrackerResource != nullptr)
	{
		delete CrashTrackerResource;
		CrashTrackerResource = nullptr;
	}

	WindowToViewportInfo.Empty();
}

/** Returns a draw buffer that can be used by Slate windows to draw window elements */
FSlateDrawBuffer& FSlateRHIRenderer::GetDrawBuffer()
{
	FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;
	
	FSlateDrawBuffer* Buffer = &DrawBuffers[FreeBufferIndex];
	
	while( !Buffer->Lock() )
	{
		// If the buffer cannot be locked then the buffer is still in use.  If we are here all buffers are in use
		// so wait until one is free.
		if (IsInSlateThread())
		{
			// We can't flush commands on the slate thread, so simply spinlock until we're done
			// this happens if the render thread becomes completely blocked by expensive tasks when the Slate thread is running
			// in this case we cannot tick Slate.
			FPlatformProcess::Sleep(0.001f);
		}
		else
		{
			FlushCommands();
			UE_LOG(LogSlate, Warning, TEXT("Slate: Had to block on waiting for a draw buffer"));
			FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;
		}
	

		Buffer = &DrawBuffers[FreeBufferIndex];
	}

	// Safely remove brushes by emptying the array and releasing references
	DynamicBrushesToRemove[FreeBufferIndex].Empty();

	Buffer->ClearBuffer();
	return *Buffer;
}

void FSlateRHIRenderer::CreateViewport( const TSharedRef<SWindow> Window )
{
	FlushRenderingCommands();

	if( !WindowToViewportInfo.Contains( &Window.Get() ) )
	{
		const FVector2D WindowSize = Window->GetViewportSize();

		// Clamp the window size to a reasonable default anything below 8 is a d3d warning and 8 is used anyway.
		// @todo Slate: This is a hack to work around menus being summoned with 0,0 for window size until they are ticked.
		const uint32 Width = FMath::Max(8,FMath::TruncToInt(WindowSize.X));
		const uint32 Height = FMath::Max(8,FMath::TruncToInt(WindowSize.Y));

		FViewportInfo* NewInfo = new FViewportInfo();
		// Create Viewport RHI if it doesn't exist (this must be done on the game thread)
		TSharedRef<FGenericWindow> NativeWindow = Window->GetNativeWindow().ToSharedRef();
		NewInfo->OSWindow = NativeWindow->GetOSWindowHandle();
		NewInfo->Width = Width;
		NewInfo->Height = Height;
		NewInfo->DesiredWidth = Width;
		NewInfo->DesiredHeight = Height;
		NewInfo->ProjectionMatrix = CreateProjectionMatrix( Width, Height );
#if ALPHA_BLENDED_WINDOWS		
		if( Window->GetTransparencySupport() == EWindowTransparency::PerPixel )
		{
			NewInfo->PixelFormat = PF_B8G8R8A8;
		}
#endif

		// Sanity check dimensions
		checkf(Width <= MAX_VIEWPORT_SIZE && Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), Width, Height);

		bool bFullscreen = IsViewportFullscreen( *Window );
		NewInfo->ViewportRHI = RHICreateViewport( NewInfo->OSWindow, Width, Height, bFullscreen, NewInfo->PixelFormat );
		NewInfo->bFullscreen = bFullscreen;

		WindowToViewportInfo.Add( &Window.Get(), NewInfo );

		BeginInitResource( NewInfo );
	}
}

void FSlateRHIRenderer::ConditionalResizeViewport( FViewportInfo* ViewInfo, uint32 Width, uint32 Height, bool bFullscreen )
{
	checkSlow( IsThreadSafeForSlateRendering() );

	if( IsInGameThread() && !IsInSlateThread() && ViewInfo && ( ViewInfo->Height != Height || ViewInfo->Width != Width ||  ViewInfo->bFullscreen != bFullscreen || !IsValidRef(ViewInfo->ViewportRHI) ) )
	{
		// The viewport size we have doesn't match the requested size of the viewport.
		// Resize it now.

		// cannot resize the viewport while potentially using it.
		FlushRenderingCommands();
	

		// Windows are allowed to be zero sized ( sometimes they are animating to/from zero for example)
		// but viewports cannot be zero sized.  Use 8x8 as a reasonably sized viewport in this case.
		uint32 NewWidth = FMath::Max<uint32>( 8, Width );
		uint32 NewHeight = FMath::Max<uint32>( 8, Height );

		// Sanity check dimensions
		if (NewWidth > MAX_VIEWPORT_SIZE)
		{
			UE_LOG(LogSlate, Warning, TEXT("Tried to set viewport width size to %d.  Clamping size to max allowed size of %d instead."), NewWidth, MAX_VIEWPORT_SIZE);
			NewWidth = MAX_VIEWPORT_SIZE;
		}

		if (NewHeight > MAX_VIEWPORT_SIZE)
		{
			UE_LOG(LogSlate, Warning, TEXT("Tried to set viewport height size to %d.  Clamping size to max allowed size of %d instead."), NewHeight, MAX_VIEWPORT_SIZE);
			NewHeight = MAX_VIEWPORT_SIZE;
		}

		ViewInfo->Width = NewWidth;
		ViewInfo->Height = NewHeight;
		ViewInfo->DesiredWidth = NewWidth;
		ViewInfo->DesiredHeight = NewHeight;
		ViewInfo->ProjectionMatrix = CreateProjectionMatrix( NewWidth, NewHeight );
		ViewInfo->bFullscreen = bFullscreen;

		if( IsValidRef( ViewInfo->ViewportRHI ) )
		{
			RHIResizeViewport(ViewInfo->ViewportRHI, NewWidth, NewHeight, bFullscreen);
		}
		else
		{
			ViewInfo->ViewportRHI = RHICreateViewport(ViewInfo->OSWindow, NewWidth, NewHeight, bFullscreen, ViewInfo->PixelFormat);
		}
	}
}

void FSlateRHIRenderer::UpdateFullscreenState( const TSharedRef<SWindow> Window, uint32 OverrideResX, uint32 OverrideResY )
{
	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef( &Window.Get() );

	if( !ViewInfo )
	{
		CreateViewport( Window );
	}

	ViewInfo = WindowToViewportInfo.FindRef( &Window.Get() );

	if( ViewInfo )
	{
		const bool bFullscreen = IsViewportFullscreen( *Window );
		
		uint32 ResX = OverrideResX ? OverrideResX : GSystemResolution.ResX;
		uint32 ResY = OverrideResY ? OverrideResY : GSystemResolution.ResY;

		if( (GIsEditor && Window->IsViewportSizeDrivenByWindow()) || (Window->GetWindowMode() == EWindowMode::WindowedFullscreen))
		{
			ResX = ViewInfo->Width;
			ResY = ViewInfo->Height;
		}

		ConditionalResizeViewport( ViewInfo, ResX, ResY, bFullscreen );
	}
}

void FSlateRHIRenderer::RestoreSystemResolution(const TSharedRef<SWindow> InWindow)
{
	if (!GIsEditor)
	{
		// Force the window system to resize the active viewport, even though nothing might have appeared to change.
		// On windows, DXGI might change the window resolution behind our backs when we alt-tab out. This will make
		// sure that we are actually in the resolution we think we are.
#if !PLATFORM_HTML5
		// @todo: fixme for HTML5. 
		GSystemResolution.ForceRefresh();
#endif 
	}
}

/** Called when a window is destroyed to give the renderer a chance to free resources */
void FSlateRHIRenderer::OnWindowDestroyed( const TSharedRef<SWindow>& InWindow )
{
	checkSlow(IsThreadSafeForSlateRendering());

	FViewportInfo** ViewportInfoPtr = WindowToViewportInfo.Find( &InWindow.Get() );
	if( ViewportInfoPtr )
	{
		BeginReleaseResource( *ViewportInfoPtr );

		// Need to flush rendering commands as the viewport may be in use by the render thread
		// and the rendering resources must be released on the render thread before the viewport can be deleted
		FlushRenderingCommands();
	
		delete *ViewportInfoPtr;
	}

	WindowToViewportInfo.Remove( &InWindow.Get() );
}

/** Draws windows from a FSlateDrawBuffer on the render thread */
void FSlateRHIRenderer::DrawWindow_RenderThread(FRHICommandListImmediate& RHICmdList, const FViewportInfo& ViewportInfo, FSlateWindowElementList& WindowElementList, bool bLockToVsync, bool bClear)
{
	SCOPED_DRAW_EVENT(RHICmdList, SlateUI);
	//SCOPED_GPU_STAT(RHICmdList, Stat_GPU_SlateUI); // Disabled since this seems to be distorted by the present time

	// Should only be called by the rendering thread
	check(IsInRenderingThread());
	
	{
		SCOPE_CYCLE_COUNTER( STAT_SlateRenderingRTTime );

		FSlateBatchData& BatchData = WindowElementList.GetBatchData();
		FElementBatchMap& RootBatchMap = WindowElementList.GetRootDrawLayer().GetElementBatchMap();

		WindowElementList.PreDraw_ParallelThread();

		{
			SCOPE_CYCLE_COUNTER(STAT_SlateRTCreateBatches);
			// Update the vertex and index buffer	
			BatchData.CreateRenderBatches(RootBatchMap);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_SlateRTFillVertexIndexBuffers);
			RenderingPolicy->UpdateVertexAndIndexBuffers(RHICmdList, BatchData);
		}

		// should have been created by the game thread
		check( IsValidRef(ViewportInfo.ViewportRHI) );

		FTexture2DRHIRef ViewportRT = ViewportInfo.GetRenderTargetTexture();
		FTexture2DRHIRef BackBuffer = (ViewportRT) ?
		ViewportRT : RHICmdList.GetViewportBackBuffer(ViewportInfo.ViewportRHI);
		
		const uint32 ViewportWidth = (ViewportRT) ? ViewportRT->GetSizeX() : ViewportInfo.Width;
		const uint32 ViewportHeight = (ViewportRT) ? ViewportRT->GetSizeY() : ViewportInfo.Height;

		RHICmdList.BeginDrawingViewport( ViewportInfo.ViewportRHI, FTextureRHIRef() );
		RHICmdList.SetViewport(0, 0, 0, ViewportWidth, ViewportHeight, 0.0f);
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, BackBuffer);
		if( ViewportInfo.bRequiresStencilTest )
		{
			check(IsValidRef( ViewportInfo.DepthStencil ));

			// Reset the backbuffer as our color render target and also set a depth stencil buffer
			FRHIRenderTargetView ColorView(BackBuffer, 0, -1, bClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
			FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView(ViewportInfo.DepthStencil, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore));

			// Clear the stencil buffer
			RHICmdList.SetRenderTargetsAndClear(Info);
		}
		else
		{
			SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef(), bClear ? ESimpleRenderTargetMode::EClearColorAndDepth : ESimpleRenderTargetMode::EExistingColorAndDepth);
		}

#if DEBUG_OVERDRAW
		RHIClear(true, FLinearColor::Black, false, 0.0f, true, 0x00, FIntRect());
#endif
		if( BatchData.GetRenderBatches().Num() > 0 )
		{
			SCOPE_CYCLE_COUNTER(STAT_SlateRTDrawBatches);

			FSlateBackBuffer BackBufferTarget( BackBuffer, FIntPoint( ViewportWidth, ViewportHeight ) );

			RenderingPolicy->DrawElements
			(
				RHICmdList,
				BackBufferTarget,
				ViewMatrix*ViewportInfo.ProjectionMatrix,
				BatchData.GetRenderBatches()
			);
		}
	}

	if (GEngine && IsValidRef(ViewportInfo.GetRenderTargetTexture()) && GEngine->StereoRenderingDevice.IsValid())
	{
		GEngine->StereoRenderingDevice->RenderTexture_RenderThread(RHICmdList, RHICmdList.GetViewportBackBuffer(ViewportInfo.ViewportRHI), ViewportInfo.GetRenderTargetTexture());
	}

	// Calculate renderthread time (excluding idle time).	
	uint32 StartTime		= FPlatformTime::Cycles();

	RHICmdList.EndDrawingViewport(ViewportInfo.ViewportRHI, true, bLockToVsync);

	uint32 EndTime		= FPlatformTime::Cycles();

	GSwapBufferTime		= EndTime - StartTime;
	SET_CYCLE_COUNTER(STAT_PresentTime, GSwapBufferTime);

	static uint32 LastTimestamp	= 0;
	uint32 ThreadTime	= EndTime - LastTimestamp;
	LastTimestamp		= EndTime;

	uint32 RenderThreadIdle = 0;	

	FThreadIdleStats& RenderThread = FThreadIdleStats::Get();
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForAllOtherSleep] = RenderThread.Waits;
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += GSwapBufferTime;
	GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
	RenderThread.Waits = 0;

	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_RenderThreadSleepTime, GRenderThreadIdle[0]);
	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_WaitingForGPUQuery, GRenderThreadIdle[1]);
	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_WaitingForGPUPresent, GRenderThreadIdle[2]);

	for (int32 Index = 0; Index < ERenderThreadIdleTypes::Num; Index++)
	{
		RenderThreadIdle += GRenderThreadIdle[Index];
		GRenderThreadIdle[Index] = 0;
		GRenderThreadNumIdle[Index] = 0;
	}

	SET_CYCLE_COUNTER(STAT_RenderingIdleTime, RenderThreadIdle);	
	GRenderThreadTime	= (ThreadTime > RenderThreadIdle) ? (ThreadTime - RenderThreadIdle) : ThreadTime;
}

void FSlateRHIRenderer::DrawWindows( FSlateDrawBuffer& WindowDrawBuffer )
{
	if (IsInSlateThread())
	{
		EnqueuedWindowDrawBuffer = &WindowDrawBuffer;
	}
	else
	{
		DrawWindows_Private(WindowDrawBuffer);
	}
}

void FSlateRHIRenderer::DrawWindows()
{
	if (EnqueuedWindowDrawBuffer)
	{
		DrawWindows_Private(*EnqueuedWindowDrawBuffer);
		EnqueuedWindowDrawBuffer = NULL;
	}
}

void FSlateRHIRenderer::PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData)
{
	check(OutColorData);

	bTakingAScreenShot = true;
	ScreenshotRect = Rect;
	OutScreenshotData = OutColorData;
}

/** 
 * Creates necessary resources to render a window and sends draw commands to the rendering thread
 *
 * @param WindowDrawBuffer	The buffer containing elements to draw 
 */
void FSlateRHIRenderer::DrawWindows_Private( FSlateDrawBuffer& WindowDrawBuffer )
{
	checkSlow( IsThreadSafeForSlateRendering() );

	// Enqueue a command to unlock the draw buffer after all windows have been drawn
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( SlateBeginDrawingWindowsCommand, 
		FSlateRHIRenderingPolicy&, Policy, *RenderingPolicy,
	{
		Policy.BeginDrawingWindows();
	});

	// Update texture atlases if needed and safe
	if (DoesThreadOwnSlateRendering())
	{
		ResourceManager->UpdateTextureAtlases();
	}

	const TSharedRef<FSlateFontCache> FontCache = SlateFontServices->GetFontCache();

	// Iterate through each element list and set up an RHI window for it if needed
	TArray<TSharedPtr<FSlateWindowElementList>>& WindowElementLists = WindowDrawBuffer.GetWindowElementLists();
	for( int32 ListIndex = 0; ListIndex < WindowElementLists.Num(); ++ListIndex )
	{
		FSlateWindowElementList& ElementList = *WindowElementLists[ListIndex];

		TSharedPtr<SWindow> Window = ElementList.GetWindow();

		if( Window.IsValid() )
		{
			const FVector2D WindowSize = Window->GetViewportSize();
			if ( WindowSize.X > 0 && WindowSize.Y > 0 )
			{
				// Add all elements for this window to the element batcher
				ElementBatcher->AddElements( ElementList );

				// Update the font cache with new text after elements are batched
				FontCache->UpdateCache();

				bool bRequiresStencilTest = false;
				bool bLockToVsync = false;

				bLockToVsync = ElementBatcher->RequiresVsync();

				bool bForceVsyncFromCVar = false;
				if(GIsEditor)
				{
					static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSyncEditor"));
					bForceVsyncFromCVar = (CVar->GetInt() != 0);
				}
				else
				{
					static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
					bForceVsyncFromCVar = (CVar->GetInt() != 0);
				}

				bLockToVsync |= bForceVsyncFromCVar;

				// All elements for this window have been batched and rendering data updated
				ElementBatcher->ResetBatches();

				// The viewport had better exist at this point  
				FViewportInfo* ViewInfo = WindowToViewportInfo.FindChecked( Window.Get() );

				if (Window->IsViewportSizeDrivenByWindow())
				{
					// Resize the viewport if needed
					ConditionalResizeViewport(ViewInfo, ViewInfo->DesiredWidth, ViewInfo->DesiredHeight, IsViewportFullscreen(*Window));
				}

				if( bRequiresStencilTest )
				{	
					ViewInfo->ConditionallyUpdateDepthBuffer(bRequiresStencilTest);
				}

				// Tell the rendering thread to draw the windows
				{
					struct FSlateDrawWindowCommandParams
					{
						FSlateRHIRenderer* Renderer;
						FSlateRHIRenderer::FViewportInfo* ViewportInfo;
						FSlateWindowElementList* WindowElementList;
						SWindow* SlateWindow;
						bool bLockToVsync;
						bool bClear;
					} Params;

					Params.Renderer = this;
					Params.ViewportInfo = ViewInfo;
					Params.WindowElementList = &ElementList;
					Params.bLockToVsync = bLockToVsync;
#if ALPHA_BLENDED_WINDOWS
					Params.bClear = Window->GetTransparencySupport() == EWindowTransparency::PerPixel;
#else
					Params.bClear = false;
#endif

					// NOTE: We pass a raw pointer to the SWindow so that we don't have to use a thread-safe weak pointer in
					// the FSlateWindowElementList structure
					Params.SlateWindow = Window.Get();

					// Skip the actual draw if we're in a headless execution environment
					if (GIsClient && !IsRunningCommandlet() && !GUsingNullRHI)
					{
						ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(SlateDrawWindowsCommand,
							FSlateDrawWindowCommandParams, Params, Params,
							{
								Params.Renderer->DrawWindow_RenderThread(RHICmdList, *Params.ViewportInfo, *Params.WindowElementList, Params.bLockToVsync, Params.bClear);
							});
					}

					SlateWindowRendered.Broadcast( *Params.SlateWindow, &ViewInfo->ViewportRHI );

					if ( bTakingAScreenShot )
					{
						ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(SlateCaptureScreenshotCommand,
							FSlateDrawWindowCommandParams, Params, Params,
							FIntRect, ScreenshotRect, ScreenshotRect,
							TArray<FColor>*, OutScreenshotData, OutScreenshotData,
						{
							FTexture2DRHIRef BackBuffer = RHICmdList.GetViewportBackBuffer(Params.ViewportInfo->ViewportRHI);
							RHICmdList.ReadSurfaceData(BackBuffer, ScreenshotRect, *OutScreenshotData, FReadSurfaceDataFlags());
						});

						FlushRenderingCommands();

						bTakingAScreenShot = false;
						OutScreenshotData = NULL;
					}
				}
			}
		}
		else
		{
			ensureMsgf( false, TEXT("Window isnt valid but being drawn!") );
		}
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER( SlateEndDrawingWindowsCommand, 
		FSlateDrawBuffer*, DrawBuffer, &WindowDrawBuffer,
		FSlateRHIRenderingPolicy&, Policy, *RenderingPolicy,
	{
		FSlateEndDrawingWindowsCommand::EndDrawingWindows(RHICmdList, DrawBuffer, Policy);
	});

	// flush the cache if needed
	FontCache->ConditionalFlushCache();
}


FIntRect FSlateRHIRenderer::SetupVirtualScreenBuffer(const bool bPrimaryWorkAreaOnly, const float ScreenScaling, ILiveStreamingService* LiveStreamingService )
{
	// Figure out how big we need our render targets to be, based on the size of the entire desktop and the configured scaling amount
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

#if !PLATFORM_WINDOWS && !PLATFORM_MAC && !PLATFORM_LINUX
	ensureMsgf(0, TEXT("This functionality is not valid for this platform"));	
	return FIntRect(FIntPoint(0, 0), FIntPoint(DisplayMetrics.PrimaryDisplayWidth, DisplayMetrics.PrimaryDisplayHeight));
#endif	

	FIntPoint UnscaledVirtualScreenOrigin;
	FIntPoint UnscaledVirtualScreenLowerRight;

	if( bPrimaryWorkAreaOnly )
	{
		UnscaledVirtualScreenOrigin = FIntPoint(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left, DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);
		UnscaledVirtualScreenLowerRight = FIntPoint(DisplayMetrics.PrimaryDisplayWorkAreaRect.Right, DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom);
	}
	else
	{
		UnscaledVirtualScreenOrigin = FIntPoint(DisplayMetrics.VirtualDisplayRect.Left, DisplayMetrics.VirtualDisplayRect.Top);
		UnscaledVirtualScreenLowerRight = FIntPoint(DisplayMetrics.VirtualDisplayRect.Right, DisplayMetrics.VirtualDisplayRect.Bottom);
	}
	const FIntRect UnscaledVirtualScreen = FIntRect(UnscaledVirtualScreenOrigin, UnscaledVirtualScreenLowerRight);
	FIntRect ScaledVirtualScreen;
	{
		int BufferWidth = FMath::FloorToInt( (float)UnscaledVirtualScreen.Width() * ScreenScaling );
		int BufferHeight = FMath::FloorToInt( (float)UnscaledVirtualScreen.Height() * ScreenScaling );
		
		// If we're preparing a buffer for live streaming, then go ahead and make sure the resolution will be valid for that
		if( LiveStreamingService != nullptr )
		{
			// @todo livestream: This could cause the aspect ratio to be changed and the buffer to be stretched non-uniformly, but usually the aspect only changes slightly
			LiveStreamingService->MakeValidVideoBufferResolution( BufferWidth, BufferHeight );
		}

		const float XScaling = (float)BufferWidth / (float)UnscaledVirtualScreen.Width();
		const float YScaling = (float)BufferHeight / (float)UnscaledVirtualScreen.Height();

		ScaledVirtualScreen.Min.X = FMath::FloorToInt(( float)UnscaledVirtualScreen.Min.X * XScaling );
		ScaledVirtualScreen.Max.X = FMath::FloorToInt(( float)UnscaledVirtualScreen.Max.X * XScaling );
		ScaledVirtualScreen.Min.Y = FMath::FloorToInt(( float)UnscaledVirtualScreen.Min.Y * YScaling );
		ScaledVirtualScreen.Max.Y = FMath::FloorToInt(( float)UnscaledVirtualScreen.Max.Y * YScaling );
	}

	// @todo livestream: This CrashTrackerResource is now also used for editor live streaming, so we should consider renaming it and cleaning
	// up the API a little bit more
	if( CrashTrackerResource == nullptr || 
		ScaledVirtualScreen != CrashTrackerResource->GetVirtualScreen() )
	{
		if( CrashTrackerResource != nullptr )
		{
			// Size has changed, so clear out our old resource and create a new one
			BeginReleaseResource(CrashTrackerResource);

			FlushRenderingCommands();
	
			if (CrashTrackerResource != nullptr)
			{
				delete CrashTrackerResource;
				CrashTrackerResource = NULL;
			}
		}

		CrashTrackerResource = new FSlateCrashReportResource(ScaledVirtualScreen, UnscaledVirtualScreen);
		BeginInitResource(CrashTrackerResource);
	}

	return CrashTrackerResource->GetVirtualScreen();
}


void FSlateRHIRenderer::CopyWindowsToVirtualScreenBuffer(const TArray<FString>& KeypressBuffer)
{
#if !PLATFORM_WINDOWS && !PLATFORM_MAC && !PLATFORM_LINUX
	ensureMsgf(0, TEXT("This functionality is not valid for this platform"));
	return;
#endif

	SCOPE_CYCLE_COUNTER(STAT_GenerateCaptureBuffer);

	// Make sure to call SetupDrawBuffer() before calling this function!
	check( CrashTrackerResource != nullptr );

	const FIntRect VirtualScreen = CrashTrackerResource->GetVirtualScreen();
	const FIntPoint VirtualScreenPos = VirtualScreen.Min;
	const FIntPoint VirtualScreenSize = VirtualScreen.Size();
	const FIntRect UnscaledVirtualScreen = CrashTrackerResource->GetUnscaledVirtualScreen();
	const float XScaling = (float)VirtualScreen.Width() / (float)UnscaledVirtualScreen.Width();
	const float YScaling = (float)VirtualScreen.Height() / (float)UnscaledVirtualScreen.Height();
	
	// setup state
	struct FSetupWindowStateContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
	};
	FSetupWindowStateContext SetupWindowStateContext =
	{
		CrashTrackerResource,
		VirtualScreen
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		SetupWindowState,
		FSetupWindowStateContext,Context,SetupWindowStateContext,
	{
		SetRenderTarget(RHICmdList, Context.CrashReportResource->GetBuffer(), FTextureRHIRef());		
		RHICmdList.SetViewport(0, 0, 0.0f, Context.IntermediateBufferSize.Width(), Context.IntermediateBufferSize.Height(), 1.0f);
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		
		RHICmdList.SetViewport(0, 0, 0.0f, Context.IntermediateBufferSize.Width(), Context.IntermediateBufferSize.Height(), 1.0f);
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

		// @todo livestream: Ideally this "desktop background color" should be configurable in the editor's preferences
		RHICmdList.Clear(true, FLinearColor(0.8f, 0.00f, 0.0f), false, 0.f, false, 0x00, FIntRect());
	});

	// draw windows to buffer
	TArray< TSharedRef<SWindow> > OutWindows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(OutWindows);

	static const FName RendererModuleName( "Renderer" );
	IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>( RendererModuleName );

	//if ( false )
	{
	for (int32 i = 0; i < OutWindows.Num(); ++i)
	{
		TSharedPtr<SWindow> WindowPtr = OutWindows[i];
		SWindow* Window = WindowPtr.Get();
		FViewportInfo* ViewportInfo = WindowToViewportInfo.FindChecked(Window);

		const FSlateRect SlateWindowRect = Window->GetRectInScreen();
		const FVector2D WindowSize = SlateWindowRect.GetSize();
		if ( WindowSize.X > 0 && WindowSize.Y > 0 )
		{
			FIntRect ScaledWindowRect;
			ScaledWindowRect.Min.X = SlateWindowRect.Left * XScaling - VirtualScreenPos.X;
			ScaledWindowRect.Max.X = SlateWindowRect.Right * XScaling - VirtualScreenPos.X;
			ScaledWindowRect.Min.Y = SlateWindowRect.Top * YScaling - VirtualScreenPos.Y;
			ScaledWindowRect.Max.Y = SlateWindowRect.Bottom * YScaling - VirtualScreenPos.Y;

			struct FDrawWindowToBufferContext
			{
				FViewportInfo* InViewportInfo;
				FIntRect WindowRect;
				FIntRect IntermediateBufferSize;
				IRendererModule* RendererModule;
			};
			FDrawWindowToBufferContext DrawWindowToBufferContext =
			{
				ViewportInfo,
				ScaledWindowRect,
				VirtualScreen,
				&RendererModule
			};

			// Draw a quad mapping scene color to the view's render target
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				DrawWindowToBuffer,
				FDrawWindowToBufferContext,Context,DrawWindowToBufferContext,
			{
				const auto FeatureLevel = GMaxRHIFeatureLevel;
				auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

				TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
				TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, Context.RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

				if( Context.WindowRect.Width() != Context.InViewportInfo->Width || Context.WindowRect.Height() != Context.InViewportInfo->Height )
				{
					// We're scaling down the window, so use bilinear filtering
					PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), RHICmdList.GetViewportBackBuffer(Context.InViewportInfo->ViewportRHI));
				}
				else
				{
					// Drawing 1:1, so no filtering needed
					PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), RHICmdList.GetViewportBackBuffer(Context.InViewportInfo->ViewportRHI));
				}

				Context.RendererModule->DrawRectangle(
					RHICmdList,
					Context.WindowRect.Min.X, Context.WindowRect.Min.Y,
					Context.WindowRect.Width(), Context.WindowRect.Height(),
					0, 0,
					1, 1,
					FIntPoint(Context.IntermediateBufferSize.Width(), Context.IntermediateBufferSize.Height()),
					FIntPoint(1, 1),
					*VertexShader,
					EDRF_Default);
			});
		}
	}
	}

	// draw mouse cursor and keypresses
	const FVector2D MouseCursorLocation = FSlateApplication::Get().GetCursorPos();
	const FIntPoint ScaledCursorLocation = FIntPoint(MouseCursorLocation.X * XScaling, MouseCursorLocation.Y * YScaling) - VirtualScreenPos;

	FSlateWindowElementList* WindowElementList = CrashTrackerResource->GetNextElementList();
	WindowElementList->ResetBuffers();

	// Don't draw cursor when it is hidden (mouse looking, scrolling, etc.)
	// @todo livestream: The cursor is probably still hidden when dragging with the mouse captured (grabby hand)
	if( FSlateApplication::Get().GetMouseCaptureWindow() == nullptr )
	{
		FSlateDrawElement::MakeBox(
			*WindowElementList,
			0,
			FPaintGeometry(ScaledCursorLocation, FVector2D(32, 32) * XScaling, XScaling),
			FCoreStyle::Get().GetBrush("CrashTracker.Cursor"),
			FSlateRect(0, 0, VirtualScreenSize.X, VirtualScreenSize.Y));
	}
	
	for (int32 i = 0; i < KeypressBuffer.Num(); ++i)
	{
		FSlateDrawElement::MakeText(
			*WindowElementList,
			0,
			FPaintGeometry(FVector2D(10, 10 + i * 30), FVector2D(300, 30), 1.f),
			KeypressBuffer[i],
			FCoreStyle::Get().GetFontStyle(TEXT("CrashTracker.Font")),
			FSlateRect(0, 0, VirtualScreenSize.X, VirtualScreenSize.Y));
	}
	
	ElementBatcher->AddElements(*WindowElementList);
	ElementBatcher->ResetBatches();
	
	struct FWriteMouseCursorAndKeyPressesContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
		FSlateRHIRenderingPolicy* RenderPolicy;
		FSlateWindowElementList* SlateElementList;
		FIntPoint ViewportSize;
	};
	FWriteMouseCursorAndKeyPressesContext WriteMouseCursorAndKeyPressesContext =
	{
		CrashTrackerResource,
		VirtualScreen,
		RenderingPolicy.Get(),
		WindowElementList,
		VirtualScreenSize
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		WriteMouseCursorAndKeyPresses,
		FWriteMouseCursorAndKeyPressesContext, Context, WriteMouseCursorAndKeyPressesContext,
	{
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI());
		
		FSlateBatchData& BatchData = Context.SlateElementList->GetBatchData();
		FElementBatchMap& RootBatchMap = Context.SlateElementList->GetRootDrawLayer().GetElementBatchMap();

		BatchData.CreateRenderBatches(RootBatchMap);

		Context.RenderPolicy->UpdateVertexAndIndexBuffers(RHICmdList, BatchData);
		if( BatchData.GetRenderBatches().Num() > 0 )
		{
			FTexture2DRHIRef UnusedTargetTexture;
			FSlateBackBuffer UnusedTarget( UnusedTargetTexture, FIntPoint::ZeroValue );

			Context.RenderPolicy->DrawElements(RHICmdList, UnusedTarget, CreateProjectionMatrix(Context.ViewportSize.X, Context.ViewportSize.Y), BatchData.GetRenderBatches());
		}
	});

	// copy back to the cpu
	struct FReadbackFromIntermediateBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
	};
	FReadbackFromIntermediateBufferContext ReadbackFromIntermediateBufferContext =
	{
		CrashTrackerResource,
		VirtualScreen
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadbackFromIntermediateBuffer,
		FReadbackFromIntermediateBufferContext,Context,ReadbackFromIntermediateBufferContext,
	{
		RHICmdList.CopyToResolveTarget(
			Context.CrashReportResource->GetBuffer(),
			Context.CrashReportResource->GetReadbackBuffer(),
			false,
			FResolveParams());
	});
}


void FSlateRHIRenderer::MapVirtualScreenBuffer(FMappedTextureBuffer* OutTextureData)
{
	const FIntRect VirtualScreen = CrashTrackerResource->GetVirtualScreen();

	struct FReadbackFromStagingBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FMappedTextureBuffer* TextureData;
		FIntRect ExpectedBufferSize;
	};
	FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
	{
		CrashTrackerResource,
		OutTextureData,
		VirtualScreen,
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadbackFromStagingBuffer,
		FReadbackFromStagingBufferContext,Context,ReadbackFromStagingBufferContext,
	{
		SCOPE_CYCLE_COUNTER(STAT_MapStagingBuffer);
		RHICmdList.MapStagingSurface(Context.CrashReportResource->GetReadbackBuffer(), Context.TextureData->Data, Context.TextureData->Width, Context.TextureData->Height);

		Context.CrashReportResource->SwapTargetReadbackBuffer();
	});
}

void FSlateRHIRenderer::UnmapVirtualScreenBuffer()
{
	struct FReadbackFromStagingBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
	};
	FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
	{
		CrashTrackerResource
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadbackFromStagingBuffer,
		FReadbackFromStagingBufferContext,Context,ReadbackFromStagingBufferContext,
	{
		SCOPE_CYCLE_COUNTER(STAT_UnmapStagingBuffer);
		RHICmdList.UnmapStagingSurface(Context.CrashReportResource->GetReadbackBuffer());
	});
}

FIntPoint FSlateRHIRenderer::GenerateDynamicImageResource(const FName InTextureName)
{
	check( IsInGameThread() );

	uint32 Width = 0;
	uint32 Height = 0;
	TArray<uint8> RawData;

	TSharedPtr<FSlateDynamicTextureResource> TextureResource = ResourceManager->GetDynamicTextureResourceByName( InTextureName );
	if( !TextureResource.IsValid() )
	{
		// Load the image from disk
		bool bSucceeded = ResourceManager->LoadTexture(InTextureName, InTextureName.ToString(), Width, Height, RawData);
		if (bSucceeded)
		{
			TextureResource = ResourceManager->MakeDynamicTextureResource(InTextureName, Width, Height, RawData);
		}
	}

	return TextureResource.IsValid() ? TextureResource->Proxy->ActualSize : FIntPoint( 0, 0 );
}

bool FSlateRHIRenderer::GenerateDynamicImageResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes )
{
	check( IsInGameThread() );

	TSharedPtr<FSlateDynamicTextureResource> TextureResource = ResourceManager->GetDynamicTextureResourceByName( ResourceName );
	if( !TextureResource.IsValid() )
	{
		TextureResource = ResourceManager->MakeDynamicTextureResource( ResourceName, Width, Height, Bytes );
	}
	return TextureResource.IsValid();
}

FSlateResourceHandle FSlateRHIRenderer::GetResourceHandle( const FSlateBrush& Brush )
{
	return ResourceManager->GetResourceHandle( Brush );
}

void FSlateRHIRenderer::RemoveDynamicBrushResource( TSharedPtr<FSlateDynamicImageBrush> BrushToRemove )
{
	if( BrushToRemove.IsValid() )
	{
		DynamicBrushesToRemove[FreeBufferIndex].Add( BrushToRemove );
	}
}

/**
 * Gives the renderer a chance to wait for any render commands to be completed before returning/
 */
void FSlateRHIRenderer::FlushCommands() const
{
	if( IsInGameThread() )
	{
		FlushRenderingCommands();
	}
}

/**
 * Gives the renderer a chance to synchronize with another thread in the event that the renderer runs 
 * in a multi-threaded environment.  This function does not return until the sync is complete
 */
void FSlateRHIRenderer::Sync() const
{
	// Sync game and render thread. Either total sync or allowing one frame lag.
	static FFrameEndSync FrameEndSync;
	static auto CVarAllowOneFrameThreadLag = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OneFrameThreadLag"));
	FrameEndSync.Sync( CVarAllowOneFrameThreadLag->GetValueOnAnyThread() != 0 );
}

void FSlateRHIRenderer::ReloadTextureResources()
{
	ResourceManager->ReloadTextures();
}

void FSlateRHIRenderer::LoadUsedTextures()
{
	if ( ResourceManager.IsValid() )
	{
		ResourceManager->LoadUsedTextures();
	}
}

void FSlateRHIRenderer::LoadStyleResources( const ISlateStyle& Style )
{
	if ( ResourceManager.IsValid() )
	{
		ResourceManager->LoadStyleResources( Style );
	}
}

void FSlateRHIRenderer::ReleaseDynamicResource( const FSlateBrush& InBrush )
{
	ensure( IsInGameThread() );
	ResourceManager->ReleaseDynamicResource( InBrush );
}

void* FSlateRHIRenderer::GetViewportResource( const SWindow& Window )
{
	checkSlow(IsThreadSafeForSlateRendering());

	FViewportInfo** InfoPtr = WindowToViewportInfo.Find( &Window );

	if( InfoPtr )
	{
		FViewportInfo* ViewportInfo = *InfoPtr;

		// Create the viewport if it doesnt exist
		if( !IsValidRef(ViewportInfo->ViewportRHI) )
		{
			// Sanity check dimensions
			checkf(ViewportInfo->Width <= MAX_VIEWPORT_SIZE && ViewportInfo->Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), ViewportInfo->Width, ViewportInfo->Height);

			const bool bFullscreen = IsViewportFullscreen( Window );

			ViewportInfo->ViewportRHI = RHICreateViewport( ViewportInfo->OSWindow, ViewportInfo->Width, ViewportInfo->Height, bFullscreen, ViewportInfo->PixelFormat );
		}

		return &ViewportInfo->ViewportRHI;
	}
	else
	{
		return NULL;
	}
}

void FSlateRHIRenderer::SetColorVisionDeficiencyType( uint32 Type )
{
	GSlateShaderColorVisionDeficiencyType = Type;
}

FSlateUpdatableTexture* FSlateRHIRenderer::CreateUpdatableTexture(uint32 Width, uint32 Height)
{
	const bool bCreateEmptyTexture = true;
	FSlateTexture2DRHIRef* NewTexture = new FSlateTexture2DRHIRef(Width, Height, PF_B8G8R8A8, nullptr, TexCreate_Dynamic, bCreateEmptyTexture);
	if (IsInRenderingThread())
	{
		NewTexture->InitResource();
	}
	else
	{
		BeginInitResource(NewTexture);
	}
	return NewTexture;
}

void FSlateRHIRenderer::ReleaseUpdatableTexture(FSlateUpdatableTexture* Texture)
{
	if (IsInRenderingThread())
	{
		Texture->GetRenderResource()->ReleaseResource();
		delete Texture;
	}
	else
	{
		Texture->Cleanup();
	}
}

ISlateAtlasProvider* FSlateRHIRenderer::GetTextureAtlasProvider()
{
	if( ResourceManager.IsValid() )
	{
		return ResourceManager->GetTextureAtlasProvider();
	}

	return nullptr;
}

bool FSlateRHIRenderer::AreShadersInitialized() const
{
#if WITH_EDITORONLY_DATA
	return IsGlobalShaderMapComplete(TEXT("SlateElement"));
#else
	return true;
#endif
}

void FSlateRHIRenderer::InvalidateAllViewports()
{
	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		It.Value()->ViewportRHI = nullptr;
	}
}

void FSlateRHIRenderer::ReleaseAccessedResources(bool bImmediatelyFlush)
{
	FScopeLock ScopeLock(GetResourceCriticalSection());

	// Clear accessed UTexture and Material objects from the previous frame
	ResourceManager->BeginReleasingAccessedResources(bImmediatelyFlush);

	if ( bImmediatelyFlush )
	{
		FlushCommands();
	}
}

void FSlateRHIRenderer::RequestResize( const TSharedPtr<SWindow>& Window, uint32 NewWidth, uint32 NewHeight )
{
	checkSlow( IsThreadSafeForSlateRendering() );

	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef( Window.Get() );

	if (ViewInfo)
	{
		ViewInfo->DesiredWidth = NewWidth;
		ViewInfo->DesiredHeight = NewHeight;
	}
}

void FSlateRHIRenderer::SetWindowRenderTarget(const SWindow& Window, IViewportRenderTargetProvider* Provider)
{	
	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef(&Window);
	if (ViewInfo)
	{
		ViewInfo->RTProvider = Provider;
	}
}

TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> FSlateRHIRenderer::CacheElementRenderData(const ILayoutCache* Cacher, FSlateWindowElementList& ElementList)
{
	TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> RenderDataHandle = MakeShareable(new FSlateRenderDataHandle(Cacher, ResourceManager.Get()));

	checkSlow(ElementList.GetChildDrawLayers().Num() == 0);

	// Add all elements for this window to the element batcher
	ElementBatcher->AddElements(ElementList);

	// All elements for this window have been batched and rendering data updated
	ElementBatcher->ResetBatches();

	struct FCacheElementBatchesContext
	{
		FSlateRHIRenderingPolicy* RenderPolicy;
		FSlateWindowElementList* SlateElementList;
		TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe> RenderDataHandle;
	};
	FCacheElementBatchesContext CacheElementBatchesContext =
	{
		RenderingPolicy.Get(),
		&ElementList,
		RenderDataHandle,
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		CacheElementBatches,
		FCacheElementBatchesContext, Context, CacheElementBatchesContext,
	{
		FSlateBatchData& BatchData = Context.SlateElementList->GetBatchData();
		FElementBatchMap& RootBatchMap = Context.SlateElementList->GetRootDrawLayer().GetElementBatchMap();

		BatchData.SetRenderDataHandle(Context.RenderDataHandle);
		BatchData.CreateRenderBatches(RootBatchMap);
		Context.RenderPolicy->UpdateVertexAndIndexBuffers(RHICmdList, BatchData, Context.RenderDataHandle);
	});

	return RenderDataHandle;
}

void FSlateRHIRenderer::ReleaseCachingResourcesFor(const ILayoutCache* Cacher)
{
	struct FReleaseCachingResourcesForContext
	{
		FSlateRHIRenderingPolicy* RenderPolicy;
		const ILayoutCache* Cacher;
	};
	FReleaseCachingResourcesForContext MarshalContext =
	{
		RenderingPolicy.Get(),
		Cacher,
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReleaseCachingResourcesFor,
		FReleaseCachingResourcesForContext, Context, MarshalContext,
		{
			Context.RenderPolicy->ReleaseCachingResourcesFor(RHICmdList, Context.Cacher);
		});
}

FSlateEndDrawingWindowsCommand::FSlateEndDrawingWindowsCommand(FSlateRHIRenderingPolicy& InPolicy, FSlateDrawBuffer* InDrawBuffer)
	: Policy(InPolicy)
	, DrawBuffer(InDrawBuffer)
{}

void FSlateEndDrawingWindowsCommand::Execute(FRHICommandListBase& CmdList)
{
	for ( auto& ElementList : DrawBuffer->GetWindowElementLists() )
	{
		ElementList->PostDraw_ParallelThread();
	}

	DrawBuffer->Unlock();
	Policy.EndDrawingWindows();
}

void FSlateEndDrawingWindowsCommand::EndDrawingWindows(FRHICommandListImmediate& RHICmdList, FSlateDrawBuffer* DrawBuffer, FSlateRHIRenderingPolicy& Policy)
{
	if (!RHICmdList.Bypass())
	{
		new (RHICmdList.AllocCommand<FSlateEndDrawingWindowsCommand>()) FSlateEndDrawingWindowsCommand(Policy, DrawBuffer);
	}
	else
	{
		FSlateEndDrawingWindowsCommand Cmd(Policy, DrawBuffer);
		Cmd.Execute(RHICmdList);
	}
}
