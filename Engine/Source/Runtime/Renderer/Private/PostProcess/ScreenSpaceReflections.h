// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenSpaceReflections.h: Post processing Screen Space Reflections implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDepthDownSample : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// ePId_Input0: scene color
// ePId_Input1: scene depth
// ePId_Input2: hierarchical scene color (optional)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessScreenSpaceReflections : public TRenderingCompositePassBase<3, 1>
{
public:
	FRCPassPostProcessScreenSpaceReflections( bool InPrevFrame )
	: bPrevFrame( InPrevFrame )
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	bool bPrevFrame;
};


// ePId_Input0: half res scene color
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessApplyScreenSpaceReflections : public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

void ScreenSpaceReflections(FRHICommandListImmediate& RHICmdList, FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& SSROutput);

bool DoScreenSpaceReflections(const FViewInfo& View);
