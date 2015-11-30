// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandListCommandExecutes.inl: RHI Command List execute functions.
=============================================================================*/

#if !defined(INTERNAL_DECORATOR)
	#define INTERNAL_DECORATOR(Method) CmdList.GetContext().RHI##Method
#endif

//for functions where the signatures do not match between gfx and compute commandlists
#if !defined(INTERNAL_DECORATOR_COMPUTE)
#define INTERNAL_DECORATOR_COMPUTE(Method) CmdList.GetComputeContext().RHI##Method
#endif

//for functions where the signatures match between gfx and compute commandlists
#if !defined(INTERNAL_DECORATOR_CONTEXT_PARAM1)
#define INTERNAL_DECORATOR_CONTEXT(Method) IRHIComputeContext& Context = (CmdListType == ECmdList::EGfx) ? CmdList.GetContext() : CmdList.GetComputeContext(); Context.RHI##Method
#endif



void FRHICommandSetRasterizerState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRasterizerState);
	INTERNAL_DECORATOR(SetRasterizerState)(State);
}

void FRHICommandSetDepthStencilState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetDepthStencilState);
	INTERNAL_DECORATOR(SetDepthStencilState)(State, StencilRef);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameter);
	INTERNAL_DECORATOR(SetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue); 
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
	INTERNAL_DECORATOR_COMPUTE(SetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderUniformBuffer<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUniformBuffer);
	INTERNAL_DECORATOR(SetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
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
	INTERNAL_DECORATOR_COMPUTE(SetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderTexture<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderTexture);
	INTERNAL_DECORATOR(SetShaderTexture)(Shader, TextureIndex, Texture);
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
	INTERNAL_DECORATOR_COMPUTE(SetShaderTexture)(Shader, TextureIndex, Texture);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderResourceViewParameter);
	INTERNAL_DECORATOR(SetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
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
	INTERNAL_DECORATOR_COMPUTE(SetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
}

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetUAVParameter<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR_CONTEXT(SetUAVParameter)(Shader, UAVIndex, UAV);
}
template struct FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>;

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetUAVParameter_IntialCount<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR_CONTEXT(SetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
}
template struct FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx>;
template struct FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::ECompute>;

template <typename TShaderRHIParamRef, ECmdList CmdListType>
void FRHICommandSetShaderSampler<TShaderRHIParamRef, CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderSampler);
	INTERNAL_DECORATOR(SetShaderSampler)(Shader, SamplerIndex, Sampler);
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
	INTERNAL_DECORATOR_COMPUTE(SetShaderSampler)(Shader, SamplerIndex, Sampler);
}

void FRHICommandDrawPrimitive::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawPrimitive);
	INTERNAL_DECORATOR(DrawPrimitive)(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
}

void FRHICommandDrawIndexedPrimitive::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedPrimitive);
	INTERNAL_DECORATOR(DrawIndexedPrimitive)(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
}

void FRHICommandSetBoundShaderState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetBoundShaderState);
	INTERNAL_DECORATOR(SetBoundShaderState)(BoundShaderState);
}

void FRHICommandSetBlendState::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetBlendState);
	INTERNAL_DECORATOR(SetBlendState)(State, BlendFactor);
}

void FRHICommandSetStreamSource::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetStreamSource);
	INTERNAL_DECORATOR(SetStreamSource)(StreamIndex, VertexBuffer, Stride, Offset);
}

void FRHICommandSetViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetViewport);
	INTERNAL_DECORATOR(SetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FRHICommandSetScissorRect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetScissorRect);
	INTERNAL_DECORATOR(SetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
}

void FRHICommandSetRenderTargets::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRenderTargets);
	INTERNAL_DECORATOR(SetRenderTargets)(
		NewNumSimultaneousRenderTargets,
		NewRenderTargetsRHI,
		&NewDepthStencilTarget,
		NewNumUAVs,
		UAVs);
}

void FRHICommandSetRenderTargetsAndClear::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetRenderTargetsAndClear);
	INTERNAL_DECORATOR(SetRenderTargetsAndClear)(RenderTargetsInfo);
}

void FRHICommandBindClearMRTValues::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BindClearMRTValues);
	INTERNAL_DECORATOR(BindClearMRTValues)(
		bClearColor,
		bClearDepth,
		bClearStencil		
		);
}

void FRHICommandEndDrawPrimitiveUP::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawPrimitiveUP);
	void* Buffer = NULL;
	INTERNAL_DECORATOR(BeginDrawPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, Buffer);
	FMemory::Memcpy(Buffer, OutVertexData, NumVertices * VertexDataStride);
	INTERNAL_DECORATOR(EndDrawPrimitiveUP)();
}

void FRHICommandEndDrawIndexedPrimitiveUP::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawIndexedPrimitiveUP);
	void* VertexBuffer = nullptr;
	void* IndexBuffer = nullptr;
	INTERNAL_DECORATOR(BeginDrawIndexedPrimitiveUP)(
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
	INTERNAL_DECORATOR(EndDrawIndexedPrimitiveUP)();
}

template<ECmdList CmdListType>
void FRHICommandSetComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetComputeShader);
	INTERNAL_DECORATOR_CONTEXT(SetComputeShader)(ComputeShader);
}
template struct FRHICommandSetComputeShader<ECmdList::EGfx>;
template struct FRHICommandSetComputeShader<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandDispatchComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchComputeShader);
	INTERNAL_DECORATOR_CONTEXT(DispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
template struct FRHICommandDispatchComputeShader<ECmdList::EGfx>;
template struct FRHICommandDispatchComputeShader<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandDispatchIndirectComputeShader<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchIndirectComputeShader);
	INTERNAL_DECORATOR_CONTEXT(DispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
}
template struct FRHICommandDispatchIndirectComputeShader<ECmdList::EGfx>;
template struct FRHICommandDispatchIndirectComputeShader<ECmdList::ECompute>;

void FRHICommandAutomaticCacheFlushAfterComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(AutomaticCacheFlushAfterComputeShader);
	INTERNAL_DECORATOR(AutomaticCacheFlushAfterComputeShader)(bEnable);
}

void FRHICommandFlushComputeShaderCache::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(FlushComputeShaderCache);
	INTERNAL_DECORATOR(FlushComputeShaderCache)();
}

void FRHICommandDrawPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawPrimitiveIndirect);
	INTERNAL_DECORATOR(DrawPrimitiveIndirect)(PrimitiveType, ArgumentBuffer, ArgumentOffset);
}

void FRHICommandDrawIndexedIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedIndirect);
	INTERNAL_DECORATOR(DrawIndexedIndirect)(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
}

void FRHICommandDrawIndexedPrimitiveIndirect::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DrawIndexedPrimitiveIndirect);
	INTERNAL_DECORATOR(DrawIndexedPrimitiveIndirect)(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
}

void FRHICommandEnableDepthBoundsTest::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EnableDepthBoundsTest);
	INTERNAL_DECORATOR(EnableDepthBoundsTest)(bEnable, MinDepth, MaxDepth);
}

void FRHICommandClearUAV::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ClearUAV);
	INTERNAL_DECORATOR(ClearUAV)(UnorderedAccessViewRHI, Values);
}

void FRHICommandCopyToResolveTarget::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(CopyToResolveTarget);
	INTERNAL_DECORATOR(CopyToResolveTarget)(SourceTexture, DestTexture, bKeepOriginalSurface, ResolveParams);
}

void FRHICommandTransitionTextures::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionTextures);
	INTERNAL_DECORATOR(TransitionResources)(TransitionType, &Textures[0], NumTextures);
}

void FRHICommandTransitionTexturesArray::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionTextures);
	INTERNAL_DECORATOR(TransitionResources)(TransitionType, &Textures[0], Textures.Num());
}

template<ECmdList CmdListType>
void FRHICommandTransitionUAVs<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(TransitionUAVs);
	INTERNAL_DECORATOR_CONTEXT(TransitionResources)(TransitionType, TransitionPipeline, UAVs, NumUAVs, WriteFence);
}
template struct FRHICommandTransitionUAVs<ECmdList::EGfx>;
template struct FRHICommandTransitionUAVs<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandWaitComputeFence<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(WaitComputeFence);
	INTERNAL_DECORATOR_CONTEXT(WaitComputeFence)(WaitFence);
}
template struct FRHICommandWaitComputeFence<ECmdList::EGfx>;
template struct FRHICommandWaitComputeFence<ECmdList::ECompute>;

void FRHICommandClear::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(Clear);
	INTERNAL_DECORATOR(Clear)(bClearColor, Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FRHICommandClearMRT::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(ClearMRT);
	INTERNAL_DECORATOR(ClearMRT)(bClearColor, NumClearColors, ColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FRHIBeginAsyncComputeJob_DrawThread::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginAsyncComputeJob_DrawThread);
	INTERNAL_DECORATOR(BeginAsyncComputeJob_DrawThread)(Priority);
}

void FRHIEndAsyncComputeJob_DrawThread::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndAsyncComputeJob_DrawThread);
	INTERNAL_DECORATOR(EndAsyncComputeJob_DrawThread)(FenceIndex);
}

void FRHIGraphicsWaitOnAsyncComputeJob::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(GraphicsWaitOnAsyncComputeJob);
	INTERNAL_DECORATOR(GraphicsWaitOnAsyncComputeJob)(FenceIndex);
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

	INTERNAL_DECORATOR(SetBoundShaderState)(LocalBoundShaderState.WorkArea->ComputedBSS->BSS);

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
	INTERNAL_DECORATOR(SetShaderUniformBuffer)(Shader, BaseIndex, LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UniformBuffer);
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
	INTERNAL_DECORATOR(BeginRenderQuery)(RenderQuery);
}

void FRHICommandEndRenderQuery::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndRenderQuery);
	INTERNAL_DECORATOR(EndRenderQuery)(RenderQuery);
}

void FRHICommandBeginOcclusionQueryBatch::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginOcclusionQueryBatch);
	INTERNAL_DECORATOR(BeginOcclusionQueryBatch)();
}

void FRHICommandEndOcclusionQueryBatch::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndOcclusionQueryBatch);
	INTERNAL_DECORATOR(EndOcclusionQueryBatch)();
}

template<ECmdList CmdListType>
void FRHICommandSubmitCommandsHint<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SubmitCommandsHint);
	INTERNAL_DECORATOR_CONTEXT(SubmitCommandsHint)();
}
template struct FRHICommandSubmitCommandsHint<ECmdList::EGfx>;
template struct FRHICommandSubmitCommandsHint<ECmdList::ECompute>;

void FRHICommandUpdateTextureReference::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(UpdateTextureReference);
	INTERNAL_DECORATOR(UpdateTextureReference)(TextureRef, NewTexture);
}

void FRHICommandBeginScene::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginScene);
	INTERNAL_DECORATOR(BeginScene)();
}

void FRHICommandEndScene::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndScene);
	INTERNAL_DECORATOR(EndScene)();
}

void FRHICommandBeginFrame::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginFrame);
	INTERNAL_DECORATOR(BeginFrame)();
}

void FRHICommandEndFrame::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndFrame);
	INTERNAL_DECORATOR(EndFrame)();
}

void FRHICommandBeginDrawingViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(BeginDrawingViewport);
	INTERNAL_DECORATOR(BeginDrawingViewport)(Viewport, RenderTargetRHI);
}

void FRHICommandEndDrawingViewport::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(EndDrawingViewport);
	INTERNAL_DECORATOR(EndDrawingViewport)(Viewport, bPresent, bLockToVsync);
}

template<ECmdList CmdListType>
void FRHICommandPushEvent<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PushEvent);
	INTERNAL_DECORATOR_CONTEXT(PushEvent)(Name);
}
template struct FRHICommandPushEvent<ECmdList::EGfx>;
template struct FRHICommandPushEvent<ECmdList::ECompute>;

template<ECmdList CmdListType>
void FRHICommandPopEvent<CmdListType>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PopEvent);
	INTERNAL_DECORATOR_CONTEXT(PopEvent)();
}
template struct FRHICommandPopEvent<ECmdList::EGfx>;
template struct FRHICommandPopEvent<ECmdList::ECompute>;


