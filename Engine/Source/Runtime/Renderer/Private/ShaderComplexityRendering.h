// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.h: Declarations used for the shader complexity viewmode.
=============================================================================*/

#pragma once

/**
* Pixel shader that renders normalized shader complexity.
*/
class FShaderComplexityAccumulatePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShaderComplexityAccumulatePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FShaderComplexityAccumulatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		NormalizedComplexity.Bind(Initializer.ParameterMap,TEXT("NormalizedComplexity"));
	}

	FShaderComplexityAccumulatePS() {}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		uint32 NumVertexInstructions, 
		uint32 NumPixelInstructions,
		ERHIFeatureLevel::Type InFeatureLevel);

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << NormalizedComplexity;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter NormalizedComplexity;
};


