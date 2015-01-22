// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUBenchmark.h: GPUBenchmark to compute performance index to set video options automatically
=============================================================================*/

#pragma once

// @param >0, WorkScale 10 for normal precision and runtime of less than a second
// @param bDebugOut has no effect in shipping
void RendererGPUBenchmark(FRHICommandListImmediate& RHICmdList, FSynthBenchmarkResults& InOut, const FSceneView& View, float WorkScale, bool bDebugOut = false);
