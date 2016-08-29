// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12CommandContext.h: D3D12 Command Context Interfaces
=============================================================================*/

#pragma once

class FD3D12CommandContext : public IRHICommandContext, public FD3D12DeviceChild
{
public:
	FD3D12CommandContext(class FD3D12Device* InParent, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc, bool InIsDefaultContext, bool InIsAsyncComputeContext = false);
	virtual ~FD3D12CommandContext();

	FD3D12CommandListManager& GetCommandListManager();

	template<typename TRHIType>
	static FORCEINLINE typename TD3D12ResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TD3D12ResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}

	template <EShaderFrequency ShaderFrequency>
	void ClearShaderResourceViews(FD3D12ResourceLocation* Resource)
	{
		StateCache.ClearShaderResourceViews<ShaderFrequency>(Resource);
	}

	void CheckIfSRVIsResolved(FD3D12ShaderResourceView* SRV);

	template <EShaderFrequency ShaderFrequency>
	void InternalSetShaderResourceView(FD3D12ResourceLocation* Resource, FD3D12ShaderResourceView* SRV, int32 ResourceIndex, FD3D12StateCache::ESRV_Type SrvType = FD3D12StateCache::SRV_Unknown)
	{
		// Check either both are set, or both are null.
		check((Resource && SRV) || (!Resource && !SRV));
		CheckIfSRVIsResolved(SRV);

		// Set the SRV we have been given (or null).
		StateCache.SetShaderResourceView<ShaderFrequency>(SRV, ResourceIndex, SrvType);
	}

	void SetCurrentComputeShader(FComputeShaderRHIParamRef ComputeShader)
	{
		CurrentComputeShader = ComputeShader;
	}

	const FComputeShaderRHIRef& GetCurrentComputeShader() const
	{
		return CurrentComputeShader;
	}

	template <EShaderFrequency ShaderFrequency>
	void SetShaderResourceView(FD3D12ResourceLocation* Resource, FD3D12ShaderResourceView* SRV, int32 ResourceIndex, FD3D12StateCache::ESRV_Type SrvType = FD3D12StateCache::SRV_Unknown)
	{
		InternalSetShaderResourceView<ShaderFrequency>(Resource, SRV, ResourceIndex, SrvType);
	}

	void EndFrame()
	{
		StateCache.GetDescriptorCache()->EndFrame();

		// Return the current command allocator to the pool so it can be reused for a future frame
		// Note: the default context releases it's command allocator before Present.
		if (!IsDefaultContext())
		{
			ReleaseCommandAllocator();
		}
	}

	void ConditionalObtainCommandAllocator();
	void ReleaseCommandAllocator();

	// Cycle to a new command list, but don't execute the current one yet.
	void OpenCommandList(bool bRestoreState = false);
	void CloseCommandList();
	void ExecuteCommandList(bool WaitForCompletion = false);

	// Close the D3D command list and execute it.  Optionally wait for the GPU to finish. Returns the handle to the command list so you can wait for it later.
	FD3D12CommandListHandle FlushCommands(bool WaitForCompletion = false);

	void Finish(TArray<FD3D12CommandListHandle>& CommandLists);

	void ClearState();
	void ConditionalClearShaderResource(FD3D12ResourceLocation* Resource);
	void ClearAllShaderResources();

	FD3D12FastAllocatorPagePool FastAllocatorPagePool;
	FD3D12FastAllocator FastAllocator;

	FD3D12FastAllocatorPagePool ConstantsAllocatorPagePool;
	FD3D12FastAllocator ConstantsAllocator;

	// Handles to the command list and direct command allocator this context owns (granted by the command list manager/command allocator manager), and a direct pointer to the D3D command list/command allocator.
	FD3D12CommandListHandle CommandListHandle;
	FD3D12CommandAllocator* CommandAllocator;
	FD3D12CommandAllocatorManager CommandAllocatorManager;

	FD3D12StateCache StateCache;

	FD3D12DynamicRHI& OwningRHI;

	// Tracks the currently set state blocks.
	TRefCountPtr<FD3D12RenderTargetView> CurrentRenderTargets[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	TRefCountPtr<FD3D12UnorderedAccessView> CurrentUAVs[D3D12_PS_CS_UAV_REGISTER_COUNT];
	TRefCountPtr<FD3D12DepthStencilView> CurrentDepthStencilTarget;
	TRefCountPtr<FD3D12TextureBase> CurrentDepthTexture;
	uint32 NumSimultaneousRenderTargets;
	uint32 NumUAVs;

	/** D3D12  defines a maximum of 14 constant buffers per shader stage. */
	enum { MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE = 14 };

	/** Track the currently bound uniform buffers. */
	FUniformBufferRHIRef BoundUniformBuffers[SF_NumFrequencies][MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE];

	/** Bit array to track which uniform buffers have changed since the last draw call. */
	uint16 DirtyUniformBuffers[SF_NumFrequencies];

	/** Tracks the current depth stencil access type. */
	FExclusiveDepthStencil CurrentDSVAccessType;

	/** When a new shader is set, we discard all old constants set for the previous shader. */
	bool bDiscardSharedConstants;

	/** Set to true when the current shading setup uses tessellation */
	bool bUsingTessellation;

	const bool bIsDefaultContext;
	const bool bIsAsyncComputeContext;

	uint32 numDraws;
	uint32 numDispatches;
	uint32 numClears;
	uint32 numBarriers;
	uint32 numCopies;
	uint32 otherWorkCounter;

	bool HasDoneWork()
	{
		return (numDraws + numDispatches + numClears + numBarriers + numCopies + otherWorkCounter) > 0;
	}

	/** Dynamic vertex and index buffers. */
	FD3D12DynamicBuffer DynamicVB;
	FD3D12DynamicBuffer DynamicIB;

	// State for begin/end draw primitive UP interface.
	uint32 PendingNumVertices;
	uint32 PendingVertexDataStride;
	uint32 PendingPrimitiveType;
	uint32 PendingNumPrimitives;
	uint32 PendingMinVertexIndex;
	uint32 PendingNumIndices;
	uint32 PendingIndexDataStride;

	/** A list of all D3D constant buffers RHIs that have been created. */
	TArray<TRefCountPtr<FD3D12ConstantBuffer> > VSConstantBuffers;
	TArray<TRefCountPtr<FD3D12ConstantBuffer> > HSConstantBuffers;
	TArray<TRefCountPtr<FD3D12ConstantBuffer> > DSConstantBuffers;
	TArray<TRefCountPtr<FD3D12ConstantBuffer> > PSConstantBuffers;
	TArray<TRefCountPtr<FD3D12ConstantBuffer> > GSConstantBuffers;
	TArray<TRefCountPtr<FD3D12ConstantBuffer> > CSConstantBuffers;

	FComputeShaderRHIRef CurrentComputeShader;

#if CHECK_SRV_TRANSITIONS
	/*
	* Rendertargets must be explicitly 'resolved' to manage their transition to an SRV on some platforms and DX12
	* We keep track of targets that need 'resolving' to provide safety asserts at SRV binding time.
	*/
	struct FUnresolvedRTInfo
	{
		FUnresolvedRTInfo(FName InResourceName, int32 InMipLevel, int32 InNumMips, int32 InArraySlice, int32 InArraySize)
			: ResourceName(InResourceName)
			, MipLevel(InMipLevel)
			, NumMips(InNumMips)
			, ArraySlice(InArraySlice)
			, ArraySize(InArraySize)
		{
		}

		bool operator==(const FUnresolvedRTInfo& Other) const
		{
			return MipLevel == Other.MipLevel &&
				NumMips == Other.NumMips &&
				ArraySlice == Other.ArraySlice &&
				ArraySize == Other.ArraySize;
		}

		FName ResourceName;
		int32 MipLevel;
		int32 NumMips;
		int32 ArraySlice;
		int32 ArraySize;
	};
	TMultiMap<ID3D12Resource*, FUnresolvedRTInfo> UnresolvedTargets;
#endif

	TRefCountPtr<FD3D12BoundShaderState> CurrentBoundShaderState;

	/** Initializes the constant buffers.  Called once at RHI initialization time. */
	void InitConstantBuffers();

	TArray<ID3D12DescriptorHeap*> DescriptorHeaps;
	void SetDescriptorHeaps(TArray<ID3D12DescriptorHeap*>& InHeaps)
	{
		DescriptorHeaps = InHeaps;

		// Need to set the descriptor heaps on the underlying command list because they can change mid command list
		if (CommandListHandle != nullptr)
		{
			CommandListHandle->SetDescriptorHeaps(DescriptorHeaps.Num(), DescriptorHeaps.GetData());
		}
	}

	/** needs to be called before each draw call */
	void CommitNonComputeShaderConstants();

	/** needs to be called before each dispatch call */
	void CommitComputeShaderConstants();

	template <class ShaderType> void SetResourcesFromTables(const ShaderType* RESTRICT);
	void CommitGraphicsResourceTables();
	void CommitComputeResourceTables(FD3D12ComputeShader* ComputeShader);

	void ValidateExclusiveDepthStencilAccess(FExclusiveDepthStencil Src) const;

	void CommitRenderTargetsAndUAVs();

	template<typename TPixelShader>
	void ResolveTextureUsingShader(
		FRHICommandList_RecursiveHazardous& RHICmdList,
		FD3D12Texture2D* SourceTexture,
		FD3D12Texture2D* DestTexture,
		FD3D12RenderTargetView* DestSurfaceRTV,
		FD3D12DepthStencilView* DestSurfaceDSV,
		const D3D12_RESOURCE_DESC& ResolveTargetDesc,
		const FResolveRect& SourceRect,
		const FResolveRect& DestRect,
		typename TPixelShader::FParameter PixelShaderParameter
		);

	// Some platforms might want to override this
	virtual void SetScissorRectIfRequiredWhenSettingViewport(uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		RHISetScissorRect(false, 0, 0, 0, 0);
	}

	inline bool IsDefaultContext() const
	{
		return bIsDefaultContext;
	}

	TRefCountPtr<FD3D12Fence> PendingFence;

	// IRHIComputeContext interface
	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) final override;
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) final override;
	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) final override;
	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFenceRHI) final override;
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) final override;
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) final override;
	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) final override;
	virtual void RHIPopEvent() final override;
	virtual void RHISubmitCommandsHint() final override;

	// IRHICommandContext interface
	virtual void RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) final override;
	virtual void RHIFlushComputeShaderCache() final override;
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) final override;
	virtual void RHIClearUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values) final override;
	virtual void RHICopyToResolveTarget(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, bool bKeepOriginalSurface, const FResolveParams& ResolveParams) final override;
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures) final override;
	virtual void RHIBeginRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	virtual void RHIEndRenderQuery(FRenderQueryRHIParamRef RenderQuery) final override;
	virtual void RHIBeginOcclusionQueryBatch() final override;
	virtual void RHIEndOcclusionQueryBatch() final override;
	virtual void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI) final override;
	virtual void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync) final override;
	virtual void RHIBeginFrame() final override;
	virtual void RHIEndFrame() final override;
	virtual void RHIBeginScene() final override;
	virtual void RHIEndScene() final override;
	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset) final override;
	virtual void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState) final override;
	virtual void RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ) final override;
	virtual void RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 MinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 MaxY, float MaxZ) final override;
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) final override;
	virtual void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState) final override;
	virtual void RHISetShaderTexture(FVertexShaderRHIParamRef VertexShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) final override;
	virtual void RHISetShaderSampler(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) final override;
	virtual void RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) final override;
	virtual void RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) final override;
	virtual void RHISetShaderParameter(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) final override;
	virtual void RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewState, uint32 StencilRef) final override;
	virtual void RHISetBlendState(FBlendStateRHIParamRef NewState, const FLinearColor& BlendFactor) final override;
	virtual void RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs) final override;
	virtual void RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo) final override;
	virtual void RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil) final override;
	virtual void RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) final override;
	virtual void RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) final override;
	virtual void RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData) final override;
	virtual void RHIEndDrawPrimitiveUP() final override;
	virtual void RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData) final override;
	virtual void RHIEndDrawIndexedPrimitiveUP() final override;
	virtual void RHIClear(bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect) final override;
	virtual void RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect) final override;
	virtual void RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth) final override;
	virtual void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture) final override;

	virtual void RHIClearMRTImpl(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect);
};