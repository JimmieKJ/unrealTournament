// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessUpscale.h: Post processing Upscale implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor (bilinear)
// ePId_Input1: SceneColor (point)
class FRCPassPostProcessUpscale : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	// @param InUpscaleQuality - value denoting Upscale method to use:
	//				0: Nearest
	//				1: Bilinear
	//				2: 4 tap Bilinear (with radius adjustment)
	//				3: Directional blur with unsharp mask upsample.
	// @param InCylinderDistortion 0=none..1=full in percent, can be outside of that range
	FRCPassPostProcessUpscale(uint32 InUpscaleQuality, float InCylinderDistortion);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
private:
	// @param InCylinderDistortion 0=none..1=full in percent, must be in that range
	template <uint32 Quality, uint32 bTesselatedQuad> static FShader* SetShader(const FRenderingCompositePassContext& Context, float InCylinderDistortion = 0.0f);

	// 0: Nearest, 1: Bilinear, 2: 4 tap Bilinear (with radius adjustment), 3: Directional blur with unsharp mask upsample.
	uint32 UpscaleQuality;
	// 0=none..1=full, must be in that range
	float CylinderDistortion;
};

