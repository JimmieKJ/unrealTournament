// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileTranslucentRendering.cpp: translucent rendering implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "HitProxies.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "DrawingPolicy.h"
#include "SceneRendering.h"
#include "LightMapRendering.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "BasePassRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "TranslucentRendering.h"
#include "MobileBasePassRendering.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"

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
	FDrawingPolicyRenderState DrawRenderState;
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FDrawMobileTranslucentMeshAction(
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		float InDitheredLODTransitionAlpha,
		FDepthStencilStateRHIParamRef InDepthStencilState,
		const FDrawingPolicyRenderState& InDrawRenderState,
		FHitProxyId InHitProxyId
		):
		View(InView),
		DrawRenderState(&InRHICmdList, InDrawRenderState),
		HitProxyId(InHitProxyId)
	{
		DrawRenderState.SetDitheredLODTransitionAlpha(InDitheredLODTransitionAlpha);
		if (InDepthStencilState)
		{
			DrawRenderState.SetDepthStencilState(InRHICmdList, InDepthStencilState);
		}
	}
	
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
			ComputeMeshOverrideSettings(Parameters.Mesh),
			View.Family->GetDebugViewShaderMode(),
			View.GetFeatureLevel()
			);

		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, typename TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, NumDynamicPointLights>::ContextDataType(), DrawRenderState);

		if (Parameters.bUseMobileMultiViewMask)
		{
			// Mask opposite view
			DrawingPolicy.SetMobileMultiViewMask(RHICmdList, (View.StereoPass == EStereoscopicPass::eSSP_LEFT_EYE) ? 1 : 0);
		}

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
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
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
		FDepthStencilStateRHIParamRef DepthStencilState = nullptr;
		if (!DrawingContext.bRenderingSeparateTranslucency && Material->ShouldDisableDepthTest())
		{
			DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
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
				FeatureLevel, 
				false, // ISR disabled for mobile
				View.bIsMobileMultiViewEnabled
				),
			FDrawMobileTranslucentMeshAction(
				RHICmdList,
				View,
				Mesh.DitheredLODTransitionAlpha,
				DepthStencilState,
				DrawRenderState,
				HitProxyId
				)
			);

		if (DepthStencilState)
		{
			// Restore default depth state
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		}

		bDirty = true;
	}
	return bDirty;
}

template <class TDrawingPolicyFactory>
void FTranslucentPrimSet::DrawPrimitivesForMobile(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState,
	typename TDrawingPolicyFactory::ContextType& DrawingContext) const
{
	ETranslucencyPass::Type TranslucencyPass = DrawingContext.ShouldRenderSeparateTranslucency() ? ETranslucencyPass::TPT_SeparateTranslucency : ETranslucencyPass::TPT_StandardTranslucency;

	FInt32Range PassRange = SortedPrimsNum.GetPassRange(TranslucencyPass);

	// Draw sorted scene prims
	for (int32 PrimIdx = PassRange.GetLowerBoundValue(); PrimIdx < PassRange.GetUpperBoundValue(); PrimIdx++)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = SortedPrims[PrimIdx].PrimitiveSceneInfo;
		int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveId];

		checkSlow(ViewRelevance.HasTranslucency());

		if (ViewRelevance.bDrawRelevance)
		{
			// range in View.DynamicMeshElements[]
			FInt32Range range = View.GetDynamicMeshElementRange(PrimitiveSceneInfo->GetIndex());

			for (int32 MeshBatchIndex = range.GetLowerBoundValue(); MeshBatchIndex < range.GetUpperBoundValue(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				checkSlow(MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneInfo->Proxy);

				const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
				TDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, DrawingContext, MeshBatch, false, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
			}

			// Render static scene prim
			if (ViewRelevance.bStaticRelevance)
			{
				// Render static meshes from static scene prim
				for (int32 StaticMeshIdx = 0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
				{
					FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
					if (View.StaticMeshVisibilityMap[StaticMesh.Id]
						// Only render static mesh elements using translucent materials
						&& StaticMesh.IsTranslucent(View.GetFeatureLevel()))
					{
						TDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, DrawingContext, StaticMesh, false, DrawRenderState, PrimitiveSceneInfo->Proxy, StaticMesh.BatchHitProxyId);
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
			FDrawingPolicyRenderState DrawRenderState(&RHICmdList, View);

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
				// Mobile multi-view is not side by side stereo
				const FViewInfo& TranslucentViewport = (View.bIsMobileMultiViewEnabled) ? Views[0] : View;
				RHICmdList.SetViewport(TranslucentViewport.ViewRect.Min.X, TranslucentViewport.ViewRect.Min.Y, 0.0f, TranslucentViewport.ViewRect.Max.X, TranslucentViewport.ViewRect.Max.Y, 1.0f);
			}

			// Enable depth test, disable depth writes.
			DrawRenderState.SetDepthStencilState(RHICmdList, TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

			// Draw only translucent prims that don't read from scene color
			FMobileTranslucencyDrawingPolicyFactory::ContextType DrawingContext(false);
			View.TranslucentPrimSet.DrawPrimitivesForMobile<FMobileTranslucencyDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, DrawingContext);
			// Draw the view's mesh elements with the translucent drawing policy.
			DrawViewElements<FMobileTranslucencyDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, FMobileTranslucencyDrawingPolicyFactory::ContextType(false), SDPG_World, false);
			// Draw the view's mesh elements with the translucent drawing policy.
			DrawViewElements<FMobileTranslucencyDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, FMobileTranslucencyDrawingPolicyFactory::ContextType(false), SDPG_Foreground, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Translucent material inverse opacity render code
// Used to generate inverse opacity channel for scene captures that require opacity information.
// See MobileSceneCaptureRendering for more details.

/**
* Vertex shader for mobile opacity only pass
*/
class FOpacityOnlyVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FOpacityOnlyVS, MeshMaterial);
protected:

	FOpacityOnlyVS() {}
	FOpacityOnlyVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
	}

public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return IsTranslucentBlendMode(Material->GetBlendMode()) && IsMobilePlatform(Platform);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool result = FMeshMaterialShader::Serialize(Ar);
		return result;
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& MaterialResource,
		const FSceneView& View
	)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, MaterialResource, View, View.ViewUniformBuffer, ESceneRenderTargetsMode::DontSet);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FOpacityOnlyVS, TEXT("MobileOpacityShaders"), TEXT("MainVS"), SF_Vertex);

/**
* Pixel shader for mobile opacity only pass, writes opacity to alpha channel.
*/
class FOpacityOnlyPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FOpacityOnlyPS, MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return IsTranslucentBlendMode(Material->GetBlendMode()) && IsMobilePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOBILE_FORCE_DEPTH_TEXTURE_READS"), 1u);
	}

	FOpacityOnlyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
	}

	FOpacityOnlyPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& MaterialResource, const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, MaterialResource, *View, View->ViewUniformBuffer, ESceneRenderTargetsMode::DontSet);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FOpacityOnlyPS, TEXT("MobileOpacityShaders"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(MobileOpacityPipeline, FOpacityOnlyVS, FOpacityOnlyPS, true);

class FMobileOpacityDrawingPolicy : public FMeshDrawingPolicy
{
public:

	struct ContextDataType : public FMeshDrawingPolicy::ContextDataType
	{
	};

	FMobileOpacityDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings
	) :
		FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings)
	{
		const EMaterialTessellationMode TessellationMode = InMaterialResource.GetTessellationMode();
		static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderPipelines"));
		bool bUseShaderPipelines = CVar && CVar->GetValueOnAnyThread() != 0;

		ShaderPipeline = bUseShaderPipelines ? InMaterialResource.GetShaderPipeline(&MobileOpacityPipeline, InVertexFactory->GetType()) : nullptr;

		if (ShaderPipeline)
		{
			VertexShader = ShaderPipeline->GetShader<FOpacityOnlyVS >();
			PixelShader = ShaderPipeline->GetShader<FOpacityOnlyPS>();
		}
		else
		{
			VertexShader = InMaterialResource.GetShader<FOpacityOnlyVS >(VertexFactory->GetType());
			PixelShader = InMaterialResource.GetShader<FOpacityOnlyPS>(InVertexFactory->GetType());
		}
	}

	// FMeshDrawingPolicy interface.
	FDrawingPolicyMatchResult Matches(const FMobileOpacityDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
		DRAWING_POLICY_MATCH_END
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const FMobileOpacityDrawingPolicy::ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState) const
	{
		VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View);
		PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View);

		// Set the shared mesh resources.
		FMeshDrawingPolicy::SetSharedState(RHICmdList, View, PolicyContext, DrawRenderState);
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
			nullptr,
			nullptr,
			PixelShader->GetPixelShader(),
			NULL);
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList,
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		const FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
	) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
		VertexShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
		PixelShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);

		FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderState, ElementData, PolicyContext);
	}

	friend int32 CompareDrawingPolicy(const FMobileOpacityDrawingPolicy& A, const FMobileOpacityDrawingPolicy& B);

private:

	FShaderPipeline* ShaderPipeline;
	FOpacityOnlyVS* VertexShader;
	FOpacityOnlyPS* PixelShader;
};

int32 CompareDrawingPolicy(const FMobileOpacityDrawingPolicy& A, const FMobileOpacityDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}

class FMobileOpacityDrawingPolicyFactory
{
public:

	struct ContextType
	{
		bool ShouldRenderSeparateTranslucency() const { return false; }
	};

	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
	)
	{
		return DrawMesh(
			RHICmdList,
			View,
			DrawingContext,
			Mesh,
			Mesh.Elements.Num() == 1 ? 1ull : (1ull << Mesh.Elements.Num()) - 1ull,	// 1 bit set for each mesh element
			DrawRenderState,
			bPreFog,
			PrimitiveSceneProxy,
			HitProxyId
		);
	}

private:
	/**
	* Render a dynamic or static mesh using the opacity draw policy
	* @return true if the mesh rendered
	*/
	static bool DrawMesh(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		const uint64& BatchElementMask,
		const FDrawingPolicyRenderState& DrawRenderState,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
	)
	{
		bool bDirty = false;

		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
		const EBlendMode BlendMode = Material->GetBlendMode();

		bool bDraw = IsTranslucentBlendMode(BlendMode);
		if (bDraw)
		{
			FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(Mesh);
			OverrideSettings.MeshOverrideFlags |= Material->IsTwoSided() ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;
			FMobileOpacityDrawingPolicy DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View.GetFeatureLevel(), OverrideSettings);
			RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));

			FDrawingPolicyRenderState DrawRenderStateLocal(&RHICmdList, DrawRenderState);
			DrawingPolicy.SetSharedState(RHICmdList, &View, FMobileOpacityDrawingPolicy::ContextDataType(), DrawRenderStateLocal);

			int32 BatchElementIndex = 0;
			uint64 Mask = BatchElementMask;
			do
			{
				if (Mask & 1)
				{
					const bool bIsInstancedMesh = Mesh.Elements[BatchElementIndex].bIsInstancedMesh;
					TDrawEvent<FRHICommandList> MeshEvent;
					BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

					DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal, FMeshDrawingPolicy::ElementDataType(), FMobileOpacityDrawingPolicy::ContextDataType());
					DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex);
				}
				Mask >>= 1;
				BatchElementIndex++;
			} while (Mask);

			bDirty = true;
		}		

		return bDirty;
	}
};

bool FMobileSceneRenderer::RenderInverseOpacity(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FDrawingPolicyRenderState& DrawRenderState)
{
	bool bDirty = false;

	if (ShouldRenderTranslucency())
	{
		const bool bGammaSpace = !IsMobileHDR();
		const bool bLinearHDR64 = !bGammaSpace && !IsMobileHDR32bpp();

		{
			if (!bGammaSpace)
			{
				FSceneRenderTargets::Get(RHICmdList).BeginRenderingTranslucency(RHICmdList, View);
			}
			else
			{
				// Mobile multi-view is not side by side stereo
				const FViewInfo& TranslucentViewport = (View.bIsMobileMultiViewEnabled) ? Views[0] : View;
				RHICmdList.SetViewport(TranslucentViewport.ViewRect.Min.X, TranslucentViewport.ViewRect.Min.Y, 0.0f, TranslucentViewport.ViewRect.Max.X, TranslucentViewport.ViewRect.Max.Y, 1.0f);
			}
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			// Enable depth test, disable depth writes.
			DrawRenderState.SetDepthStencilState(RHICmdList, TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
			DrawRenderState.SetBlendState(RHICmdList, TStaticBlendState<CW_ALPHA, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());

			{
				SCOPED_DRAW_EVENT(RHICmdList, InverseOpacity);
				bDirty |= RenderInverseOpacityDynamic(RHICmdList, View, DrawRenderState);
			}
		}
	}
	return bDirty;
}

bool FMobileSceneRenderer::RenderInverseOpacityDynamic(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState)
{
	FMobileOpacityDrawingPolicyFactory::ContextType DrawingContext;
	View.TranslucentPrimSet.DrawPrimitivesForMobile<FMobileOpacityDrawingPolicyFactory>(RHICmdList, View, DrawRenderState, DrawingContext);
	return View.TranslucentPrimSet.NumPrims() > 0;
}
