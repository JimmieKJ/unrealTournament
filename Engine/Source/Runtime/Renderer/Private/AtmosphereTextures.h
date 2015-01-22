// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AtmosphereTextures.h: System textures definitions.
=============================================================================*/

#pragma once

#include "ShaderParameters.h"
#include "RenderTargetPool.h"

/**
 * Encapsulates the system textures used for scene rendering.
 */
class FAtmosphereTextures : public FRenderResource
{
public:
	FAtmosphereTextures(const FAtmospherePrecomputeParameters* InPrecomputeParams)
		: PrecomputeParams(InPrecomputeParams)
	{
		InitResource();
	}

	~FAtmosphereTextures()
	{
		ReleaseResource();
	}

	const struct FAtmospherePrecomputeParameters* PrecomputeParams;

	// FRenderResource interface.
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	// Final result
	TRefCountPtr<IPooledRenderTarget> AtmosphereTransmittance;
	TRefCountPtr<IPooledRenderTarget> AtmosphereIrradiance;
	TRefCountPtr<IPooledRenderTarget> AtmosphereInscatter;
	// Intermediate result
	TRefCountPtr<IPooledRenderTarget> AtmosphereDeltaE;
	TRefCountPtr<IPooledRenderTarget> AtmosphereDeltaSR;
	TRefCountPtr<IPooledRenderTarget> AtmosphereDeltaSM;
	TRefCountPtr<IPooledRenderTarget> AtmosphereDeltaJ;
};

