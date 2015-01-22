// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMotionBlur.h: Post process MotionBlur implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// down sample and prepare for soft masked blurring
// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// MRT0: half res velocity, MRT1: half res scene color with depth in alpha
// ePId_Input0: optional Full Res velocity input
// ePId_Input1: Full Res Scene Color
// ePId_Input2: Full Res Scene Depth
class FRCPassPostProcessMotionBlurSetup : public TRenderingCompositePassBase<3, 2>
{
public:
	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;
};


// input half res scene with depth, velocity, output half res motion blurred version
// ePId_Input0: Half res SceneColor with depth in alpha
// ePId_Input1: optional quarter res blurred screen space velocity
// ePId_Input2: optional half res screen space masked velocity
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessMotionBlur : public TRenderingCompositePassBase<3, 1>
{
public:
	// @param InQuality 0xffffffff to visualize, 0:off(no shader is used), 1:low, 2:medium, 3:high, 4:very high
	FRCPassPostProcessMotionBlur(uint32 InQuality);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;

	// 1:low, 2:medium, 3:high, 4: very high
	uint32 Quality;
};


// ePId_Input0: Full Res Scene color
// ePId_Input1: output from FRCPassPostProcessMotionBlur
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessMotionBlurRecombine : public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;
};


// ePId_Input0: Half res SceneColor with depth in alpha
// ePId_Input1: optional quarter res blurred screen space velocity
// ePId_Input2: optional half res screen space masked velocity
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessVisualizeMotionBlur : public TRenderingCompositePassBase<3, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;
};
