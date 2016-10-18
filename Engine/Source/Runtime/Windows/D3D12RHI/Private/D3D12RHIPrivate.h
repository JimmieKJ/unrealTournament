// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RHIPrivate.h: Private D3D RHI definitions.
	=============================================================================*/

#pragma once

#define D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE				1

#if UE_BUILD_SHIPPING
#define D3D12_PROFILING_ENABLED 0
#elif UE_BUILD_TEST
#define D3D12_PROFILING_ENABLED 1
#else
#define D3D12_PROFILING_ENABLED 1
#endif

// Dependencies.
#include "Core.h"
#include "RHI.h"
#include "GPUProfiler.h"
#include "ShaderCore.h"
#include "Engine.h"

DECLARE_LOG_CATEGORY_EXTERN(LogD3D12RHI, Log, All);

#include "D3D12RHI.h"
#include "Windows/D3D12RHIBasePrivate.h"
#include "StaticArray.h"

#include "D3D12Residency.h"

// D3D RHI public headers.
#include "../Public/D3D12Util.h"
#include "../Public/D3D12State.h"
#include "../Public/D3D12Resources.h"
#include "D3D12CommandList.h"
#include "D3D12Texture.h"
#include "../Public/D3D12Viewport.h"
#include "../Public/D3D12ConstantBuffer.h"
#include "D3D12Query.h"
#include "D3D12DiskCache.h"
#include "D3D12PipelineState.h"
#include "D3D12DescriptorCache.h"
#include "Windows/D3D12StateCache.h"
#include "D3D12DirectCommandListManager.h"
#include "D3D12Allocation.h"
#include "D3D12CommandContext.h"
#include "D3D12Device.h"
#include "D3D12Stats.h"

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
#define CHECK_SRV_TRANSITIONS 0
#else
#define CHECK_SRV_TRANSITIONS 0	// MSFT: Seb: TODO: ENable
#endif

// Definitions.
#define USE_D3D12RHI_RESOURCE_STATE_TRACKING 1	// Fully relying on the engine's resource barriers is a work in progress. For now, continue to use the D3D12 RHI's resource state tracking.

#define EXECUTE_DEBUG_COMMAND_LISTS 0
#define ENABLE_PLACED_RESOURCES 0 // Disabled due to a couple of NVidia bugs related to placed resources. Works fine on Intel
#define REMOVE_OLD_QUERY_BATCHES 1  // D3D12: MSFT: TODO: Works around a suspected UE4 InfiltratorDemo bug where a query is never released

#define DEFAULT_BUFFER_POOL_SIZE (8 * 1024 * 1024)
#define DEFAULT_BUFFER_POOL_MAX_ALLOC_SIZE (512 * 1024)
#define DEFAULT_CONTEXT_UPLOAD_POOL_SIZE (8 * 1024 * 1024)
#define DEFAULT_CONTEXT_UPLOAD_POOL_MAX_ALLOC_SIZE (4 * 1024 * 1024)
#define DEFAULT_CONTEXT_UPLOAD_POOL_ALIGNMENT (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
#define TEXTURE_POOL_SIZE (8 * 1024 * 1024)

#if DEBUG_RESOURCE_STATES
#define LOG_EXECUTE_COMMAND_LISTS 1
#define ASSERT_RESOURCE_STATES 0	// Disabled for now.
#define LOG_PRESENT 1
#else
#define LOG_EXECUTE_COMMAND_LISTS 0
#define ASSERT_RESOURCE_STATES 0
#define LOG_PRESENT 0
#endif

#if EXECUTE_DEBUG_COMMAND_LISTS
bool GIsDoingQuery = false;
#define DEBUG_EXECUTE_COMMAND_LIST(scope) if (!GIsDoingQuery) { scope##->FlushCommands(true); }
#define DEBUG_EXECUTE_COMMAND_CONTEXT(context) if (!GIsDoingQuery) { context##.FlushCommands(true); }
#define DEBUG_RHI_EXECUTE_COMMAND_LIST(scope) if (!GIsDoingQuery) { scope##->GetRHIDevice()->GetDefaultCommandContext().FlushCommands(true); }
#else
#define DEBUG_EXECUTE_COMMAND_LIST(scope) 
#define DEBUG_EXECUTE_COMMAND_CONTEXT(context) 
#define DEBUG_RHI_EXECUTE_COMMAND_LIST(scope) 
#endif

using namespace D3D12RHI;

/** The interface which is implemented by the dynamically bound RHI. */
class FD3D12DynamicRHI : public FDynamicRHI
{
	friend class FD3D12CommandContext;

	static FD3D12DynamicRHI* SingleD3DRHI;

public:

	static FD3D12DynamicRHI* GetD3DRHI() { return SingleD3DRHI; }

private:
	/** Global D3D12 lock list */
	TMap<FD3D12LockedKey, FD3D12LockedData> OutstandingLocks;
	FCriticalSection OutstandingLocksCS;

	/** Texture pool size */
	int64 RequestedTexturePoolSize;

public:

	inline void ClearOutstandingLocks()
	{
		FScopeLock Lock(&OutstandingLocksCS);
		OutstandingLocks.Empty();
	}

	inline void AddToOutstandingLocks(FD3D12LockedKey& Key, FD3D12LockedData& Value)
	{
		FScopeLock Lock(&OutstandingLocksCS);
		OutstandingLocks.Add(Key, Value);
	}

	inline void RemoveFromOutstandingLocks(FD3D12LockedKey& Key)
	{
		FScopeLock Lock(&OutstandingLocksCS);
		OutstandingLocks.Remove(Key);
	}

	inline FD3D12LockedData* FindInOutstandingLocks(FD3D12LockedKey& Key)
	{
		FScopeLock Lock(&OutstandingLocksCS);
		return OutstandingLocks.Find(Key);
	}

	/** Initialization constructor. */
	FD3D12DynamicRHI(IDXGIFactory4* InDXGIFactory, FD3D12Adapter& InAdapter);

	/** Destructor */
	virtual ~FD3D12DynamicRHI();

	// FDynamicRHI interface.
	virtual void Init() override;
	virtual void PostInit() override;
	virtual void Shutdown() override;

	template<typename TRHIType>
	static FORCEINLINE typename TD3D12ResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TD3D12ResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}

	virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) final override;
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) final override;
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) final override;
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) final override;
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) final override;
	virtual FPixelShaderRHIRef RHICreatePixelShader(const TArray<uint8>& Code) final override;
	virtual FVertexShaderRHIRef RHICreateVertexShader(const TArray<uint8>& Code) final override;
	virtual FHullShaderRHIRef RHICreateHullShader(const TArray<uint8>& Code) final override;
	virtual FDomainShaderRHIRef RHICreateDomainShader(const TArray<uint8>& Code) final override;
	virtual FGeometryShaderRHIRef RHICreateGeometryShader(const TArray<uint8>& Code) final override;
	virtual FGeometryShaderRHIRef RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) final override;
	virtual FComputeShaderRHIRef RHICreateComputeShader(const TArray<uint8>& Code) final override;
	virtual FComputeFenceRHIRef RHICreateComputeFence(const FName& Name) final override;
	virtual FBoundShaderStateRHIRef RHICreateBoundShaderState(FVertexDeclarationRHIParamRef VertexDeclaration, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader) final override;
	virtual FUniformBufferRHIRef RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage) final override;
	virtual FIndexBufferRHIRef RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void* RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode) final override;
	virtual void RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer) final override;
	virtual FVertexBufferRHIRef RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void* RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode) final override;
	virtual void RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer) final override;
	virtual void RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBuffer, FVertexBufferRHIParamRef DestBuffer) final override;
	virtual FStructuredBufferRHIRef RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void* RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode) final override;
	virtual void RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FTextureRHIParamRef Texture, uint32 MipLevel) final override;
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBuffer, uint8 Format) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBuffer) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FIndexBufferRHIParamRef Buffer) final override;
	virtual uint64 RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign) final override;
	virtual uint64 RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign) final override;
	virtual uint64 RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign) final override;
	virtual void RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats) final override;
	virtual bool RHIGetTextureMemoryVisualizeData(FColor* TextureData, int32 SizeX, int32 SizeY, int32 Pitch, int32 PixelSize) final override;
	virtual FTextureReferenceRHIRef RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime) final override;
	virtual FTexture2DRHIRef RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual FTexture2DRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips) final override;
	virtual void RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D) final override;
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual FTexture3DRHIRef RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel) final override;
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel) final override;
	virtual void RHIGenerateMips(FTextureRHIParamRef Texture) final override;
	virtual uint32 RHIComputeMemorySize(FTextureRHIParamRef TextureRHI) final override;
	virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) final override;
	virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted) final override;
	virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted) final override;
	virtual void* RHILockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void* RHILockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIUpdateTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData) final override;
	virtual void RHIUpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData) final override;
	virtual FTextureCubeRHIRef RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual FTextureCubeRHIRef RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) final override;
	virtual void* RHILockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) final override;
	virtual void RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) final override;
	virtual void RHIBindDebugLabelName(FTextureRHIParamRef Texture, const TCHAR* Name) final override;
	virtual void RHIReadSurfaceData(FTextureRHIParamRef Texture, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags) final override;
	virtual void RHIMapStagingSurface(FTextureRHIParamRef Texture, void*& OutData, int32& OutWidth, int32& OutHeight) final override;
	virtual void RHIUnmapStagingSurface(FTextureRHIParamRef Texture) final override;
	virtual void RHIReadSurfaceFloatData(FTextureRHIParamRef Texture, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace, int32 ArrayIndex, int32 MipIndex) final override;
	virtual void RHIRead3DSurfaceFloatData(FTextureRHIParamRef Texture, FIntRect Rect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData) final override;
	virtual FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType) final override;
	virtual bool RHIGetRenderQueryResult(FRenderQueryRHIParamRef RenderQuery, uint64& OutResult, bool bWait) final override;
	virtual FTexture2DRHIRef RHIGetViewportBackBuffer(FViewportRHIParamRef Viewport) final override;
	virtual void RHIAdvanceFrameForGetViewportBackBuffer() final override;
	virtual void RHIAcquireThreadOwnership() final override;
	virtual void RHIReleaseThreadOwnership() final override;
	virtual void RHIFlushResources() final override;
	virtual uint32 RHIGetGPUFrameCycles() final override;
	virtual FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) final override;
	virtual void RHIResizeViewport(FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen) final override;
	virtual void RHITick(float DeltaTime) final override;
	virtual void RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets) final override;
	virtual void RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask) final override;
	virtual void RHIBlockUntilGPUIdle() final override;
	virtual bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate) final override;
	virtual void RHIGetSupportedResolution(uint32& Width, uint32& Height) final override;
	virtual void RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef Texture, uint32 FirstMip) final override;
	virtual void RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef Texture, uint32 FirstMip) final override;
	virtual void RHIExecuteCommandList(FRHICommandList* CmdList) final override;
	virtual void* RHIGetNativeDevice() final override;
	virtual class IRHICommandContext* RHIGetDefaultContext() final override;
	virtual class IRHIComputeContext* RHIGetDefaultAsyncComputeContext() final override;
	virtual class IRHICommandContextContainer* RHIGetCommandContextContainer() final override;

	// FD3D12DynamicRHI interface.
	virtual ID3D12CommandQueue* RHIGetD3DCommandQueue();
	virtual FTexture2DRHIRef RHICreateTexture2DFromD3D12Resource(uint8 Format, uint32 Flags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource);
	virtual void RHIAliasTexture2DResources(FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D);

	//
	// The Following functions are the _RenderThread version of the above functions. They allow the RHI to control the thread synchronization for greater efficiency.
	// These will be un-commented as they are implemented.
	//

	virtual FVertexBufferRHIRef CreateVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual void* LockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode);
	virtual void UnlockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer);
	virtual FVertexBufferRHIRef CreateAndLockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer);
	virtual FIndexBufferRHIRef CreateAndLockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer);
	// virtual FTexture2DRHIRef AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus);
	// virtual ETextureReallocationStatus FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted);
	// virtual ETextureReallocationStatus CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted);
	virtual FIndexBufferRHIRef CreateIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual void* LockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode);
	virtual void UnlockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBuffer);
	// virtual FVertexDeclarationRHIRef CreateVertexDeclaration_RenderThread(class FRHICommandListImmediate& RHICmdList, const FVertexDeclarationElementList& Elements);
	// virtual void UpdateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData);
	// virtual void* LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true);
	// virtual void UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true);
	// 
	// virtual FTexture2DRHIRef RHICreateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	// virtual FTexture2DArrayRHIRef RHICreateTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	// virtual FTexture3DRHIRef RHICreateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer);
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTextureRHIParamRef Texture, uint32 MipLevel);
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint8 Format);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer);
	// virtual FTextureCubeRHIRef RHICreateTextureCube_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	// virtual FTextureCubeRHIRef RHICreateTextureCubeArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	// virtual FRenderQueryRHIRef RHICreateRenderQuery_RenderThread(class FRHICommandListImmediate& RHICmdList, ERenderQueryType QueryType);

	FD3D12ResourceLocation* CreateBuffer(FRHICommandListImmediate* RHICmdList, const D3D12_RESOURCE_DESC Desc, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, uint32 Alignment);

	template<class BufferType>
	void* LockBuffer(FRHICommandListImmediate* RHICmdList, BufferType* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode);

	template<class BufferType>
	void UnlockBuffer(FRHICommandListImmediate* RHICmdList, BufferType* Buffer);

	inline bool ShouldDeferBufferLockOperation(FRHICommandList* RHICmdList)
	{
		if (RHICmdList == nullptr)
		{
			return false;
		}

		if (RHICmdList->Bypass() || !GRHIThread)
		{
			return false;
		}

		return true;
	}

	void UpdateBuffer(FD3D12Resource* Dest, uint32 DestOffset, FD3D12Resource* Source, uint32 SourceOffset, uint32 NumBytes);

#if UE_BUILD_DEBUG	
	uint32 SubmissionLockStalls;
	uint32 DrawCount;
	uint64 PresentCount;
#endif

	inline IDXGIFactory4* GetFactory() const
	{
		return DXGIFactory;
	}

	inline void InitD3DDevices()
	{
		MainDevice->InitD3DDevice();
		PerRHISetup(MainDevice);
	}

	inline void UpdataTextureMemorySize(int64 TextureSizeInKiloBytes) { FPlatformAtomics::InterlockedAdd(&GCurrentTextureMemorySize, TextureSizeInKiloBytes); }

	/** Determine if an two views intersect */
	template <class LeftT, class RightT>
	static inline bool ResourceViewsIntersect(FD3D12View<LeftT>* pLeftView, FD3D12View<RightT>* pRightView)
	{
		if (pLeftView == nullptr || pRightView == nullptr)
		{
			// Cannot intersect if at least one is null
			return false;
		}

		if ((void*)pLeftView == (void*)pRightView)
		{
			// Cannot intersect with itself
			return false;
		}

		FD3D12Resource* pRTVResource = pLeftView->GetResource();
		FD3D12Resource* pSRVResource = pRightView->GetResource();
		if (pRTVResource != pSRVResource)
		{
			// Not the same resource
			return false;
		}

		// Same resource, so see if their subresources overlap
		return !pLeftView->DoesNotOverlap(*pRightView);
	}

	static inline bool IsTransitionNeeded(uint32 before, uint32 after)
	{
		check(before != D3D12_RESOURCE_STATE_CORRUPT && after != D3D12_RESOURCE_STATE_CORRUPT);
		check(before != D3D12_RESOURCE_STATE_TBD && after != D3D12_RESOURCE_STATE_TBD);

		// Depth write is actually a suitable for read operations as a "normal" depth buffer.
		if ((before == D3D12_RESOURCE_STATE_DEPTH_WRITE) && (after == D3D12_RESOURCE_STATE_DEPTH_READ))
		{
			return false;
		}

		// If 'after' is a subset of 'before', then there's no need for a transition
		// Note: COMMON is an oddball state that doesn't follow the RESOURE_STATE pattern of 
		// having exactly one bit set so we need to special case these
		return before != after &&
			((before | after) != before ||
				after == D3D12_RESOURCE_STATE_COMMON);
	}

	/** Transition a resource's state based on a Render target view */
	static inline void TransitionResource(FD3D12CommandListHandle& hCommandList, FD3D12RenderTargetView* pView, D3D12_RESOURCE_STATES after)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		FD3D12Resource* pResource = pView->GetResource();

		const D3D12_RENDER_TARGET_VIEW_DESC &desc = pView->GetDesc();
		switch (desc.ViewDimension)
		{
		case D3D12_RTV_DIMENSION_TEXTURE3D:
			// Note: For volume (3D) textures, all slices for a given mipmap level are a single subresource index.
			// Fall-through
		case D3D12_RTV_DIMENSION_TEXTURE2D:
		case D3D12_RTV_DIMENSION_TEXTURE2DMS:
			// Only one subresource to transition
			TransitionResource(hCommandList, pResource, after, desc.Texture2D.MipSlice);
			break;

		case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
		{
			// Multiple subresources to transition
			TransitionResource(hCommandList, pResource, after, pView->GetViewSubresourceSubset());
			break;
		}

		default:
			check(false);	// Need to update this code to include the view type
			break;
		}
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	/** Transition a resource's state based on a Depth stencil view's desc flags */
	static inline void TransitionResource(FD3D12CommandListHandle& hCommandList, FD3D12DepthStencilView* pView)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		// Determine the required subresource states from the view desc
		const D3D12_DEPTH_STENCIL_VIEW_DESC& DSVDesc = pView->GetDesc();
		const bool bDSVDepthIsWritable = (DSVDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH) == 0;
		const bool bDSVStencilIsWritable = (DSVDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL) == 0;
		// TODO: Check if the PSO depth stencil is writable. When this is done, we need to transition in SetDepthStencilState too.

		// This code assumes that the DSV always contains the depth plane
		check(pView->HasDepth());
		const bool bHasDepth = true;
		const bool bHasStencil = pView->HasStencil();
		const bool bDepthIsWritable = bHasDepth && bDSVDepthIsWritable;
		const bool bStencilIsWritable = bHasStencil && bDSVStencilIsWritable;

		// DEPTH_WRITE is suitable for read operations when used as a normal depth/stencil buffer.
		FD3D12Resource* pResource = pView->GetResource();
		if (bDepthIsWritable)
		{
			TransitionResource(hCommandList, pResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, pView->GetDepthOnlyViewSubresourceSubset());
		}

		if (bStencilIsWritable)
		{
			TransitionResource(hCommandList, pResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, pView->GetStencilOnlyViewSubresourceSubset());
		}
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	/** Transition a resource's state based on a Depth stencil view */
	static inline void TransitionResource(FD3D12CommandListHandle& hCommandList, FD3D12DepthStencilView* pView, D3D12_RESOURCE_STATES after)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		FD3D12Resource* pResource = pView->GetResource();

		const D3D12_DEPTH_STENCIL_VIEW_DESC &desc = pView->GetDesc();
		switch (desc.ViewDimension)
		{
		case D3D12_DSV_DIMENSION_TEXTURE2D:
		case D3D12_DSV_DIMENSION_TEXTURE2DMS:
			if (pResource->GetPlaneCount() > 1)
			{
				// Multiple subresources to transtion
				TransitionResource(hCommandList, pResource, after, pView->GetViewSubresourceSubset());
				break;
			}
			else
			{
				// Only one subresource to transition
				check(pResource->GetPlaneCount() == 1);
				TransitionResource(hCommandList, pResource, after, desc.Texture2D.MipSlice);
			}
			break;

		case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
		case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
		{
			// Multiple subresources to transtion
			TransitionResource(hCommandList, pResource, after, pView->GetViewSubresourceSubset());
			break;
		}

		default:
			check(false);	// Need to update this code to include the view type
			break;
		}
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	/** Transition a resource's state based on a Unordered access view */
	static inline void TransitionResource(FD3D12CommandListHandle& hCommandList, FD3D12UnorderedAccessView* pView, D3D12_RESOURCE_STATES after)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		FD3D12Resource* pResource = pView->GetResource();

		const D3D12_UNORDERED_ACCESS_VIEW_DESC &desc = pView->GetDesc();
		switch (desc.ViewDimension)
		{
		case D3D12_UAV_DIMENSION_BUFFER:
			TransitionResource(hCommandList, pResource, after, 0);
			break;

		case D3D12_UAV_DIMENSION_TEXTURE2D:
			// Only one subresource to transition
			TransitionResource(hCommandList, pResource, after, desc.Texture2D.MipSlice);
			break;

		case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
		{
			// Multiple subresources to transtion
			TransitionResource(hCommandList, pResource, after, pView->GetViewSubresourceSubset());
			break;
		}
		case D3D12_UAV_DIMENSION_TEXTURE3D:
		{
			// Multiple subresources to transtion
			TransitionResource(hCommandList, pResource, after, pView->GetViewSubresourceSubset());
			break;
		}

		default:
			check(false);	// Need to update this code to include the view type
			break;
		}
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	/** Transition a resource's state based on a Shader resource view */
	static inline void TransitionResource(FD3D12CommandListHandle& hCommandList, FD3D12ShaderResourceView* pView, D3D12_RESOURCE_STATES after)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		FD3D12Resource* pResource = pView->GetResource();

		if (!pResource || !pResource->RequiresResourceStateTracking())
		{
			// Early out if we never need to do state tracking, the resource should always be in an SRV state.
			return;
		}

		const D3D12_RESOURCE_DESC &ResDesc = pResource->GetDesc();
		const CViewSubresourceSubset &subresourceSubset = pView->GetViewSubresourceSubset();

		const D3D12_SHADER_RESOURCE_VIEW_DESC &desc = pView->GetDesc();
		switch (desc.ViewDimension)
		{
		default:
		{
			// Transition the resource
			TransitionResource(hCommandList, pResource, after, subresourceSubset);
			break;
		}

		case D3D12_SRV_DIMENSION_BUFFER:
		{
			if (pResource->GetHeapType() == D3D12_HEAP_TYPE_DEFAULT)
			{
				// Transition the resource
				TransitionResource(hCommandList, pResource, after, subresourceSubset);
			}
			break;
		}
		}
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	// Transition a specific subresource to the after state.
	static inline void TransitionResource(FD3D12CommandListHandle& hCommandList, FD3D12Resource* pResource, D3D12_RESOURCE_STATES after, uint32 subresource)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		TransitionResourceWithTracking(hCommandList, pResource, after, subresource);
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	// Transition a subset of subresources to the after state.
	static inline void TransitionResource(FD3D12CommandListHandle& hCommandList, FD3D12Resource* pResource, D3D12_RESOURCE_STATES after, const CViewSubresourceSubset& subresourceSubset)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		TransitionResourceWithTracking(hCommandList, pResource, after, subresourceSubset);
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	// Transition a subresource from current to a new state, using resource state tracking.
	static void TransitionResourceWithTracking(FD3D12CommandListHandle& hCommandList, FD3D12Resource* pResource, D3D12_RESOURCE_STATES after, uint32 subresource)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		check(pResource->RequiresResourceStateTracking());

		hCommandList.UpdateResidency(pResource);

		CResourceState& ResourceState = hCommandList.GetResourceState(pResource);
		if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !ResourceState.AreAllSubresourcesSame())
		{
			// Slow path. Want to transition the entire resource (with multiple subresources). But they aren't in the same state.

			const uint8 SubresourceCount = pResource->GetSubresourceCount();
			for (uint32 SubresourceIndex = 0; SubresourceIndex < SubresourceCount; SubresourceIndex++)
			{
				const D3D12_RESOURCE_STATES before = ResourceState.GetSubresourceState(SubresourceIndex);
				if (before == D3D12_RESOURCE_STATE_TBD)
				{
					// We need a pending resource barrier so we can setup the state before this command list executes
					hCommandList.AddPendingResourceBarrier(pResource, after, SubresourceIndex);
				}
				// We're not using IsTransitionNeeded() because we do want to transition even if 'after' is a subset of 'before'
				// This is so that we can ensure all subresources are in the same state, simplifying future barriers
				else if (before != after)
				{
					hCommandList.AddTransitionBarrier(pResource, before, after, SubresourceIndex);
				}
			}

			// The entire resource should now be in the after state on this command list (even if all barriers are pending)
			check(ResourceState.CheckResourceState(after));
			ResourceState.SetResourceState(after);
		}
		else
		{
			const D3D12_RESOURCE_STATES before = ResourceState.GetSubresourceState(subresource);
			if (before == D3D12_RESOURCE_STATE_TBD)
			{
				// We need a pending resource barrier so we can setup the state before this command list executes
				hCommandList.AddPendingResourceBarrier(pResource, after, subresource);
				ResourceState.SetSubresourceState(subresource, after);
			}
			else if (IsTransitionNeeded(before, after))
			{
				hCommandList.AddTransitionBarrier(pResource, before, after, subresource);
				ResourceState.SetSubresourceState(subresource, after);
			}
		}
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	// Transition subresources from current to a new state, using resource state tracking.
	static void TransitionResourceWithTracking(FD3D12CommandListHandle& hCommandList, FD3D12Resource* pResource, D3D12_RESOURCE_STATES after, const CViewSubresourceSubset& subresourceSubset)
	{
#if USE_D3D12RHI_RESOURCE_STATE_TRACKING
		check(pResource->RequiresResourceStateTracking());

		hCommandList.UpdateResidency(pResource);
		D3D12_RESOURCE_STATES before;
		const bool bIsWholeResource = subresourceSubset.IsWholeResource();
		CResourceState& ResourceState = hCommandList.GetResourceState(pResource);
		if (bIsWholeResource && ResourceState.AreAllSubresourcesSame())
		{
			// Fast path. Transition the entire resource from one state to another.
			before = ResourceState.GetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			if (before == D3D12_RESOURCE_STATE_TBD)
			{
				// We need a pending resource barrier so we can setup the state before this command list executes
				hCommandList.AddPendingResourceBarrier(pResource, after, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				ResourceState.SetResourceState(after);
			}
			else if (IsTransitionNeeded(before, after))
			{
				hCommandList.AddTransitionBarrier(pResource, before, after, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
				ResourceState.SetResourceState(after);
			}
		}
		else
		{
			// Slower path. Either the subresources are in more than 1 state, or the view only partially covers the resource.
			// Either way, we'll need to loop over each subresource in the view...

			bool bWholeResourceWasTransitionedToSameState = bIsWholeResource;
			for (CViewSubresourceSubset::CViewSubresourceIterator it = subresourceSubset.begin(); it != subresourceSubset.end(); ++it)
			{
				for (uint32 SubresourceIndex = it.StartSubresource(); SubresourceIndex < it.EndSubresource(); SubresourceIndex++)
				{
					before = ResourceState.GetSubresourceState(SubresourceIndex);
					if (before == D3D12_RESOURCE_STATE_TBD)
					{
						// We need a pending resource barrier so we can setup the state before this command list executes
						hCommandList.AddPendingResourceBarrier(pResource, after, SubresourceIndex);
						ResourceState.SetSubresourceState(SubresourceIndex, after);
					}
					else if (IsTransitionNeeded(before, after))
					{
						hCommandList.AddTransitionBarrier(pResource, before, after, SubresourceIndex);
						ResourceState.SetSubresourceState(SubresourceIndex, after);
					}
					else
					{
						// Didn't need to transition the subresource.
						if (before != after)
						{
							bWholeResourceWasTransitionedToSameState = false;
						}
					}
				}
			}

			// If we just transtioned every subresource to the same state, lets update it's tracking so it's on a per-resource level
			if (bWholeResourceWasTransitionedToSameState)
			{
				// Sanity check to make sure all subresources are really in the 'after' state
				check(ResourceState.CheckResourceState(after));

				ResourceState.SetResourceState(after);
			}
		}
#endif // USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	void GetLocalVideoMemoryInfo(DXGI_QUERY_VIDEO_MEMORY_INFO* LocalVideoMemoryInfo);

public:

	inline FD3D12ThreadSafeFastAllocator& GetHelperThreadDynamicUploadHeapAllocator()
	{
		check(!IsInActualRenderingThread());

		static const uint32 AsyncTexturePoolSize = 1024 * 512;

		if (HelperThreadDynamicHeapAllocator == nullptr)
		{
			if (SharedFastAllocPool == nullptr)
			{
				SharedFastAllocPool = new FD3D12ThreadSafeFastAllocatorPagePool(GetRHIDevice(), D3D12_HEAP_TYPE_UPLOAD, AsyncTexturePoolSize);
			}

			uint32 NextIndex = InterlockedIncrement(&NumThreadDynamicHeapAllocators) - 1;
			check(NextIndex < _countof(ThreadDynamicHeapAllocatorArray));
			HelperThreadDynamicHeapAllocator = new FD3D12ThreadSafeFastAllocator(GetRHIDevice(), SharedFastAllocPool);

			ThreadDynamicHeapAllocatorArray[NextIndex] = HelperThreadDynamicHeapAllocator;
		}

		return *HelperThreadDynamicHeapAllocator;
	}

	// The texture streaming threads can all share a pool of buffers to save on memory
	// (it doesn't really matter if they take a lock as they are async and low priority)
	FD3D12ThreadSafeFastAllocatorPagePool* SharedFastAllocPool = nullptr;
	FD3D12ThreadSafeFastAllocator* ThreadDynamicHeapAllocatorArray[16];
	uint32 NumThreadDynamicHeapAllocators;
	static __declspec(thread) FD3D12ThreadSafeFastAllocator* HelperThreadDynamicHeapAllocator;

	static DXGI_FORMAT GetPlatformTextureResourceFormat(DXGI_FORMAT InFormat, uint32 InFlags);

#if	PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	virtual void* CreateVirtualTexture(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, bool bCubeTexture, uint32 Flags, void* D3DTextureDesc, void* D3DTextureResource) = 0;
	virtual void DestroyVirtualTexture(uint32 Flags, void* RawTextureMemory) = 0;
	virtual bool HandleSpecialLock(FD3D12LockedData& LockedData, uint32 MipIndex, uint32 ArrayIndex, uint32 Flags, EResourceLockMode LockMode,
		void* D3DTextureResource, void* RawTextureMemory, uint32 NumMips, uint32& DestStride) = 0;
	virtual bool HandleSpecialUnlock(uint32 MipIndex, uint32 Flags, void* D3DTextureResource, void* RawTextureMemory) = 0;
#endif

	inline void RegisterGPUWork(uint32 NumPrimitives = 0, uint32 NumVertices = 0)
	{
		GPUProfilingData.RegisterGPUWork(NumPrimitives, NumVertices);
	}

	inline void PushGPUEvent(const TCHAR* Name, FColor Color)
	{
		GPUProfilingData.PushEvent(Name, Color);
	}

	inline void PopGPUEvent()
	{
		GPUProfilingData.PopEvent();
	}

	inline void IncrementSetShaderTextureCalls(uint32 value = 1)
	{
		SetShaderTextureCalls += value;
	}

	inline void IncrementSetShaderTextureCycles(uint32 value)
	{
		SetShaderTextureCycles += value;
	}

	inline void IncrementCacheResourceTableCalls(uint32 value = 1)
	{
		CacheResourceTableCalls += value;
	}

	inline void IncrementCacheResourceTableCycles(uint32 value)
	{
		CacheResourceTableCycles += value;
	}

	inline void IncrementCommitComputeResourceTables(uint32 value = 1)
	{
		CommitResourceTableCycles += value;
	}

	inline void AddBoundShaderState(FD3D12BoundShaderState* state)
	{
		BoundShaderStateHistory.Add(state);
	}

	inline void IncrementSetTextureInTableCalls(uint32 value = 1)
	{
		SetTextureInTableCalls += value;
	}

	inline uint32 GetResourceTableFrameCounter() { return ResourceTableFrameCounter; }

	/** Consumes about 100ms of GPU time (depending on resolution and GPU), useful for making sure we're not CPU bound when GPU profiling. */
	void IssueLongGPUTask();

	void ResetViewportFrameCounter() { ViewportFrameCounter = 0; }

private:
	inline void PerRHISetup(FD3D12Device* MainDevice);

	/** The global D3D interface. */
	TRefCountPtr<IDXGIFactory4> DXGIFactory;

	FD3D12Device* MainDevice;
	FD3D12Adapter MainAdapter;

	/** The feature level of the device. */
	D3D_FEATURE_LEVEL FeatureLevel;

	/** A buffer in system memory containing all zeroes of the specified size. */
	void* ZeroBuffer;
	uint32 ZeroBufferSize;

	uint32 CommitResourceTableCycles;
	uint32 CacheResourceTableCalls;
	uint32 CacheResourceTableCycles;
	uint32 SetShaderTextureCycles;
	uint32 SetShaderTextureCalls;
	uint32 SetTextureInTableCalls;

	uint32 ViewportFrameCounter;

	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;

	/**
	* Internal counter used for resource table caching.
	* INDEX_NONE means caching is not allowed.
	*/
	uint32 ResourceTableFrameCounter;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<10000> > BoundShaderStateHistory;

	FD3DGPUProfiler GPUProfilingData;

	template<typename BaseResourceType>
	TD3D12Texture2D<BaseResourceType>* CreateD3D12Texture2D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bTextureArray, bool CubeTexture, uint8 Format,
		uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);

	FD3D12Texture3D* CreateD3D12Texture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);

	/**
	 * Gets the best supported MSAA settings from the provided MSAA count to check against.
	 *
	 * @param PlatformFormat		The format of the texture being created
	 * @param MSAACount				The MSAA count to check against.
	 * @param OutBestMSAACount		The best MSAA count that is suppored.  Could be smaller than MSAACount if it is not supported
	 * @param OutMSAAQualityLevels	The number MSAA quality levels for the best msaa count supported
	 */
	void GetBestSupportedMSAASetting(DXGI_FORMAT PlatformFormat, uint32 MSAACount, uint32& OutBestMSAACount, uint32& OutMSAAQualityLevels);


	// @return 0xffffffff if not not supported
	uint32 GetMaxMSAAQuality(uint32 SampleCount);

	/**
	* Returns a pointer to a texture resource that can be used for CPU reads.
	* Note: the returned resource could be the original texture or a new temporary texture.
	* @param TextureRHI - Source texture to create a staging texture from.
	* @param InRect - rectangle to 'stage'.
	* @param StagingRectOUT - parameter is filled with the rectangle to read from the returned texture.
	* @return The CPU readable Texture object.
	*/
	TRefCountPtr<FD3D12Resource> GetStagingTexture(FTextureRHIParamRef TextureRHI, FIntRect InRect, FIntRect& OutRect, FReadSurfaceDataFlags InFlags, D3D12_PLACED_SUBRESOURCE_FOOTPRINT &readBackHeapDesc);

	void ReadSurfaceDataNoMSAARaw(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	void ReadSurfaceDataMSAARaw(FRHICommandList_RecursiveHazardous& RHICmdList, FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	void SetupRecursiveResources();

	// This should only be called by Dynamic RHI member functions
	inline FD3D12Device* GetRHIDevice() const
	{
		return MainDevice;
	}
};

/** Implements the D3D12RHI module as a dynamic RHI providing module. */
class FD3D12DynamicRHIModule : public IDynamicRHIModule
{
public:
	// IModuleInterface
	virtual bool SupportsDynamicReloading() override { return false; }

	// IDynamicRHIModule
	virtual bool IsSupported() override;
	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) override;

private:
	FD3D12Adapter ChosenAdapter;

	// set MaxSupportedFeatureLevel and ChosenAdapter
	void FindAdapter();
};

/** Vertex declaration for just one FVector4 position. */
class FVector4VertexDeclaration : public FRenderResource
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

namespace D3D12RHI
{
	extern TGlobalResource<FVector4VertexDeclaration> GD3D12Vector4VertexDeclaration;

	/**
	 *	Default 'Fast VRAM' allocator...
	 */
	class FFastVRAMAllocator
	{
	public:
		FFastVRAMAllocator() {}

		virtual ~FFastVRAMAllocator() {}

		/**
		 *	IMPORTANT: This function CAN modify the TextureDesc!
		 */
		virtual FVRamAllocation AllocTexture2D(D3D12_RESOURCE_DESC& TextureDesc)
		{
			return FVRamAllocation();
		}

		/**
		 *	IMPORTANT: This function CAN modify the TextureDesc!
		 */
		virtual FVRamAllocation AllocTexture3D(D3D12_RESOURCE_DESC& TextureDesc)
		{
			return FVRamAllocation();
		}

		/**
		 *	IMPORTANT: This function CAN modify the BufferDesc!
		 */
		virtual FVRamAllocation AllocUAVBuffer(D3D12_RESOURCE_DESC& BufferDesc)
		{
			return FVRamAllocation();
		}

		template< typename t_A, typename t_B >
		static t_A RoundUpToNextMultiple(const t_A& a, const t_B& b)
		{
			return ((a - 1) / b + 1) * b;
		}

		static FFastVRAMAllocator* GetFastVRAMAllocator();
	};
}

/**
*	Class of a scoped resource barrier.
*	This class avoids resource state tracking because resources will be
*	returned to their original state when the object leaves scope.
*/
class FScopeResourceBarrier
{
private:
	FD3D12CommandListHandle& hCommandList;
	FD3D12Resource* pResource;
	D3D12_RESOURCE_STATES Current;
	D3D12_RESOURCE_STATES Desired;
	uint32 Subresource;

public:
	explicit FScopeResourceBarrier(FD3D12CommandListHandle &hInCommandList, FD3D12Resource* pInResource, D3D12_RESOURCE_STATES InCurrent, D3D12_RESOURCE_STATES InDesired, const uint32 InSubresource)
		: hCommandList(hInCommandList)
		, pResource(pInResource)
		, Current(InCurrent)
		, Desired(InDesired)
		, Subresource(InSubresource)
	{
		check(!pResource->RequiresResourceStateTracking());
		hCommandList.AddTransitionBarrier(pResource, Current, Desired, Subresource);

		hCommandList.FlushResourceBarriers();	// Must flush so the desired state is actually set.
	}

	~FScopeResourceBarrier()
	{
		hCommandList.AddTransitionBarrier(pResource, Desired, Current, Subresource);
	}
};

/**
*	Class of a scoped resource barrier.
*	This class conditionally uses resource state tracking.
*	This should only be used with the Editor.
*/
class FConditionalScopeResourceBarrier
{
private:
	FD3D12CommandListHandle& hCommandList;
	FD3D12Resource* pResource;
	D3D12_RESOURCE_STATES Current;
	D3D12_RESOURCE_STATES Desired;
	uint32 Subresource;
	bool bUseTracking;

public:
	explicit FConditionalScopeResourceBarrier(FD3D12CommandListHandle &hInCommandList, FD3D12Resource* pInResource, D3D12_RESOURCE_STATES InDesired, const uint32 InSubresource)
		: hCommandList(hInCommandList)
		, pResource(pInResource)
		, Current(D3D12_RESOURCE_STATE_TBD)
		, Desired(InDesired)
		, Subresource(InSubresource)
		, bUseTracking(pResource->RequiresResourceStateTracking())
	{
		if (!bUseTracking)
		{
			Current = pResource->GetDefaultResourceState();
			hCommandList.AddTransitionBarrier(pResource, Current, Desired, Subresource);
		}
		else
		{
			FD3D12DynamicRHI::TransitionResource(hCommandList, pResource, Desired, Subresource);
		}

		hCommandList.FlushResourceBarriers();	// Must flush so the desired state is actually set.
	}

	~FConditionalScopeResourceBarrier()
	{
		// Return the resource to it's default state if it doesn't use tracking
		if (!bUseTracking)
		{
			hCommandList.AddTransitionBarrier(pResource, Desired, Current, Subresource);
		}
	}
};
