// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileTranslucentRendering.cpp: translucent rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"

/** Pixel shader used to copy scene color into another texture so that materials can read from scene color with a node. */
class FMobileCopySceneAlphaPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileCopySceneAlphaPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FMobileCopySceneAlphaPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
	}
	FMobileCopySceneAlphaPS() {}

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

IMPLEMENT_SHADER_TYPE(,FMobileCopySceneAlphaPS,TEXT("TranslucentLightingShaders"),TEXT("CopySceneAlphaMain"),SF_Pixel);

FGlobalBoundShaderState MobileCopySceneAlphaBoundShaderState;

void FMobileSceneRenderer::CopySceneAlpha(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
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
	TShaderMapRef<FMobileCopySceneAlphaPS> PixelShader(View.ShaderMap);
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, MobileCopySceneAlphaBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

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
class FDrawMobileTranslucentMeshAction
{
public:

	const FViewInfo& View;
	bool bBackFace;
	FMeshDrawingRenderState DrawRenderState;
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FDrawMobileTranslucentMeshAction(
		const FViewInfo& InView,
		bool bInBackFace,
		const FMeshDrawingRenderState& InDrawRenderState,
		FHitProxyId InHitProxyId
		):
		View(InView),
		bBackFace(bInBackFace),
		DrawRenderState(InDrawRenderState),
		HitProxyId(InHitProxyId)
	{}
	
	inline bool ShouldPackAmbientSH() const
	{
		// So shader code can read a single constant to get the ambient term
		return true;
	}

	bool CanReceiveStaticAndCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy) const { return false; }

	const FScene* GetScene() const
	{ 
		return ((FScene*)View.Family->Scene);
	}

	/** Draws the translucent mesh with a specific light-map type, and fog volume type */
	template<int32 NumDynamicPointLights>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const FUniformLightMapPolicy& LightMapPolicy,
		const typename FUniformLightMapPolicy::ElementDataType& LightMapElementData
		) const
	{
		const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
		const FScene* Scene = Parameters.PrimitiveSceneProxy ? Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->Scene : NULL;

		TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, NumDynamicPointLights> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.BlendMode,
			Parameters.TextureMode,
			Parameters.ShadingModel != MSM_Unlit && Scene && Scene->ShouldRenderSkylight(Parameters.BlendMode),
			View.Family->GetDebugViewShaderMode(),
			View.GetFeatureLevel()
			);

		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, typename TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, NumDynamicPointLights>::ContextDataType());

		for (int32 BatchElementIndex = 0; BatchElementIndex<Parameters.Mesh.Elements.Num(); BatchElementIndex++)
		{
			TDrawEvent<FRHICommandList> MeshEvent;
			BeginMeshDrawEvent(RHICmdList, Parameters.PrimitiveSceneProxy, Parameters.Mesh, MeshEvent);

			DrawingPolicy.SetMeshRenderState(
				RHICmdList, 
				View,
				Parameters.PrimitiveSceneProxy,
				Parameters.Mesh,
				BatchElementIndex,
				bBackFace,
				DrawRenderState,
				typename TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, NumDynamicPointLights>::ElementDataType(LightMapElementData),
				typename TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, NumDynamicPointLights>::ContextDataType()
				);
			DrawingPolicy.DrawMesh(RHICmdList, Parameters.Mesh, BatchElementIndex);
		}
	}
};

/**
 * Render a dynamic mesh using a translucent draw policy
 * @return true if the mesh rendered
 */
bool FMobileTranslucencyDrawingPolicyFactory::DrawDynamicMesh(
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
		const bool bDisableDepthTest = !DrawingContext.bRenderingSeparateTranslucency && Material->ShouldDisableDepthTest();
		if (bDisableDepthTest)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		}

		ProcessMobileBasePassMesh<FDrawMobileTranslucentMeshAction, 0>(
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
			FDrawMobileTranslucentMeshAction(
				View,
				bBackFace,
				FMeshDrawingRenderState(Mesh.DitheredLODTransitionAlpha),
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
bool FMobileTranslucencyDrawingPolicyFactory::DrawStaticMesh(
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

void FTranslucentPrimSet::DrawPrimitivesForMobile(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const bool bRenderSeparateTranslucency) const
{
	ETranslucencyPass::Type TranslucencyPass = bRenderSeparateTranslucency ? ETranslucencyPass::TPT_SeparateTranslucency : ETranslucencyPass::TPT_StandardTranslucency;

	FInt32Range PassRange = SortedPrimsNum.GetPassRange(TranslucencyPass);

	// Draw sorted scene prims
	for (int32 PrimIdx = PassRange.GetLowerBoundValue(); PrimIdx < PassRange.GetUpperBoundValue(); PrimIdx++)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = SortedPrims[PrimIdx].PrimitiveSceneInfo;
		int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveId];

		checkSlow(ViewRelevance.HasTranslucency());

		if(ViewRelevance.bDrawRelevance)
		{
			FMobileTranslucencyDrawingPolicyFactory::ContextType Context(bRenderSeparateTranslucency);

			// range in View.DynamicMeshElements[]
			FInt32Range range = View.GetDynamicMeshElementRange(PrimitiveSceneInfo->GetIndex());

			for (int32 MeshBatchIndex = range.GetLowerBoundValue(); MeshBatchIndex < range.GetUpperBoundValue(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				checkSlow(MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneInfo->Proxy);

				const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
				FMobileTranslucencyDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, false, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
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
						&& StaticMesh.IsTranslucent(View.GetFeatureLevel()))
					{
						FMobileTranslucencyDrawingPolicyFactory::DrawStaticMesh(
							RHICmdList, 
							View,
							FMobileTranslucencyDrawingPolicyFactory::ContextType(bRenderSeparateTranslucency),
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

void FMobileSceneRenderer::RenderTranslucency(FRHICommandListImmediate& RHICmdList)
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
			View.TranslucentPrimSet.DrawPrimitivesForMobile(RHICmdList, View, false);
			// Draw the view's mesh elements with the translucent drawing policy.
			DrawViewElements<FMobileTranslucencyDrawingPolicyFactory>(RHICmdList, View, FMobileTranslucencyDrawingPolicyFactory::ContextType(false), SDPG_World, false);
			// Draw the view's mesh elements with the translucent drawing policy.
			DrawViewElements<FMobileTranslucencyDrawingPolicyFactory>(RHICmdList, View, FMobileTranslucencyDrawingPolicyFactory::ContextType(false), SDPG_Foreground, false);
		}
	}
}
