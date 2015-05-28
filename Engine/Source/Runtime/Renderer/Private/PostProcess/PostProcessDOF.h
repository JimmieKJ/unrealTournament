// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDOF.h: Post process Depth of Field implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// down sample and setup DOF input
// ePId_Input0: SceneColor
// ePId_Input1: SceneDepth
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDOFSetup : public TRenderingCompositePassBase<2, 2>
{
public:

	FRCPassPostProcessDOFSetup(bool bInNearBlurEnabled) : bNearBlurEnabled ( bInNearBlurEnabled ) {}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	bool bNearBlurEnabled;
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Full res scene color
// ePId_Input1: output 0 from the DOFSetup (possibly further blurred)
// ePId_Input2: output 1 from the DOFSetup (possibly further blurred)
class FRCPassPostProcessDOFRecombine : public TRenderingCompositePassBase<3, 1>
{
public:

	FRCPassPostProcessDOFRecombine(bool bInNearBlurEnabled) : bNearBlurEnabled ( bInNearBlurEnabled ) {}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	bool bNearBlurEnabled;
};


// down sample and setup DOF input
// ePId_Input0: SceneColor
// ePId_Input1: SceneDepth
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessCircleDOFSetup : public TRenderingCompositePassBase<2, 2>
{
public:

	FRCPassPostProcessCircleDOFSetup(bool bInNearBlurEnabled) : bNearBlurEnabled ( bInNearBlurEnabled ) {}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	bool bNearBlurEnabled;
};


// ePId_Input0: SceneColor
class FRCPassPostProcessCircleDOFDilate : public TRenderingCompositePassBase<1, 1>
{
public:

	FRCPassPostProcessCircleDOFDilate(void) {}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};




// ePId_Input0: setup results for far
// ePId_Input1: setup results for near
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessCircleDOF : public TRenderingCompositePassBase<2, 2>
{
public:

	FRCPassPostProcessCircleDOF(bool bInNearBlurEnabled) : bNearBlurEnabled ( bInNearBlurEnabled ) {}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	bool bNearBlurEnabled;
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
// ePId_Input0: Full res scene color
// ePId_Input1: output 0 from the DOFSetup (possibly further blurred)
// ePId_Input2: output 1 from the DOFSetup (possibly further blurred)
class FRCPassPostProcessCircleDOFRecombine : public TRenderingCompositePassBase<3, 1>
{
public:

	FRCPassPostProcessCircleDOFRecombine(bool bInNearBlurEnabled) : bNearBlurEnabled ( bInNearBlurEnabled ) {}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:

	bool bNearBlurEnabled;
};
