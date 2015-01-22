// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AtmosphereTextureParameters.h: Shader base classes
=============================================================================*/

#pragma once

/** Shader parameters needed for atmosphere passes. */
class FAtmosphereShaderTextureParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap);

	template< typename ShaderRHIParamRef >
	void Set( FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FSceneView& View ) const;

	friend FArchive& operator<<(FArchive& Ar,FAtmosphereShaderTextureParameters& P);

private:
	FShaderResourceParameter TransmittanceTexture;
	FShaderResourceParameter TransmittanceTextureSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceTextureSampler;
	FShaderResourceParameter InscatterTexture;
	FShaderResourceParameter InscatterTextureSampler;
};