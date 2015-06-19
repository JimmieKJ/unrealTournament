// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessSubsurface.h: Screenspace subsurface scattering implementation
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// ePId_Input0: SceneColor
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// uses some GBuffer attributes
// alpha is unused
class FRCPassPostProcessSubsurfaceVisualize : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessSubsurfaceVisualize();

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
};

// ePId_Input0: SceneColor
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// uses some GBuffer attributes
class FRCPassPostProcessSubsurfaceExtractSpecular : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
};

// ePId_Input0: SceneColor
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// uses some GBuffer attributes
// alpha is unused
class FRCPassPostProcessSubsurfaceSetup : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessSubsurfaceSetup(FViewInfo& View);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }

	FIntRect ViewRect;
};


// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor (horizontal blur) or the pass before (vertical blur)
// ePId_Input1: optional Setup pass (only for InDirection==1)
// modifies SceneColor, uses some GBuffer attributes
class FRCPassPostProcessSubsurface : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	// @param InDirection 0:horizontal/1:vertical
	FRCPassPostProcessSubsurface(uint32 InDirection);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// 0:horizontal/1:vertical
	uint32 Direction;
};


// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: output from FRCPassPostProcessSubsurface
// ePId_Input1: SceneColor before Screen Space Subsurface input
// modifies SceneColor, uses some GBuffer attributes
class FRCPassPostProcessSubsurfaceRecombine : public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};
