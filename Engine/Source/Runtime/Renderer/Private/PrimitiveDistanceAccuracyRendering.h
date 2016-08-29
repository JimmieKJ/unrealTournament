// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PrimitiveDistanceAccuracyRendering.h: Declarations used for the viewmode.
=============================================================================*/

#pragma once

/**
* Pixel shader that renders texture streamer wanted mips accuracy.
*/
class FPrimitiveDistanceAccuracyPS : public FGlobalShader, public IDebugViewModePSInterface
{
	DECLARE_SHADER_TYPE(FPrimitiveDistanceAccuracyPS,Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return AllowDebugViewPS(DVSM_PrimitiveDistanceAccuracy, Platform);
	}

	FPrimitiveDistanceAccuracyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		AccuracyColorsParameter.Bind(Initializer.ParameterMap,TEXT("AccuracyColors"));
		CPULogDistanceParameter.Bind(Initializer.ParameterMap, TEXT("CPULogDistance"));
		PrimitiveAlphaParameter.Bind(Initializer.ParameterMap, TEXT("PrimitiveAlpha"));
	}

	FPrimitiveDistanceAccuracyPS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AccuracyColorsParameter;
		Ar << CPULogDistanceParameter;
		Ar << PrimitiveAlphaParameter;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("UNDEFINED_ACCURACY"), UndefinedStreamingAccuracyIntensity);
	}

	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		const FShader* OriginalVS, 
		const FShader* OriginalPS, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View
		) override;

	virtual void SetMesh(
		FRHICommandList& RHICmdList, 
		const FVertexFactory* VertexFactory,
		const FSceneView& View,
		const FPrimitiveSceneProxy* Proxy,
		int32 VisualizeLODIndex,
		const FMeshBatchElement& BatchElement, 
		const FMeshDrawingRenderState& DrawRenderState
		) override;

	virtual void SetMesh(FRHICommandList& RHICmdList, const FSceneView& View) override;

	virtual FShader* GetShader() override { return static_cast<FShader*>(this); }

private:

	FShaderParameter AccuracyColorsParameter;
	FShaderParameter CPULogDistanceParameter;
	FShaderParameter PrimitiveAlphaParameter;
};
