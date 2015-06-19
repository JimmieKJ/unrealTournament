// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandListCommandExecutes.inl: RHI Command List execute functions.
=============================================================================*/

#if !defined(INTERNAL_DECORATOR)
	#define INTERNAL_DECORATOR(Method) CmdList.GetContext().RHI##Method
#endif

// set this one to get a stat for each RHI command 
#define RHI_STATS 0

#if RHI_STATS
	DECLARE_STATS_GROUP(TEXT("RHICommands"),STATGROUP_RHI_COMMANDS, STATCAT_Advanced);
	#define RHISTAT(Method)	DECLARE_SCOPE_CYCLE_COUNTER(TEXT(#Method), STAT_RHI##Method, STATGROUP_RHI_COMMANDS)
#else
	#define RHISTAT(Method)
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

template <typename TShaderRHIParamRef>
void FRHICommandSetShaderParameter<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderParameter);
	INTERNAL_DECORATOR(SetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue); 
}
template struct FRHICommandSetShaderParameter<FVertexShaderRHIParamRef>;
template struct FRHICommandSetShaderParameter<FHullShaderRHIParamRef>;
template struct FRHICommandSetShaderParameter<FDomainShaderRHIParamRef>;
template struct FRHICommandSetShaderParameter<FGeometryShaderRHIParamRef>;
template struct FRHICommandSetShaderParameter<FPixelShaderRHIParamRef>;
template struct FRHICommandSetShaderParameter<FComputeShaderRHIParamRef>;

template <typename TShaderRHIParamRef>
void FRHICommandSetShaderUniformBuffer<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderUniformBuffer);
	INTERNAL_DECORATOR(SetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
}
template struct FRHICommandSetShaderUniformBuffer<FVertexShaderRHIParamRef>;
template struct FRHICommandSetShaderUniformBuffer<FHullShaderRHIParamRef>;
template struct FRHICommandSetShaderUniformBuffer<FDomainShaderRHIParamRef>;
template struct FRHICommandSetShaderUniformBuffer<FGeometryShaderRHIParamRef>;
template struct FRHICommandSetShaderUniformBuffer<FPixelShaderRHIParamRef>;
template struct FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef>;

template <typename TShaderRHIParamRef>
void FRHICommandSetShaderTexture<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderTexture);
	INTERNAL_DECORATOR(SetShaderTexture)(Shader, TextureIndex, Texture);
}
template struct FRHICommandSetShaderTexture<FVertexShaderRHIParamRef>;
template struct FRHICommandSetShaderTexture<FHullShaderRHIParamRef>;
template struct FRHICommandSetShaderTexture<FDomainShaderRHIParamRef>;
template struct FRHICommandSetShaderTexture<FGeometryShaderRHIParamRef>;
template struct FRHICommandSetShaderTexture<FPixelShaderRHIParamRef>;
template struct FRHICommandSetShaderTexture<FComputeShaderRHIParamRef>;

template <typename TShaderRHIParamRef>
void FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderResourceViewParameter);
	INTERNAL_DECORATOR(SetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
}
template struct FRHICommandSetShaderResourceViewParameter<FVertexShaderRHIParamRef>;
template struct FRHICommandSetShaderResourceViewParameter<FHullShaderRHIParamRef>;
template struct FRHICommandSetShaderResourceViewParameter<FDomainShaderRHIParamRef>;
template struct FRHICommandSetShaderResourceViewParameter<FGeometryShaderRHIParamRef>;
template struct FRHICommandSetShaderResourceViewParameter<FPixelShaderRHIParamRef>;
template struct FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef>;

template <typename TShaderRHIParamRef>
void FRHICommandSetUAVParameter<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR(SetUAVParameter)(Shader, UAVIndex, UAV);
}
template struct FRHICommandSetUAVParameter<FComputeShaderRHIParamRef>;

template <typename TShaderRHIParamRef>
void FRHICommandSetUAVParameter_IntialCount<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetUAVParameter);
	INTERNAL_DECORATOR(SetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
}
template struct FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef>;

template <typename TShaderRHIParamRef>
void FRHICommandSetShaderSampler<TShaderRHIParamRef>::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetShaderSampler);
	INTERNAL_DECORATOR(SetShaderSampler)(Shader, SamplerIndex, Sampler);
}
template struct FRHICommandSetShaderSampler<FVertexShaderRHIParamRef>;
template struct FRHICommandSetShaderSampler<FHullShaderRHIParamRef>;
template struct FRHICommandSetShaderSampler<FDomainShaderRHIParamRef>;
template struct FRHICommandSetShaderSampler<FGeometryShaderRHIParamRef>;
template struct FRHICommandSetShaderSampler<FPixelShaderRHIParamRef>;
template struct FRHICommandSetShaderSampler<FComputeShaderRHIParamRef>;

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
		NumClearColors,
		ColorArray,
		bClearDepth,
		Depth,
		bClearStencil,
		Stencil	
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

void FRHICommandSetComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SetComputeShader);
	INTERNAL_DECORATOR(SetComputeShader)(ComputeShader);
}

void FRHICommandDispatchComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchComputeShader);
	INTERNAL_DECORATOR(DispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void FRHICommandDispatchIndirectComputeShader::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(DispatchIndirectComputeShader);
	INTERNAL_DECORATOR(DispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
}

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
	check(!IsValidRef(WorkArea.ComputedUniformBuffer->UniformBuffer) && WorkArea.Layout && WorkArea.Contents); // should not already have been created
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

void FRHICommandSubmitCommandsHint::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(SubmitCommandsHint);
	INTERNAL_DECORATOR(SubmitCommandsHint)();
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

void FRHICommandUpdateVertexBuffer::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(UpdateVertexBuffer);
	void* Data = LockVertexBuffer_Internal(VertexBuffer, 0, BufferSize, RLM_WriteOnly);
	FMemory::Memcpy(Data, Buffer, BufferSize);
	UnlockVertexBuffer_Internal(VertexBuffer);
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

void FRHICommandPushEvent::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PushEvent);
	INTERNAL_DECORATOR(PushEvent)(Name);
}

void FRHICommandPopEvent::Execute(FRHICommandListBase& CmdList)
{
	RHISTAT(PopEvent);
	INTERNAL_DECORATOR(PopEvent)();
}


