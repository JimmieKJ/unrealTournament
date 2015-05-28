// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.h: Base pass rendering definitions.
=============================================================================*/

#pragma once

#include "LightMapRendering.h"
#include "VelocityRendering.h"
#include "ShaderBaseClasses.h"
#include "LightGrid.h"
#include "EditorCompositeParams.h"

/** Whether to allow the indirect lighting cache to be applied to dynamic objects. */
extern int32 GIndirectLightingCache;

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without atmospheric fog.
 */

template<typename LightMapPolicyType>
class TBasePassVertexShaderBaseType : public FMeshMaterialShader, public LightMapPolicyType::VertexParametersType
{
protected:
	TBasePassVertexShaderBaseType() {}
	TBasePassVertexShaderBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
		AtmosphericFogTextureParameters.Bind(Initializer.ParameterMap);
		const bool bOutputsVelocityToGBuffer = FVelocityRendering::OutputsToGBuffer();
		if (bOutputsVelocityToGBuffer)
		{
			PreviousLocalToWorldParameter.Bind(Initializer.ParameterMap, TEXT("PreviousLocalToWorld"));
//@todo-rco: Move to pixel shader
			SkipOutputVelocityParameter.Bind(Initializer.ParameterMap, TEXT("SkipOutputVelocity"));
		}
	}

public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		LightMapPolicyType::VertexParametersType::Serialize(Ar);
		Ar << HeightFogParameters;
		Ar << AtmosphericFogTextureParameters;
		Ar << PreviousLocalToWorldParameter;
		Ar << SkipOutputVelocityParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FVertexFactory* VertexFactory,
		const FMaterial& InMaterialResource,
		const FSceneView& View,
		bool bAllowGlobalFog,
		ESceneRenderTargetsMode::Type TextureMode
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,InMaterialResource,View,TextureMode);

		if (bAllowGlobalFog)
		{
			HeightFogParameters.Set(RHICmdList, GetVertexShader(), &View);
			AtmosphericFogTextureParameters.Set(RHICmdList, GetVertexShader(), View);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy, const FMeshBatch& Mesh, const FMeshBatchElement& BatchElement);

private:
	
	/** The parameters needed to calculate the fog contribution from height fog layers. */
	FHeightFogShaderParameters HeightFogParameters;
	FAtmosphereShaderTextureParameters AtmosphericFogTextureParameters;
	// When outputting from base pass, the previous transform
	FShaderParameter PreviousLocalToWorldParameter;
	FShaderParameter SkipOutputVelocityParameter;
};

template<typename LightMapPolicyType, bool bEnableAtmosphericFog>
class TBasePassVS : public TBasePassVertexShaderBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassVS,MeshMaterial);
	typedef TBasePassVertexShaderBaseType<LightMapPolicyType> Super;

protected:

	TBasePassVS() {}
	TBasePassVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		Super(Initializer)
	{
	}

public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		bool bShouldCache = Super::ShouldCache(Platform, Material, VertexFactoryType);
		return bShouldCache 
			&& (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
			&& (!bEnableAtmosphericFog || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("BASEPASS_ATMOSPHERIC_FOG"),(uint32)(bEnableAtmosphericFog ? 1 : 0));
	}
};

/**
 * The base shader type for hull shaders.
 */
template<typename LightMapPolicyType>
class TBasePassHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TBasePassHS,MeshMaterial);

protected:

	TBasePassHS() {}

	TBasePassHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TBasePassVS<LightMapPolicyType,false>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVS<LightMapPolicyType,false>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

/**
 * The base shader type for Domain shaders.
 */
template<typename LightMapPolicyType>
class TBasePassDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(TBasePassDS,MeshMaterial);

protected:

	TBasePassDS() {}

	TBasePassDS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TBasePassVS<LightMapPolicyType,false>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVS<LightMapPolicyType,false>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

public:
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FBaseDS::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FVertexFactory* VertexFactory,
		const FSceneView& View
		)
	{
		FBaseDS::SetParameters(RHICmdList, MaterialRenderProxy, View);
	}
};

/** Parameters needed to implement the sky light cubemap reflection. */
class FSkyLightReflectionParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		SkyLightCubemap.Bind(ParameterMap, TEXT("SkyLightCubemap"));
		SkyLightCubemapSampler.Bind(ParameterMap, TEXT("SkyLightCubemapSampler"));
		SkyLightParameters.Bind(ParameterMap, TEXT("SkyLightParameters"));
	}

	template<typename TParamRef>
	void SetParameters(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FScene* Scene, bool bApplySkyLight)
	{
		FTexture* SkyLightTextureResource = GBlackTextureCube;
		float ApplySkyLightMask = 0;
		float SkyMipCount = 1;
		bool bSkyLightIsDynamic = false;

		GetSkyParametersFromScene(Scene, bApplySkyLight, SkyLightTextureResource, ApplySkyLightMask, SkyMipCount, bSkyLightIsDynamic);

		SetTextureParameter(RHICmdList, ShaderRHI, SkyLightCubemap, SkyLightCubemapSampler, SkyLightTextureResource);
		const FVector SkyParametersValue(SkyMipCount - 1.0f, ApplySkyLightMask, bSkyLightIsDynamic ? 1.0f : 0.0f);
		SetShaderValue(RHICmdList, ShaderRHI, SkyLightParameters, SkyParametersValue);
	}

	friend FArchive& operator<<(FArchive& Ar,FSkyLightReflectionParameters& P)
	{
		Ar << P.SkyLightCubemap << P.SkyLightCubemapSampler << P.SkyLightParameters;
		return Ar;
	}

private:

	FShaderResourceParameter SkyLightCubemap;
	FShaderResourceParameter SkyLightCubemapSampler;
	FShaderParameter SkyLightParameters;

	void GetSkyParametersFromScene(const FScene* Scene, bool bApplySkyLight, FTexture*& OutSkyLightTextureResource, float& OutApplySkyLightMask, float& OutSkyMipCount, bool& bSkyLightIsDynamic);
};

/** Parameters needed for lighting translucency, shared by multiple shaders. */
class FTranslucentLightingParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		TranslucencyLightingVolumeAmbientInner.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeAmbientInner"));
		TranslucencyLightingVolumeAmbientInnerSampler.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeAmbientInnerSampler"));
		TranslucencyLightingVolumeAmbientOuter.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeAmbientOuter"));
		TranslucencyLightingVolumeAmbientOuterSampler.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeAmbientOuterSampler"));
		TranslucencyLightingVolumeDirectionalInner.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalInner"));
		TranslucencyLightingVolumeDirectionalInnerSampler.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalInnerSampler"));
		TranslucencyLightingVolumeDirectionalOuter.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalOuter"));
		TranslucencyLightingVolumeDirectionalOuterSampler.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalOuterSampler"));
		ReflectionCubemap.Bind(ParameterMap, TEXT("ReflectionCubemap"));
		ReflectionCubemapSampler.Bind(ParameterMap, TEXT("ReflectionCubemapSampler"));
		CubemapArrayIndex.Bind(ParameterMap, TEXT("CubemapArrayIndex"));
		SkyLightReflectionParameters.Bind(ParameterMap);
		HZBTexture.Bind(ParameterMap, TEXT("HZBTexture"));
		HZBSampler.Bind(ParameterMap, TEXT("HZBSampler"));
		PrevSceneColor.Bind(ParameterMap, TEXT("PrevSceneColor"));
		PrevSceneColorSampler.Bind(ParameterMap, TEXT("PrevSceneColorSampler"));
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FViewInfo* View);

	void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FPrimitiveSceneProxy* Proxy, ERHIFeatureLevel::Type FeatureLevel);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FTranslucentLightingParameters& P)
	{
		Ar << P.TranslucencyLightingVolumeAmbientInner;
		Ar << P.TranslucencyLightingVolumeAmbientInnerSampler;
		Ar << P.TranslucencyLightingVolumeAmbientOuter;
		Ar << P.TranslucencyLightingVolumeAmbientOuterSampler;
		Ar << P.TranslucencyLightingVolumeDirectionalInner;
		Ar << P.TranslucencyLightingVolumeDirectionalInnerSampler;
		Ar << P.TranslucencyLightingVolumeDirectionalOuter;
		Ar << P.TranslucencyLightingVolumeDirectionalOuterSampler;
		Ar << P.ReflectionCubemap;
		Ar << P.ReflectionCubemapSampler;
		Ar << P.CubemapArrayIndex;
		Ar << P.SkyLightReflectionParameters;
		Ar << P.HZBTexture;
		Ar << P.HZBSampler;
		Ar << P.PrevSceneColor;
		Ar << P.PrevSceneColorSampler;
		return Ar;
	}

private:

	FShaderResourceParameter TranslucencyLightingVolumeAmbientInner;
	FShaderResourceParameter TranslucencyLightingVolumeAmbientInnerSampler;
	FShaderResourceParameter TranslucencyLightingVolumeAmbientOuter;
	FShaderResourceParameter TranslucencyLightingVolumeAmbientOuterSampler;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalInner;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalInnerSampler;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalOuter;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalOuterSampler;
	FShaderResourceParameter ReflectionCubemap;
	FShaderResourceParameter ReflectionCubemapSampler;
	FShaderParameter CubemapArrayIndex;
	FSkyLightReflectionParameters SkyLightReflectionParameters;
	FShaderResourceParameter HZBTexture;
	FShaderResourceParameter HZBSampler;
	FShaderResourceParameter PrevSceneColor;
	FShaderResourceParameter PrevSceneColorSampler;
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename LightMapPolicyType>
class TBasePassPixelShaderBaseType : public FMeshMaterialShader, public LightMapPolicyType::PixelParametersType
{
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		const bool bOutputVelocity = FVelocityRendering::OutputsToGBuffer();
		if (bOutputVelocity)
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
			// This needs to match FSceneRenderTargets::GetNumGBufferTargets()
			const int32 VelocityIndex = (CVar && CVar->GetValueOnAnyThread() != 0) ? 6 : 5;
			OutEnvironment.SetRenderTargetOutputFormat(VelocityIndex, PF_G16R16);
		}
	}

	/** Initialization constructor. */
	TBasePassPixelShaderBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::PixelParametersType::Bind(Initializer.ParameterMap);
		TranslucentLightingParameters.Bind(Initializer.ParameterMap);
		EditorCompositeParams.Bind(Initializer.ParameterMap);
		LightGrid.Bind(Initializer.ParameterMap,TEXT("LightGrid"));
	}
	TBasePassPixelShaderBaseType() {}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& MaterialResource, 
		const FViewInfo* View, 
		EBlendMode BlendMode, 
		bool bEnableEditorPrimitveDepthTest,
		ESceneRenderTargetsMode::Type TextureMode)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMeshMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialRenderProxy, MaterialResource, *View, TextureMode);

		if (View->GetFeatureLevel() >= ERHIFeatureLevel::SM4)
		{
			if (IsTranslucentBlendMode(BlendMode))
			{
				TranslucentLightingParameters.Set(RHICmdList, this, View);

				// Experimental dynamic forward lighting for translucency. Can be the base for opaque forward lighting which will allow more lighting models or rendering without a GBuffer
				if(BlendMode == BLEND_Translucent && MaterialResource.GetTranslucencyLightingMode() == TLM_SurfacePerPixelLighting)
				{
					check(GetUniformBufferParameter<FForwardLightData>().IsInitialized());

					if(GetUniformBufferParameter<FForwardLightData>().IsBound())
					{
						SetUniformBufferParameter(RHICmdList, ShaderRHI,GetUniformBufferParameter<FForwardLightData>(),View->ForwardLightData);
					}

					if(LightGrid.IsBound())
					{
						RHICmdList.SetShaderResourceViewParameter(ShaderRHI, LightGrid.GetBaseIndex(), GLightGridVertexBuffer.VertexBufferSRV);
					}
				}
			}
		}
		
		EditorCompositeParams.SetParameters(RHICmdList, MaterialResource, View, bEnableEditorPrimitveDepthTest, GetPixelShader());
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement, EBlendMode BlendMode);

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		LightMapPolicyType::PixelParametersType::Serialize(Ar);
		Ar << TranslucentLightingParameters;
 		Ar << EditorCompositeParams;
		Ar << LightGrid;
		return bShaderHasOutdatedParameters;
	}

private:
	FTranslucentLightingParameters TranslucentLightingParameters;
	FEditorCompositingParameters EditorCompositeParams;
	FShaderResourceParameter LightGrid;
};

/** The concrete base pass pixel shader type. */
template<typename LightMapPolicyType, bool bEnableSkyLight>
class TBasePassPS : public TBasePassPixelShaderBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassPS,MeshMaterial);
public:
	
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile skylight version for lit materials
		const bool bCacheShaders = !bEnableSkyLight || (Material->GetShadingModel() != MSM_Unlit);

		return bCacheShaders
			&& (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
			&& TBasePassPixelShaderBaseType<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"),(uint32)(bEnableSkyLight ? 1 : 0));
		TBasePassPixelShaderBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
	
	/** Initialization constructor. */
	TBasePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TBasePassPixelShaderBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TBasePassPS() {}
};

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType>
class TBasePassDrawingPolicy : public FMeshDrawingPolicy
{
public:

	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:

		/** The element's light-map data. */
		typename LightMapPolicyType::ElementDataType LightMapElementData;

		/** Default constructor. */
		ElementDataType()
		{}

		/** Initialization constructor. */
		ElementDataType(const typename LightMapPolicyType::ElementDataType& InLightMapElementData)
		:	LightMapElementData(InLightMapElementData)
		{}
	};

	/** Initialization constructor. */
	TBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		ERHIFeatureLevel::Type InFeatureLevel,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		ESceneRenderTargetsMode::Type InSceneTextureMode,
		bool bInEnableSkyLight,
		bool bInEnableAtmosphericFog,
		bool bOverrideWithShaderComplexity = false,
		bool bInAllowGlobalFog = false,
		bool bInEnableEditorPrimitiveDepthTest = false
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,bOverrideWithShaderComplexity),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode),
		SceneTextureMode(InSceneTextureMode),
		bAllowGlobalFog(bInAllowGlobalFog),
		bEnableSkyLight(bInEnableSkyLight),
		bEnableEditorPrimitiveDepthTest(bInEnableEditorPrimitiveDepthTest),
		bEnableAtmosphericFog(bInEnableAtmosphericFog)
	{
		HullShader = NULL;
		DomainShader = NULL;
	
		const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetTessellationMode();

		if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
			&& InVertexFactory->GetType()->SupportsTessellationShaders() 
			&& MaterialTessellationMode != MTM_NoTessellation)
		{
			// Find the base pass tessellation shaders since the material is tessellated
			HullShader = InMaterialResource.GetShader<TBasePassHS<LightMapPolicyType> >(VertexFactory->GetType());
			DomainShader = InMaterialResource.GetShader<TBasePassDS<LightMapPolicyType> >(VertexFactory->GetType());
		}

		if (bEnableAtmosphericFog)
		{
			VertexShader = InMaterialResource.GetShader<TBasePassVS<LightMapPolicyType, true> >(InVertexFactory->GetType());
		}
		else
		{
			VertexShader = InMaterialResource.GetShader<TBasePassVS<LightMapPolicyType, false> >(InVertexFactory->GetType());
		}

		if (bEnableSkyLight)
		{
			PixelShader = InMaterialResource.GetShader<TBasePassPS<LightMapPolicyType, true> >(InVertexFactory->GetType());
		}
		else
		{
			PixelShader = InMaterialResource.GetShader<TBasePassPS<LightMapPolicyType, false> >(InVertexFactory->GetType());
		}

#if DO_GUARD_SLOW
		// Somewhat hacky
		if (SceneTextureMode == ESceneRenderTargetsMode::DontSet && !bEnableEditorPrimitiveDepthTest && InMaterialResource.IsUsedWithEditorCompositing())
		{
			SceneTextureMode = ESceneRenderTargetsMode::DontSetIgnoreBoundByEditorCompositing;
		}
#endif
	}

	// FMeshDrawingPolicy interface.

	bool Matches(const TBasePassDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) &&
			VertexShader == Other.VertexShader &&
			PixelShader == Other.PixelShader &&
			HullShader == Other.HullShader &&
			DomainShader == Other.DomainShader &&
			SceneTextureMode == Other.SceneTextureMode &&
			bAllowGlobalFog == Other.bAllowGlobalFog &&
			bEnableSkyLight == Other.bEnableSkyLight && 

			LightMapPolicy == Other.LightMapPolicy;
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FViewInfo* View, const ContextDataType PolicyContext) const
	{
		// Set the light-map policy.
		LightMapPolicy.Set(RHICmdList, VertexShader,bOverrideWithShaderComplexity ? NULL : PixelShader,VertexShader,PixelShader,VertexFactory,MaterialRenderProxy,View);

		VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, VertexFactory, *MaterialResource, *View, bAllowGlobalFog, SceneTextureMode);

		if(HullShader)
		{
			HullShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
		}

		if (DomainShader)
		{
			DomainShader->SetParameters(RHICmdList, MaterialRenderProxy, VertexFactory, *View);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			// If we are in the translucent pass then override the blend mode, otherwise maintain additive blending.
			if (IsTranslucentBlendMode(BlendMode))
			{
				RHICmdList.SetBlendState( TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
			}

			TShaderMapRef<FShaderComplexityAccumulatePS> ShaderComplexityPixelShader(View->ShaderMap);
			const uint32 NumPixelShaderInstructions = PixelShader->GetNumInstructions();
			const uint32 NumVertexShaderInstructions = VertexShader->GetNumInstructions();
			ShaderComplexityPixelShader->SetParameters(RHICmdList, NumVertexShaderInstructions, NumPixelShaderInstructions, View->GetFeatureLevel());
		}
		else
#endif
		{
			PixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*MaterialResource,View,BlendMode,bEnableEditorPrimitiveDepthTest,SceneTextureMode);

			switch(BlendMode)
			{
			default:
			case BLEND_Opaque:
				// Opaque materials are rendered together in the base pass, where the blend state is set at a higher level
				break;
			case BLEND_Masked:
				// Masked materials are rendered together in the base pass, where the blend state is set at a higher level
				break;
			case BLEND_Translucent:
				// Alpha channel is only needed for SeparateTranslucency, before this was preserving the alpha channel but we no longer store depth in the alpha channel so it's no problem

				RHICmdList.SetBlendState( TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Additive:
				// Add to the existing scene color
				// Alpha channel is only needed for SeparateTranslucency, before this was preserving the alpha channel but we no longer store depth in the alpha channel so it's no problem
				RHICmdList.SetBlendState( TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Modulate:
				// Modulate with the existing scene color, preserve destination alpha.
				RHICmdList.SetBlendState( TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
				break;
			};
		}
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
	{
		FPixelShaderRHIParamRef PixelShaderRHIRef = PixelShader->GetPixelShader();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			TShaderMapRef<FShaderComplexityAccumulatePS> ShaderComplexityAccumulatePixelShader(GetGlobalShaderMap(InFeatureLevel));
			PixelShaderRHIRef = ShaderComplexityAccumulatePixelShader->GetPixelShader();
		}
#endif

		return FBoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(), 
			VertexShader->GetVertexShader(),
			GETSAFERHISHADER_HULL(HullShader), 
			GETSAFERHISHADER_DOMAIN(DomainShader), 
			PixelShaderRHIRef,
			FGeometryShaderRHIRef());
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		bool bBackFace,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const
	{
		// Set the light-map policy's mesh-specific settings.
		LightMapPolicy.SetMesh(
			RHICmdList, 
			View,
			PrimitiveSceneProxy,
			VertexShader,
			bOverrideWithShaderComplexity ? NULL : PixelShader,
			VertexShader,
			PixelShader,
			VertexFactory,
			MaterialRenderProxy,
			ElementData.LightMapElementData);

		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
		VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy, Mesh,BatchElement);
		
		if(HullShader && DomainShader)
		{
			HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement);
			DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			// If we are in the translucent pass or rendering a masked material then override the blend mode, otherwise maintain opaque blending
			if (BlendMode != BLEND_Opaque)
			{
				// Add complexity to existing, keep alpha
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGB,BO_Add,BF_One,BF_One>::GetRHI());
			}

			const auto FeatureLevel = View.GetFeatureLevel();
			TShaderMapRef<FShaderComplexityAccumulatePS> ShaderComplexityPixelShader(View.ShaderMap);
			const uint32 NumPixelShaderInstructions = PixelShader->GetNumInstructions();
			const uint32 NumVertexShaderInstructions = VertexShader->GetNumInstructions();
			ShaderComplexityPixelShader->SetParameters(RHICmdList, NumVertexShaderInstructions,NumPixelShaderInstructions, FeatureLevel);
		}
		else
#endif
		{
			PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,BlendMode);
		}

		FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType(),PolicyContext);
	}

	friend int32 CompareDrawingPolicy(const TBasePassDrawingPolicy& A,const TBasePassDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(SceneTextureMode);
		COMPAREDRAWINGPOLICYMEMBERS(bAllowGlobalFog);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableSkyLight);

		return CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

protected:
	TBasePassVertexShaderBaseType<LightMapPolicyType>* VertexShader;
	TBasePassHS<LightMapPolicyType>* HullShader;
	TBasePassDS<LightMapPolicyType>* DomainShader;
	TBasePassPixelShaderBaseType<LightMapPolicyType>* PixelShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;

	ESceneRenderTargetsMode::Type SceneTextureMode;

	uint32 bAllowGlobalFog : 1;

	uint32 bEnableSkyLight : 1;

	/** Whether or not this policy is compositing editor primitives and needs to depth test against the scene geometry in the base pass pixel shader */
	uint32 bEnableEditorPrimitiveDepthTest : 1;
	/** Whether or not this policy enables atmospheric fog */
	uint32 bEnableAtmosphericFog : 1;
};

/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FBasePassOpaqueDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		/** Whether or not to perform depth test in the pixel shader */
		bool bEditorCompositeDepthTest;

		ESceneRenderTargetsMode::Type TextureMode;

		ContextType(bool bInEditorCompositeDepthTest, ESceneRenderTargetsMode::Type InTextureMode)
			: bEditorCompositeDepthTest( bInEditorCompositeDepthTest )
			, TextureMode( InTextureMode )
		{}
	};

	static void AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh);
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bBackFace,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};

/** The parameters used to process a base pass mesh. */
class FProcessBasePassMeshParameters
{
public:

	const FMeshBatch& Mesh;
	const uint64 BatchElementMask;
	const FMaterial* Material;
	const FPrimitiveSceneProxy* PrimitiveSceneProxy;
	EBlendMode BlendMode;
	EMaterialShadingModel ShadingModel;
	const bool bAllowFog;
	/** Whether or not to perform depth test in the pixel shader */
	const bool bEditorCompositeDepthTest;
	ESceneRenderTargetsMode::Type TextureMode;
	ERHIFeatureLevel::Type FeatureLevel;

	/** Initialization constructor. */
	FProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const FMaterial* InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		bool bInEditorCompositeDepthTest,
		ESceneRenderTargetsMode::Type InTextureMode,
		ERHIFeatureLevel::Type InFeatureLevel
		):
		Mesh(InMesh),
		BatchElementMask(Mesh.Elements.Num()==1 ? 1 : (1<<Mesh.Elements.Num())-1), // 1 bit set for each mesh element
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		BlendMode(InMaterial->GetBlendMode()),
		ShadingModel(InMaterial->GetShadingModel()),
		bAllowFog(InbAllowFog),
		bEditorCompositeDepthTest(bInEditorCompositeDepthTest),
		TextureMode(InTextureMode),
		FeatureLevel(InFeatureLevel)
	{
	}

	/** Initialization constructor. */
	FProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const uint64& InBatchElementMask,
		const FMaterial* InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		bool bInEditorCompositeDepthTest,
		ESceneRenderTargetsMode::Type InTextureMode,
		ERHIFeatureLevel::Type InFeatureLevel
		) :
		Mesh(InMesh),
		BatchElementMask(InBatchElementMask),
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		BlendMode(InMaterial->GetBlendMode()),
		ShadingModel(InMaterial->GetShadingModel()),
		bAllowFog(InbAllowFog),
		bEditorCompositeDepthTest(bInEditorCompositeDepthTest),
		TextureMode(InTextureMode),
		FeatureLevel(InFeatureLevel)
	{
	}
};

/** Processes a base pass mesh using an unknown light map policy, and unknown fog density policy. */
template<typename ProcessActionType>
void ProcessBasePassMesh(
	FRHICommandList& RHICmdList,
	const FProcessBasePassMeshParameters& Parameters,
	const ProcessActionType& Action
	)
{
	// Check for a cached light-map.
	const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
	const bool bNeedsSceneTextures = Parameters.Material->NeedsSceneTextures();
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

	// Skip rendering meshes that want to use scene textures in passes which can't bind scene textures
	// This should be avoided at a higher level when possible, because this is just a silent failure
	// This happens for example when rendering a thumbnail of an opaque post process material that uses scene textures
	if (!(Parameters.TextureMode == ESceneRenderTargetsMode::DontSet && bNeedsSceneTextures))
	{
		// Render self-shadowing only for >= SM4 and fallback to non-shadowed for lesser shader models
		if (bIsLitMaterial && Action.UseTranslucentSelfShadowing() && Parameters.FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			if (IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
					&& Action.AllowIndirectLightingCache()
					&& Parameters.PrimitiveSceneProxy)
				{
					// Apply cached point indirect lighting as well as self shadowing if needed
					Action.template Process<FSelfShadowedCachedPointIndirectLightingPolicy>(RHICmdList, Parameters, FSelfShadowedCachedPointIndirectLightingPolicy(), FSelfShadowedTranslucencyPolicy::ElementDataType(Action.GetTranslucentSelfShadow()));
				}
				else
				{
					Action.template Process<FSelfShadowedTranslucencyPolicy>(RHICmdList, Parameters, FSelfShadowedTranslucencyPolicy(), FSelfShadowedTranslucencyPolicy::ElementDataType(Action.GetTranslucentSelfShadow()));
				}
			}
		else
		{
			const FLightMapInteraction LightMapInteraction = (bAllowStaticLighting && Parameters.Mesh.LCI && bIsLitMaterial) 
				? Parameters.Mesh.LCI->GetLightMapInteraction(Parameters.FeatureLevel) 
				: FLightMapInteraction();

			// force LQ lightmaps based on system settings
			const bool bAllowHighQualityLightMaps = AllowHighQualityLightmaps(Parameters.FeatureLevel) && LightMapInteraction.AllowsHighQualityLightmaps();

			switch(LightMapInteraction.GetType())
			{
				case LMIT_Texture: 
					if( bAllowHighQualityLightMaps ) 
					{ 
						const FShadowMapInteraction ShadowMapInteraction = (bAllowStaticLighting && Parameters.Mesh.LCI && bIsLitMaterial) 
							? Parameters.Mesh.LCI->GetShadowMapInteraction() 
							: FShadowMapInteraction();

						if (ShadowMapInteraction.GetType() == SMIT_Texture)
						{
							Action.template Process< TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP> >(
								RHICmdList,
								Parameters,
								TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP>(),
								TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP>::ElementDataType(ShadowMapInteraction, LightMapInteraction));
						}
						else
						{
							Action.template Process< TLightMapPolicy<HQ_LIGHTMAP> >(RHICmdList, Parameters, TLightMapPolicy<HQ_LIGHTMAP>(), LightMapInteraction);
						}
					} 
					else 
					{ 
						Action.template Process< TLightMapPolicy<LQ_LIGHTMAP> >(RHICmdList, Parameters, TLightMapPolicy<LQ_LIGHTMAP>(), LightMapInteraction);
					} 
					break;
				default:
					{
						// Use simple dynamic lighting if enabled, which just renders an unshadowed directional light and a skylight
						if (bIsLitMaterial)
						{
							if (IsSimpleDynamicLightingEnabled())
							{
								Action.template Process<FSimpleDynamicLightingPolicy>(RHICmdList, Parameters, FSimpleDynamicLightingPolicy(), FSimpleDynamicLightingPolicy::ElementDataType());
							}
							else if (IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
								&& Action.AllowIndirectLightingCache()
								&& Parameters.PrimitiveSceneProxy)
							{
								const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation = Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
								const bool bPrimitiveIsMovable = Parameters.PrimitiveSceneProxy->IsMovable();

								// Use the indirect lighting cache shaders if the object has a cache allocation
								// This happens for objects with unbuilt lighting
								if ((IndirectLightingCacheAllocation && IndirectLightingCacheAllocation->IsValid())
									// Use the indirect lighting cache shaders if the object is movable, it may not have a cache allocation yet because that is done in InitViews
									// And movable objects are sometimes rendered in the static draw lists
									|| bPrimitiveIsMovable)
								{
									if (CanIndirectLightingCacheUseVolumeTexture(Parameters.FeatureLevel) 
										// Translucency forces point sample for pixel performance
										&& Action.AllowIndirectLightingCacheVolumeTexture()
										&& ((IndirectLightingCacheAllocation && !IndirectLightingCacheAllocation->bPointSample)
											|| (bPrimitiveIsMovable && Parameters.PrimitiveSceneProxy->GetIndirectLightingCacheQuality() == ILCQ_Volume)))
									{
										// Use a lightmap policy that supports reading indirect lighting from a volume texture for dynamic objects
										Action.template Process<FCachedVolumeIndirectLightingPolicy>(RHICmdList, Parameters, FCachedVolumeIndirectLightingPolicy(), FCachedVolumeIndirectLightingPolicy::ElementDataType());
									}
									else
									{
										// Use a lightmap policy that supports reading indirect lighting from a single SH sample
										Action.template Process<FCachedPointIndirectLightingPolicy>(RHICmdList, Parameters, FCachedPointIndirectLightingPolicy(), FCachedPointIndirectLightingPolicy::ElementDataType(false));
									}
								}
								else
								{
									Action.template Process<FNoLightMapPolicy>(RHICmdList, Parameters, FNoLightMapPolicy(), FNoLightMapPolicy::ElementDataType());
								}
							}
							else
							{
								Action.template Process<FNoLightMapPolicy>(RHICmdList, Parameters, FNoLightMapPolicy(), FNoLightMapPolicy::ElementDataType());
							}
						}
						else
						{
							Action.template Process<FNoLightMapPolicy>(RHICmdList, Parameters, FNoLightMapPolicy(), FNoLightMapPolicy::ElementDataType());
						}
					}
					break;
			};
		}
	}
}
