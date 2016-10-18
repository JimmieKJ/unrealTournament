// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDeferredDecals.h: Deferred Decals implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"
#include "DecalRenderingShared.h"

// ePId_Input0: SceneColor (not needed for DBuffer decals)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDeferredDecals : public TRenderingCompositePassBase<1, 1>
{
public:
	// One instance for each render stage
	FRCPassPostProcessDeferredDecals(EDecalRenderStage InDecalRenderStage);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// see EDecalRenderStage
	EDecalRenderStage CurrentStage;
	void DecodeRTWriteMask(FRenderingCompositePassContext& Context);
};

bool IsDBufferEnabled();

static inline bool IsWritingToGBufferA(FDecalRenderingCommon::ERenderTargetMode RenderTargetMode)
{
	return RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferWithNormal
		|| RenderTargetMode == FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteWithNormal
		|| RenderTargetMode == FDecalRenderingCommon::RTM_GBufferNormal;
}

struct FDecalRenderTargetManager
{
	enum EDecalResolveBufferIndex
	{
		SceneColorIndex,
		GBufferAIndex,
		GBufferBIndex,
		GBufferCIndex,
		GBufferEIndex,
		DBufferAIndex,
		DBufferBIndex,
		DBufferCIndex,
		ResolveBufferMax,
	};
	//
	FRHICommandList& RHICmdList;
	//
	bool TargetsToTransitionWritable[ResolveBufferMax];
	//
	FTextureRHIParamRef TargetsToResolve[ResolveBufferMax];
	//
	bool bGufferADirty;

	// constructor
	FDecalRenderTargetManager(FRHICommandList& InRHICmdList, EShaderPlatform ShaderPlatform, EDecalRenderStage CurrentStage)
		: RHICmdList(InRHICmdList)
		, bGufferADirty(false) 
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		for (uint32 i = 0; i < ResolveBufferMax; ++i)
		{
			TargetsToTransitionWritable[i] = true;
			TargetsToResolve[i] = nullptr;
		}

		if(SceneContext.DBufferA)
		{
			TargetsToResolve[DBufferAIndex] = SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture;
		}
		if(SceneContext.DBufferB)
		{
			TargetsToResolve[DBufferBIndex] = SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture;
		}

		if(SceneContext.DBufferC)
		{
			TargetsToResolve[DBufferCIndex] = SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture;
		}

		if (!IsAnyForwardShadingEnabled(ShaderPlatform))
		{
			// Normal buffer is already dirty at this point and needs resolve before being read from (irrelevant for DBuffer).
			bGufferADirty = CurrentStage == DRS_AfterBasePass;
		}
	}

	// destructor
	~FDecalRenderTargetManager()
	{

	}

	void ResolveTargets()
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// If GBuffer A is dirty, mark it as needing resolve since the content of TargetsToResolve[GBufferAIndex] could have been nullified by modes like RTM_SceneColorAndGBufferNoNormal
		if (bGufferADirty)
		{
			TargetsToResolve[FDecalRenderTargetManager::GBufferAIndex] = SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture;
		}

		// resolve the targets we wrote to.
		FResolveParams ResolveParams;
		for (int32 i = 0; i < ResolveBufferMax; ++i)
		{
			if (TargetsToResolve[i])
			{
				RHICmdList.CopyToResolveTarget(TargetsToResolve[i], TargetsToResolve[i], true, ResolveParams);
			}
		}
	}

	void FlushMetaData()
	{
		RHICmdList.TransitionResource(EResourceTransitionAccess::EMetaData, nullptr);
	}

	void SetRenderTargetMode(FDecalRenderingCommon::ERenderTargetMode CurrentRenderTargetMode, bool bHasNormal)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// If GBufferA was resolved for read, and we want to write to it again.
		if (!bGufferADirty && IsWritingToGBufferA(CurrentRenderTargetMode)) 
		{
			// This is required to be compliant with RHISetRenderTargets resource transition code : const bool bAccessValid = !bReadable || LastFrameWritten != CurrentFrame;
			// If the normal buffer was resolved as a texture before, then bReadable && LastFrameWritten == CurrentFrame, and an error msg will be triggered. 
			// Which is not needed here since no more read will be done at this point (at least not before any other CopyToResolvedTarget).
			RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture);
		}
		switch (CurrentRenderTargetMode)
		{
			case FDecalRenderingCommon::RTM_SceneColorAndGBufferWithNormal:
			case FDecalRenderingCommon::RTM_SceneColorAndGBufferNoNormal:
				TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
				TargetsToResolve[GBufferAIndex] = bHasNormal ? SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture : (PLATFORM_MAC ? SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture : nullptr); // @todo Workaround a Mac NV/Intel graphics driver bug that requires we pointlessly bind into RT1 even though we don't write to it, otherwise the writes to RT2 and RT3 go haywire. This isn't really possible to fix lower down the stack.
				TargetsToResolve[GBufferBIndex] = SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture;
				TargetsToResolve[GBufferCIndex] = SceneContext.GBufferC->GetRenderTargetItem().TargetableTexture;
				SetRenderTargets(RHICmdList, 4, TargetsToResolve, SceneContext.GetSceneDepthTexture(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
				break;

			case FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteWithNormal:
			case FDecalRenderingCommon::RTM_SceneColorAndGBufferDepthWriteNoNormal:
				TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
				TargetsToResolve[GBufferAIndex] = bHasNormal ? SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture : (PLATFORM_MAC ? SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture : nullptr); // @todo Workaround a Mac NV/Intel graphics driver bug that requires we pointlessly bind into RT1 even though we don't write to it, otherwise the writes to RT2 and RT3 go haywire. This isn't really possible to fix lower down the stack.
				TargetsToResolve[GBufferBIndex] = SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture;
				TargetsToResolve[GBufferCIndex] = SceneContext.GBufferC->GetRenderTargetItem().TargetableTexture;
				TargetsToResolve[GBufferEIndex] = SceneContext.GBufferE->GetRenderTargetItem().TargetableTexture;
				SetRenderTargets(RHICmdList, 5, TargetsToResolve, SceneContext.GetSceneDepthTexture(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
				break;

			case FDecalRenderingCommon::RTM_GBufferNormal:
				TargetsToResolve[GBufferAIndex] = SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture;
				SetRenderTarget(RHICmdList, TargetsToResolve[GBufferAIndex], SceneContext.GetSceneDepthTexture(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
				break;

			case FDecalRenderingCommon::RTM_SceneColor:
				TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
				SetRenderTarget(RHICmdList, TargetsToResolve[SceneColorIndex], SceneContext.GetSceneDepthTexture(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
				break;

			case FDecalRenderingCommon::RTM_DBuffer:
				TargetsToResolve[DBufferAIndex] = SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture;
				TargetsToResolve[DBufferBIndex] = SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture;
				TargetsToResolve[DBufferCIndex] = SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture;
				SetRenderTargets(RHICmdList, 3, &TargetsToResolve[DBufferAIndex], SceneContext.GetSceneDepthTexture(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
				break;

			default:
				check(0);
				break;
		}
		TargetsToTransitionWritable[CurrentRenderTargetMode] = false;

		// we need to reset the stream source after any call to SetRenderTarget (at least for Metal, which doesn't queue up VB assignments)
		RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), sizeof(FVector4), 0);
	}
};

extern void SetDecalBlendState(FRHICommandList& RHICmdList, const ERHIFeatureLevel::Type SMFeatureLevel, EDecalRenderStage InDecalRenderStage, EDecalBlendMode DecalBlendMode, bool bHasNormal);

extern void RenderMeshDecals(FRenderingCompositePassContext& Context, EDecalRenderStage CurrentDecalStage);
