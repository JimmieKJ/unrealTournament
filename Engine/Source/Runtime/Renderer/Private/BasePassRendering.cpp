// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.cpp: Base pass rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"

/** Whether to replace lightmap textures with solid colors to visualize the mip-levels. */
bool GVisualizeMipLevels = false;

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
// BasePass Vertex Shader needs to include hull and domain shaders for tessellation, these only compile for D3D11
#define IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TBasePassVS< LightMapPolicyType, false > TBasePassVS##LightMapPolicyName ; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassVS##LightMapPolicyName,TEXT("BasePassVertexShader"),TEXT("Main"),SF_Vertex); \
	typedef TBasePassHS< LightMapPolicyType > TBasePassHS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassHS##LightMapPolicyName,TEXT("BasePassTessellationShaders"),TEXT("MainHull"),SF_Hull); \
	typedef TBasePassDS< LightMapPolicyType > TBasePassDS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassDS##LightMapPolicyName,TEXT("BasePassTessellationShaders"),TEXT("MainDomain"),SF_Domain); 

#define IMPLEMENT_BASEPASS_VERTEXSHADER_ONLY_TYPE(LightMapPolicyType,LightMapPolicyName,AtmosphericFogShaderName) \
	typedef TBasePassVS<LightMapPolicyType,true> TBasePassVS##LightMapPolicyName##AtmosphericFogShaderName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassVS##LightMapPolicyName##AtmosphericFogShaderName,TEXT("BasePassVertexShader"),TEXT("Main"),SF_Vertex)

#define IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,bEnableSkyLight,SkyLightName) \
	typedef TBasePassPS<LightMapPolicyType, bEnableSkyLight> TBasePassPS##LightMapPolicyName##SkyLightName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TBasePassPS##LightMapPolicyName##SkyLightName,TEXT("BasePassPixelShader"),TEXT("Main"),SF_Pixel);

// Implement a pixel shader type for skylights and one without, and one vertex shader that will be shared between them
#define IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_BASEPASS_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_BASEPASS_VERTEXSHADER_ONLY_TYPE(LightMapPolicyType,LightMapPolicyName,AtmosphericFog) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,true,Skylight) \
	IMPLEMENT_BASEPASS_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName,false,);

// Implement shader types per lightmap policy
// If renaming or refactoring these, remember to update FMaterialResource::GetRepresentativeInstructionCounts and FPreviewMaterial::ShouldCache().
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FNoLightMapPolicy, FNoLightMapPolicy ); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TLightMapPolicy<HQ_LIGHTMAP>, TLightMapPolicyHQ ); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TLightMapPolicy<LQ_LIGHTMAP>, TLightMapPolicyLQ ); 
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP>, TDistanceFieldShadowsAndLightMapPolicyHQ );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSelfShadowedTranslucencyPolicy, FSelfShadowedTranslucencyPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSelfShadowedCachedPointIndirectLightingPolicy, FSelfShadowedCachedPointIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FCachedVolumeIndirectLightingPolicy, FCachedVolumeIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FCachedPointIndirectLightingPolicy, FCachedPointIndirectLightingPolicy );
IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE( FSimpleDynamicLightingPolicy, FSimpleDynamicLightingPolicy );

void FSkyLightReflectionParameters::GetSkyParametersFromScene(const FScene* Scene, bool bApplySkyLight, FTexture*& OutSkyLightTextureResource, float& OutApplySkyLightMask, float& OutSkyMipCount, bool& bOutSkyLightIsDynamic)
{
	OutSkyLightTextureResource = GBlackTextureCube;
	OutApplySkyLightMask = 0;
	bOutSkyLightIsDynamic = false;

	if (Scene
		&& Scene->SkyLight 
		&& Scene->SkyLight->ProcessedTexture
		&& bApplySkyLight)
	{
		OutSkyLightTextureResource = Scene->SkyLight->ProcessedTexture;
		OutApplySkyLightMask = 1;
		bOutSkyLightIsDynamic = !Scene->SkyLight->bHasStaticLighting && !Scene->SkyLight->bWantsStaticShadowing;
	}

	OutSkyMipCount = 1;

	if (OutSkyLightTextureResource)
	{
		int32 CubemapWidth = OutSkyLightTextureResource->GetSizeX();
		OutSkyMipCount = FMath::Log2(CubemapWidth) + 1.0f;
	}
}

void FTranslucentLightingParameters::Set(FRHICommandList& RHICmdList, FShader* Shader, const FViewInfo* View)
{
	SetTextureParameter(
		RHICmdList, 
		Shader->GetPixelShader(), 
		TranslucencyLightingVolumeAmbientInner, 
		TranslucencyLightingVolumeAmbientInnerSampler, 
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
		GSceneRenderTargets.GetTranslucencyVolumeAmbient(TVC_Inner)->GetRenderTargetItem().ShaderResourceTexture);

	SetTextureParameter(
		RHICmdList, 
		Shader->GetPixelShader(), 
		TranslucencyLightingVolumeAmbientOuter, 
		TranslucencyLightingVolumeAmbientOuterSampler, 
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
		GSceneRenderTargets.GetTranslucencyVolumeAmbient(TVC_Outer)->GetRenderTargetItem().ShaderResourceTexture);

	SetTextureParameter(
		RHICmdList, 
		Shader->GetPixelShader(), 
		TranslucencyLightingVolumeDirectionalInner, 
		TranslucencyLightingVolumeDirectionalInnerSampler, 
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
		GSceneRenderTargets.GetTranslucencyVolumeDirectional(TVC_Inner)->GetRenderTargetItem().ShaderResourceTexture);

	SetTextureParameter(
		RHICmdList, 
		Shader->GetPixelShader(), 
		TranslucencyLightingVolumeDirectionalOuter, 
		TranslucencyLightingVolumeDirectionalOuterSampler, 
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
		GSceneRenderTargets.GetTranslucencyVolumeDirectional(TVC_Outer)->GetRenderTargetItem().ShaderResourceTexture);

	SkyLightReflectionParameters.SetParameters(RHICmdList, Shader->GetPixelShader(), (const FScene*)(View->Family->Scene), true);

	if (View->HZB)
	{
		SetTextureParameter(
			RHICmdList, 
			Shader->GetPixelShader(), 
			HZBTexture, 
			HZBSampler, 
			TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			View->HZB->GetRenderTargetItem().ShaderResourceTexture );

		TRefCountPtr<IPooledRenderTarget>* PrevSceneColorRT = &GSystemTextures.BlackDummy;

		FSceneViewState* ViewState = (FSceneViewState*)View->State;
		if( ViewState && ViewState->TemporalAAHistoryRT && !View->bCameraCut )
		{
			PrevSceneColorRT = &ViewState->TemporalAAHistoryRT;
		}

		SetTextureParameter(
			RHICmdList, 
			Shader->GetPixelShader(), 
			PrevSceneColor, 
			PrevSceneColorSampler, 
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			(*PrevSceneColorRT)->GetRenderTargetItem().ShaderResourceTexture );
	}
}

void FTranslucentLightingParameters::SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FPrimitiveSceneProxy* Proxy, ERHIFeatureLevel::Type FeatureLevel)
{
	// Note: GBlackCubeArrayTexture has an alpha of 0, which is needed to represent invalid data so the sky cubemap can still be applied
	FTextureRHIParamRef CubeArrayTexture = FeatureLevel >= ERHIFeatureLevel::SM5 ? GBlackCubeArrayTexture->TextureRHI : GBlackTextureCube->TextureRHI;
	int32 ArrayIndex = 0;
	const FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy ? Proxy->GetPrimitiveSceneInfo() : NULL;

	if (PrimitiveSceneInfo && PrimitiveSceneInfo->CachedReflectionCaptureProxy)
	{
		PrimitiveSceneInfo->Scene->GetCaptureParameters(PrimitiveSceneInfo->CachedReflectionCaptureProxy, CubeArrayTexture, ArrayIndex);
	}

	SetTextureParameter(
		RHICmdList, 
		Shader->GetPixelShader(), 
		ReflectionCubemap, 
		ReflectionCubemapSampler, 
		TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
		CubeArrayTexture);

	SetShaderValue(RHICmdList, Shader->GetPixelShader(), CubemapArrayIndex, ArrayIndex);
}

/** The action used to draw a base pass static mesh element. */
class FDrawBasePassStaticMeshAction
{
public:

	FScene* Scene;
	FStaticMesh* StaticMesh;

	/** Initialization constructor. */
	FDrawBasePassStaticMeshAction(FScene* InScene,FStaticMesh* InStaticMesh):
		Scene(InScene),
		StaticMesh(InStaticMesh)
	{}

	bool UseTranslucentSelfShadowing() const { return false; }
	const FProjectedShadowInfo* GetTranslucentSelfShadow() const { return NULL; }

	bool AllowIndirectLightingCache() const 
	{ 
		return true; 
	}

	bool AllowIndirectLightingCacheVolumeTexture() const
	{
		return true;
	}

	/** Draws the translucent mesh with a specific light-map type, and fog volume type */
	template<typename LightMapPolicyType>
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

		if (Scene)
		{

			// Find the appropriate draw list for the static mesh based on the light-map policy type.
			TStaticMeshDrawList<TBasePassDrawingPolicy<LightMapPolicyType> >& DrawList =
				Scene->GetBasePassDrawList<LightMapPolicyType>(DrawType);

			// Add the static mesh to the draw list.
			DrawList.AddMesh(
				StaticMesh,
				typename TBasePassDrawingPolicy<LightMapPolicyType>::ElementDataType(LightMapElementData),
				TBasePassDrawingPolicy<LightMapPolicyType>(
				StaticMesh->VertexFactory,
				StaticMesh->MaterialRenderProxy,
				*Parameters.Material,
				Parameters.FeatureLevel,
				LightMapPolicy,
				Parameters.BlendMode,
				Parameters.TextureMode,
				Parameters.ShadingModel != MSM_Unlit && Scene->SkyLight && Scene->SkyLight->bWantsStaticShadowing && !Scene->SkyLight->bHasStaticLighting,
				IsTranslucentBlendMode(Parameters.BlendMode) && Scene->HasAtmosphericFog()
				),
				Scene->GetFeatureLevel()
				);
		}
	}
};

void FBasePassOpaqueDrawingPolicyFactory::AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh)
{
	// Determine the mesh's material and blend mode.
	const FMaterial* Material = StaticMesh->MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Don't composite static meshes
	const bool bEditorCompositeDepthTest = false;

	// Only draw opaque materials.
	if( !IsTranslucentBlendMode(BlendMode) )
	{
		ProcessBasePassMesh(
			RHICmdList, 
			FProcessBasePassMeshParameters(
				*StaticMesh,
				Material,
				StaticMesh->PrimitiveSceneInfo->Proxy,
				false,
				bEditorCompositeDepthTest,
				ESceneRenderTargetsMode::DontSet,
				Scene->GetFeatureLevel()),
			FDrawBasePassStaticMeshAction(Scene,StaticMesh)
			);
	}
}

/** The action used to draw a base pass dynamic mesh element. */
class FDrawBasePassDynamicMeshAction
{
public:

	const FViewInfo& View;
	bool bBackFace;
	bool bPreFog;
	FHitProxyId HitProxyId;

	/** Initialization constructor. */
	FDrawBasePassDynamicMeshAction(
		const FViewInfo& InView,
		const bool bInBackFace,
		const FHitProxyId InHitProxyId
		)
		: View(InView)
		, bBackFace(bInBackFace)
		, HitProxyId(InHitProxyId)
	{}

	bool UseTranslucentSelfShadowing() const { return false; }
	const FProjectedShadowInfo* GetTranslucentSelfShadow() const { return NULL; }

	bool AllowIndirectLightingCache() const 
	{ 
		return View.Family->EngineShowFlags.IndirectLightingCache; 
	}

	bool AllowIndirectLightingCacheVolumeTexture() const
	{
		return true;
	}

	/** Draws the translucent mesh with a specific light-map type, and shader complexity predicate. */
	template<typename LightMapPolicyType>
	void Process(
		FRHICommandList& RHICmdList, 
		const FProcessBasePassMeshParameters& Parameters,
		const LightMapPolicyType& LightMapPolicy,
		const typename LightMapPolicyType::ElementDataType& LightMapElementData
		) const
	{
		const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// When rendering masked materials in the shader complexity viewmode, 
		// We want to overwrite complexity for the pixels which get depths written,
		// And accumulate complexity for pixels which get killed due to the opacity mask being below the clip value.
		// This is accomplished by forcing the masked materials to render depths in the depth only pass, 
		// Then rendering in the base pass with additive complexity blending, depth tests on, and depth writes off.
		if(View.Family->EngineShowFlags.ShaderComplexity)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());
		}
#endif
		const FScene* Scene = Parameters.PrimitiveSceneProxy ? Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->Scene : NULL;

		TBasePassDrawingPolicy<LightMapPolicyType> DrawingPolicy(
			Parameters.Mesh.VertexFactory,
			Parameters.Mesh.MaterialRenderProxy,
			*Parameters.Material,
			Parameters.FeatureLevel,
			LightMapPolicy,
			Parameters.BlendMode,
			Parameters.TextureMode,
			Scene && Scene->SkyLight && !Scene->SkyLight->bHasStaticLighting && Scene->SkyLight->bWantsStaticShadowing && bIsLitMaterial,
			IsTranslucentBlendMode(Parameters.BlendMode) && (Scene && Scene->HasAtmosphericFog()) && View.Family->EngineShowFlags.AtmosphericFog,
			View.Family->EngineShowFlags.ShaderComplexity,
			false,
			Parameters.bEditorCompositeDepthTest
			);
		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, typename TBasePassDrawingPolicy<LightMapPolicyType>::ContextDataType());

		for( int32 BatchElementIndex = 0, Num = Parameters.Mesh.Elements.Num(); BatchElementIndex < Num; BatchElementIndex++ )
		{
			DrawingPolicy.SetMeshRenderState(
				RHICmdList, 
				View,
				Parameters.PrimitiveSceneProxy,
				Parameters.Mesh,
				BatchElementIndex,
				bBackFace,
				typename TBasePassDrawingPolicy<LightMapPolicyType>::ElementDataType(LightMapElementData),
				typename TBasePassDrawingPolicy<LightMapPolicyType>::ContextDataType()
				);
			DrawingPolicy.DrawMesh(RHICmdList, Parameters.Mesh, BatchElementIndex);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if(View.Family->EngineShowFlags.ShaderComplexity)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true,CF_DepthNearOrEqual>::GetRHI());
		}
#endif
	}
};

bool FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(
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
	const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
	const EBlendMode BlendMode = Material->GetBlendMode();

	// Only draw opaque materials.
	if(!IsTranslucentBlendMode(BlendMode))
	{
		ProcessBasePassMesh(
			RHICmdList, 
			FProcessBasePassMeshParameters(
				Mesh,
				Material,
				PrimitiveSceneProxy,
				!bPreFog,
				DrawingContext.bEditorCompositeDepthTest,
				DrawingContext.TextureMode,
				View.GetFeatureLevel()
				),
			FDrawBasePassDynamicMeshAction(
				View,
				bBackFace,
				HitProxyId
				)
			);
		return true;
	}
	else
	{
		return false;
	}
}

void FCachedVolumeIndirectLightingPolicy::Set(
	FRHICommandList& RHICmdList, 
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FSceneView* View
	) const
{
	FNoLightMapPolicy::Set(RHICmdList, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, View);

	if (PixelShaderParameters)
	{
		FTextureRHIParamRef Texture0 = GBlackVolumeTexture->TextureRHI;
		FTextureRHIParamRef Texture1 = GBlackVolumeTexture->TextureRHI;
		FTextureRHIParamRef Texture2 = GBlackVolumeTexture->TextureRHI;

		FScene* Scene = (FScene*)View->Family->Scene;
		FIndirectLightingCache& LightingCache = Scene->IndirectLightingCache;

		// If we are using FCachedVolumeIndirectLightingPolicy then InitViews should have updated the lighting cache which would have initialized it
		// However the conditions for updating the lighting cache are complex and fail very occasionally in non-reproducible ways
		// Silently skipping setting the cache texture under failure for now
		if (LightingCache.IsInitialized())
		{
			Texture0 = LightingCache.GetTexture0().ShaderResourceTexture;
			Texture1 = LightingCache.GetTexture1().ShaderResourceTexture;
			Texture2 = LightingCache.GetTexture2().ShaderResourceTexture;
		}

		const FPixelShaderRHIParamRef PixelShaderRHI = PixelShader->GetPixelShader();

		SetTextureParameter(
			RHICmdList, 
			PixelShaderRHI,
			PixelShaderParameters->IndirectLightingCacheTexture0,
			PixelShaderParameters->IndirectLightingCacheSampler0,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			Texture0
			);

		SetTextureParameter(
			RHICmdList, 
			PixelShaderRHI,
			PixelShaderParameters->IndirectLightingCacheTexture1,
			PixelShaderParameters->IndirectLightingCacheSampler1,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			Texture1
			);

		SetTextureParameter(
			RHICmdList, 
			PixelShaderRHI,
			PixelShaderParameters->IndirectLightingCacheTexture2,
			PixelShaderParameters->IndirectLightingCacheSampler2,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			Texture2
			);
	}
}

void FCachedVolumeIndirectLightingPolicy::SetMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const ElementDataType& ElementData
	) const
{
	if (PixelShaderParameters)
	{
		FVector AllocationAdd(0, 0, 0);
		FVector AllocationScale(1, 1, 1);
		FVector MinUV(0, 0, 0);
		FVector MaxUV(1, 1, 1);
		FVector4 SkyBentNormal(0, 0, 1, 1);

		if (PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation
			&& PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation->IsValid())
		{
			const FIndirectLightingCacheAllocation& LightingAllocation = *PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
			
			AllocationAdd = LightingAllocation.Add;
			AllocationScale = LightingAllocation.Scale;
			MinUV = LightingAllocation.MinUV;
			MaxUV = LightingAllocation.MaxUV;
			SkyBentNormal = LightingAllocation.CurrentSkyBentNormal;
		}
		
		const FPixelShaderRHIParamRef PixelShaderRHI = PixelShader->GetPixelShader();
		SetShaderValue(RHICmdList, PixelShaderRHI, PixelShaderParameters->IndirectlightingCachePrimitiveAdd, AllocationAdd);
		SetShaderValue(RHICmdList, PixelShaderRHI, PixelShaderParameters->IndirectlightingCachePrimitiveScale, AllocationScale);
		SetShaderValue(RHICmdList, PixelShaderRHI, PixelShaderParameters->IndirectlightingCacheMinUV, MinUV);
		SetShaderValue(RHICmdList, PixelShaderRHI, PixelShaderParameters->IndirectlightingCacheMaxUV, MaxUV);
		SetShaderValue(RHICmdList, PixelShaderRHI, PixelShaderParameters->PointSkyBentNormal, SkyBentNormal);
	}
}

void FCachedPointIndirectLightingPolicy::SetMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const ElementDataType& ElementData
	) const
{
	if (PixelShaderParameters)
	{
		if (PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation
			&& PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation->IsValid()
			&& View.Family->EngineShowFlags.GlobalIllumination)
		{
			const FIndirectLightingCacheAllocation& LightingAllocation = *PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;

			if (ElementData.bPackAmbientTermInFirstVector)
			{
				// So ambient term is contained in one shader constant
				const FVector4 SwizzledAmbientTerm = 
					FVector4(LightingAllocation.SingleSamplePacked[0].X, LightingAllocation.SingleSamplePacked[1].X, LightingAllocation.SingleSamplePacked[2].X)
					//@todo - why is .5f needed to match directional?
					* FSHVector2::ConstantBasisIntegral * .5f;

				SetShaderValue(
					RHICmdList, 
					PixelShader->GetPixelShader(), 
					PixelShaderParameters->IndirectLightingSHCoefficients, 
					SwizzledAmbientTerm);
			}
			else
			{
				SetShaderValueArray(
					RHICmdList, 
					PixelShader->GetPixelShader(), 
					PixelShaderParameters->IndirectLightingSHCoefficients, 
					&LightingAllocation.SingleSamplePacked, 
					ARRAY_COUNT(LightingAllocation.SingleSamplePacked));
			}

			SetShaderValue(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->PointSkyBentNormal, 
				LightingAllocation.CurrentSkyBentNormal);

			SetShaderValue(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->DirectionalLightShadowing, 
				LightingAllocation.CurrentDirectionalShadowing);
		}
		else
		{
			const FVector4 ZeroArray[sizeof(FSHVectorRGB2) / sizeof(FVector4)] = {FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0)};

			SetShaderValueArray(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->IndirectLightingSHCoefficients, 
				&ZeroArray, 
				ARRAY_COUNT(ZeroArray));

			SetShaderValue(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->PointSkyBentNormal, 
				FVector::ZeroVector);

			SetShaderValue(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->DirectionalLightShadowing, 
				1);
		}
	}
}

void FSelfShadowedCachedPointIndirectLightingPolicy::SetMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const ElementDataType& ElementData
	) const
{
	if (PixelShaderParameters)
	{
		if (PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation
			&& PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation->IsValid()
			&& View.Family->EngineShowFlags.GlobalIllumination)
		{
			const FIndirectLightingCacheAllocation& LightingAllocation = *PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;

			SetShaderValueArray(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->IndirectLightingSHCoefficients, 
				&LightingAllocation.SingleSamplePacked, 
				ARRAY_COUNT(LightingAllocation.SingleSamplePacked));

			SetShaderValue(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->PointSkyBentNormal, 
				LightingAllocation.CurrentSkyBentNormal);
		}
		else
		{
			const FVector4 ZeroArray[sizeof(FSHVectorRGB2) / sizeof(FVector4)] = {FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0)};

			SetShaderValueArray(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->IndirectLightingSHCoefficients, 
				&ZeroArray, 
				ARRAY_COUNT(ZeroArray));

			SetShaderValue(
				RHICmdList, 
				PixelShader->GetPixelShader(), 
				PixelShaderParameters->PointSkyBentNormal, 
				FVector::ZeroVector);
		}
	}

	FSelfShadowedTranslucencyPolicy::SetMesh(
		RHICmdList, 
		View, 
		PrimitiveSceneProxy, 
		VertexShaderParameters,
		PixelShaderParameters,
		VertexShader,
		PixelShader,
		VertexFactory,
		MaterialRenderProxy,
		ElementData);
}
