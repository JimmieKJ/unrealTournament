// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CompositionLighting.h: The center for all deferred lighting activities.
=============================================================================*/

#pragma once

#include "PostProcess/RenderingCompositionGraph.h"

/**
 * The center for all screen space processing activities (e.g. G-buffer manipulation, lighting).
 */
class FCompositionLighting
{
public:
	void ProcessBeforeBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View);

	void ProcessAfterBasePass(FRHICommandListImmediate& RHICmdList,  FViewInfo& View);

	// only call if LPV is enabled
	void ProcessLpvIndirect(FRHICommandListImmediate& RHICmdList, FViewInfo& View);

	void ProcessAfterLighting(FRHICommandListImmediate& RHICmdList, FViewInfo& View);
};

/** The global used for deferred lighting. */
extern FCompositionLighting GCompositionLighting;
