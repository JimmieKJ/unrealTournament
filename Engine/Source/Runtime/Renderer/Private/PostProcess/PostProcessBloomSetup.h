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
	virtual void Process(FRenderingCompositePassContext& Context);
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;
	virtual void Release() override { delete this; }
};
