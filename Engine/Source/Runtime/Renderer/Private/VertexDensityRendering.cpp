// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMapDensityRendering.cpp: Implementation for rendering lightmap density.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"

static int32 GVertexDensityTriangleSize = 16;
static FAutoConsoleVariableRef CVarVertexDensitySize(
	TEXT("r.VertexDensity.Size"),
	GVertexDensityTriangleSize,
	TEXT("Size of the density indicator.")
	);

class FVertexDensityVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVertexDensityVS, MeshMaterial);

public:
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialMayModifyMeshPosition())
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FVertexDensityVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		//LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
	}
	FVertexDensityVS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		//LightMapPolicyType::VertexParametersType::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}
};

class FVertexDensityGS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVertexDensityGS, MeshMaterial);

public:
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialMayModifyMeshPosition())
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && RHISupportsGeometryShaders(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FVertexDensityGS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		TriangleSize.Bind(Initializer.ParameterMap, TEXT("TriangleSize"), SPF_Mandatory);
	}
	FVertexDensityGS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << TriangleSize;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetGeometryShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, ESceneRenderTargetsMode::SetTextures);
		FVector4 TriangleSizeValue(GVertexDensityTriangleSize, GVertexDensityTriangleSize, 0, 0);
		SetShaderValue(RHICmdList, GetGeometryShader(), TriangleSize, TriangleSizeValue);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetGeometryShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}

	FShaderParameter TriangleSize;
};

class FVertexDensityPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVertexDensityPS, MeshMaterial);

public:
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialMayModifyMeshPosition())
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FVertexDensityPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		//LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
	}
	FVertexDensityPS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		//LightMapPolicyType::VertexParametersType::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}
};

class FVertexDensityDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:
	};

	FVertexDensityDrawingPolicy(
		const FViewInfo& View,
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		EBlendMode InBlendMode
		) :
		FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, *InMaterialRenderProxy->GetMaterial(View.GetFeatureLevel())),
		BlendMode(InBlendMode)
	{
		HullShader = nullptr;
		DomainShader = nullptr;

		const EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetTessellationMode();
		if (RHISupportsTessellation(View.GetShaderPlatform())
			&& InVertexFactory->GetType()->SupportsTessellationShaders()
			&& MaterialTessellationMode != MTM_NoTessellation)
		{
			//HullShader = MaterialResource->GetShader<TLightMapDensityHS<LightMapPolicyType> >(VertexFactory->GetType());
			//DomainShader = MaterialResource->GetShader<TLightMapDensityDS<LightMapPolicyType> >(VertexFactory->GetType());
		}

		VertexShader = MaterialResource->GetShader<FVertexDensityVS>(InVertexFactory->GetType());
		GeometryShader = MaterialResource->GetShader<FVertexDensityGS>(InVertexFactory->GetType());
		PixelShader = MaterialResource->GetShader<FVertexDensityPS>(InVertexFactory->GetType());
	}

	// FMeshDrawingPolicy interface.
	bool Matches(const FVertexDensityDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) &&
			VertexShader == Other.VertexShader &&
			GeometryShader == Other.GeometryShader &&
			PixelShader == Other.PixelShader &&
			HullShader == Other.HullShader &&
			DomainShader == Other.DomainShader;
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType) const
	{
		if (VertexFactory)
		{
			VertexFactory->Set(RHICmdList);
		}

		// Set the base pass shader parameters for the material.
		VertexShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		GeometryShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
		PixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);

		//if(HullShader && DomainShader)
		//{
		//	HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		//	DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		//}

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		bool bBackFace,
		const FMeshDrawingRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

		VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);

		//if(HullShader && DomainShader)
		//{
		//	HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		//	DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		//}

		GeometryShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);

		const auto FeatureLevel = View.GetFeatureLevel();
		PixelShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
		//PixelShader->SetMesh(RHICmdList, VertexFactory,PrimitiveSceneProxy, BatchElement, View, bBackFace, BuiltLightingAndSelectedFlags, LMResolutionScale, bTextureMapped);	
		FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,DrawRenderState,FMeshDrawingPolicy::ElementDataType(),PolicyContext);
	}

	/** 
	 * Create bound shader state using the vertex decl from the mesh draw policy
	 * as well as the shaders needed to draw the mesh
	 * @return new bound shader state object
	 */
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
	{
		return FBoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(), 
			VertexShader->GetVertexShader(),
			FHullShaderRHIRef(),//GETSAFERHISHADER_HULL(HullShader), 
			FDomainShaderRHIRef(),//GETSAFERHISHADER_DOMAIN(DomainShader),
			PixelShader->GetPixelShader(),
			GeometryShader->GetGeometryShader());
	}

	friend int32 CompareDrawingPolicy(const FVertexDensityDrawingPolicy& A,const FVertexDensityDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(GeometryShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		return 0;//CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

private:
	FVertexDensityVS* VertexShader;
	FVertexDensityGS* GeometryShader;
	FVertexDensityPS* PixelShader;
	struct FVertexDensityHS* HullShader;
	struct FVertexDensityDS* DomainShader;

	EBlendMode BlendMode;
};

class FVertexDensityDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = false };
	struct ContextType {};

	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bBackFace,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		)
	{
		const auto FeatureLevel = View.GetFeatureLevel();
		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
		const EBlendMode BlendMode = Material->GetBlendMode();

		const bool bMaterialMasked = Material->IsMasked();
		const bool bMaterialModifiesMesh = Material->MaterialModifiesMeshPosition_RenderThread();
		if (!bMaterialMasked && !bMaterialModifiesMesh)
		{
			// Override with the default material for opaque materials that are not two sided
			MaterialRenderProxy = GEngine->LevelColorationLitMaterial->GetRenderProxy(false);
		}

		FVertexDensityDrawingPolicy DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, BlendMode);
		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
		DrawingPolicy.SetSharedState(RHICmdList, &View, FVertexDensityDrawingPolicy::ContextDataType());
		const FMeshDrawingRenderState DrawRenderState(Mesh.DitheredLODTransitionAlpha);
		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
		{
			DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, bBackFace, DrawRenderState,
				FVertexDensityDrawingPolicy::ElementDataType(),
				FVertexDensityDrawingPolicy::ContextDataType()
				);
			DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex);
		}
		return true;
	}
};

#if !UE_BUILD_DOCS
IMPLEMENT_MATERIAL_SHADER_TYPE(, FVertexDensityVS, TEXT("VertexDensityShader"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FVertexDensityGS, TEXT("VertexDensityShader"), TEXT("MainGS"), SF_Geometry);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FVertexDensityPS, TEXT("VertexDensityShader"), TEXT("MainPS"), SF_Pixel);
#endif

bool FDeferredShadingSceneRenderer::RenderVertexDensities(FRHICommandListImmediate& RHICmdList)
{
	if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4 && RHISupportsGeometryShaders(Scene->GetShaderPlatform()))
	{
		SCOPED_DRAW_EVENT(RHICmdList, VertexDensity);

		for (int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			// Opaque blending, depth tests and writes.
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

			{
				SCOPED_DRAW_EVENT(RHICmdList, Dynamic);
				FVertexDensityDrawingPolicyFactory::ContextType Context;

				for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
				{
					const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

					if (MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial || ViewFamily.EngineShowFlags.Wireframe)
					{
						const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
						FVertexDensityDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
					}
				}
			}
		}
	}

	return false;
}
