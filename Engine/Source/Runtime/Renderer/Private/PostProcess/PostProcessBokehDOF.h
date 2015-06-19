// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBokehDOF.h: Post processing lens blur implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

struct FDepthOfFieldStats
{
	FDepthOfFieldStats()
		: bNear(false)
		, bFar(false)
	{
	}

	bool bNear;
	bool bFar;
};


// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Color input
class FRCPassPostProcessVisualizeDOF : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessVisualizeDOF(const FDepthOfFieldStats& InDepthOfFieldStats)
		: DepthOfFieldStats(InDepthOfFieldStats)
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	FDepthOfFieldStats DepthOfFieldStats;
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Color input
// ePId_Input1: Depth input
class FRCPassPostProcessBokehDOFSetup : public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Half res scene with depth in alpha
// ePId_Input1: SceneColor for high quality input (experimental)
// ePId_Input2: SceneDepth for high quality input (experimental)
class FRCPassPostProcessBokehDOF : public TRenderingCompositePassBase<3, 1>
{
public:
	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	static void ComputeDepthOfFieldParams(const FRenderingCompositePassContext& Context, FVector4 Out[2]);

private:

	template <uint32 HighQuality, uint32 IndexStyle>
	void SetShaderTempl(const FRenderingCompositePassContext& Context, FIntPoint LeftTop, FIntPoint TileCount, uint32 TileSize, float PixelKernelSize);

	// border between front and back layer as we don't use viewports (only possible with GS)
	const static uint32 SafetyBorder = 40;
};
