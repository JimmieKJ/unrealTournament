// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TexelFactorAccuracyRendering.h: Declarations used for the viewmode.
=============================================================================*/

#pragma once

/**
* Pixel shader that renders the accuracy of the texel factor.
*/
class FTexelFactorAccuracyPS : public FGlobalShader, public IDebugViewModePSInterface
{
	DECLARE_SHADER_TYPE(FTexelFactorAccuracyPS,Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return AllowDebugViewModeShader(Platform);
	}

	FTexelFactorAccuracyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		AccuracyColorsParameter.Bind(Initializer.ParameterMap,TEXT("AccuracyColors"));
		CPUTexelFactorParameter.Bind(Initializer.ParameterMap,TEXT("CPUTexelFactor"));
	}

	FTexelFactorAccuracyPS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AccuracyColorsParameter;
		Ar << CPUTexelFactorParameter;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
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
	FShaderParameter CPUTexelFactorParameter;
};
