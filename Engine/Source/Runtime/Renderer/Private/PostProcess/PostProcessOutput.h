// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessOutput : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessOutput(TRefCountPtr<IPooledRenderTarget>* InExternalRenderTarget);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

protected:

	TRefCountPtr<IPooledRenderTarget>* ExternalRenderTarget;
};
