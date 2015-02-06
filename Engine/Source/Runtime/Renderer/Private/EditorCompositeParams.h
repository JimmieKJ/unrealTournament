// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
EditorCompositeParams.h: Manages shader parameters required for editor's composited primitives.
=============================================================================*/
#pragma once

class FEditorCompositingParameters
{
public:
	FEditorCompositingParameters();

	void Bind(const FShaderParameterMap& ParameterMap);
	void SetParameters(
		FRHICommandList& RHICmdList,
		const FMaterial& MaterialResource,
		const FSceneView* View,
		bool bEnableEditorPrimitveDepthTest,
		const FPixelShaderRHIParamRef ShaderRHI);

	/** Serializer. */
	friend FArchive& operator << (FArchive& Ar, FEditorCompositingParameters& P)
	{
		Ar << P.EditorCompositeDepthTestParameter;
		Ar << P.MSAASampleCount;
		Ar << P.FilteredSceneDepthTexture;
		Ar << P.FilteredSceneDepthTextureSampler;
		return Ar;
	}

private:
	/** Parameter for whether or not to depth test in the pixel shader for editor primitives */
	FShaderParameter EditorCompositeDepthTestParameter;
	FShaderParameter MSAASampleCount;
	/** Parameter for reading filtered depth values */
	FShaderResourceParameter FilteredSceneDepthTexture;
	FShaderResourceParameter FilteredSceneDepthTextureSampler;
};