// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MaterialTexCoordScalesRendering.h: Declarations used for the viewmode.
=============================================================================*/

#pragma once

/**
* Pixel shader that renders texcoord scales.
* The shader is only compiled with the local vertex factory to prevent multiple compilation.
* Nothing from the factory is actually used, but the shader must still derive from FMeshMaterialShader.
* This is required to call FMeshMaterialShader::SetMesh and bind primitive related data.
*/
class FMaterialTexCoordScalePS : public FMeshMaterialShader, public IDebugViewModePSInterface
{
	DECLARE_SHADER_TYPE(FMaterialTexCoordScalePS,MeshMaterial);

public:
	enum { MAX_NUM_TEXTURE_REGISTER = FMaterialTexCoordBuildInfo::MAX_NUM_TEXTURE_REGISTER };

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return AllowDebugViewPS(DVSM_MaterialTexCoordScalesAnalysis, Platform) && 
			Material->GetFriendlyName().Contains(TEXT("FDebugViewModeMaterialProxy")) && 
			VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find));
	}

	FMaterialTexCoordScalePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		AccuracyColorsParameter.Bind(Initializer.ParameterMap,TEXT("AccuracyColors"));
		TextureAnalysisIndexParameter.Bind(Initializer.ParameterMap,TEXT("TextureAnalysisIndex"));
		OneOverCPUTexCoordScalesParameter.Bind(Initializer.ParameterMap,TEXT("OneOverCPUTexCoordScales"));
		TexCoordIndicesParameter.Bind(Initializer.ParameterMap, TEXT("TexCoordIndices"));
		PrimitiveAlphaParameter.Bind(Initializer.ParameterMap, TEXT("PrimitiveAlpha"));
	}

	FMaterialTexCoordScalePS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << AccuracyColorsParameter;
		Ar << TextureAnalysisIndexParameter;
		Ar << OneOverCPUTexCoordScalesParameter;
		Ar << TexCoordIndicesParameter;
		Ar << PrimitiveAlphaParameter;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("UNDEFINED_ACCURACY"), UndefinedStreamingAccuracyIntensity);
		OutEnvironment.SetDefine(TEXT("MAX_NUM_TEX_COORD"), (uint32)FMaterialTexCoordBuildInfo::MAX_NUM_TEX_COORD);
		OutEnvironment.SetDefine(TEXT("INITIAL_GPU_SCALE"), (uint32)FMaterialTexCoordBuildInfo::INITIAL_GPU_SCALE);
		OutEnvironment.SetDefine(TEXT("TILE_RESOLUTION"), (uint32)FMaterialTexCoordBuildInfo::TILE_RESOLUTION);
		OutEnvironment.SetDefine(TEXT("MAX_NUM_TEXTURE_REGISTER"), (uint32)FMaterialTexCoordBuildInfo::MAX_NUM_TEXTURE_REGISTER);
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
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

	virtual void SetMesh(FRHICommandList& RHICmdList, const FSceneView& View) override { check(false); }

	virtual FShader* GetShader() override { return static_cast<FShader*>(this); }

private:

	FShaderParameter AccuracyColorsParameter;
	FShaderParameter TextureAnalysisIndexParameter;
	FShaderParameter OneOverCPUTexCoordScalesParameter;
	FShaderParameter TexCoordIndicesParameter;
	FShaderParameter PrimitiveAlphaParameter;
};
