// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessPassThrough.h: Post processing pass through implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// ePId_Input0: Input image
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessPassThrough : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	// @param InDest - 0 if a new intermediate target should be created
	FRCPassPostProcessPassThrough(IPooledRenderTarget* InDest, bool bInAdditiveBlend = false);
	// constructor
	FRCPassPostProcessPassThrough(FPooledRenderTargetDesc InNewDesc);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// 0 if a new intermediate should be created
	IPooledRenderTarget* Dest;
	bool bAdditiveBlend;
	FPooledRenderTargetDesc NewDesc;
};

// useful to replace the SceneColor and leave the other views e.g. when doing stereo rendering, assumed there is no scaling on the size (input and output is BufferSize)
// call before SetViewport()
// assumes this to be called before
//  SetRenderTarget()
//  Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
//  Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
// 	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
void CopyOverOtherViewportsIfNeeded(FRenderingCompositePassContext& Context, const FSceneView& ExcludeView);