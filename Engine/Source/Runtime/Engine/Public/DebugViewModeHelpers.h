// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Definition and helpers for debug view modes
 */

#pragma once

/** 
 * Enumeration for different Quad Overdraw visualization mode.
 */
enum EDebugViewShaderMode
{
	DVSM_None,						// No debug view.
	DVSM_ShaderComplexity,			// Default shader complexity viewmode
	DVSM_ShaderComplexityContainedQuadOverhead,	// Show shader complexity with quad overdraw scaling the PS instruction count.
	DVSM_ShaderComplexityBleedingQuadOverhead,	// Show shader complexity with quad overdraw bleeding the PS instruction count over the quad.
	DVSM_QuadComplexity,			// Show quad overdraw only.
	DVSM_PrimitiveDistanceAccuracy,	// Visualize the accuracy of the CPU primitive distance when compared with the GPU value.
	DVSM_MeshTexCoordSizeAccuracy,	// Visualize the accuracy of the CPU mesh texture coordinate size when compared to the GPU value.
	DVSM_MaterialTexCoordScalesAccuracy, // Visualize the accuracy of CPU material texture coordinate scales when compared to the GPU values.
	DVSM_MaterialTexCoordScalesAnalysis, // Outputs the material texture coordinate scales.
	DVSM_MAX
};

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/** Returns true if the specified shadermode is available for the given shader platform. Called for shader compilation shader compilation. */
extern ENGINE_API bool AllowDebugViewPS(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform);
/** Returns true if the vertex shader (and potential hull and domain) should be compiled on the given platform. */
extern ENGINE_API bool AllowDebugViewVSDSHS(EShaderPlatform Platform);
/** Returns true if the shader mode can be enabled. This is only for UI elements as no shader platform is actually passed. */
extern ENGINE_API bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode);
#else
FORCEINLINE bool AllowDebugViewPS(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform) { return false; }
FORCEINLINE bool AllowDebugViewVSDSHS(EShaderPlatform Platform)  { return false; }
FORCEINLINE bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode) { return false; }
#endif



