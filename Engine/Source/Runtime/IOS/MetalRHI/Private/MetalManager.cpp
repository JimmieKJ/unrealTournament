// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "IOSAppDelegate.h"

const uint32 RingBufferSize = 8 * 1024 * 1024;

#if SHOULD_TRACK_OBJECTS
TMap<id, int32> ClassCounts;
#endif

#define NUMBITS_BLEND_STATE				5 //(x6=30)
#define NUMBITS_RENDER_TARGET_FORMAT	4 //(x6=24)
#define NUMBITS_DEPTH_ENABLED			1 //(x1=1)
#define NUMBITS_STENCIL_ENABLED			1 //(x1=1)
#define NUMBITS_SAMPLE_COUNT			3 //(x1=3)


#define OFFSET_BLEND_STATE0				(0)
#define OFFSET_BLEND_STATE1				(OFFSET_BLEND_STATE0			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE2				(OFFSET_BLEND_STATE1			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE3				(OFFSET_BLEND_STATE2			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE4				(OFFSET_BLEND_STATE3			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE5				(OFFSET_BLEND_STATE4			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT0	(OFFSET_BLEND_STATE5			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT1	(OFFSET_RENDER_TARGET_FORMAT0	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT2	(OFFSET_RENDER_TARGET_FORMAT1	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT3	(OFFSET_RENDER_TARGET_FORMAT2	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT4	(OFFSET_RENDER_TARGET_FORMAT3	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT5	(OFFSET_RENDER_TARGET_FORMAT4	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_DEPTH_ENABLED			(OFFSET_RENDER_TARGET_FORMAT5	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_STENCIL_ENABLED			(OFFSET_DEPTH_ENABLED			+ NUMBITS_DEPTH_ENABLED)
#define OFFSET_SAMPLE_COUNT				(OFFSET_STENCIL_ENABLED			+ NUMBITS_STENCIL_ENABLED)
#define OFFSET_END						(OFFSET_SAMPLE_COUNT			+ NUMBITS_SAMPLE_COUNT)

static_assert(OFFSET_END <= 64, "Too many bits used for the Hash");

static uint32 BlendBitOffsets[] = { OFFSET_BLEND_STATE0, OFFSET_BLEND_STATE1, OFFSET_BLEND_STATE2, OFFSET_BLEND_STATE3, OFFSET_BLEND_STATE4, OFFSET_BLEND_STATE5, };
static uint32 RTBitOffsets[] = { OFFSET_RENDER_TARGET_FORMAT0, OFFSET_RENDER_TARGET_FORMAT1, OFFSET_RENDER_TARGET_FORMAT2, OFFSET_RENDER_TARGET_FORMAT3, OFFSET_RENDER_TARGET_FORMAT4, OFFSET_RENDER_TARGET_FORMAT5, };

#define SET_HASH(Offset, NumBits, Value) \
	{ \
		uint64 BitMask = ((1ULL << NumBits) - 1) << Offset; \
		Pipeline.Hash = (Pipeline.Hash & ~BitMask) | (((uint64)Value << Offset) & BitMask); \
	}
#define GET_HASH(Offset, NumBits) ((Hash >> Offset) & ((1ULL << NumBits) - 1))

/*
void FPipelineShadow::SetHash(uint64 InHash)
{
	Hash = InHash;

	RenderTargets[0] = [[MTLRenderPipelineColorAttachmentDescriptor alloc] init];
	FMetalManager::Get()->ReleaseObject(RenderTargets[0]);
	RenderTargets[0].sourceRGBBlendFactor = (MTLBlendFactor)GET_HASH(OFFSET_SOURCE_RGB_BLEND_FACTOR, NUMBITS_SOURCE_RGB_BLEND_FACTOR);
	RenderTargets[0].destinationRGBBlendFactor = (MTLBlendFactor)GET_HASH(OFFSET_DEST_RGB_BLEND_FACTOR, NUMBITS_DEST_RGB_BLEND_FACTOR);
	RenderTargets[0].rgbBlendOperation = (MTLBlendOperation)GET_HASH(OFFSET_RGB_BLEND_OPERATION, NUMBITS_RGB_BLEND_OPERATION);
	RenderTargets[0].sourceAlphaBlendFactor = (MTLBlendFactor)GET_HASH(OFFSET_SOURCE_A_BLEND_FACTOR, NUMBITS_SOURCE_A_BLEND_FACTOR);
	RenderTargets[0].destinationAlphaBlendFactor = (MTLBlendFactor)GET_HASH(OFFSET_DEST_A_BLEND_FACTOR, NUMBITS_DEST_A_BLEND_FACTOR);
	RenderTargets[0].alphaBlendOperation = (MTLBlendOperation)GET_HASH(OFFSET_A_BLEND_OPERATION, NUMBITS_A_BLEND_OPERATION);
	RenderTargets[0].writeMask = GET_HASH(OFFSET_WRITE_MASK, NUMBITS_WRITE_MASK);
	RenderTargets[0].blendingEnabled =
		RenderTargets[0].rgbBlendOperation != MTLBlendOperationAdd || RenderTargets[0].destinationRGBBlendFactor != MTLBlendFactorZero || RenderTargets[0].sourceRGBBlendFactor != MTLBlendFactorOne ||
		RenderTargets[0].alphaBlendOperation != MTLBlendOperationAdd || RenderTargets[0].destinationAlphaBlendFactor != MTLBlendFactorZero || RenderTargets[0].sourceAlphaBlendFactor != MTLBlendFactorOne;

	RenderTargets[0].pixelFormat = (MTLPixelFormat)GET_HASH(OFFSET_RENDER_TARGET_FORMAT, NUMBITS_RENDER_TARGET_FORMAT);
	DepthTargetFormat = (MTLPixelFormat)GET_HASH(OFFSET_DEPTH_TARGET_FORMAT, NUMBITS_DEPTH_TARGET_FORMAT);
	SampleCount = GET_HASH(OFFSET_SAMPLE_COUNT, NUMBITS_SAMPLE_COUNT);
}
*/



static MTLTriangleFillMode TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch (FillMode)
	{
		case FM_Wireframe:	return MTLTriangleFillModeLines;
		case FM_Point:		return MTLTriangleFillModeFill;
		default:			return MTLTriangleFillModeFill;
	};
}

static MTLCullMode TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch (CullMode)
	{
		case CM_CCW:	return MTLCullModeFront;
		case CM_CW:		return MTLCullModeBack;
		default:		return MTLCullModeNone;
	}
}


id<MTLRenderPipelineState> FPipelineShadow::CreatePipelineStateForBoundShaderState(FMetalBoundShaderState* BSS) const
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPipelineStateTime);
	
	MTLRenderPipelineDescriptor* Desc = [[MTLRenderPipelineDescriptor alloc] init];

	// set per-MRT settings
	for (uint32 RenderTargetIndex = 0; RenderTargetIndex < MaxMetalRenderTargets; ++RenderTargetIndex)
	{
		[Desc.colorAttachments setObject:RenderTargets[RenderTargetIndex] atIndexedSubscript:RenderTargetIndex];
	}

	// depth setting if it's actually used
	Desc.depthAttachmentPixelFormat = DepthTargetFormat;
	Desc.stencilAttachmentPixelFormat = StencilTargetFormat;

	// set the bound shader state settings
	Desc.vertexDescriptor = BSS->VertexDeclaration->Layout;
	Desc.vertexFunction = BSS->VertexShader->Function;
	Desc.fragmentFunction = BSS->PixelShader ? BSS->PixelShader->Function : nil;
	Desc.sampleCount = SampleCount;

	check(SampleCount > 0);

	NSError* Error = nil;
	id<MTLRenderPipelineState> PipelineState = [FMetalManager::GetDevice() newRenderPipelineStateWithDescriptor:Desc error : &Error];
	TRACK_OBJECT(PipelineState);

	if (PipelineState == nil)
	{
		NSLog(@"Failed to generate a pipeline state object: %@", Error);
		NSLog(@"Vertex shader: %@", BSS->VertexShader->GlslCodeNSString);
		NSLog(@"Pixel shader: %@", BSS->PixelShader->GlslCodeNSString);
		NSLog(@"Descriptor: %@", Desc);
		return nil;
	}

	[Desc release];

	return PipelineState;
}

FMetalManager* FMetalManager::Get()
{
	static FMetalManager Singleton;
	return &Singleton;
}

id<MTLDevice> FMetalManager::GetDevice()
{
	return FMetalManager::Get()->Device;
}

id<MTLRenderCommandEncoder> FMetalManager::GetContext()
{
	return FMetalManager::Get()->CurrentContext;
}

id<MTLComputeCommandEncoder> FMetalManager::GetComputeContext()
{
	id<MTLComputeCommandEncoder> Encoder = FMetalManager::Get()->CurrentComputeContext;
	checkf(Encoder != nil, TEXT("Attempted to use GetComputeContext before calling FMetalManager::ConditionalSwitchToCompute() to create the encoder"));
	return Encoder;
}

void FMetalManager::ReleaseObject(id Object)
{
	FMetalManager::Get()->DelayedFreeLists[FMetalManager::Get()->WhichFreeList].Add(Object);
}

 
FMetalManager::FMetalManager()
	: Device([IOSAppDelegate GetDelegate].IOSView->MetalDevice)
	, CurrentCommandBuffer(nil)
	, CurrentDrawable(nil)
	, CurrentContext(nil)
	, CurrentComputeContext(nil)
	, CurrentNumRenderTargets(0)
	, PreviousNumRenderTargets(0)
	, CurrentDepthRenderTexture(nil)
	, PreviousDepthRenderTexture(nil)
	, CurrentStencilRenderTexture(nil)
	, PreviousStencilRenderTexture(nil)
	, CurrentMSAARenderTexture(nil)
	, RingBuffer(Device, RingBufferSize, BufferOffsetAlignment)
	, QueryBuffer(Device, 64 * 1024, 8)
	, WhichFreeList(0)
	, CurrentQueryEventIndex(-1)
	, SceneFrameCounter(0)
	, ResourceTableFrameCounter(INDEX_NONE)
{
	for (int32 Index = 0; Index < ARRAY_COUNT(CurrentColorRenderTextures); Index++)
	{
		CurrentColorRenderTextures[Index] = nil;
		PreviousColorRenderTextures[Index] = nil;
	}
	FMemory::Memzero(CurrentRenderTargetsViewInfo);
	FMemory::Memzero(PreviousRenderTargetsViewInfo);
	FMemory::Memzero(CurrentDepthViewInfo);
	FMemory::Memzero(PreviousDepthViewInfo);
	FMemory::Memzero(CurrentStencilViewInfo);
	FMemory::Memzero(PreviousStencilViewInfo);

	CommandQueue = [Device newCommandQueue];

	// get the size of the window
	CGRect ViewFrame = [[IOSAppDelegate GetDelegate].IOSView frame];
	FRHIResourceCreateInfo CreateInfo;

	float ScalingFactor = [[IOSAppDelegate GetDelegate].IOSView contentScaleFactor];
	BackBuffer = (FMetalTexture2D*)(FTexture2DRHIParamRef)RHICreateTexture2D(ViewFrame.size.width*ScalingFactor, ViewFrame.size.height*ScalingFactor, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable | TexCreate_Presentable, CreateInfo);

//@todo-rco: What Size???
	// make a buffer for each shader type
	ShaderParameters = new FMetalShaderParameterCache[CrossCompiler::NUM_SHADER_STAGES];
	ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].InitializeResources(1024*1024);
	ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].InitializeResources(1024*1024);

	// create a semaphore for multi-buffering the command buffer
	CommandBufferSemaphore = dispatch_semaphore_create(FParse::Param(FCommandLine::Get(),TEXT("gpulockstep")) ? 1 : 3);

	AutoReleasePoolTLSSlot = FPlatformTLS::AllocTlsSlot();
	
	// Hook into the ios framepacer, if it's enabled for this platform.
	FrameReadyEvent = NULL;
	if( FIOSPlatformRHIFramePacer::IsEnabled() )
	{
		FrameReadyEvent = FPlatformProcess::GetSynchEventFromPool();
		FIOSPlatformRHIFramePacer::InitWithEvent( FrameReadyEvent );
	}
	
	BoundRenderTargetDimensions = FIntPoint(0,0);
	
	InitFrame();
}

void FMetalManager::CreateAutoreleasePool()
{
	if (FPlatformTLS::GetTlsValue(AutoReleasePoolTLSSlot) == NULL)
	{
		FPlatformTLS::SetTlsValue(AutoReleasePoolTLSSlot, FPlatformMisc::CreateAutoreleasePool());
	}
}

void FMetalManager::DrainAutoreleasePool()
{
	FPlatformMisc::ReleaseAutoreleasePool(FPlatformTLS::GetTlsValue(AutoReleasePoolTLSSlot));
	FPlatformTLS::SetTlsValue(AutoReleasePoolTLSSlot, NULL);
}

void FMetalManager::BeginScene()
{
	// Increment the frame counter. INDEX_NONE is a special value meaning "uninitialized", so if
	// we hit it just wrap around to zero.
	SceneFrameCounter++;
	if (SceneFrameCounter == INDEX_NONE)
	{
		SceneFrameCounter++;
	}

	static auto* ResourceTableCachingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("rhi.ResourceTableCaching"));
	if (ResourceTableCachingCvar == NULL || ResourceTableCachingCvar->GetValueOnAnyThread() == 1)
	{
		ResourceTableFrameCounter = SceneFrameCounter;
	}
}

void FMetalManager::EndScene()
{
	ResourceTableFrameCounter = INDEX_NONE;
}

void FMetalManager::BeginFrame()
{
}

void FMetalManager::InitFrame()
{
	// reset the event index for this frame
	CurrentQueryEventIndex = -1;

	// start an auto release pool (EndFrame will drain and remake)
	CreateAutoreleasePool();

	// create the command buffer for this frame
	CreateCurrentCommandBuffer(true);

	//	double Start = FPlatformTime::Seconds();
	// block on the semaphore
//	double Mid = FPlatformTime::Seconds();

	// mark us to get later
	BackBuffer->Surface.Texture = nil;

	// make sure first SetRenderTarget goes through
	PreviousRenderTargetsInfo.NumColorRenderTargets = -1;

//	double End = FPlatformTime::Seconds();
//	NSLog(@"Semaphore Block Time: %.2f   -- MakeDrawable Time: %.2f", 1000.0f*(Mid - Start), 1000.0f*(End - Mid));
	

	extern void InitFrame_UniformBufferPoolCleanup();
	InitFrame_UniformBufferPoolCleanup();

	NumDrawCalls = 0;
}

void FMetalManager::CreateCurrentCommandBuffer(bool bWait)
{
	if (bWait)
	{
		dispatch_semaphore_wait(CommandBufferSemaphore, DISPATCH_TIME_FOREVER);
	}
	
	CurrentCommandBuffer = [CommandQueue commandBufferWithUnretainedReferences];
	[CurrentCommandBuffer retain];
	TRACK_OBJECT(CurrentCommandBuffer);

	// move to the next event index
	CurrentQueryEventIndex++;

	FEvent* LocalEvent;
	// make a new event if we don't have enough
	if (CurrentQueryEventIndex >= QueryEvents[WhichFreeList].Num())
	{
		LocalEvent = FPlatformProcess::GetSynchEventFromPool(true);
		CurrentQueryEventIndex = QueryEvents[WhichFreeList].Add(LocalEvent);
	}
	// otherwise, reuse an old one
	else
	{
		LocalEvent = QueryEvents[WhichFreeList][CurrentQueryEventIndex];
	}

	// unset the event
	LocalEvent->Reset();
	[CurrentCommandBuffer addCompletedHandler:^(id <MTLCommandBuffer> Buffer)
	{
		// when the command buffer is done, release the event
		LocalEvent->Trigger();
	}];
}

void FMetalManager::SubmitCommandBufferAndWait()
{
	[CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
	 {
		dispatch_semaphore_signal(CommandBufferSemaphore);
	 }];
	
	// commit the render context to the commandBuffer
	[CurrentContext endEncoding];
	[CurrentContext release];
	CurrentContext = nil;

	// kick the whole buffer
	// Commit to hand the commandbuffer off to the gpu
	[CurrentCommandBuffer commit];

	// wait for the gpu to finish executing our commands.
	[CurrentCommandBuffer waitUntilCompleted];
	
	//once a commandbuffer is commited it can't be added to again.
	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];
	
	// create a new command buffer.
	CreateCurrentCommandBuffer(true);
}

void FMetalManager::SubmitComputeCommandBufferAndWait()
{
}

void FMetalManager::EndFrame(bool bPresent)
{
//	NSLog(@"There were %d draw calls for final RT in frame %lld", NumDrawCalls, GFrameCounter);
	// commit the render context to the commandBuffer
	if (CurrentContext)
	{
		[CurrentContext endEncoding];
		[CurrentContext release];
		CurrentContext = nil;
	}
	if (CurrentComputeContext)
	{
		[CurrentComputeContext endEncoding];
		[CurrentComputeContext release];
		CurrentComputeContext = nil;
	}

	// kick the whole buffer
	[CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
	 {
		dispatch_semaphore_signal(CommandBufferSemaphore);
	 }];
	
	// We may be limiting our framerate to the display link
	if( FrameReadyEvent != nullptr )
	{
		FrameReadyEvent->Wait();
	}

	// Commit before waiting to avoid leaving the gpu idle
	[CurrentCommandBuffer commit];
	
	// enqueue a present if desired
	if (CurrentDrawable)
	{
		if (bPresent)
		{
			[CurrentCommandBuffer waitUntilScheduled];
			
			[CurrentDrawable present];
		}

		CurrentDrawable = nil;
	}

	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];

	// xcode helper function
	[CommandQueue insertDebugCaptureBoundary];

	// drain the oldest delayed free list
	uint32 PrevFreeList = (WhichFreeList + 1) % ARRAY_COUNT(DelayedFreeLists);
	for (int32 Index = 0; Index < DelayedFreeLists[PrevFreeList].Num(); Index++)
	{
		id Object = DelayedFreeLists[PrevFreeList][Index];
		UNTRACK_OBJECT(Object);
		[Object release];
	}
	DelayedFreeLists[PrevFreeList].Reset();

	// and switch to it
	WhichFreeList = PrevFreeList;

#if SHOULD_TRACK_OBJECTS
	// print out outstanding objects
	if ((GFrameCounter % 500) == 10)
	{
		for (auto It = ClassCounts.CreateIterator(); It; ++It)
		{
			NSLog(@"%@ has %d outstanding allocations", It.Key(), It.Value());
		}
	}
#endif

	// drain the pool
	DrainAutoreleasePool();

	InitFrame();
}


FMetalTexture2D* FMetalManager::GetBackBuffer()
{
	return BackBuffer;
}

void FMetalManager::PrepareToDraw(uint32 NumVertices)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPrepareDrawTime);

	NumDrawCalls++;

	// make sure the BSS has a valid pipeline state object
	CurrentBoundShaderState->PrepareToDraw(Pipeline);
	
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
}

void FMetalManager::SetBlendState(FMetalBlendState* BlendState)
{
	for(uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxMetalRenderTargets; ++RenderTargetIndex)
	{
		MTLRenderPipelineColorAttachmentDescriptor* Blend = BlendState->RenderTargetStates[RenderTargetIndex].BlendState;
		MTLRenderPipelineColorAttachmentDescriptor* Dest = Pipeline.RenderTargets[RenderTargetIndex];

		// assign each property manually, would be nice if this was faster
		Dest.blendingEnabled = Blend.blendingEnabled;
		Dest.sourceRGBBlendFactor = Blend.sourceRGBBlendFactor;
		Dest.destinationRGBBlendFactor = Blend.destinationRGBBlendFactor;
		Dest.rgbBlendOperation = Blend.rgbBlendOperation;
		Dest.sourceAlphaBlendFactor = Blend.sourceAlphaBlendFactor;
		Dest.destinationAlphaBlendFactor = Blend.destinationAlphaBlendFactor;
		Dest.alphaBlendOperation = Blend.alphaBlendOperation;
		Dest.writeMask = Blend.writeMask;

		// set the hash bits for this RT
		SET_HASH(BlendBitOffsets[RenderTargetIndex], NUMBITS_BLEND_STATE, BlendState->RenderTargetStates[RenderTargetIndex].BlendStateKey);
	}
}

void FMetalManager::SetBoundShaderState(FMetalBoundShaderState* BoundShaderState)
{
#if NO_DRAW
	return;
#endif
	CurrentBoundShaderState = BoundShaderState;
}



void FMetalManager::ConditionalUpdateBackBuffer(FMetalSurface& Surface)
{
	// are we setting the back buffer? if so, make sure we have the drawable
	if (&Surface == &BackBuffer->Surface)
	{
		// update the back buffer texture the first time used this frame
		if (Surface.Texture == nil && CurrentDrawable == nil)
		{
			SCOPE_CYCLE_COUNTER(STAT_MetalMakeDrawableTime);

			uint32 IdleStart = FPlatformTime::Cycles();

			// make a drawable object for this frame
			CurrentDrawable = [[IOSAppDelegate GetDelegate].IOSView MakeDrawable];

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;

			// set the texture into the backbuffer
            if (CurrentDrawable != nil)
            {
                Surface.Texture = CurrentDrawable.texture;
            }
            else
            {
                Surface.Texture = nil;
            }
		}
	}
}

bool FMetalManager::NeedsToSetRenderTarget(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	// see if our new Info matches our previous Info

	// basic checks
	bool bAllChecksPassed = RenderTargetsInfo.NumColorRenderTargets == PreviousRenderTargetsInfo.NumColorRenderTargets &&
		// handle the case where going from backbuffer + depth -> backbuffer + null, no need to reset RT and do a store/load
		(RenderTargetsInfo.DepthStencilRenderTarget.Texture == PreviousRenderTargetsInfo.DepthStencilRenderTarget.Texture || RenderTargetsInfo.DepthStencilRenderTarget.Texture == nullptr);

	// now check each color target if the basic tests passe
	if (bAllChecksPassed)
	{
		for (int32 RenderTargetIndex = 0; RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets; RenderTargetIndex++)
		{
			const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
			const FRHIRenderTargetView& PreviousRenderTargetView = PreviousRenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];

			// handle simple case of switching textures or mip/slice
			if (RenderTargetView.Texture != PreviousRenderTargetView.Texture ||
				RenderTargetView.MipIndex != PreviousRenderTargetView.MipIndex ||
				RenderTargetView.ArraySliceIndex != PreviousRenderTargetView.ArraySliceIndex)
			{
				bAllChecksPassed = false;
				break;
			}
			
			// it's non-trivial when we need to switch based on load/store action:
			// LoadAction - it only matters what we are switching to in the new one
			//    If we switch to Load, no need to switch as we can re-use what we already have
			//    If we switch to Clear, we have to always switch to a new RT to force the clear
			//    If we switch to DontCare, there's definitely no need to switch
			if (RenderTargetView.LoadAction == ERenderTargetLoadAction::EClear)
			{
				bAllChecksPassed = false;
				break;
			}
			// StoreAction - this matters what the previous one was **In Spirit**
			//    If we come from Store, we need to switch to a new RT to force the store
			//    If we come from DontCare, then there's no need to switch
			//    @todo metal: However, we basically only use Store now, and don't
			//        care about intermediate results, only final, so we don't currently check the value
//			if (PreviousRenderTargetView.StoreAction == ERenderTTargetStoreAction::EStore)
//			{
//				bAllChecksPassed = false;
//				break;
//			}
		}
	}

	// if we are setting them to nothing, then this is probably end of frame, and we can't make a framebuffer
	// with nothng, so just abort this (only need to check on single MRT case)
	if (RenderTargetsInfo.NumColorRenderTargets == 1 && RenderTargetsInfo.ColorRenderTarget[0].Texture == nullptr &&
		RenderTargetsInfo.DepthStencilRenderTarget.Texture == nullptr)
	{
		bAllChecksPassed = true;
	}

	return bAllChecksPassed == false;
}

void FMetalManager::SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	ConditionalSwitchToGraphics();
	
	// see if our new Info matches our previous Info
	if (NeedsToSetRenderTarget(RenderTargetsInfo) == false)
	{
		return;
	}

	// commit pending commands on the old render target
	if (CurrentContext)
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		//		if (NumDrawCalls == 0)
		//		{
		//			NSLog(@"There were %d draw calls for an RT in frame %lld", NumDrawCalls, GFrameCounter);
		//		}
#endif
        
		// Check if CurrentCommandBuffer was rendering to the BackBuffer.
		if( PreviousRenderTargetsInfo.NumColorRenderTargets == 1 )
		{
			const FRHIRenderTargetView& RenderTargetView = PreviousRenderTargetsInfo.ColorRenderTarget[0];
			FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);
			if(&Surface == &BackBuffer->Surface && CurrentDrawable != nil)
			{
                // release our record of it to ensure we are allocated a new drawable.
				CurrentDrawable = nil;
				BackBuffer->Surface.Texture = nil;
			}
		}
	}
	
	// back this up for next frame
	PreviousRenderTargetsInfo = RenderTargetsInfo;

	FIntPoint MaxDimensions(TNumericLimits<decltype(FIntPoint::X)>::Max(),TNumericLimits<decltype(FIntPoint::Y)>::Max());
	// at this point, we need to fully set up an encoder/command buffer, so make a new one (autoreleased)
	MTLRenderPassDescriptor* RenderPass = [MTLRenderPassDescriptor renderPassDescriptor];

	// if we need to do queries, write to the ring buffer (we set the offset into the ring buffer per query)
	RenderPass.visibilityResultBuffer = QueryBuffer.Buffer;

	// default to non-msaa
    int32 OldCount = Pipeline.SampleCount;
	Pipeline.SampleCount = 0;

	for (uint32 RenderTargetIndex = 0; RenderTargetIndex < MaxMetalRenderTargets; RenderTargetIndex++)
	{
		// default to invalid
		uint8 FormatKey = 0;
		// only try to set it if it was one that was set (ie less than CurrentNumRenderTargets)
		if (RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets && RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex].Texture != nullptr)
		{
			const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
			FMetalSurface& Surface = *GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);
			FormatKey = Surface.FormatKey;
		
			MaxDimensions.X = FMath::Min((uint32)MaxDimensions.X, Surface.SizeX);
			MaxDimensions.Y = FMath::Min((uint32)MaxDimensions.Y, Surface.SizeY);

			// if this is the back buffer, make sure we have a usable drawable
			ConditionalUpdateBackBuffer(Surface);
            
            if (Surface.Texture == nil)
            {
                Pipeline.SampleCount = OldCount;
                return;
            }

			// user code generally passes -1 as a default, but we need 0
			uint32 ArraySliceIndex = RenderTargetView.ArraySliceIndex == 0xFFFFFFFF ? 0 : RenderTargetView.ArraySliceIndex;
			if (Surface.bIsCubemap)
			{
				ArraySliceIndex = GetMetalCubeFace((ECubeFace)ArraySliceIndex);
			}

			MTLRenderPassColorAttachmentDescriptor* ColorAttachment = [[MTLRenderPassColorAttachmentDescriptor alloc] init];

			if (Surface.MSAATexture != nil)
			{
				// set up an MSAA attachment
				ColorAttachment.texture = Surface.MSAATexture;
				ColorAttachment.storeAction = MTLStoreActionMultisampleResolve;
				ColorAttachment.resolveTexture = Surface.Texture;
				Pipeline.SampleCount = Surface.MSAATexture.sampleCount;

				// only allow one MRT with msaa
				checkf(RenderTargetsInfo.NumColorRenderTargets == 1, TEXT("Only expected one MRT when using MSAA"));
			}
			else
			{
				// set up non-MSAA attachment
				ColorAttachment.texture = Surface.Texture;
				ColorAttachment.storeAction = GetMetalRTStoreAction(RenderTargetView.StoreAction);
				Pipeline.SampleCount = 1;
			}
			ColorAttachment.level = RenderTargetView.MipIndex;
			ColorAttachment.slice = ArraySliceIndex;
			ColorAttachment.loadAction = GetMetalRTLoadAction(RenderTargetView.LoadAction);
			const FLinearColor& ClearColor = RenderTargetsInfo.ClearColors[RenderTargetIndex];
			ColorAttachment.clearColor = MTLClearColorMake(ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A);

			// assign the attachment to the slot
			[RenderPass.colorAttachments setObject:ColorAttachment atIndexedSubscript:RenderTargetIndex];

			Pipeline.RenderTargets[RenderTargetIndex].pixelFormat = ColorAttachment.texture.pixelFormat;

			[ColorAttachment release];
		}
		else
		{
			Pipeline.RenderTargets[RenderTargetIndex].pixelFormat = MTLPixelFormatInvalid;
		}

		// update the hash no matter what case (null, unused, used)
		SET_HASH(RTBitOffsets[RenderTargetIndex], NUMBITS_RENDER_TARGET_FORMAT, FormatKey);
	}

	// default to invalid
	Pipeline.DepthTargetFormat = MTLPixelFormatInvalid;
	Pipeline.StencilTargetFormat = MTLPixelFormatInvalid;

	// setup depth and/or stencil
	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture != nullptr)
	{
		FMetalSurface& Surface= *GetMetalSurfaceFromRHITexture(RenderTargetsInfo.DepthStencilRenderTarget.Texture);
		MaxDimensions.X = FMath::Min((uint32)MaxDimensions.X, Surface.SizeX);
		MaxDimensions.Y = FMath::Min((uint32)MaxDimensions.Y, Surface.SizeY);

		if (Surface.Texture != nil)
		{
			MTLRenderPassDepthAttachmentDescriptor* DepthAttachment = [[MTLRenderPassDepthAttachmentDescriptor alloc] init];

			// set up the depth attachment
			DepthAttachment.texture = Surface.Texture;
			DepthAttachment.loadAction = GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction);
			DepthAttachment.storeAction = GetMetalRTStoreAction(RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction);
			DepthAttachment.clearDepth = RenderTargetsInfo.DepthClearValue;

			Pipeline.DepthTargetFormat = DepthAttachment.texture.pixelFormat;
			if (Pipeline.SampleCount == 0)
			{
				Pipeline.SampleCount = DepthAttachment.texture.sampleCount;
			}

			// and assign it
			RenderPass.depthAttachment = DepthAttachment;
			[DepthAttachment release];
		}

		if (Surface.StencilTexture != nil)
		{
			MTLRenderPassStencilAttachmentDescriptor* StencilAttachment = [[MTLRenderPassStencilAttachmentDescriptor alloc] init];

			// set up the stencil attachment
			StencilAttachment.texture = Surface.StencilTexture;
			StencilAttachment.loadAction = GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction);
			StencilAttachment.storeAction = GetMetalRTStoreAction(RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction());
			StencilAttachment.clearStencil = RenderTargetsInfo.StencilClearValue;

			Pipeline.StencilTargetFormat = StencilAttachment.texture.pixelFormat;
			if (Pipeline.SampleCount == 0)
			{
				Pipeline.SampleCount = StencilAttachment.texture.sampleCount;
			}

			// and assign it
			RenderPass.stencilAttachment = StencilAttachment;
			[StencilAttachment release];
		}
	}

	check( MaxDimensions.X != TNumericLimits<decltype(FIntPoint::X)>::Max() &&
		   MaxDimensions.Y != TNumericLimits<decltype(FIntPoint::Y)>::Max());
	
	BoundRenderTargetDimensions = MaxDimensions;

	// update hash for the depth buffer
	SET_HASH(OFFSET_DEPTH_ENABLED, NUMBITS_DEPTH_ENABLED, (Pipeline.DepthTargetFormat == MTLPixelFormatInvalid ? 0 : 1));
	SET_HASH(OFFSET_STENCIL_ENABLED, NUMBITS_STENCIL_ENABLED, (Pipeline.StencilTargetFormat == MTLPixelFormatInvalid ? 0 : 1));
	SET_HASH(OFFSET_SAMPLE_COUNT, NUMBITS_SAMPLE_COUNT, Pipeline.SampleCount);

    // commit pending commands on the old render target
    if (CurrentContext)
    {
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
        //		if (NumDrawCalls == 0)
        //		{
        //			NSLog(@"There were %d draw calls for an RT in frame %lld", NumDrawCalls, GFrameCounter);
        //		}
#endif
        
        [CurrentContext endEncoding];
        NumDrawCalls = 0;
        
        UNTRACK_OBJECT(CurrentContext);
        [CurrentContext release];
        CurrentContext = nil;
        
        // commit the buffer for this context
        [CurrentCommandBuffer commit];
        UNTRACK_OBJECT(CurrentCommandBuffer);
        [CurrentCommandBuffer release];
        
        // create the command buffer for this frame
        CreateCurrentCommandBuffer(false);
    }

    // make a new render context to use to render to the framebuffer
	CurrentContext = [CurrentCommandBuffer renderCommandEncoderWithDescriptor:RenderPass];
	[CurrentContext retain];
	TRACK_OBJECT(CurrentContext);

	// make sure the rasterizer state is set the first time for each new encoder
	bFirstRasterizerState = true;
}

uint32 FRingBuffer::Allocate(uint32 Size, uint32 Alignment)
{
	if (Alignment == 0)
	{
		Alignment = DefaultAlignment;
	}

	// align the offset
	Offset = Align(Offset, Alignment);
	
	// wrap if needed
	if (Offset + Size > Buffer.length)
	{
// 		NSLog(@"Wrapping at frame %lld [size = %d]", GFrameCounter, (uint32)Buffer.length);
		Offset = 0;
	}
	
	// get current location
	uint32 ReturnOffset = Offset;
	
	// allocate
	Offset += Size;
	
	return ReturnOffset;
}

uint32 FMetalManager::AllocateFromRingBuffer(uint32 Size, uint32 Alignment)
{
	return RingBuffer.Allocate(Size, Alignment);
}

uint32 FMetalManager::AllocateFromQueryBuffer()
{
	return QueryBuffer.Allocate(8, 0);
}




FORCEINLINE void SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSurface* RESTRICT Surface)
{
//	check(Surface->Texture != nil);
	if (Surface != nullptr)
	{
		if (ShaderStage == CrossCompiler::SHADER_STAGE_PIXEL)
		{
			[FMetalManager::GetContext() setFragmentTexture:Surface->Texture atIndex:BindIndex];
		}
		else
		{
			[FMetalManager::GetContext() setVertexTexture:Surface->Texture atIndex : BindIndex];
		}
	}
	else
	{
		[FMetalManager::GetContext() setVertexTexture:nil atIndex : BindIndex];
	}
}

FORCEINLINE void SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSamplerState* RESTRICT SamplerState)
{
	check(SamplerState->State != nil);
	if (ShaderStage == CrossCompiler::SHADER_STAGE_PIXEL)
	{
		[FMetalManager::GetContext() setFragmentSamplerState:SamplerState->State atIndex:BindIndex];
	}
	else
	{
		[FMetalManager::GetContext() setVertexSamplerState:SamplerState->State atIndex:BindIndex];
	}
}

template <typename MetalResourceType>
inline int32 SetShaderResourcesFromBuffer(uint32 ShaderStage, FMetalUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
{
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			// todo: could coalesce adjacent bound resources.
			MetalResourceType* ResourcePtr = (MetalResourceType*)Buffer->RawResourceTable[ResourceIndex];
			SetResource(ShaderStage, BindIndex, ResourcePtr);

			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}

	return NumSetCalls;
}

template <class ShaderType>
void SetResourcesFromTables(ShaderType Shader, uint32 ShaderStage, uint32 ResourceTableFrameCounter)
{
	checkSlow(Shader);

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->Bindings.ShaderResourceTable.ResourceTableBits & Shader->DirtyUniformBuffers;
	uint32 NumSetCalls = 0;
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FMetalUniformBuffer* Buffer = (FMetalUniformBuffer*)Shader->BoundUniformBuffers[BufferIndex].GetReference();
		check(Buffer);
		check(BufferIndex < Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes.Num());
		check(Buffer->GetLayout().GetHash() == Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);
		Buffer->CacheResources(ResourceTableFrameCounter);

		// todo: could make this two pass: gather then set
//		NumSetCalls += SetShaderResourcesFromBuffer<FMetalSurface>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex);
		NumSetCalls += SetShaderResourcesFromBuffer<FMetalSurface>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.TextureMap.GetData(), BufferIndex);
		NumSetCalls += SetShaderResourcesFromBuffer<FMetalSamplerState>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.SamplerMap.GetData(), BufferIndex);
	}
	Shader->DirtyUniformBuffers = 0;
//	SetTextureInTableCalls += NumSetCalls;
}

void FMetalManager::CommitGraphicsResourceTables()
{
	uint32 Start = FPlatformTime::Cycles();

	check(CurrentBoundShaderState);

	SetResourcesFromTables(CurrentBoundShaderState->VertexShader, CrossCompiler::SHADER_STAGE_VERTEX, ResourceTableFrameCounter);
	if (IsValidRef(CurrentBoundShaderState->PixelShader))
	{
		SetResourcesFromTables(CurrentBoundShaderState->PixelShader, CrossCompiler::SHADER_STAGE_PIXEL, ResourceTableFrameCounter);
	}

//	CommitResourceTableCycles += (FPlatformTime::Cycles() - Start);
}

void FMetalManager::CommitNonComputeShaderConstants()
{
	ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].CommitPackedUniformBuffers(CurrentBoundShaderState, CrossCompiler::SHADER_STAGE_VERTEX, CurrentBoundShaderState->VertexShader->BoundUniformBuffers, CurrentBoundShaderState->VertexShader->UniformBuffersCopyInfo);
	ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].CommitPackedGlobals(CrossCompiler::SHADER_STAGE_VERTEX, CurrentBoundShaderState->VertexShader->Bindings);
	
	if (IsValidRef(CurrentBoundShaderState->PixelShader))
	{
		ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].CommitPackedUniformBuffers(CurrentBoundShaderState, CrossCompiler::SHADER_STAGE_PIXEL, CurrentBoundShaderState->PixelShader->BoundUniformBuffers, CurrentBoundShaderState->PixelShader->UniformBuffersCopyInfo);
		ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].CommitPackedGlobals(CrossCompiler::SHADER_STAGE_PIXEL, CurrentBoundShaderState->PixelShader->Bindings);
	}
}

void FMetalManager::SetRasterizerState(const FRasterizerStateInitializerRHI& State)
{
	if (bFirstRasterizerState)
	{
		[CurrentContext setFrontFacingWinding:MTLWindingCounterClockwise];
	}

	if (bFirstRasterizerState || ShadowRasterizerState.CullMode != State.CullMode)
	{
		[CurrentContext setCullMode:TranslateCullMode(State.CullMode)];
		ShadowRasterizerState.CullMode = State.CullMode;
	}

	if (bFirstRasterizerState || ShadowRasterizerState.DepthBias != State.DepthBias || ShadowRasterizerState.SlopeScaleDepthBias != State.SlopeScaleDepthBias)
	{
		// no clamping
		[CurrentContext setDepthBias:State.DepthBias slopeScale:State.SlopeScaleDepthBias clamp:FLT_MAX];
		ShadowRasterizerState.DepthBias = State.DepthBias;
		ShadowRasterizerState.SlopeScaleDepthBias = State.SlopeScaleDepthBias;
	}

	// @todo metal: Would we ever need this in a shipping app?
#if !UE_BUILD_SHIPPING
	if (bFirstRasterizerState || ShadowRasterizerState.FillMode != State.FillMode)
	{
		[CurrentContext setTriangleFillMode:TranslateFillMode(State.FillMode)];
		ShadowRasterizerState.FillMode = State.FillMode;
	}
#endif

	bFirstRasterizerState = false;
}


void FMetalManager::ConditionalSwitchToGraphics()
{
	// were we in compute mode?
	if (CurrentComputeContext != nil)
	{
		// stop the compute encoding and cleanup up
		[CurrentComputeContext endEncoding];
		[CurrentComputeContext release];
		CurrentComputeContext = nil;
	
		// we can't start graphics encoding until a new SetRenderTargetsInfo is called because it needs the whole render pass
		// we could cache the render pass if we want to support going back to previous render targets
		// we will catch it by the fact that CurrentContext will be nil until we call SetRenderTargetsInfo
	}
}

void FMetalManager::ConditionalSwitchToCompute()
{
	// if we were in graphics mode, stop the encoding and start compute
	if (CurrentContext != nil)
	{
		// stop encoding graphics and clean up
		[CurrentContext endEncoding];
		[CurrentContext release];
		CurrentContext = nil;
		
		// make sure that the next SetRenderTargetInfos will definitely set a new RT (if we SetRT to X, switched to compute, then back to X,
		// the code would think it was already set and skip it, so this will make it so the next SetRT is 'different')
		PreviousRenderTargetsInfo.NumColorRenderTargets = 0;

		// start encoding for compute
		CurrentComputeContext = [CurrentCommandBuffer computeCommandEncoder];
		[CurrentComputeContext retain];
	}
	
}

void FMetalManager::SetComputeShader(FMetalComputeShader* InComputeShader)
{
	// active compute mode
	ConditionalSwitchToCompute();
	
	// cache this for Dispatch
	CurrentComputeShader = InComputeShader;
	
	// set this compute shader pipeline as the current (this resets all state, so we need to set all resources after calling this)
	[CurrentComputeContext setComputePipelineState:CurrentComputeShader->Kernel];
}

void FMetalManager::Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	check(CurrentComputeShader);
	auto* ComputeShader = (FMetalComputeShader*)CurrentComputeShader;
	MTLSize ThreadgroupCounts = MTLSizeMake(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
	MTLSize Threadgroups = MTLSizeMake(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	//@todo-rco: setThreadgroupMemoryLength?
	[CurrentComputeContext dispatchThreadgroups:Threadgroups threadsPerThreadgroup:ThreadgroupCounts];
}

void FMetalManager::ResizeBackBuffer(uint32 InSizeX, uint32 InSizeY)
{
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	FIOSView* GLView = AppDelegate.IOSView;
	[GLView UpdateRenderWidth:InSizeX andHeight:InSizeY];
	FRHIResourceCreateInfo CreateInfo;
	BackBuffer = (FMetalTexture2D*)(FTexture2DRHIParamRef)RHICreateTexture2D(InSizeX, InSizeY, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable | TexCreate_Presentable, CreateInfo);

	// ensure a new drawable is created when this new backbuffer is used:
	PreviousRenderTargetsInfo.NumColorRenderTargets = 0;
	CurrentDrawable = nil;
	BackBuffer->Surface.Texture = nil;
	
	// check the size of the window
	float ScalingFactor = [[IOSAppDelegate GetDelegate].IOSView contentScaleFactor];
	CGRect ViewFrame = [[IOSAppDelegate GetDelegate].IOSView frame];
	check(ScalingFactor * ViewFrame.size.width == InSizeX && ScalingFactor * ViewFrame.size.height == InSizeY);
}