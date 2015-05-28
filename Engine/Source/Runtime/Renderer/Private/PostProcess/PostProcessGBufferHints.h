// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessGBufferHints.h: Post processing GBufferHints implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: Emissive (SceneColor without lighting)
class FRCPassPostProcessGBufferHints : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	FRCPassPostProcessGBufferHints();

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

