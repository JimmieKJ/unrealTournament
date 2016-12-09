// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "Misc/App.h"
#if PLATFORM_IOS
#include "IOSAppDelegate.h"
#include "IOS/IOSPlatformFramePacer.h"
#endif

#include "MetalContext.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"

#if METAL_STATISTICS
#include "MetalStatistics.h"
#include "ModuleManager.h"
#endif

static int32 GMetalCommandBufferCommitThreshold = 100;
static FAutoConsoleVariableRef CVarMetalCommandBufferCommitThreshold(
	TEXT("rhi.Metal.CommandBufferCommitThreshold"),
	GMetalCommandBufferCommitThreshold,
	TEXT("When enabled (> 0) if the command buffer has more than this number of draw/dispatch command encoded then it will be committed at the next encoder boundary to keep the GPU busy. (Default: 100, set to <= 0 to disable)"));

int32 GMetalSupportsIntermediateBackBuffer = PLATFORM_MAC ? 1 : 0;
static FAutoConsoleVariableRef CVarMetalSupportsIntermediateBackBuffer(
	TEXT("rhi.Metal.SupportsIntermediateBackBuffer"),
	GMetalSupportsIntermediateBackBuffer,
	TEXT("When enabled (> 0) allocate an intermediate texture to use as the back-buffer & blit from there into the actual device back-buffer, thereby allowing screenshots & video capture that would otherwise be impossible as the texture required has already been released back to the OS as required by Metal's API. (Off by default (0) on iOS/tvOS but enabled (1) on Mac)"));

#if PLATFORM_MAC
static int32 GMetalCommandQueueSize = 5120; // This number is large due to texture streaming - currently each texture is its own command-buffer.
// The whole MetalRHI needs to be changed to use MTLHeaps/MTLFences & reworked so that operations with the same synchronisation requirements are collapsed into a single blit command-encoder/buffer.
#else
static int32 GMetalCommandQueueSize = 0;
#endif
static FAutoConsoleVariableRef CVarMetalCommandQueueSize(
	TEXT("rhi.Metal.CommandQueueSize"),
	GMetalCommandQueueSize,
	TEXT("The maximum number of command-buffers that can be allocated from each command-queue. (Default: 5120 Mac, 64 iOS/tvOS)"));

#if METAL_DEBUG_OPTIONS
int32 GMetalBufferScribble = 0; // Deliberately not static, see InitFrame_UniformBufferPoolCleanup
static FAutoConsoleVariableRef CVarMetalBufferScribble(
	TEXT("rhi.Metal.BufferScribble"),
	GMetalBufferScribble,
	TEXT("Debug option: when enabled will scribble over the buffer contents with 0xCD when releasing Shared & Managed buffer objects. (Default: 0, Off)"));

int32 GMetalBufferZeroFill = 0; // Deliberately not static
static FAutoConsoleVariableRef CVarMetalBufferZeroFill(
	TEXT("rhi.Metal.BufferZeroFill"),
	GMetalBufferZeroFill,
	TEXT("Debug option: when enabled will fill the buffer contents with 0 when allocating Shared & Managed buffer objects, or regions thereof. (Default: 0, Off)"));

static int32 GMetalResourcePurgeOnDelete = 0;
static FAutoConsoleVariableRef CVarMetalResourcePurgeOnDelete(
	TEXT("rhi.Metal.ResourcePurgeOnDelete"),
	GMetalResourcePurgeOnDelete,
	TEXT("Debug option: when enabled all MTLResource objects will have their backing stores purged on release - any subsequent access will be invalid and cause a command-buffer failure. Useful for making intermittent resource lifetime errors more common and easier to track. (Default: 0, Off)"));

static int32 GMetalResourceDeferDeleteNumFrames = 0;
static FAutoConsoleVariableRef CVarMetalResourceDeferDeleteNumFrames(
	TEXT("rhi.Metal.ResourceDeferDeleteNumFrames"),
	GMetalResourcePurgeOnDelete,
	TEXT("Debug option: set to the number of frames that must have passed before resource free-lists are processed and resources disposed of. (Default: 0, Off)"));

static int32 GMetalRuntimeDebugLevel = 0;
static FAutoConsoleVariableRef CVarMetalRuntimeDebugLevel(
	TEXT("rhi.Metal.RuntimeDebugLevel"),
	GMetalRuntimeDebugLevel,
	TEXT("The level of debug validation performed by MetalRHI in addition to the underlying Metal API & validation layer.\n")
	TEXT("Each subsequent level adds more tests and reporting in addition to the previous level.\n")
	TEXT("*IGNORED IN SHIPPING BUILDS*. (Default: 0, off)\n")
	TEXT("\t0: Off,\n")
	TEXT("\t1: Enable validation checks for encoder resets,\n")
	TEXT("\t2: Record the debug-groups issued into a command-buffer and report them on failure,\n")
	TEXT("\t3: Record the draw, blit & dispatch commands issued into a command-buffer and report them on failure,\n")
	TEXT("\t4: Allow rhi.Metal.CommandBufferCommitThreshold to break command-encoders (except when MSAA is enabled),\n")
	TEXT("\t5: Wait for each command-buffer to complete immediately after submission."));
#endif

const uint32 RingBufferSize = 8 * 1024 * 1024;

/** Zero-filled uniform buffer used for debugging cases where no buffer was bound. */
class FMetalNullUniformBuffer : public FRenderResource
{
public:
	FUniformBufferRHIRef UniformBuffer;
	virtual void InitRHI() override
	{
		FRHIUniformBufferLayout Layout(FRHIUniformBufferLayout::Zero);
		Layout.ConstantBufferSize = 65536;
		
		TArray<uint8> Data;
		Data.AddZeroed(Layout.ConstantBufferSize);
		
		UniformBuffer = RHICreateUniformBuffer(Data.GetData(), Layout, UniformBuffer_MultiFrame);
	}
	virtual void ReleaseRHI() override
	{
		UniformBuffer.SafeRelease();
	}
	id<MTLBuffer> GetNativeBuffer()
	{
		check(IsValidRef(UniformBuffer));
		FMetalUniformBuffer* UB = (FMetalUniformBuffer*)UniformBuffer.GetReference();
		return UB->Buffer;
	}
};
static TGlobalResource<FMetalNullUniformBuffer> GMetalNullUniformBuffer;

#if SHOULD_TRACK_OBJECTS
TMap<id, int32> ClassCounts;

FCriticalSection* GetClassCountsMutex()
{
	static FCriticalSection Mutex;
	return &Mutex;
}

void TrackMetalObject(NSObject* Obj)
{
	check(Obj);
	
	if (GIsRHIInitialized)
	{
		FScopeLock Lock(GetClassCountsMutex());
		ClassCounts.FindOrAdd([Obj class])++;
	}
}

void UntrackMetalObject(NSObject* Obj)
{
	check(Obj);
	
	if (GIsRHIInitialized)
	{
		FScopeLock Lock(GetClassCountsMutex());
		ClassCounts.FindOrAdd([Obj class])--;
	}
}

#endif

static void* LocalCreateAutoreleasePool()
{
	return [[NSAutoreleasePool alloc] init];
}

static void LocalReleaseAutoreleasePool(void *Pool)
{
	if (Pool)
	{
		[(NSAutoreleasePool*)Pool release];
	}
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
	
#if METAL_STATISTICS
	IMetalStatisticsModule* StatsModule = FModuleManager::Get().LoadModulePtr<IMetalStatisticsModule>(TEXT("MetalStatistics"));
#endif
	
	NSArray* DeviceList = MTLCopyAllDevices();
	[DeviceList autorelease];
	
	const int32 NumDevices = [DeviceList count];
	
	TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
	check(GPUs.Num() > 0);
	
	int32 ExplicitRendererId = FPlatformMisc::GetExplicitRendererIndex();
	if(ExplicitRendererId < 0 && GPUs.Num() > 1 && FMacPlatformMisc::MacOSXVersionCompare(10, 11, 5) == 0)
	{
		int32 OverrideRendererId = -1;
		bool bForceExplicitRendererId = false;
		for(uint32 i = 0; i < GPUs.Num(); i++)
		{
			FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[i];
			if((GPU.GPUVendorId == 0x10DE))
			{
				OverrideRendererId = i;
				bForceExplicitRendererId = (GPU.GPUMetalBundle && ![GPU.GPUMetalBundle isEqualToString:@"GeForceMTLDriverWeb"]);
			}
			else if(!GPU.GPUHeadless && GPU.GPUVendorId != 0x8086)
			{
				OverrideRendererId = i;
			}
		}
		if (bForceExplicitRendererId)
		{
			ExplicitRendererId = OverrideRendererId;
		}
	}
	
	id<MTLDevice> SelectedDevice = nil;
	if (ExplicitRendererId >= 0 && ExplicitRendererId < GPUs.Num())
	{
		FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[ExplicitRendererId];
		TArray<FString> NameComponents;
		FString(GPU.GPUName).Trim().ParseIntoArray(NameComponents, TEXT(" "));	
		for (id<MTLDevice> Device in DeviceList)
		{
			if(([Device.name rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x10DE)
			   || ([Device.name rangeOfString:@"AMD" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x1002)
			   || ([Device.name rangeOfString:@"Intel" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x8086))
			{
				bool bMatchesName = (NameComponents.Num() > 0);
				for (FString& Component : NameComponents)
				{
					bMatchesName &= FString(Device.name).Contains(Component);
				}
				if((Device.headless == GPU.GPUHeadless || GPU.GPUVendorId != 0x1002) && bMatchesName)
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
		TArray<FString> NameComponents;
		SelectedDevice = MTLCreateSystemDefaultDevice();
		bool bFoundDefault = false;
		for (uint32 i = 0; i < GPUs.Num(); i++)
		{
			FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[i];
			if(([SelectedDevice.name rangeOfString:@"Nvidia" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x10DE)
			   || ([SelectedDevice.name rangeOfString:@"AMD" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x1002)
			   || ([SelectedDevice.name rangeOfString:@"Intel" options:NSCaseInsensitiveSearch].location != NSNotFound && GPU.GPUVendorId == 0x8086))
			{
				NameComponents.Empty();
				bool bMatchesName = FString(GPU.GPUName).Trim().ParseIntoArray(NameComponents, TEXT(" ")) > 0;
				for (FString& Component : NameComponents)
				{
					bMatchesName &= FString(SelectedDevice.name).Contains(Component);
				}
				if((SelectedDevice.headless == GPU.GPUHeadless || GPU.GPUVendorId != 0x1002) && bMatchesName)
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
	FMetalCommandQueue* Queue = new FMetalCommandQueue(Device, GMetalCommandQueueSize);
	check(Queue);
	
	return new FMetalDeviceContext(Device, DeviceIndex, Queue);
}

FMetalDeviceContext::FMetalDeviceContext(id<MTLDevice> MetalDevice, uint32 InDeviceIndex, FMetalCommandQueue* Queue)
: FMetalContext(*Queue, true)
, Device(MetalDevice)
, DeviceIndex(InDeviceIndex)
, FreeBuffers([NSMutableSet new])
, SceneFrameCounter(0)
, FrameCounter(0)
, ActiveContexts(1)
, AllocatedContexts(1)
{
#if METAL_DEBUG_OPTIONS
	CommandQueue.SetRuntimeDebuggingLevel(GMetalRuntimeDebugLevel);
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
		SubmitCommandsHint(EMetalSubmitFlagsWaitOnCommandBuffer);
	}
	delete &(GetCommandQueue());
}



void FMetalDeviceContext::BeginFrame()
{
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
}

#if METAL_DEBUG_OPTIONS
void ScribbleBuffer(id<MTLBuffer> Buffer)
{
#if PLATFORM_MAC
	if (Buffer.storageMode != MTLStorageModePrivate)
#endif
	{
		FMemory::Memset(Buffer.contents, 0xCD, Buffer.length);
#if PLATFORM_MAC
		if (Buffer.storageMode == MTLStorageModeManaged)
		{
			[Buffer didModifyRange:NSMakeRange(0, Buffer.length)];
		}
#endif
	}
}
#endif

void FMetalDeviceContext::ClearFreeList()
{
	uint32 Index = 0;
	while(Index < DelayedFreeLists.Num())
	{
		FMetalDelayedFreeList* Pair = DelayedFreeLists[Index];
		if(METAL_DEBUG_OPTION(Pair->DeferCount-- <= 0 &&) dispatch_semaphore_wait(Pair->Signal, DISPATCH_TIME_NOW) == 0)
		{
			dispatch_release(Pair->Signal);
			for( id Entry : Pair->FreeList )
			{
				CommandEncoder.UnbindObject(Entry);
				
#if METAL_DEBUG_OPTIONS
				if (GMetalBufferScribble && [Entry conformsToProtocol:@protocol(MTLBuffer)])
				{
					ScribbleBuffer((id<MTLBuffer>)Entry);
				}
				if (GMetalResourcePurgeOnDelete && [Entry conformsToProtocol:@protocol(MTLResource)])
				{
					[((id<MTLResource>)Entry) setPurgeableState:MTLPurgeableStateEmpty];
				}
#endif
				
				[Entry release];
			}
			{
				FScopeLock Lock(&PoolMutex);
				for ( id<MTLBuffer> Buffer in Pair->FreeBuffers )
				{
					DEC_MEMORY_STAT_BY(STAT_MetalUsedPooledBufferMem, Buffer.length);
					INC_DWORD_STAT(STAT_MetalBufferFreed);
					INC_DWORD_STAT_BY(STAT_MetalBufferMemFreed, Buffer.length);
					INC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, Buffer.length);
					
#if METAL_DEBUG_OPTIONS
					if (GMetalBufferScribble)
					{
						ScribbleBuffer(Buffer);
					}
					if (GMetalResourcePurgeOnDelete)
					{
						[Buffer setPurgeableState:MTLPurgeableStateEmpty];
					}
#endif
				
					BufferPool.ReleasePooledResource(Buffer);
				}
				[Pair->FreeBuffers release];
			}
			delete Pair;
			DelayedFreeLists.RemoveAt(Index, 1, false);
		}
		else
		{
			Index++;
		}
	}
}

void FMetalDeviceContext::EndFrame()
{
	ClearFreeList();
	
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
}

void FMetalDeviceContext::EndScene()
{
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
	NewList->Signal = dispatch_semaphore_create(0);
	METAL_DEBUG_OPTION(NewList->DeferCount = GMetalResourceDeferDeleteNumFrames);
	if(GUseRHIThread)
	{
		FreeListMutex.Lock();
	}
	NewList->FreeList = FreeList;
	
	PoolMutex.Lock();
	NewList->FreeBuffers = FreeBuffers;
	FreeBuffers = [NSMutableSet new];
	PoolMutex.Unlock();
	
#if STATS
	for (id Obj : FreeList)
	{
		check(Obj);
		
		if([[Obj class] conformsToProtocol:@protocol(MTLBuffer)])
		{
			DEC_DWORD_STAT(STAT_MetalBufferCount);
		}
		else if([[Obj class] conformsToProtocol:@protocol(MTLTexture)])
		{
			DEC_DWORD_STAT(STAT_MetalTextureCount);
		}
		else if([[Obj class] conformsToProtocol:@protocol(MTLSamplerState)])
		{
			DEC_DWORD_STAT(STAT_MetalSamplerStateCount);
		}
		else if([[Obj class] conformsToProtocol:@protocol(MTLDepthStencilState)])
		{
			DEC_DWORD_STAT(STAT_MetalDepthStencilStateCount);
		}
		else if([[Obj class] conformsToProtocol:@protocol(MTLRenderPipelineState)])
		{
			DEC_DWORD_STAT(STAT_MetalRenderPipelineStateCount);
		}
		else if([Obj isKindOfClass:[MTLRenderPipelineColorAttachmentDescriptor class]])
		{
			DEC_DWORD_STAT(STAT_MetalRenderPipelineColorAttachmentDescriptor);
		}
		else if([Obj isKindOfClass:[MTLRenderPassDescriptor class]])
		{
			DEC_DWORD_STAT(STAT_MetalRenderPassDescriptorCount);
		}
		else if([Obj isKindOfClass:[MTLRenderPassColorAttachmentDescriptor class]])
		{
			DEC_DWORD_STAT(STAT_MetalRenderPassColorAttachmentDescriptorCount);
		}
		else if([Obj isKindOfClass:[MTLRenderPassDepthAttachmentDescriptor class]])
		{
			DEC_DWORD_STAT(STAT_MetalRenderPassDepthAttachmentDescriptorCount);
		}
		else if([Obj isKindOfClass:[MTLRenderPassStencilAttachmentDescriptor class]])
		{
			DEC_DWORD_STAT(STAT_MetalRenderPassStencilAttachmentDescriptorCount);
		}
		else if([Obj isKindOfClass:[MTLVertexDescriptor class]])
		{
			DEC_DWORD_STAT(STAT_MetalVertexDescriptorCount);
		}
		
#if SHOULD_TRACK_OBJECTS
		UntrackMetalObject(Obj);
#endif
	}
#endif
	FreeList.Empty(FreeList.Num());
	if(GUseRHIThread)
	{
		FreeListMutex.Unlock();
	}
	
	dispatch_semaphore_t Signal = NewList->Signal;
	dispatch_retain(Signal);
	[CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
	 {
		dispatch_semaphore_signal(CommandBufferSemaphore);
		dispatch_semaphore_signal(Signal);
		dispatch_release(Signal);
	}];
    DelayedFreeLists.Add(NewList);
	
	// enqueue a present if desired
	if (bPresent)
	{
		id<CAMetalDrawable> Drawable = (id<CAMetalDrawable>)Viewport->GetDrawable(EMetalViewportAccessRHI);
		if (Drawable && Drawable.texture)
		{
			[Drawable retain];
			
			if (GMetalSupportsIntermediateBackBuffer)
			{
				id<MTLBlitCommandEncoder> Blitter = GetBlitContext();
				
				id<MTLTexture> Src = [Viewport->GetBackBuffer(EMetalViewportAccessRHI)->Surface.Texture retain];
				id<MTLTexture> Dst = [Drawable.texture retain];
				
				NSUInteger Width = FMath::Min(Src.width, Dst.width);
				NSUInteger Height = FMath::Min(Src.height, Dst.height);
				
				METAL_DEBUG_COMMAND_BUFFER_TRACK_RES(CurrentCommandBuffer, Src);
				METAL_DEBUG_COMMAND_BUFFER_TRACK_RES(CurrentCommandBuffer, Dst);
				
				[Blitter copyFromTexture:Src sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(Width, Height, 1) toTexture:Dst destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
				
				METAL_DEBUG_COMMAND_BUFFER_BLIT_LOG(this, @"Present(BackBuffer %p, Drawable %p)", Viewport->GetBackBuffer(EMetalViewportAccessRHI), Drawable);
				
				CommandEncoder.EndEncoding();
				
				[CurrentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
					[Src release];
					[Dst release];
				}];
			}
			
			[CurrentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
				[Drawable release];
			}];
			[CurrentCommandBuffer addScheduledHandler:^(id<MTLCommandBuffer>) {
				[Drawable present];
			}];
		}
	}
	
	// We may be limiting our framerate to the display link
	if( FrameReadyEvent != nullptr )
	{
		FrameReadyEvent->Wait();
	}
	
	// Latched update of whether to use runtime debugging features
	uint32 SubmitFlags = EMetalSubmitFlagsNone;
#if METAL_DEBUG_OPTIONS
	if (GMetalRuntimeDebugLevel != CommandQueue.GetRuntimeDebuggingLevel())
	{
		CommandQueue.SetRuntimeDebuggingLevel(GMetalRuntimeDebugLevel);
		
		// After change the debug features level wait on commit
		SubmitFlags |= EMetalSubmitFlagsWaitOnCommandBuffer;
	}
#endif
	SubmitCommandsHint(SubmitFlags);
	
#if SHOULD_TRACK_OBJECTS
	// print out outstanding objects
	if ((GFrameCounter % 500) == 10)
	{
		for (auto It = ClassCounts.CreateIterator(); It; ++It)
		{
			UE_LOG(LogMetal, Display, TEXT("%s has %d outstanding allocations"), *FString([It.Key() description]), It.Value());
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
		if(!FreeList.Contains(Object))
        {
            FreeList.Add(Object);
        }
        else
        {
            [Object release];
        }
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
#if METAL_DEBUG_OPTIONS
		if (GMetalResourcePurgeOnDelete)
		{
			[Buffer.Buffer setPurgeableState:MTLPurgeableStateNonVolatile];
		}
#endif
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
		TRACK_OBJECT(STAT_MetalBufferCount, Buffer.Buffer);
		INC_DWORD_STAT(STAT_MetalPooledBufferCount);
		INC_MEMORY_STAT_BY(STAT_MetalPooledBufferMem, BufferSize);
		INC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, BufferSize);
	}
	INC_MEMORY_STAT_BY(STAT_MetalUsedPooledBufferMem, Buffer.Buffer.length);
	INC_DWORD_STAT(STAT_MetalBufferAlloctations);
	INC_DWORD_STAT_BY(STAT_MetalBufferMemAlloc, Buffer.Buffer.length);
	DEC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, Buffer.Buffer.length);
	
#if METAL_DEBUG_OPTIONS
	if (GMetalBufferZeroFill && Args.Storage != MTLStorageModePrivate)
	{
		FMemory::Memset([Buffer.Buffer contents], 0x0, Buffer.Buffer.length);
	}
#endif
	
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
		
		FScopeLock Lock(&FreeListMutex);
		[FreeBuffers addObject:Buf.Buffer];
		//[Buf.Buffer setPurgeableState:MTLPurgeableStateVolatile];
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
		
		++AllocatedContexts;
	}
	check(Context);
	FPlatformAtomics::InterlockedIncrement(&ActiveContexts);
	return Context;
}

void FMetalDeviceContext::ReleaseContext(FMetalRHICommandContext* Context)
{
	check(!Context->GetInternalContext().GetCurrentCommandBuffer());
	check(!Context->GetInternalContext().GetCommandEncoder().IsRenderCommandEncoderActive());
	check(!Context->GetInternalContext().GetCommandEncoder().IsComputeCommandEncoderActive());
	check(!Context->GetInternalContext().GetCommandEncoder().IsBlitCommandEncoderActive());
	
	ParallelContexts.Push(Context);
	FPlatformAtomics::InterlockedDecrement(&ActiveContexts);
	check(ActiveContexts >= 1);
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
, OutstandingOpCount(0)
, bValidationEnabled(false)
{
	// create a semaphore for multi-buffering the command buffer
	CommandBufferSemaphore = dispatch_semaphore_create(FParse::Param(FCommandLine::Get(),TEXT("gpulockstep")) ? 1 : 3);
	
	FMemory::Memzero(BufferSideTable);
	
	bValidationEnabled = false;
	
#if METAL_STATISTICS
	IMetalStatisticsModule* StatsModule = FModuleManager::Get().LoadModulePtr<IMetalStatisticsModule>(TEXT("MetalStatistics"));
	if (StatsModule)
	{
		bValidationEnabled = StatsModule->IsValidationEnabled();
	}
#endif
}

FMetalContext::~FMetalContext()
{
	if (CurrentCommandBuffer)
	{
		SubmitCommandsHint(EMetalSubmitFlagsWaitOnCommandBuffer);
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

void FMetalContext::InsertCommandBufferFence(FMetalCommandBufferFence& Fence)
{
	check(CurrentCommandBuffer);
	
	TSharedPtr<MTLCommandBufferRef, ESPMode::ThreadSafe>* CmdBufRef = new TSharedPtr<MTLCommandBufferRef, ESPMode::ThreadSafe>(new MTLCommandBufferRef([CurrentCommandBuffer retain]));
	
	[CurrentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull CommandBuffer)
	{
		if(CommandBuffer.status == MTLCommandBufferStatusError)
		{
			FMetalCommandList::HandleMetalCommandBufferFailure(CommandBuffer);
		}
		delete CmdBufRef;
	}];
	
	Fence.CommandBufferRef = *CmdBufRef;
}

void FMetalContext::CreateAutoreleasePool()
{
	// Create an autorelease pool. Not on the game thread, though. It's not needed there, as the game thread provides its own pools.
	// Also, it'd cause problems as we sometimes call EndDrawingViewport on the game thread. That would drain the pool for Metal
	// context while some other pool is active, which is illegal.
	if (FPlatformTLS::GetTlsValue(AutoReleasePoolTLSSlot) == NULL && FPlatformTLS::GetCurrentThreadId() != GGameThreadId)
	{
		FPlatformTLS::SetTlsValue(AutoReleasePoolTLSSlot, LocalCreateAutoreleasePool());
	}
}

void FMetalContext::DrainAutoreleasePool()
{
	LocalReleaseAutoreleasePool(FPlatformTLS::GetTlsValue(AutoReleasePoolTLSSlot));
	FPlatformTLS::SetTlsValue(AutoReleasePoolTLSSlot, NULL);
}

void FMetalContext::MakeCurrent(FMetalContext* Context)
{
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, Context);
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
	
	// Zero the side table
	FMemory::Memzero(BufferSideTable);
}

void FMetalContext::FinishFrame()
{
	// Issue any outstanding commands.
	if (CurrentCommandBuffer)
	{
		SubmitCommandsHint(EMetalSubmitFlagsNone);
	}
	
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
	
	CurrentCommandBuffer = CreateCommandBuffer(false);
	[CurrentCommandBuffer retain];
	
	CommandEncoder.StartCommandBuffer(CurrentCommandBuffer);
}

void FMetalContext::SubmitCommandsHint(uint32 const bFlags)
{
	bool const bCreateNew = (bFlags & EMetalSubmitFlagsCreateCommandBuffer);
	bool const bWait = (bFlags & EMetalSubmitFlagsWaitOnCommandBuffer);
	if (!(bFlags & EMetalSubmitFlagsBreakCommandBuffer))
	{
		uint32 RingBufferOffset = RingBuffer->GetOffset();
		uint32 StartOffset = RingBuffer->LastWritten;
		RingBuffer->LastWritten = RingBufferOffset;
		id<MTLBuffer> CurrentRingBuffer = RingBuffer->Buffer;
		TWeakPtr<FRingBuffer, ESPMode::ThreadSafe>* WeakRingBufferRef = new TWeakPtr<FRingBuffer, ESPMode::ThreadSafe>(RingBuffer.ToSharedRef());
		[CurrentCommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> Buffer)
		{
			TSharedPtr<FRingBuffer, ESPMode::ThreadSafe> CmdBufferRingBuffer = WeakRingBufferRef->Pin();
			if(CmdBufferRingBuffer.IsValid() && CmdBufferRingBuffer->Buffer == CurrentRingBuffer)
			{
	#if METAL_DEBUG_OPTIONS
				if (GMetalBufferScribble && StartOffset != RingBufferOffset)
				{
					if (StartOffset < RingBufferOffset)
					{
						FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.contents) + StartOffset, 0xCD, RingBufferOffset - StartOffset);
					}
					else
					{
						uint32 TrailLen = CmdBufferRingBuffer->Buffer.length - StartOffset;
						FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.contents) + StartOffset, 0xCD, TrailLen);
						FMemory::Memset(((uint8*)CmdBufferRingBuffer->Buffer.contents), 0xCD, RingBufferOffset);
					}
				}
	#endif
				CmdBufferRingBuffer->SetLastRead(RingBufferOffset);
			}
			delete WeakRingBufferRef;
		}];
	}
	
	// commit the render context to the commandBuffer
	if (CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive())
	{
		CommandEncoder.EndEncoding();
	}
	
	// kick the whole buffer
	// Commit to hand the commandbuffer off to the gpu
	CommandEncoder.CommitCommandBuffer(bWait);
	
	//once a commandbuffer is commited it can't be added to again.
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
	
	OutstandingOpCount = 0;
}

void FMetalContext::SubmitCommandBufferAndWait()
{
	// kick the whole buffer
	// Commit to hand the commandbuffer off to the gpu
	// Wait for completion as requested.
	SubmitCommandsHint((EMetalSubmitFlagsCreateCommandBuffer | EMetalSubmitFlagsWaitOnCommandBuffer));
}

void FMetalContext::ResetRenderCommandEncoder()
{
	SubmitCommandsHint();
	
	SetRenderTargetsInfo(StateCache.GetRenderTargetsInfo(), false);
	
	if (CommandEncoder.IsRenderCommandEncoderActive())
	{
		CommandEncoder.RestoreRenderCommandEncodingState();
	}
	else
	{
		CommandEncoder.RestoreRenderCommandEncoding();
	}
}

bool FMetalContext::PrepareToDraw(uint32 PrimitiveType)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPrepareDrawTime);
	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
	
	// Enforce calls to SetRenderTarget prior to issuing draw calls.
#if PLATFORM_MAC
	check(StateCache.GetHasValidRenderTarget());
#else
	if (!StateCache.GetHasValidRenderTarget())
	{
		return false;
	}
#endif
	
	bool bUpdatedStrides = false;
	uint32 StrideHash = 0;
	MTLVertexDescriptor* Layout = CurrentBoundShaderState->VertexDeclaration->Layout.VertexDesc;
	if(Layout && Layout.layouts)
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalPrepareVertexDescTime);
		
		for (uint32 ElementIndex = 0; ElementIndex < CurrentBoundShaderState->VertexDeclaration->Elements.Num(); ElementIndex++)
		{
			const FVertexElement& Element = CurrentBoundShaderState->VertexDeclaration->Elements[ElementIndex];
			
			uint32 StreamStride = StateCache.GetVertexStride(Element.StreamIndex);
			StrideHash ^= (StreamStride << ElementIndex);
			
			bool const bStrideMistmatch = (StreamStride > 0 && Element.Stride != StreamStride);
			
			uint32 const ActualStride = bStrideMistmatch ? StreamStride : (Element.Stride ? Element.Stride : TranslateElementTypeToSize(Element.Type));
			
			bool const bStepRateMistmatch = StreamStride == 0 && Element.Stride != StreamStride;

			if (bStrideMistmatch || bStepRateMistmatch)
			{
				if (!bUpdatedStrides)
				{
					bUpdatedStrides = true;
					Layout = [Layout copy];
					TRACK_OBJECT(STAT_MetalVertexDescriptorCount, Layout);
				}
				auto BufferLayout = [Layout.layouts objectAtIndexedSubscript:UNREAL_TO_METAL_BUFFER_INDEX(Element.StreamIndex)];
				check(BufferLayout);
				if (bStrideMistmatch)
				{
					BufferLayout.stride = ActualStride;
				}
				if (bStepRateMistmatch)
				{
					BufferLayout.stepFunction = MTLVertexStepFunctionConstant;
					BufferLayout.stepRate = 0;
				}
			}
		}
	}
	
	FMetalHashedVertexDescriptor VertexDesc;
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalPrepareVertexDescTime);
		VertexDesc = !bUpdatedStrides ? CurrentBoundShaderState->VertexDeclaration->Layout : FMetalHashedVertexDescriptor(Layout, (CurrentBoundShaderState->VertexDeclaration->BaseHash ^ StrideHash));
	}
	
	// Validate the vertex layout in debug mode, or when the validation layer is enabled for development builds.
	// Other builds will just crash & burn if it is incorrect.
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if(CommandQueue.GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
	{
		if(Layout && Layout.layouts)
		{
			for (uint32 i = 0; i < MaxVertexElementCount; i++)
			{
				auto Attribute = [Layout.attributes objectAtIndexedSubscript:i];
				if(Attribute && Attribute.format > MTLVertexFormatInvalid)
				{
					auto BufferLayout = [Layout.layouts objectAtIndexedSubscript:Attribute.bufferIndex];
					uint32 BufferLayoutStride = BufferLayout ? BufferLayout.stride : 0;
					
					uint32 BufferIndex = METAL_TO_UNREAL_BUFFER_INDEX(Attribute.bufferIndex);
					
					uint64 MetalSize = StateCache.GetVertexBufferSize(BufferIndex);
					
					uint32 MetalStride = StateCache.GetVertexStride(BufferIndex);
					
					// If the vertex attribute is required and either no Metal buffer is bound or the size of the buffer is smaller than the stride, or the stride is explicitly specified incorrectly then the layouts don't match.
					if (BufferLayoutStride > 0 && (MetalSize < BufferLayoutStride || (MetalStride > 0 && MetalStride != BufferLayoutStride)))
					{
						FString Report = FString::Printf(TEXT("Vertex Layout Mismatch: Index: %d, Len: %lld, Decl. Stride: %d, Stream Stride: %d"), Attribute.bufferIndex, MetalSize, BufferLayoutStride, StateCache.GetVertexStride(BufferIndex));
						UE_LOG(LogMetal, Warning, TEXT("%s"), *Report);
						
						if (GEmitDrawEvents)
						{
							CommandEncoder.PushDebugGroup(Report.GetNSString());
							CommandEncoder.PopDebugGroup();
						}
					}
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
	if (IsValidRef(CurrentBoundShaderState->PixelShader) && (CurrentBoundShaderState->PixelShader->Bindings.InOutMask & 0x8000) && !StateCache.HasValidDepthStencilSurface() && StateCache.GetRenderPipelineDesc().PipelineDescriptor.depthAttachmentPixelFormat == MTLPixelFormatInvalid && !FShaderCache::IsPredrawCall())
	{
#if UE_BUILD_DEBUG
		UE_LOG(LogMetal, Warning, TEXT("Binding a temporary depth-stencil surface as the bound shader pipeline writes to depth/stencil but no depth/stencil surface was bound!"));
#endif
		check(StateCache.GetRenderTargetArraySize() <= 1);
		CGSize FBSize = StateCache.GetFrameBufferSize();
		
		FRHISetRenderTargetsInfo Info = StateCache.GetRenderTargetsInfo();
		
		// Cache the fallback depth-stencil surface to reduce memory bloat - only need to recreate if size changes.
		if (!IsValidRef(FallbackDepthStencilSurface) || FallbackDepthStencilSurface->GetSizeX() != FBSize.width || FallbackDepthStencilSurface->GetSizeY() != FBSize.height)
		{
			FRHIResourceCreateInfo TexInfo;
			FallbackDepthStencilSurface = RHICreateTexture2D(FBSize.width, FBSize.height, PF_DepthStencil, 1, 1, TexCreate_DepthStencilTargetable, TexInfo);
		}
		check(IsValidRef(FallbackDepthStencilSurface));
		Info.DepthStencilRenderTarget.Texture = FallbackDepthStencilSurface;
		
		StateCache.SetRenderTargetsInfo(Info, StateCache.GetVisibilityResultsBuffer(), false);
		
		bRestoreState = true;
		
		// Enforce calls to SetRenderTarget prior to issuing draw calls.
		check(StateCache.GetHasValidRenderTarget());
		check(CommandEncoder.IsRenderPassDescriptorValid());
	}
	
	// make sure the BSS has a valid pipeline state object
#if METAL_DEBUG_OPTIONS
	MTLRenderPipelineReflection* Reflection = nil;
	if (CommandQueue.GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
	{
		CurrentBoundShaderState->PrepareToDraw(this, VertexDesc, StateCache.GetRenderPipelineDesc(), &Reflection);
		check(Reflection);
	}
	else
#endif
	{
		CurrentBoundShaderState->PrepareToDraw(this, VertexDesc, StateCache.GetRenderPipelineDesc());
	}	
	
	if(!FShaderCache::IsPredrawCall())
	{
#if METAL_DEBUG_OPTIONS
		// Force a command-encoder when GMetalRuntimeDebugLevel is enabled to help track down intermittent command-buffer failures.
		if (GMetalCommandBufferCommitThreshold > 0 && OutstandingOpCount >= GMetalCommandBufferCommitThreshold && CommandQueue.GetRuntimeDebuggingLevel() >= EMetalDebugLevelConditionalSubmit)
		{
			FRHISetRenderTargetsInfo CurrentRenderTargets = StateCache.GetRenderTargetsInfo();
			bool bCanChangeRT = true;
			
			if (CommandEncoder.IsRenderCommandEncoderActive())
			{
				const bool bIsMSAAActive = StateCache.GetHasValidRenderTarget() && StateCache.GetRenderPipelineDesc().SampleCount != 1;
				bCanChangeRT = !bIsMSAAActive;
				
				for (int32 RenderTargetIndex = 0; bCanChangeRT && RenderTargetIndex < CurrentRenderTargets.NumColorRenderTargets; RenderTargetIndex++)
				{
					FRHIRenderTargetView& RenderTargetView = CurrentRenderTargets.ColorRenderTarget[RenderTargetIndex];
					
					if (RenderTargetView.StoreAction != ERenderTargetStoreAction::EMultisampleResolve)
					{
						RenderTargetView.LoadAction = ERenderTargetLoadAction::ELoad;
						RenderTargetView.StoreAction = ERenderTargetStoreAction::EStore;
					}
					else
					{
						bCanChangeRT = false;
					}
				}
				
				if (bCanChangeRT && CurrentRenderTargets.DepthStencilRenderTarget.Texture)
				{
					if (CurrentRenderTargets.DepthStencilRenderTarget.DepthStoreAction != ERenderTargetStoreAction::EMultisampleResolve && CurrentRenderTargets.DepthStencilRenderTarget.GetStencilStoreAction() != ERenderTargetStoreAction::EMultisampleResolve)
					{
						CurrentRenderTargets.DepthStencilRenderTarget = FRHIDepthRenderTargetView(CurrentRenderTargets.DepthStencilRenderTarget.Texture, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
					}
					else
					{
						bCanChangeRT = false;
					}
				}
			}
			
			if (bCanChangeRT)
			{
				// Force submit if there's enough outstanding commands to prevent the GPU going idle.
				SubmitCommandsHint(EMetalSubmitFlagsCreateCommandBuffer|EMetalSubmitFlagsBreakCommandBuffer);
				
                StateCache.ConditionalSwitchToRender();
				
				if (IsFeatureLevelSupported( GMaxRHIShaderPlatform, ERHIFeatureLevel::SM4 ))
				{
					StateCache.SetRenderTargetsInfo(CurrentRenderTargets, StateCache.GetVisibilityResultsBuffer(), false);
				}
				else
				{
					StateCache.SetRenderTargetsInfo(CurrentRenderTargets, NULL, false);
				}
				
				if(!CommandEncoder.IsRenderCommandEncoderActive())
				{
					CommandEncoder.RestoreRenderCommandEncoding();
				}
				else
				{
					CommandEncoder.RestoreRenderCommandEncodingState();
				}
			}
		}
#endif
		
		CommitGraphicsResourceTables();
		
		if(CommandEncoder.IsBlitCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive())
		{
			CommandEncoder.EndEncoding();
		}
		
		if(!CommandEncoder.IsRenderCommandEncoderActive())
		{
			if (!CommandEncoder.IsRenderPassDescriptorValid())
			{
				UE_LOG(LogMetal, Warning, TEXT("Re-binding the render-target because no RenderPassDescriptor was bound!"));
				FRHISetRenderTargetsInfo Info = StateCache.GetRenderTargetsInfo();
				StateCache.SetHasValidRenderTarget(false);
				StateCache.SetRenderTargetsInfo(Info, StateCache.GetVisibilityResultsBuffer(), false);
			}
			
			CommandEncoder.RestoreRenderCommandEncoding();
		}
		else if (bRestoreState)
		{
			CommandEncoder.RestoreRenderCommandEncodingState();
		}
		
		check(CommandEncoder.IsRenderCommandEncoderActive());
		if (CurrentBoundShaderState->VertexShader->SideTableBinding >= 0)
		{
			uint32 Offset = AllocateFromRingBuffer((sizeof(BufferSideTable) / SF_NumFrequencies));
			id<MTLBuffer> Buffer = GetRingBuffer();
			
			FMemory::Memcpy(((uint8*)[Buffer contents]) + Offset, BufferSideTable[SF_Vertex], (sizeof(BufferSideTable) / SF_NumFrequencies));
			
			CommandEncoder.SetShaderBuffer(SF_Vertex, Buffer, Offset, CurrentBoundShaderState->VertexShader->SideTableBinding);
		}
		
		if (IsValidRef(CurrentBoundShaderState->PixelShader) && CurrentBoundShaderState->PixelShader->SideTableBinding >= 0)
		{
			uint32 Offset = AllocateFromRingBuffer((sizeof(BufferSideTable) / SF_NumFrequencies));
			id<MTLBuffer> Buffer = GetRingBuffer();
			
			FMemory::Memcpy(((uint8*)[Buffer contents]) + Offset, BufferSideTable[SF_Pixel], (sizeof(BufferSideTable) / SF_NumFrequencies));
			
			CommandEncoder.SetShaderBuffer(SF_Pixel, Buffer, Offset, CurrentBoundShaderState->PixelShader->SideTableBinding);
		}
		
		CommitNonComputeShaderConstants();
		
#if METAL_DEBUG_OPTIONS
		if (CommandQueue.GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
		{
			auto& VBBindings = CurrentBoundShaderState->VertexShader->Bindings;
			for (uint32 i = 0; i < VBBindings.NumUniformBuffers; i++)
			{
				bool const bBoundZeroUniform = CommandEncoder.SetShaderBufferConditional(SF_Vertex, GMetalNullUniformBuffer.GetNativeBuffer(), 0, i);
				if (bBoundZeroUniform)
				{
					bool bWarn = false;
					for(MTLArgument* Arg in Reflection.vertexArguments)
					{
						if (Arg.type == MTLArgumentTypeBuffer && Arg.index == i)
						{
							UE_LOG(LogMetal, Error, TEXT("No uniform bound to index %d when required by vertex shader:\n%s"), i, *FString(CurrentBoundShaderState->VertexShader->GlslCodeNSString));
							break;
						}
					}
				}
			}
			
			if (IsValidRef(CurrentBoundShaderState->PixelShader))
			{
				auto& PBBindings = CurrentBoundShaderState->PixelShader->Bindings;
				for (uint32 i = 0; i < PBBindings.NumUniformBuffers; i++)
				{
					bool const bBoundZeroUniform = CommandEncoder.SetShaderBufferConditional(SF_Pixel, GMetalNullUniformBuffer.GetNativeBuffer(), 0, i);
					if (bBoundZeroUniform)
					{
						bool bWarn = false;
						for(MTLArgument* Arg in Reflection.fragmentArguments)
						{
							if (Arg.type == MTLArgumentTypeBuffer && Arg.index == i)
							{
								UE_LOG(LogMetal, Error, TEXT("No uniform bound to index %d when required by pixel shader:\n%s"), i, *FString(CurrentBoundShaderState->PixelShader->GlslCodeNSString));
								break;
							}
						}
					}
				}
			}
			
			if (!CommandEncoder.ValidateArgumentBindings(SF_Vertex, Reflection))
			{
				UE_LOG(LogMetal, Error, TEXT("Metal Validation failures for vertex shader:\n%s"), *FString(CurrentBoundShaderState->VertexShader->GlslCodeNSString));
			}
			if (IsValidRef(CurrentBoundShaderState->PixelShader))
			{
				if (!CommandEncoder.ValidateArgumentBindings(SF_Pixel, Reflection))
				{
					UE_LOG(LogMetal, Error, TEXT("Metal Validation failures for pixel shader:\n%s"), *FString(CurrentBoundShaderState->PixelShader->GlslCodeNSString));
				}
			}
		}
#endif
		
		OutstandingOpCount++;
	}
	
	return true;
}

void FMetalContext::SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& RenderTargetsInfo, bool const bReset)
{
	// Force submit if there's enough outstanding commands to prevent the GPU going idle.
	ConditionalSwitchToGraphics(StateCache.NeedsToSetRenderTarget(RenderTargetsInfo), !(PLATFORM_MAC));
	
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (!CommandList.IsImmediate())
	{
		bool bClearInParallelBuffer = false;
		
		for (uint32 RenderTargetIndex = 0; RenderTargetIndex < MaxMetalRenderTargets; RenderTargetIndex++)
		{
			if (RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets && RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex].Texture != nullptr)
			{
				const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
				if(RenderTargetView.LoadAction == ERenderTargetLoadAction::EClear)
				{
					bClearInParallelBuffer = true;
				}
			}
		}
		
		if (bClearInParallelBuffer)
		{
			UE_LOG(LogMetal, Warning, TEXT("One or more render targets bound for clear during parallel encoding: this will not behave as expected because each command-buffer will clear the target of the previous contents."));
		}
		
		if (RenderTargetsInfo.DepthStencilRenderTarget.Texture != nullptr)
		{
			if(RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction == ERenderTargetLoadAction::EClear)
			{
				UE_LOG(LogMetal, Warning, TEXT("Depth-target bound for clear during parallel encoding: this will not behave as expected because each command-buffer will clear the target of the previous contents."));
			}
			if(RenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction == ERenderTargetLoadAction::EClear)
			{
				UE_LOG(LogMetal, Warning, TEXT("Stencil-target bound for clear during parallel encoding: this will not behave as expected because each command-buffer will clear the target of the previous contents."));
			}
		}
	}
#endif
	
	if (IsFeatureLevelSupported( GMaxRHIShaderPlatform, ERHIFeatureLevel::SM4 ))
	{
		StateCache.SetRenderTargetsInfo(RenderTargetsInfo, QueryBuffer->GetCurrentQueryBuffer()->Buffer, bReset);
	}
	else
	{
		StateCache.SetRenderTargetsInfo(RenderTargetsInfo, NULL, bReset);
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
		FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
		FMetalIndexBuffer* IB = SRV->SourceIndexBuffer.GetReference();
		if (Texture)
		{
			FMetalSurface* Surface = SRV->TextureView;
			if (Surface != nullptr)
			{
				Surface->UpdateSRV(SRV->SourceTexture);
				GetCommandEncoder().SetShaderTexture(ShaderStage, Surface->Texture, BindIndex);
			}
			else
			{
				GetCommandEncoder().SetShaderTexture(ShaderStage, nil, BindIndex);
			}
		}
		else if (VB)
		{
			BufferSideTable[ShaderStage][BindIndex] = VB->GetSize();
			if (!VB->Data)
			{
		        GetCommandEncoder().SetShaderBuffer(ShaderStage, VB->Buffer, 0, BindIndex);
			}
			else
			{
		        GetCommandEncoder().SetShaderBytes(ShaderStage, VB->Data, 0, BindIndex);
			}
		}
		else if (IB)
		{
			BufferSideTable[ShaderStage][BindIndex] = IB->GetSize();
			GetCommandEncoder().SetShaderBuffer(ShaderStage, IB->Buffer, 0, BindIndex);
		}
	}
	else
	{
		GetCommandEncoder().SetShaderTexture(ShaderStage, nil, BindIndex);
	}
	FShaderCache::SetSRV(ShaderStage, BindIndex, SRV);
}

void FMetalContext::SetShaderUnorderedAccessView(EShaderFrequency ShaderStage, uint32 BindIndex, FMetalUnorderedAccessView* RESTRICT UAV)
{
	if (UAV)
	{
		// figure out which one of the resources we need to set
		FMetalStructuredBuffer* StructuredBuffer = UAV->SourceStructuredBuffer.GetReference();
		FMetalVertexBuffer* VertexBuffer = UAV->SourceVertexBuffer.GetReference();
		FRHITexture* Texture = UAV->SourceTexture.GetReference();
		if (StructuredBuffer)
		{
			BufferSideTable[ShaderStage][BindIndex] = StructuredBuffer->GetSize();
			GetCommandEncoder().SetShaderBuffer(ShaderStage, StructuredBuffer->Buffer, 0, BindIndex);
		}
		else if (VertexBuffer)
		{
			BufferSideTable[ShaderStage][BindIndex] = VertexBuffer->GetSize();
			check(!VertexBuffer->Data && VertexBuffer->Buffer);
	        GetCommandEncoder().SetShaderBuffer(ShaderStage, VertexBuffer->Buffer, 0, BindIndex);
		}
		else if (Texture)
		{
			FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture);
			if (Surface != nullptr)
			{
				Surface->bWritten = true;
				GetCommandEncoder().SetShaderTexture(ShaderStage, Surface->Texture, BindIndex);
			}
			else
			{
				GetCommandEncoder().SetShaderTexture(ShaderStage, nil, BindIndex);
			}
		}
	}
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

void FMetalContext::SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalUnorderedAccessView* RESTRICT UAV, float CurrentTime)
{
	switch (ShaderStage)
	{
		case CrossCompiler::SHADER_STAGE_PIXEL:
			SetShaderUnorderedAccessView(SF_Pixel, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_VERTEX:
			SetShaderUnorderedAccessView(SF_Vertex, BindIndex, UAV);
			break;
			
		case CrossCompiler::SHADER_STAGE_COMPUTE:
			SetShaderUnorderedAccessView(SF_Compute, BindIndex, UAV);
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
		SetShaderResourcesFromBuffer<FMetalUnorderedAccessView>(ShaderStage, Buffer, Shader->Bindings.ShaderResourceTable.UnorderedAccessViewMap.GetData(), BufferIndex);
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
	check(CommandEncoder.IsRenderCommandEncoderActive());
	
	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedUniformBuffers(CurrentBoundShaderState, nullptr, CrossCompiler::SHADER_STAGE_VERTEX, StateCache.GetBoundUniformBuffers(SF_Vertex), CurrentBoundShaderState->VertexShader->UniformBuffersCopyInfo);
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_VERTEX, CurrentBoundShaderState->VertexShader->Bindings);
	
	if (IsValidRef(CurrentBoundShaderState->PixelShader))
	{
		StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedUniformBuffers(CurrentBoundShaderState, nullptr, CrossCompiler::SHADER_STAGE_PIXEL, StateCache.GetBoundUniformBuffers(SF_Pixel), CurrentBoundShaderState->PixelShader->UniformBuffersCopyInfo);
		StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_PIXEL, CurrentBoundShaderState->PixelShader->Bindings);
	}
}

void FMetalContext::ConditionalSwitchToGraphics(bool bRTChangePending, bool bRTChangeForce)
{
	ConditionalSubmit(bRTChangePending, bRTChangeForce);
	
	StateCache.ConditionalSwitchToRender();
}

void FMetalContext::ConditionalSwitchToBlit()
{
	ConditionalSubmit();
	
	StateCache.ConditionalSwitchToBlit();
}

void FMetalContext::ConditionalSwitchToCompute()
{
	ConditionalSubmit();
	
	if (!CommandEncoder.IsComputeCommandEncoderActive())
	{
		StateCache.ConditionalSwitchToCompute();
		CommandEncoder.RestoreComputeCommandEncodingState();
	}
}

void FMetalContext::ConditionalSubmit(bool bRTChangePending, bool bRTChangeForce)
{
	if ((GMetalCommandBufferCommitThreshold > 0 && OutstandingOpCount >= GMetalCommandBufferCommitThreshold) || (bRTChangePending && bRTChangeForce))
	{
		bool const bEncoderActive = CommandEncoder.IsRenderCommandEncoderActive() || CommandEncoder.IsComputeCommandEncoderActive() || CommandEncoder.IsBlitCommandEncoderActive();
		
		// Normally we break only at natural encoder boundaries but in debug modes we can break at anytime.
		bool bCanConditionallySubmit = !bEncoderActive;
#if METAL_DEBUG_OPTIONS
		bCanConditionallySubmit |= (CommandQueue.GetRuntimeDebuggingLevel() >= EMetalDebugLevelConditionalSubmit);
#endif

		// AJB: This triggers a 'unset' of the RT. Causing a Load/Store at potentially awkward times.
		// check that the load/store behaviour of the current RT setup will allows resumption without affecting RT content.
		// i.e. - dont want to clear 1/2 way through a pass either.
		if (bRTChangePending == false && CommandEncoder.IsRenderCommandEncoderActive())
		{
			const FRHISetRenderTargetsInfo& CurrentRenderTargets = StateCache.GetRenderTargetsInfo();
			const bool bIsMSAAActive = StateCache.GetHasValidRenderTarget() && StateCache.GetRenderPipelineDesc().SampleCount != 1;
			bCanConditionallySubmit = !bIsMSAAActive;
			for (int32 RenderTargetIndex = 0; RenderTargetIndex < CurrentRenderTargets.NumColorRenderTargets && bCanConditionallySubmit; RenderTargetIndex++)
			{
				const FRHIRenderTargetView& RenderTargetView = CurrentRenderTargets.ColorRenderTarget[RenderTargetIndex];
				bCanConditionallySubmit = (bCanConditionallySubmit
					&& RenderTargetView.LoadAction == ERenderTargetLoadAction::ELoad
					&& RenderTargetView.StoreAction == ERenderTargetStoreAction::EStore);
			}
			bCanConditionallySubmit = (bCanConditionallySubmit
				&& (!CurrentRenderTargets.DepthStencilRenderTarget.Texture
				|| (CurrentRenderTargets.DepthStencilRenderTarget.DepthLoadAction == ERenderTargetLoadAction::ELoad
					&& CurrentRenderTargets.DepthStencilRenderTarget.DepthStoreAction == ERenderTargetStoreAction::EStore
					&& CurrentRenderTargets.DepthStencilRenderTarget.StencilLoadAction == ERenderTargetLoadAction::ELoad
					&& CurrentRenderTargets.DepthStencilRenderTarget.GetStencilStoreAction() == ERenderTargetStoreAction::EStore)));
		}

		// Only permit conditional submit if one of the conditions is met.
		if(bRTChangePending || bCanConditionallySubmit)
		{
			SubmitCommandsHint(EMetalSubmitFlagsCreateCommandBuffer|EMetalSubmitFlagsBreakCommandBuffer);
		}
	}
}

void FMetalContext::Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	ConditionalSwitchToCompute();
	check(CommandEncoder.IsComputeCommandEncoderActive());
	
	TRefCountPtr<FMetalComputeShader> CurrentComputeShader = StateCache.GetComputeShader();
	check(CurrentComputeShader);
	
	auto* ComputeShader = (FMetalComputeShader*)CurrentComputeShader;
	
	SetResourcesFromTables(ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE);
	
	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState = StateCache.GetBoundShaderState();
	
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedUniformBuffers(CurrentBoundShaderState, ComputeShader, CrossCompiler::SHADER_STAGE_COMPUTE, StateCache.GetBoundUniformBuffers(SF_Compute), ComputeShader->UniformBuffersCopyInfo);
	StateCache.GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).CommitPackedGlobals(this, CrossCompiler::SHADER_STAGE_COMPUTE, ComputeShader->Bindings);
	
	if (ComputeShader->SideTableBinding >= 0)
	{
		uint32 Offset = AllocateFromRingBuffer((sizeof(BufferSideTable) / SF_NumFrequencies));
		id<MTLBuffer> Buffer = GetRingBuffer();
		
		FMemory::Memcpy(((uint8*)[Buffer contents]) + Offset, BufferSideTable[SF_Compute], (sizeof(BufferSideTable) / SF_NumFrequencies));
		
		CommandEncoder.SetShaderBuffer(SF_Compute, Buffer, Offset, ComputeShader->SideTableBinding);
	}
	
	MTLSize ThreadgroupCounts = MTLSizeMake(ComputeShader->NumThreadsX, ComputeShader->NumThreadsY, ComputeShader->NumThreadsZ);
	check(ComputeShader->NumThreadsX > 0 && ComputeShader->NumThreadsY > 0 && ComputeShader->NumThreadsZ > 0);
	MTLSize Threadgroups = MTLSizeMake(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	//@todo-rco: setThreadgroupMemoryLength?
	[CommandEncoder.GetComputeCommandEncoder() dispatchThreadgroups:Threadgroups threadsPerThreadgroup:ThreadgroupCounts];
	
	OutstandingOpCount++;
}

#if METAL_API_1_1
void FMetalContext::DispatchIndirect(FMetalVertexBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
	ConditionalSwitchToCompute();
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
	
	METAL_DEBUG_COMMAND_BUFFER_TRACK_RES(GetCurrentCommandBuffer(), ArgumentBuffer->Buffer);
	[CommandEncoder.GetComputeCommandEncoder() dispatchThreadgroupsWithIndirectBuffer:ArgumentBuffer->Buffer indirectBufferOffset:ArgumentOffset threadsPerThreadgroup:ThreadgroupCounts];
	
	OutstandingOpCount++;
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
	: CmdContext(nullptr)
	{
	}
	
	virtual ~FMetalCommandContextContainer() override
	{
		if (CmdContext)
		{
			GetMetalDeviceContext().ReleaseContext(CmdContext);
			CmdContext = nullptr;
			check(!CmdContext);
		}
	}
	
	virtual IRHICommandContext* GetContext() override
	{
		CmdContext = GetMetalDeviceContext().AcquireContext();
		check(CmdContext);
		CmdContext->GetInternalContext().InitFrame(false);
		return CmdContext;
	}
	virtual void FinishContext() override
	{
		if (CmdContext)
		{
			CmdContext->GetInternalContext().FinishFrame();
		}
	}
	virtual void SubmitAndFreeContextContainer(int32 Index, int32 Num) override
	{
		if (CmdContext)
		{
			check(CmdContext->GetInternalContext().GetCurrentCommandBuffer() == nil);
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
