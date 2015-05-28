// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessHistogramReduce.h: Post processing histogram reduce implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"
#include "PostProcessHistogram.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessHistogramReduce : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	
	static const uint32 ThreadGroupSizeX = FRCPassPostProcessHistogram::HistogramTexelCount;
	static const uint32 ThreadGroupSizeY = 4;

private:

	static uint32 ComputeLoopSize(FIntPoint PixelExtent);
};
