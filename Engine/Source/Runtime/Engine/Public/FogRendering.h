// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FogRendering.h: 
=============================================================================*/

#pragma once

/** Parameters needed to render exponential height fog. */
class FExponentialHeightFogShaderParameters
{
public:

	/** Binds the parameters. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** 
	 * Sets exponential height fog shader parameters.
	 */
	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, ShaderRHIParamRef Shader, const FSceneView* View) const
	{
		const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(View);
		SetShaderValue(RHICmdList, Shader, ExponentialFogParameters, ViewInfo->ExponentialFogParameters);
		SetShaderValue(RHICmdList, Shader, ExponentialFogColorParameter, FVector4(ViewInfo->ExponentialFogColor, 1.0f - ViewInfo->FogMaxOpacity));
		SetShaderValue(RHICmdList, Shader, InscatteringLightDirection, FVector4(ViewInfo->InscatteringLightDirection, ViewInfo->bUseDirectionalInscattering ? 1 : 0));
		SetShaderValue(RHICmdList, Shader, DirectionalInscatteringColor, FVector4(FVector(ViewInfo->DirectionalInscatteringColor), ViewInfo->DirectionalInscatteringExponent));
		SetShaderValue(RHICmdList, Shader, DirectionalInscatteringStartDistance, ViewInfo->DirectionalInscatteringStartDistance);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FExponentialHeightFogShaderParameters& P);

	FShaderParameter ExponentialFogParameters;
	FShaderParameter ExponentialFogColorParameter;
	FShaderParameter InscatteringLightDirection;
	FShaderParameter DirectionalInscatteringColor;
	FShaderParameter DirectionalInscatteringStartDistance;
};

/** Encapsulates parameters needed to calculate height fog in a vertex shader. */
class FHeightFogShaderParameters
{
public:

	/** Binds the parameters. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** 
	 * Sets height fog shader parameters.
	 */
	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, ShaderRHIParamRef Shader, const FSceneView* View) const
	{
		// Set the fog constants.
		ExponentialParameters.Set(RHICmdList, Shader, View);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FHeightFogShaderParameters& P);

private:

	FExponentialHeightFogShaderParameters ExponentialParameters;
};

extern bool ShouldRenderFog(const FSceneViewFamily& Family);