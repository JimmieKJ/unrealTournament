// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldLightingPost.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "DistanceFieldGlobalIllumination.h"
#include "PostProcessAmbientOcclusion.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "OneColorShader.h"
#include "BasePassRendering.h"
#include "HeightfieldLighting.h"

int32 GAOFillGaps = 1;
FAutoConsoleVariableRef CVarAOFillGaps(
	TEXT("r.AOFillGaps"),
	GAOFillGaps,
	TEXT("Whether to fill in pixels using a screen space filter that had no valid world space interpolation weight from surface cache samples.\n")
	TEXT("This is needed whenever r.AOMinLevel is not 0."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOFillGapsHighQuality = 1;
FAutoConsoleVariableRef CVarAOFillGapsHighQuality(
	TEXT("r.AOFillGapsHighQuality"),
	GAOFillGapsHighQuality,
	TEXT("Whether to use the higher quality gap filling method that does a 5x5 gather, or the cheaper method that just looks up the 4 grid samples."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOUseHistory = 1;
FAutoConsoleVariableRef CVarAOUseHistory(
	TEXT("r.AOUseHistory"),
	GAOUseHistory,
	TEXT("Whether to apply a temporal filter to the distance field AO, which reduces flickering but also adds trails when occluders are moving."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOHistoryStabilityPass = 1;
FAutoConsoleVariableRef CVarAOHistoryStabilityPass(
	TEXT("r.AOHistoryStabilityPass"),
	GAOHistoryStabilityPass,
	TEXT("Whether to gather stable results to fill in holes in the temporal reprojection.  Adds some GPU cost but improves temporal stability with foliage."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOHistoryWeight = .85f;
FAutoConsoleVariableRef CVarAOHistoryWeight(
	TEXT("r.AOHistoryWeight"),
	GAOHistoryWeight,
	TEXT("Amount of last frame's AO to lerp into the final result.  Higher values increase stability, lower values have less streaking under occluder movement."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOHistoryDistanceThreshold = 30;
FAutoConsoleVariableRef CVarAOHistoryDistanceThreshold(
	TEXT("r.AOHistoryDistanceThreshold"),
	GAOHistoryDistanceThreshold,
	TEXT("World space distance threshold needed to discard last frame's DFAO results.  Lower values reduce ghosting from characters when near a wall but increase flickering artifacts."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOViewFadeDistanceScale = .7f;
FAutoConsoleVariableRef CVarAOViewFadeDistanceScale(
	TEXT("r.AOViewFadeDistanceScale"),
	GAOViewFadeDistanceScale,
	TEXT("Distance over which AO will fade out as it approaches r.AOMaxViewDistance, as a fraction of r.AOMaxViewDistance."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

template<bool bSupportIrradiance>
class TDistanceFieldAOCombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldAOCombinePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TDistanceFieldAOCombinePS() {}

	/** Initialization constructor. */
	TDistanceFieldAOCombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		AOLevelParameters.Bind(Initializer.ParameterMap);
		BentNormalAOTexture.Bind(Initializer.ParameterMap,TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap,TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap,TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap,TEXT("IrradianceSampler"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
		DistanceFadeScale.Bind(Initializer.ParameterMap, TEXT("DistanceFadeScale"));
		SelfOcclusionReplacement.Bind(Initializer.ParameterMap, TEXT("SelfOcclusionReplacement"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& InBentNormalTexture, 
		IPooledRenderTarget* InIrradianceTexture, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		AOLevelParameters.Set(RHICmdList, ShaderRHI, View, GAODownsampleFactor);

		SetTextureParameter(RHICmdList, ShaderRHI, BentNormalAOTexture, BentNormalAOSampler, TStaticSamplerState<SF_Point>::GetRHI(), InBentNormalTexture.ShaderResourceTexture);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, IrradianceTexture, IrradianceSampler, TStaticSamplerState<SF_Point>::GetRHI(), InIrradianceTexture->GetRenderTargetItem().ShaderResourceTexture);
		}

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);

		const float DistanceFadeScaleValue = 1.0f / ((1.0f - GAOViewFadeDistanceScale) * GetMaxAOViewDistance());
		SetShaderValue(RHICmdList, ShaderRHI, DistanceFadeScale, DistanceFadeScaleValue);

		extern float GVPLSelfOcclusionReplacement;
		SetShaderValue(RHICmdList, ShaderRHI, SelfOcclusionReplacement, GVPLSelfOcclusionReplacement);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << AOParameters;
		Ar << AOLevelParameters;
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		Ar << DistanceFadeScale;
		Ar << SelfOcclusionReplacement;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
	FAOLevelParameters AOLevelParameters;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
	FShaderParameter DistanceFadeScale;
	FShaderParameter SelfOcclusionReplacement;
};

IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldAOCombinePS<true>,TEXT("DistanceFieldLightingPost"),TEXT("AOCombinePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldAOCombinePS<false>,TEXT("DistanceFieldLightingPost"),TEXT("AOCombinePS"),SF_Pixel);

template<bool bSupportIrradiance, bool bHighQuality>
class TFillGapsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFillGapsPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("HIGH_QUALITY_FILL_GAPS"), bHighQuality ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TFillGapsPS() {}

	/** Initialization constructor. */
	TFillGapsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		BentNormalAOTexelSize.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexelSize"));
		MinDownsampleFactorToBaseLevel.Bind(Initializer.ParameterMap, TEXT("MinDownsampleFactorToBaseLevel"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FSceneRenderTargetItem& DistanceFieldAOBentNormal2, IPooledRenderTarget* DistanceFieldIrradiance2)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldAOBentNormal2.ShaderResourceTexture
			);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceTexture,
				IrradianceSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				DistanceFieldIrradiance2->GetRenderTargetItem().ShaderResourceTexture
				);
		}
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		const FIntPoint DownsampledBufferSize(SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor));
		const FVector2D BaseLevelTexelSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalAOTexelSize, BaseLevelTexelSizeValue);

		extern int32 GAOMinLevel;
		extern int32 GAOPowerOfTwoBetweenLevels;
		const float MinDownsampleFactor = 1 << (GAOMinLevel * GAOPowerOfTwoBetweenLevels); 
		SetShaderValue(RHICmdList, ShaderRHI, MinDownsampleFactorToBaseLevel, MinDownsampleFactor);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << BentNormalAOTexelSize;
		Ar << MinDownsampleFactorToBaseLevel;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderParameter BentNormalAOTexelSize;
	FShaderParameter MinDownsampleFactorToBaseLevel;
};

#define IMPLEMENT_FILLGAPS_PS_TYPE(bSupportIrradiance,bHighQuality) \
	typedef TFillGapsPS<bSupportIrradiance, bHighQuality> TFillGapsPS##bSupportIrradiance##bHighQuality; \
	IMPLEMENT_SHADER_TYPE(template<>,TFillGapsPS##bSupportIrradiance##bHighQuality,TEXT("DistanceFieldLightingPost"),TEXT("FillGapsPS"),SF_Pixel); 

IMPLEMENT_FILLGAPS_PS_TYPE(true, true);
IMPLEMENT_FILLGAPS_PS_TYPE(true, false);
IMPLEMENT_FILLGAPS_PS_TYPE(false, true);
IMPLEMENT_FILLGAPS_PS_TYPE(false, false);

template<bool bSupportIrradiance>
class TUpdateHistoryPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TUpdateHistoryPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TUpdateHistoryPS() {}

	/** Initialization constructor. */
	TUpdateHistoryPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		BentNormalHistoryTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalHistoryTexture"));
		BentNormalHistorySampler.Bind(Initializer.ParameterMap, TEXT("BentNormalHistorySampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		IrradianceHistoryTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceHistoryTexture"));
		IrradianceHistorySampler.Bind(Initializer.ParameterMap, TEXT("IrradianceHistorySampler"));
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		HistoryDistanceThreshold.Bind(Initializer.ParameterMap, TEXT("HistoryDistanceThreshold"));
		UseHistoryFilter.Bind(Initializer.ParameterMap, TEXT("UseHistoryFilter"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		VelocityTextureSampler.Bind(Initializer.ParameterMap, TEXT("VelocityTextureSampler"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& BentNormalHistoryTextureValue, 
		TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryRT, 
		FSceneRenderTargetItem& DistanceFieldAOBentNormal, 
		IPooledRenderTarget* DistanceFieldIrradiance,
		IPooledRenderTarget* VelocityTextureValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldAOBentNormal.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalHistoryTexture,
			BentNormalHistorySampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			BentNormalHistoryTextureValue.ShaderResourceTexture
			);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceTexture,
				IrradianceSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				DistanceFieldIrradiance->GetRenderTargetItem().ShaderResourceTexture
				);
		}
		
		if (IrradianceHistoryTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceHistoryTexture,
				IrradianceHistorySampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				(*IrradianceHistoryRT)->GetRenderTargetItem().ShaderResourceTexture
				);
		}

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FCameraMotionParameters>(), CreateCameraMotionParametersUniformBuffer(View));

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GAOHistoryWeight);
		SetShaderValue(RHICmdList, ShaderRHI, HistoryDistanceThreshold, GAOHistoryDistanceThreshold);
		SetShaderValue(RHICmdList, ShaderRHI, UseHistoryFilter, (GAOHistoryStabilityPass ? 1.0f : 0.0f));

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			VelocityTexture,
			VelocityTextureSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			VelocityTextureValue ? VelocityTextureValue->GetRenderTargetItem().ShaderResourceTexture : GBlackTexture->TextureRHI
			);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << BentNormalHistoryTexture;
		Ar << BentNormalHistorySampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << IrradianceHistoryTexture;
		Ar << IrradianceHistorySampler;
		Ar << HistoryWeight;
		Ar << HistoryDistanceThreshold;
		Ar << UseHistoryFilter;
		Ar << VelocityTexture;
		Ar << VelocityTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter BentNormalHistoryTexture;
	FShaderResourceParameter BentNormalHistorySampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderResourceParameter IrradianceHistoryTexture;
	FShaderResourceParameter IrradianceHistorySampler;
	FShaderParameter HistoryWeight;
	FShaderParameter HistoryDistanceThreshold;
	FShaderParameter UseHistoryFilter;
	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter VelocityTextureSampler;
};

IMPLEMENT_SHADER_TYPE(template<>,TUpdateHistoryPS<true>,TEXT("DistanceFieldLightingPost"),TEXT("UpdateHistoryPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TUpdateHistoryPS<false>,TEXT("DistanceFieldLightingPost"),TEXT("UpdateHistoryPS"),SF_Pixel);


template<bool bSupportIrradiance>
class TFilterHistoryPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterHistoryPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TFilterHistoryPS() {}

	/** Initialization constructor. */
	TFilterHistoryPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		BentNormalAOTexelSize.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexelSize"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& BentNormalHistoryTextureValue, 
		IPooledRenderTarget* IrradianceHistoryRT)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			BentNormalHistoryTextureValue.ShaderResourceTexture
			);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceTexture,
				IrradianceSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				IrradianceHistoryRT->GetRenderTargetItem().ShaderResourceTexture
				);
		}

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GAOHistoryWeight);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		
		const FIntPoint DownsampledBufferSize(SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor));
		const FVector2D BaseLevelTexelSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalAOTexelSize, BaseLevelTexelSizeValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << HistoryWeight;
		Ar << BentNormalAOTexelSize;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderParameter HistoryWeight;
	FShaderParameter BentNormalAOTexelSize;
};

IMPLEMENT_SHADER_TYPE(template<>,TFilterHistoryPS<true>,TEXT("DistanceFieldLightingPost"),TEXT("FilterHistoryPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TFilterHistoryPS<false>,TEXT("DistanceFieldLightingPost"),TEXT("FilterHistoryPS"),SF_Pixel);


void AllocateOrReuseAORenderTarget(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& Target, const TCHAR* Name, bool bWithAlpha)
{
	if (!Target)
	{
		FIntPoint BufferSize = GetBufferSizeForAO();

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, bWithAlpha ? PF_FloatRGBA : PF_FloatRGB, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
		Desc.AutoWritable = false;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Target, Name);
	}
}

void UpdateHistory(
	FRHICommandList& RHICmdList,
	const FViewInfo& View, 
	const TCHAR* BentNormalHistoryRTName,
	const TCHAR* IrradianceHistoryRTName,
	IPooledRenderTarget* VelocityTexture,
	/** Contains last frame's history, if non-NULL.  This will be updated with the new frame's history. */
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState,
	TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryState,
	/** Source */
	TRefCountPtr<IPooledRenderTarget>& BentNormalSource, 
	TRefCountPtr<IPooledRenderTarget>& IrradianceSource, 
	/** Output of Temporal Reprojection for the next step in the pipeline. */
	TRefCountPtr<IPooledRenderTarget>& BentNormalHistoryOutput,
	TRefCountPtr<IPooledRenderTarget>& IrradianceHistoryOutput)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	if (BentNormalHistoryState)
	{
		const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

		if (*BentNormalHistoryState 
			&& !View.bCameraCut 
			&& !View.bPrevTransformsReset 
			&& (!bUseDistanceFieldGI || (IrradianceHistoryState && *IrradianceHistoryState)))
		{
			// Reuse a render target from the pool with a consistent name, for vis purposes
			TRefCountPtr<IPooledRenderTarget> NewBentNormalHistory;
			AllocateOrReuseAORenderTarget(RHICmdList, NewBentNormalHistory, BentNormalHistoryRTName, true);

			TRefCountPtr<IPooledRenderTarget> NewIrradianceHistory;

			if (bUseDistanceFieldGI)
			{
				AllocateOrReuseAORenderTarget(RHICmdList, NewIrradianceHistory, IrradianceHistoryRTName, false);
			}

			SCOPED_DRAW_EVENT(RHICmdList, UpdateHistory);

			{
				FTextureRHIParamRef RenderTargets[2] =
				{
					NewBentNormalHistory->GetRenderTargetItem().TargetableTexture,
					bUseDistanceFieldGI ? NewIrradianceHistory->GetRenderTargetItem().TargetableTexture : NULL,
				};

				SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);

				RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
				RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

				TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TUpdateHistoryPS<true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, (*BentNormalHistoryState)->GetRenderTargetItem(), IrradianceHistoryState, BentNormalSource->GetRenderTargetItem(), IrradianceSource, VelocityTexture);
				}
				else
				{
					TShaderMapRef<TUpdateHistoryPS<false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, (*BentNormalHistoryState)->GetRenderTargetItem(), IrradianceHistoryState, BentNormalSource->GetRenderTargetItem(), IrradianceSource, VelocityTexture);
				}

				VertexShader->SetParameters(RHICmdList, View);

				DrawRectangle( 
					RHICmdList,
					0, 0, 
					View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
					View.ViewRect.Min.X / GAODownsampleFactor, View.ViewRect.Min.Y / GAODownsampleFactor,
					View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
					FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
					SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
					*VertexShader);

				RHICmdList.CopyToResolveTarget(NewBentNormalHistory->GetRenderTargetItem().TargetableTexture, NewBentNormalHistory->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
				if (bUseDistanceFieldGI)
				{
					RHICmdList.CopyToResolveTarget(NewIrradianceHistory->GetRenderTargetItem().TargetableTexture, NewIrradianceHistory->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
				}
			}

			if (GAOHistoryStabilityPass)
			{
				const FPooledRenderTargetDesc& HistoryDesc = (*BentNormalHistoryState)->GetDesc();

				// Reallocate history if buffer sizes have changed
				if (HistoryDesc.Extent != SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor))
				{
					GRenderTargetPool.FreeUnusedResource(*BentNormalHistoryState);
					*BentNormalHistoryState = NULL;
					// Update the view state's render target reference with the new history
					AllocateOrReuseAORenderTarget(RHICmdList, *BentNormalHistoryState, BentNormalHistoryRTName, true);

					if (bUseDistanceFieldGI)
					{
						GRenderTargetPool.FreeUnusedResource(*IrradianceHistoryState);
						*IrradianceHistoryState = NULL;
						AllocateOrReuseAORenderTarget(RHICmdList, *IrradianceHistoryState, IrradianceHistoryRTName, false);
					}
				}

				{
					FTextureRHIParamRef RenderTargets[2] =
					{
						(*BentNormalHistoryState)->GetRenderTargetItem().TargetableTexture,
						bUseDistanceFieldGI ? (*IrradianceHistoryState)->GetRenderTargetItem().TargetableTexture : NULL,
					};

					SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL, true);

					RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
					RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
					RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

					TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

					if (bUseDistanceFieldGI)
					{
						TShaderMapRef<TFilterHistoryPS<true> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
						PixelShader->SetParameters(RHICmdList, View, NewBentNormalHistory->GetRenderTargetItem(), NewIrradianceHistory);
					}
					else
					{
						TShaderMapRef<TFilterHistoryPS<false> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
						PixelShader->SetParameters(RHICmdList, View, NewBentNormalHistory->GetRenderTargetItem(), NewIrradianceHistory);
					}

					VertexShader->SetParameters(RHICmdList, View);

					DrawRectangle( 
						RHICmdList,
						0, 0, 
						View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
						0, 0,
						View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
						FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
						SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
						*VertexShader);

					RHICmdList.CopyToResolveTarget((*BentNormalHistoryState)->GetRenderTargetItem().TargetableTexture, (*BentNormalHistoryState)->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
					if (bUseDistanceFieldGI)
					{
						RHICmdList.CopyToResolveTarget((*IrradianceHistoryState)->GetRenderTargetItem().TargetableTexture, (*IrradianceHistoryState)->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
					}
				}

				BentNormalHistoryOutput = *BentNormalHistoryState;
				IrradianceHistoryOutput = *IrradianceHistoryState;
			}
			else
			{
				// Update the view state's render target reference with the new history
				*BentNormalHistoryState = NewBentNormalHistory;
				BentNormalHistoryOutput = NewBentNormalHistory;

				*IrradianceHistoryState = NewIrradianceHistory;
				IrradianceHistoryOutput = NewIrradianceHistory;
			}
		}
		else
		{
			// Use the current frame's mask for next frame's history
			*BentNormalHistoryState = BentNormalSource;
			BentNormalHistoryOutput = BentNormalSource;
			BentNormalSource = NULL;

			*IrradianceHistoryState = IrradianceSource;
			IrradianceHistoryOutput = IrradianceSource;
			IrradianceSource = NULL;
		}
	}
	else
	{
		// Temporal reprojection is disabled or there is no view state - pass through
		BentNormalHistoryOutput = BentNormalSource;
		IrradianceHistoryOutput = IrradianceSource;
	}
}

void PostProcessBentNormalAOSurfaceCache(
	FRHICommandList& RHICmdList, 
	const FDistanceFieldAOParameters& Parameters, 
	const FViewInfo& View, 
	IPooledRenderTarget* VelocityTexture,
	FSceneRenderTargetItem& BentNormalInterpolation, 
	IPooledRenderTarget* IrradianceInterpolation,
	FSceneRenderTargetItem& DistanceFieldNormal,
	TRefCountPtr<IPooledRenderTarget>& BentNormalOutput,
	TRefCountPtr<IPooledRenderTarget>& IrradianceOutput)
{
	const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

	TRefCountPtr<IPooledRenderTarget> DistanceFieldAOBentNormal;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldIrradiance;
	AllocateOrReuseAORenderTarget(RHICmdList, DistanceFieldAOBentNormal, TEXT("DistanceFieldBentNormalAO"), true);

	if (bUseDistanceFieldGI)
	{
		AllocateOrReuseAORenderTarget(RHICmdList, DistanceFieldIrradiance, TEXT("DistanceFieldIrradiance"), false);
	}

	TRefCountPtr<IPooledRenderTarget> DistanceFieldAOBentNormal2;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldIrradiance2;

	if (GAOFillGaps)
	{
		AllocateOrReuseAORenderTarget(RHICmdList, DistanceFieldAOBentNormal2, TEXT("DistanceFieldBentNormalAO2"), true);

		if (bUseDistanceFieldGI)
		{
			AllocateOrReuseAORenderTarget(RHICmdList, DistanceFieldIrradiance2, TEXT("DistanceFieldIrradiance2"), false);
		}
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, AOCombine);

		TRefCountPtr<IPooledRenderTarget> EffectiveDiffuseIrradianceTarget;

		if (bUseDistanceFieldGI)
		{
			EffectiveDiffuseIrradianceTarget = GAOFillGaps ? DistanceFieldIrradiance2 : DistanceFieldIrradiance;
		}

		FSceneRenderTargetItem& EffectiveBentNormalTarget = GAOFillGaps ? DistanceFieldAOBentNormal2->GetRenderTargetItem() : DistanceFieldAOBentNormal->GetRenderTargetItem();

		FTextureRHIParamRef RenderTargets[2] =
		{
			EffectiveBentNormalTarget.TargetableTexture,
			EffectiveDiffuseIrradianceTarget ? EffectiveDiffuseIrradianceTarget->GetRenderTargetItem().TargetableTexture : NULL,
		};

		SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);

		{
			RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TDistanceFieldAOCombinePS<true> > PixelShader(View.ShaderMap);
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(RHICmdList, View, BentNormalInterpolation, IrradianceInterpolation, DistanceFieldNormal, Parameters);
			}
			else
			{
				TShaderMapRef<TDistanceFieldAOCombinePS<false> > PixelShader(View.ShaderMap);
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(RHICmdList, View, BentNormalInterpolation, IrradianceInterpolation, DistanceFieldNormal, Parameters);
			}

			DrawRectangle( 
				RHICmdList,
				0, 0, 
				View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
				0, 0, 
				View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
				FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
				GetBufferSizeForAO(),
				*VertexShader);
		}

		RHICmdList.CopyToResolveTarget(EffectiveBentNormalTarget.TargetableTexture, EffectiveBentNormalTarget.ShaderResourceTexture, false, FResolveParams());
			
		if (bUseDistanceFieldGI)
		{
			RHICmdList.CopyToResolveTarget(EffectiveDiffuseIrradianceTarget->GetRenderTargetItem().TargetableTexture, EffectiveDiffuseIrradianceTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
		}
	}

	if (GAOFillGaps)
	{
		SCOPED_DRAW_EVENT(RHICmdList, FillGaps);

		FTextureRHIParamRef RenderTargets[2] =
		{
			DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture,
			bUseDistanceFieldGI ? DistanceFieldIrradiance->GetRenderTargetItem().TargetableTexture : NULL,
		};

		SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);

		{
			RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

			if (GAOFillGapsHighQuality)
			{
				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TFillGapsPS<true, true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal2->GetRenderTargetItem(), DistanceFieldIrradiance2);
				}
				else
				{
					TShaderMapRef<TFillGapsPS<false, true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal2->GetRenderTargetItem(), DistanceFieldIrradiance2);
				}
			}
			else
			{
				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TFillGapsPS<true, false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal2->GetRenderTargetItem(), DistanceFieldIrradiance2);
				}
				else
				{
					TShaderMapRef<TFillGapsPS<false, false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal2->GetRenderTargetItem(), DistanceFieldIrradiance2);
				}
			}

			VertexShader->SetParameters(RHICmdList, View);

			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			DrawRectangle( 
				RHICmdList,
				0, 0, 
				View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
				0, 0, 
				View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
				FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
				SceneContext.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
				*VertexShader);
		}

		RHICmdList.CopyToResolveTarget(DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture, DistanceFieldAOBentNormal->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
		if (bUseDistanceFieldGI)
		{
			RHICmdList.CopyToResolveTarget(DistanceFieldIrradiance->GetRenderTargetItem().TargetableTexture, DistanceFieldIrradiance->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
		}
	}

	FSceneViewState* ViewState = View.ViewState;
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState = ViewState ? &ViewState->DistanceFieldAOHistoryRT : NULL;
	TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryState = ViewState ? &ViewState->DistanceFieldIrradianceHistoryRT : NULL;
	BentNormalOutput = DistanceFieldAOBentNormal;
	IrradianceOutput = DistanceFieldIrradiance;

	if (GAOUseHistory)
	{
		UpdateHistory(
			RHICmdList,
			View, 
			TEXT("DistanceFieldAOHistory"),
			TEXT("DistanceFieldIrradianceHistory"),
			VelocityTexture,
			BentNormalHistoryState,
			IrradianceHistoryState,
			DistanceFieldAOBentNormal, 
			DistanceFieldIrradiance,
			BentNormalOutput,
			IrradianceOutput);
	}
}

enum EAOUpsampleType
{
	AOUpsample_OutputBentNormal,
	AOUpsample_OutputAO,
	AOUpsample_OutputBentNormalAndIrradiance,
	AOUpsample_OutputIrradiance
};

template<EAOUpsampleType UpsampleType>
class TDistanceFieldAOUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldAOUpsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("OUTPUT_BENT_NORMAL"), (UpsampleType == AOUpsample_OutputBentNormal || UpsampleType == AOUpsample_OutputBentNormalAndIrradiance) ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), (UpsampleType == AOUpsample_OutputIrradiance || UpsampleType == AOUpsample_OutputBentNormalAndIrradiance) ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("OUTPUT_AO"), (UpsampleType == AOUpsample_OutputAO) ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TDistanceFieldAOUpsamplePS() {}

	/** Initialization constructor. */
	TDistanceFieldAOUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		BentNormalAOTexture.Bind(Initializer.ParameterMap,TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap,TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap,TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap,TEXT("IrradianceSampler"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, IPooledRenderTarget* DistanceFieldIrradiance)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, BentNormalAOTexture, BentNormalAOSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), DistanceFieldAOBentNormal->GetRenderTargetItem().ShaderResourceTexture);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, IrradianceTexture, IrradianceSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), DistanceFieldIrradiance->GetRenderTargetItem().ShaderResourceTexture);
		}
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
};

#define IMPLEMENT_UPSAMPLE_PS_TYPE(UpsampleType) \
	typedef TDistanceFieldAOUpsamplePS<UpsampleType> TDistanceFieldAOUpsamplePS##UpsampleType; \
	IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldAOUpsamplePS##UpsampleType,TEXT("DistanceFieldLightingPost"),TEXT("AOUpsamplePS"),SF_Pixel);

IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormal)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputAO)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormalAndIrradiance)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputIrradiance)

void UpsampleBentNormalAO(
	FRHICommandList& RHICmdList, 
	const TArray<FViewInfo>& Views, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldIrradiance,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

		SCOPED_DRAW_EVENT(RHICmdList, UpsampleAO);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

		if (bVisualizeAmbientOcclusion)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputAO> > PixelShader(View.ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
		}
		else if (bVisualizeGlobalIllumination && bUseDistanceFieldGI)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputIrradiance> > PixelShader(View.ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
		}
		else
		{
			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputBentNormalAndIrradiance> > PixelShader(View.ShaderMap);
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
			}
			else
			{
				TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputBentNormal> > PixelShader(View.ShaderMap);
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
			}
		}

		DrawRectangle( 
			RHICmdList,
			0, 0, 
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X / GAODownsampleFactor, View.ViewRect.Min.Y / GAODownsampleFactor, 
			View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			GetBufferSizeForAO(),
			*VertexShader);
	}
}
