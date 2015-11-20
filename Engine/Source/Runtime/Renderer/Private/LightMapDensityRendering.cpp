// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMapDensityRendering.cpp: Implementation for rendering lightmap density.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"

//-----------------------------------------------------------------------------

#if !UE_BUILD_DOCS
// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_DENSITY_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TLightMapDensityVS< LightMapPolicyType > TLightMapDensityVS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityVS##LightMapPolicyName,TEXT("LightMapDensityShader"),TEXT("MainVertexShader"),SF_Vertex); \
	typedef TLightMapDensityHS< LightMapPolicyType > TLightMapDensityHS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityHS##LightMapPolicyName,TEXT("LightMapDensityShader"),TEXT("MainHull"),SF_Hull); \
	typedef TLightMapDensityDS< LightMapPolicyType > TLightMapDensityDS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityDS##LightMapPolicyName,TEXT("LightMapDensityShader"),TEXT("MainDomain"),SF_Domain); 

#define IMPLEMENT_DENSITY_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TLightMapDensityPS< LightMapPolicyType > TLightMapDensityPS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityPS##LightMapPolicyName,TEXT("LightMapDensityShader"),TEXT("MainPixelShader"),SF_Pixel);

// Implement a pixel shader type for skylights and one without, and one vertex shader that will be shared between them
#define IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_DENSITY_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_DENSITY_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName);

IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE( FNoLightMapPolicy, FNoLightMapPolicy );
IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE( FDummyLightMapPolicy, FDummyLightMapPolicy );
IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE( TLightMapPolicy<LQ_LIGHTMAP>, TLightMapPolicyLQ );
IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE( TLightMapPolicy<HQ_LIGHTMAP>, TLightMapPolicyHQ );
#endif

bool FDeferredShadingSceneRenderer::RenderLightMapDensities(FRHICommandListImmediate& RHICmdList)
{
	bool bDirty=0;
	if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		SCOPED_DRAW_EVENT(RHICmdList, LightMapDensity);

		// Draw the scene's emissive and light-map color.
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			// Opaque blending, depth tests and writes.
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true,CF_DepthNearOrEqual>::GetRHI());
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

			{
				SCOPED_DRAW_EVENT(RHICmdList, Dynamic);

				FLightMapDensityDrawingPolicyFactory::ContextType Context;

				for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
				{
					const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

					if (MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial || ViewFamily.EngineShowFlags.Wireframe)
					{
						const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
						FLightMapDensityDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
					}
				}
			}
		}
	}

	return bDirty;
}

bool FLightMapDensityDrawingPolicyFactory::DrawDynamicMesh(
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
	bool bDirty = false;

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

	bool bIsLitMaterial = (Material->GetShadingModel() != MSM_Unlit);
	/*const */FLightMapInteraction LightMapInteraction = (Mesh.LCI && bIsLitMaterial) ? Mesh.LCI->GetLightMapInteraction(FeatureLevel) : FLightMapInteraction();
	// force simple lightmaps based on system settings
	bool bAllowHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel) && LightMapInteraction.AllowsHighQualityLightmaps();
	if (bIsLitMaterial && PrimitiveSceneProxy && (LightMapInteraction.GetType() == LMIT_Texture))
	{
		// Should this object be texture lightmapped? Ie, is lighting not built for it??
		bool bUseDummyLightMapPolicy = false;
		if ((Mesh.LCI == NULL) || (Mesh.LCI->GetLightMapInteraction(FeatureLevel).GetType() != LMIT_Texture))
		{
			bUseDummyLightMapPolicy = true;

			LightMapInteraction.SetLightMapInteractionType(LMIT_Texture);
			LightMapInteraction.SetCoordinateScale(FVector2D(1.0f,1.0f));
			LightMapInteraction.SetCoordinateBias(FVector2D(0.0f,0.0f));
		}
		if (bUseDummyLightMapPolicy == false)
		{
			if (bAllowHighQualityLightMaps)
			{
				TLightMapDensityDrawingPolicy< TLightMapPolicy<HQ_LIGHTMAP> > DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, TLightMapPolicy<HQ_LIGHTMAP>(), BlendMode);
				RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
				DrawingPolicy.SetSharedState(RHICmdList, &View, TLightMapDensityDrawingPolicy< TLightMapPolicy<HQ_LIGHTMAP> >::ContextDataType());
				for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,Mesh.DitheredLODTransitionAlpha,
						TLightMapDensityDrawingPolicy< TLightMapPolicy<HQ_LIGHTMAP> >::ElementDataType(LightMapInteraction),
						TLightMapDensityDrawingPolicy< TLightMapPolicy<HQ_LIGHTMAP> >::ContextDataType()
						);
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
				}
				bDirty = true;
			}
			else
			{
				TLightMapDensityDrawingPolicy< TLightMapPolicy<LQ_LIGHTMAP> > DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, TLightMapPolicy<LQ_LIGHTMAP>(), BlendMode);
				RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
				DrawingPolicy.SetSharedState(RHICmdList, &View, TLightMapDensityDrawingPolicy< TLightMapPolicy<LQ_LIGHTMAP> >::ContextDataType());
				for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,Mesh.DitheredLODTransitionAlpha,
						TLightMapDensityDrawingPolicy< TLightMapPolicy<LQ_LIGHTMAP> >::ElementDataType(LightMapInteraction),
						TLightMapDensityDrawingPolicy< TLightMapPolicy<LQ_LIGHTMAP> >::ContextDataType()
						);
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
				}
				bDirty = true;
			}
		}
		else
		{
			TLightMapDensityDrawingPolicy<FDummyLightMapPolicy> DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, FDummyLightMapPolicy(), BlendMode);
			RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
			DrawingPolicy.SetSharedState(RHICmdList, &View, TLightMapDensityDrawingPolicy<FDummyLightMapPolicy>::ContextDataType());
			for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
			{
				DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,Mesh.DitheredLODTransitionAlpha,
					TLightMapDensityDrawingPolicy<FDummyLightMapPolicy>::ElementDataType(LightMapInteraction),
					TLightMapDensityDrawingPolicy<FDummyLightMapPolicy>::ContextDataType()
					);
				DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
			}
			bDirty = true;
		}
	}
	else
	{
		TLightMapDensityDrawingPolicy<FNoLightMapPolicy> DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, FNoLightMapPolicy(), BlendMode);
		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
		DrawingPolicy.SetSharedState(RHICmdList, &View, TLightMapDensityDrawingPolicy<FNoLightMapPolicy>::ContextDataType());
		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
		{
			DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,Mesh.DitheredLODTransitionAlpha,
				TLightMapDensityDrawingPolicy<FNoLightMapPolicy>::ElementDataType(),
				TLightMapDensityDrawingPolicy<FNoLightMapPolicy>::ContextDataType()
				);
			DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
		}
		bDirty = true;
	}
	
	return bDirty;
}
