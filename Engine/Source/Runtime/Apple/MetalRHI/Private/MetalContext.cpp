// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#if PLATFORM_IOS
#include "IOSAppDelegate.h"
#endif

#include "MetalContext.h"
#include "MetalProfiler.h"

const uint32 RingBufferSize = 64 * 1024 * 1024;

#if SHOULD_TRACK_OBJECTS
TMap<id, int32> ClassCounts;
#endif

static void* LocalCreateAutoreleasePool()
{
	return [[NSAutoreleasePool alloc] init];
}

static void LocalReleaseAutoreleasePool(void *Pool)
{
	[(NSAutoreleasePool*)Pool release];
}

#if PLATFORM_MAC
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101100
MTL_EXTERN NSArray* MTLCopyAllDevices(void);
#else
MTL_EXTERN NSArray <id<MTLDevice>>* MTLCopyAllDevices(void);
#endif

static id<MTLDevice> GetMTLDevice()
{
	NSArray* DeviceList = MTLCopyAllDevices();
	const int32 NumDevices = [DeviceList count];
	
	// return the first device if there’s only one in the list
	if (NumDevices == 1)
	{
		return (id<MTLDevice>)[DeviceList objectAtIndex:0];
	}
	
	const char* MetalDeviceToUse = getenv("MetalDeviceToUse");
	const bool bForceIntelGPU = MetalDeviceToUse && strcmp(MetalDeviceToUse, "Intel") == 0;
	const bool bForceDefault = FParse::Param(FCommandLine::Get(),TEXT("defaultgpu"));
	
	id<MTLDevice> DefaultDevice = MTLCreateSystemDefaultDevice();
	if (bForceDefault)
	{
		return DefaultDevice;
	}
	
#if WITH_EDITOR
	const bool bForceHeadless = !IsRunningGame() && !bForceIntelGPU;
#else
	const bool bForceHeadless = false;
#endif
	
	if (bForceIntelGPU || bForceHeadless)
	{
		// enumerate through the device list to find the low power device
		for (int32 Index = 0; Index < NumDevices; Index++)
		{
			id<MTLDevice> Device = (id<MTLDevice>)[DeviceList objectAtIndex:Index];
			const bool bIsLowPower = Device.lowPower;
			const bool bIsHeadless = Device != DefaultDevice;
			if (bIsLowPower && bForceIntelGPU)
			{
				return Device;
			}
			else if (bIsHeadless && !bIsLowPower && bForceHeadless)
			{
				return Device;
			}
		}
	}
	
	return DefaultDevice;
}

static MTLPrimitiveTopologyClass TranslatePrimitiveTopology(uint32 PrimitiveType)
{
	switch (PrimitiveType)
	{
		case PT_TriangleList:
		case PT_TriangleStrip:
			return MTLPrimitiveTopologyClassTriangle;
		case PT_LineList:
			return MTLPrimitiveTopologyClassLine;
		case PT_PointList:
			return MTLPrimitiveTopologyClassPoint;
		default:
			UE_LOG(LogMetal, Fatal, TEXT("Unsupported primitive topology %d"), (int32)PrimitiveType);
			return MTLPrimitiveTopologyClassTriangle;
	}
}
#endif

FMetalContext::FMetalContext()
#if PLATFORM_IOS
: Device([IOSAppDelegate GetDelegate].IOSView->MetalDevice)
#else // @todo zebra
: Device(GetMTLDevice())
#endif
, CommandQueue(Device)
, StateCache(CommandEncoder)
, CurrentCommandBuffer(nil)
, RingBuffer(Device, RingBufferSize, BufferOffsetAlignment)
, QueryBuffer(this)
, SceneFrameCounter(0)
, FrameCounter(0)
, ResourceTableFrameCounter(INDEX_NONE)
, Features(0)
{
#if PLATFORM_IOS
	NSOperatingSystemVersion Vers = [[NSProcessInfo processInfo] operatingSystemVersion];
	if(Vers.majorVersion >= 9)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesDepthClipMode | EMetalFeaturesResourceOptions;
	}
	else if(Vers.majorVersion == 8 && Vers.minorVersion >= 3)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset;
	}
#else // Assume that Mac & other platforms all support these from the start. They can diverge later.
	Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesDepthClipMode | EMetalFeaturesResourceOptions;
#endif
	
	// create a semaphore for multi-buffering the command buffer
	CommandBufferSemaphore = dispatch_semaphore_create(FParse::Param(FCommandLine::Get(),TEXT("gpulockstep")) ? 1 : 3);
	
	AutoReleasePoolTLSSlot = FPlatformTLS::AllocTlsSlot();
	
	// Hook into the ios framepacer, if it's enabled for this platform.
	FrameReadyEvent = NULL;
#if PLATFORM_IOS
	if( FIOSPlatformRHIFramePacer::IsEnabled() )
	{
		FrameReadyEvent = FPlatformProcess::GetSynchEventFromPool();
		FIOSPlatformRHIFramePacer::InitWithEvent( FrameReadyEvent );
	}
#endif
	
	InitFrame();
}

FMetalContext::~FMetalContext()
{
	
}

id<MTLDevice> FMetalContext::GetDevice()
{
	return Device;
}

FMetalCommandQueue& FMetalContext::GetCommandQueue()
{
	return CommandQueue;
}

FMetalCommandEncoder& FMetalContext::GetCommandEncoder()
{
	return CommandEncoder;
}

id<MTLRenderCommandEncoder> FMetalContext::GetRenderContext()
{
	if(!CommandEncoder.IsRenderCommandEncoderActive())
	{
		UE_LOG(LogMetal, Warning, TEXT("Attempted to use GetRenderContext before calling SetRenderTarget to create the encoder"));
		ConditionalSwitchToGraphics();
		CommandEncoder.RestoreRenderCommandEncoding();
	}
	return CommandEncoder.GetRenderCommandEncoder();
}

id<MTLBlitCommandEncoder> FMetalContext::GetBlitContext()
{
	ConditionalSwitchToBlit();
	return CommandEncoder.GetBlitCommandEncoder();
}

id<MTLCommandBuffer> FMetalContext::GetCurrentCommandBuffer()
{
	return CurrentCommandBuffer;
}

void FMetalContext::ReleaseObject(id Object)
{
	if (GIsRHIInitialized) // @todo zebra: there seems to be some race condition at exit when the framerate is very low
	{
		check(Object);
		CommandEncoder.UnbindObject(Object);
		check(!FreeList.Contains(Object));
		FreeList.Add(Object);
	}
}

FMetalBufferPool* FMetalContext::GetBufferPool()
{
	if(GIsRHIInitialized)
	{
		return &BufferPool;
	}
	return nullptr;
}

FMetalPooledBuffer FMetalContext::CreatePooledBuffer(FMetalPooledBufferArgs const& Args)
{
	FMetalPooledBuffer Buffer;
	if(GIsRHIInitialized)
	{
		Buffer = BufferPool.CreatePooledResource(Args);
		//[Buffer.Buffer setPurgeableState:MTLPurgeableStateNonVolatile];
	}
	else
	{
		FMetalBufferPoolPolicyData Policy;
		uint32 BufferSize = Policy.GetPoolBucketSize(Policy.GetPoolBucketIndex(Args));
		Buffer.Buffer = [Device newBufferWithLength:BufferSize options: BUFFER_CACHE_MODE
#if METAL_API_1_1
						 | (Args.Storage << MTLResourceStorageModeShift)
#endif
						 ];
	}
	return Buffer;
}

void FMetalContext::ReleasePooledBuffer(FMetalPooledBuffer Buf)
{
	if(GIsRHIInitialized)
	{
		CommandEncoder.UnbindObject(Buf.Buffer);
		//[Buf.Buffer setPurgeableState:MTLPurgeableStateVolatile];
		return BufferPool.ReleasePooledResource(Buf);
	}
}


void FMetalContext::CreateAutoreleasePool()
{
	if (FPlatformTLS::GetTlsValue(AutoReleasePoolTLSSlot) == NULL)
	{
		FPlatformTLS::SetTlsValue(AutoReleasePoolTLSSlot, LocalCreateAutoreleasePool());
	}
}

void FMetalContext::DrainAutoreleasePool()
{
	LocalReleaseAutoreleasePool(FPlatformTLS::GetTlsValue(AutoReleasePoolTLSSlot));
	FPlatformTLS::SetTlsValue(AutoReleasePoolTLSSlot, NULL);
}

void FMetalContext::BeginFrame()
{
}

void FMetalContext::EndFrame()
{
	while(DelayedFreeLists.Num())
	{
		FMetalDelayedFreeList* Pair = DelayedFreeLists[0];
		if(Pair->Signal->Wait(0))
		{
			Pair->Signal->Reset();
			FPlatformProcess::ReturnSynchEventToPool(Pair->Signal);
			for( id Entry : Pair->FreeList )
			{
				[Entry release];
			}
			delete Pair;
			DelayedFreeLists.RemoveAt(0, 1, false);
		}
		else
		{
			break;
		}
	}
	if(FrameCounter != GFrameNumberRenderThread)
	{
		FrameCounter = GFrameNumberRenderThread;
		BufferPool.DrainPool(false);
	}
	CommandQueue.InsertDebugCaptureBoundary();
}

void FMetalContext::BeginScene()
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

void FMetalContext::EndScene()
{
	ResourceTableFrameCounter = INDEX_NONE;
}

void FMetalContext::BeginDrawingViewport(FMetalViewport* Viewport)
{
#if PLATFORM_MAC
	CAMetalLayer* CurrentLayer = (CAMetalLayer*)[Viewport->View layer];
	StateCache.SetCurrentLayer(CurrentLayer);
#endif
}

void FMetalContext::EndDrawingViewport(FMetalViewport* Viewport, bool bPresent)
{
	// commit the render context to the commandBuffer
	if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
	{
		CommandEncoder.EndEncoding();
	}

	// kick the whole buffer
	uint32 RingBufferOffset = RingBuffer.GetOffset();
	
	FMetalDelayedFreeList* NewList = new FMetalDelayedFreeList;
	NewList->Signal = FPlatformProcess::GetSynchEventFromPool(true);
	NewList->FreeList = FreeList;
	DelayedFreeLists.Add(NewList);
	FreeList.Empty(FreeList.Num());

	[CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
	 {
		RingBuffer.SetLastRead(RingBufferOffset);
		dispatch_semaphore_signal(CommandBufferSemaphore);
		NewList->Signal->Trigger();
	 }];

	// We may be limiting our framerate to the display link
	if( FrameReadyEvent != nullptr )
	{
		FrameReadyEvent->Wait();
	}

	// enqueue a present if desired
	if (StateCache.GetCurrentDrawable())
	{
		if (bPresent)
		{
			id<MTLDrawable> Drawable = StateCache.GetCurrentDrawable();
			[CurrentCommandBuffer addScheduledHandler:^(id<MTLCommandBuffer>) {
				[Drawable present];
			 }];
		}

		StateCache.ResetCurrentDrawable();
	}

	// Commit before waiting to avoid leaving the gpu idle
	CommandEncoder.CommitCommandBuffer(false);

//#if WITH_EDITOR // @todo zebra
//	if (!IsRunningGame())
//	{
//		[CurrentCommandBuffer waitUntilCompleted];
//	}
//#endif
	
	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];

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

	Viewport->GetBackBuffer()->Surface.Texture = nil;
}

void FMetalContext::InitFrame()
{
	// start an auto release pool (EndFrame will drain and remake)
	CreateAutoreleasePool();

	// create the command buffer for this frame
	CreateCurrentCommandBuffer(true);

	//	double Start = FPlatformTime::Seconds();
	// block on the semaphore
//	double Mid = FPlatformTime::Seconds();

	// make sure first SetRenderTarget goes through
	StateCache.SetHasValidRenderTarget(false);

//	double End = FPlatformTime::Seconds();
//	NSLog(@"Semaphore Block Time: %.2f   -- MakeDrawable Time: %.2f", 1000.0f*(Mid - Start), 1000.0f*(End - Mid));
	

	extern void InitFrame_UniformBufferPoolCleanup();
	InitFrame_UniformBufferPoolCleanup();
}

void FMetalContext::CreateCurrentCommandBuffer(bool bWait)
{
	if (bWait)
	{
		dispatch_semaphore_wait(CommandBufferSemaphore, DISPATCH_TIME_FOREVER);
	}
	
	CurrentCommandBuffer = CommandQueue.CreateUnretainedCommandBuffer();
	[CurrentCommandBuffer retain];
	TRACK_OBJECT(CurrentCommandBuffer);
	
	CommandEncoder.StartCommandBuffer(CurrentCommandBuffer);
}

void FMetalContext::SubmitCommandsHint()
{
    uint32 RingBufferOffset = RingBuffer.GetOffset();
    [CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
    {
        RingBuffer.SetLastRead(RingBufferOffset);
        dispatch_semaphore_signal(CommandBufferSemaphore);
    }];
    
    // commit the render context to the commandBuffer
	if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
	{
		CommandEncoder.EndEncoding();
	}
    
    // kick the whole buffer
    // Commit to hand the commandbuffer off to the gpu
    CommandEncoder.CommitCommandBuffer(false);
    
    //once a commandbuffer is commited it can't be added to again.
	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];
	
    // create a new command buffer.
    CreateCurrentCommandBuffer(false);
}

void FMetalContext::SubmitCommandBufferAndWait()
{
	uint32 RingBufferOffset = RingBuffer.GetOffset();
	[CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
	 {
		RingBuffer.SetLastRead(RingBufferOffset);
		dispatch_semaphore_signal(CommandBufferSemaphore);
	 }];
	
	// commit the render context to the commandBuffer
	if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
	{
		CommandEncoder.EndEncoding();
	}
    
    // kick the whole buffer
    // Commit to hand the commandbuffer off to the gpu
    CommandEncoder.CommitCommandBuffer(true);

	//once a commandbuffer is commited it can't be added to again.
	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];
	
	// create a new command buffer.
	CreateCurrentCommandBuffer(true);
}

void FMetalContext::SubmitComputeCommandBufferAndWait()
{
}

void FMetalContext::ResetRenderCommandEncoder()
{
	SubmitCommandsHint();
	
	ConditionalSwitchToGraphics();
	StateCache.SetRenderTargetsInfo(StateCache.GetRenderTargetsInfo(), QueryBuffer.GetCurrentQueryBuffer()->Buffer);
	CommandEncoder.RestoreRenderCommandEncodingState();
}

void FMetalContext::PrepareToDraw(uint32 PrimitiveType)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPrepareDrawTime);

	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	MTLVertexDescriptor* Layout = CurrentBoundShaderState->VertexDeclaration->Layout;
	if(Layout && Layout.layouts)
	{
		for (uint32 i = 0; i < MaxMetalStreams; i++)
		{
			auto Attribute = [Layout.attributes objectAtIndexedSubscript:i];
			if(Attribute)
			{
				auto BufferLayout = [Layout.layouts objectAtIndexedSubscript:Attribute.bufferIndex];
				uint32 BufferLayoutStride = BufferLayout ? BufferLayout.stride : 0;
				uint64 MetalSize = StateCache.GetVertexBuffer(Attribute.bufferIndex) ? (uint64)StateCache.GetVertexBuffer(Attribute.bufferIndex).length : 0llu;

				if(!(BufferLayoutStride == StateCache.GetVertexStride(Attribute.bufferIndex) || (MetalSize > 0 && StateCache.GetVertexStride(Attribute.bufferIndex) == 0 && StateCache.GetVertexBuffer(Attribute.bufferIndex) && MetalSize == BufferLayoutStride)))
				{
					FString Report = FString::Printf(TEXT("Vertex Layout Mismatch: Index: %d, Buffer: %p, Len: %lld, Decl. Stride: %d, Stream Stride: %d"), Attribute.bufferIndex, StateCache.GetVertexBuffer(Attribute.bufferIndex), MetalSize, BufferLayoutStride, StateCache.GetVertexStride(Attribute.bufferIndex));
#if PLATFORM_MAC // Disable this to get a soft error that can be debugged further in a GPU trace. // @todo zebra ios
					if(!FShaderCache::IsPredrawCall())
					{
						UE_LOG(LogMetal, Warning, TEXT("%s"), *Report);
					}
					else
					{
						UE_LOG(LogMetal, Warning, TEXT("%s"), *Report);
					}
#else
					UE_LOG(LogMetal, Warning, TEXT("%s"), *Report);
					//[CommandEncoder pushDebugGroup: Report.GetNSString()];
					//[CommandEncoder popDebugGroup];
#endif
				}
			}
		}
	}
#endif
	
#if PLATFORM_MAC
	StateCache.SetPrimitiveTopology(TranslatePrimitiveTopology(PrimitiveType));
#endif
	
	// make sure the BSS has a valid pipeline state object
	CurrentBoundShaderState->PrepareToDraw(this, StateCache.GetRenderPipelineDesc());
	
	if(!FShaderCache::IsPredrawCall())
	{
		CommitGraphicsResourceTables();
		CommitNonComputeShaderConstants();
		
		if(CommandEncoder.IsBlitCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive())
		{
			CommandEncoder.EndEncoding();
		}
		
		if(!CommandEncoder.IsRenderCommandEncoderActive())
		{
			CommandEncoder.RestoreRenderCommandEncoding();
		}
	}
}

void FMetalContext::SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	StateCache.SetRenderTargetsInfo(RenderTargetsInfo, QueryBuffer.GetCurrentQueryBuffer()->Buffer);
}

uint32 FMetalContext::AllocateFromRingBuffer(uint32 Size, uint32 Alignment)
{
	return RingBuffer.Allocate(Size, Alignment);
}


void FMetalContext::SetResource(uint32 ShaderStage, uint32 BindIndex, FRHITexture* RESTRICT TextureRHI)
{
	//	check(Surface->Texture != nil);
	
	// todo: this does multiple virtual function calls to find the right type to cast to
	// this is due to multiple inheritance nastiness, NEEDS CLEANUP
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	
	if (Surface != nullptr)
	{
		switch (ShaderStage)
		{
			case CrossCompiler::SHADER_STAGE_PIXEL:
				CommandEncoder.SetShaderTexture(SF_Pixel, Surface->Texture, BindIndex);
				FShaderCache::SetTexture(SF_Pixel, BindIndex, TextureRHI);
				break;
				
			case CrossCompiler::SHADER_STAGE_VERTEX:
				CommandEncoder.SetShaderTexture(SF_Vertex, Surface->Texture, BindIndex);
				FShaderCache::SetTexture(SF_Vertex, BindIndex, TextureRHI);
				break;
				
			case CrossCompiler::SHADER_STAGE_COMPUTE:
				CommandEncoder.SetShaderTexture(SF_Compute, Surface->Texture, BindIndex);
				FShaderCache::SetTexture(SF_Compute, BindIndex, TextureRHI);
				break;
				
			default:
				check(0);
				break;
		}
	}
	else
	{
		switch (ShaderStage)
		{
			case CrossCompiler::SHADER_STAGE_PIXEL:
				CommandEncoder.SetShaderTexture(SF_Pixel, nil, BindIndex);
				FShaderCache::SetTexture(SF_Pixel, BindIndex, nullptr);
				break;
				
			case CrossCompiler::SHADER_STAGE_VERTEX:
				CommandEncoder.SetShaderTexture(SF_Vertex, nil, BindIndex);
				FShaderCache::SetTexture(SF_Vertex, BindIndex, nullptr);
				break;
				
			case CrossCompiler::SHADER_STAGE_COMPUTE:
				CommandEncoder.SetShaderTexture(SF_Compute, nil, BindIndex);
				FShaderCache::SetTexture(SF_Compute, BindIndex, nullptr);
				break;
				
			default:
				check(0);
				break;
		}
	}
}

void FMetalContext::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSamplerState* RESTRICT SamplerState)
{
	check(SamplerState->State != nil);
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			CommandEncoder.SetShaderSamplerState(SF_Pixel, SamplerState->State, BindIndex);
			FShaderCache::SetSamplerState(SF_Pixel, BindIndex, SamplerState);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			CommandEncoder.SetShaderSamplerState(SF_Vertex, SamplerState->State, BindIndex);
			FShaderCache::SetSamplerState(SF_Vertex, BindIndex, SamplerState);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			CommandEncoder.SetShaderSamplerState(SF_Compute, SamplerState->State, BindIndex);
			FShaderCache::SetSamplerState(SF_Compute, BindIndex, SamplerState);
			break;
			
		default:
			check(0);
			break;
	}
}


template <typename MetalResourceType>
inline int32 FMetalContext::SetShaderResourcesFromBuffer(uint32 ShaderStage, FMetalUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
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
void FMetalContext::SetResourcesFromTables(ShaderType Shader, uint32 ShaderStage)
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
		if(Buffer->RawResourceTable.Num())
		{
			NumSetCalls += SetShaderResourcesFromBuffer<FRHITexture>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.TextureMap.GetData(), BufferIndex);
			NumSetCalls += SetShaderResourcesFromBuffer<FMetalSamplerState>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.SamplerMap.GetData(), BufferIndex);
		}
	}
	Shader->DirtyUniformBuffers = 0;
//	SetTextureInTableCalls += NumSetCalls;
}

void FMetalContext::CommitGraphicsResourceTables()
{
	uint32 Start = FPlatformTime::Cycles();

	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
	check(CurrentBoundShaderState);

	SetResourcesFromTables(CurrentBoundShaderState->VertexShader, CrossCompiler::SHADER_STAGE_VERTEX);
	if (IsValidRef(CurrentBoundShaderState->PixelShader))
	{
		SetResourcesFromTables(CurrentBoundShaderState->PixelShader, CrossCompiler::SHADER_STAGE_PIXEL);
	}

//	CommitResourceTableCycles += (FPlatformTime::Cycles() - Start);
}

void FMetalContext::CommitNonComputeShaderConstants()
{
	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedUniformBuffers(CurrentBoundShaderState, nullptr, CrossCompiler::SHADER_STAGE_VERTEX, CurrentBoundShaderState->VertexShader->BoundUniformBuffers, CurrentBoundShaderState->VertexShader->UniformBuffersCopyInfo);
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_VERTEX, CurrentBoundShaderState->VertexShader->Bindings);
	
	if (IsValidRef(CurrentBoundShaderState->PixelShader))
	{
		StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedUniformBuffers(CurrentBoundShaderState, nullptr, CrossCompiler::SHADER_STAGE_PIXEL, CurrentBoundShaderState->PixelShader->BoundUniformBuffers, CurrentBoundShaderState->PixelShader->UniformBuffersCopyInfo);
		StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_PIXEL, CurrentBoundShaderState->PixelShader->Bindings);
	}
}

void FMetalContext::ConditionalSwitchToGraphics()
{
	StateCache.ConditionalSwitchToRender();
}

void FMetalContext::ConditionalSwitchToCompute()
{
	StateCache.ConditionalSwitchToCompute();
}

void FMetalContext::ConditionalSwitchToBlit()
{
	StateCache.ConditionalSwitchToBlit();
}

void FMetalContext::Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	TRefCountPtr<FMetalComputeShader> CurrentComputeShader = StateCache.GetComputeShader();
	check(CurrentComputeShader);

	auto* ComputeShader = (FMetalComputeShader*)CurrentComputeShader;

	SetResourcesFromTables(ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE);

	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
	
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedUniformBuffers(CurrentBoundShaderState, ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE, ComputeShader->BoundUniformBuffers, ComputeShader->UniformBuffersCopyInfo);
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_COMPUTE, ComputeShader->Bindings);
	
	MTLSize ThreadgroupCounts = MTLSizeMake(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
	check(ComputeShader->NumThreadsX > 0 && ComputeShader->NumThreadsY > 0 && ComputeShader->NumThreadsZ > 0);
	MTLSize Threadgroups = MTLSizeMake(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	//@todo-rco: setThreadgroupMemoryLength?
	[CommandEncoder.GetComputeCommandEncoder() dispatchThreadgroups:Threadgroups threadsPerThreadgroup:ThreadgroupCounts];
}

void FMetalContext::StartTiming(class FMetalEventNode* EventNode)
{
	bool bRecreateRender = CommandEncoder.IsRenderCommandEncoderActive();
	if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
	{
		CommandEncoder.EndEncoding();
	}
	
    if(EventNode && CurrentCommandBuffer)
    {
        EventNode->Start(CurrentCommandBuffer);
    }
    
    // kick the whole buffer
	// Commit to hand the commandbuffer off to the gpu
	CommandEncoder.CommitCommandBuffer(false);
	
	//once a commandbuffer is commited it can't be added to again.
	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];
	
	CreateCurrentCommandBuffer(false);
	
	if(bRecreateRender)
	{
		CommandEncoder.RestoreRenderCommandEncoding();
	}
}

void FMetalContext::EndTiming(class FMetalEventNode* EventNode)
{
	bool bRecreateRender = CommandEncoder.IsRenderCommandEncoderActive();
	if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
	{
		CommandEncoder.EndEncoding();
	}
    
	bool const bWait = EventNode->Wait();
	EventNode->Stop(CurrentCommandBuffer);
	
	// kick the whole buffer
	// Commit to hand the commandbuffer off to the gpu
	CommandEncoder.CommitCommandBuffer(bWait);
    
    //once a commandbuffer is commited it can't be added to again.
	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];

    CreateCurrentCommandBuffer(false);
	
	if(bRecreateRender)
	{
		CommandEncoder.RestoreRenderCommandEncoding();
	}
}