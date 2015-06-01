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
	// Panini configuration
	//  NOTE: more details in Common.usf
	struct PaniniParams
	{
		// 0=none..1=full, must be >= 0
		float D;

		// Panini hard vertical compression lerp (0=no vertical compresion, 1=hard compression)
		float S;

		// Panini screen fit factor (lerp between vertical and horizontal) 
		float ScreenFit;

		PaniniParams()
			: D(0.0f)
			, S(0.0f)
			, ScreenFit(1.0f)
		{}

		static const PaniniParams Default;
	};

	// constructor
	// @param InUpscaleQuality - value denoting Upscale method to use:
	//				0: Nearest
	//				1: Bilinear
	//				2: 4 tap Bilinear (with radius adjustment)
	//				3: Directional blur with unsharp mask upsample.
	// @param InPaniniConfig - the panini configuration parameter
	FRCPassPostProcessUpscale(uint32 InUpscaleQuality, const PaniniParams& InPaniniConfig = PaniniParams::Default);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
private:
	// @param InCylinderDistortion 0=none..1=full in percent, must be in that range
	template <uint32 Quality, uint32 bTesselatedQuad> static FShader* SetShader(const FRenderingCompositePassContext& Context, const PaniniParams& PaniniConfig);

	// 0: Nearest, 1: Bilinear, 2: 4 tap Bilinear (with radius adjustment), 3: Directional blur with unsharp mask upsample.
	uint32 UpscaleQuality;

	// Panini projection's parameter
	PaniniParams PaniniConfig;
};

