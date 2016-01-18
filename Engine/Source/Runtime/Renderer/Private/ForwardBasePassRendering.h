// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ForwardBasePassRendering.h: base pass rendering definitions.
=============================================================================*/

#pragma once

#include "LightMapRendering.h"
#include "ShaderBaseClasses.h"
#include "EditorCompositeParams.h"

class FBasePassFowardDynamicPointLightInfo;

enum EOutputFormat
{
	LDR_GAMMA_32,
	HDR_LINEAR_32,
	HDR_LINEAR_64,
};

#define MAX_BASEPASS_DYNAMIC_POINT_LIGHTS 4

/* Info for dynamic point lights rendered in base pass */
class FBasePassFowardDynamicPointLightInfo
{
public:
	FBasePassFowardDynamicPointLightInfo(const FPrimitiveSceneProxy* InSceneProxy);

	int32 NumDynamicPointLights;
	FVector4 LightPositionAndInvRadius[MAX_BASEPASS_DYNAMIC_POINT_LIGHTS];
	FVector4 LightColorAndFalloffExponent[MAX_BASEPASS_DYNAMIC_POINT_LIGHTS];
};

static bool ShouldCacheShaderByPlatformAndOutputFormat(EShaderPlatform Platform, EOutputFormat OutputFormat)
{
	bool bSupportsMobileHDR = IsMobileHDR();
	bool bShaderUsesLDR = (OutputFormat == LDR_GAMMA_32);
	bool bShaderUsesHDR = !bShaderUsesLDR;
	// Android ES2 uses intrinsic_GetHDR32bppEncodeModeES2 so doesn't need a HDR_LINEAR_32 permutation
	bool bIsAndroid32bpp = (OutputFormat == HDR_LINEAR_32) && (Platform == SP_OPENGL_ES2_ANDROID);

	// only cache this shader if the LDR/HDR output matches what we currently support.  IsMobileHDR can't change, so we don't need
	// the LDR shaders if we are doing HDR, and vice-versa.	Android doesn't need HDR_LINEAR_32 as it
	return (bShaderUsesLDR && !bSupportsMobileHDR) || (bShaderUsesHDR && bSupportsMobileHDR && !bIsAndroid32bpp);
}

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename VertexParametersType>
class TBasePassForForwardShadingVSPolicyParamType : public FMeshMaterialShader, public VertexParametersType
{
protected:

	TBasePassForForwardShadingVSPolicyParamType() {}
	TBasePassForForwardShadingVSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		VertexParametersType::Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
	}

public:

	// static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		VertexParametersType::Serialize(Ar);
		Ar << HeightFogParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FVertexFactory* VertexFactory,
		const FMaterial& InMaterialResource,
		const FSceneView& View,
		ESceneRenderTargetsMode::Type TextureMode
		)
	{
		HeightFogParameters.Set(RHICmdList, GetVertexShader(), &View);
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,InMaterialResource,View,TextureMode);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement, float DitheredLODTransitionValue)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DitheredLODTransitionValue);
	}

private:
	FHeightFogShaderParameters HeightFogParameters;
};

template<typename LightMapPolicyType>
class TBasePassForForwardShadingVSBaseType : public TBasePassForForwardShadingVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>
{
	typedef TBasePassForForwardShadingVSPolicyParamType<typename LightMapPolicyType::VertexParametersType> Super;

protected:

	TBasePassForForwardShadingVSBaseType() {}
	TBasePassForForwardShadingVSBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsMobilePlatform(Platform) && LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

template< typename LightMapPolicyType, EOutputFormat OutputFormat >
class TBasePassForForwardShadingVS : public TBasePassForForwardShadingVSBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassForForwardShadingVS,MeshMaterial);
public:
	
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{		
		return TBasePassForForwardShadingVSBaseType<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType) && ShouldCacheShaderByPlatformAndOutputFormat(Platform,OutputFormat);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		TBasePassForForwardShadingVSBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("OUTPUT_GAMMA_SPACE"), OutputFormat == LDR_GAMMA_32 ? 1u : 0u );
	}
	
	/** Initialization constructor. */
	TBasePassForForwardShadingVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TBasePassForForwardShadingVSBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TBasePassForForwardShadingVS() {}
};


/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename PixelParametersType, int32 NumDynamicPointLights>
class TBasePassForForwardShadingPSPolicyParamType : public FMeshMaterialShader, public PixelParametersType
{
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// We compile the point light shader combinations based on the project settings
		static auto* MobileDynamicPointLightsUseStaticBranchCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileDynamicPointLightsUseStaticBranch"));
		static auto* MobileNumDynamicPointLightsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileNumDynamicPointLights"));
		const bool bMobileDynamicPointLightsUseStaticBranch = (MobileDynamicPointLightsUseStaticBranchCVar->GetValueOnAnyThread() == 1);
		const int32 MobileNumDynamicPointLights = MobileNumDynamicPointLightsCVar->GetValueOnAnyThread();
		const bool bIsUnlit = Material->GetShadingModel() == MSM_Unlit;

		return IsMobilePlatform(Platform) &&
			(NumDynamicPointLights == 0 ||
			(!bIsUnlit && NumDynamicPointLights == INT32_MAX && bMobileDynamicPointLightsUseStaticBranch && MobileNumDynamicPointLights > 0) ||	// single shader for variable number of point lights
			(!bIsUnlit && NumDynamicPointLights <= MobileNumDynamicPointLights && !bMobileDynamicPointLightsUseStaticBranch));					// unique 1...N point light shaders
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		if (NumDynamicPointLights == INT32_MAX)
		{
			OutEnvironment.SetDefine(TEXT("MAX_DYNAMIC_POINT_LIGHTS"), (uint32)MAX_BASEPASS_DYNAMIC_POINT_LIGHTS);
			OutEnvironment.SetDefine(TEXT("VARIABLE_NUM_DYNAMIC_POINT_LIGHTS"), (uint32)1);
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("MAX_DYNAMIC_POINT_LIGHTS"), (uint32)NumDynamicPointLights);
			OutEnvironment.SetDefine(TEXT("VARIABLE_NUM_DYNAMIC_POINT_LIGHTS"), (uint32)0);
			OutEnvironment.SetDefine(TEXT("NUM_DYNAMIC_POINT_LIGHTS"), (uint32)NumDynamicPointLights);
		}
		// Modify compilation environment depending upon material shader quality level settings.
		ModifyCompilationEnvironmentForQualityLevel(Platform, Material->GetQualityLevel(), OutEnvironment);
	}

	/** Initialization constructor. */
	TBasePassForForwardShadingPSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		PixelParametersType::Bind(Initializer.ParameterMap);
		ReflectionCubemap.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap"));
		ReflectionSampler.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler"));
		EditorCompositeParams.Bind(Initializer.ParameterMap);
		LightPositionAndInvRadiusParameter.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		LightColorAndFalloffExponentParameter.Bind(Initializer.ParameterMap, TEXT("LightColorAndFalloffExponent"));
		if (NumDynamicPointLights == INT32_MAX)
		{
			NumDynamicPointLightsParameter.Bind(Initializer.ParameterMap, TEXT("NumDynamicPointLights"));
		}
	}
	TBasePassForForwardShadingPSPolicyParamType() {}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& MaterialResource, const FSceneView* View, ESceneRenderTargetsMode::Type TextureMode, bool bEnableEditorPrimitveDepthTest)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(),MaterialRenderProxy,MaterialResource,*View,TextureMode);
		EditorCompositeParams.SetParameters(RHICmdList, MaterialResource, View, bEnableEditorPrimitveDepthTest, GetPixelShader());
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement, float DitheredLODTransitionValue)
	{
		FRHIPixelShader* PixelShader = GetPixelShader();
		if (ReflectionCubemap.IsBound())
		{
			FTexture* ReflectionTexture = GBlackTextureCube;
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy ? Proxy->GetPrimitiveSceneInfo() : NULL;

			if (PrimitiveSceneInfo 
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap->IsInitialized())
			{
				ReflectionTexture = PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap;
			}

			// Set the reflection cubemap
			SetTextureParameter(RHICmdList, PixelShader, ReflectionCubemap, ReflectionSampler, ReflectionTexture);
		}

		if (NumDynamicPointLights > 0)
		{
			FBasePassFowardDynamicPointLightInfo LightInfo(Proxy);

			if (NumDynamicPointLights == INT32_MAX)
			{
				SetShaderValue(RHICmdList, PixelShader, NumDynamicPointLightsParameter, LightInfo.NumDynamicPointLights);
			}

			// Set dynamic point lights
			SetShaderValueArray(RHICmdList, PixelShader, LightPositionAndInvRadiusParameter, LightInfo.LightPositionAndInvRadius, LightInfo.NumDynamicPointLights);
			SetShaderValueArray(RHICmdList, PixelShader, LightColorAndFalloffExponentParameter, LightInfo.LightColorAndFalloffExponent, LightInfo.NumDynamicPointLights);
		}

		FMeshMaterialShader::SetMesh(RHICmdList, PixelShader,VertexFactory,View,Proxy,BatchElement,DitheredLODTransitionValue);		
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		PixelParametersType::Serialize(Ar);
		Ar << ReflectionCubemap;
		Ar << ReflectionSampler;
		Ar << EditorCompositeParams;
		Ar << LightPositionAndInvRadiusParameter;
		Ar << LightColorAndFalloffExponentParameter;
		if (NumDynamicPointLights == INT32_MAX)
		{
			Ar << NumDynamicPointLightsParameter;
		}
		return bShaderHasOutdatedParameters;
	}

private:

	static bool ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment);

	FShaderResourceParameter ReflectionCubemap;
	FShaderResourceParameter ReflectionSampler;
	FEditorCompositingParameters EditorCompositeParams;
	FShaderParameter LightPositionAndInvRadiusParameter;
	FShaderParameter LightColorAndFalloffExponentParameter;
	FShaderParameter NumDynamicPointLightsParameter;
};

template<typename LightMapPolicyType, int32 NumDynamicPointLights>
class TBasePassForForwardShadingPSBaseType : public TBasePassForForwardShadingPSPolicyParamType<typename LightMapPolicyType::PixelParametersType, NumDynamicPointLights>
{
	typedef TBasePassForForwardShadingPSPolicyParamType<typename LightMapPolicyType::PixelParametersType, NumDynamicPointLights> Super;

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCache(Platform, Material, VertexFactoryType) 
			&& Super::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TBasePassForForwardShadingPSBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}
	TBasePassForForwardShadingPSBaseType() {}
};

template< typename LightMapPolicyType, EOutputFormat OutputFormat, bool bEnableSkyLight, int32 NumDynamicPointLights>
class TBasePassForForwardShadingPS : public TBasePassForForwardShadingPSBaseType<LightMapPolicyType, NumDynamicPointLights>
{
	DECLARE_SHADER_TYPE(TBasePassForForwardShadingPS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{		
		// Only compile skylight version for lit materials on ES2 (Metal) or higher
		const bool bShouldCacheBySkylight = !bEnableSkyLight || (Material->GetShadingModel() != MSM_Unlit);

		return TBasePassForForwardShadingPSBaseType<LightMapPolicyType, NumDynamicPointLights>::ShouldCache(Platform, Material, VertexFactoryType) && ShouldCacheShaderByPlatformAndOutputFormat(Platform, OutputFormat) && bShouldCacheBySkylight;
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{		
		TBasePassForForwardShadingPSBaseType<LightMapPolicyType, NumDynamicPointLights>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), (uint32)(bEnableSkyLight ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("USE_32BPP_HDR"), OutputFormat == HDR_LINEAR_32 ? 1u : 0u );
		OutEnvironment.SetDefine(TEXT("OUTPUT_GAMMA_SPACE"), OutputFormat == LDR_GAMMA_32 ? 1u : 0u );
	}
	
	/** Initialization constructor. */
	TBasePassForForwardShadingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TBasePassForForwardShadingPSBaseType<LightMapPolicyType, NumDynamicPointLights>(Initializer)
	{}

	/** Default constructor. */
	TBasePassForForwardShadingPS() {}
};

/**
 * Get shader templates allowing to redirect between compatible shaders.
 */

template <typename LightMapPolicyType, int32 NumDynamicPointLights>
struct GetBasePassForForwardShadingShaders
{
	GetBasePassForForwardShadingShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	LightMapPolicyType LightMapPolicy, 
	bool bEnableSkyLight,
	TBasePassForForwardShadingVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>*& VertexShader,
	TBasePassForForwardShadingPSPolicyParamType<typename LightMapPolicyType::PixelParametersType, NumDynamicPointLights>*& PixelShader
	)
	{
		if (IsMobileHDR32bpp() && !GSupportsHDR32bppEncodeModeIntrinsic)
		{
			VertexShader = Material.GetShader<TBasePassForForwardShadingVS<LightMapPolicyType, HDR_LINEAR_64> >(VertexFactoryType);

			if (bEnableSkyLight)
			{
				PixelShader = Material.GetShader< TBasePassForForwardShadingPS<LightMapPolicyType, HDR_LINEAR_32, true, NumDynamicPointLights> >(VertexFactoryType);
			}
			else
			{
				PixelShader = Material.GetShader< TBasePassForForwardShadingPS<LightMapPolicyType, HDR_LINEAR_32, false, NumDynamicPointLights> >(VertexFactoryType);
			}
		}
		else if (IsMobileHDR())
		{
			VertexShader = Material.GetShader<TBasePassForForwardShadingVS<LightMapPolicyType, HDR_LINEAR_64> >(VertexFactoryType);

			if (bEnableSkyLight)
			{
				PixelShader = Material.GetShader< TBasePassForForwardShadingPS<LightMapPolicyType, HDR_LINEAR_64, true, NumDynamicPointLights> >(VertexFactoryType);
			}
			else
			{
				PixelShader = Material.GetShader< TBasePassForForwardShadingPS<LightMapPolicyType, HDR_LINEAR_64, false, NumDynamicPointLights> >(VertexFactoryType);
			}
		}
		else
		{
			VertexShader = Material.GetShader<TBasePassForForwardShadingVS<LightMapPolicyType, LDR_GAMMA_32> >(VertexFactoryType);

			if (bEnableSkyLight)
			{
				PixelShader = Material.GetShader< TBasePassForForwardShadingPS<LightMapPolicyType, LDR_GAMMA_32, true, NumDynamicPointLights> >(VertexFactoryType);
			}
			else
			{
				PixelShader = Material.GetShader< TBasePassForForwardShadingPS<LightMapPolicyType, LDR_GAMMA_32, false, NumDynamicPointLights> >(VertexFactoryType);
			}			
		}
	}
};

// Only using a struct/class allows partial specialisation here
template <int32 NumDynamicPointLights>
struct GetBasePassForForwardShadingShaders<FUniformLightMapPolicy, NumDynamicPointLights>
{
	GetBasePassForForwardShadingShaders(
		const FMaterial& Material, 
		FVertexFactoryType* VertexFactoryType, 
		FUniformLightMapPolicy LightMapPolicy, 
		bool bEnableSkyLight,
		TBasePassForForwardShadingVSPolicyParamType<typename FUniformLightMapPolicy::VertexParametersType>*& VertexShader,
		TBasePassForForwardShadingPSPolicyParamType<typename FUniformLightMapPolicy::PixelParametersType, NumDynamicPointLights>*& PixelShader
		);
};

template <ELightMapPolicyType Policy, int32 NumDynamicPointLights>
void GetUniformBasePassorForwardShadingShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	bool bEnableSkyLight,
	TBasePassForForwardShadingVSPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TBasePassForForwardShadingPSPolicyParamType<FUniformLightMapPolicyShaderParametersType, NumDynamicPointLights>*& PixelShader
	)
{
	if (IsMobileHDR32bpp() && !GSupportsHDR32bppEncodeModeIntrinsic)
	{
		VertexShader = Material.GetShader<TBasePassForForwardShadingVS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = Material.GetShader< TBasePassForForwardShadingPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_32, true, NumDynamicPointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = Material.GetShader< TBasePassForForwardShadingPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_32, false, NumDynamicPointLights> >(VertexFactoryType);
		}
	}
	else if (IsMobileHDR())
	{
		VertexShader = Material.GetShader<TBasePassForForwardShadingVS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = Material.GetShader< TBasePassForForwardShadingPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, true, NumDynamicPointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = Material.GetShader< TBasePassForForwardShadingPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, false, NumDynamicPointLights> >(VertexFactoryType);
		}	
	}
	else
	{
		VertexShader = Material.GetShader<TBasePassForForwardShadingVS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = Material.GetShader< TBasePassForForwardShadingPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, true, NumDynamicPointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = Material.GetShader< TBasePassForForwardShadingPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, false, NumDynamicPointLights> >(VertexFactoryType);
		}			
	}
}

template <int32 NumDynamicPointLights>
GetBasePassForForwardShadingShaders<FUniformLightMapPolicy, NumDynamicPointLights>::GetBasePassForForwardShadingShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	FUniformLightMapPolicy LightMapPolicy, 
	bool bEnableSkyLight,
	TBasePassForForwardShadingVSPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TBasePassForForwardShadingPSPolicyParamType<FUniformLightMapPolicyShaderParametersType, NumDynamicPointLights>*& PixelShader
	)
{
	switch (LightMapPolicy.GetIndirectPolicy())
	{
	case LMP_LQ_LIGHTMAP:
		GetUniformBasePassorForwardShadingShaders<LMP_LQ_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
		GetUniformBasePassorForwardShadingShaders<LMP_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
		GetUniformBasePassorForwardShadingShaders<LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_DIRECTIONAL_INDIRECT:
		GetUniformBasePassorForwardShadingShaders<LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_DIRECTIONAL_INDIRECT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_DIRECTIONAL_CSM_INDIRECT:
		GetUniformBasePassorForwardShadingShaders<LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_DIRECTIONAL_CSM_INDIRECT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOVABLE_DIRECTIONAL_LIGHT:
		GetUniformBasePassorForwardShadingShaders<LMP_MOVABLE_DIRECTIONAL_LIGHT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOVABLE_DIRECTIONAL_LIGHT_CSM:
		GetUniformBasePassorForwardShadingShaders<LMP_MOVABLE_DIRECTIONAL_LIGHT_CSM, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
		GetUniformBasePassorForwardShadingShaders<LMP_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
		GetUniformBasePassorForwardShadingShaders<LMP_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	default:										
		check(false);
	case LMP_NO_LIGHTMAP:
		GetUniformBasePassorForwardShadingShaders<LMP_NO_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	}
}

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType, int32 NumDynamicPointLights>
class TBasePassForForwardShadingDrawingPolicy : public FMeshDrawingPolicy
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
	TBasePassForForwardShadingDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		ESceneRenderTargetsMode::Type InSceneTextureMode,
		bool bInEnableSkyLight,
		bool bOverrideWithShaderComplexity,
		ERHIFeatureLevel::Type FeatureLevel,
		bool bInEnableEditorPrimitiveDepthTest = false
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,bOverrideWithShaderComplexity),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode),
		SceneTextureMode(InSceneTextureMode),
		bEnableEditorPrimitiveDepthTest(bInEnableEditorPrimitiveDepthTest)
	{
		GetBasePassForForwardShadingShaders<LightMapPolicyType, NumDynamicPointLights>(
					InMaterialResource, 
					VertexFactory->GetType(), 
					InLightMapPolicy, 
					bInEnableSkyLight, 
					VertexShader,
					PixelShader
					);

#if DO_GUARD_SLOW
		// Somewhat hacky
		if (SceneTextureMode == ESceneRenderTargetsMode::DontSet && !bEnableEditorPrimitiveDepthTest && InMaterialResource.IsUsedWithEditorCompositing())
		{
			SceneTextureMode = ESceneRenderTargetsMode::DontSetIgnoreBoundByEditorCompositing;
		}
#endif
	}

	// FMeshDrawingPolicy interface.

	bool Matches(const TBasePassForForwardShadingDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) &&
			VertexShader == Other.VertexShader &&
			PixelShader == Other.PixelShader &&
			LightMapPolicy == Other.LightMapPolicy &&
			SceneTextureMode == Other.SceneTextureMode;
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FViewInfo* View, const ContextDataType PolicyContext) const
	{
		VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, VertexFactory, *MaterialResource, *View, SceneTextureMode);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			if (BlendMode == BLEND_Opaque)
			{
				RHICmdList.SetBlendState( TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
			}
			else
			{
				// Add complexity to existing
				RHICmdList.SetBlendState( TStaticBlendState<CW_RGBA,BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
			}

			const uint32 NumPixelShaderInstructions = PixelShader->GetNumInstructions();
			const uint32 NumVertexShaderInstructions = VertexShader->GetNumInstructions();
			FShaderComplexityAccumulatePS::SetParameters(View->ShaderMap,RHICmdList,NumVertexShaderInstructions,NumPixelShaderInstructions,GetQuadOverdrawMode(),View->GetFeatureLevel());
		}
		else
#endif
		{
			PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View, SceneTextureMode, bEnableEditorPrimitiveDepthTest);
			bool bEncodedHDR = IsMobileHDR32bpp() && !IsMobileHDRMosaic();
			if (bEncodedHDR == false)
			{
				switch (BlendMode)
				{
				default:
				case BLEND_Opaque:
					// Opaque materials are rendered together in the base pass, where the blend state is set at a higher level
					break;
				case BLEND_Masked:
					// Masked materials are rendered together in the base pass, where the blend state is set at a higher level
					break;
				case BLEND_Translucent:
					RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
					break;
				case BLEND_Additive:
					// Add to the existing scene color
					RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
					break;
				case BLEND_Modulate:
					// Modulate with the existing scene color
					RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
					break;
				};
			}
		}
		
		// Set the light-map policy.
		LightMapPolicy.Set(RHICmdList, VertexShader,bOverrideWithShaderComplexity ? NULL : PixelShader,VertexShader,PixelShader,VertexFactory,MaterialRenderProxy,View);		
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
	{
		FPixelShaderRHIParamRef PixelShaderRHIRef = PixelShader->GetPixelShader();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			PixelShaderRHIRef = FShaderComplexityAccumulatePS::GetPixelShader(GetGlobalShaderMap(InFeatureLevel),GetQuadOverdrawMode())->GetPixelShader();
		}
#endif

		return FBoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(), 
			VertexShader->GetVertexShader(),
			FHullShaderRHIRef(), 
			FDomainShaderRHIRef(), 
			PixelShaderRHIRef,
			FGeometryShaderRHIRef());
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		bool bBackFace,
		float DitheredLODTransitionValue,
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
		VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DitheredLODTransitionValue);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bOverrideWithShaderComplexity)
		{
			// If we are in the translucent pass or rendering a masked material then override the blend mode, otherwise maintain opaque blending
			if (BlendMode != BLEND_Opaque)
			{
				// Add complexity to existing, keep alpha
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGB,BO_Add,BF_One,BF_One>::GetRHI());
			}

			const uint32 NumPixelShaderInstructions = PixelShader->GetNumInstructions();
			const uint32 NumVertexShaderInstructions = VertexShader->GetNumInstructions();
			FShaderComplexityAccumulatePS::SetParameters(GetGlobalShaderMap(View.FeatureLevel),RHICmdList,NumVertexShaderInstructions,NumPixelShaderInstructions,GetQuadOverdrawMode(),View.GetFeatureLevel());
		}
		else
#endif
		{
			PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DitheredLODTransitionValue);
		}

		FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace, DitheredLODTransitionValue,FMeshDrawingPolicy::ElementDataType(),PolicyContext);
	}

	friend int32 CompareDrawingPolicy(const TBasePassForForwardShadingDrawingPolicy& A,const TBasePassForForwardShadingDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(SceneTextureMode);

		return CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

protected:
	TBasePassForForwardShadingVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>* VertexShader;
	TBasePassForForwardShadingPSPolicyParamType<typename LightMapPolicyType::PixelParametersType, NumDynamicPointLights>* PixelShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;
	ESceneRenderTargetsMode::Type SceneTextureMode;
	/** Whether or not this policy is compositing editor primitives and needs to depth test against the scene geometry in the base pass pixel shader */
	uint32 bEnableEditorPrimitiveDepthTest : 1;
};

/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FBasePassForwardOpaqueDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		/** Whether or not to perform depth test in the pixel shader */
		bool bEditorCompositeDepthTest;

		ContextType(bool bInEditorCompositeDepthTest, ESceneRenderTargetsMode::Type InTextureMode) :
			bEditorCompositeDepthTest(bInEditorCompositeDepthTest),
			TextureMode(InTextureMode)
		{}

		ESceneRenderTargetsMode::Type TextureMode;
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

private:
	// templated version of DrawDynamicMesh on number of point lights
	template<int32 NumDynamicPointLights>
	static void DrawDynamicMeshTempl(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		const FMaterial* Material,
		bool bBackFace,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};

/** Processes a base pass mesh using an unknown light map policy, and unknown fog density policy. */
template<typename ProcessActionType, int32 NumDynamicPointLights>
void ProcessBasePassMeshForForwardShading(
	FRHICommandList& RHICmdList,
	const FProcessBasePassMeshParameters& Parameters,
	const ProcessActionType& Action
	)
{
	// Check for a cached light-map.
	const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
	if (bIsLitMaterial)
	{
		const FLightMapInteraction LightMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial) 
			? Parameters.Mesh.LCI->GetLightMapInteraction(Parameters.FeatureLevel) 
			: FLightMapInteraction();

		const FLightSceneInfo* SimpleDirectionalLight = Action.GetSimpleDirectionalLight();
		const bool bUseMovableLight = SimpleDirectionalLight && !SimpleDirectionalLight->Proxy->HasStaticShadowing();

		if (LightMapInteraction.GetType() == LMIT_Texture)
		{
			// Lightmap path
			if (bUseMovableLight)
			{
				// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
				if (SimpleDirectionalLight && SimpleDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows())
				{
					Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else
			{
				const FShadowMapInteraction ShadowMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial)
					? Parameters.Mesh.LCI->GetShadowMapInteraction()
					: FShadowMapInteraction();

				if (ShadowMapInteraction.GetType() == SMIT_Texture)
				{
					Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_LQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
		else if (IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
			&& Parameters.PrimitiveSceneProxy
			// Movable objects need to get their GI from the indirect lighting cache
			&& Parameters.PrimitiveSceneProxy->IsMovable())
		{
			if (bUseMovableLight)
			{
				if (SimpleDirectionalLight && SimpleDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows())
				{
					Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_DIRECTIONAL_CSM_INDIRECT), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_DIRECTIONAL_INDIRECT), Parameters.Mesh.LCI);
				}
			}
			else
			{
				Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT), Parameters.Mesh.LCI);
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
		else if (bUseMovableLight)
		{
			// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
			if (SimpleDirectionalLight && SimpleDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows())
			{
				Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOVABLE_DIRECTIONAL_LIGHT_CSM), Parameters.Mesh.LCI);
			}
			else
			{
				Action.template Process<FUniformLightMapPolicy, NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOVABLE_DIRECTIONAL_LIGHT), Parameters.Mesh.LCI);
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
	}

	// Default to NoLightmapPolicy
	Action.template Process<FUniformLightMapPolicy, 0>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
}
