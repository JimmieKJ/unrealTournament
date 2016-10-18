// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.h: Post process MotionBlur implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// ePId_Input0: Reflections (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessSSRTemporalAA : public TRenderingCompositePassBase<4, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// ePId_Input0: Half Res DOF input (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDOFTemporalAA : public TRenderingCompositePassBase<4, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// ePId_Input0: Half Res DOF input (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDOFTemporalAANear : public TRenderingCompositePassBase<4, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};


// ePId_Input0: Half Res light shaft input (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessLightShaftTemporalAA : public TRenderingCompositePassBase<3, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

// ePId_Input0: Full Res Scene color (point)
// ePId_Input1: Previous frame's output (bilinear)
// ePId_Input2: Previous frame's output (point)
// ePId_Input3: Velocity (point)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessTemporalAA : public TRenderingCompositePassBase<4, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};
