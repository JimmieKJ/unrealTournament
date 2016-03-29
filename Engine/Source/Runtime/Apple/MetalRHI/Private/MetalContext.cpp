// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#if PLATFORM_IOS
#include "IOSAppDelegate.h"
#endif

#include "MetalContext.h"
#include "MetalProfiler.h"

const uint32 RingBufferSize = 8 * 1024 * 1024;

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

static id<MTLDevice> GetMTLDevice(uint32& DeviceIndex)
{
	SCOPED_AUTORELEASE_POOL;
	
	DeviceIndex = 0;
	
	if(FParse::Param(FCommandLine::Get(),TEXT("metaldebug")))
	{
		setenv("METAL_DEVICE_WRAPPER_TYPE", "1", 0);
	}
	
	NSArray* DeviceList = MTLCopyAllDevices();
	[DeviceList autorelease];
	
	const int32 NumDevices = [DeviceList count];
	
	TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
	check(GPUs.Num() > 0);
	
	id<MTLDevice> SelectedDevice = nil;
	
	int32 ExplicitRendererId = FPlatformMisc::GetExplicitRendererIndex();
	if (ExplicitRendererId >= 0 && ExplicitRendererId < GPUs.Num())
	{
		FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[ExplicitRendererId];
		for (id<MTLDevice> Device in DeviceList)
		{
			if(([Device.name rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x10DE)
			|| ([Device.name rangeOfString:@"AMD" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x1002)
			|| ([Device.name rangeOfString:@"Intel" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x8086))
			{
				if((Device.headless == GPU.GPUHeadless && GPU.GPUVendorId == 0x1002) || FString(Device.name).Contains(FString(GPU.GPUName).Trim()))
				{
					DeviceIndex = ExplicitRendererId;
					SelectedDevice = Device;
					break;
				}
			}
		}
		if(!SelectedDevice)
		{
			UE_LOG(LogMetal, Warning,  TEXT("Couldn't find Metal device to match GPU descriptor (%s) from IORegistry - using default device."), *FString(GPU.GPUName));
		}
	}
	if (SelectedDevice == nil)
	{
		SelectedDevice = MTLCreateSystemDefaultDevice();
		bool bFoundDefault = false;
		for (uint32 i = 0; i < GPUs.Num(); i++)
		{
			FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[i];
			if(([SelectedDevice.name rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x10DE)
			   || ([SelectedDevice.name rangeOfString:@"AMD" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x1002)
			   || ([SelectedDevice.name rangeOfString:@"Intel" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x8086))
			{
				if((SelectedDevice.headless == GPU.GPUHeadless && GPU.GPUVendorId == 0x1002) || FString(SelectedDevice.name).Contains(FString(GPU.GPUName).Trim()))
				{
					DeviceIndex = i;
					bFoundDefault = true;
					break;
				}
			}
		}
		if(!bFoundDefault)
		{
			UE_LOG(LogMetal, Warning,  TEXT("Couldn't find Metal device %s in GPU descriptors from IORegistry - capability reporting may be wrong."), *FString(SelectedDevice.name));
		}
	}
	check(SelectedDevice);
	return SelectedDevice;
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

FMetalDeviceContext* FMetalDeviceContext::CreateDeviceContext()
{
	uint32 DeviceIndex = 0;
#if PLATFORM_IOS
	id<MTLDevice> Device = [IOSAppDelegate GetDelegate].IOSView->MetalDevice;
#else // @todo zebra
	id<MTLDevice> Device = GetMTLDevice(DeviceIndex);
#endif
	FMetalCommandQueue* Queue = new FMetalCommandQueue(Device);
	check(Queue);
	
	return new FMetalDeviceContext(Device, DeviceIndex, Queue);
}

FMetalDeviceContext::FMetalDeviceContext(id<MTLDevice> MetalDevice, uint32 InDeviceIndex, FMetalCommandQueue* Queue)
: FMetalContext(*Queue, true)
, Device(MetalDevice)
, DeviceIndex(InDeviceIndex)
, SceneFrameCounter(0)
, FrameCounter(0)
, Features(0)
, ActiveContexts(1)
{
#if PLATFORM_IOS
	NSOperatingSystemVersion Vers = [[NSProcessInfo processInfo] operatingSystemVersion];
	if(Vers.majorVersion >= 9)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesResourceOptions | EMetalFeaturesDepthStencilBlitOptions;
#if !PLATFORM_TVOS
		if ([Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1])
#endif
		{
			Features |= EMetalFeaturesDepthClipMode;
		}
	}
	else if(Vers.majorVersion == 8 && Vers.minorVersion >= 3)
	{
		Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset;
	}
#else // Assume that Mac & other platforms all support these from the start. They can diverge later.
	Features = EMetalFeaturesSeparateStencil | EMetalFeaturesSetBufferOffset | EMetalFeaturesDepthClipMode | EMetalFeaturesResourceOptions | EMetalFeaturesDepthStencilBlitOptions;
#endif
	
	// Hook into the ios framepacer, if it's enabled for this platform.
	FrameReadyEvent = NULL;
#if PLATFORM_IOS
	if( FIOSPlatformRHIFramePacer::IsEnabled() )
	{
		FrameReadyEvent = FPlatformProcess::GetSynchEventFromPool();
		FIOSPlatformRHIFramePacer::InitWithEvent( FrameReadyEvent );
	}
#endif
	
	InitFrame(true);
}

FMetalDeviceContext::~FMetalDeviceContext()
{
	if (CurrentCommandBuffer)
	{
		// commit the render context to the commandBuffer
		if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
		{
			CommandEncoder.EndEncoding();
		}
		
		// kick the whole buffer
		// Commit to hand the commandbuffer off to the gpu
		CommandEncoder.CommitCommandBuffer(true);
		
		// Wait for completion as requested.
		[CurrentCommandBuffer waitUntilCompleted];
		
		//once a commandbuffer is commited it can't be added to again.
		UNTRACK_OBJECT(CurrentCommandBuffer);
		[CurrentCommandBuffer release];
		
		CurrentCommandBuffer = nil;
	}
	delete &(GetCommandQueue());
}



void FMetalDeviceContext::BeginFrame()
{
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
}

void FMetalDeviceContext::EndFrame()
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
				CommandEncoder.UnbindObject(Entry);
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
		
		FScopeLock Lock(&PoolMutex);
		BufferPool.DrainPool(false);
	}
	CommandQueue.InsertDebugCaptureBoundary();
}

void FMetalDeviceContext::BeginScene()
{
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
	
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

void FMetalDeviceContext::EndScene()
{
	ResourceTableFrameCounter = INDEX_NONE;
}

void FMetalDeviceContext::BeginDrawingViewport(FMetalViewport* Viewport)
{
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
}

void FMetalDeviceContext::EndDrawingViewport(FMetalViewport* Viewport, bool bPresent)
{
	// commit the render context to the commandBuffer
	if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
	{
		CommandEncoder.EndEncoding();
	}
	
	// kick the whole buffer
	FMetalDelayedFreeList* NewList = new FMetalDelayedFreeList;
	NewList->Signal = FPlatformProcess::GetSynchEventFromPool(true);
	if(GUseRHIThread)
	{
		FreeListMutex.Lock();
	}
	NewList->FreeList = FreeList;
	FreeList.Empty(FreeList.Num());
	if(GUseRHIThread)
	{
		FreeListMutex.Unlock();
	}
	DelayedFreeLists.Add(NewList);
	
	uint32 RingBufferOffset = RingBuffer->GetOffset();
	id<MTLBuffer> CurrentRingBuffer = RingBuffer->Buffer;
	TWeakPtr<FRingBuffer, ESPMode::ThreadSafe>* WeakRingBufferRef = new TWeakPtr<FRingBuffer, ESPMode::ThreadSafe>(RingBuffer.ToSharedRef());
	[CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
	 {
		TSharedPtr<FRingBuffer, ESPMode::ThreadSafe> CmdBufferRingBuffer = WeakRingBufferRef->Pin();
		if(CmdBufferRingBuffer.IsValid() && CmdBufferRingBuffer->Buffer == CurrentRingBuffer)
		{
			CmdBufferRingBuffer->SetLastRead(RingBufferOffset);
		}
		delete WeakRingBufferRef;
	 
		dispatch_semaphore_signal(CommandBufferSemaphore);
		NewList->Signal->Trigger();
	}];
	
	// We may be limiting our framerate to the display link
	if( FrameReadyEvent != nullptr )
	{
		FrameReadyEvent->Wait();
	}
	
	// enqueue a present if desired
	if (bPresent)
	{
		id<MTLDrawable> Drawable = Viewport->GetDrawable();
		[CurrentCommandBuffer addScheduledHandler:^(id<MTLCommandBuffer>) {
			[Drawable present];
		 }];
	}
	
	// Commit before waiting to avoid leaving the gpu idle
	CommandEncoder.CommitCommandBuffer(false);
	
	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];
	CurrentCommandBuffer = nil;
	
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
	
	InitFrame(true);
	
	extern void InitFrame_UniformBufferPoolCleanup();
	InitFrame_UniformBufferPoolCleanup();
	
	Viewport->ReleaseDrawable();
}

void FMetalDeviceContext::ReleaseObject(id Object)
{
	if (GIsRHIInitialized) // @todo zebra: there seems to be some race condition at exit when the framerate is very low
	{
		check(Object);
		if(GUseRHIThread)
		{
			FreeListMutex.Lock();
		}
		check(!FreeList.Contains(Object));
		FreeList.Add(Object);
		if(GUseRHIThread)
		{
			FreeListMutex.Unlock();
		}
	}
}

FMetalPooledBuffer FMetalDeviceContext::CreatePooledBuffer(FMetalPooledBufferArgs const& Args)
{
	FMetalPooledBuffer Buffer;
	if(GIsRHIInitialized)
	{
		FScopeLock Lock(&PoolMutex);
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

void FMetalDeviceContext::ReleasePooledBuffer(FMetalPooledBuffer Buf)
{
	if(GIsRHIInitialized)
	{
		if (!GUseRHIThread)
		{
			CommandEncoder.UnbindObject(Buf.Buffer);
		}
		
		FScopeLock Lock(&PoolMutex);
		//[Buf.Buffer setPurgeableState:MTLPurgeableStateVolatile];
		return BufferPool.ReleasePooledResource(Buf);
	}
}


FMetalRHICommandContext* FMetalDeviceContext::AcquireContext()
{
	FMetalRHICommandContext* Context = ParallelContexts.Pop();
	if (!Context)
	{
		FMetalContext* MetalContext = new FMetalContext(GetCommandQueue(), false);
		check(MetalContext);
		
		FMetalRHICommandContext* CmdContext = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		check(CmdContext);
		
		Context = new FMetalRHICommandContext(CmdContext->GetProfiler(), MetalContext);
	}
	check(Context);
	++ActiveContexts;
	return Context;
}

void FMetalDeviceContext::ReleaseContext(FMetalRHICommandContext* Context)
{
	//delete Context;
	ParallelContexts.Push(Context);
	--ActiveContexts;
}

uint32 FMetalDeviceContext::GetNumActiveContexts(void) const
{
	return ActiveContexts;
}

uint32 FMetalDeviceContext::GetDeviceIndex(void) const
{
	return DeviceIndex;
}

uint32 FMetalContext::AutoReleasePoolTLSSlot = FPlatformTLS::AllocTlsSlot();
uint32 FMetalContext::CurrentContextTLSSlot = FPlatformTLS::AllocTlsSlot();

FMetalContext::FMetalContext(FMetalCommandQueue& Queue, bool const bIsImmediate)
: Device(Queue.GetDevice())
, CommandQueue(Queue)
, CommandList(Queue, bIsImmediate)
, CommandEncoder(CommandList)
, StateCache(CommandEncoder)
, CurrentCommandBuffer(nil)
, RingBuffer(new FRingBuffer(Device, RingBufferSize, BufferOffsetAlignment))
, QueryBuffer(new FMetalQueryBufferPool(this))
, ResourceTableFrameCounter(INDEX_NONE)
{
	// create a semaphore for multi-buffering the command buffer
	CommandBufferSemaphore = dispatch_semaphore_create(FParse::Param(FCommandLine::Get(),TEXT("gpulockstep")) ? 1 : 3);
}

FMetalContext::~FMetalContext()
{
	if (CurrentCommandBuffer)
	{
		// commit the render context to the commandBuffer
		if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
		{
			CommandEncoder.EndEncoding();
		}
		
		// kick the whole buffer
		// Commit to hand the commandbuffer off to the gpu
		CommandEncoder.CommitCommandBuffer(true);
		
		// Wait for completion as requested.
		[CurrentCommandBuffer waitUntilCompleted];
		
		//once a commandbuffer is commited it can't be added to again.
		UNTRACK_OBJECT(CurrentCommandBuffer);
		[CurrentCommandBuffer release];
		
		CurrentCommandBuffer = nil;
	}
}

FMetalContext* FMetalContext::GetCurrentContext()
{
	FMetalContext* Current = (FMetalContext*)FPlatformTLS::GetTlsValue(CurrentContextTLSSlot);
	check(Current);
	return Current;
}

id<MTLDevice> FMetalContext::GetDevice()
{
	return Device;
}

FMetalCommandQueue& FMetalContext::GetCommandQueue()
{
	return CommandQueue;
}

FMetalCommandList& FMetalContext::GetCommandList()
{
	return CommandList;
}

FMetalCommandEncoder& FMetalContext::GetCommandEncoder()
{
	return CommandEncoder;
}

id<MTLRenderCommandEncoder> FMetalContext::GetRenderContext()
{
	if(!CommandEncoder.IsRenderCommandEncoderActive())
	{
		UE_LOG(LogMetal, Fatal, TEXT("Attempted to use GetRenderContext before calling SetRenderTarget to create the encoder"));
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

void FMetalContext::InitFrame(bool const bImmediateContext)
{
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
	
	// Reset cached state in the encoder
	StateCache.Reset();
	
	// start an auto release pool (EndFrame will drain and remake)
	CreateAutoreleasePool();

	// create the command buffer for this frame
	CreateCurrentCommandBuffer(bImmediateContext);

	// make sure first SetRenderTarget goes through
	StateCache.SetHasValidRenderTarget(false);
}

void FMetalContext::FinishFrame()
{
	// Issue any outstanding commands.
	SubmitCommandsHint(false);
	
	// make sure first SetRenderTarget goes through
	StateCache.SetHasValidRenderTarget(false);

	// Drain the auto-release pool for this context
	DrainAutoreleasePool();
	
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, nullptr);
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

void FMetalContext::SubmitCommandsHint(bool const bCreateNew)
{
	uint32 RingBufferOffset = RingBuffer->GetOffset();
	id<MTLBuffer> CurrentRingBuffer = RingBuffer->Buffer;
	TWeakPtr<FRingBuffer, ESPMode::ThreadSafe>* WeakRingBufferRef = new TWeakPtr<FRingBuffer, ESPMode::ThreadSafe>(RingBuffer.ToSharedRef());
    [CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
    {
		TSharedPtr<FRingBuffer, ESPMode::ThreadSafe> CmdBufferRingBuffer = WeakRingBufferRef->Pin();
		if(CmdBufferRingBuffer.IsValid() && CmdBufferRingBuffer->Buffer == CurrentRingBuffer)
		{
			CmdBufferRingBuffer->SetLastRead(RingBufferOffset);
		}
		delete WeakRingBufferRef;
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
	if (bCreateNew)
	{
		CreateCurrentCommandBuffer(false);
	}
	else
	{
		CurrentCommandBuffer = nil;
	}
}

void FMetalContext::SubmitCommandBufferAndWait()
{
	uint32 RingBufferOffset = RingBuffer->GetOffset();
	id<MTLBuffer> CurrentRingBuffer = RingBuffer->Buffer;
	TWeakPtr<FRingBuffer, ESPMode::ThreadSafe>* WeakRingBufferRef = new TWeakPtr<FRingBuffer, ESPMode::ThreadSafe>(RingBuffer.ToSharedRef());
	[CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
	 {
		TSharedPtr<FRingBuffer, ESPMode::ThreadSafe> CmdBufferRingBuffer = WeakRingBufferRef->Pin();
		if(CmdBufferRingBuffer.IsValid() && CmdBufferRingBuffer->Buffer == CurrentRingBuffer)
		{
			CmdBufferRingBuffer->SetLastRead(RingBufferOffset);
		}
		delete WeakRingBufferRef;
	}];
	
	// commit the render context to the commandBuffer
	if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
	{
		CommandEncoder.EndEncoding();
	}
    
    // kick the whole buffer
    // Commit to hand the commandbuffer off to the gpu
    CommandEncoder.CommitCommandBuffer(true);

	// Wait for completion as requested.
	[CurrentCommandBuffer waitUntilCompleted];
	
	//once a commandbuffer is commited it can't be added to again.
	UNTRACK_OBJECT(CurrentCommandBuffer);
	[CurrentCommandBuffer release];
	
	// create a new command buffer.
	CreateCurrentCommandBuffer(false);
}

void FMetalContext::SubmitComputeCommandBufferAndWait()
{
}

void FMetalContext::ResetRenderCommandEncoder()
{
	SubmitCommandsHint();
	
	ConditionalSwitchToGraphics();
	
	if (IsFeatureLevelSupported( GMaxRHIShaderPlatform, ERHIFeatureLevel::SM4 ))
    {
        StateCache.SetRenderTargetsInfo(StateCache.GetRenderTargetsInfo(), QueryBuffer->GetCurrentQueryBuffer()->Buffer, false);
    }
    else
    {
        StateCache.SetRenderTargetsInfo(StateCache.GetRenderTargetsInfo(), NULL, false);
    }
	
	if (CommandEncoder.IsRenderCommandEncoderActive())
	{
		CommandEncoder.RestoreRenderCommandEncodingState();
	}
	else
	{
		CommandEncoder.RestoreRenderCommandEncoding();
	}
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
	
	// @todo Handle the editor not setting a depth-stencil target for the material editor's tiles which render to depth even when they shouldn't.
	bool bRestoreState = false;
	if (IsValidRef(CurrentBoundShaderState->PixelShader) && (CurrentBoundShaderState->PixelShader->Bindings.InOutMask & 0x8000) && StateCache.GetRenderPipelineDesc().PipelineDescriptor.depthAttachmentPixelFormat == MTLPixelFormatInvalid && !FShaderCache::IsPredrawCall())
	{
		UE_LOG(LogMetal, Warning, TEXT("Binding a temporary depth-stencil surface as the bound shader pipeline that writes to depth/stencil but no depth/stencil surface was bound!"));
		check(StateCache.GetRenderTargetArraySize() <= 1);
		CGSize FBSize = StateCache.GetFrameBufferSize();
		
		FRHISetRenderTargetsInfo Info = StateCache.GetRenderTargetsInfo();
		
		FRHIResourceCreateInfo TexInfo;
		FTexture2DRHIRef DepthStencil = RHICreateTexture2D(FBSize.width, FBSize.height, PF_DepthStencil, 1, 1, TexCreate_DepthStencilTargetable, TexInfo);
		Info.DepthStencilRenderTarget.Texture = DepthStencil;
		
		StateCache.SetRenderTargetsInfo(Info, StateCache.GetVisibilityResultsBuffer(), false);
		
		bRestoreState = true;
	}
	
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
		else if (bRestoreState)
		{
			CommandEncoder.RestoreRenderCommandEncodingState();
		}
	}
}

void FMetalContext::SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	// Force submit if there's an RHI thread to prevent the GPU going idle.
	// There may be other locations where this is necessary.
//	if (GUseRHIThread)
//	{
//		SubmitCommandsHint();
//	}
    if (IsFeatureLevelSupported( GMaxRHIShaderPlatform, ERHIFeatureLevel::SM4 ))
    {
        StateCache.SetRenderTargetsInfo(RenderTargetsInfo, QueryBuffer->GetCurrentQueryBuffer()->Buffer, true);
    }
    else
    {
        StateCache.SetRenderTargetsInfo(RenderTargetsInfo, NULL, true);
    }
}

uint32 FMetalContext::AllocateFromRingBuffer(uint32 Size, uint32 Alignment)
{
	return RingBuffer->Allocate(Size, Alignment);
}


void FMetalContext::SetResource(uint32 ShaderStage, uint32 BindIndex, FRHITexture* RESTRICT TextureRHI, float CurrentTime)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	
	if (Surface != nullptr)
	{
		TextureRHI->SetLastRenderTime(CurrentTime);
		
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

void FMetalContext::SetShaderResourceView(EShaderFrequency ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV)
{
	if (SRV)
	{
		FRHITexture* Texture = SRV->SourceTexture.GetReference();
		if (Texture)
		{
			FMetalSurface* Surface = SRV->TextureView;
			if (Surface != nullptr)
			{
				Surface->UpdateSRV();
				GetCommandEncoder().SetShaderTexture(ShaderStage, Surface->Texture, BindIndex);
			}
			else
			{
				GetCommandEncoder().SetShaderTexture(ShaderStage, nil, BindIndex);
			}
		}
		else
		{
			FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
			if (VB)
			{
				GetCommandEncoder().SetShaderBuffer(ShaderStage, VB->Buffer, 0, BindIndex);
			}
		}
	}
	else
	{
		GetCommandEncoder().SetShaderTexture(ShaderStage, nil, BindIndex);
	}
	FShaderCache::SetSRV(ShaderStage, BindIndex, SRV);
}

void FMetalContext::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV, float CurrentTime)
{
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderResourceView(SF_Pixel, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderResourceView(SF_Vertex, BindIndex, SRV);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderResourceView(SF_Compute, BindIndex, SRV);
			break;
			
		default:
			check(0);
			break;
	}
}

void FMetalContext::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSamplerState* RESTRICT SamplerState, float CurrentTime)
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
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	float CurrentTime = FApp::GetCurrentTime();
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
			
			MetalResourceType* ResourcePtr = (MetalResourceType*)Resources[ResourceIndex].GetReference();
			
			// todo: could coalesce adjacent bound resources.
			SetResource(ShaderStage, BindIndex, ResourcePtr, CurrentTime);
			
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

	EShaderFrequency Frequency;
	switch(Shader->Function.functionType)
	{
		case MTLFunctionTypeVertex:
		{
			Frequency = SF_Vertex;
			break;
		}
		case MTLFunctionTypeFragment:
		{
			Frequency = SF_Pixel;
			break;
		}
		case MTLFunctionTypeKernel:
		{
			Frequency = SF_Compute;
			break;
		}
		default:
		{
			check(false);
			break;
		}
	}
	
	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->Bindings.ShaderResourceTable.ResourceTableBits & StateCache.GetDirtyUniformBuffers(Frequency);
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FMetalUniformBuffer* Buffer = (FMetalUniformBuffer*)StateCache.GetBoundUniformBuffers(Frequency)[BufferIndex].GetReference();
		check(Buffer);
		check(BufferIndex < Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes.Num());
		check(Buffer->GetLayout().GetHash() == Shader->Bindings.ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);

		// todo: could make this two pass: gather then set
		SetShaderResourcesFromBuffer<FRHITexture>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.TextureMap.GetData(), BufferIndex);
		SetShaderResourcesFromBuffer<FMetalShaderResourceView>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex);
		SetShaderResourcesFromBuffer<FMetalSamplerState>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.SamplerMap.GetData(), BufferIndex);
	}
	StateCache.SetDirtyUniformBuffers(Frequency, 0);
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
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedUniformBuffers(CurrentBoundShaderState, nullptr, CrossCompiler::SHADER_STAGE_VERTEX, StateCache.GetBoundUniformBuffers(SF_Vertex), CurrentBoundShaderState->VertexShader->UniformBuffersCopyInfo);
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_VERTEX, CurrentBoundShaderState->VertexShader->Bindings);
	
	if (IsValidRef(CurrentBoundShaderState->PixelShader))
	{
		StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedUniformBuffers(CurrentBoundShaderState, nullptr, CrossCompiler::SHADER_STAGE_PIXEL, StateCache.GetBoundUniformBuffers(SF_Pixel), CurrentBoundShaderState->PixelShader->UniformBuffersCopyInfo);
		StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_PIXEL, CurrentBoundShaderState->PixelShader->Bindings);
	}
}

void FMetalContext::ConditionalSwitchToGraphics()
{
	StateCache.ConditionalSwitchToRender();
}

void FMetalContext::ConditionalSwitchToBlit()
{
	StateCache.ConditionalSwitchToBlit();
}

void FMetalContext::Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	if (!CommandEncoder.IsComputeCommandEncoderActive())
	{
		StateCache.ConditionalSwitchToCompute();
		CommandEncoder.RestoreComputeCommandEncodingState();
	}
	check(CommandEncoder.IsComputeCommandEncoderActive());
	
	TRefCountPtr<FMetalComputeShader> CurrentComputeShader = StateCache.GetComputeShader();
	check(CurrentComputeShader);

	auto* ComputeShader = (FMetalComputeShader*)CurrentComputeShader;

	SetResourcesFromTables(ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE);

	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
	
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedUniformBuffers(CurrentBoundShaderState, ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE, StateCache.GetBoundUniformBuffers(SF_Compute), ComputeShader->UniformBuffersCopyInfo);
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_COMPUTE, ComputeShader->Bindings);
	
	MTLSize ThreadgroupCounts = MTLSizeMake(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
	check(ComputeShader->NumThreadsX > 0 && ComputeShader->NumThreadsY > 0 && ComputeShader->NumThreadsZ > 0);
	MTLSize Threadgroups = MTLSizeMake(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	//@todo-rco: setThreadgroupMemoryLength?
	[CommandEncoder.GetComputeCommandEncoder() dispatchThreadgroups:Threadgroups threadsPerThreadgroup:ThreadgroupCounts];
}

#if PLATFORM_MAC
void FMetalContext::DispatchIndirect(FMetalVertexBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
	if (!CommandEncoder.IsComputeCommandEncoderActive())
	{
		StateCache.ConditionalSwitchToCompute();
		CommandEncoder.RestoreComputeCommandEncodingState();
	}
	check(CommandEncoder.IsComputeCommandEncoderActive());
	
	TRefCountPtr<FMetalComputeShader> CurrentComputeShader = StateCache.GetComputeShader();
	check(CurrentComputeShader);
	
	auto* ComputeShader = (FMetalComputeShader*)CurrentComputeShader;
	
	SetResourcesFromTables(ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE);
	
	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
	
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedUniformBuffers(CurrentBoundShaderState, ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE, StateCache.GetBoundUniformBuffers(SF_Compute), ComputeShader->UniformBuffersCopyInfo);
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_COMPUTE, ComputeShader->Bindings);
	
	MTLSize ThreadgroupCounts = MTLSizeMake(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
	check(ComputeShader->NumThreadsX > 0 && ComputeShader->NumThreadsY > 0 && ComputeShader->NumThreadsZ > 0);
	
	[CommandEncoder.GetComputeCommandEncoder() dispatchThreadgroupsWithIndirectBuffer:ArgumentBuffer->Buffer indirectBufferOffset:ArgumentOffset threadsPerThreadgroup:ThreadgroupCounts];
}
#endif

void FMetalContext::StartTiming(class FMetalEventNode* EventNode)
{
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
}

void FMetalContext::EndTiming(class FMetalEventNode* EventNode)
{
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
}

#if METAL_SUPPORTS_PARALLEL_RHI_EXECUTE

class FMetalCommandContextContainer : public IRHICommandContextContainer
{
	FMetalRHICommandContext* CmdContext;
	
public:
	
	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);
	
	FMetalCommandContextContainer()
	: CmdContext(GetMetalDeviceContext().AcquireContext())
	{
		check(CmdContext);
	}
	
	virtual ~FMetalCommandContextContainer() override
	{
	}
	
	virtual IRHICommandContext* GetContext() override
	{
		CmdContext->GetInternalContext().InitFrame(false);
		return CmdContext;
	}
	virtual void FinishContext() override
	{
		check(CmdContext);
		CmdContext->GetInternalContext().FinishFrame();
	}
	virtual void SubmitAndFreeContextContainer(int32 Index, int32 Num) override
	{
		if (CmdContext)
		{
			CmdContext->GetInternalContext().GetCommandList().Submit(Index, Num);
			
			GetMetalDeviceContext().ReleaseContext(CmdContext);
			CmdContext = nullptr;
			check(!CmdContext);
		}
		delete this;
	}
};

static TLockFreeFixedSizeAllocator<sizeof(FMetalCommandContextContainer), PLATFORM_CACHE_LINE_SIZE, FThreadSafeCounter> FMetalCommandContextContainerAllocator;

void* FMetalCommandContextContainer::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

/**
 * Custom delete
 */
void FMetalCommandContextContainer::operator delete(void *RawMemory)
{
	FMemory::Free(RawMemory);
}

IRHICommandContextContainer* FMetalDynamicRHI::RHIGetCommandContextContainer()
{
	return new FMetalCommandContextContainer();
}

#else

IRHICommandContextContainer* FMetalDynamicRHI::RHIGetCommandContextContainer()
{
	return nullptr;
}

#endif