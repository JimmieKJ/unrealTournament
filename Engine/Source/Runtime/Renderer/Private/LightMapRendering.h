// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMapRendering.h: Light map rendering definitions.
=============================================================================*/

#ifndef __LIGHTMAPRENDERING_H__
#define __LIGHTMAPRENDERING_H__

#include "Engine/ShadowMapTexture2D.h"

extern ENGINE_API bool GShowDebugSelectedLightmap;
extern ENGINE_API class FLightMap2D* GDebugSelectedLightmap;
extern bool GVisualizeMipLevels;

/**
 */
class FNullLightMapShaderComponent
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{}
	void Serialize(FArchive& Ar)
	{}
};

/**
 * A policy for shaders without a light-map.
 */
class FNoLightMapPolicy
{
public:

	typedef FNullLightMapShaderComponent VertexParametersType;
	typedef FNullLightMapShaderComponent PixelParametersType;
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{}

	void Set(
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

	void SetMesh(
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
	{}

	friend bool operator==(const FNoLightMapPolicy A,const FNoLightMapPolicy B)
	{
		return true;
	}

	friend int32 CompareDrawingPolicy(const FNoLightMapPolicy&,const FNoLightMapPolicy&)
	{
		return 0;
	}
};

enum ELightmapQuality
{
	LQ_LIGHTMAP,
	HQ_LIGHTMAP,
};

/**
 * Base policy for shaders with lightmaps.
 */
template< ELightmapQuality LightmapQuality >
class TLightMapPolicy
{
public:

	typedef FLightMapInteraction ElementDataType;

	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightMapCoordinateScaleBiasParameter.Bind(ParameterMap,TEXT("LightMapCoordinateScaleBias"));
		}

		void SetLightMapScale(FRHICommandList& RHICmdList, FShader* VertexShader,const FLightMapInteraction& LightMapInteraction) const
		{
			const FVector2D LightmapCoordinateScale = LightMapInteraction.GetCoordinateScale();
			const FVector2D LightmapCoordinateBias = LightMapInteraction.GetCoordinateBias();
			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(),LightMapCoordinateScaleBiasParameter,FVector4(
				LightmapCoordinateScale.X,
				LightmapCoordinateScale.Y,
				LightmapCoordinateBias.X,
				LightmapCoordinateBias.Y
				));
		}

		void Serialize(FArchive& Ar)
		{
			Ar << LightMapCoordinateScaleBiasParameter;
		}

	private:
		FShaderParameter LightMapCoordinateScaleBiasParameter;
	};

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			LightMapTextureParameter.Bind(ParameterMap,TEXT("LightMapTexture"));
			LightMapSamplerParameter.Bind(ParameterMap,TEXT("LightMapSampler"));
			SkyOcclusionTexture.Bind(ParameterMap,TEXT("SkyOcclusionTexture"));
			SkyOcclusionSampler.Bind(ParameterMap,TEXT("SkyOcclusionSampler"));
			AOMaterialMaskTexture.Bind(ParameterMap,TEXT("AOMaterialMaskTexture"));
			AOMaterialMaskSampler.Bind(ParameterMap,TEXT("AOMaterialMaskSampler"));
			LightMapScaleParameter.Bind(ParameterMap,TEXT("LightMapScale"));
			LightMapAddParameter.Bind(ParameterMap,TEXT("LightMapAdd"));
		}

		void SetLightMapTexture(FRHICommandList& RHICmdList, FShader* PixelShader, const UTexture2D* LightMapTexture) const
		{
			FTexture* TextureResource = GBlackTexture;

			if (LightMapTexture)
			{
				TextureResource = LightMapTexture->Resource;
			}

			SetTextureParameter(
				RHICmdList, 
				PixelShader->GetPixelShader(),
				LightMapTextureParameter,
				LightMapSamplerParameter,
				TextureResource
				);
		}

		void SetSkyOcclusionTexture(FRHICommandList& RHICmdList, FShader* PixelShader, const UTexture2D* SkyOcclusionTextureValue) const
		{
			FTexture* TextureResource = GWhiteTexture;

			if (SkyOcclusionTextureValue)
			{
				TextureResource = SkyOcclusionTextureValue->Resource;
			}

			SetTextureParameter(
				RHICmdList, 
				PixelShader->GetPixelShader(),
				SkyOcclusionTexture,
				SkyOcclusionSampler,
				TextureResource
				);
		}

		void SetAOMaterialMaskTexture(FRHICommandList& RHICmdList, FShader* PixelShader, const UTexture2D* AOMaterialMaskTextureValue) const
		{
			FTexture* TextureResource = GBlackTexture;

			if (AOMaterialMaskTextureValue)
			{
				TextureResource = AOMaterialMaskTextureValue->Resource;
			}

			SetTextureParameter(
				RHICmdList, 
				PixelShader->GetPixelShader(),
				AOMaterialMaskTexture,
				AOMaterialMaskSampler,
				TextureResource
				);
		}

		void SetLightMapScale(FRHICommandList& RHICmdList, FShader* PixelShader,const FLightMapInteraction& LightMapInteraction) const
		{
			const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

			SetShaderValueArray(RHICmdList,ShaderRHI,LightMapScaleParameter,LightMapInteraction.GetScaleArray(),LightMapInteraction.GetNumLightmapCoefficients());
			SetShaderValueArray(RHICmdList,ShaderRHI,LightMapAddParameter,LightMapInteraction.GetAddArray(),LightMapInteraction.GetNumLightmapCoefficients());
		}

		void Serialize(FArchive& Ar)
		{
			Ar << LightMapTextureParameter;
			Ar << LightMapSamplerParameter;
			Ar << SkyOcclusionTexture;
			Ar << SkyOcclusionSampler;
			Ar << AOMaterialMaskTexture;
			Ar << AOMaterialMaskSampler;
			Ar << LightMapScaleParameter;
			Ar << LightMapAddParameter;
		}

	private:
		FShaderResourceParameter LightMapTextureParameter;
		FShaderResourceParameter LightMapSamplerParameter;
		FShaderResourceParameter SkyOcclusionTexture;
		FShaderResourceParameter SkyOcclusionSampler;
		FShaderResourceParameter AOMaterialMaskTexture;
		FShaderResourceParameter AOMaterialMaskSampler;
		FShaderParameter LightMapScaleParameter;
		FShaderParameter LightMapAddParameter;
	};

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		switch( LightmapQuality )
		{
			case LQ_LIGHTMAP:
				OutEnvironment.SetDefine(TEXT("LQ_TEXTURE_LIGHTMAP"),TEXT("1"));
				OutEnvironment.SetDefine(TEXT("NUM_LIGHTMAP_COEFFICIENTS"), NUM_LQ_LIGHTMAP_COEF);
				break;
			case HQ_LIGHTMAP:
				OutEnvironment.SetDefine(TEXT("HQ_TEXTURE_LIGHTMAP"),TEXT("1"));
				OutEnvironment.SetDefine(TEXT("NUM_LIGHTMAP_COEFFICIENTS"), NUM_HQ_LIGHTMAP_COEF);
				break;
			default:
				check(0);
		}
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));

		// GetValueOnAnyThread() as it's possible that ShouldCache is called from rendering thread. That is to output some error message.
		return Material->GetShadingModel() != MSM_Unlit 
			&& VertexFactoryType->SupportsStaticLighting() 
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0)
			&& (Material->IsUsedWithStaticLighting() || Material->IsSpecialEngineMaterial());

		// if LQ
		//return (IsPCPlatform(Platform) || IsES2Platform(Platform));
	}

	void Set(
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

	void SetMesh(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FLightMapInteraction& LightMapInteraction
		) const
	{
		if(VertexShaderParameters)
		{
			VertexShaderParameters->SetLightMapScale(RHICmdList, VertexShader,LightMapInteraction);
		}

		if(PixelShaderParameters)
		{
			PixelShaderParameters->SetLightMapScale(RHICmdList, PixelShader,LightMapInteraction);
			PixelShaderParameters->SetLightMapTexture(RHICmdList, PixelShader, LightMapInteraction.GetTexture(AllowHighQualityLightmaps(View.GetFeatureLevel())));
			PixelShaderParameters->SetSkyOcclusionTexture(RHICmdList, PixelShader, LightMapInteraction.GetSkyOcclusionTexture());
			PixelShaderParameters->SetAOMaterialMaskTexture(RHICmdList, PixelShader, LightMapInteraction.GetAOMaterialMaskTexture());
		}
	}

	friend bool operator==( const TLightMapPolicy< LightmapQuality > A, const TLightMapPolicy< LightmapQuality > B )
	{
		return true;
	}

	friend int32 CompareDrawingPolicy( const TLightMapPolicy< LightmapQuality >& A, const TLightMapPolicy< LightmapQuality >& B )
	{
		return 0;
	}
};

// A light map policy for computing up to 4 signed distance field shadow factors in the base pass.
template< ELightmapQuality LightmapQuality >
class TDistanceFieldShadowsAndLightMapPolicy : public TLightMapPolicy< LightmapQuality >
{
	typedef TLightMapPolicy< LightmapQuality >	Super;
	typedef typename Super::ElementDataType		SuperElementDataType;
public:

	struct ElementDataType
	{
		SuperElementDataType SuperElementData;
		
		FShadowMapInteraction ShadowMapInteraction;

		/** Initialization constructor. */
		ElementDataType( const FShadowMapInteraction& InShadowMapInteraction, const SuperElementDataType& InSuperElementData )
			: SuperElementData(InSuperElementData)
		{
			ShadowMapInteraction = InShadowMapInteraction;
		}
	};

	class VertexParametersType : public Super::VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			ShadowMapCoordinateScaleBias.Bind(ParameterMap, TEXT("ShadowMapCoordinateScaleBias"));
			Super::VertexParametersType::Bind(ParameterMap);
		}

		void SetMesh(FRHICommandList& RHICmdList, FShader* VertexShader, const FShadowMapInteraction& ShadowMapInteraction) const
		{
			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(), ShadowMapCoordinateScaleBias, FVector4(ShadowMapInteraction.GetCoordinateScale(), ShadowMapInteraction.GetCoordinateBias()));
		}

		void Serialize(FArchive& Ar)
		{
			Ar << ShadowMapCoordinateScaleBias;
			Super::VertexParametersType::Serialize(Ar);
		}

	private:
		FShaderParameter ShadowMapCoordinateScaleBias;
	};


	class PixelParametersType : public Super::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			InvUniformPenumbraSizes.Bind(ParameterMap,TEXT("InvUniformPenumbraSizes"));
			StaticShadowMapMasks.Bind(ParameterMap,TEXT("StaticShadowMapMasks"));
			StaticShadowTexture.Bind(ParameterMap,TEXT("StaticShadowTexture"));
			StaticShadowSampler.Bind(ParameterMap, TEXT("StaticShadowTextureSampler"));

			Super::PixelParametersType::Bind(ParameterMap);
		}

		void SetMesh(FRHICommandList& RHICmdList, FShader* PixelShader, const FShadowMapInteraction& ShadowMapInteraction) const
		{
			const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

			SetShaderValue(RHICmdList, ShaderRHI, InvUniformPenumbraSizes, ShadowMapInteraction.GetInvUniformPenumbraSize());

			SetShaderValue(RHICmdList, ShaderRHI, StaticShadowMapMasks, FVector4(
				ShadowMapInteraction.GetChannelValid(0),
				ShadowMapInteraction.GetChannelValid(1),
				ShadowMapInteraction.GetChannelValid(2),
				ShadowMapInteraction.GetChannelValid(3)
				));

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				StaticShadowTexture,
				StaticShadowSampler,
				ShadowMapInteraction.GetTexture() ? ShadowMapInteraction.GetTexture()->Resource : GWhiteTexture
				);
		}

		void Serialize(FArchive& Ar)
		{
			Ar << InvUniformPenumbraSizes;
			Ar << StaticShadowMapMasks;
			Ar << StaticShadowTexture;
			Ar << StaticShadowSampler;
			Super::PixelParametersType::Serialize(Ar);
		}

	private:
		FShaderParameter StaticShadowMapMasks;
		FShaderResourceParameter StaticShadowTexture;
		FShaderResourceParameter StaticShadowSampler;
		FShaderParameter InvUniformPenumbraSizes;
	};

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("STATICLIGHTING_TEXTUREMASK"), 1);
		OutEnvironment.SetDefine(TEXT("STATICLIGHTING_SIGNEDDISTANCEFIELD"), 1);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	TDistanceFieldShadowsAndLightMapPolicy() {}

	void SetMesh(
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
		if (VertexShaderParameters)
		{
			VertexShaderParameters->SetMesh(RHICmdList, VertexShader, ElementData.ShadowMapInteraction);
		}

		if (PixelShaderParameters)
		{
			PixelShaderParameters->SetMesh(RHICmdList, PixelShader, ElementData.ShadowMapInteraction);
		}

		Super::SetMesh(RHICmdList, View, PrimitiveSceneProxy, VertexShaderParameters, PixelShaderParameters, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, ElementData.SuperElementData);
	}
};

/**
 * Policy for 'fake' texture lightmaps, such as the LightMap density rendering mode
 */
class FDummyLightMapPolicy : public TLightMapPolicy< HQ_LIGHTMAP >
{
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetShadingModel() != MSM_Unlit && VertexFactoryType->SupportsStaticLighting();
	}

	void Set(
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

	void SetMesh(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FLightMapInteraction& LightMapInteraction
		) const
	{
		if(VertexShaderParameters)
		{
			VertexShaderParameters->SetLightMapScale(RHICmdList, VertexShader,LightMapInteraction);
		}

		if(PixelShaderParameters)
		{
			PixelShaderParameters->SetLightMapScale(RHICmdList, PixelShader,LightMapInteraction);
		}
	}

	/** Initialization constructor. */
	FDummyLightMapPolicy()
	{
	}

	friend bool operator==(const FDummyLightMapPolicy A,const FDummyLightMapPolicy B)
	{
		return true;
	}

	friend int32 CompareDrawingPolicy(const FDummyLightMapPolicy& A,const FDummyLightMapPolicy& B)
	{
		return 0;
	}
};

/**
 * Policy for self shadowing translucency from a directional light
 */
class FSelfShadowedTranslucencyPolicy : public FNoLightMapPolicy
{
public:

	struct ElementDataType
	{
		ElementDataType(const FProjectedShadowInfo* InTranslucentSelfShadow) :
			TranslucentSelfShadow(InTranslucentSelfShadow)
		{}

		const FProjectedShadowInfo* TranslucentSelfShadow;
	};

	class PixelParametersType : public FNoLightMapPolicy::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			TranslucencyShadowParameters.Bind(ParameterMap);
			WorldToShadowMatrix.Bind(ParameterMap, TEXT("WorldToShadowMatrix"));
			ShadowUVMinMax.Bind(ParameterMap, TEXT("ShadowUVMinMax"));
			DirectionalLightDirection.Bind(ParameterMap, TEXT("DirectionalLightDirection"));
			DirectionalLightColor.Bind(ParameterMap, TEXT("DirectionalLightColor"));
		}

		void Serialize(FArchive& Ar)
		{
			Ar << TranslucencyShadowParameters;
			Ar << WorldToShadowMatrix;
			Ar << ShadowUVMinMax;
			Ar << DirectionalLightDirection;
			Ar << DirectionalLightColor;
		}

		FTranslucencyShadowProjectionShaderParameters TranslucencyShadowParameters;
		FShaderParameter WorldToShadowMatrix;
		FShaderParameter ShadowUVMinMax;
		FShaderParameter DirectionalLightDirection;
		FShaderParameter DirectionalLightColor;
	};

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetShadingModel() != MSM_Unlit && IsTranslucentBlendMode(Material->GetBlendMode()) && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("TRANSLUCENT_SELF_SHADOWING"),TEXT("1"));
		FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	FSelfShadowedTranslucencyPolicy() {}

	void SetMesh(
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
			const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

			// Set these even if ElementData.TranslucentSelfShadow is NULL to avoid a d3d debug error from the shader expecting texture SRV's when a different type are bound
			PixelShaderParameters->TranslucencyShadowParameters.Set(RHICmdList, PixelShader);

			if (ElementData.TranslucentSelfShadow)
			{
				FVector4 ShadowmapMinMax;
				FMatrix WorldToShadowMatrixValue = ElementData.TranslucentSelfShadow->GetWorldToShadowMatrix(ShadowmapMinMax);

				SetShaderValue(RHICmdList, ShaderRHI, PixelShaderParameters->WorldToShadowMatrix, WorldToShadowMatrixValue);
				SetShaderValue(RHICmdList, ShaderRHI, PixelShaderParameters->ShadowUVMinMax, ShadowmapMinMax);

				const FLightSceneProxy* const LightProxy = ElementData.TranslucentSelfShadow->GetLightSceneInfo().Proxy;
				SetShaderValue(RHICmdList, ShaderRHI, PixelShaderParameters->DirectionalLightDirection, LightProxy->GetDirection());
				//@todo - support fading from both views
				const float FadeAlpha = ElementData.TranslucentSelfShadow->FadeAlphas[0];
				// Incorporate the diffuse scale of 1 / PI into the light color
				const FVector4 DirectionalLightColorValue(FVector(LightProxy->GetColor() * FadeAlpha / PI), FadeAlpha);
				SetShaderValue(RHICmdList, ShaderRHI, PixelShaderParameters->DirectionalLightColor, DirectionalLightColorValue);
			}
			else
			{
				SetShaderValue(RHICmdList, ShaderRHI, PixelShaderParameters->DirectionalLightColor, FVector4(0, 0, 0, 0));
			}
		}
	}
};

/**
 * Allows a dynamic object to access indirect lighting through a per-object allocation in a volume texture atlas
 */
class FCachedVolumeIndirectLightingPolicy : public FNoLightMapPolicy
{
public:

	struct ElementDataType {};

	class PixelParametersType : public FNoLightMapPolicy::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			IndirectLightingCacheTexture0.Bind(ParameterMap, TEXT("IndirectLightingCacheTexture0"));
			IndirectLightingCacheTexture1.Bind(ParameterMap, TEXT("IndirectLightingCacheTexture1"));
			IndirectLightingCacheTexture2.Bind(ParameterMap, TEXT("IndirectLightingCacheTexture2"));
			IndirectLightingCacheSampler0.Bind(ParameterMap, TEXT("IndirectLightingCacheTextureSampler0"));
			IndirectLightingCacheSampler1.Bind(ParameterMap, TEXT("IndirectLightingCacheTextureSampler1"));
			IndirectLightingCacheSampler2.Bind(ParameterMap, TEXT("IndirectLightingCacheTextureSampler2"));
			IndirectlightingCachePrimitiveAdd.Bind(ParameterMap, TEXT("IndirectlightingCachePrimitiveAdd"));
			IndirectlightingCachePrimitiveScale.Bind(ParameterMap, TEXT("IndirectlightingCachePrimitiveScale"));
			IndirectlightingCacheMinUV.Bind(ParameterMap, TEXT("IndirectlightingCacheMinUV"));
			IndirectlightingCacheMaxUV.Bind(ParameterMap, TEXT("IndirectlightingCacheMaxUV"));
			PointSkyBentNormal.Bind(ParameterMap, TEXT("PointSkyBentNormal"));
		}

		void Serialize(FArchive& Ar)
		{
			Ar << IndirectLightingCacheTexture0;
			Ar << IndirectLightingCacheTexture1;
			Ar << IndirectLightingCacheTexture2;
			Ar << IndirectLightingCacheSampler0;
			Ar << IndirectLightingCacheSampler1;
			Ar << IndirectLightingCacheSampler2;
			Ar << IndirectlightingCachePrimitiveAdd;
			Ar << IndirectlightingCachePrimitiveScale;
			Ar << IndirectlightingCacheMinUV;
			Ar << IndirectlightingCacheMaxUV;
			Ar << PointSkyBentNormal;
		}

		FShaderResourceParameter IndirectLightingCacheTexture0;
		FShaderResourceParameter IndirectLightingCacheTexture1;
		FShaderResourceParameter IndirectLightingCacheTexture2;
		FShaderResourceParameter IndirectLightingCacheSampler0;
		FShaderResourceParameter IndirectLightingCacheSampler1;
		FShaderResourceParameter IndirectLightingCacheSampler2;
		FShaderParameter IndirectlightingCachePrimitiveAdd;
		FShaderParameter IndirectlightingCachePrimitiveScale;
		FShaderParameter IndirectlightingCacheMinUV;
		FShaderParameter IndirectlightingCacheMaxUV;
		FShaderParameter PointSkyBentNormal;
	};

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));

		return Material->GetShadingModel() != MSM_Unlit 
			&& !IsTranslucentBlendMode(Material->GetBlendMode()) 
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0)
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("CACHED_VOLUME_INDIRECT_LIGHTING"),TEXT("1"));
		FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	FCachedVolumeIndirectLightingPolicy() {}

	void Set(
		FRHICommandList& RHICmdList, 
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const;

	void SetMesh(
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
		) const;
};


/**
 * Allows a dynamic object to access indirect lighting through a per-object lighting sample
 */
class FCachedPointIndirectLightingPolicy : public FNoLightMapPolicy
{
public:

	struct ElementDataType 
	{
		ElementDataType(bool bInPackAmbientTermInFirstVector) :
			bPackAmbientTermInFirstVector(bInPackAmbientTermInFirstVector)
		{}

		bool bPackAmbientTermInFirstVector;
	};

	class PixelParametersType : public FNoLightMapPolicy::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			IndirectLightingSHCoefficients.Bind(ParameterMap, TEXT("IndirectLightingSHCoefficients"));
			PointSkyBentNormal.Bind(ParameterMap, TEXT("PointSkyBentNormal"));
			DirectionalLightShadowing.Bind(ParameterMap, TEXT("DirectionalLightShadowing"));
		}

		void Serialize(FArchive& Ar)
		{
			Ar << IndirectLightingSHCoefficients;
			Ar << PointSkyBentNormal;
			Ar << DirectionalLightShadowing;
		}

		FShaderParameter IndirectLightingSHCoefficients;
		FShaderParameter PointSkyBentNormal;
		FShaderParameter DirectionalLightShadowing;
	};

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	
		return Material->GetShadingModel() != MSM_Unlit
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnAnyThread() != 0);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("CACHED_POINT_INDIRECT_LIGHTING"),TEXT("1"));
		FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	FCachedPointIndirectLightingPolicy() {}

	void SetMesh(
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
		) const;
};

/**
 * Self shadowing translucency from a directional light + allows a dynamic object to access indirect lighting through a per-object lighting sample
 */
class FSelfShadowedCachedPointIndirectLightingPolicy : public FSelfShadowedTranslucencyPolicy
{
public:

	class PixelParametersType : public FSelfShadowedTranslucencyPolicy::PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			IndirectLightingSHCoefficients.Bind(ParameterMap, TEXT("IndirectLightingSHCoefficients"));
			PointSkyBentNormal.Bind(ParameterMap, TEXT("PointSkyBentNormal"));
			FSelfShadowedTranslucencyPolicy::PixelParametersType::Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			Ar << IndirectLightingSHCoefficients;
			Ar << PointSkyBentNormal;

			FSelfShadowedTranslucencyPolicy::PixelParametersType::Serialize(Ar);
		}

		FShaderParameter IndirectLightingSHCoefficients;
		FShaderParameter PointSkyBentNormal;
	};

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static IConsoleVariable* AllowStaticLightingVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowStaticLighting"));

		return Material->GetShadingModel() != MSM_Unlit 
			&& IsTranslucentBlendMode(Material->GetBlendMode()) 
			&& (!AllowStaticLightingVar || AllowStaticLightingVar->GetInt() != 0)
			&& FSelfShadowedTranslucencyPolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("CACHED_POINT_INDIRECT_LIGHTING"),TEXT("1"));
		FSelfShadowedTranslucencyPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	FSelfShadowedCachedPointIndirectLightingPolicy() {}

	void SetMesh(
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
		) const;
};

/**
 * Renders an unshadowed directional light in the base pass, used to support low end hardware where deferred shading is too expensive.
 */
class FSimpleDynamicLightingPolicy : public FNoLightMapPolicy
{
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetShadingModel() != MSM_Unlit;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SIMPLE_DYNAMIC_LIGHTING"),TEXT("1"));
		FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	FSimpleDynamicLightingPolicy() {}
};

/** Combines an unshadowed directional light with indirect lighting from a single SH sample. */
class FSimpleDirectionalLightAndSHIndirectPolicy
{
public:

	struct ElementDataType : public FSimpleDynamicLightingPolicy::ElementDataType, FCachedPointIndirectLightingPolicy::ElementDataType
	{
		ElementDataType(bool bInPackAmbientTermInFirstVector)
			: FCachedPointIndirectLightingPolicy::ElementDataType(bInPackAmbientTermInFirstVector)
		{}
	};

	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			SimpleDynamicLightingParameters.Bind(ParameterMap);
			CachedPointIndirectParameters.Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			SimpleDynamicLightingParameters.Serialize(Ar);
			CachedPointIndirectParameters.Serialize(Ar);
		}

		FSimpleDynamicLightingPolicy::VertexParametersType SimpleDynamicLightingParameters;
		FCachedPointIndirectLightingPolicy::VertexParametersType CachedPointIndirectParameters;
	};

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
			SimpleDynamicLightingParameters.Bind(ParameterMap);
			CachedPointIndirectParameters.Bind(ParameterMap);
		}

		void Serialize(FArchive& Ar)
		{
			SimpleDynamicLightingParameters.Serialize(Ar);
			CachedPointIndirectParameters.Serialize(Ar);
		}

		FSimpleDynamicLightingPolicy::PixelParametersType SimpleDynamicLightingParameters;
		FCachedPointIndirectLightingPolicy::PixelParametersType CachedPointIndirectParameters;
	};

	/** Initialization constructor. */
	FSimpleDirectionalLightAndSHIndirectPolicy() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FSimpleDynamicLightingPolicy::ShouldCache(Platform, Material, VertexFactoryType) && FCachedPointIndirectLightingPolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FSimpleDynamicLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		FCachedPointIndirectLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	void Set(
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

	void SetMesh(
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
		if (VertexShaderParameters && PixelShaderParameters)
		{
			CachedPointIndirectLightingPolicy.SetMesh(
				RHICmdList, 
				View, 
				PrimitiveSceneProxy, 
				&VertexShaderParameters->CachedPointIndirectParameters, 
				&PixelShaderParameters->CachedPointIndirectParameters, 
				VertexShader, 
				PixelShader, 
				VertexFactory, 
				MaterialRenderProxy, 
				ElementData.bPackAmbientTermInFirstVector);
		}
	}

	friend bool operator==(const FSimpleDirectionalLightAndSHIndirectPolicy A,const FSimpleDirectionalLightAndSHIndirectPolicy B)
	{
		return true;
	}

	friend int32 CompareDrawingPolicy(const FSimpleDirectionalLightAndSHIndirectPolicy&,const FSimpleDirectionalLightAndSHIndirectPolicy&)
	{
		return 0;
	}

private:

	FSimpleDynamicLightingPolicy SimpleDynamicLightingPolicy;
	FCachedPointIndirectLightingPolicy CachedPointIndirectLightingPolicy;
};

/** Combines a directional light with indirect lighting from a single SH sample. */
class FSimpleDirectionalLightAndSHDirectionalIndirectPolicy : public FSimpleDirectionalLightAndSHIndirectPolicy
{
public:
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_FORWARD_SHADOWCASCADES)), MAX_FORWARD_SHADOWCASCADES);
		FSimpleDirectionalLightAndSHIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	friend bool operator==(const FSimpleDirectionalLightAndSHDirectionalIndirectPolicy A, const FSimpleDirectionalLightAndSHDirectionalIndirectPolicy B)
	{
		return true;
	}

	friend int32 CompareDrawingPolicy(const FSimpleDirectionalLightAndSHDirectionalIndirectPolicy&, const FSimpleDirectionalLightAndSHDirectionalIndirectPolicy&)
	{
		return 0;
	}
};

/** Combines a directional light with CSM with indirect lighting from a single SH sample. */
class FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy : public FSimpleDirectionalLightAndSHDirectionalIndirectPolicy
{
public:
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT_CSM"), TEXT("1"));
		FSimpleDirectionalLightAndSHDirectionalIndirectPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	friend bool operator==(const FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy A, const FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy B)
	{
		return true;
	}

	friend int32 CompareDrawingPolicy(const FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy&, const FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy&)
	{
		return 0;
	}
};

class FMovableDirectionalLightLightingPolicy : public FNoLightMapPolicy
{
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetShadingModel() != MSM_Unlit;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"),TEXT("1"));
		FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

class FMovableDirectionalLightCSMLightingPolicy : public FNoLightMapPolicy
{
public:
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return Material->GetShadingModel() != MSM_Unlit;
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
};

class FMovableDirectionalLightWithLightmapLightingPolicy : public TLightMapPolicy<LQ_LIGHTMAP>
{
	typedef TLightMapPolicy< LQ_LIGHTMAP >	Super;

public:
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return (Material->GetShadingModel() != MSM_Unlit) && Super::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_FORWARD_SHADOWCASCADES)), MAX_FORWARD_SHADOWCASCADES);

		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

class FMovableDirectionalLightCSMWithLightmapLightingPolicy : public FMovableDirectionalLightWithLightmapLightingPolicy
{
public:
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT_CSM"), TEXT("1"));

		FMovableDirectionalLightWithLightmapLightingPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

#endif // __LIGHTMAPRENDERING_H__
