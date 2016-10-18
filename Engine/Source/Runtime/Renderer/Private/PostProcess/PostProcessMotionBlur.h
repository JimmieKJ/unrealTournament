// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMotionBlur.h: Post process MotionBlur implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// Find max velocity per 16x16 tile
// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessVelocityFlatten : public TRenderingCompositePassBase<2, 2>
{
public:
	FRCPassPostProcessVelocityFlatten();

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	// -------------------------------------------

	static FIntPoint ComputeThreadGroupCount(FIntPoint PixelExtent);
};


// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessVelocityScatter : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessVelocityGather : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};


// ePId_Input0: Full Res Scene Color
// ePId_Input1: Full Res Scene Depth
// ePId_Input2: Velocity
// ePId_Input3: Max tile velocity
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessMotionBlur : public TRenderingCompositePassBase<4, 1>
{
public:
	// @param InQuality 0xffffffff to visualize, 0:off(no shader is used), 1:low, 2:medium, 3:high, 4:very high
	FRCPassPostProcessMotionBlur( uint32 InQuality, int32 InPass )
		: Quality(InQuality)
		, Pass(InPass)
	{
		// internal error
		check(Quality >= 1 && Quality <= 4);
	}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	// 1:low, 2:medium, 3:high, 4: very high
	uint32	Quality;
	int32	Pass;
};


// ePId_Input0: Full Res Scene Color
// ePId_Input1: Full Res Scene Depth
// ePId_Input2: Full Res velocity input
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessVisualizeMotionBlur : public TRenderingCompositePassBase<3, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};