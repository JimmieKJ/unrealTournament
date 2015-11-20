// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ForwardBasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "Algo/Partition.h"
#include "MaterialShaderQualitySettings.h"

#define IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_VERTEX_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TBasePassForForwardShadingVS< LightMapPolicyType, LDR_GAMMA_32 > TBasePassForForwardShadingVS##LightMapPolicyName##LDRGamma32; \
	typedef TBasePassForForwardShadingVS< LightMapPolicyType, HDR_LINEAR_64 > TBasePassForForwardShadingVS##LightMapPolicyName##HDRLinear64; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TBasePassForForwardShadingVS##LightMapPolicyName##LDRGamma32, TEXT("BasePassForForwardShadingVertexShader"), TEXT("Main"), SF_Vertex); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TBasePassForForwardShadingVS##LightMapPolicyName##HDRLinear64, TEXT("BasePassForForwardShadingVertexShader"), TEXT("Main"), SF_Vertex);

#define IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName,NumDynamicPointLights) \
	typedef TBasePassForForwardShadingPS< LightMapPolicyType, LDR_GAMMA_32, false, NumDynamicPointLights > TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##LDRGamma32; \
	typedef TBasePassForForwardShadingPS< LightMapPolicyType, HDR_LINEAR_32, false, NumDynamicPointLights > TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear32; \
	typedef TBasePassForForwardShadingPS< LightMapPolicyType, HDR_LINEAR_64, false, NumDynamicPointLights > TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear64; \
	typedef TBasePassForForwardShadingPS< LightMapPolicyType, LDR_GAMMA_32, true, NumDynamicPointLights > TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##LDRGamma32##Skylight; \
	typedef TBasePassForForwardShadingPS< LightMapPolicyType, HDR_LINEAR_32, true, NumDynamicPointLights > TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear32##Skylight; \
	typedef TBasePassForForwardShadingPS< LightMapPolicyType, HDR_LINEAR_64, true, NumDynamicPointLights > TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear64##Skylight; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##LDRGamma32, TEXT("BasePassForForwardShadingPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear32, TEXT("BasePassForForwardShadingPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear64, TEXT("BasePassForForwardShadingPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##LDRGamma32##Skylight, TEXT("BasePassForForwardShadingPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear32##Skylight, TEXT("BasePassForForwardShadingPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TBasePassForForwardShadingPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear64##Skylight, TEXT("BasePassForForwardShadingPixelShader"), TEXT("Main"), SF_Pixel);

static_assert(MAX_BASEPASS_DYNAMIC_POINT_LIGHTS == 4, "If you change MAX_BASEPASS_DYNAMIC_POINT_LIGHTS, you need to add shader types below");

// Permutations for the number of point lights to support. INT32_MAX indicates the shader should use branching to support a variable number of point lights.
#define IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_VERTEX_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName) \
	IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 0) \
	IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 1) \
	IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 2) \
	IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 3) \
	IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 4) \
	IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, INT32_MAX)

// Implement shader types per lightmap policy 
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FNoLightMapPolicy, FNoLightMapPolicy);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TLightMapPolicy<LQ_LIGHTMAP>, TLightMapPolicyLQ);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TDistanceFieldShadowsAndLightMapPolicy<LQ_LIGHTMAP>, TDistanceFieldShadowsAndLightMapPolicyLQ);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FSimpleDirectionalLightAndSHIndirectPolicy, FSimpleDirectionalLightAndSHIndirectPolicy);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FSimpleDirectionalLightAndSHDirectionalIndirectPolicy, FSimpleDirectionalLightAndSHDirectionalIndirectPolicy);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy, FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FMovableDirectionalLightLightingPolicy, FMovableDirectionalLightLightingPolicy);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FMovableDirectionalLightCSMLightingPolicy, FMovableDirectionalLightCSMLightingPolicy);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FMovableDirectionalLightWithLightmapLightingPolicy, FMovableDirectionalLightWithLightmapLightingPolicy);
IMPLEMENT_FORWARD_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FMovableDirectionalLightCSMWithLightmapLightingPolicy, FMovableDirectionalLightCSMWithLightmapLightingPolicy);

template<typename LightMapPolicyType, int32 NumDynamicPointLights>
bool TBasePassForForwardShadingPSBaseType<LightMapPolicyType, NumDynamicPointLights>::ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment)
{
	// Get quality settings for shader platform
	const UShaderPlatformQualitySettings* MaterialShadingQuality = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(Platform);
	const FMaterialQualityOverrides& QualityOverrides = MaterialShadingQuality->GetQualityOverrides(QualityLevel);

	OutEnvironment.SetDefine(TEXT("FORWARD_QL_FORCE_FULLY_ROUGH"), QualityOverrides.bEnableOverride && QualityOverrides.bForceFullyRough != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("FORWARD_QL_FORCE_NONMETAL"), QualityOverrides.bEnableOverride && QualityOverrides.bForceNonMetal != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("QL_FORCEDISABLE_LM_DIRECTIONALITY"), QualityOverrides.bEnableOverride && QualityOverrides.bForceDisableLMDirectionality != 0 ? 1u : 0u);
	
	return true;
}

FBasePassFowardDynamicPointLightInfo::FBasePassFowardDynamicPointLightInfo(const FPrimitiveSceneProxy* InSceneProxy)
: NumDynamicPointLights(0)
{
	static auto* MobileNumDynamicPointLightsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileNumDynamicPointLights"));
	const int32 MobileNumDynamicPointLights = MobileNumDynamicPointLightsCVar->GetValueOnRenderThread();

	if (InSceneProxy != nullptr)
	{
		for (FLightPrimitiveInteraction* LPI = InSceneProxy->GetPrimitiveSceneInfo()->LightList; LPI && NumDynamicPointLights < MobileNumDynamicPointLights; LPI = LPI->GetNextLight())
		{
			FLightSceneProxy* LightProxy = LPI->GetLight()->Proxy;
			if (LightProxy->GetLightType() == LightType_Point && LightProxy->IsMovable())
			{
				FVector NormalizedLightDirection;
				FVector2D SpotAngles;
				float SourceRadius;
				float SourceLength;
				float MinRoughness;

				// Get the light parameters
				LightProxy->GetParameters(
					LightPositionAndInvRadius[NumDynamicPointLights],
					LightColorAndFalloffExponent[NumDynamicPointLights],
					NormalizedLightDirection,
					SpotAngles,
					SourceRadius,
					SourceLength,
					MinRoughness);

				if (LightProxy->IsInverseSquared())
				{
					// Correction for lumen units
					LightColorAndFalloffExponent[NumDynamicPointLights].X *= 16.0f;
					LightColorAndFalloffExponent[NumDynamicPointLights].Y *= 16.0f;
					LightColorAndFalloffExponent[NumDynamicPointLights].Z *= 16.0f;
					LightColorAndFalloffExponent[NumDynamicPointLights].W = 0;
				}

				NumDynamicPointLights++;
			}
		}
	}
}

/** The action used to draw a base pass static mesh element. */
class FDrawBasePassForwardShadingStaticMeshAction
{
public:

	FScene* Scene;
	FStaticMesh* StaticMesh;

	/** Initialization constructor. */
	FDrawBasePassForwardShadingStaticMeshAction(FScene* InScene,FStaticMesh* InStaticMesh):
		Scene(InScene),
		StaticMesh(InStaticMesh)
	{}

	inline bool ShouldPackAmbientSH() const
	{
		return false;
	}

	const FLightSceneInfo* GetSimpleDirectionalLight() const 
	{ 
		return Scene->SimpleDirectionalLight;
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
		FScene::EBasePassDrawListType DrawType = FScene::EBasePass_Default;		
 
		if (StaticMesh->IsMasked(Parameters.FeatureLevel))
		{
			DrawType = FScene::EBasePass_Masked;	
		}

		if ( Scene )
		{
			// Find the appropriate draw list for the static mesh based on the light-map policy type.
			TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType, 0> >& DrawList =
				Scene->GetForwardShadingBasePassDrawList<LightMapPolicyType>(DrawType);

			ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();
			// Add the static mesh to the draw list.
			DrawList.AddMesh(
				StaticMesh,
				typename TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType, 0>::ElementDataType(LightMapElementData),
				TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType, 0>(
				StaticMesh->VertexFactory,
				StaticMesh->MaterialRenderProxy,
				*Parameters.Material,
				LightMapPolicy,
				Parameters.BlendMode,
				Parameters.TextureMode,
				Parameters.ShadingModel != MSM_Unlit && Scene->ShouldRenderSkylight(),
				false,
				FeatureLevel,
				Parameters.bEditorCompositeDepthTest
				),
				FeatureLevel
				);
		}
	}
};

void FBasePassForwardOpaqueDrawingPolicyFactory::AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh)
{
	// Determine the mesh's material and blend mode.
	const auto FeatureLevel = Scene->GetFeatureLevel();
	const FMaterial* Material = StaticMesh->MaterialRenderProxy->GetMaterial(FeatureLevel);
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Don't composite static meshes
	const bool bEditorCompositeDepthTest = false;

	// Only draw opaque materials.
	if( !IsTranslucentBlendMode(BlendMode) )
	{
		// following check moved from ProcessBasePassMeshForForwardShading to avoid passing feature level.
		check(!AllowHighQualityLightmaps(Scene->GetFeatureLevel()));

		ProcessBasePassMeshForForwardShading<FDrawBasePassForwardShadingStaticMeshAction, 0>(
			RHICmdList,
			FProcessBasePassMeshParameters(
				*StaticMesh,
				Material,
				StaticMesh->PrimitiveSceneInfo->Proxy,
				true,
				bEditorCompositeDepthTest,
				ESceneRenderTargetsMode::DontSet,
				FeatureLevel
				),
			FDrawBasePassForwardShadingStaticMeshAction(Scene,StaticMesh)
			);
	}
}

/** The action used to draw a base pass dynamic mesh element. */
class FDrawBasePassForwardShadingDynamicMeshAction
{
public:

	const FViewInfo& View;
	bool bBackFace;
	float DitheredLODTransitionValue;
	FHitProxyId HitProxyId;

	inline bool ShouldPackAmbientSH() const
	{
		return false;
	}

	const FLightSceneInfo* GetSimpleDirectionalLight() const 
	{
		auto* Scene = (FScene*)View.Family->Scene;
		return Scene ? Scene->SimpleDirectionalLight : nullptr;
	}

	/** Initialization constructor. */
	FDrawBasePassForwardShadingDynamicMeshAction(
		const FViewInfo& InView,
		const bool bInBackFace,
		float InDitheredLODTransitionValue,
		const FHitProxyId InHitProxyId
		)
		: View(InView)
		, bBackFace(bInBackFace)
		, DitheredLODTransitionValue(InDitheredLODTransitionValue)
		, HitProxyId(InHitProxyId)
	{}

	/** Draws the translucent mesh with a specific light-map type, and shader complexity predicate. */
	template<typename LightMapPolicyType, int32 NumDynamicPointLights>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		) const
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Treat masked materials as if they don't occlude in shader complexity, which is PVR behavior
		if(Parameters.BlendMode == BLEND_Masked && View.Family->EngineShowFlags.ShaderComplexity)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());
		}
#endif

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
			View.GetFeatureLevel(),
			Parameters.bEditorCompositeDepthTest
			);
		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, typename TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType, NumDynamicPointLights>::ContextDataType());

		for( int32 BatchElementIndex=0;BatchElementIndex<Parameters.Mesh.Elements.Num();BatchElementIndex++ )
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

		// Restore
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if(Parameters.BlendMode == BLEND_Masked && View.Family->EngineShowFlags.ShaderComplexity)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true,CF_DepthNearOrEqual>::GetRHI());
		}
#endif
	}
};

template<int32 NumDynamicPointLights>
void FBasePassForwardOpaqueDrawingPolicyFactory::DrawDynamicMeshTempl(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	const FMaterial* Material,
	bool bBackFace,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	ProcessBasePassMeshForForwardShading<FDrawBasePassForwardShadingDynamicMeshAction, NumDynamicPointLights>(
		RHICmdList, 
		FProcessBasePassMeshParameters(
			Mesh, 
			Material, 
			PrimitiveSceneProxy, 
			true, 
			DrawingContext.bEditorCompositeDepthTest, 
			DrawingContext.TextureMode, 
			View.GetFeatureLevel()
		), 
		FDrawBasePassForwardShadingDynamicMeshAction(
			View, 
			bBackFace, 
			Mesh.DitheredLODTransitionAlpha, 
			HitProxyId														
		)																
	);
}

bool FBasePassForwardOpaqueDrawingPolicyFactory::DrawDynamicMesh(
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
	// Determine the mesh's material and blend mode.
	const auto FeatureLevel = View.GetFeatureLevel();
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel);
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque materials.
	if(!IsTranslucentBlendMode(BlendMode))
	{
		static auto* MobileNumDynamicPointLightsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileNumDynamicPointLights"));
		const int32 MobileNumDynamicPointLights = MobileNumDynamicPointLightsCVar->GetValueOnRenderThread();

		int32 NumDynamicPointLights = PrimitiveSceneProxy ? FMath::Min<int32>(PrimitiveSceneProxy->GetPrimitiveSceneInfo()->NumES2DynamicPointLights, MobileNumDynamicPointLights) : 0;
		const bool bIsUnlit = Material->GetShadingModel() == MSM_Unlit;

		if (NumDynamicPointLights == 0 || bIsUnlit)
		{
			DrawDynamicMeshTempl<0>(RHICmdList, View, DrawingContext, Mesh, Material, bBackFace, PrimitiveSceneProxy, HitProxyId);
		}
		else
		{
			static auto* MobileDynamicPointLightsUseStaticBranchCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileDynamicPointLightsUseStaticBranch"));
			if (MobileDynamicPointLightsUseStaticBranchCVar->GetValueOnRenderThread() == 1)
			{
				DrawDynamicMeshTempl<INT32_MAX>(RHICmdList, View, DrawingContext, Mesh, Material, bBackFace, PrimitiveSceneProxy, HitProxyId);
			}
			else
			{
				static_assert(MAX_BASEPASS_DYNAMIC_POINT_LIGHTS == 4, "If you change MAX_BASEPASS_DYNAMIC_POINT_LIGHTS, you need to change the switch statement below");

				switch (NumDynamicPointLights)
				{
				case 1:
					DrawDynamicMeshTempl<1>(RHICmdList, View, DrawingContext, Mesh, Material, bBackFace, PrimitiveSceneProxy, HitProxyId);
					break;
				case 2:
					DrawDynamicMeshTempl<2>(RHICmdList, View, DrawingContext, Mesh, Material, bBackFace, PrimitiveSceneProxy, HitProxyId);
					break;
				case 3:
					DrawDynamicMeshTempl<3>(RHICmdList, View, DrawingContext, Mesh, Material, bBackFace, PrimitiveSceneProxy, HitProxyId);
					break;
				default:
					DrawDynamicMeshTempl<4>(RHICmdList, View, DrawingContext, Mesh, Material, bBackFace, PrimitiveSceneProxy, HitProxyId);
					break;
				}
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}

/** Base pass sorting modes. */
namespace EBasePassSort
{
	enum Type
	{
		/** Automatically select based on hardware/platform. */
		Auto = 0,
		/** No sorting. */
		None = 1,
		/** Sorts state buckets, not individual meshes. */
		SortStateBuckets = 2,
		/** Per mesh sorting. */
		SortPerMesh = 3,

		/** Useful range of sort modes. */
		FirstForcedMode = None,
		LastForcedMode = SortPerMesh
	};
};
TAutoConsoleVariable<int32> GSortBasePass(TEXT("r.ForwardBasePassSort"),0,
	TEXT("How to sort the forward shading base pass:\n")
	TEXT("\t0: Decide automatically based on the hardware.\n")
	TEXT("\t1: Sort drawing policies.\n")
	TEXT("\t2: Sort drawing policies and the meshes within them."),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> GMaxBasePassDraws(TEXT("r.MaxForwardBasePassDraws"),0,TEXT("Stops rendering static forward base pass draws after the specified number of times. Useful for seeing the order in which meshes render when optimizing."),ECVF_RenderThreadSafe);

EBasePassSort::Type GetSortMode()
{
	int32 SortMode = GSortBasePass.GetValueOnRenderThread();
	if (SortMode >= EBasePassSort::FirstForcedMode && SortMode <= EBasePassSort::LastForcedMode)
	{
		return (EBasePassSort::Type)SortMode;
	}

	// Determine automatically.
	if (GHardwareHiddenSurfaceRemoval)
	{
		return EBasePassSort::None;
	}
	else
	{
		return EBasePassSort::SortPerMesh;
	}
}

void FForwardShadingSceneRenderer::RenderForwardShadingBasePass(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, BasePass);
	SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);

	EBasePassSort::Type SortMode = GetSortMode();
	int32 MaxDraws = GMaxBasePassDraws.GetValueOnRenderThread();
	if (MaxDraws <= 0)
	{
		MaxDraws = MAX_int32;
	}

	if (SortMode == EBasePassSort::SortStateBuckets)
	{
		SCOPE_CYCLE_COUNTER(STAT_SortStaticDrawLists);

		for (int32 DrawType = 0; DrawType < FScene::EBasePass_MAX; DrawType++)
		{
			Scene->BasePassForForwardShadingNoLightMapDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingLowQualityLightMapDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalIndirectDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalCSMIndirectDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingMovableDirectionalLightDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingMovableDirectionalLightCSMDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingMovableDirectionalLightLightmapDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
		}
	}

	// Draw the scene's emissive and light-map color.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
		FViewInfo& View = Views[ViewIndex];

		// Opaque blending
		RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

		{
			// Render the base pass static data
			if (SortMode == EBasePassSort::SortPerMesh)
			{
				SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
				MaxDraws -= Scene->BasePassForForwardShadingNoLightMapDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingLowQualityLightMapDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalIndirectDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalCSMIndirectDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingMovableDirectionalLightDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingMovableDirectionalLightCSMDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingMovableDirectionalLightLightmapDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				MaxDraws -= Scene->BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList[FScene::EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
			}
			else
			{
				SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
				Scene->BasePassForForwardShadingNoLightMapDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingLowQualityLightMapDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalIndirectDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalCSMIndirectDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingMovableDirectionalLightDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingMovableDirectionalLightCSMDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingMovableDirectionalLightLightmapDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				Scene->BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList[FScene::EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
			}
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_DynamicPrimitiveDrawTime);
			SCOPED_DRAW_EVENT(RHICmdList, Dynamic);

			FBasePassForwardOpaqueDrawingPolicyFactory::ContextType Context(false, ESceneRenderTargetsMode::DontSet);
			
			int32 MeshBatchIndex = 0;
			int32 MeshBatchNum = View.DynamicMeshElements.Num();
			// If the scene has decals we need to partition the draw list into two groups: 
			// those that receive decals and those that don't, and render the latter with stencil writes enabled
			if (Scene->Decals.Num() > 0)
			{
				MeshBatchNum = Algo::Partition(View.DynamicMeshElements.GetData(), MeshBatchNum, [](const FMeshBatchAndRelevance& El) { 
					return El.PrimitiveSceneProxy->ReceivesDecals(); 
				});
			}

			for (; MeshBatchIndex < MeshBatchNum; MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				if (MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial || ViewFamily.EngineShowFlags.Wireframe)
				{
					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
					FBasePassForwardOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
				}
			}

			if (MeshBatchNum < View.DynamicMeshElements.Num())
			{
				// Primitives without decals are rendered with stencil enabled
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), 0x80);
				MeshBatchNum = View.DynamicMeshElements.Num();

				for (; MeshBatchIndex < MeshBatchNum; MeshBatchIndex++)
				{
					const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

					if (MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial || ViewFamily.EngineShowFlags.Wireframe)
					{
						const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
						FBasePassForwardOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
					}
				}
				// Restore depthstencil state
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
			}

			View.SimpleElementCollector.DrawBatchedElements(RHICmdList, View, NULL, EBlendModeFilter::OpaqueAndMasked);

			if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
			{
				const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[FeatureLevel]) && !IsMobileHDR();

				// Draw the base pass for the view's batched mesh elements.
				DrawViewElements<FBasePassForwardOpaqueDrawingPolicyFactory>(RHICmdList, View, FBasePassForwardOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), SDPG_World, true);

				// Draw the view's batched simple elements(lines, sprites, etc).
				View.BatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false);

				// Draw foreground objects last
				DrawViewElements<FBasePassForwardOpaqueDrawingPolicyFactory>(RHICmdList, View, FBasePassForwardOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), SDPG_Foreground, true);

				// Draw the view's batched simple elements(lines, sprites, etc).
				View.TopBatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false);
			}
		}

		// Issue static draw list masked draw calls last, as PVR wants it
		if (SortMode == EBasePassSort::SortPerMesh)
		{
			SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
			MaxDraws -= Scene->BasePassForForwardShadingNoLightMapDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility,MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingLowQualityLightMapDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility,MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility,MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility,MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalIndirectDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility,MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalCSMIndirectDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility,MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingMovableDirectionalLightDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility,MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingMovableDirectionalLightCSMDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingMovableDirectionalLightLightmapDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
			MaxDraws -= Scene->BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList[FScene::EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
		}
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
			Scene->BasePassForForwardShadingNoLightMapDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingLowQualityLightMapDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalIndirectDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingDirectionalLightAndSHDirectionalCSMIndirectDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingMovableDirectionalLightDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingMovableDirectionalLightCSMDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingMovableDirectionalLightLightmapDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
			Scene->BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList[FScene::EBasePass_Masked].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
		}
	}
}
