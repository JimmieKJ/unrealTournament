// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeShadingModels.h: Post processing VisualizeShadingModels implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: LDR SceneColor
class FRCPassPostProcessVisualizeShadingModels : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessVisualizeShadingModels(FRHICommandList& RHICmdList);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

