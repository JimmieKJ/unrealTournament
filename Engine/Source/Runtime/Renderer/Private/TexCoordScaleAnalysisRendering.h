// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TexCoordScaleAnalysisRendering.h: Declarations used for the viewmode.
=============================================================================*/

#pragma once

/**
* Pixel shader that renders texcoord scales.
* The shader is only compiled with the local vertex factory to prevent multiple compilation.
* Nothing from the factory is actually used, but the shader must still derive from FMeshMaterialShader.
* This is required to call FMeshMaterialShader::SetMesh and bind primitive related data.
*/
class FTexCoordScaleAnalysisPS : public FMeshMaterialShader, public IDebugViewModePSInterface
{
	DECLARE_SHADER_TYPE(FTexCoordScaleAnalysisPS,MeshMaterial);

public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// Debug viewmode pixel shaders have predefined interpolants, so we only need to compile it for one vertex factory.
		return Material->GetFriendlyName().Contains(TEXT("FDebugViewModeMaterialProxy")) &&  AllowDebugViewModeShader(Platform) && VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find));
	}

	FTexCoordScaleAnalysisPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		AccuracyColorsParameter.Bind(Initializer.ParameterMap,TEXT("AccuracyColors"));
		TextureAnalysisIndexParameter.Bind(Initializer.ParameterMap,TEXT("TextureAnalysisIndex"));
		CPUScaleParameter.Bind(Initializer.ParameterMap,TEXT("CPUScale"));
	}

	FTexCoordScaleAnalysisPS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << AccuracyColorsParameter;
		Ar << TextureAnalysisIndexParameter;
		Ar << CPUScaleParameter;
		return bShaderHasOutdatedParameters;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
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
	FShaderParameter CPUScaleParameter;
};
