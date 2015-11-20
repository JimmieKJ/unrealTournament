// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ForwardTranslucentRendering.cpp: translucent rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"

/** Pixel shader used to copy scene color into another texture so that materials can read from scene color with a node. */
class FForwardCopySceneAlphaPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FForwardCopySceneAlphaPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FForwardCopySceneAlphaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
	}
	FForwardCopySceneAlphaPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(,FForwardCopySceneAlphaPS,TEXT("TranslucentLightingShaders"),TEXT("CopySceneAlphaMain"),SF_Pixel);

FGlobalBoundShaderState ForwardCopySceneAlphaBoundShaderState;

void FForwardShadingSceneRenderer::CopySceneAlpha(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	SCOPED_DRAW_EVENTF(RHICmdList, EventCopy, TEXT("CopySceneAlpha"));
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

	SceneContext.ResolveSceneColor(RHICmdList);

	SceneContext.BeginRenderingSceneAlphaCopy(RHICmdList);

	int X = SceneContext.GetBufferSizeXY().X;
	int Y = SceneContext.GetBufferSizeXY().Y;

	RHICmdList.SetViewport(0, 0, 0.0f, X, Y, 1.0f);

	TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
	TShaderMapRef<FForwardCopySceneAlphaPS> PixelShader(View.ShaderMap);
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, ForwardCopySceneAlphaBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

	PixelShader->SetParameters(RHICmdList, View);

	DrawRectangle( 
		RHICmdList,
		0, 0, 
		X, Y, 
		0, 0, 
		X, Y,
		FIntPoint(X, Y),
		SceneContext.GetBufferSizeXY(),
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);

	SceneContext.FinishRenderingSceneAlphaCopy(RHICmdList);
}







/** The parameters used to draw a translucent mesh. */
class FDrawTranslucentMeshForwardShadingAction
{
public:

	const FViewInfo& View;
	bool bBackFace;
	float DitheredLODTransitionValue;
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FDrawTranslucentMeshForwardShadingAction(
		const FViewInfo& InView,
		bool bInBackFace,
		float InDitheredLODTransitionValue,
		FHitProxyId InHitProxyId
		):
		View(InView),
		bBackFace(bInBackFace),
		DitheredLODTransitionValue(InDitheredLODTransitionValue),
		HitProxyId(InHitProxyId)
	{}
	
	inline bool ShouldPackAmbientSH() const
	{
		// So shader code can read a single constant to get the ambient term
		return true;
	}

	const FLightSceneInfo* GetSimpleDirectionalLight() const 
	{ 
		return ((FScene*)View.Family->Scene)->SimpleDirectionalLight;
	}

	/** Draws the translucent mesh with a specific light-map type, and fog volume type */
	template<typename LightMapPolicyType, int32 NumDynamicPointLights>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		) const
	{
		const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
		const FScene* Scene = Parameters.PrimitiveSceneProxy ? Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->Scene : NULL;

		TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType, NumDynamicPointLights> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.BlendMode,
			Parameters.TextureMode,
			Parameters.ShadingModel != MSM_Unlit && Scene && Scene->ShouldRenderSkylight(),
			View.Family->EngineShowFlags.ShaderComplexity,
			View.GetFeatureLevel()
			);

		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, typename TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType, NumDynamicPointLights>::ContextDataType());

		for (int32 BatchElementIndex = 0; BatchElementIndex<Parameters.Mesh.Elements.Num(); BatchElementIndex++)
		{
			DrawingPolicy.SetMeshRenderState(
				RHICmdList, 
				View,
				Parameters.PrimitiveSceneProxy,
				Parameters.Mesh,
				BatchElementIndex,
				bBackFace,
				DitheredLODTransitionValue,
				typename TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType, NumDynamicPointLights>::ElementDataType(LightMapElementData),
				typename TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType, NumDynamicPointLights>::ContextDataType()
				);
			DrawingPolicy.DrawMesh(RHICmdList, Parameters.Mesh, BatchElementIndex);
		}
	}
};

/**
 * Render a dynamic mesh using a translucent draw policy
 * @return true if the mesh rendered
 */
bool FTranslucencyForwardShadingDrawingPolicyFactory::DrawDynamicMesh(
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

	// Determine the mesh's material and blend mode.
	const auto FeatureLevel = View.GetFeatureLevel();
	const auto ShaderPlatform = View.GetShaderPlatform();
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel);
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only render translucent materials.
	if (IsTranslucentBlendMode(BlendMode))
	{
		const bool bDisableDepthTest = Material->ShouldDisableDepthTest();
		if (bDisableDepthTest)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		}

		ProcessBasePassMeshForForwardShading<FDrawTranslucentMeshForwardShadingAction, 0>(
			RHICmdList,
			FProcessBasePassMeshParameters(
				Mesh,
				Material,
				PrimitiveSceneProxy,
				true,
				false,
				ESceneRenderTargetsMode::SetTextures,
				FeatureLevel
				),
			FDrawTranslucentMeshForwardShadingAction(
				View,
				bBackFace,
				Mesh.DitheredLODTransitionAlpha,
				HitProxyId
				)
			);

		if (bDisableDepthTest)
		{
			// Restore default depth state
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		}

		bDirty = true;
	}
	return bDirty;
}

/**
 * Render a static mesh using a translucent draw policy
 * @return true if the mesh rendered
 */
bool FTranslucencyForwardShadingDrawingPolicyFactory::DrawStaticMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FStaticMesh& StaticMesh,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	bool bDirty = false;
	const FMaterial* Material = StaticMesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());

	bDirty |= DrawDynamicMesh(
		RHICmdList, 
		View,
		DrawingContext,
		StaticMesh,
		false,
		bPreFog,
		PrimitiveSceneProxy,
		HitProxyId
		);
	return bDirty;
}

/*-----------------------------------------------------------------------------
FTranslucentPrimSet
-----------------------------------------------------------------------------*/

void FTranslucentPrimSet::DrawPrimitivesForForwardShading(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FSceneRenderer& Renderer) const
{
	// Draw sorted scene prims
	for (int32 PrimIdx = 0; PrimIdx < SortedPrims.Num(); PrimIdx++)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = SortedPrims[PrimIdx].PrimitiveSceneInfo;
		int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveId];

		checkSlow(ViewRelevance.HasTranslucency());

		if(ViewRelevance.bDrawRelevance)
		{
			FTranslucencyForwardShadingDrawingPolicyFactory::ContextType Context;

			//@todo parallelrendering - come up with a better way to filter these by primitive
			for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				if (MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneInfo->Proxy)
				{
					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
					FTranslucencyForwardShadingDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, false, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
				}
			}

			// Render static scene prim
			if( ViewRelevance.bStaticRelevance )
			{
				// Render static meshes from static scene prim
				for( int32 StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++ )
				{
					FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
					if (View.StaticMeshVisibilityMap[StaticMesh.Id]
						// Only render static mesh elements using translucent materials
						&& StaticMesh.IsTranslucent(View.GetFeatureLevel()) )
					{
						FTranslucencyForwardShadingDrawingPolicyFactory::DrawStaticMesh(
							RHICmdList, 
							View,
							FTranslucencyForwardShadingDrawingPolicyFactory::ContextType(),
							StaticMesh,
							false,
							PrimitiveSceneInfo->Proxy,
							StaticMesh.BatchHitProxyId
							);
					}
				}
			}
		}
	}

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, View, FTexture2DRHIRef(), EBlendModeFilter::Translucent);
}

void FForwardShadingSceneRenderer::RenderTranslucency(FRHICommandListImmediate& RHICmdList)
{
	if (ShouldRenderTranslucency())
	{
		const bool bGammaSpace = !IsMobileHDR();
		const bool bLinearHDR64 = !bGammaSpace && !IsMobileHDR32bpp();

		SCOPED_DRAW_EVENT(RHICmdList, Translucency);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			const FViewInfo& View = Views[ViewIndex];

			#if PLATFORM_HTML5
			// Copy the view so emulation of framebuffer fetch works for alpha=depth.
			// Possible optimization: this copy shouldn't be needed unless something uses fetch of depth.
			if(bLinearHDR64 && GSupportsRenderTargetFormat_PF_FloatRGBA && (GSupportsShaderFramebufferFetch == false) && (!IsPCPlatform(View.GetShaderPlatform())))
			{
				CopySceneAlpha(RHICmdList, View);
			}
			#endif 

			if (!bGammaSpace)
			{
				FSceneRenderTargets::Get(RHICmdList).BeginRenderingTranslucency(RHICmdList, View);
			}
			else
			{
				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			}

			// Enable depth test, disable depth writes.
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());

			// Draw only translucent prims that don't read from scene color
			View.TranslucentPrimSet.DrawPrimitivesForForwardShading(RHICmdList, View, *this);
			// Draw the view's mesh elements with the translucent drawing policy.
			DrawViewElements<FTranslucencyForwardShadingDrawingPolicyFactory>(RHICmdList, View, FTranslucencyForwardShadingDrawingPolicyFactory::ContextType(), SDPG_World, false);
			// Draw the view's mesh elements with the translucent drawing policy.
			DrawViewElements<FTranslucencyForwardShadingDrawingPolicyFactory>(RHICmdList, View, FTranslucencyForwardShadingDrawingPolicyFactory::ContextType(), SDPG_Foreground, false);
		}
	}
}
