// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDeferredDecals.h: Deferred Decals implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"
#include "DecalRenderingShared.h"

// ePId_Input0: SceneColor (not needed for DBuffer decals)
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessDeferredDecals : public TRenderingCompositePassBase<1, 1>
{
public:
	// One instance for each render stage
	FRCPassPostProcessDeferredDecals(EDecalRenderStage InDecalRenderStage);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	// see EDecalRenderStage
	EDecalRenderStage CurrentStage;
};

bool IsDBufferEnabled();
