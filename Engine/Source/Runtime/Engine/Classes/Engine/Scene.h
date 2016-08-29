// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// Scene - script exposed scene enums
//=============================================================================

#pragma once
#include "BlendableInterface.h"
#include "Scene.generated.h"

/** Used by FPostProcessSettings Depth of Fields */
UENUM()
enum EDepthOfFieldMethod
{
	DOFM_BokehDOF UMETA(DisplayName="BokehDOF"),
	DOFM_Gaussian UMETA(DisplayName="GaussianDOF"),
	DOFM_CircleDOF UMETA(DisplayName="CircleDOF"),
	DOFM_MAX,
};

/** Used by rendering project settings. */
UENUM()
enum EAntiAliasingMethod
{
	AAM_None UMETA(DisplayName="None"),
	AAM_FXAA UMETA(DisplayName="FXAA"),
	AAM_TemporalAA UMETA(DisplayName="TemporalAA"),
	/** Only supported with forward shading.  MSAA sample count is controlled by r.MSAACount. */
	AAM_MSAA UMETA(DisplayName="MSAA"),
	AAM_MAX,
};

/** Used by FPostProcessSettings Auto Exposure */
UENUM()
enum EAutoExposureMethod
{
	/** Not supported on mobile, requires compute shader to construct 64 bin histogram */
	AEM_Histogram  UMETA(DisplayName = "Auto Exposure Histogram"),
	/** Not supported on mobile, faster method that computes single value by downsampling */
	AEM_Basic      UMETA(DisplayName = "Auto Exposure Basic"),
	AEM_MAX,
};

USTRUCT()
struct FWeightedBlendable
{
	GENERATED_USTRUCT_BODY()

	/** 0:no effect .. 1:full effect */
	UPROPERTY(interp, BlueprintReadWrite, Category=FWeightedBlendable, meta=(ClampMin = "0.0", ClampMax = "1.0", Delta = "0.01"))
	float Weight;

	/** should be of the IBlendableInterface* type but UProperties cannot express that */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FWeightedBlendable, meta=( AllowedClasses="BlendableInterface", Keywords="PostProcess" ))
	UObject* Object;

	// default constructor
	FWeightedBlendable()
		: Weight(-1)
		, Object(0)
	{
	}

	// constructor
	// @param InWeight -1 is used to hide the weight and show the "Choose" UI, 0:no effect .. 1:full effect
	FWeightedBlendable(float InWeight, UObject* InObject)
		: Weight(InWeight)
		, Object(InObject)
	{
	}
};

// for easier detail customization, needed?
USTRUCT()
struct FWeightedBlendables
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcessSettings", meta=( Keywords="PostProcess" ))
	TArray<FWeightedBlendable> Array;
};


/** To be able to use struct PostProcessSettings. */
// Each property consists of a bool to enable it (by default off),
// the variable declaration and further down the default value for it.
// The comment should include the meaning and usable range.
USTRUCT(BlueprintType, meta=(HiddenByDefault))
struct FPostProcessSettings
{
	GENERATED_USTRUCT_BODY()

	// first all bOverride_... as they get grouped together into bitfields

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_WhiteTemp:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_WhiteTint:1;

	// Color Correction controls
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorSaturation:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorContrast:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGamma:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGain:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorOffset:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorSaturationShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorContrastShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGammaShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGainShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorOffsetShadows : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorSaturationMidtones : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorContrastMidtones : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGammaMidtones : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGainMidtones : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorOffsetMidtones : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorSaturationHighlights : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorContrastHighlights : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGammaHighlights : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGainHighlights : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorOffsetHighlights : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmWhitePoint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmSaturation:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmChannelMixerRed:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmChannelMixerGreen:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmChannelMixerBlue:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmContrast:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmDynamicRange:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmHealAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmToeAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmShadowTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmShadowTintBlend:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmShadowTintAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmSlope:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmToe:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmShoulder:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmBlackClip:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmWhiteClip:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_SceneColorTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_SceneFringeIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientCubemapTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientCubemapIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom1Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom1Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom2Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom2Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom3Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom3Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom4Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom4Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom5Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom5Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom6Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom6Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomSizeScale:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomDirtMaskIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomDirtMaskTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomDirtMask:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
    uint32 bOverride_AutoExposureMethod:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureLowPercent:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureHighPercent:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureMinBrightness:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureMaxBrightness:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureSpeedUp:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureSpeedDown:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureBias:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_HistogramLogMin:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_HistogramLogMax:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareTints:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareBokehSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareBokehShape:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VignetteIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_GrainIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_GrainJitter:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionStaticFraction:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionRadius:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionFadeDistance:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionFadeRadius:1;

	UPROPERTY()
	uint32 bOverride_AmbientOcclusionDistance_DEPRECATED:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionRadiusInWS:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionPower:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionBias:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionQuality:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionMipBlend:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionMipScale:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionMipThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVIntensity:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVDirectionalOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVDirectionalOcclusionRadius:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVDiffuseOcclusionExponent:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVSpecularOcclusionExponent:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVDiffuseOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVSpecularOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSecondaryOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSecondaryBounceIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVGeometryVolumeBias:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVVplInjectionBias:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVEmissiveInjectionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_IndirectLightingColor:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_IndirectLightingIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGradingIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGradingLUT:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFocalDistance:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFstop:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldSensorWidth:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldDepthBlurRadius:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldDepthBlurAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFocalRegion:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldNearTransitionRegion:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFarTransitionRegion:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldScale:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldMaxBokehSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldNearBlurSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFarBlurSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldMethod:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_MobileHQGaussian:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldBokehShape:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldOcclusion:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldColorThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldSizeThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldSkyFocusDistance:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldVignetteSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_MotionBlurAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_MotionBlurMax:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_MotionBlurPerObjectSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenPercentage:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenSpaceReflectionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenSpaceReflectionQuality:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenSpaceReflectionMaxRoughness:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenSpaceReflectionRoughnessScale:1;

	// -----------------------------------------------------------------------

	UPROPERTY(interp, BlueprintReadWrite, Category=WhiteBalance, meta=(UIMin = "1500.0", UIMax = "15000.0", editcondition = "bOverride_WhiteTemp", DisplayName = "Temp"))
	float WhiteTemp;
	UPROPERTY(interp, BlueprintReadWrite, Category=WhiteBalance, meta=(UIMin = "-1.0", UIMax = "1.0", editcondition = "bOverride_WhiteTint", DisplayName = "Tint"))
	float WhiteTint;

	// Color Correction controls
	UPROPERTY(interp, BlueprintReadWrite, Category = ColorGrading, meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorSaturation", DisplayName = "Saturation"))
	FVector4 ColorSaturation;
	UPROPERTY(interp, BlueprintReadWrite, Category = ColorGrading, meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorContrast", DisplayName = "Contrast"))
	FVector4 ColorContrast;
	UPROPERTY(interp, BlueprintReadWrite, Category = ColorGrading, meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorGamma", DisplayName = "Gamma"))
	FVector4 ColorGamma;
	UPROPERTY(interp, BlueprintReadWrite, Category = ColorGrading, meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorGain", DisplayName = "Gain"))
	FVector4 ColorGain;
	UPROPERTY(interp, BlueprintReadWrite, Category = ColorGrading, meta = (UIMin = "-1.0", UIMax = "1.0", editcondition = "bOverride_ColorOffset", DisplayName = "Offset"))
	FVector4 ColorOffset;

	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Shadows", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorSaturationShadows", DisplayName = "SaturationShadows"))
	FVector4 ColorSaturationShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Shadows", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorContrastShadows", DisplayName = "ContrastShadows"))
	FVector4 ColorContrastShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Shadows", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorGammaShadows", DisplayName = "GammaShadows"))
	FVector4 ColorGammaShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Shadows", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorGainShadows", DisplayName = "GainShadows"))
	FVector4 ColorGainShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Shadows", meta = (UIMin = "-1.0", UIMax = "1.0", editcondition = "bOverride_ColorOffsetShadows", DisplayName = "OffsetShadows"))
	FVector4 ColorOffsetShadows;

	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Midtones", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorSaturationMidtones", DisplayName = "SaturationMidtones"))
	FVector4 ColorSaturationMidtones;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Midtones", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorContrastMidtones", DisplayName = "ContrastMidtones"))
	FVector4 ColorContrastMidtones;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Midtones", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorGammaMidtones", DisplayName = "GammaMidtones"))
	FVector4 ColorGammaMidtones;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Midtones", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorGainMidtones", DisplayName = "GainMidtones"))
	FVector4 ColorGainMidtones;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Midtones", meta = (UIMin = "-1.0", UIMax = "1.0", editcondition = "bOverride_ColorOffsetMidtones", DisplayName = "OffsetMidtones"))
	FVector4 ColorOffsetMidtones;

	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Highlights", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorSaturationHighlights", DisplayName = "SaturationHighlights"))
	FVector4 ColorSaturationHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Highlights", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorContrastHighlights", DisplayName = "ContrastHighlights"))
	FVector4 ColorContrastHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Highlights", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorGammaHighlights", DisplayName = "GammaHighlights"))
	FVector4 ColorGammaHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Highlights", meta = (UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_ColorGainHighlights", DisplayName = "GainHighlights"))
	FVector4 ColorGainHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "ColorGrading|Highlights", meta = (UIMin = "-1.0", UIMax = "1.0", editcondition = "bOverride_ColorOffsetHighlights", DisplayName = "OffsetHighlights"))
	FVector4 ColorOffsetHighlights;

	UPROPERTY(interp, BlueprintReadWrite, Category=Film, meta=(editcondition = "bOverride_FilmWhitePoint", DisplayName = "Tint", HideAlphaChannel))
	FLinearColor FilmWhitePoint;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(editcondition = "bOverride_FilmShadowTint", DisplayName = "Tint Shadow", HideAlphaChannel))
	FLinearColor FilmShadowTint;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmShadowTintBlend", DisplayName = "Tint Shadow Blend"))
	float FilmShadowTintBlend;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmShadowTintAmount", DisplayName = "Tint Shadow Amount"))
	float FilmShadowTintAmount;

	UPROPERTY(interp, BlueprintReadWrite, Category=Film, meta=(UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_FilmSaturation", DisplayName = "Saturation"))
	float FilmSaturation;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerRed", DisplayName = "Channel Mixer Red", HideAlphaChannel))
	FLinearColor FilmChannelMixerRed;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerGreen", DisplayName = "Channel Mixer Green", HideAlphaChannel))
	FLinearColor FilmChannelMixerGreen;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerBlue", DisplayName = " Channel Mixer Blue", HideAlphaChannel))
	FLinearColor FilmChannelMixerBlue;

	UPROPERTY(interp, BlueprintReadWrite, Category=Film, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmContrast", DisplayName = "Contrast"))
	float FilmContrast;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmToeAmount", DisplayName = "Crush Shadows"))
	float FilmToeAmount;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmHealAmount", DisplayName = "Crush Highlights"))
	float FilmHealAmount;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "1.0", UIMax = "4.0", editcondition = "bOverride_FilmDynamicRange", DisplayName = "Dynamic Range"))
	float FilmDynamicRange;

	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmSlope", DisplayName = "Slope"))
	float FilmSlope;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmToe", DisplayName = "Toe"))
	float FilmToe;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmShoulder", DisplayName = "Shoulder"))
	float FilmShoulder;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmBlackClip", DisplayName = "Black clip"))
	float FilmBlackClip;
	UPROPERTY(interp, BlueprintReadWrite, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmWhiteClip", DisplayName = "White clip"))
	float FilmWhiteClip;
	
	/** Scene tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=SceneColor, AdvancedDisplay, meta=(editcondition = "bOverride_SceneColorTint", HideAlphaChannel))
	FLinearColor SceneColorTint;
	
	/** in percent, Scene chromatic aberration / color fringe (camera imperfection) to simulate an artifact that happens in real-world lens, mostly visible in the image corners. */
	UPROPERTY(interp, BlueprintReadWrite, Category=SceneColor, meta=(UIMin = "0.0", UIMax = "5.0", editcondition = "bOverride_SceneFringeIntensity", DisplayName = "Fringe Intensity"))
	float SceneFringeIntensity;

	/** Multiplier for all bloom contributions >=0: off, 1(default), >1 brighter */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_BloomIntensity", DisplayName = "Intensity"))
	float BloomIntensity;

	/**
	 * minimum brightness the bloom starts having effect
	 * -1:all pixels affect bloom equally (physically correct, faster as a threshold pass is omitted), 0:all pixels affect bloom brights more, 1(default), >1 brighter
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "-1.0", UIMax = "8.0", editcondition = "bOverride_BloomThreshold", DisplayName = "Threshold"))
	float BloomThreshold;

	/**
	 * Scale for all bloom sizes
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "64.0", editcondition = "bOverride_BloomSizeScale", DisplayName = "Size scale"))
	float BloomSizeScale;

	/**
	 * Diameter size for the Bloom1 in percent of the screen width
	 * (is done in 1/2 resolution, larger values cost more performance, good for high frequency details)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_Bloom1Size", DisplayName = "#1 Size"))
	float Bloom1Size;
	/**
	 * Diameter size for Bloom2 in percent of the screen width
	 * (is done in 1/4 resolution, larger values cost more performance)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_Bloom2Size", DisplayName = "#2 Size"))
	float Bloom2Size;
	/**
	 * Diameter size for Bloom3 in percent of the screen width
	 * (is done in 1/8 resolution, larger values cost more performance)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "16.0", editcondition = "bOverride_Bloom3Size", DisplayName = "#3 Size"))
	float Bloom3Size;
	/**
	 * Diameter size for Bloom4 in percent of the screen width
	 * (is done in 1/16 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "32.0", editcondition = "bOverride_Bloom4Size", DisplayName = "#4 Size"))
	float Bloom4Size;
	/**
	 * Diameter size for Bloom5 in percent of the screen width
	 * (is done in 1/32 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "64.0", editcondition = "bOverride_Bloom5Size", DisplayName = "#5 Size"))
	float Bloom5Size;
	/**
	 * Diameter size for Bloom6 in percent of the screen width
	 * (is done in 1/64 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "128.0", editcondition = "bOverride_Bloom6Size", DisplayName = "#6 Size"))
	float Bloom6Size;

	/** Bloom1 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom1Tint", DisplayName = "#1 Tint", HideAlphaChannel))
	FLinearColor Bloom1Tint;
	/** Bloom2 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom2Tint", DisplayName = "#2 Tint", HideAlphaChannel))
	FLinearColor Bloom2Tint;
	/** Bloom3 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom3Tint", DisplayName = "#3 Tint", HideAlphaChannel))
	FLinearColor Bloom3Tint;
	/** Bloom4 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom4Tint", DisplayName = "#4 Tint", HideAlphaChannel))
	FLinearColor Bloom4Tint;
	/** Bloom5 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom5Tint", DisplayName = "#5 Tint", HideAlphaChannel))
	FLinearColor Bloom5Tint;
	/** Bloom6 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom6Tint", DisplayName = "#6 Tint", HideAlphaChannel))
	FLinearColor Bloom6Tint;

	/** BloomDirtMask intensity */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_BloomDirtMaskIntensity", DisplayName = "Dirt Mask Intensity"))
	float BloomDirtMaskIntensity;

	/** BloomDirtMask tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_BloomDirtMaskTint", DisplayName = "Dirt Mask Tint", HideAlphaChannel))
	FLinearColor BloomDirtMaskTint;

	/**
	 * Texture that defines the dirt on the camera lens where the light of very bright objects is scattered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Bloom, meta=(editcondition = "bOverride_BloomDirtMask", DisplayName = "Dirt Mask"))
	class UTexture* BloomDirtMask;	// The plan is to replace this texture with many small textures quads for better performance, more control and to animate the effect.

	/** How strong the dynamic GI from the LPV should be. 0.0 is off, 1.0 is the "normal" value, but higher values can be used to boost the effect*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVIntensity", UIMin = "0", UIMax = "20", DisplayName = "Intensity") )
	float LPVIntensity;

	/** Bias applied to light injected into the LPV in cell units. Increase to reduce bleeding through thin walls*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVVplInjectionBias", UIMin = "0", UIMax = "2", DisplayName = "Light Injection Bias") )
	float LPVVplInjectionBias;

	/** The size of the LPV volume, in Unreal units*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSize", UIMin = "100", UIMax = "20000", DisplayName = "Size") )
	float LPVSize;

	/** Secondary occlusion strength (bounce light shadows). Set to 0 to disable*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSecondaryOcclusionIntensity", UIMin = "0", UIMax = "1", DisplayName = "Secondary Occlusion Intensity") )
	float LPVSecondaryOcclusionIntensity;

	/** Secondary bounce light strength (bounce light shadows). Set to 0 to disable*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVSecondaryBounceIntensity", UIMin = "0", UIMax = "1", DisplayName = "Secondary Bounce Intensity") )
	float LPVSecondaryBounceIntensity;

	/** Bias applied to the geometry volume in cell units. Increase to reduce darkening due to secondary occlusion */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVGeometryVolumeBias", UIMin = "0", UIMax = "2", DisplayName = "Geometry Volume Bias"))
	float LPVGeometryVolumeBias;

	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVEmissiveInjectionIntensity", UIMin = "0", UIMax = "20", DisplayName = "Emissive Injection Intensity") )
	float LPVEmissiveInjectionIntensity;

	/** Controls the amount of directional occlusion. Requires LPV. Values very close to 1.0 are recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVDirectionalOcclusionIntensity", UIMin = "0", UIMax = "1", DisplayName = "Occlusion Intensity") )
	float LPVDirectionalOcclusionIntensity;

	/** Occlusion Radius - 16 is recommended for most scenes */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVDirectionalOcclusionRadius", UIMin = "1", UIMax = "16", DisplayName = "Occlusion Radius") )
	float LPVDirectionalOcclusionRadius;

	/** Diffuse occlusion exponent - increase for more contrast. 1 to 2 is recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVDiffuseOcclusionExponent", UIMin = "0.5", UIMax = "5", DisplayName = "Diffuse occlusion exponent") )
	float LPVDiffuseOcclusionExponent;

	/** Specular occlusion exponent - increase for more contrast. 6 to 9 is recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSpecularOcclusionExponent", UIMin = "1", UIMax = "16", DisplayName = "Specular occlusion exponent") )
	float LPVSpecularOcclusionExponent;

	/** Diffuse occlusion intensity - higher values provide increased diffuse occlusion.*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVDiffuseOcclusionIntensity", UIMin = "0", UIMax = "4", DisplayName = "Diffuse occlusion intensity") )
	float LPVDiffuseOcclusionIntensity;

	/** Specular occlusion intensity - higher values provide increased specular occlusion.*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVSpecularOcclusionIntensity", UIMin = "0", UIMax = "4", DisplayName = "Specular occlusion intensity") )
	float LPVSpecularOcclusionIntensity;
	
	/** AmbientCubemap tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientCubemap, AdvancedDisplay, meta=(editcondition = "bOverride_AmbientCubemapTint", DisplayName = "Tint", HideAlphaChannel))
	FLinearColor AmbientCubemapTint;
	/**
	 * To scale the Ambient cubemap brightness
	 * >=0: off, 1(default), >1 brighter
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientCubemap, meta=(ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_AmbientCubemapIntensity", DisplayName = "Intensity"))
	float AmbientCubemapIntensity;
	/** The Ambient cubemap (Affects diffuse and specular shading), blends additively which if different from all other settings here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AmbientCubemap, meta=(DisplayName = "Cubemap Texture"))
	class UTextureCube* AmbientCubemap;


	/** Luminance computation method */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AutoExposure, meta=(editcondition = "bOverride_AutoExposureMethod", DisplayName = "Method"))
    TEnumAsByte<enum EAutoExposureMethod> AutoExposureMethod;

	/**
	 * The eye adaptation will adapt to a value extracted from the luminance histogram of the scene color.
	 * The value is defined as having x percent below this brightness. Higher values give bright spots on the screen more priority
	 * but can lead to less stable results. Lower values give the medium and darker values more priority but might cause burn out of
	 * bright spots.
	 * >0, <100, good values are in the range 70 .. 80
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AutoExposure, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_AutoExposureLowPercent", DisplayName = "Low Percent"))
	float AutoExposureLowPercent;

	/**
	 * The eye adaptation will adapt to a value extracted from the luminance histogram of the scene color.
	 * The value is defined as having x percent below this brightness. Higher values give bright spots on the screen more priority
	 * but can lead to less stable results. Lower values give the medium and darker values more priority but might cause burn out of
	 * bright spots.
	 * >0, <100, good values are in the range 80 .. 95
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AutoExposure, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_AutoExposureHighPercent", DisplayName = "High Percent"))
	float AutoExposureHighPercent;

	/**
	 * A good value should be positive near 0. This is the minimum brightness the auto exposure can adapt to.
	 * It should be tweaked in a dark lighting situation (too small: image appears too bright, too large: image appears too dark).
	 * Note: Tweaking emissive materials and lights or tweaking auto exposure can look the same. Tweaking auto exposure has global
	 * effect and defined the HDR range - you don't want to change that late in the project development.
	 * Eye Adaptation is disabled if MinBrightness = MaxBrightness
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AutoExposure, meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AutoExposureMinBrightness", DisplayName = "Min Brightness"))
	float AutoExposureMinBrightness;

	/**
	 * A good value should be positive (2 is a good value). This is the maximum brightness the auto exposure can adapt to.
	 * It should be tweaked in a bright lighting situation (too small: image appears too bright, too large: image appears too dark).
	 * Note: Tweaking emissive materials and lights or tweaking auto exposure can look the same. Tweaking auto exposure has global
	 * effect and defined the HDR range - you don't want to change that late in the project development.
	 * Eye Adaptation is disabled if MinBrightness = MaxBrightness
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AutoExposure, meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AutoExposureMaxBrightness", DisplayName = "Max Brightness"))
	float AutoExposureMaxBrightness;

	/** >0 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AutoExposure, AdvancedDisplay, meta=(ClampMin = "0.02", UIMax = "20.0", editcondition = "bOverride_AutoExposureSpeedUp", DisplayName = "Speed Up"))
	float AutoExposureSpeedUp;

	/** >0 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AutoExposure, AdvancedDisplay, meta=(ClampMin = "0.02", UIMax = "20.0", editcondition = "bOverride_AutoExposureSpeedDown", DisplayName = "Speed Down"))
	float AutoExposureSpeedDown;

	/**
	 * Logarithmic adjustment for the exposure. Only used if a tonemapper is specified.
	 * 0: no adjustment, -1:2x darker, -2:4x darker, 1:2x brighter, 2:4x brighter, ...
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category = AutoExposure, meta = (UIMin = "-8.0", UIMax = "8.0", editcondition = "bOverride_AutoExposureBias", DisplayName = "Exposure Bias"))
	float AutoExposureBias;

	/** temporary exposed until we found good values, -8: 1/256, -10: 1/1024 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AutoExposure, AdvancedDisplay, meta=(UIMin = "-16", UIMax = "0.0", editcondition = "bOverride_HistogramLogMin"))
	float HistogramLogMin;

	/** temporary exposed until we found good values 4: 16, 8: 256 */
	UPROPERTY(interp, BlueprintReadWrite, Category=AutoExposure, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "16.0", editcondition = "bOverride_HistogramLogMax"))
	float HistogramLogMax;

	/** Brightness scale of the image cased lens flares (linear) */
	UPROPERTY(interp, BlueprintReadWrite, Category=LensFlares, meta=(UIMin = "0.0", UIMax = "16.0", editcondition = "bOverride_LensFlareIntensity", DisplayName = "Intensity"))
	float LensFlareIntensity;

	/** Tint color for the image based lens flares. */
	UPROPERTY(interp, BlueprintReadWrite, Category=LensFlares, AdvancedDisplay, meta=(editcondition = "bOverride_LensFlareTint", DisplayName = "Tint", HideAlphaChannel))
	FLinearColor LensFlareTint;

	/** Size of the Lens Blur (in percent of the view width) that is done with the Bokeh texture (note: performance cost is radius*radius) */
	UPROPERTY(interp, BlueprintReadWrite, Category=LensFlares, meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_LensFlareBokehSize", DisplayName = "BokehSize"))
	float LensFlareBokehSize;

	/** Minimum brightness the lens flare starts having effect (this should be as high as possible to avoid the performance cost of blurring content that is too dark too see) */
	UPROPERTY(interp, BlueprintReadWrite, Category=LensFlares, AdvancedDisplay, meta=(UIMin = "0.1", UIMax = "32.0", editcondition = "bOverride_LensFlareThreshold", DisplayName = "Threshold"))
	float LensFlareThreshold;

	/** Defines the shape of the Bokeh when the image base lens flares are blurred, cannot be blended */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LensFlares, meta=(editcondition = "bOverride_LensFlareBokehShape", DisplayName = "BokehShape"))
	class UTexture* LensFlareBokehShape;

	/** RGB defines the lens flare color, A it's position. This is a temporary solution. */
	UPROPERTY(EditAnywhere, Category=LensFlares, meta=(editcondition = "bOverride_LensFlareTints", DisplayName = "Tints"))
	FLinearColor LensFlareTints[8];

	/** 0..1 0=off/no vignette .. 1=strong vignette */
	UPROPERTY(interp, BlueprintReadWrite, Category=SceneColor, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_VignetteIntensity"))
	float VignetteIntensity;

	/** 0..1 grain jitter */
	UPROPERTY(interp, BlueprintReadWrite, Category = SceneColor, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_GrainJitter"))
	float GrainJitter;

	/** 0..1 grain intensity */
	UPROPERTY(interp, BlueprintReadWrite, Category=SceneColor, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_GrainIntensity"))
	float GrainIntensity;

	/** 0..1 0=off/no ambient occlusion .. 1=strong ambient occlusion, defines how much it affects the non direct lighting after base pass */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_AmbientOcclusionIntensity", DisplayName = "Intensity"))
	float AmbientOcclusionIntensity;

	/** 0..1 0=no effect on static lighting .. 1=AO affects the stat lighting, 0 is free meaning no extra rendering pass */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_AmbientOcclusionStaticFraction", DisplayName = "Static Fraction"))
	float AmbientOcclusionStaticFraction;

	/** >0, in unreal units, bigger values means even distant surfaces affect the ambient occlusion */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, meta=(ClampMin = "0.1", UIMax = "500.0", editcondition = "bOverride_AmbientOcclusionRadius", DisplayName = "Radius"))
	float AmbientOcclusionRadius;

	/** true: AO radius is in world space units, false: AO radius is locked the view space in 400 units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(editcondition = "bOverride_AmbientOcclusionRadiusInWS", DisplayName = "Radius in WorldSpace"))
	uint32 AmbientOcclusionRadiusInWS:1;

	/** >0, in unreal units, at what distance the AO effect disppears in the distance (avoding artifacts and AO effects on huge object) */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "20000.0", editcondition = "bOverride_AmbientOcclusionFadeDistance", DisplayName = "Fade Out Distance"))
	float AmbientOcclusionFadeDistance;
	
	/** >0, in unreal units, how many units before AmbientOcclusionFadeOutDistance it starts fading out */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "20000.0", editcondition = "bOverride_AmbientOcclusionFadeRadius", DisplayName = "Fade Out Radius"))
	float AmbientOcclusionFadeRadius;

	/** >0, in unreal units, how wide the ambient occlusion effect should affect the geometry (in depth), will be removed - only used for non normal method which is not exposed */
	UPROPERTY()
	float AmbientOcclusionDistance_DEPRECATED;

	/** >0, in unreal units, bigger values means even distant surfaces affect the ambient occlusion */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.1", UIMax = "8.0", editcondition = "bOverride_AmbientOcclusionPower", DisplayName = "Power"))
	float AmbientOcclusionPower;

	/** >0, in unreal units, default (3.0) works well for flat surfaces but can reduce details */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AmbientOcclusionBias", DisplayName = "Bias"))
	float AmbientOcclusionBias;

	/** 0=lowest quality..100=maximum quality, only a few quality levels are implemented, no soft transition */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_AmbientOcclusionQuality", DisplayName = "Quality"))
	float AmbientOcclusionQuality;

	/** Affects the blend over the multiple mips (lower resolution versions) , 0:fully use full resolution, 1::fully use low resolution, around 0.6 seems to be a good value */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.1", UIMax = "1.0", editcondition = "bOverride_AmbientOcclusionMipBlend", DisplayName = "Mip Blend"))
	float AmbientOcclusionMipBlend;

	/** Affects the radius AO radius scale over the multiple mips (lower resolution versions) */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.5", UIMax = "4.0", editcondition = "bOverride_AmbientOcclusionMipScale", DisplayName = "Mip Scale"))
	float AmbientOcclusionMipScale;

	/** to tweak the bilateral upsampling when using multiple mips (lower resolution versions) */
	UPROPERTY(interp, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "0.1", editcondition = "bOverride_AmbientOcclusionMipThreshold", DisplayName = "Mip Threshold"))
	float AmbientOcclusionMipThreshold;

	/** Adjusts indirect lighting color. (1,1,1) is default. (0,0,0) to disable GI. The show flag 'Global Illumination' must be enabled to use this property. */
	UPROPERTY(interp, BlueprintReadWrite, Category=GlobalIllumination, meta=(editcondition = "bOverride_IndirectLightingColor", DisplayName = "Indirect Lighting Color", HideAlphaChannel))
	FLinearColor IndirectLightingColor;

	/** Scales the indirect lighting contribution. A value of 0 disables GI. Default is 1. The show flag 'Global Illumination' must be enabled to use this property. */
	UPROPERTY(interp, BlueprintReadWrite, Category=GlobalIllumination, meta=(ClampMin = "0", UIMax = "4.0", editcondition = "bOverride_IndirectLightingIntensity", DisplayName = "Indirect Lighting Intensity"))
	float IndirectLightingIntensity;

	/** 0..1=full intensity */
	UPROPERTY(interp, BlueprintReadWrite, Category=SceneColor, meta=(ClampMin = "0", ClampMax = "1.0", editcondition = "bOverride_ColorGradingIntensity", DisplayName = "Color Grading Intensity"))
	float ColorGradingIntensity;

	/** Name of the LUT texture e.g. MyPackage01.LUTNeutral, empty if not used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneColor, meta=(editcondition = "bOverride_ColorGradingLUT", DisplayName = "Color Grading"))
	class UTexture* ColorGradingLUT;

	/** BokehDOF, Simple gaussian, ... Mobile supports Gaussian only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DepthOfField, meta=(editcondition = "bOverride_DepthOfFieldMethod", DisplayName = "Method"))
	TEnumAsByte<enum EDepthOfFieldMethod> DepthOfFieldMethod;

	/** Enable HQ Gaussian on high end mobile platforms. (ES3_1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DepthOfField, meta = (editcondition = "bOverride_MobileHQGaussian", DisplayName = "High Quality Gaussian DoF on Mobile"))
	uint32 bMobileHQGaussian : 1;

	/** CircleDOF only: Defines the opening of the camera lens, Aperture is 1/fstop, typical lens go down to f/1.2 (large opening), larger numbers reduce the DOF effect */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(ClampMin = "1.0", ClampMax = "32.0", editcondition = "bOverride_DepthOfFieldFstop", DisplayName = "Aperture F-stop"))
	float DepthOfFieldFstop;

	/** Width of the camera sensor to assume, in mm. */
	UPROPERTY(BlueprintReadWrite, Category=DepthOfField, meta=(ForceUnits=mm, ClampMin = "0.1", UIMin="0.1", UIMax= "1000.0", editcondition = "bOverride_DepthOfFieldSensorWidth", DisplayName = "Sensor Width (mm)"))
	float DepthOfFieldSensorWidth;

	/** Distance in which the Depth of Field effect should be sharp, in unreal units (cm) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(ClampMin = "0.0", UIMin = "1.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFocalDistance", DisplayName = "Focal Distance"))
	float DepthOfFieldFocalDistance;

	/** CircleDOF only: Depth blur km for 50% */
	UPROPERTY(interp, BlueprintReadWrite, AdvancedDisplay, Category=DepthOfField, meta=(ClampMin = "0.000001", ClampMax = "100.0", editcondition = "bOverride_DepthOfFieldDepthBlurAmount", DisplayName = "Depth Blur km for 50%"))
	float DepthOfFieldDepthBlurAmount;

	/** CircleDOF only: Depth blur radius in pixels at 1920x */
	UPROPERTY(interp, BlueprintReadWrite, AdvancedDisplay, Category=DepthOfField, meta=(ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_DepthOfFieldDepthBlurRadius", DisplayName = "Depth Blur Radius"))
	float DepthOfFieldDepthBlurRadius;

	/** Artificial region where all content is in focus, starting after DepthOfFieldFocalDistance, in unreal units  (cm) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFocalRegion", DisplayName = "Focal Region"))
	float DepthOfFieldFocalRegion;

	/** To define the width of the transition region next to the focal region on the near side (cm) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldNearTransitionRegion", DisplayName = "Near Transition Region"))
	float DepthOfFieldNearTransitionRegion;

	/** To define the width of the transition region next to the focal region on the near side (cm) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFarTransitionRegion", DisplayName = "Far Transition Region"))
	float DepthOfFieldFarTransitionRegion;

	/** SM5: BokehDOF only: To amplify the depth of field effect (like aperture)  0=off 
	    ES2: Used to blend DoF. 0=off
	*/
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(ClampMin = "0.0", ClampMax = "2.0", editcondition = "bOverride_DepthOfFieldScale", DisplayName = "Scale"))
	float DepthOfFieldScale;

	/** BokehDOF only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size*size) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldMaxBokehSize", DisplayName = "Max Bokeh Size"))
	float DepthOfFieldMaxBokehSize;

	/** Gaussian only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldNearBlurSize", DisplayName = "Near Blur Size"))
	float DepthOfFieldNearBlurSize;

	/** Gaussian only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldFarBlurSize", DisplayName = "Far Blur Size"))
	float DepthOfFieldFarBlurSize;

	/** Defines the shape of the Bokeh when object get out of focus, cannot be blended */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=DepthOfField, meta=(editcondition = "bOverride_DepthOfFieldBokehShape", DisplayName = "Shape"))
	class UTexture* DepthOfFieldBokehShape;

	/** Occlusion tweak factor 1 (0.18 to get natural occlusion, 0.4 to solve layer color leaking issues) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_DepthOfFieldOcclusion", DisplayName = "Occlusion"))
	float DepthOfFieldOcclusion;
	
	/** Color threshold to do full quality DOF (BokehDOF only) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "10.0", editcondition = "bOverride_DepthOfFieldColorThreshold", DisplayName = "Color Threshold"))
	float DepthOfFieldColorThreshold;

	/** Size threshold to do full quality DOF (BokehDOF only) */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_DepthOfFieldSizeThreshold", DisplayName = "Size Threshold"))
	float DepthOfFieldSizeThreshold;
	
	/** Artificial distance to allow the skybox to be in focus (e.g. 200000), <=0 to switch the feature off, only for GaussianDOF, can cost performance */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "200000.0", editcondition = "bOverride_DepthOfFieldSkyFocusDistance", DisplayName = "Sky Distance"))
	float DepthOfFieldSkyFocusDistance;

	/** Artificial circular mask to (near) blur content outside the radius, only for GaussianDOF, diameter in percent of screen width, costs performance if the mask is used, keep Feather can Radius on default to keep it off */
	UPROPERTY(interp, BlueprintReadWrite, Category=DepthOfField, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "100.0", editcondition = "bOverride_DepthOfFieldVignetteSize", DisplayName = "Vignette Size"))
	float DepthOfFieldVignetteSize;

	/** Strength of motion blur, 0:off, should be renamed to intensity */
	UPROPERTY(interp, BlueprintReadWrite, Category=MotionBlur, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_MotionBlurAmount", DisplayName = "Amount"))
	float MotionBlurAmount;
	/** max distortion caused by motion blur, in percent of the screen width, 0:off */
	UPROPERTY(interp, BlueprintReadWrite, Category=MotionBlur, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_MotionBlurMax", DisplayName = "Max"))
	float MotionBlurMax;
	/** The minimum projected screen radius for a primitive to be drawn in the velocity pass, percentage of screen width. smaller numbers cause more draw calls, default: 4% */
	UPROPERTY(interp, BlueprintReadWrite, Category=MotionBlur, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_MotionBlurPerObjectSize", DisplayName = "Per Object Size"))
	float MotionBlurPerObjectSize;

	/**
	 * To render with lower or high resolution than it is presented, 
	 * controlled by console variable, 
	 * 100:off, needs to be <99 to get upsampling and lower to get performance,
	 * >100 for super sampling (slower but higher quality), 
	 * only applied in game
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category=Misc, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "400.0", editcondition = "bOverride_ScreenPercentage"))
	float ScreenPercentage;

	/** Enable/Fade/disable the Screen Space Reflection feature, in percent, avoid numbers between 0 and 1 fo consistency */
	UPROPERTY(interp, BlueprintReadWrite, Category=ScreenSpaceReflections, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_ScreenSpaceReflectionIntensity", DisplayName = "Intensity"))
	float ScreenSpaceReflectionIntensity;

	/** 0=lowest quality..100=maximum quality, only a few quality levels are implemented, no soft transition, 50 is the default for better performance. */
	UPROPERTY(interp, BlueprintReadWrite, Category=ScreenSpaceReflections, meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_ScreenSpaceReflectionQuality", DisplayName = "Quality"))
	float ScreenSpaceReflectionQuality;

	/** Until what roughness we fade the screen space reflections, 0.8 works well, smaller can run faster */
	UPROPERTY(interp, BlueprintReadWrite, Category=ScreenSpaceReflections, meta=(ClampMin = "0.01", ClampMax = "1.0", editcondition = "bOverride_ScreenSpaceReflectionMaxRoughness", DisplayName = "Max Roughness"))
	float ScreenSpaceReflectionMaxRoughness;

	// Note: Adding properties before this line require also changes to the OverridePostProcessSettings() function and 
	// FPostProcessSettings constructor and possibly the SetBaseValues() method.
	// -----------------------------------------------------------------------
	
	/**
	 * Allows custom post process materials to be defined, using a MaterialInstance with the same Material as its parent to allow blending.
	 * For materials this needs to be the "PostProcess" domain type. This can be used for any UObject object implementing the IBlendableInterface (e.g. could be used to fade weather settings).
	 */
	UPROPERTY(EditAnywhere, Category="PostProcessSettings", meta=( Keywords="PostProcess", DisplayName = "Blendables" ))
	FWeightedBlendables WeightedBlendables;

	// for backwards compatibility
	UPROPERTY()
	TArray<UObject*> Blendables_DEPRECATED;

	// for backwards compatibility
	void OnAfterLoad()
	{
		for(int32 i = 0, count = Blendables_DEPRECATED.Num(); i < count; ++i)
		{
			if(Blendables_DEPRECATED[i])
			{
				FWeightedBlendable Element(1.0f, Blendables_DEPRECATED[i]);
				WeightedBlendables.Array.Add(Element);
			}
		}
		Blendables_DEPRECATED.Empty();
	}

	// Adds an Blendable (implements IBlendableInterface) to the array of Blendables (if it doesn't exist) and update the weight
	// @param InBlendableObject silently ignores if no object is referenced
	// @param 0..1 InWeight, values outside of the range get clampled later in the pipeline
	void AddBlendable(TScriptInterface<IBlendableInterface> InBlendableObject, float InWeight)
	{
		// update weight, if the Blendable is already in the array
		if(UObject* Object = InBlendableObject.GetObject())
		{
			for (int32 i = 0, count = WeightedBlendables.Array.Num(); i < count; ++i)
			{
				if (WeightedBlendables.Array[i].Object == Object)
				{
					WeightedBlendables.Array[i].Weight = InWeight;
					// We assumes we only have one
					return;
				}
			}

			// add in the end
			WeightedBlendables.Array.Add(FWeightedBlendable(InWeight, Object));
		}
	}

	// removes one or multiple blendables from the array
	void RemoveBlendable(TScriptInterface<IBlendableInterface> InBlendableObject)
	{
		if(UObject* Object = InBlendableObject.GetObject())
		{
			for (int32 i = 0, count = WeightedBlendables.Array.Num(); i < count; ++i)
			{
				if (WeightedBlendables.Array[i].Object == Object)
				{
					// this might remove multiple
					WeightedBlendables.Array.RemoveAt(i);
					--i;
					--count;
				}
			}
		}
	}

	// good start values for a new volume, by default no value is overriding
	FPostProcessSettings()
	{
		// to set all bOverride_.. by default to false
		FMemory::Memzero(this, sizeof(FPostProcessSettings));

		WhiteTemp = 6500.0f;
		WhiteTint = 0.0f;

		// Color Correction controls
		ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorContrast = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorGamma = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorGain = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorOffset = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

		ColorSaturationShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorContrastShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorGammaShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorGainShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorOffsetShadows = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

		ColorSaturationMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorContrastMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorGammaMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorGainMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorOffsetMidtones = FVector4(0.f, 0.0f, 0.0f, 0.0f);

		ColorSaturationHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorContrastHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorGammaHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorGainHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ColorOffsetHighlights = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

		// default values:
		FilmWhitePoint = FLinearColor(1.0f,1.0f,1.0f);
		FilmSaturation = 1.0f;
		FilmChannelMixerRed = FLinearColor(1.0f,0.0f,0.0f);
		FilmChannelMixerGreen = FLinearColor(0.0f,1.0f,0.0f);
		FilmChannelMixerBlue = FLinearColor(0.0f,0.0f,1.0f);
		FilmContrast = 0.03f;
		FilmDynamicRange = 4.0f;
		FilmHealAmount = 0.18f;
		FilmToeAmount = 1.0f;
		FilmShadowTint = FLinearColor(1.0f,1.0f,1.0f);
		FilmShadowTintBlend = 0.5;
		FilmShadowTintAmount = 0.0;

		// ACES settings
		FilmSlope = 0.88f;
		FilmToe = 0.55f;
		FilmShoulder = 0.26f;
		FilmBlackClip = 0.0f;
		FilmWhiteClip = 0.04f;

		SceneColorTint = FLinearColor(1, 1, 1);
		SceneFringeIntensity = 0.0f;
		// next value might get overwritten by r.DefaultFeature.Bloom
		BloomIntensity = 1.0f;
		BloomThreshold = 1.0f;
		Bloom1Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		// default is 4 to maintain old settings after fixing something that caused a factor of 4
		BloomSizeScale = 4.0;
		Bloom1Size = 1.0f;
		Bloom2Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom2Size = 4.0f;
		Bloom3Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom3Size = 16.0f;
		Bloom4Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom4Size = 32.0f;
		Bloom5Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom5Size = 64.0f;
		Bloom6Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom6Size = 64.0f;
		BloomDirtMaskIntensity = 1.0f;
		BloomDirtMaskTint = FLinearColor(0.5f, 0.5f, 0.5f);
		AmbientCubemapIntensity = 1.0f;
		AmbientCubemapTint = FLinearColor(1, 1, 1);
		LPVIntensity = 1.0f;
		LPVSize = 5312.0f;
		LPVSecondaryOcclusionIntensity = 0.0f;
		LPVSecondaryBounceIntensity = 0.0f;
		LPVVplInjectionBias = 0.64f;
		LPVGeometryVolumeBias = 0.384f;
		LPVEmissiveInjectionIntensity = 1.0f;
		// next value might get overwritten by r.DefaultFeature.AutoExposure.Method
		AutoExposureMethod = AEM_Histogram;
		AutoExposureLowPercent = 80.0f;
		AutoExposureHighPercent = 98.3f;
		// next value might get overwritten by r.DefaultFeature.AutoExposure
		AutoExposureMinBrightness = 0.03f;
		// next value might get overwritten by r.DefaultFeature.AutoExposure
		AutoExposureMaxBrightness = 2.0f;
		AutoExposureBias = 0.0f;
		AutoExposureSpeedUp = 3.0f;
		AutoExposureSpeedDown = 1.0f;
		LPVDirectionalOcclusionIntensity = 0.0f;
		LPVDirectionalOcclusionRadius = 8.0f;
		LPVDiffuseOcclusionExponent = 1.0f;
		LPVSpecularOcclusionExponent = 7.0f;
		LPVDiffuseOcclusionIntensity = 1.0f;
		LPVSpecularOcclusionIntensity = 1.0f;
		HistogramLogMin = -8.0f;
		HistogramLogMax = 4.0f;
		// next value might get overwritten by r.DefaultFeature.LensFlare
		LensFlareIntensity = 1.0f;
		LensFlareTint = FLinearColor(1.0f, 1.0f, 1.0f);
		LensFlareBokehSize = 3.0f;
		LensFlareThreshold = 8.0f;
		VignetteIntensity = 0.4f;
		GrainIntensity = 0.0f;
		GrainJitter = 0.0f;
		// next value might get overwritten by r.DefaultFeature.AmbientOcclusion
		AmbientOcclusionIntensity = .5f;
		// next value might get overwritten by r.DefaultFeature.AmbientOcclusionStaticFraction
		AmbientOcclusionStaticFraction = 1.0f;
		AmbientOcclusionRadius = 200.0f;
		AmbientOcclusionDistance_DEPRECATED = 80.0f;
		AmbientOcclusionFadeDistance = 8000.0f;
		AmbientOcclusionFadeRadius = 5000.0f;
		AmbientOcclusionPower = 2.0f;
		AmbientOcclusionBias = 3.0f;
		AmbientOcclusionQuality = 50.0f;
		AmbientOcclusionMipBlend = 0.6f;
		AmbientOcclusionMipScale = 1.7f;
		AmbientOcclusionMipThreshold = 0.01f;
		AmbientOcclusionRadiusInWS = false;
		IndirectLightingColor = FLinearColor(1.0f, 1.0f, 1.0f);
		IndirectLightingIntensity = 1.0f;
		ColorGradingIntensity = 1.0f;
		DepthOfFieldFocalDistance = 1000.0f;
		DepthOfFieldFstop = 4.0f;
		DepthOfFieldSensorWidth = 24.576f;			// APS-C
		DepthOfFieldDepthBlurAmount = 1.0f;
		DepthOfFieldDepthBlurRadius = 0.0f;
		DepthOfFieldFocalRegion = 0.0f;
		DepthOfFieldNearTransitionRegion = 300.0f;
		DepthOfFieldFarTransitionRegion = 500.0f;
		DepthOfFieldScale = 0.0f;
		DepthOfFieldMaxBokehSize = 15.0f;
		DepthOfFieldNearBlurSize = 15.0f;
		DepthOfFieldFarBlurSize = 15.0f;
		DepthOfFieldOcclusion = 0.4f;
		DepthOfFieldColorThreshold = 1.0f;
		DepthOfFieldSizeThreshold = 0.08f;
		DepthOfFieldSkyFocusDistance = 0.0f;
		// 200 should be enough even for extreme aspect ratios to give the default no effect
		DepthOfFieldVignetteSize = 200.0f;
		LensFlareTints[0] = FLinearColor(1.0f, 0.8f, 0.4f, 0.6f);
		LensFlareTints[1] = FLinearColor(1.0f, 1.0f, 0.6f, 0.53f);
		LensFlareTints[2] = FLinearColor(0.8f, 0.8f, 1.0f, 0.46f);
		LensFlareTints[3] = FLinearColor(0.5f, 1.0f, 0.4f, 0.39f);
		LensFlareTints[4] = FLinearColor(0.5f, 0.8f, 1.0f, 0.31f);
		LensFlareTints[5] = FLinearColor(0.9f, 1.0f, 0.8f, 0.27f);
		LensFlareTints[6] = FLinearColor(1.0f, 0.8f, 0.4f, 0.22f);
		LensFlareTints[7] = FLinearColor(0.9f, 0.7f, 0.7f, 0.15f);
		// next value might get overwritten by r.DefaultFeature.MotionBlur
		MotionBlurAmount = 0.5f;
		MotionBlurMax = 5.0f;
		MotionBlurPerObjectSize = 0.5f;
		ScreenPercentage = 100.0f;
		ScreenSpaceReflectionIntensity = 100.0f;
		ScreenSpaceReflectionQuality = 50.0f;
		ScreenSpaceReflectionMaxRoughness = 0.6f;
		bMobileHQGaussian = false;
	}

	/**
		* Used to define the values before any override happens.
		* Should be as neutral as possible.
		*/		
	void SetBaseValues()
	{
		*this = FPostProcessSettings();

		AmbientCubemapIntensity = 0.0f;
		ColorGradingIntensity = 0.0f;
	}
};

UCLASS()
class UScene : public UObject
{
	GENERATED_UCLASS_BODY()


	/** bits needed to store DPG value */
	#define SDPG_NumBits	3
};