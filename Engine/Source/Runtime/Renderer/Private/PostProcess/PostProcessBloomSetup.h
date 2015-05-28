// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBloomSetup.h: Post processing bloom threshold pass implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// ePId_Input0: Half res HDR scene color
// ePId_Input1: EyeAdaptation
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessBloomSetup : public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
};

// ePId_Input0: HDR SceneColor
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessVisualizeBloomSetup : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
};

// ePId_Input0: LDR SceneColor
// ePId_Input1: HDR SceneColor
// ePId_Input2: BloomOutputCombined
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessVisualizeBloomOverlay : public TRenderingCompositePassBase<3, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
};
