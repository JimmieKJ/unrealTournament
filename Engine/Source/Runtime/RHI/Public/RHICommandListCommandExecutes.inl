// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandListCommandExecutes.inl: RHI Command List execute functions.
=============================================================================*/

#if !defined(INTERNAL_DECORATOR)
	#define INTERNAL_DECORATOR(Method) CmdList.GetContext().Method
#endif

//for functions where the signatures do not match between gfx and compute commandlists
#if !defined(INTERNAL_DECORATOR_COMPUTE)
#define INTERNAL_DECORATOR_COMPUTE(Method) CmdList.GetComputeContext().Method
#endif

//for functions where the signatures match between gfx and compute commandlists
#if !defined(INTERNAL_DECORATOR_CONTEXT_PARAM1)
#define INTERNAL_DECORATOR_CONTEXT(Method) IRHIComputeContext& Context = (CmdListType == ECmdList::EGfx) ? CmdList.GetContext() : CmdList.GetComputeContext(); Context.Method
#endif



void FRHICommandSetRasterizerState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRasterizerState);
	INTERNAL_DECORATOR(RHISetRasterizerState)(State);
}

void FRHICommandSetDepthStencilState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetDepthStencilState);
	INTERNAL_DECORATOR(RHISetDepthStencilState)(State, StencilRef);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameter);
	INTERNAL_DECORATOR(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue); 
}
template struct FRHICommandSetShaderParameter<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameter);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderUniformBuffer<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUniformBuffer);
	INTERNAL_DECORATOR(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
}
template struct FRHICommandSetShaderUniformBuffer<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUniformBuffer);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderTexture<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderTexture);
	INTERNAL_DECORATOR(RHISetShaderTexture)(Shader, TextureIndex, Texture);
}
template struct FRHICommandSetShaderTexture<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderTexture);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderTexture)(Shader, TextureIndex, Texture);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderResourceViewParameter);
	INTERNAL_DECORATOR(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
}
template struct FRHICommandSetShaderResourceViewParameter<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderResourceViewParameter);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetUAVParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV);
}
template struct FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>;

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetUAVParameter_IntialCount<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
}
template struct FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::ECompute>;

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderSampler<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderSampler);
	INTERNAL_DECORATOR(RHISetShaderSampler)(Shader, SamplerIndex, Sampler);
}
template struct FRHICommandSetShaderSampler<FVertexShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FHullShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FDomainShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FGeometryShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FPixelShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template<> void FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderSampler);
	INTERNAL_DECORATOR_COMPUTE(RHISetShaderSampler)(Shader, SamplerIndex, Sampler);
}

void FRHICommandDrawPrimitive::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawPrimitive);
	INTERNAL_DECORATOR(RHIDrawPrimitive)(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
}

void FRHICommandDrawIndexedPrimitive::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedPrimitive);
	INTERNAL_DECORATOR(RHIDrawIndexedPrimitive)(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
}

void FRHICommandSetBoundShaderState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetBoundShaderState);
	INTERNAL_DECORATOR(RHISetBoundShaderState)(BoundShaderState);
}

void FRHICommandSetBlendState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetBlendState);
	INTERNAL_DECORATOR(RHISetBlendState)(State, BlendFactor);
}

void FRHICommandSetStreamSource::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStreamSource);
	INTERNAL_DECORATOR(RHISetStreamSource)(StreamIndex, VertexBuffer, Stride, Offset);
}

void FRHICommandSetViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetViewport);
	INTERNAL_DECORATOR(RHISetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FRHICommandSetStereoViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStereoViewport);
	INTERNAL_DECORATOR(RHISetStereoViewport)(LeftMinX, RightMinX, MinY, MinZ, LeftMaxX, RightMaxX, MaxY, MaxZ);
}

void FRHICommandSetScissorRect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetScissorRect);
	INTERNAL_DECORATOR(RHISetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
}

void FRHICommandSetRenderTargets::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRenderTargets);
	INTERNAL_DECORATOR(RHISetRenderTargets)(
		NewNumSimultaneousRenderTargets,
		NewRenderTargetsRHI,
		&NewDepthStencilTarget,
		NewNumUAVs,
		UAVs);
}

void FRHICommandSetRenderTargetsAndClear::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRenderTargetsAndClear);
	INTERNAL_DECORATOR(RHISetRenderTargetsAndClear)(RenderTargetsInfo);
}

void FRHICommandBindClearMRTValues::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BindClearMRTValues);
	INTERNAL_DECORATOR(RHIBindClearMRTValues)(
		bClearColor,
		bClearDepth,
		bClearStencil		
		);
}

void FRHICommandEndDrawPrimitiveUP::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawPrimitiveUP);
	void* Buffer = NULL;
	INTERNAL_DECORATOR(RHIBeginDrawPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, Buffer);
	FMemory::Memcpy(Buffer, OutVertexData, NumVertices * VertexDataStride);
	INTERNAL_DECORATOR(RHIEndDrawPrimitiveUP)();
}

void FRHICommandEndDrawIndexedPrimitiveUP::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawIndexedPrimitiveUP);
	void* VertexBuffer = nullptr;
	void* IndexBuffer = nullptr;
	INTERNAL_DECORATOR(RHIBeginDrawIndexedPrimitiveUP)(
		PrimitiveType,
		NumPrimitives,
		NumVertices,
		VertexDataStride,
		VertexBuffer,
		MinVertexIndex,
		NumIndices,
		IndexDataStride,
		IndexBuffer);
	FMemory::Memcpy(VertexBuffer, OutVertexData, NumVertices * VertexDataStride);
	FMemory::Memcpy(IndexBuffer, OutIndexData, NumIndices * IndexDataStride);
	INTERNAL_DECORATOR(RHIEndDrawIndexedPrimitiveUP)();
}

template<ECmdList CmdListType>
void FRHICommandSetComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetComputeShader);
	INTERNAL_DECORATOR_CONTEXT(RHISetComputeShader)(ComputeShader);
}
template struct FRHICommandSetComputeShader<ECmdList::EGfx>;
template struct FRHICommandSetComputeShader<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandDispatchComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchComputeShader);
	INTERNAL_DECORATOR_CONTEXT(RHIDispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
template struct FRHICommandDispatchComputeShader<ECmdList::EGfx>;
template struct FRHICommandDispatchComputeShader<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandDispatchIndirectComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchIndirectComputeShader);
	INTERNAL_DECORATOR_CONTEXT(RHIDispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
}
template struct FRHICommandDispatchIndirectComputeShader<ECmdList::EGfx>;
template struct FRHICommandDispatchIndirectComputeShader<ECmdList::ECompute>;

void FRHICommandAutomaticCacheFlushAfterComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(AutomaticCacheFlushAfterComputeShader);
	INTERNAL_DECORATOR(RHIAutomaticCacheFlushAfterComputeShader)(bEnable);
}

void FRHICommandFlushComputeShaderCache::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(FlushComputeShaderCache);
	INTERNAL_DECORATOR(RHIFlushComputeShaderCache)();
}

void FRHICommandDrawPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawPrimitiveIndirect);
	INTERNAL_DECORATOR(RHIDrawPrimitiveIndirect)(PrimitiveType, ArgumentBuffer, ArgumentOffset);
}

void FRHICommandDrawIndexedIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedIndirect);
	INTERNAL_DECORATOR(RHIDrawIndexedIndirect)(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
}

void FRHICommandDrawIndexedPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedPrimitiveIndirect);
	INTERNAL_DECORATOR(RHIDrawIndexedPrimitiveIndirect)(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
}

void FRHICommandEnableDepthBoundsTest::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EnableDepthBoundsTest);
	INTERNAL_DECORATOR(RHIEnableDepthBoundsTest)(bEnable, MinDepth, MaxDepth);
}

void FRHICommandClearUAV::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ClearUAV);
	INTERNAL_DECORATOR(RHIClearUAV)(UnorderedAccessViewRHI, Values);
}

void FRHICommandCopyToResolveTarget::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CopyToResolveTarget);
	INTERNAL_DECORATOR(RHICopyToResolveTarget)(SourceTexture, DestTexture, bKeepOriginalSurface, ResolveParams);
}

void FRHICommandTransitionTextures::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionTextures);
	INTERNAL_DECORATOR(RHITransitionResources)(TransitionType, &Textures[0], NumTextures);
}

void FRHICommandTransitionTexturesArray::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionTextures);
	INTERNAL_DECORATOR(RHITransitionResources)(TransitionType, &Textures[0], Textures.Num());
}

template<ECmdList CmdListType>
void FRHICommandTransitionUAVs<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionUAVs);
	INTERNAL_DECORATOR_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, UAVs, NumUAVs, WriteFence);
}
template struct FRHICommandTransitionUAVs<ECmdList::EGfx>;
template struct FRHICommandTransitionUAVs<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandSetAsyncComputeBudget<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetAsyncComputeBudget);
	INTERNAL_DECORATOR_CONTEXT(RHISetAsyncComputeBudget)(Budget);
}
template struct FRHICommandSetAsyncComputeBudget<ECmdList::EGfx>;
template struct FRHICommandSetAsyncComputeBudget<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandWaitComputeFence<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(WaitComputeFence);
	INTERNAL_DECORATOR_CONTEXT(RHIWaitComputeFence)(WaitFence);
}
template struct FRHICommandWaitComputeFence<ECmdList::EGfx>;
template struct FRHICommandWaitComputeFence<ECmdList::ECompute>;

void FRHICommandClear::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(Clear);
	INTERNAL_DECORATOR(RHIClear)(bClearColor, Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FRHICommandClearMRT::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ClearMRT);
	INTERNAL_DECORATOR(RHIClearMRT)(bClearColor, NumClearColors, ColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FRHICommandBuildLocalBoundShaderState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BuildLocalBoundShaderState);
	check(!IsValidRef(WorkArea.ComputedBSS->BSS)); // should not already have been created
	if (WorkArea.ComputedBSS->UseCount)
	{
		WorkArea.ComputedBSS->BSS = 	
			RHICreateBoundShaderState(WorkArea.Args.VertexDeclarationRHI, WorkArea.Args.VertexShaderRHI, WorkArea.Args.HullShaderRHI, WorkArea.Args.DomainShaderRHI, WorkArea.Args.PixelShaderRHI, WorkArea.Args.GeometryShaderRHI);
	}
}

void FRHICommandSetLocalBoundShaderState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetLocalBoundShaderState);
	check(LocalBoundShaderState.WorkArea->ComputedBSS->UseCount > 0 && IsValidRef(LocalBoundShaderState.WorkArea->ComputedBSS->BSS)); // this should have been created and should have uses outstanding

	INTERNAL_DECORATOR(RHISetBoundShaderState)(LocalBoundShaderState.WorkArea->ComputedBSS->BSS);

	if (--LocalBoundShaderState.WorkArea->ComputedBSS->UseCount == 0)
	{
		LocalBoundShaderState.WorkArea->ComputedBSS->~FComputedBSS();
	}
}

void FRHICommandBuildLocalUniformBuffer::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BuildLocalUniformBuffer);
	check(!IsValidRef(WorkArea.ComputedUniformBuffer->UniformBuffer)); // should not already have been created
	check(WorkArea.Layout);
	check(WorkArea.Contents); 
	if (WorkArea.ComputedUniformBuffer->UseCount)
	{
		WorkArea.ComputedUniformBuffer->UniformBuffer = RHICreateUniformBuffer(WorkArea.Contents, *WorkArea.Layout, UniformBuffer_SingleFrame); 
	}
	WorkArea.Layout = nullptr;
	WorkArea.Contents = nullptr;
}

template <typename TShaderRHIParamRef>
void FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetLocalUniformBuffer);
	check(LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount > 0 && IsValidRef(LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UniformBuffer)); // this should have been created and should have uses outstanding
	INTERNAL_DECORATOR(RHISetShaderUniformBuffer)(Shader, BaseIndex, LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UniformBuffer);
	if (--LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount == 0)
	{
		LocalUniformBuffer.WorkArea->ComputedUniformBuffer->~FComputedUniformBuffer();
	}
}
template struct FRHICommandSetLocalUniformBuffer<FVertexShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FHullShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FDomainShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FGeometryShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FPixelShaderRHIParamRef>;
template struct FRHICommandSetLocalUniformBuffer<FComputeShaderRHIParamRef>;

void FRHICommandBeginRenderQuery::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginRenderQuery);
	INTERNAL_DECORATOR(RHIBeginRenderQuery)(RenderQuery);
}

void FRHICommandEndRenderQuery::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndRenderQuery);
	INTERNAL_DECORATOR(RHIEndRenderQuery)(RenderQuery);
}

void FRHICommandBeginOcclusionQueryBatch::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginOcclusionQueryBatch);
	INTERNAL_DECORATOR(RHIBeginOcclusionQueryBatch)();
}

void FRHICommandEndOcclusionQueryBatch::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndOcclusionQueryBatch);
	INTERNAL_DECORATOR(RHIEndOcclusionQueryBatch)();
}

template<ECmdList CmdListType>
void FRHICommandSubmitCommandsHint<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SubmitCommandsHint);
	INTERNAL_DECORATOR_CONTEXT(RHISubmitCommandsHint)();
}
template struct FRHICommandSubmitCommandsHint<ECmdList::EGfx>;
template struct FRHICommandSubmitCommandsHint<ECmdList::ECompute>;

void FRHICommandUpdateTextureReference::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(UpdateTextureReference);
	INTERNAL_DECORATOR(RHIUpdateTextureReference)(TextureRef, NewTexture);
}

void FRHICommandBeginScene::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginScene);
	INTERNAL_DECORATOR(RHIBeginScene)();
}

void FRHICommandEndScene::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndScene);
	INTERNAL_DECORATOR(RHIEndScene)();
}

void FRHICommandBeginFrame::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginFrame);
	INTERNAL_DECORATOR(RHIBeginFrame)();
}

void FRHICommandEndFrame::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndFrame);
	INTERNAL_DECORATOR(RHIEndFrame)();
}

void FRHICommandBeginDrawingViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginDrawingViewport);
	INTERNAL_DECORATOR(RHIBeginDrawingViewport)(Viewport, RenderTargetRHI);
}

void FRHICommandEndDrawingViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawingViewport);
	INTERNAL_DECORATOR(RHIEndDrawingViewport)(Viewport, bPresent, bLockToVsync);
}

template<ECmdList CmdListType>
void FRHICommandPushEvent<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PushEvent);
	INTERNAL_DECORATOR_CONTEXT(RHIPushEvent)(Name, Color);
}
template struct FRHICommandPushEvent<ECmdList::EGfx>;
template struct FRHICommandPushEvent<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandPopEvent<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PopEvent);
	INTERNAL_DECORATOR_CONTEXT(RHIPopEvent)();
}
template struct FRHICommandPopEvent<ECmdList::EGfx>;
template struct FRHICommandPopEvent<ECmdList::ECompute>;


