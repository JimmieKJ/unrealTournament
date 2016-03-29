// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SystemTextures.h: System textures definitions.
=============================================================================*/

#pragma once

#include "ShaderParameters.h"
#include "PostProcess/RenderTargetPool.h"

/**
 * Encapsulates the system textures used for scene rendering.
 */
class FSystemTextures : public FRenderResource
{
public:
	FSystemTextures()
	: FRenderResource()
	, FeatureLevelInitializedTo(ERHIFeatureLevel::ES2)
	, bTexturesInitialized(false)
	{}

	/**
	 * Initialize/allocate textures if not already.
	 */
	void InitializeTextures(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type InFeatureLevel);

	// FRenderResource interface.
	/**
	 * Release textures when device is lost/destroyed.
	 */
	virtual void ReleaseDynamicRHI();

	// -----------

	// float4(1,1,1,1) can be used in case a light is not shadow casting
	TRefCountPtr<IPooledRenderTarget> WhiteDummy;
	// float4(0,0,0,0) can be used in additive postprocessing to avoid a shader combination
	TRefCountPtr<IPooledRenderTarget> BlackDummy;
	// float4(0,0,0,1)
	TRefCountPtr<IPooledRenderTarget> BlackAlphaOneDummy;
	// used by the material expression Noise
	TRefCountPtr<IPooledRenderTarget> PerlinNoiseGradient;
	// used by the material expression Noise (faster version, should replace old version), todo: move out of SceneRenderTargets
	TRefCountPtr<IPooledRenderTarget> PerlinNoise3D;
	/** SSAO randomization */
	TRefCountPtr<IPooledRenderTarget> SSAORandomization;
	/** Preintegrated GF for single sample IBL */
	TRefCountPtr<IPooledRenderTarget> PreintegratedGF;
	/** Texture that holds a single value containing the maximum depth that can be stored as FP16. */
	TRefCountPtr<IPooledRenderTarget> MaxFP16Depth;
	/** Depth texture that holds a single depth value */
	TRefCountPtr<IPooledRenderTarget> DepthDummy;

	/** Maximum feature level that the textures have been initialized up to */
	ERHIFeatureLevel::Type FeatureLevelInitializedTo;
	bool bTexturesInitialized;
};

/** The global system textures used for scene rendering. */
extern TGlobalResource<FSystemTextures> GSystemTextures;
