// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeComplexity.h: PP pass used when visualizing complexity, maps scene color complexity value to colors
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

class FRCPassPostProcessVisualizeComplexity : public TRenderingCompositePassBase<1, 1>
{
public:

	FRCPassPostProcessVisualizeComplexity(const TArray<FLinearColor>& InColors) :
		Colors(InColors)
	{}

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private: 

	TArray<FLinearColor> Colors;
};
