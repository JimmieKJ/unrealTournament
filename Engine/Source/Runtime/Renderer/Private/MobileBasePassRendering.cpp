// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileBasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "MaterialShaderQualitySettings.h"

#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_VERTEX_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TMobileBasePassVS< LightMapPolicyType, LDR_GAMMA_32 > TMobileBasePassVS##LightMapPolicyName##LDRGamma32; \
	typedef TMobileBasePassVS< LightMapPolicyType, HDR_LINEAR_64 > TMobileBasePassVS##LightMapPolicyName##HDRLinear64; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassVS##LightMapPolicyName##LDRGamma32, TEXT("MobileBasePassVertexShader"), TEXT("Main"), SF_Vertex); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassVS##LightMapPolicyName##HDRLinear64, TEXT("MobileBasePassVertexShader"), TEXT("Main"), SF_Vertex);

#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName,NumDynamicPointLights) \
	typedef TMobileBasePassPS< LightMapPolicyType, LDR_GAMMA_32, false, NumDynamicPointLights > TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##LDRGamma32; \
	typedef TMobileBasePassPS< LightMapPolicyType, HDR_LINEAR_32, false, NumDynamicPointLights > TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear32; \
	typedef TMobileBasePassPS< LightMapPolicyType, HDR_LINEAR_64, false, NumDynamicPointLights > TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear64; \
	typedef TMobileBasePassPS< LightMapPolicyType, LDR_GAMMA_32, true, NumDynamicPointLights > TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##LDRGamma32##Skylight; \
	typedef TMobileBasePassPS< LightMapPolicyType, HDR_LINEAR_32, true, NumDynamicPointLights > TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear32##Skylight; \
	typedef TMobileBasePassPS< LightMapPolicyType, HDR_LINEAR_64, true, NumDynamicPointLights > TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear64##Skylight; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##LDRGamma32, TEXT("MobileBasePassPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear32, TEXT("MobileBasePassPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear64, TEXT("MobileBasePassPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##LDRGamma32##Skylight, TEXT("MobileBasePassPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear32##Skylight, TEXT("MobileBasePassPixelShader"), TEXT("Main"), SF_Pixel); \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TMobileBasePassPS##LightMapPolicyName##NumDynamicPointLights##HDRLinear64##Skylight, TEXT("MobileBasePassPixelShader"), TEXT("Main"), SF_Pixel);

static_assert(MAX_BASEPASS_DYNAMIC_POINT_LIGHTS == 4, "If you change MAX_BASEPASS_DYNAMIC_POINT_LIGHTS, you need to add shader types below");

// Permutations for the number of point lights to support. INT32_MAX indicates the shader should use branching to support a variable number of point lights.
#define IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_VERTEX_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 0) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 1) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 2) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 3) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, 4) \
	IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_PIXEL_SHADER_TYPE(LightMapPolicyType, LightMapPolicyName, INT32_MAX)

// Implement shader types per lightmap policy 
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, TLightMapPolicyLQ);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP>, TDistanceFieldShadowsAndLightMapPolicyLQ);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM>, FDistanceFieldShadowsLightMapAndCSMLightingPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT>, FMobileDirectionalLightAndSHIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_DIRECTIONAL_INDIRECT>, FMobileDirectionalLightAndSHDirectionalIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_DIRECTIONAL_CSM_INDIRECT>, FMobileDirectionalLightAndSHDirectionalCSMIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_CSM_INDIRECT>, FMobileDirectionalLightAndSHCSMIndirectPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOVABLE_DIRECTIONAL_LIGHT>, FMovableDirectionalLightLightingPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOVABLE_DIRECTIONAL_LIGHT_CSM>, FMovableDirectionalLightCSMLightingPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP>, FMovableDirectionalLightWithLightmapLightingPolicy);
IMPLEMENT_MOBILE_SHADING_BASEPASS_LIGHTMAPPED_SHADER_TYPE(TUniformLightMapPolicy<LMP_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP>, FMovableDirectionalLightCSMWithLightmapLightingPolicy);

const FLightSceneInfo* GetSceneMobileDirectionalLights(FScene const* Scene, uint32 LightChannel)
{
	return Scene->MobileDirectionalLights[LightChannel];
}

template<typename PixelParametersType, int32 NumDynamicPointLights>
bool TMobileBasePassPSPolicyParamType<PixelParametersType, NumDynamicPointLights>::ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment)
{
	// Get quality settings for shader platform
	const UShaderPlatformQualitySettings* MaterialShadingQuality = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(Platform);
	const FMaterialQualityOverrides& QualityOverrides = MaterialShadingQuality->GetQualityOverrides(QualityLevel);

	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_FULLY_ROUGH"), QualityOverrides.bEnableOverride && QualityOverrides.bForceFullyRough != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_NONMETAL"), QualityOverrides.bEnableOverride && QualityOverrides.bForceNonMetal != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("QL_FORCEDISABLE_LM_DIRECTIONALITY"), QualityOverrides.bEnableOverride && QualityOverrides.bForceDisableLMDirectionality != 0 ? 1u : 0u);
	OutEnvironment.SetDefine(TEXT("MOBILE_QL_FORCE_LQ_REFLECTIONS"), QualityOverrides.bEnableOverride && QualityOverrides.bForceLQReflections != 0 ? 1u : 0u);
	
	return true;
}

static TAutoConsoleVariable<int32> CVarAllReceiveDynamicCSM(
	TEXT("r.AllReceiveDynamicCSM"),
	1,
	TEXT("Which primitives should receive dynamic-only CSM shadows. 0: Only primitives marked bReceiveCSMFromDynamicObjects. 1: All primitives (default)"));

FMobileBasePassDynamicPointLightInfo::FMobileBasePassDynamicPointLightInfo(const FPrimitiveSceneProxy* InSceneProxy)
: NumDynamicPointLights(0)
{
	static auto* MobileNumDynamicPointLightsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileNumDynamicPointLights"));
	const int32 MobileNumDynamicPointLights = MobileNumDynamicPointLightsCVar->GetValueOnRenderThread();

	if (InSceneProxy != nullptr)
	{
		for (FLightPrimitiveInteraction* LPI = InSceneProxy->GetPrimitiveSceneInfo()->LightList; LPI && NumDynamicPointLights < MobileNumDynamicPointLights; LPI = LPI->GetNextLight())
		{
			FLightSceneProxy* LightProxy = LPI->GetLight()->Proxy;
			if (LightProxy->GetLightType() == LightType_Point && LightProxy->IsMovable() && (LightProxy->GetLightingChannelMask() & InSceneProxy->GetLightingChannelMask()) != 0)
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
class FDrawMobileBasePassStaticMeshAction
{
public:

	FScene* Scene;
	FStaticMesh* StaticMesh;

	/** Initialization constructor. */
	FDrawMobileBasePassStaticMeshAction(FScene* InScene,FStaticMesh* InStaticMesh):
		Scene(InScene),
		StaticMesh(InStaticMesh)
	{}

	inline bool ShouldPackAmbientSH() const
	{
		return false;
	}

	bool CanUseDrawlistToToggleCombinedStaticAndCSM(const FPrimitiveSceneProxy* PrimitiveSceneProxy) const
	{
		static auto* CVarMobileEnableStaticAndCSMShadowReceivers = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableStaticAndCSMShadowReceivers"));
		// Ideally we would also check for 'r.AllReceiveDynamicCSM' || PrimitiveSceneProxy->ShouldReceiveCombinedCSMAndStaticShadowsFromStationaryLights().
		// It's being omitted here to avoid requiring a drawlist rebuild whenever the cvar is toggled.
		return CVarMobileEnableStaticAndCSMShadowReceivers->GetValueOnRenderThread() != 0;
	}

	bool CanReceiveStaticAndCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy) const
	{
		// CSM use for static meshes is determined during InitDynamicShadows.
		return false; 
	}

	const FScene* GetScene() const 
	{ 
		return Scene;
	}

	/** Draws the mesh with a specific light-map type */
	template<int32 NumDynamicPointLights>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const FUniformLightMapPolicy& LightMapPolicy,
		const FUniformLightMapPolicy::ElementDataType& LightMapElementData
		) const
	{
		EBasePassDrawListType DrawType = EBasePass_Default;

		if (StaticMesh->IsMasked(Parameters.FeatureLevel))
		{
			DrawType = EBasePass_Masked;	
		}

		if ( Scene )
		{
			// Determine if this primitive has the possibility of using combined static and CSM.
			if (CanUseDrawlistToToggleCombinedStaticAndCSM(Parameters.PrimitiveSceneProxy))
			{
				// if applicable, returns the corresponding CSM or non-CSM lightmap policy of LightMapPolicyType
				auto GetAlternativeLightMapPolicy = [](ELightMapPolicyType LightMapPolicyType)
				{
					switch (LightMapPolicyType)
					{
						case LMP_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
							return LMP_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP;
						case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_CSM_INDIRECT:
							return LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT;
						case LMP_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
							return LMP_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM;
						case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
							return LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_CSM_INDIRECT;
					}
					return LightMapPolicyType;
				};

				const ELightMapPolicyType AlternativeLightMapPolicy = GetAlternativeLightMapPolicy(LightMapPolicy.GetIndirectPolicy());
				const bool bHasCSMCounterpart = AlternativeLightMapPolicy != LightMapPolicy.GetIndirectPolicy();
				if (bHasCSMCounterpart)
				{
					// Is the passed in lightmap policy CSM capable or not
					const bool bIsCSMCapableLightPolicy = LightMapPolicy.GetIndirectPolicy() == LMP_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM
					|| LightMapPolicy.GetIndirectPolicy() == LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_CSM_INDIRECT;

					if (bIsCSMCapableLightPolicy)
					{
						// Alternative policy is the non-CSM version.
						AddMeshToStaticDrawList(Scene->GetMobileBasePassCSMDrawList<FUniformLightMapPolicy>(DrawType), Parameters, LightMapPolicy, LightMapElementData);
						AddMeshToStaticDrawList(Scene->GetMobileBasePassDrawList<FUniformLightMapPolicy>(DrawType), Parameters, FUniformLightMapPolicy(AlternativeLightMapPolicy), LightMapElementData);
					}
					else
					{
						// Alternative policy is the CSM version.
						AddMeshToStaticDrawList(Scene->GetMobileBasePassCSMDrawList<FUniformLightMapPolicy>(DrawType), Parameters, FUniformLightMapPolicy(AlternativeLightMapPolicy), LightMapElementData);
						AddMeshToStaticDrawList(Scene->GetMobileBasePassDrawList<FUniformLightMapPolicy>(DrawType), Parameters, LightMapPolicy, LightMapElementData);
					}

					return; // avoid adding to draw list twice.
				}
			}

			AddMeshToStaticDrawList(Scene->GetMobileBasePassDrawList<FUniformLightMapPolicy>(DrawType), Parameters, LightMapPolicy, LightMapElementData);
		}
	}

	template<typename LightMapPolicyType>
	void AddMeshToStaticDrawList(TStaticMeshDrawList<TMobileBasePassDrawingPolicy<LightMapPolicyType, 0>> &DrawList, 
		const FProcessBasePassMeshParameters &Parameters, const LightMapPolicyType& LightMapPolicy, const typename LightMapPolicyType::ElementDataType& LightMapElementData) const
	{
		ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();
		// Add the static mesh to the draw list.
		DrawList.AddMesh(
			StaticMesh,
			typename TMobileBasePassDrawingPolicy<LightMapPolicyType, 0>::ElementDataType(LightMapElementData),
			TMobileBasePassDrawingPolicy<LightMapPolicyType, 0>(
				StaticMesh->VertexFactory,
				StaticMesh->MaterialRenderProxy,
				*Parameters.Material,
				LightMapPolicy,
				Parameters.BlendMode,
				Parameters.TextureMode,
				Parameters.ShadingModel != MSM_Unlit && Scene->ShouldRenderSkylight(Parameters.BlendMode),
				DVSM_None,
				FeatureLevel,
				Parameters.bEditorCompositeDepthTest,
				IsMobileHDR() // bEnableReceiveDecalOutput
				),
			FeatureLevel
		);
	}
};

void FMobileBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh)
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
		// following check moved from ProcessMobileBasePassMesh to avoid passing feature level.
		check(!AllowHighQualityLightmaps(Scene->GetFeatureLevel()));

		ProcessMobileBasePassMesh<FDrawMobileBasePassStaticMeshAction, 0>(
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
			FDrawMobileBasePassStaticMeshAction(Scene,StaticMesh)
			);
	}
}

/** The action used to draw a base pass dynamic mesh element. */
class FDrawMobileBasePassDynamicMeshAction
{
public:

	const FViewInfo& View;
	bool bBackFace;
	FMeshDrawingRenderState DrawRenderState;
	FHitProxyId HitProxyId;

	inline bool ShouldPackAmbientSH() const
	{
		return false;
	}

	bool CanReceiveStaticAndCSM(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy) const
	{
		if (PrimitiveSceneProxy == nullptr || LightSceneInfo == nullptr)
		{
			return false;
		}

		// Check that this primitive is eligible for CSM.
 		const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
		static auto* ConsoleVarAllReceiveDynamicCSM = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllReceiveDynamicCSM"));
		const bool bShouldReceiveCombinedCSMAndStaticShadows = PrimitiveSceneProxy->ShouldReceiveCombinedCSMAndStaticShadowsFromStationaryLights() || ConsoleVarAllReceiveDynamicCSM->GetValueOnRenderThread();
		return View.MobileCSMVisibilityInfo.bMobileDynamicCSMInUse && bShouldReceiveCombinedCSMAndStaticShadows;
	}

	const FScene* GetScene() const
	{
		auto* Scene = (FScene*)View.Family->Scene;
		return Scene;
	}

	/** Initialization constructor. */
	FDrawMobileBasePassDynamicMeshAction(
		const FViewInfo& InView,
		const bool bInBackFace,
		FMeshDrawingRenderState InDrawRenderState,
		const FHitProxyId InHitProxyId
		)
		: View(InView)
		, bBackFace(bInBackFace)
		, DrawRenderState(InDrawRenderState)
		, HitProxyId(InHitProxyId)
	{}

	/** Draws the translucent mesh with a specific light-map type, and shader complexity predicate. */
	template<int32 NumDynamicPointLights>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const FUniformLightMapPolicy& LightMapPolicy,
		const typename FUniformLightMapPolicy::ElementDataType& LightMapElementData
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

		TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, NumDynamicPointLights> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			LightMapPolicy,
			Parameters.BlendMode,
			Parameters.TextureMode,
			Parameters.ShadingModel != MSM_Unlit && Scene && Scene->ShouldRenderSkylight(Parameters.BlendMode),
			View.Family->GetDebugViewShaderMode(),
			View.GetFeatureLevel(),
			Parameters.bEditorCompositeDepthTest,
			IsMobileHDR() // bEnableReceiveDecalOutput
			);
		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, typename TMobileBasePassDrawingPolicy<FUniformLightMapPolicy, NumDynamicPointLights>::ContextDataType());

		for( int32 BatchElementIndex=0;BatchElementIndex<Parameters.Mesh.Elements.Num();BatchElementIndex++ )
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
void FMobileBasePassOpaqueDrawingPolicyFactory::DrawDynamicMeshTempl(
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
	ProcessMobileBasePassMesh<FDrawMobileBasePassDynamicMeshAction, NumDynamicPointLights>(
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
		FDrawMobileBasePassDynamicMeshAction(
			View, 
			bBackFace, 
			FMeshDrawingRenderState(Mesh.DitheredLODTransitionAlpha), 
			HitProxyId														
		)																
	);
}

bool FMobileBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(
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
	TEXT("How to sort the mobile base pass:\n")
	TEXT("\t0: Decide automatically based on the hardware.\n")
	TEXT("\t1: Sort drawing policies.\n")
	TEXT("\t2: Sort drawing policies and the meshes within them."),
	ECVF_RenderThreadSafe);
TAutoConsoleVariable<int32> GMaxBasePassDraws(TEXT("r.MaxForwardBasePassDraws"),0,TEXT("Stops rendering static mobile base pass draws after the specified number of times. Useful for seeing the order in which meshes render when optimizing."),ECVF_RenderThreadSafe);

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

void FMobileSceneRenderer::RenderMobileBasePass(FRHICommandListImmediate& RHICmdList)
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

		for (int32 DrawType = 0; DrawType < EBasePass_MAX; DrawType++)
		{
			Scene->MobileBasePassUniformLightMapPolicyDrawList[DrawType].SortFrontToBack(Views[0].ViewLocation);
			Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[DrawType].SortFrontToBack(Views[0].ViewLocation);
		}
	}

	// Draw the scene's emissive and light-map color.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
		FViewInfo& View = Views[ViewIndex];
		const FMobileCSMVisibilityInfo* MobileCSMVisibilityInfo = View.MobileCSMVisibilityInfo.bMobileDynamicCSMInUse ? &View.MobileCSMVisibilityInfo : nullptr;

		// Opaque blending
		if (View.bIsPlanarReflection)
		{
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_Zero, BF_Zero>::GetRHI());
		}
		else
		{
			RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
		}

		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

		{
			// Render the base pass static data
			if (SortMode == EBasePassSort::SortPerMesh)
			{
				SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
				if(MobileCSMVisibilityInfo)
				{
					MaxDraws -= Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility, MaxDraws);
					MaxDraws -= Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility, MaxDraws);
				}
				else
				{
					MaxDraws -= Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Default].DrawVisibleFrontToBack(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility, MaxDraws);
				}
			}
			else
			{
				SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
				if (MobileCSMVisibilityInfo)
				{
					Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Default].DrawVisible(RHICmdList, View, MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility);
					Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[EBasePass_Default].DrawVisible(RHICmdList, View, MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility);
				}
				else
				{
					Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Default].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
				}

			}
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_DynamicPrimitiveDrawTime);
			SCOPED_DRAW_EVENT(RHICmdList, Dynamic);

			FMobileBasePassOpaqueDrawingPolicyFactory::ContextType Context(false, ESceneRenderTargetsMode::DontSet);
			
			for (const FMeshBatchAndRelevance& MeshBatchAndRelevance : View.DynamicMeshElements)
			{
				if (MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial() || ViewFamily.EngineShowFlags.Wireframe)
				{
					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
					FMobileBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
				}
			}

			View.SimpleElementCollector.DrawBatchedElements(RHICmdList, View, NULL, EBlendModeFilter::OpaqueAndMasked);

			if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
			{
				const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[FeatureLevel]) && !IsMobileHDR();

				// Draw the base pass for the view's batched mesh elements.
				DrawViewElements<FMobileBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, FMobileBasePassOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), SDPG_World, true);

				// Draw the view's batched simple elements(lines, sprites, etc).
				View.BatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false);

				// Draw foreground objects last
				DrawViewElements<FMobileBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, FMobileBasePassOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), SDPG_Foreground, true);

				// Draw the view's batched simple elements(lines, sprites, etc).
				View.TopBatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false);
			}
		}

		// Issue static draw list masked draw calls last, as PVR wants it
		if (SortMode == EBasePassSort::SortPerMesh)
		{
			SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
			if (MobileCSMVisibilityInfo)
			{
				MaxDraws -= Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View, MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility, MaxDraws);
				MaxDraws -= Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View, MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility, MaxDraws);
			}
			else
			{
				MaxDraws -= Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Masked].DrawVisibleFrontToBack(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility,MaxDraws);
			}
		}
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);
			if (MobileCSMVisibilityInfo)
			{
				Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Masked].DrawVisible(RHICmdList, View, MobileCSMVisibilityInfo->MobileNonCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileNonCSMStaticBatchVisibility);
				Scene->MobileBasePassUniformLightMapPolicyDrawListWithCSM[EBasePass_Masked].DrawVisible(RHICmdList, View, MobileCSMVisibilityInfo->MobileCSMStaticMeshVisibilityMap, MobileCSMVisibilityInfo->MobileCSMStaticBatchVisibility);
			}
			else
			{
				Scene->MobileBasePassUniformLightMapPolicyDrawList[EBasePass_Masked].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			}
		}
	}
}
