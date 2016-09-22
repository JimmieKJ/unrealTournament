// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

DECLARE_FLOAT_COUNTER_STAT(TEXT("Reflection Environment"), Stat_GPU_ReflectionEnvironment, STATGROUP_GPU);

/** Tile size for the reflection environment compute shader, tweaked for PS4. */
const int32 GReflectionEnvironmentTileSizeX = 8;
const int32 GReflectionEnvironmentTileSizeY = 8;

extern TAutoConsoleVariable<int32> CVarLPVMixing;

static TAutoConsoleVariable<int32> CVarReflectionEnvironment(
	TEXT("r.ReflectionEnvironment"),
	1,
	TEXT("Whether to render the reflection environment feature, which implements local reflections through Reflection Capture actors.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on and blend with scene (default)")
	TEXT(" 2: on and overwrite scene (only in non-shipping builds)"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

int32 GReflectionEnvironmentLightmapMixing = 1;
FAutoConsoleVariableRef CVarReflectionEnvironmentLightmapMixing(
	TEXT("r.ReflectionEnvironmentLightmapMixing"),
	GReflectionEnvironmentLightmapMixing,
	TEXT("Whether to mix indirect specular from reflection captures with indirect diffuse from lightmaps for rough surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GReflectionEnvironmentBeginMixingRoughness = .1f;
FAutoConsoleVariableRef CVarReflectionEnvironmentBeginMixingRoughness(
	TEXT("r.ReflectionEnvironmentBeginMixingRoughness"),
	GReflectionEnvironmentBeginMixingRoughness,
	TEXT("Min roughness value at which to begin mixing reflection captures with lightmap indirect diffuse."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GReflectionEnvironmentEndMixingRoughness = .3f;
FAutoConsoleVariableRef CVarReflectionEnvironmentEndMixingRoughness(
	TEXT("r.ReflectionEnvironmentEndMixingRoughness"),
	GReflectionEnvironmentEndMixingRoughness,
	TEXT("Min roughness value at which to end mixing reflection captures with lightmap indirect diffuse."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

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

FVector2D GetReflectionEnvironmentRoughnessMixingScaleBias()
{
	float RoughnessMixingRange = 1.0f / FMath::Max(GReflectionEnvironmentEndMixingRoughness - GReflectionEnvironmentBeginMixingRoughness, .001f);

	if (GReflectionEnvironmentLightmapMixing == 0)
	{
		return FVector2D(0, 0);
	}

	if (GReflectionEnvironmentEndMixingRoughness == 0.0f && GReflectionEnvironmentBeginMixingRoughness == 0.0f)
	{
		// Make sure a Roughness of 0 results in full mixing when disabling roughness-based mixing
		return FVector2D(0, 1);
	}

	return FVector2D(RoughnessMixingRange, -GReflectionEnvironmentBeginMixingRoughness * RoughnessMixingRange);
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
		const int32 NumReflectionCaptureMips = FMath::CeilLogTwo(CubemapSize) + 1;

		ReleaseCubeArray();

		FPooledRenderTargetDesc Desc(
			FPooledRenderTargetDesc::CreateCubemapDesc(
				CubemapSize,
				// Alpha stores sky mask
				PF_FloatRGBA, 
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_None,
				false, 
				// Cubemap array of 1 produces a regular cubemap, so guarantee it will be allocated as an array
				FMath::Max<uint32>(MaxCubemaps, 2),
				NumReflectionCaptureMips
				)
			);

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

void FReflectionEnvironmentCubemapArray::UpdateMaxCubemaps(uint32 InMaxCubemaps, int32 InCubemapSize)
{
	MaxCubemaps = InMaxCubemaps;
	CubemapSize = InCubemapSize;

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
		OcclusionTintAndMinOcclusion.Bind(ParameterMap, TEXT("OcclusionTintAndMinOcclusion"));
	}

	template<typename ShaderRHIParamRef, typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, float SkySpecularOcclusionStrength, const FVector4& OcclusionTintAndMinOcclusionValue)
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
		SetShaderValue(RHICmdList, ShaderRHI, OcclusionTintAndMinOcclusion, OcclusionTintAndMinOcclusionValue);
	}

	friend FArchive& operator<<(FArchive& Ar,FDistanceFieldAOSpecularOcclusionParameters& P)
	{
		Ar << P.BentNormalAOTexture << P.BentNormalAOSampler << P.ApplyBentNormalAO << P.InvSkySpecularOcclusionStrength << P.OcclusionTintAndMinOcclusion;
		return Ar;
	}

private:
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderParameter ApplyBentNormalAO;
	FShaderParameter InvSkySpecularOcclusionStrength;
	FShaderParameter OcclusionTintAndMinOcclusion;
};

struct FReflectionCaptureSortData
{
	uint32 Guid;
	int32 CaptureIndex;
	FVector4 PositionAndRadius;
	FVector4 CaptureProperties;
	FMatrix BoxTransform;
	FVector4 BoxScales;
	FVector4 CaptureOffsetAndAverageBrightness;
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
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FReflectionEnvironmentTiledDeferredCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ReflectionCubemap.Bind(Initializer.ParameterMap,TEXT("ReflectionCubemap"));
		ReflectionCubemapSampler.Bind(Initializer.ParameterMap,TEXT("ReflectionCubemapSampler"));
		ScreenSpaceReflections.Bind(Initializer.ParameterMap, TEXT("ScreenSpaceReflections"));
		InSceneColor.Bind(Initializer.ParameterMap, TEXT("InSceneColor"));
		OutSceneColor.Bind(Initializer.ParameterMap, TEXT("OutSceneColor"));
		NumCaptures.Bind(Initializer.ParameterMap, TEXT("NumCaptures"));
		ViewDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
		PreIntegratedGF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGF"));
		PreIntegratedGFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGFSampler"));
		SkyLightParameters.Bind(Initializer.ParameterMap);
		SpecularOcclusionParameters.Bind(Initializer.ParameterMap);
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
	}

	FReflectionEnvironmentTiledDeferredCS()
	{
	}

	void SetParameters(
		FRHIAsyncComputeCommandListImmediate& RHICmdList, 
		const FViewInfo& View,
		FTextureRHIParamRef SSRTexture,
		FUnorderedAccessViewRHIParamRef OutSceneColorUAV, 
		const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO
		)
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
			ReflectionCubemap, 
			ReflectionCubemapSampler, 
			TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
			CubemapArray.ShaderResourceTexture);

		SetTextureParameter(RHICmdList, ShaderRHI, ScreenSpaceReflections, SSRTexture );

		SetTextureParameter(RHICmdList, ShaderRHI, InSceneColor, FSceneRenderTargets::Get(RHICmdList).GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture );

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, OutSceneColorUAV);
		OutSceneColor.SetTexture(RHICmdList, ShaderRHI, NULL, OutSceneColorUAV);

		SetShaderValue(RHICmdList, ShaderRHI, ViewDimensionsParameter, View.ViewRect);

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FReflectionCaptureData>(), View.ReflectionCaptureUniformBuffer);
		SetShaderValue(RHICmdList, ShaderRHI, NumCaptures, View.NumBoxReflectionCaptures + View.NumSphereReflectionCaptures);

		SetTextureParameter(RHICmdList, ShaderRHI, PreIntegratedGF, PreIntegratedGFSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture);
	
		SkyLightParameters.SetParameters(RHICmdList, ShaderRHI, Scene, View.Family->EngineShowFlags.SkyLighting);

		const float MinOcclusion = Scene->SkyLight ? Scene->SkyLight->MinOcclusion : 0;
		const FVector OcclusionTint = Scene->SkyLight ? (const FVector&)Scene->SkyLight->OcclusionTint : FVector::ZeroVector;
		SpecularOcclusionParameters.SetParameters(RHICmdList, ShaderRHI, DynamicBentNormalAO, CVarSkySpecularOcclusionStrength.GetValueOnRenderThread(), FVector4(OcclusionTint, MinOcclusion));

		ForwardLightingParameters.Set(RHICmdList, ShaderRHI, View);
	}

	void UnsetParameters(FRHIAsyncComputeCommandListImmediate& RHICmdList, FUnorderedAccessViewRHIParamRef OutSceneColorUAV)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();		
		OutSceneColor.UnsetUAV(RHICmdList, ShaderRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ReflectionCubemap;
		Ar << ReflectionCubemapSampler;
		Ar << ScreenSpaceReflections;
		Ar << InSceneColor;
		Ar << OutSceneColor;
		Ar << NumCaptures;
		Ar << ViewDimensionsParameter;
		Ar << PreIntegratedGF;
		Ar << PreIntegratedGFSampler;
		Ar << SkyLightParameters;
		Ar << SpecularOcclusionParameters;
		Ar << ForwardLightingParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter ReflectionCubemap;
	FShaderResourceParameter ReflectionCubemapSampler;
	FShaderResourceParameter ScreenSpaceReflections;
	FShaderResourceParameter InSceneColor;
	FRWShaderParameter OutSceneColor;
	FShaderParameter NumCaptures;
	FShaderParameter ViewDimensionsParameter;
	FShaderResourceParameter PreIntegratedGF;
	FShaderResourceParameter PreIntegratedGFSampler;
	FSkyLightReflectionParameters SkyLightParameters;
	FDistanceFieldAOSpecularOcclusionParameters SpecularOcclusionParameters;
	FForwardLightingParameters ForwardLightingParameters;
};

template< uint32 bUseLightmaps, uint32 bHasSkyLight, uint32 bBoxCapturesOnly, uint32 bSphereCapturesOnly, uint32 bSupportDFAOIndirectOcclusion >
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
		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), bHasSkyLight);
		OutEnvironment.SetDefine(TEXT("REFLECTION_COMPOSITE_HAS_BOX_CAPTURES"), bBoxCapturesOnly);
		OutEnvironment.SetDefine(TEXT("REFLECTION_COMPOSITE_HAS_SPHERE_CAPTURES"), bSphereCapturesOnly);
		OutEnvironment.SetDefine(TEXT("SUPPORT_DFAO_INDIRECT_OCCLUSION"), bSupportDFAOIndirectOcclusion);
	}
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(A, B, C, D, E) \
	typedef TReflectionEnvironmentTiledDeferredCS<A,B,C,D,E> TReflectionEnvironmentTiledDeferredCS##A##B##C##D##E; \
	IMPLEMENT_SHADER_TYPE(template<>,TReflectionEnvironmentTiledDeferredCS##A##B##C##D##E,TEXT("ReflectionEnvironmentComputeShaders"),TEXT("ReflectionEnvironmentTiledDeferredMain"),SF_Compute)

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 0, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 0, 1, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 1, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 1, 1, 0);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 0, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 0, 1, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 1, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 1, 1, 0);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 0, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 0, 1, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 1, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 1, 1, 0);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 0, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 0, 1, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 1, 0, 0);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 1, 1, 0);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 0, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 0, 1, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 1, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 0, 1, 1, 1);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 0, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 0, 1, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 1, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(0, 1, 1, 1, 1);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 0, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 0, 1, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 1, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 0, 1, 1, 1);

IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 0, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 0, 1, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 1, 0, 1);
IMPLEMENT_REFLECTION_COMPUTESHADER_TYPE(1, 1, 1, 1, 1);

template< uint32 bSSR, uint32 bReflectionEnv, uint32 bSkylight, uint32 bSupportDFAOIndirectOcclusion >
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
		OutEnvironment.SetDefine(TEXT("SUPPORT_DFAO_INDIRECT_OCCLUSION"), bSupportDFAOIndirectOcclusion);
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
		const FVector OcclusionTint = Scene->SkyLight ? (const FVector&)Scene->SkyLight->OcclusionTint : FVector::ZeroVector;
		SpecularOcclusionParameters.SetParameters(RHICmdList, ShaderRHI, DynamicBentNormalAO, CVarSkySpecularOcclusionStrength.GetValueOnRenderThread(), FVector4(OcclusionTint, MinOcclusion));
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
#define IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(A, B, C, D) \
	typedef FReflectionApplyPS<A,B,C,D> FReflectionApplyPS##A##B##C##D; \
	IMPLEMENT_SHADER_TYPE(template<>,FReflectionApplyPS##A##B##C##D,TEXT("ReflectionEnvironmentShaders"),TEXT("ReflectionApplyPS"),SF_Pixel);

IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,0,0,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,0,1,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,1,0,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,1,1,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,0,0,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,0,1,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,1,0,0);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,1,1,0);

IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,0,0,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,0,1,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,1,0,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(0,1,1,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,0,0,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,0,1,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,1,0,1);
IMPLEMENT_REFLECTION_APPLY_PIXELSHADER_TYPE(1,1,1,1);

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
		CaptureOffsetAndAverageBrightness.Bind(Initializer.ParameterMap, TEXT("CaptureOffsetAndAverageBrightness"));
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
		SetShaderValue(RHICmdList, ShaderRHI, CaptureOffsetAndAverageBrightness, SortData.CaptureOffsetAndAverageBrightness);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CapturePositionAndRadius;
		Ar << CaptureProperties;
		Ar << CaptureBoxTransform;
		Ar << CaptureBoxScales;
		Ar << CaptureOffsetAndAverageBrightness;
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
	FShaderParameter CaptureOffsetAndAverageBrightness;
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
	const ERHIFeatureLevel::Type SceneFeatureLevel = Scene->GetFeatureLevel();

	return IsReflectionEnvironmentAvailable(SceneFeatureLevel)
		&& Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num()
		&& ViewFamily.EngineShowFlags.ReflectionEnvironment
		&& (SceneFeatureLevel == ERHIFeatureLevel::SM4 || Scene->ReflectionSceneData.CubemapArray.IsValid());
}

void GatherAndSortReflectionCaptures(const FViewInfo& View, const FScene* Scene, TArray<FReflectionCaptureSortData>& OutSortData, int32& OutNumBoxCaptures, int32& OutNumSphereCaptures, float& OutFurthestReflectionCaptureDistance)
{	
	OutSortData.Reset(Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num());
	OutNumBoxCaptures = 0;
	OutNumSphereCaptures = 0;
	OutFurthestReflectionCaptureDistance = 1000;

	const int32 MaxCubemaps = Scene->ReflectionSceneData.CubemapArray.GetMaxCubemaps();

	// Pack only visible reflection captures into the uniform buffer, each with an index to its cubemap array entry
	//@todo - view frustum culling
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
			NewSortEntry.CaptureOffsetAndAverageBrightness = FVector4(CurrentCapture->CaptureOffset, CurrentCapture->AverageBrightness);

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

			const FSphere BoundingSphere(CurrentCapture->Position, CurrentCapture->InfluenceRadius);
			const float Distance = View.ViewMatrices.ViewMatrix.TransformPosition(BoundingSphere.Center).Z + BoundingSphere.W;
			OutFurthestReflectionCaptureDistance = FMath::Max(OutFurthestReflectionCaptureDistance, Distance);

			OutSortData.Add(NewSortEntry);
		}
	}

	OutSortData.Sort();	
}

void FDeferredShadingSceneRenderer::SetupReflectionCaptureBuffers(FViewInfo& View, FRHICommandListImmediate& RHICmdList)
{
	if (View.GetFeatureLevel() >= ERHIFeatureLevel::SM5)
	{
		TArray<FReflectionCaptureSortData> SortData;
		GatherAndSortReflectionCaptures(View, Scene, SortData, View.NumBoxReflectionCaptures, View.NumSphereReflectionCaptures, View.FurthestReflectionCaptureDistance);

		FReflectionCaptureData SamplePositionsBuffer;

		for (int32 CaptureIndex = 0; CaptureIndex < SortData.Num(); CaptureIndex++)
		{
			SamplePositionsBuffer.PositionAndRadius[CaptureIndex] = SortData[CaptureIndex].PositionAndRadius;
			SamplePositionsBuffer.CaptureProperties[CaptureIndex] = SortData[CaptureIndex].CaptureProperties;
			SamplePositionsBuffer.CaptureOffsetAndAverageBrightness[CaptureIndex] = SortData[CaptureIndex].CaptureOffsetAndAverageBrightness;
			SamplePositionsBuffer.BoxTransform[CaptureIndex] = SortData[CaptureIndex].BoxTransform;
			SamplePositionsBuffer.BoxScales[CaptureIndex] = SortData[CaptureIndex].BoxScales;
		}

		View.ReflectionCaptureUniformBuffer = TUniformBufferRef<FReflectionCaptureData>::CreateUniformBufferImmediate(SamplePositionsBuffer, UniformBuffer_SingleFrame);
	}
}

template<bool bSuportDFAOIndirectOcclusion>
FReflectionEnvironmentTiledDeferredCS* SelectReflectionEnvironmentTiledDeferredCSInner(TShaderMap<FGlobalShaderType>* ShaderMap, bool bUseLightmaps, bool bHasSkyLight, bool bHasBoxCaptures, bool bHasSphereCaptures)
{
	FReflectionEnvironmentTiledDeferredCS* ComputeShader = nullptr;
	if (bUseLightmaps)
	{
		if (bHasSkyLight)
		{
			if (bHasBoxCaptures && bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 1, 1, 1, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else if (bHasBoxCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 1, 1, 0, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else if (bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 1, 0, 1, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 1, 0, 0, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
		}
		else
		{
			if (bHasBoxCaptures && bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 0, 1, 1, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else if (bHasBoxCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 0, 1, 0, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else if (bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 0, 0, 1, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<1, 0, 0, 0, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
		}		
	}
	else
	{
		if (bHasSkyLight)
		{
			if (bHasBoxCaptures && bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 1, 1, 1, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else if (bHasBoxCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 1, 1, 0, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else if (bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 1, 0, 1, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 1, 0, 0, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
		}
		else
		{
			if (bHasBoxCaptures && bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 0, 1, 1, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else if (bHasBoxCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 0, 1, 0, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else if (bHasSphereCaptures)
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 0, 0, 1, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
			else
			{
				ComputeShader = *TShaderMapRef< TReflectionEnvironmentTiledDeferredCS<0, 0, 0, 0, bSuportDFAOIndirectOcclusion> >(ShaderMap);
			}
		}		
	}
	check(ComputeShader);
	return ComputeShader;
}

FReflectionEnvironmentTiledDeferredCS* SelectReflectionEnvironmentTiledDeferredCS(TShaderMap<FGlobalShaderType>* ShaderMap, bool bUseLightmaps, bool bHasSkyLight, bool bHasBoxCaptures, bool bHasSphereCaptures, bool bSuportDFAOIndirectOcclusion)
{
	if (bSuportDFAOIndirectOcclusion)
	{
		return SelectReflectionEnvironmentTiledDeferredCSInner<true>(ShaderMap, bUseLightmaps, bHasSkyLight, bHasBoxCaptures, bHasSphereCaptures);
	}
	else
	{
		return SelectReflectionEnvironmentTiledDeferredCSInner<false>(ShaderMap, bUseLightmaps, bHasSkyLight, bHasBoxCaptures, bHasSphereCaptures);
	}
}

void FDeferredShadingSceneRenderer::RenderTiledDeferredImageBasedReflections(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bUseLightmaps = (AllowStaticLightingVar->GetValueOnRenderThread() == 1);

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

		const uint32 bSSR = ShouldRenderScreenSpaceReflections(Views[ViewIndex]);

		TRefCountPtr<IPooledRenderTarget> SSROutput = GSystemTextures.BlackDummy;
		if( bSSR )
		{
			RenderScreenSpaceReflections(RHICmdList, View, SSROutput, VelocityRT);
		}

		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ReflectionEnvironment)
		RenderDeferredPlanarReflections(RHICmdList, false, SSROutput);

		// ReflectionEnv is assumed to be on when going into this method
		{
			SetRenderTarget(RHICmdList, NULL, NULL);

			FReflectionEnvironmentTiledDeferredCS* ComputeShader = NULL;
			// Render the reflection environment with tiled deferred culling
			bool bHasBoxCaptures = (View.NumBoxReflectionCaptures > 0);
			bool bHasSphereCaptures = (View.NumSphereReflectionCaptures > 0);
			bool bHasSkyLight = Scene && Scene->SkyLight && !Scene->SkyLight->bHasStaticLighting;

			static const FName TiledReflBeginComputeName(TEXT("ReflectionEnvBeginComputeFence"));
			static const FName TiledReflEndComputeName(TEXT("ReflectionEnvEndComputeFence"));
			FComputeFenceRHIRef ReflectionBeginFence = RHICmdList.CreateComputeFence(TiledReflBeginComputeName);
			FComputeFenceRHIRef ReflectionEndFence = RHICmdList.CreateComputeFence(TiledReflEndComputeName);

			//Grab the async compute commandlist.
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
				SCOPED_COMPUTE_EVENTF(RHICmdListComputeImmediate, ReflectionEnvironment, TEXT("ReflectionEnvironment ComputeShader %dx%d Tile:%dx%d Box:%d Sphere:%d SkyLight:%d"),
					View.ViewRect.Width(), View.ViewRect.Height(), GReflectionEnvironmentTileSizeX, GReflectionEnvironmentTileSizeY,
					View.NumBoxReflectionCaptures, View.NumSphereReflectionCaptures, bHasSkyLight);

				ComputeShader = SelectReflectionEnvironmentTiledDeferredCS(View.ShaderMap, bUseLightmaps, bHasSkyLight, bHasBoxCaptures, bHasSphereCaptures, DynamicBentNormalAO != NULL);

				//Really we should write this fence where we transition the final depedency for the reflections.  We may add an RHI command just for writing fences if this
				//can't be done in the general case.  In the meantime, hack this command a bit to write the fence.
				RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, nullptr, 0, ReflectionBeginFence);
					
				//we must wait on the fence written from the Gfx pipe to let us know all our dependencies are ready.
				RHICmdListComputeImmediate.WaitComputeFence(ReflectionBeginFence);

				//standard compute setup, but on the async commandlist.
				RHICmdListComputeImmediate.SetComputeShader(ComputeShader->GetComputeShader());

				FUnorderedAccessViewRHIParamRef OutUAV = NewSceneColor->GetRenderTargetItem().UAV;
				ComputeShader->SetParameters(RHICmdListComputeImmediate, View, SSROutput->GetRenderTargetItem().ShaderResourceTexture, OutUAV, DynamicBentNormalAO);
			
				uint32 GroupSizeX = (View.ViewRect.Size().X + GReflectionEnvironmentTileSizeX - 1) / GReflectionEnvironmentTileSizeX;
				uint32 GroupSizeY = (View.ViewRect.Size().Y + GReflectionEnvironmentTileSizeY - 1) / GReflectionEnvironmentTileSizeY;
				DispatchComputeShader(RHICmdListComputeImmediate, ComputeShader, GroupSizeX, GroupSizeY, 1);

				ComputeShader->UnsetParameters(RHICmdListComputeImmediate, OutUAV);
			
				//transition the output to readable and write the fence to allow the Gfx pipe to carry on.
				RHICmdListComputeImmediate.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, &OutUAV, 1, ReflectionEndFence);
			}

			//immediately dispatch our async compute commands to the RHI thread to be submitted to the GPU as soon as possible.
			//dispatch after the scope so the drawevent pop is inside the dispatch
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);			
			
			//Gfx pipe must wait for the async compute reflection job to complete.
			RHICmdList.WaitComputeFence(ReflectionEndFence);
		}
	}

	SceneContext.SetSceneColor(NewSceneColor);
	check(SceneContext.GetSceneColor());
}

void FDeferredShadingSceneRenderer::RenderStandardDeferredImageBasedReflections(FRHICommandListImmediate& RHICmdList, bool bReflectionEnv, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	if(!ViewFamily.EngineShowFlags.Lighting)
	{
		return;
	}

	const bool bSkyLight = Scene->SkyLight
		&& Scene->SkyLight->ProcessedTexture
		&& !Scene->SkyLight->bHasStaticLighting
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
			NewSortEntry.CaptureOffsetAndAverageBrightness = FVector4(CurrentCapture->CaptureOffset, CurrentCapture->AverageBrightness);

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

		const bool bSSR = ShouldRenderScreenSpaceReflections(View);

		TRefCountPtr<IPooledRenderTarget> SSROutput = GSystemTextures.BlackDummy;
		if (bSSR)
		{
			bRequiresApply = true;

			RenderScreenSpaceReflections(RHICmdList, View, SSROutput, VelocityRT);
		}

		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ReflectionEnvironment)
		bool bApplyFromSSRTexture = bSSR;

		if (RenderDeferredPlanarReflections(RHICmdList, true, SSROutput))
		{
			bRequiresApply = true;
			bApplyFromSSRTexture = true;
		}

	    /* Light Accumulation moved to SceneRenderTargets */
	    TRefCountPtr<IPooledRenderTarget> LightAccumulation = SceneContext.LightAccumulation;

		if (!LightAccumulation)
		{
			// should never be used but during debugging it can happen
			ensureMsgf(LightAccumulation, TEXT("White dummy system texture about to be corrupted."));
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
			SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ReflectionEnvironment);

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

			const bool bSupportDFAOIndirectShadowing = DynamicBentNormalAO != NULL;

#define CASE(A,B,C,D) \
			case ((A << 3) | (B << 2) | (C << 1) | D) : \
			{ \
			TShaderMapRef< FReflectionApplyPS<A, B, C, D> > PixelShader(View.ShaderMap); \
			static FGlobalBoundShaderState BoundShaderState; \
			SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader); \
			PixelShader->SetParameters(RHICmdList, View, LightAccumulation->GetRenderTargetItem().ShaderResourceTexture, SSROutput->GetRenderTargetItem().ShaderResourceTexture, DynamicBentNormalAO); \
			}; \
			break

			switch (((uint32)bApplyFromSSRTexture << 3) | ((uint32)bReflectionEnv << 2) | ((uint32)bSkyLight << 1) | (uint32)bSupportDFAOIndirectShadowing)
			{
				CASE(0, 0, 0, 0);
				CASE(0, 0, 1, 0);
				CASE(0, 1, 0, 0);
				CASE(0, 1, 1, 0);
				CASE(1, 0, 0, 0);
				CASE(1, 0, 1, 0);
				CASE(1, 1, 0, 0);
				CASE(1, 1, 1, 0);
				CASE(0, 0, 0, 1);
				CASE(0, 0, 1, 1);
				CASE(0, 1, 0, 1);
				CASE(0, 1, 1, 1);
				CASE(1, 0, 0, 1);
				CASE(1, 0, 1, 1);
				CASE(1, 1, 0, 1);
				CASE(1, 1, 1, 1);
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

void FDeferredShadingSceneRenderer::RenderDeferredReflections(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	if (ViewFamily.EngineShowFlags.VisualizeLightCulling)
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
			RenderTiledDeferredImageBasedReflections(RHICmdList, DynamicBentNormalAO, VelocityRT);
		}
		else
		{
			RenderStandardDeferredImageBasedReflections(RHICmdList, bReflectionEnvironment, DynamicBentNormalAO, VelocityRT);
		}
	}
}
