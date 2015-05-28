// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommands.cpp: Metal RHI commands implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "GlobalShader.h"
#include "OneColorShader.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"

static const bool GUsesInvertedZ = true;
static FGlobalBoundShaderState GClearMRTBoundShaderState[8];

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
static TGlobalResource<FVector4VertexDeclaration> GVector4VertexDeclaration;

static MTLPrimitiveType TranslatePrimitiveType(uint32 PrimitiveType)
{
	switch (PrimitiveType)
	{
		case PT_TriangleList:	return MTLPrimitiveTypeTriangle;
		case PT_TriangleStrip:	return MTLPrimitiveTypeTriangleStrip;
		case PT_LineList:		return MTLPrimitiveTypeLine;
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unsupported primitive type %d"), (int32)PrimitiveType); return MTLPrimitiveTypeTriangle;
	}
}

void FMetalDynamicRHI::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Stride,uint32 Offset)
{
	FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	if (VertexBuffer != NULL)
	{
		[FMetalManager::GetContext() setVertexBuffer:VertexBuffer->Buffer offset:VertexBuffer->Offset + Offset atIndex:UNREAL_TO_METAL_BUFFER_INDEX(StreamIndex)];
	}
}

void FMetalDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	NOT_SUPPORTED("RHISetStreamOutTargets");

}

void FMetalDynamicRHI::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	FMetalRasterizerState* NewState = ResourceCast(NewStateRHI);

	NewState->Set();
}

void FMetalDynamicRHI::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	FMetalComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	FMetalManager::Get()->SetComputeShader(ComputeShader);
	
	// set this compute shader pipeline as the current (this resets all state, so we need to set all resources after calling this)
	[FMetalManager::GetComputeContext() setComputePipelineState:ComputeShader->Kernel];
}

void FMetalDynamicRHI::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{ 
	FMetalManager::Get()->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void FMetalDynamicRHI::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset) 
{ 
	NOT_SUPPORTED("RHIDispatchIndirectComputeShader");
}

void FMetalDynamicRHI::RHISetViewport(uint32 MinX,uint32 MinY,float MinZ,uint32 MaxX,uint32 MaxY,float MaxZ)
{
	// Ensure viewport fits inside the currently bound RT.
	FIntPoint TargetDims = FMetalManager::Get()->GetBoundRenderTargetDimensions();
	MinX = FMath::Min(MinX, (uint32)TargetDims.X);
	MaxX = FMath::Min(MaxX, (uint32)TargetDims.X);
	MinY = FMath::Min(MinY, (uint32)TargetDims.Y);
	MaxY = FMath::Min(MaxY, (uint32)TargetDims.Y);
	
	MTLViewport Viewport;
	Viewport.originX = MinX;
	Viewport.originY = MinY;
	Viewport.width = MaxX - MinX;
	Viewport.height = MaxY - MinY;
	Viewport.znear = MinZ;
	Viewport.zfar = MaxZ;
	
	[FMetalManager::GetContext() setViewport:Viewport];
}

void FMetalDynamicRHI::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) 
{ 
	NOT_SUPPORTED("RHISetMultipleViewports");
}

void FMetalDynamicRHI::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{
	MTLScissorRect Scissor;
	Scissor.x = MinX;
	Scissor.y = MinY;
	Scissor.width = MaxX - MinX;
	Scissor.height = MaxY - MinY;

	// metal doesn't support 0 sized scissor rect
	if (Scissor.width == 0 || Scissor.height == 0)
	{
		return;
	}
	[FMetalManager::GetContext() setScissorRect:Scissor];
}

void FMetalDynamicRHI::RHISetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	FMetalBoundShaderState* BoundShaderState = ResourceCast(BoundShaderStateRHI);

	FMetalManager::Get()->SetBoundShaderState(BoundShaderState);
	BoundShaderStateHistory.Add(BoundShaderState);
}


void FMetalDynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	FMetalUnorderedAccessView* UAV = ResourceCast(UAVRHI);

	if (UAV)
	{
		UAV->Set(UAVIndex);
	}
}

void FMetalDynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	NOT_SUPPORTED("RHISetUAVParameter");
}


void FMetalDynamicRHI::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
	{
		[FMetalManager::GetContext() setVertexTexture:Surface->Texture atIndex:TextureIndex];
	}
	else
	{
		[FMetalManager::GetContext() setVertexTexture:nil atIndex:TextureIndex];
	}
}

void FMetalDynamicRHI::RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	NOT_SUPPORTED("RHISetShaderTexture-Hull");
}

void FMetalDynamicRHI::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	NOT_SUPPORTED("RHISetShaderTexture-Domain");
}

void FMetalDynamicRHI::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	NOT_SUPPORTED("RHISetShaderTexture-Geometry");

}

void FMetalDynamicRHI::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
	{
		[FMetalManager::GetContext() setFragmentTexture:Surface->Texture atIndex:TextureIndex];
	}
	else
	{
		[FMetalManager::GetContext() setFragmentTexture:nil atIndex:TextureIndex];
	}
}

void FMetalDynamicRHI::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(NewTextureRHI);
	if (Surface != nullptr)
	{
		[FMetalManager::GetComputeContext() setTexture:Surface->Texture atIndex : TextureIndex];
	}
	else
	{
		[FMetalManager::GetComputeContext() setTexture:nil atIndex : TextureIndex];
	}
}


void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	if (SRV)
	{
		FRHITexture* Texture = SRV->SourceTexture.GetReference();
		if (Texture)
		{
			FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture);
			if (Surface != nullptr)
			{
				[FMetalManager::GetContext() setVertexTexture:Surface->Texture atIndex : TextureIndex];
			}
			else
			{
				[FMetalManager::GetContext() setVertexTexture:nil atIndex : TextureIndex];
			}
		}
		else
		{
			FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
			if (VB)
			{
				[FMetalManager::GetContext() setVertexBuffer:VB->Buffer offset:VB->Offset atIndex:TextureIndex];
			}
		}
	}
	else
	{
		[FMetalManager::GetContext() setVertexTexture:nil atIndex : TextureIndex];
	}
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	NOT_SUPPORTED("RHISetShaderResourceViewParameter");
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	NOT_SUPPORTED("RHISetShaderResourceViewParameter");
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	NOT_SUPPORTED("RHISetShaderResourceViewParameter");
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);
	if (SRV)
	{
		FRHITexture* Texture = SRV->SourceTexture.GetReference();
		if (Texture)
		{
			FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture);
			if (Surface != nullptr)
			{
				[FMetalManager::GetContext() setFragmentTexture:Surface->Texture atIndex : TextureIndex];
			}
			else
			{
				[FMetalManager::GetContext() setFragmentTexture:nil atIndex : TextureIndex];
			}
		}
		else
		{
			FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
			if (VB)
			{
				[FMetalManager::GetContext() setFragmentBuffer:VB->Buffer offset:VB->Offset atIndex:TextureIndex];
			}
		}
	}
	else
	{
		[FMetalManager::GetContext() setFragmentTexture:nil atIndex : TextureIndex];
	}
}

void FMetalDynamicRHI::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	FMetalShaderResourceView* SRV = ResourceCast(SRVRHI);

	if (SRV != NULL)
	{
		FRHITexture* Texture = SRV->SourceTexture.GetReference();
		if (Texture)
		{
			FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(Texture);
			if (Surface != nullptr)
			{
				[FMetalManager::GetComputeContext() setTexture:Surface->Texture atIndex : TextureIndex];
			}
			else
			{
				[FMetalManager::GetComputeContext() setTexture:nil atIndex : TextureIndex];
			}
		}
		else
		{
			FMetalVertexBuffer* VB = SRV->SourceVertexBuffer.GetReference();
			if (VB)
			{
				[FMetalManager::GetComputeContext() setBuffer:VB->Buffer offset : VB->Offset atIndex : TextureIndex];
			}
		}
	}
}


void FMetalDynamicRHI::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	[FMetalManager::GetContext() setVertexSamplerState:NewState->State atIndex:SamplerIndex];
}

void FMetalDynamicRHI::RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	NOT_SUPPORTED("RHISetSamplerState-Hull");
}

void FMetalDynamicRHI::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	NOT_SUPPORTED("RHISetSamplerState-Domain");
}

void FMetalDynamicRHI::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	NOT_SUPPORTED("RHISetSamplerState-Geometry");
}

void FMetalDynamicRHI::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	[FMetalManager::GetContext() setFragmentSamplerState:NewState->State atIndex:SamplerIndex];
}

void FMetalDynamicRHI::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FMetalSamplerState* NewState = ResourceCast(NewStateRHI);

	[FMetalManager::GetComputeContext() setSamplerState:NewState->State atIndex : SamplerIndex];
}

void FMetalDynamicRHI::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	FMetalManager::Get()->GetShaderParameters(CrossCompiler::SHADER_STAGE_VERTEX).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FMetalDynamicRHI::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	NOT_SUPPORTED("RHISetShaderParameter-Hull");
}

void FMetalDynamicRHI::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	FMetalManager::Get()->GetShaderParameters(CrossCompiler::SHADER_STAGE_PIXEL).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FMetalDynamicRHI::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	NOT_SUPPORTED("RHISetShaderParameter-Domain");
}

void FMetalDynamicRHI::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	NOT_SUPPORTED("RHISetShaderParameter-Geometry");
}

void FMetalDynamicRHI::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	FMetalManager::Get()->GetShaderParameters(CrossCompiler::SHADER_STAGE_COMPUTE).Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FMetalVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
	VertexShader->BoundUniformBuffers[BufferIndex] = BufferRHI;
	VertexShader->DirtyUniformBuffers |= 1 << BufferIndex;

	auto& Bindings = VertexShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		[FMetalManager::GetContext() setVertexBuffer:UB->Buffer offset:UB->Offset atIndex:BufferIndex];
	}
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	NOT_SUPPORTED("RHISetShaderUniformBuffer-Hull");
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	NOT_SUPPORTED("RHISetShaderUniformBuffer-Domain");
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	NOT_SUPPORTED("RHISetShaderUniformBuffer-Geometry");
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FMetalPixelShader* PixelShader = ResourceCast(PixelShaderRHI);
	PixelShader->BoundUniformBuffers[BufferIndex] = BufferRHI;
	PixelShader->DirtyUniformBuffers |= 1 << BufferIndex;

	auto& Bindings = PixelShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		[FMetalManager::GetContext() setFragmentBuffer:UB->Buffer offset:UB->Offset atIndex:BufferIndex];
	}
}

void FMetalDynamicRHI::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	FMetalComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	ComputeShader->BoundUniformBuffers[BufferIndex] = BufferRHI;
	ComputeShader->DirtyUniformBuffers |= 1 << BufferIndex;

	auto& Bindings = ComputeShader->Bindings;
	check(BufferIndex < Bindings.NumUniformBuffers);
	if (Bindings.bHasRegularUniformBuffers)
	{
		auto* UB = (FMetalUniformBuffer*)BufferRHI;
		[FMetalManager::GetComputeContext() setBuffer:UB->Buffer offset : UB->Offset atIndex : BufferIndex];
	}
}


void FMetalDynamicRHI::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	FMetalDepthStencilState* NewState = ResourceCast(NewStateRHI);

	NewState->Set();
}

void FMetalDynamicRHI::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	FMetalBlendState* NewState = ResourceCast(NewStateRHI);

	NewState->Set();
}


void FMetalDynamicRHI::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{
	FMetalManager* Manager = FMetalManager::Get();
	FRHIDepthRenderTargetView DepthView;
	if (NewDepthStencilTargetRHI)
	{
		DepthView = *NewDepthStencilTargetRHI;
	}
	else
	{
		DepthView = FRHIDepthRenderTargetView(FTextureRHIParamRef(), ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction);
	}

	FRHISetRenderTargetsInfo Info(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);
	RHISetRenderTargetsAndClear(Info);
	
	checkf(NumUAVs == 0, TEXT("Calling SetRenderTargets with UAVs is not supported in Metal yet"));
}

void FMetalDynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
}

void FMetalDynamicRHI::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	FMetalManager* Manager = FMetalManager::Get();

#if 1
	Manager->SetRenderTargetsInfo(RenderTargetsInfo);
#else
	for (int32 RenderTargetIndex = 0; RenderTargetIndex < MaxMetalRenderTargets; RenderTargetIndex++)
	{
		const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];
		// update the current RTs
		if (RenderTargetIndex < RenderTargetsInfo.NumColorRenderTargets && RenderTargetView.Texture != NULL)
		{
			FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);
			Manager->SetCurrentRenderTarget(Surface, RenderTargetIndex, RenderTargetView.MipIndex, RenderTargetView.ArraySliceIndex, GetMetalRTLoadAction(RenderTargetView.LoadAction), GetMetalRTStoreAction(RenderTargetView.StoreAction), RenderTargetsInfo.NumColorRenderTargets);
		}
		else
		{
			Manager->SetCurrentRenderTarget(NULL, RenderTargetIndex, 0, 0, MTLLoadActionDontCare, MTLStoreActionStore, RenderTargetsInfo.NumColorRenderTargets);
		}
	}

	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
	{
		FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(RenderTargetsInfo.DepthStencilRenderTarget.Texture);
		Manager->SetCurrentDepthStencilTarget(Surface, 
			GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction), 
			GetMetalRTStoreAction(RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction), 
			RenderTargetsInfo.DepthClearValue,
			GetMetalRTLoadAction(RenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction),
			GetMetalRTStoreAction(RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction()),
			RenderTargetsInfo.StencilClearValue);
	}
	else
	{
		Manager->SetCurrentDepthStencilTarget(NULL);
	}

	// now that we have a new render target, we need a new context to render to it!
	Manager->UpdateContext();
#endif


	// Set the viewport to the full size of render target 0.
	if (RenderTargetsInfo.ColorRenderTarget[0].Texture)
	{
		const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[0];
		FMetalSurface* RenderTarget = GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);

		uint32 Width = FMath::Max((uint32)(RenderTarget->Texture.width >> RenderTargetView.MipIndex), (uint32)1);
		uint32 Height = FMath::Max((uint32)(RenderTarget->Texture.height >> RenderTargetView.MipIndex), (uint32)1);

		RHISetViewport(0, 0, 0.0f, Width, Height, 1.0f);
	}
}

// Occlusion/Timer queries.
void FMetalDynamicRHI::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);

	Query->Begin();
}

void FMetalDynamicRHI::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);

	Query->End();
}


void FMetalDynamicRHI::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	
	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	// how many verts to render
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	// finalize any pending state
	FMetalManager::Get()->PrepareToDraw(NumVertices);

	// draw!
	[FMetalManager::GetContext() drawPrimitives:TranslatePrimitiveType(PrimitiveType)
									vertexStart:BaseVertexIndex
									vertexCount:NumVertices
								  instanceCount:NumInstances];
}

void FMetalDynamicRHI::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	NOT_SUPPORTED("RHIDrawPrimitiveIndirect");
}


void FMetalDynamicRHI::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	checkf(BaseVertexIndex  == 0, TEXT("BaseVertexIndex must be 0, see GRHISupportsBaseVertexIndex"));
	checkf(FirstInstance  == 0, TEXT("FirstInstance is currently unsupported on this RHI"));

	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	// finalize any pending state
	FMetalManager::Get()->PrepareToDraw(NumVertices);
	
	uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	[FMetalManager::GetContext() drawIndexedPrimitives:TranslatePrimitiveType(PrimitiveType)
											indexCount:NumIndices
											 indexType:IndexBuffer->IndexType
										   indexBuffer:IndexBuffer->Buffer
									 indexBufferOffset:IndexBuffer->Offset + StartIndex * IndexBuffer->GetStride()
										 instanceCount:NumInstances];
}

void FMetalDynamicRHI::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	NOT_SUPPORTED("RHIDrawIndexedIndirect");
}

void FMetalDynamicRHI::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	NOT_SUPPORTED("RHIDrawIndexedPrimitiveIndirect");
}


/** Some locally global variables to track the pending primitive information uised in RHIEnd*UP functions */
static uint32 GPendingVertexBufferOffset = 0xFFFFFFFF;
static uint32 GPendingVertexDataStride;

static uint32 GPendingIndexBufferOffset = 0xFFFFFFFF;
static uint32 GPendingIndexDataStride;

static uint32 GPendingPrimitiveType;
static uint32 GPendingNumPrimitives;

void FMetalDynamicRHI::RHIBeginDrawPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	checkSlow(GPendingVertexBufferOffset == 0xFFFFFFFF);

	// allocate space
	GPendingVertexBufferOffset = FMetalManager::Get()->AllocateFromRingBuffer(VertexDataStride * NumVertices);

	// get the pointer to send back for writing
	uint8* RingBufferBytes = (uint8*)[FMetalManager::Get()->GetRingBuffer() contents];
	OutVertexData = RingBufferBytes + GPendingVertexBufferOffset;
	
	GPendingPrimitiveType = PrimitiveType;
	GPendingNumPrimitives = NumPrimitives;
	GPendingVertexDataStride = VertexDataStride;
}


void FMetalDynamicRHI::RHIEndDrawPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);

	RHI_DRAW_CALL_STATS(GPendingPrimitiveType,GPendingNumPrimitives);

	// set the vertex buffer
	[FMetalManager::GetContext() setVertexBuffer:FMetalManager::Get()->GetRingBuffer() offset:GPendingVertexBufferOffset atIndex:UNREAL_TO_METAL_BUFFER_INDEX(0)];
	
	// how many to draw
	uint32 NumVertices = GetVertexCountForPrimitiveCount(GPendingNumPrimitives, GPendingPrimitiveType);

	// last minute draw setup
	FMetalManager::Get()->PrepareToDraw(0);
	
	[FMetalManager::GetContext() drawPrimitives:TranslatePrimitiveType(GPendingPrimitiveType)
									vertexStart:0
									vertexCount:NumVertices
								  instanceCount:1];
	
	// mark temp memory as usable
	GPendingVertexBufferOffset = 0xFFFFFFFF;
}

void FMetalDynamicRHI::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	checkSlow(GPendingVertexBufferOffset == 0xFFFFFFFF);
	checkSlow(GPendingIndexBufferOffset == 0xFFFFFFFF);

	// allocate space
	GPendingVertexBufferOffset = FMetalManager::Get()->AllocateFromRingBuffer(VertexDataStride * NumVertices);
	GPendingIndexBufferOffset = FMetalManager::Get()->AllocateFromRingBuffer(IndexDataStride * NumIndices);
	
	// get the pointers to send back for writing
	uint8* RingBufferBytes = (uint8*)[FMetalManager::Get()->GetRingBuffer() contents];
	OutVertexData = RingBufferBytes + GPendingVertexBufferOffset;
	OutIndexData = RingBufferBytes + GPendingIndexBufferOffset;
	
	GPendingPrimitiveType = PrimitiveType;
	GPendingNumPrimitives = NumPrimitives;
	GPendingIndexDataStride = IndexDataStride;

	GPendingVertexDataStride = VertexDataStride;
}

void FMetalDynamicRHI::RHIEndDrawIndexedPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);

	RHI_DRAW_CALL_STATS(GPendingPrimitiveType,GPendingNumPrimitives);

	// set the vertex buffer
	[FMetalManager::GetContext() setVertexBuffer:FMetalManager::Get()->GetRingBuffer() offset:GPendingVertexBufferOffset atIndex:UNREAL_TO_METAL_BUFFER_INDEX(0)];

	// how many to draw
	uint32 NumIndices = GetVertexCountForPrimitiveCount(GPendingNumPrimitives, GPendingPrimitiveType);

	// last minute draw setup
	FMetalManager::Get()->PrepareToDraw(0);
	
	[FMetalManager::GetContext() drawIndexedPrimitives:TranslatePrimitiveType(GPendingPrimitiveType)
											indexCount:NumIndices
											 indexType:(GPendingIndexDataStride == 2) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
										   indexBuffer:FMetalManager::Get()->GetRingBuffer()
									 indexBufferOffset:GPendingIndexBufferOffset
										 instanceCount:1];
	
	// mark temp memory as usable
	GPendingVertexBufferOffset = 0xFFFFFFFF;
	GPendingIndexBufferOffset = 0xFFFFFFFF;
}


void FMetalDynamicRHI::RHIClear(bool bClearColor,const FLinearColor& Color, bool bClearDepth,float Depth, bool bClearStencil,uint32 Stencil, FIntRect ExcludeRect)
{
	FMetalDynamicRHI::RHIClearMRT(bClearColor, 1, &Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FMetalDynamicRHI::RHIClearMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil, FIntRect ExcludeRect)
{
	// @todo metalmrt: This is currently only supporting clearing stencil by a shader (because shadows need to clear
	// stencil in the middle of an encoder, so we can't use LoadAction). This code was adapted from D3D code (would
	// be nice to share this code!), and parts removed until basically just what was needed for Stencil only is left
	// If we need other types of clears, diff this against the D3D version to see what it's missing
	if (bClearStencil && !bClearColor && !bClearDepth)
	{
		RHIPushEvent(TEXT("MetalClearStencil"));
		// we don't support draw call clears before the RHI is initialized, reorder the code or make sure it's not a draw call clear
		check(GIsRHIInitialized);

 		// Build new states
 		FBlendStateRHIParamRef BlendStateRHI = TStaticBlendState<CW_NONE>::GetRHI();

		FRasterizerStateRHIParamRef RasterizerStateRHI = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		float BF[4] = { 0, 0, 0, 0 };

		const FDepthStencilStateRHIParamRef DepthStencilStateRHI = 
			(bClearDepth && bClearStencil)
				? TStaticDepthStencilState<
					true, CF_Always,
					true,CF_Always,SO_Replace,SO_Replace,SO_Replace,
					false,CF_Always,SO_Replace,SO_Replace,SO_Replace,
					0xff,0xff
					>::GetRHI()
			: bClearDepth
				? TStaticDepthStencilState<true, CF_Always>::GetRHI()
			: bClearStencil
				? TStaticDepthStencilState<
					false, CF_Always,
					true,CF_Always,SO_Replace,SO_Replace,SO_Replace,
					true,CF_Always,SO_Replace,SO_Replace,SO_Replace,
					0xff,0xff
					>::GetRHI()
			:     TStaticDepthStencilState<false, CF_Always>::GetRHI();

		RHISetBlendState(BlendStateRHI, FLinearColor::Transparent);
		RHISetDepthStencilState(DepthStencilStateRHI, Stencil);
		RHISetRasterizerState(RasterizerStateRHI);

		// Set the new shaders
		auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);

		FOneColorPS* PixelShader = NULL;

		TShaderMapRef<TOneColorPixelShaderMRT<1> > MRTPixelShader(ShaderMap);
		PixelShader = *MRTPixelShader;

		{
			FRHICommandList_RecursiveHazardous RHICmdList(this);
			SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, GClearMRTBoundShaderState[0], GVector4VertexDeclaration.VertexDeclarationRHI, *VertexShader, PixelShader);
			FLinearColor ShaderClearColors[MaxSimultaneousRenderTargets];
			FMemory::Memzero(ShaderClearColors);

			for (int32 i = 0; i < NumClearColors; i++)
			{
				ShaderClearColors[i] = ClearColorArray[i];
			}

			SetShaderValueArray(RHICmdList, PixelShader->GetPixelShader(), PixelShader->ColorParameter, ShaderClearColors, NumClearColors);

			{
				// Draw a fullscreen quad
				if (ExcludeRect.Width() > 0 && ExcludeRect.Height() > 0)
				{
					// with a hole in it (optimization in case the hardware has non constant clear performance)
					FVector4 OuterVertices[4];
					OuterVertices[0].Set(-1.0f, 1.0f, Depth, 1.0f);
					OuterVertices[1].Set(1.0f, 1.0f, Depth, 1.0f);
					OuterVertices[2].Set(1.0f, -1.0f, Depth, 1.0f);
					OuterVertices[3].Set(-1.0f, -1.0f, Depth, 1.0f);

					UE_LOG(LogMetal, Fatal, TEXT("ExcludeRect not supported yet in Metal Clear"));
					float InvViewWidth = 1.0f / 100;//Viewport.Width;
					float InvViewHeight = 1.0f / 100;//Viewport.Height;
					FVector4 FractionRect = FVector4(ExcludeRect.Min.X * InvViewWidth, ExcludeRect.Min.Y * InvViewHeight, (ExcludeRect.Max.X - 1) * InvViewWidth, (ExcludeRect.Max.Y - 1) * InvViewHeight);

					FVector4 InnerVertices[4];
					InnerVertices[0].Set(FMath::Lerp(-1.0f, 1.0f, FractionRect.X), FMath::Lerp(1.0f, -1.0f, FractionRect.Y), Depth, 1.0f);
					InnerVertices[1].Set(FMath::Lerp(-1.0f, 1.0f, FractionRect.Z), FMath::Lerp(1.0f, -1.0f, FractionRect.Y), Depth, 1.0f);
					InnerVertices[2].Set(FMath::Lerp(-1.0f, 1.0f, FractionRect.Z), FMath::Lerp(1.0f, -1.0f, FractionRect.W), Depth, 1.0f);
					InnerVertices[3].Set(FMath::Lerp(-1.0f, 1.0f, FractionRect.X), FMath::Lerp(1.0f, -1.0f, FractionRect.W), Depth, 1.0f);

					FVector4 Vertices[10];
					Vertices[0] = OuterVertices[0];
					Vertices[1] = InnerVertices[0];
					Vertices[2] = OuterVertices[1];
					Vertices[3] = InnerVertices[1];
					Vertices[4] = OuterVertices[2];
					Vertices[5] = InnerVertices[2];
					Vertices[6] = OuterVertices[3];
					Vertices[7] = InnerVertices[3];
					Vertices[8] = OuterVertices[0];
					Vertices[9] = InnerVertices[0];

					DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 8, Vertices, sizeof(Vertices[0]));
				}
				else
				{
					// without a hole
					FVector4 Vertices[4];
					Vertices[0].Set(-1.0f, 1.0f, Depth, 1.0f);
					Vertices[1].Set(1.0f, 1.0f, Depth, 1.0f);
					Vertices[2].Set(-1.0f, -1.0f, Depth, 1.0f);
					Vertices[3].Set(1.0f, -1.0f, Depth, 1.0f);
					DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
				}
			}
			// Implicit flush. Always call flush when using a command list in RHI implementations before doing anything else. This is super hazardous.
		}

		RHIPopEvent();
	}
}											

void FMetalDynamicRHI::RHIBindClearMRTValues(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
{
	// Not necessary
}

void FMetalDynamicRHI::RHISuspendRendering()
{
	// Not supported
}

void FMetalDynamicRHI::RHIResumeRendering()
{
	// Not supported
}

bool FMetalDynamicRHI::RHIIsRenderingSuspended()
{
	// Not supported
	return false;
}

void FMetalDynamicRHI::RHIBlockUntilGPUIdle()
{
	NOT_SUPPORTED("RHIBlockUntilGPUIdle");
}

uint32 FMetalDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FMetalDynamicRHI::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) 
{
	NOT_SUPPORTED("RHIAutomaticCacheFlushAfterComputeShader");
}

void FMetalDynamicRHI::RHIFlushComputeShaderCache()
{
	NOT_SUPPORTED("RHIFlushComputeShaderCache");
}

void FMetalDynamicRHI::RHIExecuteCommandList(FRHICommandList* RHICmdList)
{
	NOT_SUPPORTED("RHIExecuteCommandList");
}

void FMetalDynamicRHI::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	// not supported
}

void FMetalDynamicRHI::RHISubmitCommandsHint()
{
    
}

IRHICommandContext* FMetalDynamicRHI::RHIGetDefaultContext()
{
	return this;
}

IRHICommandContextContainer* FMetalDynamicRHI::RHIGetCommandContextContainer()
{
	return nullptr;
}



