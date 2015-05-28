// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBusyWait.h: Post processing busy wait implementation. For Debugging GPU timing.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessBusyWait : public TRenderingCompositePassBase<0, 1>
{
public:
	static bool IsPassRequired();

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
};

