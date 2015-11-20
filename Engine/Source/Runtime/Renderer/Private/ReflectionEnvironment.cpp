// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Reflection Environment - feature that provides HDR glossy reflections on any surfaces, leveraging precomputation to prefilter cubemaps of the scene
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessing.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "ScreenRendering.h"
#include "ScreenSpaceReflections.h"
#include "PostProcessTemporalAA.h"
#include "PostProcessDownsample.h"
#include "ReflectionEnvironment.h"
#include "ShaderParameterUtils.h"
#include "LightRendering.h"
#include "SceneUtils.h"
#include "LightPropagationVolumeBlendable.h"

/** Tile size for the reflection environment compute shader, tweaked for 680 GTX. */
const int32 GReflectionEnvironmentTileSizeX = 16;
const int32 GReflectionEnvironmentTileSizeY = 16;
extern ENGINE_API int32 GReflectionCaptureSize;

extern TAutoConsoleVariable<int32> CVarLPVMixing;

static TAutoConsoleVariable<int32> CVarDiffuseFromCaptures(
	TEXT("r.DiffuseFromCaptures"),
	0,
	TEXT("Apply indirect diffuse lighting from captures instead of lightmaps.\n")
	TEXT(" 0 is off (default), 1 is on"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarReflectionEnvironment(
	TEXT("r.ReflectionEnvironment"),
	1,
	TEXT("Whether to render the reflection environment feature, which implements local reflections through Reflection Capture actors.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on and blend with scene (default)")
	TEXT(" 2: on and overwrite scene (only in non-shipping builds)"),
	ECVF_RenderThreadSafe | ECVF_Scalability);


static TAutoConsoleVariable<int32> CVarDoTiledReflections(
	TEXT("r.DoTiledReflections"),
	1,
	TEXT("Compute Reflection Environment with Tiled compute shader..\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSkySpecularOcclusionStrength(
	TEXT("r.SkySpecularOcclusionStrength"),
	1,
	TEXT("Strength of skylight specular occlusion from DFAO (default is 1.0)"),
	ECVF_RenderThreadSafe);

// to avoid having direct access from many places
static int GetReflectionEnvironmentCVar()
{
	int32 RetVal = CVarReflectionEnvironment.GetValueOnAnyThread();

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Disabling the debug part of this CVar when in shipping
	if (RetVal == 2)
	{
		RetVal = 1;
	}
#endif

	return RetVal;
}

bool IsReflectionEnvironmentAvailable(ERHIFeatureLevel::Type InFeatureLevel)
{
	return (InFeatureLevel >= ERHIFeatureLevel::SM4) && (GetReflectionEnvironmentCVar() != 0);
}

bool IsReflectionCaptureAvailable()
{
	static IConsoleVariable* AllowStaticLightingVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowStaticLighting"));
	return (!AllowStaticLightingVar || AllowStaticLightingVar->GetInt() != 0);
}

void FReflectionEnvironmentCubemapArray::InitDynamicRHI()
{
	if (GetFeatureLevel() >= ERHIFeatureLevel::SM5)
	{

		const int32 NumReflectionCaptureMips = FMath::CeilLogTwo(GReflectionCaptureSize) + 1;

		ReleaseCubeArray();

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::CreateCubemapDesc(
		GReflectionCaptureSize,
		// Alpha stores sky mask
		PF_FloatRGBA, 
		FClearValueBinding::None,
		TexCreate_None,
		TexCreate_None,
		false, 
		// Cubemap array of 1 produces a regular cubemap, so guarantee it will be allocated as an array
		FMath::Max<uint32>(MaxCubemaps, 2),
		NumReflectionCaptureMips));

		Desc.AutoWritable = false;
	
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		// Allocate TextureCubeArray for the scene's reflection captures
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ReflectionEnvs, TEXT("ReflectionEnvs"));
	}
}

void FReflectionEnvironmentCubemapArray::ReleaseCubeArray()
{
	// it's unlikely we can reuse the TextureCubeArray so when we release it we want to really remove it
	GRenderTargetPool.FreeUnusedResource(ReflectionEnvs);
}

void FReflectionEnvironmentCubemapArray::ReleaseDynamicRHI()
{
	ReleaseCubeArray();
}

void FReflectionEnvironmentCubemapArray::UpdateMaxCubemaps(uint32 InMaxCubemaps)
{
	MaxCubemaps = InMaxCubemaps;

	// Reallocate the cubemap array
	if (IsInitialized())
	{
		UpdateRHI();
	}
	else
	{
		InitResource();
	}
}

class FDistanceFieldAOSpecularOcclusionParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		BentNormalAOTexture.Bind(ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(ParameterMap, TEXT("BentNormalAOSampler"));
		ApplyBentNormalAO.Bind(ParameterMap, TEXT("ApplyBentNormalAO"));
		InvSkySpecularOcclusionStrength.Bind(ParameterMap, TEXT("InvSkySpecularOcclusionStrength"));
		MinSkySpecularOcclusion.Bind(ParameterMap, TEXT("MinSkySpecularOcclusion"));
	}

	template<typename ShaderRHIParamRef>
	void SetParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, float SkySpecularOcclusionStrength, float MinOcclusionValue)
	{
		FTextureRHIParamRef BentNormalAO = GWhiteTexture->TextureRHI;
		bool bApplyBentNormalAO = false;

		if (DynamicBentNormalAO)
		{
			BentNormalAO = DynamicBentNormalAO->GetRenderTargetItem().ShaderResourceTexture;
			bApplyBentNormalAO = true;
		}

		SetTextureParameter(RHICmdList, ShaderRHI, BentNormalAOTexture, BentNormalAOSampler, TStaticSamplerState<SF_Point>::GetRHI(), BentNormalAO);
		SetShaderValue(RHICmdList, ShaderRHI, ApplyBentNormalAO, bApplyBentNormalAO ? 1.0f : 0.0f);
		SetShaderValue(RHICmdList, ShaderRHI, InvSkySpecularOcclusionStrength, 1.0f / FMath::Max(SkySpecularOcclusionStrength, .1f));
		SetShaderValue(RHICmdList, ShaderRHI, MinSkySpecularOcclusion, MinOcclusionValue);
	}

	friend FArchive& operator<<(FArchive& Ar,FDistanceFieldAOSpecularOcclusionParameters& P)
	{
		Ar << P.BentNormalAOTexture << P.BentNormalAOSampler << P.ApplyBentNormalAO << P.InvSkySpecularOcclusionStrength << P.MinSkySpecularOcclusion;
		return Ar;
	}

private:
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderParameter ApplyBentNormalAO;
	FShaderParameter InvSkySpecularOcclusionStrength;
	FShaderParameter MinSkySpecularOcclusion;
};

struct FReflectionCaptureSortData
{
	uint32 Guid;
	int32 CaptureIndex;
	FVector4 PositionAndRadius;
	FVector4 CaptureProperties;
	FMatrix BoxTransform;
	FVector4 BoxScales;
	FTexture* SM4FullHDRCubemap;

	bool operator < (const FReflectionCaptureSortData& Other) const
	{
		if( PositionAndRadius.W != Other.PositionAndRadius.W )
		{
			return PositionAndRadius.W < Other.PositionAndRadius.W;
		}
		else
		{
			return Guid < Other.Guid;
		}
	}
};

/** Per-reflection capture data needed by the shader. */
BEGIN_UNIFORM_BUFFER_STRUCT(FReflectionCaptureData,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,PositionAndRadius,[GMaxNumReflectionCaptures])
	// R is brightness, G is array index, B is shape
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,CaptureProperties,[GMaxNumReflectionCaptures])
	// Stores the box transform for a box shape, other data is packed for other shapes
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FMatrix,BoxTransform,[GMaxNumReflectionCaptures])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,BoxScales,[GMaxNumReflectionCaptures])
END_UNIFORM_BUFFER_STRUCT(FReflectionCaptureData)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FReflectionCaptureData,TEXT("ReflectionCapture"));

/** Compute shader that does tiled deferred culling of reflection captures, then sorts and composites them. */
class FReflectionEnvironmentTiledDeferredCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FReflectionEnvironmentTiledDeferredCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GReflectionEnvironmentTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GReflectionEnvironmentTileSizeY);
		OutEnvironment.SetDefine(TEXT("MAX_CAPTURES"), GMaxNumReflectionCaptures);
		OutEnvironment.SetDefine(TEXT("TILED_DEFERRED_CULL_SHADER"), 1);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FReflectionEnvironmentTiledDeferredCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ReflectionEnvironmentColorTexture.Bind(Initializer.ParameterMap,TEXT("ReflectionEnvironmentColorTexture"));
		ReflectionEnvironmentColorSampler.Bind(Initializer.ParameterMap,TEXT("ReflectionEnvironmentColorSampler"));
		ScreenSpaceReflections.Bind(Initializer.ParameterMap, TEXT("ScreenSpaceReflections"));
		InSceneColor.Bind(Initializer.ParameterMap, TEXT("InSceneColor"));
		OutSceneColor.Bind(Initializer.ParameterMap, TEXT("OutSceneColor"));
		NumCaptures.Bind(Initializer.ParameterMap, TEXT("NumCaptures"));
		ViewDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
		PreIntegratedGF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGF"));
		PreIntegratedGFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGFSampler"));
		SkyLightParameters.Bind(Initializer.ParameterMap);
		SpecularOcclusionParameters.Bind(Initializer.ParameterMap);
	}

	FReflectionEnvironmentTiledDeferredCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FTextureRHIParamRef SSRTexture, TArray<FReflectionCaptureSortData>& SortData, FUnorderedAccessViewRHIParamRef OutSceneColorUAV, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		FScene* Scene = (FScene*)View.Family->Scene;

		check(Scene->ReflectionSceneData.CubemapArray.IsValid());
		check(Scene->ReflectionSceneData.CubemapArray.GetRenderTarget().IsValid());

		FSceneRenderTargetItem& CubemapArray = Scene->ReflectionSceneData.CubemapArray.GetRenderTarget();

		SetTextureParameter(
			RHICmdList, 
			ShaderRHI, 
			ReflectionEnvironmentColorTexture, 
			ReflectionEnvironmentColorSampler, 
			TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			CubemapArray.ShaderResourceTexture);

		SetTextureParameter(RHICmdList, ShaderRHI, ScreenSpaceReflections, SSRTexture );

		SetTextureParameter(RHICmdList, ShaderRHI, InSceneColor, FSceneRenderTargets::Get(RHICmdList).GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture );

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, OutSceneColorUAV);
		OutSceneColor.SetTexture(RHICmdList, ShaderRHI, NULL, OutSceneColorUAV);

		SetShaderValue(RHICmdList, ShaderRHI, ViewDimensionsParameter, View.ViewRect);

		FReflectionCaptureData SamplePositionsBuffer;

		for (int32 CaptureIndex = 0; CaptureIndex < SortData.Num(); CaptureIndex++)
		{
			SamplePositionsBuffer.PositionAndRadius[CaptureIndex] = SortData[CaptureIndex].PositionAndRadius;
			SamplePositionsBuffer.CaptureProperties[CaptureIndex] = SortData[CaptureIndex].CaptureProperties;
			SamplePositionsBuffer.BoxTransform[CaptureIndex] = SortData[CaptureIndex].BoxTransform;
			SamplePositionsBuffer.BoxScales[CaptureIndex] = SortData[CaptureIndex].BoxScales;
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FReflectionCaptureData>(), SamplePositionsBuffer);
		SetShaderValue(RHICmdList, ShaderRHI, NumCaptures, SortData.Num());

		SetTextureParameter(RHICmdList, ShaderRHI, PreIntegratedGF, PreIntegratedGFSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture);
	
		SkyLightParameters.SetParameters(RHICmdList, ShaderRHI, Scene, View.Family->EngineShowFlags.SkyLighting);

		const float MinOcclusion = Scene->SkyLight ? Scene->SkyLight->MinOcclusion : 0;
		SpecularOcclusionParameters.SetParameters(RHICmdList, ShaderRHI, DynamicBentNormalAO, CVarSkySpecularOcclusionStrength.GetValueOnRenderThread(), MinOcclusion);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef OutSceneColorUAV)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutSceneColorUAV);
		OutSceneColor.UnsetUAV(RHICmdList, ShaderRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ReflectionEnvironmentColorTexture;
		Ar << ReflectionEnvironmentColorSampler;
		Ar << ScreenSpaceReflections;
		Ar << InSceneColor;
		Ar << OutSceneColor;
		Ar << NumCaptures;
		Ar << ViewDimensionsParameter;
		Ar << PreIntegratedGF;
		Ar << PreIntegratedGFSampler;
		Ar << SkyLightParameters;
		Ar << SpecularOcclusionParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter ReflectionEnvironmentColorTexture;
	FShaderResourceParameter ReflectionEnvironmentColorSampler;
	FShaderResourceParameter ScreenSpaceReflections;
	FShaderResourceParameter InSceneColor;
	FRWShaderParameter OutSceneColor;
	FShaderParameter NumCaptures;
	FShaderParameter ViewDimensionsParameter;
	FShaderResourceParameter PreIntegratedGF;
	FShaderResourceParameter PreIntegratedGFSampler;
	FSkyLightReflectionParameters SkyLightParameters;
	FDistanceFieldAOSpecularOcclusionParameters SpecularOcclusionParameters;
};

template< uint32 bUseLightmaps, uint32 bUseClearCoat, uint32 bBoxCapturesOnly, uint32 bSphereCapturesOnly >
class TReflectionEnvironmentTiledDeferredCS : public FReflectionEnvironmentTiledDeferredCS
{
	DECLARE_SHADER_TYPE(TReflectionEnvironmentTiledDeferredCS, Global);

	/** Default constructor. */
	TReflectionEnvironmentTiledDeferredCS() {}
public:
	TReflectionEnvironmentTiledDeferredCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FReflectionEnvironmentTiledDeferredCS(Initializer)
	{}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FReflectionEnvironmentTiledDeferredCS::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_LIGHTMAPS"), bUseLightmaps);
		OutEnvironment.SetDefine(TEXT("USE_CLEARCOAT"), bUseClearCoat);
		OutEnvironment.SetDefine(TEXT("HAS_BOX_CAPTURES"), bBoxCapturesOnly);
		OutEnvironment.SetDefine(TEXT("HAS_SPHERE_CAPTURES"), bSphereCapturesOnly);
	}
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(A, B, C, D) \
	typedef TReflectionEnvironmentTiledDeferredCS<A,B,C,D> TReflectionEnvironmentTiledDeferredCS##A##B##C##D; \
	IMPLEMENT_SHADER_TYPE(template<>,TReflectionEnvironmentTiledDeferredCS##A##B##C##D,TEXT("ReflectionEnvironmentComputeShaders"),TEXT("ReflectionEnvironmentTiledDeferredMain"),SF_Compute)

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 1, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 1, 1);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 1, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 1, 1);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 1, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 1, 1);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 1, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 1, 1);


template< uint32 bSSR, uint32 bReflectionEnv, uint32 bSkylight >
class FReflectionApplyPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FReflectionApplyPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("APPLY_SSR"), bSSR);
		OutEnvironment.SetDefine(TEXT("APPLY_REFLECTION_ENV"), bReflectionEnv);
		OutEnvironment.SetDefine(TEXT("APPLY_SKYLIGHT"), bSkylight);
	}

	/** Default constructor. */
	FReflectionApplyPS() {}

	/** Initialization constructor. */
	FReflectionApplyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		SkyLightParameters.Bind(Initializer.ParameterMap);
		ReflectionEnvTexture.Bind(Initializer.ParameterMap,TEXT("ReflectionEnvTexture"));
		ReflectionEnvSampler.Bind(Initializer.ParameterMap,TEXT("ReflectionEnvSampler"));
		ScreenSpaceReflectionsTexture.Bind(Initializer.ParameterMap,TEXT("ScreenSpaceReflectionsTexture"));
		ScreenSpaceReflectionsSampler.Bind(Initializer.ParameterMap,TEXT("ScreenSpaceReflectionsSampler"));
		PreIntegratedGF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGF"));
		PreIntegratedGFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGFSampler"));
		SpecularOcclusionParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FTextureRHIParamRef ReflectionEnv, FTextureRHIParamRef ScreenSpaceReflections, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		SkyLightParameters.SetParameters(RHICmdList, ShaderRHI, (FScene*)View.Family->Scene, true);
		
		SetTextureParameter(RHICmdList, ShaderRHI, ReflectionEnvTexture, ReflectionEnvSampler, TStaticSamplerState<SF_Point>::GetRHI(), ReflectionEnv );
		SetTextureParameter(RHICmdList, ShaderRHI, ScreenSpaceReflectionsTexture, ScreenSpaceReflectionsSampler, TStaticSamplerState<SF_Point>::GetRHI(), ScreenSpaceReflections );
		SetTextureParameter(RHICmdList, ShaderRHI, PreIntegratedGF, PreIntegratedGFSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture );
		
		FScene* Scene = (FScene*)View.Family->Scene;
		const float MinOcclusion = Scene->SkyLight ? Scene->SkyLight->MinOcclusion : 0;
		SpecularOcclusionParameters.SetParameters(RHICmdList, ShaderRHI, DynamicBentNormalAO, CVarSkySpecularOcclusionStrength.GetValueOnRenderThread(), MinOcclusion);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << SkyLightParameters;
		Ar << ReflectionEnvTexture;
		Ar << ReflectionEnvSampler;
		Ar << ScreenSpaceReflectionsTexture;
		Ar << ScreenSpaceReflectionsSampler;
		Ar << PreIntegratedGF;
		Ar << PreIntegratedGFSampler;
		Ar << SpecularOcclusionParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FSkyLightReflectionParameters SkyLightParameters;
	FShaderResourceParameter ReflectionEnvTexture;
	FShaderResourceParameter ReflectionEnvSampler;
	FShaderResourceParameter ScreenSpaceReflectionsTexture;
	FShaderResourceParameter ScreenSpaceReflectionsSampler;
	FShaderResourceParameter PreIntegratedGF;
	FShaderResourceParameter PreIntegratedGFSampler;
	FDistanceFieldAOSpecularOcclusionParameters SpecularOcclusionParameters;
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(A, B, C) \
	typedef FReflectionApplyPS<A,B,C> FReflectionApplyPS##A##B##C; \
	IMPLEMENT_SHADER_TYPE(template<>,FReflectionApplyPS##A##B##C,TEXT("ReflectionEnvironmentShaders"),TEXT("ReflectionApplyPS"),SF_Pixel);

IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,0,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,0,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,1,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,1,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,0,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,0,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,1,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,1,1);


class FReflectionCaptureSpecularBouncePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FReflectionCaptureSpecularBouncePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	FReflectionCaptureSpecularBouncePS() {}

public:
	FDeferredPixelShaderParameters DeferredParameters;

	/** Initialization constructor. */
	FReflectionCaptureSpecularBouncePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FReflectionCaptureSpecularBouncePS,TEXT("ReflectionEnvironmentShaders"),TEXT("SpecularBouncePS"),SF_Pixel);

template<bool bSphereCapture>
class TStandardDeferredReflectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TStandardDeferredReflectionPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SPHERE_CAPTURE"), (uint32)bSphereCapture);
		OutEnvironment.SetDefine(TEXT("BOX_CAPTURE"), (uint32)!bSphereCapture);
	}

	/** Default constructor. */
	TStandardDeferredReflectionPS() {}

	/** Initialization constructor. */
	TStandardDeferredReflectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CapturePositionAndRadius.Bind(Initializer.ParameterMap, TEXT("CapturePositionAndRadius"));
		CaptureProperties.Bind(Initializer.ParameterMap, TEXT("CaptureProperties"));
		CaptureBoxTransform.Bind(Initializer.ParameterMap, TEXT("CaptureBoxTransform"));
		CaptureBoxScales.Bind(Initializer.ParameterMap, TEXT("CaptureBoxScales"));
		CaptureArrayIndex.Bind(Initializer.ParameterMap, TEXT("CaptureArrayIndex"));
		ReflectionEnvironmentColorTexture.Bind(Initializer.ParameterMap, TEXT("ReflectionEnvironmentColorTexture"));
		ReflectionEnvironmentColorTextureArray.Bind(Initializer.ParameterMap, TEXT("ReflectionEnvironmentColorTextureArray"));
		ReflectionEnvironmentColorSampler.Bind(Initializer.ParameterMap, TEXT("ReflectionEnvironmentColorSampler"));
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FReflectionCaptureSortData& SortData)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		if (View.GetFeatureLevel() >= ERHIFeatureLevel::SM5)
		{
			FScene* Scene = (FScene*)View.Family->Scene;

			check(Scene->ReflectionSceneData.CubemapArray.IsValid());
			check(Scene->ReflectionSceneData.CubemapArray.GetRenderTarget().IsValid());

			FSceneRenderTargetItem& CubemapArray = Scene->ReflectionSceneData.CubemapArray.GetRenderTarget();

			SetTextureParameter(RHICmdList, ShaderRHI, ReflectionEnvironmentColorTextureArray, ReflectionEnvironmentColorSampler, TStaticSamplerState<SF_Trilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), CubemapArray.ShaderResourceTexture);
			SetShaderValue(RHICmdList, ShaderRHI, CaptureArrayIndex, SortData.CaptureIndex);
		}
		else
		{
			SetTextureParameter(RHICmdList, ShaderRHI, ReflectionEnvironmentColorTexture, ReflectionEnvironmentColorSampler, TStaticSamplerState<SF_Trilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), SortData.SM4FullHDRCubemap->TextureRHI);
		}

		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		SetShaderValue(RHICmdList, ShaderRHI, CapturePositionAndRadius, SortData.PositionAndRadius);
		SetShaderValue(RHICmdList, ShaderRHI, CaptureProperties, SortData.CaptureProperties);
		SetShaderValue(RHICmdList, ShaderRHI, CaptureBoxTransform, SortData.BoxTransform);
		SetShaderValue(RHICmdList, ShaderRHI, CaptureBoxScales, SortData.BoxScales);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CapturePositionAndRadius;
		Ar << CaptureProperties;
		Ar << CaptureBoxTransform;
		Ar << CaptureBoxScales;
		Ar << CaptureArrayIndex;
		Ar << ReflectionEnvironmentColorTexture;
		Ar << ReflectionEnvironmentColorTextureArray;
		Ar << ReflectionEnvironmentColorSampler;
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter CapturePositionAndRadius;
	FShaderParameter CaptureProperties;
	FShaderParameter CaptureBoxTransform;
	FShaderParameter CaptureBoxScales;
	FShaderParameter CaptureArrayIndex;
	FShaderResourceParameter ReflectionEnvironmentColorTexture;
	FShaderResourceParameter ReflectionEnvironmentColorTextureArray;
	FShaderResourceParameter ReflectionEnvironmentColorSampler;
	FDeferredPixelShaderParameters DeferredParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,TStandardDeferredReflectionPS<true>,TEXT("ReflectionEnvironmentShaders"),TEXT("StandardDeferredReflectionPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TStandardDeferredReflectionPS<false>,TEXT("ReflectionEnvironmentShaders"),TEXT("StandardDeferredReflectionPS"),SF_Pixel);

void FDeferredShadingSceneRenderer::RenderReflectionCaptureSpecularBounceForAllViews(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EUninitializedColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
	RHICmdList.SetRasterizerState(TStaticRasterizerState< FM_Solid, CM_None >::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState< false, CF_Always >::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState< CW_RGB, BO_Add, BF_One, BF_One >::GetRHI());

	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef< FPostProcessVS > VertexShader(ShaderMap);
	TShaderMapRef< FReflectionCaptureSpecularBouncePS > PixelShader(ShaderMap);

	static FGlobalBoundShaderState BoundShaderState;
	
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		
		PixelShader->SetParameters(RHICmdList, View);

		DrawRectangle( 
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			SceneContext.GetBufferSizeXY(),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}

	SceneContext.FinishRenderingSceneColor(RHICmdList);
}

bool FDeferredShadingSceneRenderer::ShouldDoReflectionEnvironment() const
{
	const ERHIFeatureLevel::Type FeatureLevel = Scene->GetFeatureLevel();

	return IsReflectionEnvironmentAvailable(FeatureLevel)
		&& Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num()
		&& ViewFamily.EngineShowFlags.ReflectionEnvironment
		&& (FeatureLevel == ERHIFeatureLevel::SM4 || Scene->ReflectionSceneData.CubemapArray.IsValid());
}

void GatherAndSortReflectionCaptures(const FScene* Scene, TArray<FReflectionCaptureSortData>& OutSortData, int32& OutNumBoxCaptures, int32& OutNumSphereCaptures)
{	
	OutSortData.Reset(Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num());
	OutNumBoxCaptures = 0;
	OutNumSphereCaptures = 0;

	const int32 MaxCubemaps = Scene->ReflectionSceneData.CubemapArray.GetMaxCubemaps();

	// Pack only visible reflection captures into the uniform buffer, each with an index to its cubemap array entry
	for (int32 ReflectionProxyIndex = 0; ReflectionProxyIndex < Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() && OutSortData.Num() < GMaxNumReflectionCaptures; ReflectionProxyIndex++)
	{
		FReflectionCaptureProxy* CurrentCapture = Scene->ReflectionSceneData.RegisteredReflectionCaptures[ReflectionProxyIndex];
		// Find the cubemap index this component was allocated with
		const FCaptureComponentSceneState* ComponentStatePtr = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Find(CurrentCapture->Component);

		if (ComponentStatePtr)
		{
			int32 CubemapIndex = ComponentStatePtr->CaptureIndex;
			check(CubemapIndex < MaxCubemaps);

			FReflectionCaptureSortData NewSortEntry;

			NewSortEntry.CaptureIndex = CubemapIndex;
			NewSortEntry.SM4FullHDRCubemap = NULL;
			NewSortEntry.Guid = CurrentCapture->Guid;
			NewSortEntry.PositionAndRadius = FVector4(CurrentCapture->Position, CurrentCapture->InfluenceRadius);
			float ShapeTypeValue = (float)CurrentCapture->Shape;
			NewSortEntry.CaptureProperties = FVector4(CurrentCapture->Brightness, CubemapIndex, ShapeTypeValue, 0);

			if (CurrentCapture->Shape == EReflectionCaptureShape::Plane)
			{
				//planes count as boxes in the compute shader.
				++OutNumBoxCaptures;
				NewSortEntry.BoxTransform = FMatrix(
					FPlane(CurrentCapture->ReflectionPlane),
					FPlane(CurrentCapture->ReflectionXAxisAndYScale),
					FPlane(0, 0, 0, 0),
					FPlane(0, 0, 0, 0));

				NewSortEntry.BoxScales = FVector4(0);
			}
			else if (CurrentCapture->Shape == EReflectionCaptureShape::Sphere)
			{
				++OutNumSphereCaptures;
			}
			else
			{
				++OutNumBoxCaptures;
				NewSortEntry.BoxTransform = CurrentCapture->BoxTransform;
				NewSortEntry.BoxScales = FVector4(CurrentCapture->BoxScales, CurrentCapture->BoxTransitionDistance);
			}

			OutSortData.Add(NewSortEntry);
		}
	}

	OutSortData.Sort();	
}

FReflectionEnvironmentTiledDeferredCS* SelectReflectionEnvironmentTiledDeferredCS(TShaderMap<FGlobalShaderType>* ShaderMap, bool bUseLightmaps, bool bUseClearCoat, bool bHasBoxCaptures, bool bHasSphereCaptures)
{
	FReflectionEnvironmentTiledDeferredCS* ComputeShader = nullptr;
	if (bUseLightmaps)
	{
		if (bUseClearCoat)
		{
			if (bHasBoxCaptures && bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 1, 1, 1> >(ShaderMap);
			}
			else if (bHasBoxCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 1, 1, 0> >(ShaderMap);
			}
			else if (bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 1, 0, 1> >(ShaderMap);
			}
			else
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 1, 0, 0> >(ShaderMap);
			}
		}
		else
		{
			if (bHasBoxCaptures && bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 0, 1, 1> >(ShaderMap);
			}
			else if (bHasBoxCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 0, 1, 0> >(ShaderMap);
			}
			else if (bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 0, 0, 1> >(ShaderMap);
			}
			else
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 0, 0, 0> >(ShaderMap);
			}
		}		
	}
	else
	{
		if (bUseClearCoat)
		{
			if (bHasBoxCaptures && bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 1, 1, 1> >(ShaderMap);
			}
			else if (bHasBoxCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 1, 1, 0> >(ShaderMap);
			}
			else if (bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 1, 0, 1> >(ShaderMap);
			}
			else
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 1, 0, 0> >(ShaderMap);
			}
		}
		else
		{
			if (bHasBoxCaptures && bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 0, 1, 1> >(ShaderMap);
			}
			else if (bHasBoxCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 0, 1, 0> >(ShaderMap);
			}
			else if (bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 0, 0, 1> >(ShaderMap);
			}
			else
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 0, 0, 0> >(ShaderMap);
			}
		}		
	}
	check(ComputeShader);
	return ComputeShader;
}

void FDeferredShadingSceneRenderer::RenderTiledDeferredImageBasedReflections(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bUseLightmaps = (AllowStaticLightingVar->GetValueOnRenderThread() == 1) && (CVarDiffuseFromCaptures.GetValueOnRenderThread() == 0);

	TRefCountPtr<IPooledRenderTarget> NewSceneColor;
	{
		SceneContext.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

		FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.TargetableFlags |= TexCreate_UAV;
		Desc.TargetableFlags |= TexCreate_NoFastClear;
		Desc.ClearValue = FClearValueBinding::None;

		// we don't create a new name to make it easier to use "vis SceneColor" and get the last HDRSceneColor
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, NewSceneColor, TEXT("SceneColor") );
	}

	// If we are in SM5, use the compute shader gather method
	for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		const uint32 bSSR = DoScreenSpaceReflections(Views[ViewIndex]);

		TRefCountPtr<IPooledRenderTarget> SSROutput = GSystemTextures.BlackDummy;
		if( bSSR )
		{
			ScreenSpaceReflections(RHICmdList, View, SSROutput);
		}

		// ReflectionEnv is assumed to be on when going into this method
		{
			SetRenderTarget(RHICmdList, NULL, NULL);

			FReflectionEnvironmentTiledDeferredCS* ComputeShader = NULL;
			// Render the reflection environment with tiled deferred culling
			TArray<FReflectionCaptureSortData> SortData;
			int32 NumBoxCaptures = 0;
			int32 NumSphereCaptures = 0;
			GatherAndSortReflectionCaptures(Scene, SortData, NumBoxCaptures, NumSphereCaptures);

			bool bHasBoxCaptures = (NumBoxCaptures > 0);
			bool bHasSphereCaptures = (NumSphereCaptures > 0);
			bool bNeedsClearCoat = (View.ShadingModelMaskInView & (1 << MSM_ClearCoat)) != 0;

			SCOPED_DRAW_EVENTF(RHICmdList, ReflectionEnvironment, TEXT("ReflectionEnvironment ComputeShader %dx%d Tile:%dx%d Box:%d Sphere:%d ClearCoat:%d"), 
				View.ViewRect.Width(), View.ViewRect.Height(), GReflectionEnvironmentTileSizeX, GReflectionEnvironmentTileSizeY,
				NumBoxCaptures, NumSphereCaptures, bNeedsClearCoat);

			ComputeShader = SelectReflectionEnvironmentTiledDeferredCS(View.ShaderMap, bUseLightmaps, bNeedsClearCoat, bHasBoxCaptures, bHasSphereCaptures);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

			FUnorderedAccessViewRHIParamRef OutUAV = NewSceneColor->GetRenderTargetItem().UAV;
			ComputeShader->SetParameters(RHICmdList, View, SSROutput->GetRenderTargetItem().ShaderResourceTexture, SortData, OutUAV, DynamicBentNormalAO);

			uint32 GroupSizeX = (View.ViewRect.Size().X + GReflectionEnvironmentTileSizeX - 1) / GReflectionEnvironmentTileSizeX;
			uint32 GroupSizeY = (View.ViewRect.Size().Y + GReflectionEnvironmentTileSizeY - 1) / GReflectionEnvironmentTileSizeY;
			DispatchComputeShader(RHICmdList, ComputeShader, GroupSizeX, GroupSizeY, 1);

			ComputeShader->UnsetParameters(RHICmdList, OutUAV);
		}
	}

	SceneContext.SetSceneColor(NewSceneColor);
	check(SceneContext.GetSceneColor());
}

void FDeferredShadingSceneRenderer::RenderStandardDeferredImageBasedReflections(FRHICommandListImmediate& RHICmdList, bool bReflectionEnv, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
{
	if(!ViewFamily.EngineShowFlags.Lighting)
	{
		return;
	}

	const bool bSkyLight = Scene->SkyLight
		&& Scene->SkyLight->ProcessedTexture
		&& ViewFamily.EngineShowFlags.SkyLighting;

	static TArray<FReflectionCaptureSortData> SortData;

	if (bReflectionEnv)
	{
		// shared for multiple views

		SortData.Reset(Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num());

		// Gather visible reflection capture data
		for (int32 ReflectionProxyIndex = 0; ReflectionProxyIndex < Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() && SortData.Num() < GMaxNumReflectionCaptures; ReflectionProxyIndex++)
		{
			FReflectionCaptureProxy* CurrentCapture = Scene->ReflectionSceneData.RegisteredReflectionCaptures[ReflectionProxyIndex];
			FReflectionCaptureSortData NewSortEntry;

			NewSortEntry.CaptureIndex = -1;

			if (FeatureLevel >= ERHIFeatureLevel::SM5)
			{
				const FCaptureComponentSceneState* ComponentStatePtr = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Find(CurrentCapture->Component);
				NewSortEntry.CaptureIndex = ComponentStatePtr ? ComponentStatePtr->CaptureIndex : -1;
			}
			
			NewSortEntry.SM4FullHDRCubemap = CurrentCapture->SM4FullHDRCubemap;
			NewSortEntry.Guid = CurrentCapture->Guid;
			NewSortEntry.PositionAndRadius = FVector4(CurrentCapture->Position, CurrentCapture->InfluenceRadius);
			float ShapeTypeValue = (float)CurrentCapture->Shape;
			NewSortEntry.CaptureProperties = FVector4(CurrentCapture->Brightness, 0, ShapeTypeValue, 0);

			if (CurrentCapture->Shape == EReflectionCaptureShape::Plane)
			{
				NewSortEntry.BoxTransform = FMatrix(
					FPlane(CurrentCapture->ReflectionPlane),
					FPlane(CurrentCapture->ReflectionXAxisAndYScale),
					FPlane(0, 0, 0, 0),
					FPlane(0, 0, 0, 0));

				NewSortEntry.BoxScales = FVector4(0);
			}
			else
			{
				NewSortEntry.BoxTransform = CurrentCapture->BoxTransform;

				NewSortEntry.BoxScales = FVector4(CurrentCapture->BoxScales, CurrentCapture->BoxTransitionDistance);
			}

			SortData.Add(NewSortEntry);
		}

		SortData.Sort();
	}
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// Use standard deferred shading to composite reflection capture contribution
	for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		bool bLPV = false;
		
		if(ViewState && ViewState->GetLightPropagationVolume(View.GetFeatureLevel()))
		{
			const FLightPropagationVolumeSettings& LPVSettings = View.FinalPostProcessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();

			bLPV = LPVSettings.LPVIntensity > 0.0f;
		} 
		
		bool bAmbient = View.FinalPostProcessSettings.ContributingCubemaps.Num() > 0;
		bool bMixing = bLPV && (CVarLPVMixing.GetValueOnRenderThread() != 0);
		bool bEnvironmentMixing = bMixing && (bAmbient || bLPV);
		bool bRequiresApply = bSkyLight
			// If Reflection Environment is active and mixed with indirect lighting (Ambient + LPV), apply is required!
			|| (View.Family->EngineShowFlags.ReflectionEnvironment && (bReflectionEnv || bEnvironmentMixing) );

		const bool bSSR = DoScreenSpaceReflections(View);

		TRefCountPtr<IPooledRenderTarget> SSROutput = GSystemTextures.BlackDummy;
		if (bSSR)
		{
			bRequiresApply = true;

			ScreenSpaceReflections(RHICmdList, View, SSROutput);
		}

	    /* Light Accumulation moved to SceneRenderTargets */
	    TRefCountPtr<IPooledRenderTarget> LightAccumulation = SceneContext.LightAccumulation;

		if (!LightAccumulation)
		{
			// should never be used but during debugging it can happen
			LightAccumulation = GSystemTextures.WhiteDummy;
		}

		if (bReflectionEnv)
		{
			bRequiresApply = true;

			SCOPED_DRAW_EVENTF(RHICmdList, ReflectionEnvironment, TEXT("ReflectionEnvironment PixelShader"));

			{
				// Clear to no reflection contribution, alpha of 1 indicates full background contribution
				ESimpleRenderTargetMode SimpleRenderTargetMode = ESimpleRenderTargetMode::EExistingColorAndDepth;

				// If Reflection Environment is mixed with indirect lighting (Ambient + LPV), skip clear!
				if (!bMixing)
				{
					SimpleRenderTargetMode = ESimpleRenderTargetMode::EClearColorExistingDepth;
				}
				SetRenderTarget(RHICmdList, LightAccumulation->GetRenderTargetItem().TargetableTexture, NULL, SimpleRenderTargetMode);
			}


			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			// rgb accumulates reflection contribution front to back, alpha accumulates (1 - alpha0) * (1 - alpha 1)...
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_DestAlpha, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());

			for (int32 ReflectionCaptureIndex = 0; ReflectionCaptureIndex < SortData.Num(); ReflectionCaptureIndex++)
			{
				const FReflectionCaptureSortData& ReflectionCapture = SortData[ReflectionCaptureIndex];

				if (FeatureLevel >= ERHIFeatureLevel::SM5 || ReflectionCapture.SM4FullHDRCubemap)
				{
					const FSphere LightBounds(ReflectionCapture.PositionAndRadius, ReflectionCapture.PositionAndRadius.W);

					TShaderMapRef<TDeferredLightVS<true> > VertexShader(View.ShaderMap);

					// Use the appropriate shader for the capture shape
					if (ReflectionCapture.CaptureProperties.Z == 0)
					{
						TShaderMapRef<TStandardDeferredReflectionPS<true> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						
						SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);

						PixelShader->SetParameters(RHICmdList, View, ReflectionCapture);
					}
					else
					{
						TShaderMapRef<TStandardDeferredReflectionPS<false> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						
						SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);

						PixelShader->SetParameters(RHICmdList, View, ReflectionCapture);
					}

					SetBoundingGeometryRasterizerAndDepthState(RHICmdList, View, LightBounds);
					VertexShader->SetSimpleLightParameters(RHICmdList, View, LightBounds);
					StencilingGeometry::DrawSphere(RHICmdList);
				}
			}
			RHICmdList.CopyToResolveTarget(LightAccumulation->GetRenderTargetItem().TargetableTexture, LightAccumulation->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, LightAccumulation);
		}

		if (bRequiresApply)
		{
			// Apply reflections to screen
			SCOPED_DRAW_EVENT(RHICmdList, ReflectionApply);

			SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EUninitializedColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

			if (GetReflectionEnvironmentCVar() == 2)
			{
				// override scene color for debugging
				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			}
			else
			{
				// additive to scene color
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
			}

			TShaderMapRef< FPostProcessVS >		VertexShader(View.ShaderMap);

			// Activate Reflection Environment if we choose to mix it with indirect lighting (Ambient + LPV)
			// todo: refactor (we abuse another boolean to pass the data through)
			bReflectionEnv = bReflectionEnv || bEnvironmentMixing;

#define CASE(A,B,C) \
			case ((A << 2) | (B << 1) | C) : \
			{ \
			TShaderMapRef< FReflectionApplyPS<A, B, C> > PixelShader(View.ShaderMap); \
			static FGlobalBoundShaderState BoundShaderState; \
			SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader); \
			PixelShader->SetParameters(RHICmdList, View, LightAccumulation->GetRenderTargetItem().ShaderResourceTexture, SSROutput->GetRenderTargetItem().ShaderResourceTexture, DynamicBentNormalAO); \
			}; \
			break

			switch (((uint32)bSSR << 2) | ((uint32)bReflectionEnv << 1) | (uint32)bSkyLight)
			{
				CASE(0, 0, 0);
				CASE(0, 0, 1);
				CASE(0, 1, 0);
				CASE(0, 1, 1);
				CASE(1, 0, 0);
				CASE(1, 0, 1);
				CASE(1, 1, 0);
				CASE(1, 1, 1);
			}
#undef CASE

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
				SceneContext.GetBufferSizeXY(),
				*VertexShader);

			SceneContext.FinishRenderingSceneColor(RHICmdList);
		}
	}
}

void FDeferredShadingSceneRenderer::RenderDeferredReflections(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
{
	if (IsSimpleDynamicLightingEnabled() || ViewFamily.EngineShowFlags.VisualizeLightCulling)
	{
		return;
	}

	bool bAnyViewIsReflectionCapture = false;
	for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		bAnyViewIsReflectionCapture = bAnyViewIsReflectionCapture || View.bIsReflectionCapture;
	}

	// If we're currently capturing a reflection capture, output SpecularColor * IndirectIrradiance for metals so they are not black in reflections,
	// Since we don't have multiple bounce specular reflections
	if (bAnyViewIsReflectionCapture)
	{
		RenderReflectionCaptureSpecularBounceForAllViews(RHICmdList);
	}
	else
	{
		const uint32 bDoTiledReflections = CVarDoTiledReflections.GetValueOnRenderThread() != 0;
		const bool bReflectionEnvironment = ShouldDoReflectionEnvironment();
		const bool bReflectionsWithCompute = bDoTiledReflections && (FeatureLevel >= ERHIFeatureLevel::SM5) && bReflectionEnvironment && Scene->ReflectionSceneData.CubemapArray.IsValid();
		
		if (bReflectionsWithCompute)
		{
			RenderTiledDeferredImageBasedReflections(RHICmdList, DynamicBentNormalAO);
		}
		else
		{
			RenderStandardDeferredImageBasedReflections(RHICmdList, bReflectionEnvironment, DynamicBentNormalAO);
		}
	}
}
