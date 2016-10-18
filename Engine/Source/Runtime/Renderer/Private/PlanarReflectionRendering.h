// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PlanarReflectionRendering.h: shared planar reflection rendering declarations.
=============================================================================*/

#pragma once

/** Parameters needed for planar reflections, shared by multiple shaders. */
class FPlanarReflectionParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ReflectionPlane.Bind(ParameterMap, TEXT("ReflectionPlane"));
		PlanarReflectionOrigin.Bind(ParameterMap, TEXT("PlanarReflectionOrigin"));
		PlanarReflectionXAxis.Bind(ParameterMap, TEXT("PlanarReflectionXAxis"));
		PlanarReflectionYAxis.Bind(ParameterMap, TEXT("PlanarReflectionYAxis"));
		InverseTransposeMirrorMatrix.Bind(ParameterMap, TEXT("InverseTransposeMirrorMatrix"));
		PlanarReflectionParameters.Bind(ParameterMap, TEXT("PlanarReflectionParameters"));
		PlanarReflectionParameters2.Bind(ParameterMap, TEXT("PlanarReflectionParameters2"));
		ProjectionWithExtraFOV.Bind(ParameterMap, TEXT("ProjectionWithExtraFOV"));
		IsStereoParameter.Bind(ParameterMap, TEXT("bIsStereo"));
		PlanarReflectionTexture.Bind(ParameterMap, TEXT("PlanarReflectionTexture"));
		PlanarReflectionSampler.Bind(ParameterMap, TEXT("PlanarReflectionSampler"));
	}

	void SetParameters(FRHICommandList& RHICmdList, FPixelShaderRHIParamRef ShaderRHI, const class FPlanarReflectionSceneProxy* ReflectionSceneProxy);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar, FPlanarReflectionParameters& P)
	{
		Ar << P.ReflectionPlane;
		Ar << P.PlanarReflectionOrigin;
		Ar << P.PlanarReflectionXAxis;
		Ar << P.PlanarReflectionYAxis;
		Ar << P.InverseTransposeMirrorMatrix;
		Ar << P.PlanarReflectionParameters;
		Ar << P.PlanarReflectionParameters2;
		Ar << P.ProjectionWithExtraFOV;
		Ar << P.IsStereoParameter;
		Ar << P.PlanarReflectionTexture;
		Ar << P.PlanarReflectionSampler;
		return Ar;
	}

private:

	FShaderParameter ReflectionPlane;
	FShaderParameter PlanarReflectionOrigin;
	FShaderParameter PlanarReflectionXAxis;
	FShaderParameter PlanarReflectionYAxis;
	FShaderParameter InverseTransposeMirrorMatrix;
	FShaderParameter PlanarReflectionParameters;
	FShaderParameter PlanarReflectionParameters2;
	FShaderParameter ProjectionWithExtraFOV;
	FShaderParameter IsStereoParameter;
	FShaderResourceParameter PlanarReflectionTexture;
	FShaderResourceParameter PlanarReflectionSampler;
};
