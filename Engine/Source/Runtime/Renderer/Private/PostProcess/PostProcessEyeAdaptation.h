// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessEyeAdaptation.h: Post processing eye adaptation implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// ePId_Input0: HDRHistogram or nothing
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessEyeAdaptation : public TRenderingCompositePassBase<1, 1>
{
public:
	// 
	static void ComputeEyeAdaptationParamsValue(const FViewInfo& View, FVector4 Out[3]);
	// computes the ExposureScale (useful if eyeadaptation is locked)
	static float ComputeExposureScaleValue(const FViewInfo& View);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};
