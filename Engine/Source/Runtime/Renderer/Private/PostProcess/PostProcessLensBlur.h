// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLensBlur.h: Post processing lens blur implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// ePId_Input0: Input image
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessLensBlur : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessLensBlur(float InPercentKernelSize, float InThreshold);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	

	float PercentKernelSize;
	float Threshold;
};
