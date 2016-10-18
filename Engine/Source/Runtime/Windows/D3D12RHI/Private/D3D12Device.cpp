// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Device.cpp: D3D device RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "AllowWindowsPlatformTypes.h"
	#include <delayimp.h>

	#if D3D12_PROFILING_ENABLED
		#define USE_PIX 0
		#include "pix.h"
	#endif
#include "HideWindowsPlatformTypes.h"

namespace D3D12RHI
{
	extern void EmptyD3DSamplerStateCache();
}
using namespace D3D12RHI;

bool D3D12RHI_ShouldCreateWithD3DDebug()
{
	// Use a debug device if specified on the command line.
	static bool bCreateWithD3DDebug = 
		FParse::Param(FCommandLine::Get(), TEXT("d3ddebug")) ||
		FParse::Param(FCommandLine::Get(), TEXT("d3debug")) ||
		FParse::Param(FCommandLine::Get(), TEXT("dxdebug"));
	return bCreateWithD3DDebug;
}

bool D3D12RHI_ShouldCreateWithWarp()
{
	// Use the warp adapter if specified on the command line.
	static bool bCreateWithWarp = FParse::Param(FCommandLine::Get(), TEXT("warp"));
	return bCreateWithWarp;
}

bool D3D12RHI_ShouldAllowAsyncResourceCreation()
{
	static bool bAllowAsyncResourceCreation = !FParse::Param(FCommandLine::Get(), TEXT("nod3dasync"));
	return bAllowAsyncResourceCreation;
}

bool D3D12RHI_ShouldForceCompatibility()
{
	// Suppress the use of newer D3D12 features.
	static bool bForceCompatibility =
		FParse::Param(FCommandLine::Get(), TEXT("d3dcompat")) ||
		FParse::Param(FCommandLine::Get(), TEXT("d3d12compat"));
	return bForceCompatibility;
}

IMPLEMENT_MODULE(FD3D12DynamicRHIModule, D3D12RHI);

FD3D12DynamicRHI* FD3D12DynamicRHI::SingleD3DRHI = nullptr;

IRHICommandContext* FD3D12DynamicRHI::RHIGetDefaultContext()
{
	static IRHICommandContext* DefaultCommandContext = static_cast<IRHICommandContext*>(&GetRHIDevice()->GetDefaultCommandContext());

	check(DefaultCommandContext);
	return DefaultCommandContext;
}

IRHIComputeContext* FD3D12DynamicRHI::RHIGetDefaultAsyncComputeContext()
{
	static IRHIComputeContext* DefaultAsyncComputeContext = GEnableAsyncCompute ?
		static_cast<IRHIComputeContext*>(&GetRHIDevice()->GetDefaultAsyncComputeContext()) :
		static_cast<IRHIComputeContext*>(&GetRHIDevice()->GetDefaultCommandContext());

	check(DefaultAsyncComputeContext);
	return DefaultAsyncComputeContext;
}

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE

//todo recycle these to avoid alloc


class FD3D12CommandContextContainer : public IRHICommandContextContainer
{
	FD3D12Device* OwningDevice;
	FD3D12CommandContext* CmdContext;
	TArray<FD3D12CommandListHandle> CommandLists;

public:

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void* RawMemory);

	FD3D12CommandContextContainer(FD3D12Device* InOwningDevice)
		: OwningDevice(InOwningDevice), CmdContext(nullptr)
	{
		CommandLists.Reserve(16);
	}

	virtual ~FD3D12CommandContextContainer() override
	{
	}

	virtual IRHICommandContext* GetContext() override
	{
		check(CmdContext == nullptr);
		CmdContext = OwningDevice->ObtainCommandContext();
		check(CmdContext != nullptr);
		check(CmdContext->CommandListHandle == nullptr);

		CmdContext->OpenCommandList();
		CmdContext->ClearState();

		return CmdContext;
	}

	virtual void FinishContext() override
	{
		// We never "Finish" the default context. It gets submitted when FlushCommands() is called.
		check(!CmdContext->IsDefaultContext());

		CmdContext->Finish(CommandLists);

		OwningDevice->ReleaseCommandContext(CmdContext);
		CmdContext = nullptr;
	}

	virtual void SubmitAndFreeContextContainer(int32 Index, int32 Num) override
	{
		if (Index == 0)
		{
			check((IsInRenderingThread() || IsInRHIThread()));

			FD3D12CommandContext& DefaultContext = OwningDevice->GetDefaultCommandContext();

			// Don't really submit the default context yet, just start a new command list.
			// Close the command list, add it to the pending command lists, then open a new command list (with the previous state restored).
			DefaultContext.CloseCommandList();

			OwningDevice->PendingCommandLists.Add(DefaultContext.CommandListHandle);
			OwningDevice->PendingCommandListsTotalWorkCommands +=
				DefaultContext.numClears +
				DefaultContext.numCopies +
				DefaultContext.numDraws;

			DefaultContext.OpenCommandList(true);
		}

		// Add the current lists for execution (now or possibly later depending on the command list batching mode).
		for (int32 i = 0; i < CommandLists.Num(); ++i)
		{
			OwningDevice->PendingCommandLists.Add(CommandLists[i]);
			OwningDevice->PendingCommandListsTotalWorkCommands +=
				CommandLists[i].GetCurrentOwningContext()->numClears +
				CommandLists[i].GetCurrentOwningContext()->numCopies +
				CommandLists[i].GetCurrentOwningContext()->numDraws;
		}

		CommandLists.Reset();

		bool Flush = false;
		// If the GPU is starving (i.e. we are CPU bound) feed it asap!
		if (OwningDevice->IsGPUIdle() && OwningDevice->PendingCommandLists.Num() > 0)
		{
			Flush = true;
		}
		else
		{
			if (GCommandListBatchingMode != CLB_AggressiveBatching)
			{
				// Submit when the batch is finished.
				const bool FinalCommandListInBatch = Index == (Num - 1);
				if (FinalCommandListInBatch && OwningDevice->PendingCommandLists.Num() > 0)
				{
					Flush = true;
				}
			}
		}

		if (Flush)
		{
			OwningDevice->GetCommandListManager().ExecuteCommandLists(OwningDevice->PendingCommandLists);
			OwningDevice->PendingCommandLists.Reset();
			OwningDevice->PendingCommandListsTotalWorkCommands = 0;
		}

		delete this;
	}
};

void* FD3D12CommandContextContainer::operator new(size_t Size)
{
	return FMemory::Malloc(Size);
}

void FD3D12CommandContextContainer::operator delete(void* RawMemory)
{
	FMemory::Free(RawMemory);
}

IRHICommandContextContainer* FD3D12DynamicRHI::RHIGetCommandContextContainer()
{
	return new FD3D12CommandContextContainer(GetRHIDevice());
}

#endif // D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE

FD3D12CommandContext::FD3D12CommandContext(FD3D12Device* InParent, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc, bool InIsDefaultContext, bool InIsAsyncComputeContext) :
	OwningRHI(*InParent->GetOwningRHI()),
	bUsingTessellation(false),
	PendingNumVertices(0),
	PendingVertexDataStride(0),
	PendingPrimitiveType(0),
	PendingNumPrimitives(0),
	PendingMinVertexIndex(0),
	PendingNumIndices(0),
	PendingIndexDataStride(0),
	CurrentDepthTexture(nullptr),
	NumSimultaneousRenderTargets(0),
	NumUAVs(0),
	CurrentDSVAccessType(FExclusiveDepthStencil::DepthWrite_StencilWrite),
	bDiscardSharedConstants(false),
	bIsDefaultContext(InIsDefaultContext),
	bIsAsyncComputeContext(InIsAsyncComputeContext),
	CommandListHandle(),
	CommandAllocator(nullptr),
	CommandAllocatorManager(InParent, InIsAsyncComputeContext ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT),
	FastAllocatorPagePool(InParent, D3D12_HEAP_TYPE_UPLOAD, 1024 * 512),
	FastAllocator(InParent, &FastAllocatorPagePool),
	ConstantsAllocatorPagePool(InParent, InParent->GetConstantBufferPageProperties(), 1024 * 512),
	ConstantsAllocator(InParent, &ConstantsAllocatorPagePool),
	DynamicVB(InParent, FastAllocator),
	DynamicIB(InParent, FastAllocator),
	FD3D12DeviceChild(InParent)
{
	FMemory::Memzero(DirtyUniformBuffers, sizeof(DirtyUniformBuffers));

	// Initialize the constant buffers.
	InitConstantBuffers();

	StateCache.Init(GetParentDevice(), this, nullptr, SubHeapDesc);
}

FD3D12CommandContext::~FD3D12CommandContext()
{
	ClearState();
	StateCache.Clear();	// Clears its descriptor cache too

	// Release references to bound uniform buffers.
	for (int32 Frequency = 0; Frequency < SF_NumFrequencies; ++Frequency)
	{
		for (int32 BindIndex = 0; BindIndex < MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE; ++BindIndex)
		{
			BoundUniformBuffers[Frequency][BindIndex].SafeRelease();
		}
	}
}

FD3D12Device::FD3D12Device(FD3D12DynamicRHI* InOwningRHI, IDXGIFactory4* InDXGIFactory, FD3D12Adapter& InAdapter) :
	OwningRHI(InOwningRHI),
	DXGIFactory(InDXGIFactory),
	DeviceAdapter(InAdapter),
	bDeviceRemoved(false),
	RTVAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256),
	DSVAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256),
	SRVAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024),
	UAVAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024),
	SamplerAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128),
	CBVAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024),
	SamplerID(0),
	OcclusionQueryHeap(this, D3D12_QUERY_HEAP_TYPE_OCCLUSION, 32768),
	PipelineStateCache(this),
	ResourceHelper(this),
	DeferredDeletionQueue(this),
	DefaultBufferAllocator(this),
	PendingCommandListsTotalWorkCommands(0),
	RootSignatureManager(this),
	CommandListManager(this, D3D12_COMMAND_LIST_TYPE_DIRECT),
	CopyCommandListManager(this, D3D12_COMMAND_LIST_TYPE_COPY),
	AsyncCommandListManager(this, D3D12_COMMAND_LIST_TYPE_COMPUTE),
	TextureStreamingCommandAllocatorManager(this, D3D12_COMMAND_LIST_TYPE_COPY),
	GlobalSamplerHeap(this),
	GlobalViewHeap(this),
	FirstFrameSeen(false),
	DefaultFastAllocatorPagePool(this, D3D12_HEAP_TYPE_UPLOAD, 1024 * 1024 * 4),
	DefaultFastAllocator(this, &DefaultFastAllocatorPagePool),
	BufferInitializerFastAllocatorPagePool(this, D3D12_HEAP_TYPE_UPLOAD, 1024 * 512),
	BufferInitializerFastAllocator(this, &BufferInitializerFastAllocatorPagePool),
	DefaultUploadHeapAllocator(this, FString(L"Upload Buffer Allocator"),kManualSubAllocationStrategy, DEFAULT_CONTEXT_UPLOAD_POOL_MAX_ALLOC_SIZE, DEFAULT_CONTEXT_UPLOAD_POOL_SIZE, DEFAULT_CONTEXT_UPLOAD_POOL_ALIGNMENT),
	TextureAllocator(this),
	FenceCorePool(this)
{
}

FD3D12ThreadSafeFastAllocator* FD3D12DynamicRHI::HelperThreadDynamicHeapAllocator = nullptr;

TAutoConsoleVariable<int32> CVarD3D12ZeroBufferSizeInMB(
	TEXT("d3d12.ZeroBufferSizeInMB"),
	4,
	TEXT("The D3D12 RHI needs a static allocation of zeroes to use when streaming textures asynchronously. It should be large enough to support the largest mipmap you need to stream. The default is 4MB."),
	ECVF_ReadOnly
	);

FD3D12DynamicRHI::FD3D12DynamicRHI(IDXGIFactory4* InDXGIFactory, FD3D12Adapter& InAdapter) :
	DXGIFactory(InDXGIFactory),
	CommitResourceTableCycles(0),
	CacheResourceTableCalls(0),
	CacheResourceTableCycles(0),
	SetShaderTextureCycles(0),
	SetShaderTextureCalls(0),
	SetTextureInTableCalls(0),
	SceneFrameCounter(0),
	ResourceTableFrameCounter(INDEX_NONE),
	NumThreadDynamicHeapAllocators(0),
	ViewportFrameCounter(0),
	MainAdapter(InAdapter),
	MainDevice(nullptr)
{
	FMemory::Memzero(ThreadDynamicHeapAllocatorArray, sizeof(ThreadDynamicHeapAllocatorArray));

	// The FD3D12DynamicRHI must be a singleton
	check(SingleD3DRHI == nullptr);

	// This should be called once at the start 
	check(IsInGameThread());
	check(!GIsThreadedRendering);

	SingleD3DRHI = this;

	check(MainAdapter.IsValid());
	MainDevice = new FD3D12Device(this, DXGIFactory, MainAdapter);

	FeatureLevel = MainAdapter.MaxSupportedFeatureLevel;

	GPUProfilingData.Init(MainDevice);

	// Allocate a buffer of zeroes. This is used when we need to pass D3D memory
	// that we don't care about and will overwrite with valid data in the future.
	ZeroBufferSize = FMath::Max(CVarD3D12ZeroBufferSizeInMB.GetValueOnAnyThread(), 0) * (1 << 20);
	ZeroBuffer = FMemory::Malloc(ZeroBufferSize);
	FMemory::Memzero(ZeroBuffer, ZeroBufferSize);

	GPoolSizeVRAMPercentage = 0;
	GTexturePoolSize = 0;
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSizeVRAMPercentage"), GPoolSizeVRAMPercentage, GEngineIni);

	// Initialize the RHI capabilities.
	check(FeatureLevel == D3D_FEATURE_LEVEL_11_0 || FeatureLevel == D3D_FEATURE_LEVEL_10_0);

	if (FeatureLevel == D3D_FEATURE_LEVEL_10_0)
	{
		GSupportsDepthFetchDuringDepthTest = false;
	}

	// ES2 feature level emulation in D3D11
	if (FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES2")) && !GIsEditor)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::ES2;
		GMaxRHIShaderPlatform = SP_PCD3D_ES2;
	}
	else if ((FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1"))) && !GIsEditor)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
		GMaxRHIShaderPlatform = SP_PCD3D_ES3_1;
	}
	else if (FeatureLevel == D3D_FEATURE_LEVEL_11_0)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		GMaxRHIShaderPlatform = SP_PCD3D_SM5;
	}
	else if (FeatureLevel == D3D_FEATURE_LEVEL_10_0)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM4;
		GMaxRHIShaderPlatform = SP_PCD3D_SM4;
	}

	// Initialize the platform pixel format map.
	GPixelFormats[ PF_Unknown		].PlatformFormat	= DXGI_FORMAT_UNKNOWN;
	GPixelFormats[ PF_A32B32G32R32F	].PlatformFormat	= DXGI_FORMAT_R32G32B32A32_FLOAT;
	GPixelFormats[ PF_B8G8R8A8		].PlatformFormat	= DXGI_FORMAT_B8G8R8A8_TYPELESS;
	GPixelFormats[ PF_G8			].PlatformFormat	= DXGI_FORMAT_R8_UNORM;
	GPixelFormats[ PF_G16			].PlatformFormat	= DXGI_FORMAT_R16_UNORM;
	GPixelFormats[ PF_DXT1			].PlatformFormat	= DXGI_FORMAT_BC1_TYPELESS;
	GPixelFormats[ PF_DXT3			].PlatformFormat	= DXGI_FORMAT_BC2_TYPELESS;
	GPixelFormats[ PF_DXT5			].PlatformFormat	= DXGI_FORMAT_BC3_TYPELESS;
	GPixelFormats[ PF_BC4			].PlatformFormat	= DXGI_FORMAT_BC4_UNORM;
	GPixelFormats[ PF_UYVY			].PlatformFormat	= DXGI_FORMAT_UNKNOWN;		// TODO: Not supported in D3D11
#if DEPTH_32_BIT_CONVERSION
	GPixelFormats[ PF_DepthStencil	].PlatformFormat	= DXGI_FORMAT_R32G8X24_TYPELESS; 
	GPixelFormats[ PF_DepthStencil	].BlockBytes		= 5;
	GPixelFormats[ PF_X24_G8 ].PlatformFormat			= DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
	GPixelFormats[ PF_X24_G8].BlockBytes				= 5;
#else
	GPixelFormats[ PF_DepthStencil	].PlatformFormat	= DXGI_FORMAT_R24G8_TYPELESS;
	GPixelFormats[ PF_DepthStencil	].BlockBytes		= 4;
	GPixelFormats[ PF_X24_G8 ].PlatformFormat			= DXGI_FORMAT_X24_TYPELESS_G8_UINT;
	GPixelFormats[ PF_X24_G8].BlockBytes				= 4;
#endif
	GPixelFormats[ PF_ShadowDepth	].PlatformFormat	= DXGI_FORMAT_R16_TYPELESS;
	GPixelFormats[ PF_ShadowDepth	].BlockBytes		= 2;
	GPixelFormats[ PF_R32_FLOAT		].PlatformFormat	= DXGI_FORMAT_R32_FLOAT;
	GPixelFormats[ PF_G16R16		].PlatformFormat	= DXGI_FORMAT_R16G16_UNORM;
	GPixelFormats[ PF_G16R16F		].PlatformFormat	= DXGI_FORMAT_R16G16_FLOAT;
	GPixelFormats[ PF_G16R16F_FILTER].PlatformFormat	= DXGI_FORMAT_R16G16_FLOAT;
	GPixelFormats[ PF_G32R32F		].PlatformFormat	= DXGI_FORMAT_R32G32_FLOAT;
	GPixelFormats[ PF_A2B10G10R10   ].PlatformFormat    = DXGI_FORMAT_R10G10B10A2_UNORM;
	GPixelFormats[ PF_A16B16G16R16  ].PlatformFormat    = DXGI_FORMAT_R16G16B16A16_UNORM;
	GPixelFormats[ PF_D24 ].PlatformFormat				= DXGI_FORMAT_R24G8_TYPELESS;
	GPixelFormats[ PF_R16F			].PlatformFormat	= DXGI_FORMAT_R16_FLOAT;
	GPixelFormats[ PF_R16F_FILTER	].PlatformFormat	= DXGI_FORMAT_R16_FLOAT;

	GPixelFormats[ PF_FloatRGB	].PlatformFormat		= DXGI_FORMAT_R11G11B10_FLOAT;
	GPixelFormats[ PF_FloatRGB	].BlockBytes			= 4;
	GPixelFormats[ PF_FloatRGBA	].PlatformFormat		= DXGI_FORMAT_R16G16B16A16_FLOAT;
	GPixelFormats[ PF_FloatRGBA	].BlockBytes			= 8;

	GPixelFormats[ PF_FloatR11G11B10].PlatformFormat	= DXGI_FORMAT_R11G11B10_FLOAT;
	GPixelFormats[ PF_FloatR11G11B10].BlockBytes		= 4;
	GPixelFormats[ PF_FloatR11G11B10].Supported			= true;

	GPixelFormats[ PF_V8U8			].PlatformFormat	= DXGI_FORMAT_R8G8_SNORM;
	GPixelFormats[ PF_BC5			].PlatformFormat	= DXGI_FORMAT_BC5_UNORM;
	GPixelFormats[ PF_A1			].PlatformFormat	= DXGI_FORMAT_R1_UNORM; // Not supported for rendering.
	GPixelFormats[ PF_A8			].PlatformFormat	= DXGI_FORMAT_A8_UNORM;
	GPixelFormats[ PF_R32_UINT		].PlatformFormat	= DXGI_FORMAT_R32_UINT;
	GPixelFormats[ PF_R32_SINT		].PlatformFormat	= DXGI_FORMAT_R32_SINT;

	GPixelFormats[ PF_R16_UINT         ].PlatformFormat = DXGI_FORMAT_R16_UINT;
	GPixelFormats[ PF_R16_SINT         ].PlatformFormat = DXGI_FORMAT_R16_SINT;
	GPixelFormats[ PF_R16G16B16A16_UINT].PlatformFormat = DXGI_FORMAT_R16G16B16A16_UINT;
	GPixelFormats[ PF_R16G16B16A16_SINT].PlatformFormat = DXGI_FORMAT_R16G16B16A16_SINT;

	GPixelFormats[ PF_R5G6B5_UNORM	].PlatformFormat	= DXGI_FORMAT_B5G6R5_UNORM;
	GPixelFormats[ PF_R8G8B8A8		].PlatformFormat	= DXGI_FORMAT_R8G8B8A8_TYPELESS;
	GPixelFormats[ PF_R8G8			].PlatformFormat	= DXGI_FORMAT_R8G8_UNORM;
	GPixelFormats[ PF_R32G32B32A32_UINT].PlatformFormat = DXGI_FORMAT_R32G32B32A32_UINT;
	GPixelFormats[ PF_R16G16_UINT].PlatformFormat = DXGI_FORMAT_R16G16_UINT;

	GPixelFormats[ PF_BC6H			].PlatformFormat	= DXGI_FORMAT_BC6H_UF16;
	GPixelFormats[ PF_BC7			].PlatformFormat	= DXGI_FORMAT_BC7_TYPELESS;
	GPixelFormats[ PF_R8_UINT		].PlatformFormat	= DXGI_FORMAT_R8_UINT;

	// MS - Not doing any feature level checks. D3D12 currently supports these limits.
	// However this may need to be revisited if new feature levels are introduced with different HW requirement
	GSupportsSeparateRenderTargetBlendState = true;
	GMaxTextureDimensions = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	GMaxCubeTextureDimensions = D3D12_REQ_TEXTURECUBE_DIMENSION;
	GMaxTextureArrayLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;

	GMaxTextureMipCount = FMath::CeilLogTwo(GMaxTextureDimensions) + 1;
	GMaxTextureMipCount = FMath::Min<int32>(MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount);
	GMaxShadowDepthBufferSizeX = 4096;
	GMaxShadowDepthBufferSizeY = 4096;

	// Enable multithreading if not in the editor (editor crashes with multithreading enabled).
	if (!GIsEditor)
	{
		GRHISupportsRHIThread = true;
	}
	GRHISupportsParallelRHIExecute = D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE;

	GSupportsTimestampRenderQueries = true;
	GSupportsParallelOcclusionQueries = true;

	// Disable Async compute by default for now.
	GEnableAsyncCompute = false;
}

void FD3D12Device::CreateCommandContexts()
{
	check(CommandContextArray.Num() == 0);
	check(AsyncComputeContextArray.Num() == 0);

	const uint32 NumContexts = FTaskGraphInterface::Get().GetNumWorkerThreads() + 1;
	const uint32 NumAsyncComputeContexts = GEnableAsyncCompute ? 1 : 0;
	const uint32 TotalContexts = NumContexts + NumAsyncComputeContexts;
	
	// We never make the default context free for allocation by the context containers
	CommandContextArray.Reserve(NumContexts);
	FreeCommandContexts.Reserve(NumContexts - 1);
	AsyncComputeContextArray.Reserve(NumAsyncComputeContexts);

	const uint32 DescriptorSuballocationPerContext = GlobalViewHeap.GetTotalSize() / TotalContexts;
	uint32 CurrentGlobalHeapOffset = 0;

	for (uint32 i = 0; i < NumContexts; ++i)
	{
		FD3D12SubAllocatedOnlineHeap::SubAllocationDesc SubHeapDesc(&GlobalViewHeap, CurrentGlobalHeapOffset, DescriptorSuballocationPerContext);

		const bool bIsDefaultContext = (i == 0);
		FD3D12CommandContext* NewCmdContext = new FD3D12CommandContext(this, SubHeapDesc, bIsDefaultContext);
		CurrentGlobalHeapOffset += DescriptorSuballocationPerContext;

		// without that the first RHIClear would get a scissor rect of (0,0)-(0,0) which means we get a draw call clear 
		NewCmdContext->RHISetScissorRect(false, 0, 0, 0, 0);

		CommandContextArray.Add(NewCmdContext);

		// Make available all but the first command context for parallel threads
		if (!bIsDefaultContext)
		{
			FreeCommandContexts.Add(CommandContextArray[i]);
		}
	}

	for (uint32 i = 0; i < NumAsyncComputeContexts; ++i)
	{
		FD3D12SubAllocatedOnlineHeap::SubAllocationDesc SubHeapDesc(&GlobalViewHeap, CurrentGlobalHeapOffset, DescriptorSuballocationPerContext);

		const bool bIsDefaultContext = (i == 0);
		const bool bIsAsyncComputeContext = true;
		FD3D12CommandContext* NewCmdContext = new FD3D12CommandContext(this, SubHeapDesc, bIsDefaultContext, bIsAsyncComputeContext);
		CurrentGlobalHeapOffset += DescriptorSuballocationPerContext;

		AsyncComputeContextArray.Add(NewCmdContext);
	}

	CommandContextArray[0]->OpenCommandList();
	if (GEnableAsyncCompute)
	{
		AsyncComputeContextArray[0]->OpenCommandList();
	}

	DefaultUploadHeapAllocator.SetCurrentCommandContext(CommandContextArray[0]);
}

FD3D12DynamicRHI::~FD3D12DynamicRHI()
{
	UE_LOG(LogD3D12RHI, Log, TEXT("~FD3D12DynamicRHI"));

	check(MainDevice == nullptr);
}

void FD3D12DynamicRHI::Shutdown()
{
	check(IsInGameThread() && IsInRenderingThread());  // require that the render thread has been shut down

	// Cleanup the D3D devices.
	MainDevice->CleanupD3DDevice();

	// Take a reference on the ID3D12Device so that we can delete the FD3D12Device
	// and have it's children correctly release ID3D12* objects via RAII
	TRefCountPtr<ID3D12Device> Direct3DDevice = MainDevice->GetDevice();

	delete(MainDevice);

	const bool bWithD3DDebug = D3D12RHI_ShouldCreateWithD3DDebug();
	if (bWithD3DDebug)
	{
		TRefCountPtr<ID3D12DebugDevice> Debug;

		if (SUCCEEDED(Direct3DDevice->QueryInterface(IID_PPV_ARGS(Debug.GetInitReference()))))
		{
			D3D12_RLDO_FLAGS rldoFlags = D3D12_RLDO_DETAIL;

			Debug->ReportLiveDeviceObjects(rldoFlags);
		}
	}

	// Finally remove the ID3D12Device
	Direct3DDevice = nullptr;


	MainDevice = nullptr;

	// Release buffered timestamp queries
	GPUProfilingData.FrameTiming.ReleaseResource();

	// Release the buffer of zeroes.
	FMemory::Free(ZeroBuffer);
	ZeroBuffer = NULL;
	ZeroBufferSize = 0;
}

void FD3D12CommandContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
	if (IsDefaultContext())
	{
		OwningRHI.PushGPUEvent(Name, Color);
	}

#if USE_PIX
	PIXBeginEvent(CommandListHandle.CommandList(), PIX_COLOR_DEFAULT, Name);
#endif
}

void FD3D12CommandContext::RHIPopEvent()
{
	if (IsDefaultContext())
	{
		OwningRHI.PopGPUEvent();
	}

#if USE_PIX
	PIXEndEvent(CommandListHandle.CommandList());
#endif
}

/**
* Returns a supported screen resolution that most closely matches the input.
* @param Width - Input: Desired resolution width in pixels. Output: A width that the platform supports.
* @param Height - Input: Desired resolution height in pixels. Output: A height that the platform supports.
*/
void FD3D12DynamicRHI::RHIGetSupportedResolution(uint32& Width, uint32& Height)
{
	uint32 InitializedMode = false;
	DXGI_MODE_DESC BestMode;
	BestMode.Width = 0;
	BestMode.Height = 0;

	{
		HRESULT HResult = S_OK;
		TRefCountPtr<IDXGIAdapter> Adapter;
		HResult = DXGIFactory->EnumAdapters(GetRHIDevice()->GetAdapterIndex(), Adapter.GetInitReference());
		if (DXGI_ERROR_NOT_FOUND == HResult)
		{
			return;
		}
		if (FAILED(HResult))
		{
			return;
		}

		// get the description of the adapter
		DXGI_ADAPTER_DESC AdapterDesc;
		VERIFYD3D12RESULT(Adapter->GetDesc(&AdapterDesc));

#ifndef PLATFORM_XBOXONE // No need for display mode enumeration on console
		// Enumerate outputs for this adapter
		// TODO: Cap at 1 for default output
		for (uint32 o = 0;o < 1; o++)
		{
			TRefCountPtr<IDXGIOutput> Output;
			HResult = Adapter->EnumOutputs(o, Output.GetInitReference());
			if (DXGI_ERROR_NOT_FOUND == HResult)
			{
				break;
			}
			if (FAILED(HResult))
			{
				return;
			}

			// TODO: GetDisplayModeList is a terribly SLOW call.  It can take up to a second per invocation.
			//  We might want to work around some DXGI badness here.
			DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			uint32 NumModes = 0;
			HResult = Output->GetDisplayModeList(Format, 0, &NumModes, NULL);
			if (HResult == DXGI_ERROR_NOT_FOUND)
			{
				return;
			}
			else if (HResult == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
			{
				UE_LOG(LogD3D12RHI, Fatal,
					TEXT("This application cannot be run over a remote desktop configuration")
					);
				return;
			}
			DXGI_MODE_DESC* ModeList = new DXGI_MODE_DESC[NumModes];
			VERIFYD3D12RESULT(Output->GetDisplayModeList(Format, 0, &NumModes, ModeList));

			for (uint32 m = 0;m < NumModes;m++)
			{
				// Search for the best mode

				// Suppress static analysis warnings about a potentially out-of-bounds read access to ModeList. This is a false positive - Index is always within range.
				CA_SUPPRESS(6385);
				bool IsEqualOrBetterWidth = FMath::Abs((int32)ModeList[m].Width - (int32)Width) <= FMath::Abs((int32)BestMode.Width - (int32)Width);
				bool IsEqualOrBetterHeight = FMath::Abs((int32)ModeList[m].Height - (int32)Height) <= FMath::Abs((int32)BestMode.Height - (int32)Height);
				if (!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
				{
					BestMode = ModeList[m];
					InitializedMode = true;
				}
			}

			delete[] ModeList;
		}
#endif // PLATFORM_XBOXONE
	}

	check(InitializedMode);
	Width = BestMode.Width;
	Height = BestMode.Height;
}

void FD3D12DynamicRHI::GetBestSupportedMSAASetting(DXGI_FORMAT PlatformFormat, uint32 MSAACount, uint32& OutBestMSAACount, uint32& OutMSAAQualityLevels)
{
	//  We disable MSAA for Feature level 10
	if (GMaxRHIFeatureLevel == ERHIFeatureLevel::SM4)
	{
		OutBestMSAACount = 1;
		OutMSAAQualityLevels = 0;
		return;
	}

	// start counting down from current setting (indicated the current "best" count) and move down looking for support
	for (uint32 SampleCount = MSAACount; SampleCount > 0; SampleCount--)
	{
		// The multisampleQualityLevels struct serves as both the input and output to CheckFeatureSupport.
		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multisampleQualityLevels = {};
		multisampleQualityLevels.SampleCount = SampleCount;

		if (SUCCEEDED(GetRHIDevice()->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &multisampleQualityLevels, sizeof(multisampleQualityLevels))))
		{
			OutBestMSAACount = SampleCount;
			OutMSAAQualityLevels = multisampleQualityLevels.NumQualityLevels;
			break;
		}
	}

	return;
}

uint32 FD3D12DynamicRHI::GetMaxMSAAQuality(uint32 SampleCount)
{
	if (SampleCount <= DX_MAX_MSAA_COUNT)
	{
		// 0 has better quality (a more even distribution)
		// higher quality levels might be useful for non box filtered AA or when using weighted samples 
		return 0;
		//		return AvailableMSAAQualities[SampleCount];
	}
	// not supported
	return 0xffffffff;
}

bool FD3D12Device::IsGPUIdle()
{
	FD3D12Fence& Fence = CommandListManager.GetFence(EFenceType::FT_CommandList);
	return Fence.GetLastCompletedFence() >= (Fence.GetCurrentFence() - 1);
}

void FD3D12Device::CreateSignatures()
{
	// ExecuteIndirect command signatures
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.NumArgumentDescs = 1;
	commandSignatureDesc.ByteStride = 20;

	D3D12_INDIRECT_ARGUMENT_DESC indirectParameterDesc[1] = {};
	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	commandSignatureDesc.pArgumentDescs = indirectParameterDesc;

	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

	VERIFYD3D12RESULT(GetDevice()->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DrawIndirectCommandSignature.GetInitReference())));

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	VERIFYD3D12RESULT(GetDevice()->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DrawIndexedIndirectCommandSignature.GetInitReference())));

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
	VERIFYD3D12RESULT(GetDevice()->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DispatchIndirectCommandSignature.GetInitReference())));

	CD3DX12_HEAP_PROPERTIES CounterHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC CounterBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(COUNTER_HEAP_SIZE);

	VERIFYD3D12RESULT(
		GetDevice()->CreateCommittedResource(
			&CounterHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&CounterBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(CounterUploadHeap.GetInitReference())
			)
		);

	VERIFYD3D12RESULT(
		CounterUploadHeap->Map(
			0,
			NULL,
			&CounterUploadHeapData)
		);

	CounterUploadHeapIndex = 0;
}

void FD3D12Device::SetupAfterDeviceCreation()
{
	CreateSignatures();

	FString GraphicsCacheFile = FPaths::GameSavedDir() / TEXT("D3DGraphics.ushaderprecache");
	FString ComputeCacheFile = FPaths::GameSavedDir() / TEXT("D3DCompute.ushaderprecache");
	FString DriverBlobFilename = FPaths::GameSavedDir() / TEXT("D3DDriverByteCodeBlob.ushaderprecache");

	PipelineStateCache.Init(GraphicsCacheFile, ComputeCacheFile, DriverBlobFilename);
	PipelineStateCache.RebuildFromDiskCache();

	// Needs to be called before creating command contexts
	UpdateConstantBufferPageProperties();

	CreateCommandContexts();

	UpdateMSAASettings();

	if (GRHISupportsAsyncTextureCreation)
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Async texture creation enabled"));
	}
	else
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Async texture creation disabled: %s"),
			D3D12RHI_ShouldAllowAsyncResourceCreation() ? TEXT("no driver support") : TEXT("disabled by user"));
	}
}

void FD3D12Device::UpdateConstantBufferPageProperties()
{
	//In genera, constant buffers should use write-combine memory (i.e. upload heaps) for optimal performance
	bool bForceWriteBackConstantBuffers = false;

	if (bForceWriteBackConstantBuffers)
	{
		ConstantBufferPageProperties = GetDevice()->GetCustomHeapProperties(0, D3D12_HEAP_TYPE_UPLOAD);
		ConstantBufferPageProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	}
	else
	{
		ConstantBufferPageProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	}
}

void FD3D12Device::UpdateMSAASettings()
{
	check(DX_MAX_MSAA_COUNT == 8);

	// quality levels are only needed for CSAA which we cannot use with custom resolves

	// 0xffffffff means not available
	AvailableMSAAQualities[0] = 0xffffffff;
	AvailableMSAAQualities[1] = 0xffffffff;
	AvailableMSAAQualities[2] = 0;
	AvailableMSAAQualities[3] = 0xffffffff;
	AvailableMSAAQualities[4] = 0;
	AvailableMSAAQualities[5] = 0xffffffff;
	AvailableMSAAQualities[6] = 0xffffffff;
	AvailableMSAAQualities[7] = 0xffffffff;
	AvailableMSAAQualities[8] = 0;
}

void FD3D12Device::CleanupD3DDevice()
{
	if (GIsRHIInitialized)
	{
		// Execute
		FRHICommandListExecutor::CheckNoOutstandingCmdLists();
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

		// Ensure any pending rendering is finished
		GetCommandListManager().SignalFrameComplete(true);

		check(Direct3DDevice);

		OwningRHI->ClearOutstandingLocks();

		PipelineStateCache.Close();

		// Reset the RHI initialized flag.
		GIsRHIInitialized = false;

		check(!GIsCriticalError);

		// Ask all initialized FRenderResources to release their RHI resources.
		for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			FRenderResource* Resource = *ResourceIt;
			check(Resource->IsInitialized());
			Resource->ReleaseRHI();
		}

		for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseDynamicRHI();
		}

		// Flush all pending deletes before destroying the device.
		FRHIResource::FlushPendingDeletes();

		Viewports.Empty();
		DrawingViewport = nullptr;
		SamplerMap.Empty();

		ReleasePooledUniformBuffers();
		ReleasePooledTextures();

		// Wait for the command queues to flush
		CommandListManager.WaitForCommandQueueFlush();
		CopyCommandListManager.WaitForCommandQueueFlush();
		AsyncCommandListManager.WaitForCommandQueueFlush();

		// Delete array index 0 (the default context) last
		for (int32 i = CommandContextArray.Num() - 1; i >= 0; i--)
		{
			CommandContextArray[i]->FastAllocator.Destroy();
			CommandContextArray[i]->FastAllocatorPagePool.Destroy();
			CommandContextArray[i]->ConstantsAllocator.Destroy();
			CommandContextArray[i]->ConstantsAllocatorPagePool.Destroy();

			delete CommandContextArray[i];
			CommandContextArray[i] = nullptr;
		}

		// This is needed so the deferred deletion queue empties out completely because the resources in the
		// queue use the frame fence values. This includes any deleted resources that may have been deleted 
		// after the above signal frame
		GetCommandListManager().SignalFrameComplete(true);

		// Flush all pending deletes before destroying the device.
		FRHIResource::FlushPendingDeletes();

		// Cleanup the allocator near the end, as some resources may be returned to the allocator
		DefaultBufferAllocator.FreeDefaultBufferPools();

		DefaultFastAllocator.Destroy();
		DefaultFastAllocatorPagePool.Destroy();
		BufferInitializerFastAllocator.Destroy();
		BufferInitializerFastAllocatorPagePool.Destroy();

		DefaultUploadHeapAllocator.CleanUpAllocations();
		DefaultUploadHeapAllocator.Destroy();

		// Clean up the asnyc texture thread allocators
		for (uint32 i = 0; i < GetOwningRHI()->NumThreadDynamicHeapAllocators; i++)
		{
			GetOwningRHI()->ThreadDynamicHeapAllocatorArray[i]->Destroy();
			delete(GetOwningRHI()->ThreadDynamicHeapAllocatorArray[i]);
		}

		if (GetOwningRHI()->SharedFastAllocPool)
		{
			GetOwningRHI()->SharedFastAllocPool->Destroy();
			delete(GetOwningRHI()->SharedFastAllocPool);
		}

		TextureAllocator.CleanUpAllocations();
		TextureAllocator.Destroy();
		/*
		// Cleanup thread resources
		for (uint32 index; (index = InterlockedDecrement(&NumThreadDynamicHeapAllocators)) != (uint32)-1;)
		{
			FD3D12DynamicHeapAllocator* pHeapAllocator = ThreadDynamicHeapAllocatorArray[index];
			pHeapAllocator->ReleaseAllResources();
			delete(pHeapAllocator);
		}
		*/

		// Cleanup resources
		DeferredDeletionQueue.Clear();

		CommandListManager.Destroy();
		CopyCommandListManager.Destroy();
		AsyncCommandListManager.Destroy();

		OcclusionQueryHeap.Destroy();

		FenceCorePool.Destroy();

		D3DX12Residency::DestroyResidencyManager(ResidencyManager);
	}
}

void FD3D12DynamicRHI::RHIFlushResources()
{
	// Nothing to do (yet!)
}

void FD3D12DynamicRHI::RHIAcquireThreadOwnership()
{
}

void FD3D12DynamicRHI::RHIReleaseThreadOwnership()
{
	// Nothing to do
}

void FD3D12CommandContext::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable)
{
	StateCache.AutoFlushComputeShaderCache(bEnable);
}

void FD3D12CommandContext::RHIFlushComputeShaderCache()
{
	StateCache.FlushComputeShaderCache(true);
}

void* FD3D12DynamicRHI::RHIGetNativeDevice()
{
	return (void*)GetRHIDevice()->GetDevice();
}
