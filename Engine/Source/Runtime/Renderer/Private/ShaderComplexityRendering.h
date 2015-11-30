// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.h: Declarations used for the shader complexity viewmode.
=============================================================================*/

#pragma once

// Used to know wheter QuadOverdraw should be enabled in shaders.
FORCEINLINE bool AllowQuadOverdraw(EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5;
}

// Used to know whether QuadOverdraw should be used at runtime.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
FORCEINLINE bool AllowRuntimeQuadOverdraw(ERHIFeatureLevel::Type InFeatureLevel)
{
	return AllowQuadOverdraw(GetFeatureLevelShaderPlatform(InFeatureLevel));
}
#else
FORCEINLINE bool AllowRuntimeQuadOverdraw(ERHIFeatureLevel::Type InFeatureLevel)
{
	return false;
}
#endif

/**
 * Vertex shader for quad overdraw. Required because overdraw shaders need to have SV_Position as first PS interpolant.
 */
class FQuadOverdrawVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FQuadOverdrawVS,MeshMaterial);
protected:

	FQuadOverdrawVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer) {}
	FQuadOverdrawVS() {}

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return AllowQuadOverdraw(Platform) && (Material->IsSpecialEngineMaterial() || Material->HasVertexPositionOffsetConnected() || Material->GetTessellationMode() != MTM_NoTessellation);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FMaterial& MaterialResource,const FSceneView& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,MaterialResource,View,ESceneRenderTargetsMode::DontSet);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement, float DitheredLODTransitionValue)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DitheredLODTransitionValue);
	}
};


/**
 * Hull shader for quad overdraw. Required because overdraw shaders need to have SV_Position as first PS interpolant.
 */
class FQuadOverdrawHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FQuadOverdrawHS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType) && FQuadOverdrawVS::ShouldCache(Platform,Material,VertexFactoryType);
	}

	FQuadOverdrawHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer): FBaseHS(Initializer) {}
	FQuadOverdrawHS() {}
};

/**
 * Domain shader for quad overdraw. Required because overdraw shaders need to have SV_Position as first PS interpolant.
 */
class FQuadOverdrawDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FQuadOverdrawDS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType) && FQuadOverdrawVS::ShouldCache(Platform,Material,VertexFactoryType);		
	}

	FQuadOverdrawDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer): FBaseDS(Initializer) {}
	FQuadOverdrawDS() {}
};

/**
* Pixel shader that renders normalized shader complexity.
*/
class FShaderComplexityAccumulatePS : public FGlobalShader
{
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FShaderComplexityAccumulatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		NormalizedComplexity.Bind(Initializer.ParameterMap,TEXT("NormalizedComplexity"));
		ShowQuadOverdraw.Bind(Initializer.ParameterMap,TEXT("bShowQuadOverdraw"));
		QuadBufferUAV.Bind(Initializer.ParameterMap,TEXT("RWQuadBuffer"));
	}

	FShaderComplexityAccumulatePS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << NormalizedComplexity;
		Ar << ShowQuadOverdraw;
		Ar << QuadBufferUAV;
		return bShaderHasOutdatedParameters;
	}

	// Static function to avoid accessing template classes
	static void SetParameters(
		TShaderMap<FGlobalShaderType>* ShaderMap,
		FRHICommandList& RHICmdList,
		uint32 NumVertexInstructions, 
		uint32 NumPixelInstructions,
		EQuadOverdrawMode QuadOverdrawMode,
		ERHIFeatureLevel::Type InFeatureLevel);

	static FShader* GetPixelShader(TShaderMap<FGlobalShaderType>* ShaderMap, EQuadOverdrawMode QuadOverdrawMode);

protected:

	FShaderParameter NormalizedComplexity;
	FShaderParameter ShowQuadOverdraw;
	FShaderResourceParameter QuadBufferUAV;

	void SetParameters(
		FRHICommandList& RHICmdList, 
		uint32 NumVertexInstructions, 
		uint32 NumPixelInstructions,
		EQuadOverdrawMode QuadOverdrawMode,
		ERHIFeatureLevel::Type InFeatureLevel);
};

template <bool bQuadComplexity>
class TComplexityAccumulatePS : public FShaderComplexityAccumulatePS
{
	DECLARE_SHADER_TYPE(TComplexityAccumulatePS,Global);
public:


	TComplexityAccumulatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShaderComplexityAccumulatePS(Initializer)
	{
	}

	TComplexityAccumulatePS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShaderComplexityAccumulatePS::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("OUTPUT_QUAD_OVERDRAW"), AllowQuadOverdraw(Platform) ? TEXT("1") : TEXT("0"));
	}

	static FORCEINLINE void SetParameters(
		TShaderMap<FGlobalShaderType>* ShaderMap,
		FRHICommandList& RHICmdList, 
		uint32 NumVertexInstructions, 
		uint32 NumPixelInstructions,
		EQuadOverdrawMode QuadOverdrawMode,
		ERHIFeatureLevel::Type InFeatureLevel);

	static FORCEINLINE FShader* GetPixelShader(TShaderMap<FGlobalShaderType>* ShaderMap);
};

typedef TComplexityAccumulatePS<false> TShaderComplexityAccumulatePS;
typedef TComplexityAccumulatePS<true> TQuadComplexityAccumulatePS;

void PatchBoundShaderStateInputForQuadOverdraw(
		FBoundShaderStateInput& BoundShaderStateInput,
		const FMaterial* MaterialResource,
		const FVertexFactory* VertexFactory,
		ERHIFeatureLevel::Type FeatureLevel, 
		EQuadOverdrawMode QuadOverdrawMode
		);

void SetNonPSParametersForQuadOverdraw(
	FRHICommandList& RHICmdList, 
	const FMaterialRenderProxy* MaterialRenderProxy, 
	const FMaterial* MaterialResource, 
	const FSceneView& View,
	const FVertexFactory* VertexFactory,
	bool bHasHullAndDomainShader
	);

void SetMeshForQuadOverdraw(
	FRHICommandList& RHICmdList, 
	const FMaterial* MaterialResource, 
	const FSceneView& View,
	const FVertexFactory* VertexFactory,
	bool bHasHullAndDomainShader,
	const FPrimitiveSceneProxy* Proxy,
	const FMeshBatchElement& BatchElement, 
	float DitheredLODTransitionValue
	);
