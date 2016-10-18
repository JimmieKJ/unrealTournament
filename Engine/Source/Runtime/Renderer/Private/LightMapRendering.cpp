// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LightMapRendering.cpp: Light map rendering implementations.
=============================================================================*/

#include "RendererPrivate.h"
#include "SceneManagement.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FPrecomputedLightingParameters, TEXT("PrecomputedLightingBuffer"));

const TCHAR* GLightmapDefineName[2] =
{
	TEXT("LQ_TEXTURE_LIGHTMAP"),
	TEXT("HQ_TEXTURE_LIGHTMAP")
};

int32 GNumLightmapCoefficients[2] = 
{
	NUM_LQ_LIGHTMAP_COEF,
	NUM_HQ_LIGHTMAP_COEF
};

void FMovableDirectionalLightCSMLightingPolicy::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"), TEXT("1"));
	OutEnvironment.SetDefine(TEXT("DIRECTIONAL_LIGHT_CSM"), TEXT("1"));
	OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_MOBILE_SHADOWCASCADES)), MAX_MOBILE_SHADOWCASCADES);

	FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
}

void FCachedPointIndirectLightingPolicy::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("CACHED_POINT_INDIRECT_LIGHTING"),TEXT("1"));	
}

void FSelfShadowedCachedPointIndirectLightingPolicy::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("CACHED_POINT_INDIRECT_LIGHTING"),TEXT("1"));	
	FSelfShadowedTranslucencyPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
}


void FUniformLightMapPolicy::Set(
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
	check(VertexFactory);

	VertexFactory->Set(RHICmdList);
}

void FUniformLightMapPolicy::SetMesh(
	FRHICommandList& RHICmdList,
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const VertexParametersType* VertexShaderParameters,
	const PixelParametersType* PixelShaderParameters,
	FShader* VertexShader,
	FShader* PixelShader,
	const FVertexFactory* VertexFactory,
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FLightCacheInterface* LCI
	) const
{
	FUniformBufferRHIParamRef PrecomputedLightingBuffer = NULL;

	// The buffer is not cached to prevent updating the static mesh draw lists when it changes (for instance when streaming new mips)
	if (LCI)
	{
		PrecomputedLightingBuffer = LCI->GetPrecomputedLightingBuffer();
	}
	if (!PrecomputedLightingBuffer && PrimitiveSceneProxy && PrimitiveSceneProxy->GetPrimitiveSceneInfo())
	{
		PrecomputedLightingBuffer = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheUniformBuffer;
	}
	if (!PrecomputedLightingBuffer)
	{
		PrecomputedLightingBuffer = GEmptyPrecomputedLightingUniformBuffer.GetUniformBufferRHI();
	}

	if (VertexShaderParameters && VertexShaderParameters->BufferParameter.IsBound())
	{
		SetUniformBufferParameter(RHICmdList, VertexShader->GetVertexShader(), VertexShaderParameters->BufferParameter, PrecomputedLightingBuffer);
	}
	if (PixelShaderParameters && PixelShaderParameters->BufferParameter.IsBound())
	{
		SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), PixelShaderParameters->BufferParameter, PrecomputedLightingBuffer);
	}
}

void GetPrecomputedLightingParameters(
	ERHIFeatureLevel::Type FeatureLevel,
	FPrecomputedLightingParameters& Parameters, 
	const FIndirectLightingCache* LightingCache, 
	const FIndirectLightingCacheAllocation* LightingAllocation, 
	const FLightCacheInterface* LCI
	)
{
	// FCachedVolumeIndirectLightingPolicy, FCachedPointIndirectLightingPolicy
	{
		if (LightingAllocation)
		{
			Parameters.IndirectLightingCachePrimitiveAdd = LightingAllocation->Add;
			Parameters.IndirectLightingCachePrimitiveScale = LightingAllocation->Scale;
			Parameters.IndirectLightingCacheMinUV = LightingAllocation->MinUV;
			Parameters.IndirectLightingCacheMaxUV = LightingAllocation->MaxUV;
			Parameters.PointSkyBentNormal = LightingAllocation->CurrentSkyBentNormal;
			Parameters.DirectionalLightShadowing = LightingAllocation->CurrentDirectionalShadowing;

			for (uint32 i = 0; i < 3; ++i) // RGB
			{
				Parameters.IndirectLightingSHCoefficients0[i] = LightingAllocation->SingleSamplePacked0[i];
				Parameters.IndirectLightingSHCoefficients1[i] = LightingAllocation->SingleSamplePacked1[i];
			}
			Parameters.IndirectLightingSHCoefficients2 = LightingAllocation->SingleSamplePacked2;
			Parameters.IndirectLightingSHSingleCoefficient = FVector4(LightingAllocation->SingleSamplePacked0[0].X, LightingAllocation->SingleSamplePacked0[1].X, LightingAllocation->SingleSamplePacked0[2].X)
					* FSHVector2::ConstantBasisIntegral * .5f; //@todo - why is .5f needed to match directional?
		}
		else
		{
			Parameters.IndirectLightingCachePrimitiveAdd = FVector(0, 0, 0);
			Parameters.IndirectLightingCachePrimitiveScale = FVector(1, 1, 1);
			Parameters.IndirectLightingCacheMinUV = FVector(0, 0, 0);
			Parameters.IndirectLightingCacheMaxUV = FVector(1, 1, 1);
			Parameters.PointSkyBentNormal = FVector4(0, 0, 1, 1);
			Parameters.DirectionalLightShadowing = 1;

			for (uint32 i = 0; i < 3; ++i) // RGB
			{
				Parameters.IndirectLightingSHCoefficients0[i] = FVector4(0, 0, 0, 0);
				Parameters.IndirectLightingSHCoefficients1[i] = FVector4(0, 0, 0, 0);
			}
			Parameters.IndirectLightingSHCoefficients2 = FVector4(0, 0, 0, 0);
			Parameters.IndirectLightingSHSingleCoefficient = FVector4(0, 0, 0, 0);
		}

		// If we are using FCachedVolumeIndirectLightingPolicy then InitViews should have updated the lighting cache which would have initialized it
		// However the conditions for updating the lighting cache are complex and fail very occasionally in non-reproducible ways
		// Silently skipping setting the cache texture under failure for now
		if (FeatureLevel >= ERHIFeatureLevel::SM4 && LightingCache && LightingCache->IsInitialized())
		{
			Parameters.IndirectLightingCacheTexture0 = const_cast<FIndirectLightingCache*>(LightingCache)->GetTexture0().ShaderResourceTexture;
			Parameters.IndirectLightingCacheTexture1 = const_cast<FIndirectLightingCache*>(LightingCache)->GetTexture1().ShaderResourceTexture;
			Parameters.IndirectLightingCacheTexture2 = const_cast<FIndirectLightingCache*>(LightingCache)->GetTexture2().ShaderResourceTexture;

			Parameters.IndirectLightingCacheTextureSampler0 = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			Parameters.IndirectLightingCacheTextureSampler1 = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			Parameters.IndirectLightingCacheTextureSampler2 = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		}
		else
		if (FeatureLevel >= ERHIFeatureLevel::ES3_1)
		{
			Parameters.IndirectLightingCacheTexture0 = GBlackVolumeTexture->TextureRHI;
			Parameters.IndirectLightingCacheTexture1 = GBlackVolumeTexture->TextureRHI;
			Parameters.IndirectLightingCacheTexture2 = GBlackVolumeTexture->TextureRHI;

			Parameters.IndirectLightingCacheTextureSampler0 = GBlackVolumeTexture->SamplerStateRHI;
			Parameters.IndirectLightingCacheTextureSampler1 = GBlackVolumeTexture->SamplerStateRHI;
			Parameters.IndirectLightingCacheTextureSampler2 = GBlackVolumeTexture->SamplerStateRHI;
		}
		else
		{
			Parameters.IndirectLightingCacheTexture0 = GBlackTexture->TextureRHI;
			Parameters.IndirectLightingCacheTexture1 = GBlackTexture->TextureRHI;
			Parameters.IndirectLightingCacheTexture2 = GBlackTexture->TextureRHI;

			Parameters.IndirectLightingCacheTextureSampler0 = GBlackTexture->SamplerStateRHI;
			Parameters.IndirectLightingCacheTextureSampler1 = GBlackTexture->SamplerStateRHI;
			Parameters.IndirectLightingCacheTextureSampler2 = GBlackTexture->SamplerStateRHI;
		}
	}

	// TDistanceFieldShadowsAndLightMapPolicy
	const FShadowMapInteraction ShadowMapInteraction = LCI ? LCI->GetShadowMapInteraction() : FShadowMapInteraction(); 
	if (ShadowMapInteraction.GetType() == SMIT_Texture)
	{
		const UShadowMapTexture2D* ShadowMapTexture = ShadowMapInteraction.GetTexture();
		Parameters.ShadowMapCoordinateScaleBias = FVector4(ShadowMapInteraction.GetCoordinateScale(), ShadowMapInteraction.GetCoordinateBias());
		Parameters.StaticShadowMapMasks = FVector4(ShadowMapInteraction.GetChannelValid(0), ShadowMapInteraction.GetChannelValid(1), ShadowMapInteraction.GetChannelValid(2), ShadowMapInteraction.GetChannelValid(3));
		Parameters.InvUniformPenumbraSizes = ShadowMapInteraction.GetInvUniformPenumbraSize();
		Parameters.StaticShadowTexture = ShadowMapTexture ? ShadowMapTexture->TextureReference.TextureReferenceRHI.GetReference() : GWhiteTexture->TextureRHI;
		Parameters.StaticShadowTextureSampler = ShadowMapTexture ? ShadowMapTexture->Resource->SamplerStateRHI : GWhiteTexture->SamplerStateRHI;
	}
	else
	{
		Parameters.StaticShadowMapMasks = FVector4(1, 1, 1, 1);
		Parameters.InvUniformPenumbraSizes = FVector4(0, 0, 0, 0);
		Parameters.StaticShadowTexture = GWhiteTexture->TextureRHI;
		Parameters.StaticShadowTextureSampler = GWhiteTexture->SamplerStateRHI;
	}

	// TLightMapPolicy
	const FLightMapInteraction LightMapInteraction = LCI ? LCI->GetLightMapInteraction(FeatureLevel) : FLightMapInteraction();
	if (LightMapInteraction.GetType() == LMIT_Texture)
	{
		const bool bAllowHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel) && LightMapInteraction.AllowsHighQualityLightmaps();

		// Vertex Shader
		const FVector2D LightmapCoordinateScale = LightMapInteraction.GetCoordinateScale();
		const FVector2D LightmapCoordinateBias = LightMapInteraction.GetCoordinateBias();
		Parameters.LightMapCoordinateScaleBias = FVector4(LightmapCoordinateScale.X, LightmapCoordinateScale.Y, LightmapCoordinateBias.X, LightmapCoordinateBias.Y);

		// Pixel Shader
		const ULightMapTexture2D* LightMapTexture = LightMapInteraction.GetTexture(bAllowHighQualityLightMaps);
		const ULightMapTexture2D* SkyOcclusionTexture = LightMapInteraction.GetSkyOcclusionTexture();
		const ULightMapTexture2D* AOMaterialMaskTexture = LightMapInteraction.GetAOMaterialMaskTexture();

		Parameters.LightMapTexture = LightMapTexture ? LightMapTexture->TextureReference.TextureReferenceRHI.GetReference() : GBlackTexture->TextureRHI;
		Parameters.SkyOcclusionTexture  = SkyOcclusionTexture ? SkyOcclusionTexture->TextureReference.TextureReferenceRHI.GetReference() : GWhiteTexture->TextureRHI;
		Parameters.AOMaterialMaskTexture  = AOMaterialMaskTexture ? AOMaterialMaskTexture->TextureReference.TextureReferenceRHI.GetReference() : GBlackTexture->TextureRHI;

		Parameters.LightMapSampler = LightMapTexture ? LightMapTexture->Resource->SamplerStateRHI : GBlackTexture->SamplerStateRHI;
		Parameters.SkyOcclusionSampler = SkyOcclusionTexture ? SkyOcclusionTexture->Resource->SamplerStateRHI : GWhiteTexture->SamplerStateRHI;
		Parameters.AOMaterialMaskSampler = AOMaterialMaskTexture ? AOMaterialMaskTexture->Resource->SamplerStateRHI : GBlackTexture->SamplerStateRHI;

		const uint32 NumCoef = bAllowHighQualityLightMaps ? NUM_HQ_LIGHTMAP_COEF : NUM_LQ_LIGHTMAP_COEF;
		const FVector4* Scales = LightMapInteraction.GetScaleArray();
		const FVector4* Adds = LightMapInteraction.GetAddArray();
		for (uint32 CoefIndex = 0; CoefIndex < NumCoef; ++CoefIndex)
		{
			Parameters.LightMapScale[CoefIndex] = Scales[CoefIndex];
			Parameters.LightMapAdd[CoefIndex] = Adds[CoefIndex];
		}
	}
	else
	{
		// Vertex Shader
		Parameters.LightMapCoordinateScaleBias = FVector4(1, 1, 0, 0);

		// Pixel Shader
		Parameters.LightMapTexture = GBlackTexture->TextureRHI;
		Parameters.SkyOcclusionTexture  = GWhiteTexture->TextureRHI;
		Parameters.AOMaterialMaskTexture  = GBlackTexture->TextureRHI;

		Parameters.LightMapSampler = GBlackTexture->SamplerStateRHI;
		Parameters.SkyOcclusionSampler = GWhiteTexture->SamplerStateRHI;
		Parameters.AOMaterialMaskSampler = GBlackTexture->SamplerStateRHI;

		const uint32 NumCoef = FMath::Max<uint32>(NUM_HQ_LIGHTMAP_COEF, NUM_LQ_LIGHTMAP_COEF);
		for (uint32 CoefIndex = 0; CoefIndex < NumCoef; ++CoefIndex)
		{
			Parameters.LightMapScale[CoefIndex] = FVector4(1, 1, 1, 1);
			Parameters.LightMapAdd[CoefIndex] = FVector4(0, 0, 0, 0);
		}
	}
}

FUniformBufferRHIRef CreatePrecomputedLightingUniformBuffer(
	EUniformBufferUsage BufferUsage,
	ERHIFeatureLevel::Type FeatureLevel,
	const FIndirectLightingCache* LightingCache, 
	const FIndirectLightingCacheAllocation* LightingAllocation, 
	const FLightCacheInterface* LCI
	)
{
	INC_DWORD_STAT(STAT_IndirectLightingCacheUpdates);

	FPrecomputedLightingParameters Parameters;
	GetPrecomputedLightingParameters(FeatureLevel, Parameters, LightingCache, LightingAllocation, LCI);
	return FPrecomputedLightingParameters::CreateUniformBuffer(Parameters, BufferUsage);
}

void FEmptyPrecomputedLightingUniformBuffer::InitDynamicRHI()
{
	FPrecomputedLightingParameters Parameters;
	GetPrecomputedLightingParameters(GMaxRHIFeatureLevel, Parameters);
	SetContentsNoUpdate(Parameters);

	Super::InitDynamicRHI();
}

/** Global uniform buffer containing the default precomputed lighting data. */
TGlobalResource< FEmptyPrecomputedLightingUniformBuffer > GEmptyPrecomputedLightingUniformBuffer;
