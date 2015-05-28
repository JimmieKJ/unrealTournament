// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBokehDOFRecombine.h: Post processing lens blur implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Full res scene color
// ePId_Input1: optional output from the BokehDOF (two blurred images, for in front and behind the focal plane)
// ePId_Input2: optional SeparateTranslucency
class FRCPassPostProcessBokehDOFRecombine : public TRenderingCompositePassBase<3, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	template <uint32 Method>
	static void SetShader(const FRenderingCompositePassContext& Context);
};
