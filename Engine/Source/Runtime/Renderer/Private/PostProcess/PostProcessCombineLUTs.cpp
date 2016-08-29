// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessCombineLUTs.cpp: Post processing tone mapping implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessCombineLUTs.h"
#include "PostProcessing.h"
#include "ScreenRendering.h"
#include "SceneUtils.h"


// CVars
static TAutoConsoleVariable<float> CVarColorMin(
	TEXT("r.Color.Min"),
	0.0f,
	TEXT("Allows to define where the value 0 in the color channels is mapped to after color grading.\n")
	TEXT("The value should be around 0, positive: a gray scale is added to the darks, negative: more dark values become black, Default: 0"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarColorMid(
	TEXT("r.Color.Mid"),
	0.5f,
	TEXT("Allows to define where the value 0.5 in the color channels is mapped to after color grading (This is similar to a gamma correction).\n")
	TEXT("Value should be around 0.5, smaller values darken the mid tones, larger values brighten the mid tones, Default: 0.5"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarColorMax(
	TEXT("r.Color.Max"),
	1.0f,
	TEXT("Allows to define where the value 1.0 in the color channels is mapped to after color grading.\n")
	TEXT("Value should be around 1, smaller values darken the highlights, larger values move more colors towards white, Default: 1"),
	ECVF_RenderThreadSafe);

int32 GLUTSize = 32;
static FAutoConsoleVariableRef CVarLUTSize(
	TEXT("r.LUT.Size"),
	GLUTSize,
	TEXT("Size of film LUT"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTonemapperFilm(
	TEXT("r.TonemapperFilm"),
	0,
	TEXT("Use new film tone mapper"),
	ECVF_RenderThreadSafe
	);

// false:use 256x16 texture / true:use volume texture (faster, requires geometry shader)
// USE_VOLUME_LUT: needs to be the same for C++ and HLSL
bool UseVolumeTextureLUT(EShaderPlatform Platform) 
{
	// @todo Mac OS X: in order to share precompiled shaders between GL 3.3 & GL 4.1 devices we mustn't use volume-texture rendering as it isn't universally supported.
	return (IsFeatureLevelSupported(Platform,ERHIFeatureLevel::SM4) && GSupportsVolumeTextureRendering && Platform != EShaderPlatform::SP_OPENGL_SM4_MAC && RHISupportsGeometryShaders(Platform));
}

// including the neutral one at index 0
const uint32 GMaxLUTBlendCount = 5;


struct FColorTransform
{
	FColorTransform()
	{
		Reset();
	}
	
	float			MinValue;
	float			MidValue;
	float			MaxValue;

	void Reset()
	{
		MinValue = 0.0f;
		MidValue = 0.5f;
		MaxValue = 1.0f;
	}
};

/*-----------------------------------------------------------------------------
FColorRemapShaderParameters
-----------------------------------------------------------------------------*/

FColorRemapShaderParameters::FColorRemapShaderParameters(const FShaderParameterMap& ParameterMap)
{
	MappingPolynomial.Bind(ParameterMap, TEXT("MappingPolynomial"));
}

void FColorRemapShaderParameters::Set(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef ShaderRHI)
{
	FColorTransform ColorTransform;
	ColorTransform.MinValue = FMath::Clamp(CVarColorMin.GetValueOnRenderThread(), -10.0f, 10.0f);
	ColorTransform.MidValue = FMath::Clamp(CVarColorMid.GetValueOnRenderThread(), -10.0f, 10.0f);
	ColorTransform.MaxValue = FMath::Clamp(CVarColorMax.GetValueOnRenderThread(), -10.0f, 10.0f);
	
	{
		// x is the input value, y the output value
		// RGB = a, b, c where y = a * x*x + b * x + c

		float c = ColorTransform.MinValue;
		float b = 4 * ColorTransform.MidValue - 3 * ColorTransform.MinValue - ColorTransform.MaxValue;
		float a = ColorTransform.MaxValue - ColorTransform.MinValue - b;

		SetShaderValue(RHICmdList, ShaderRHI, MappingPolynomial, FVector(a, b, c));
	}
}

FArchive& operator<<(FArchive& Ar, FColorRemapShaderParameters& P)
{
	Ar << P.MappingPolynomial;
	return Ar;
}

/**
* A pixel shader for blending multiple LUT to one
*
* @param BlendCount >0
*/
template<uint32 BlendCount>
class FLUTBlenderPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLUTBlenderPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FLUTBlenderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer) 
		, ColorRemapShaderParameters(Initializer.ParameterMap)
	{
		// Suppress static code analysis warnings about a potentially ill-defined loop. BlendCount > 0 is valid.
		CA_SUPPRESS(6294)

		// starts as 1 as 0 is the neutral one
		for(uint32 i = 1; i < BlendCount; ++i)
		{
			FString Name = FString::Printf(TEXT("Texture%d"), i);

			TextureParameter[i].Bind(Initializer.ParameterMap, *Name);
			TextureParameterSampler[i].Bind(Initializer.ParameterMap, *(Name + TEXT("Sampler")));
		}
		WeightsParameter.Bind(Initializer.ParameterMap, TEXT("LUTWeights"));
		ColorScale.Bind(Initializer.ParameterMap,TEXT("ColorScale"));
		OverlayColor.Bind(Initializer.ParameterMap,TEXT("OverlayColor"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));

		WhiteTemp.Bind( Initializer.ParameterMap,TEXT("WhiteTemp") );
		WhiteTint.Bind( Initializer.ParameterMap,TEXT("WhiteTint") );

		ColorSaturation.Bind(	Initializer.ParameterMap,TEXT("ColorSaturation") );
		ColorContrast.Bind(		Initializer.ParameterMap,TEXT("ColorContrast") );
		ColorGamma.Bind(		Initializer.ParameterMap,TEXT("ColorGamma") );
		ColorGain.Bind(			Initializer.ParameterMap,TEXT("ColorGain") );
		ColorOffset.Bind(		Initializer.ParameterMap,TEXT("ColorOffset") );

		ColorSaturationShadows.Bind(Initializer.ParameterMap, TEXT("ColorSaturationShadows"));
		ColorContrastShadows.Bind(Initializer.ParameterMap, TEXT("ColorContrastShadows"));
		ColorGammaShadows.Bind(Initializer.ParameterMap, TEXT("ColorGammaShadows"));
		ColorGainShadows.Bind(Initializer.ParameterMap, TEXT("ColorGainShadows"));
		ColorOffsetShadows.Bind(Initializer.ParameterMap, TEXT("ColorOffsetShadows"));

		ColorSaturationMidtones.Bind(Initializer.ParameterMap, TEXT("ColorSaturationMidtones"));
		ColorContrastMidtones.Bind(Initializer.ParameterMap, TEXT("ColorContrastMidtones"));
		ColorGammaMidtones.Bind(Initializer.ParameterMap, TEXT("ColorGammaMidtones"));
		ColorGainMidtones.Bind(Initializer.ParameterMap, TEXT("ColorGainMidtones"));
		ColorOffsetMidtones.Bind(Initializer.ParameterMap, TEXT("ColorOffsetMidtones"));

		ColorSaturationHighlights.Bind(Initializer.ParameterMap, TEXT("ColorSaturationHighlights"));
		ColorContrastHighlights.Bind(Initializer.ParameterMap, TEXT("ColorContrastHighlights"));
		ColorGammaHighlights.Bind(Initializer.ParameterMap, TEXT("ColorGammaHighlights"));
		ColorGainHighlights.Bind(Initializer.ParameterMap, TEXT("ColorGainHighlights"));
		ColorOffsetHighlights.Bind(Initializer.ParameterMap, TEXT("ColorOffsetHighlights"));

		FilmSlope.Bind(		Initializer.ParameterMap,TEXT("FilmSlope") );
		FilmToe.Bind(		Initializer.ParameterMap,TEXT("FilmToe") );
		FilmShoulder.Bind(	Initializer.ParameterMap,TEXT("FilmShoulder") );
		FilmBlackClip.Bind(	Initializer.ParameterMap,TEXT("FilmBlackClip") );
		FilmWhiteClip.Bind(	Initializer.ParameterMap,TEXT("FilmWhiteClip") );

		OutputDevice.Bind(Initializer.ParameterMap, TEXT("OutputDevice"));
		OutputGamut.Bind(Initializer.ParameterMap, TEXT("OutputGamut"));
		ACESInversion.Bind(Initializer.ParameterMap, TEXT("ACESInversion"));

		ColorMatrixR_ColorCurveCd1.Bind(Initializer.ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(Initializer.ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(Initializer.ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint2"));
	}
	FLUTBlenderPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FTexture* Texture[BlendCount], float Weights[BlendCount])
	{
		const FPostProcessSettings& Settings = View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(View.Family);

		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		for(uint32 i = 0; i < BlendCount; ++i)
		{
			// we don't need to set the neutral one
			if(i != 0)
			{
				// don't use texture asset sampler as it might have anisotropic filtering enabled
				FSamplerStateRHIParamRef Sampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();

				SetTextureParameter(RHICmdList, ShaderRHI, TextureParameter[i], TextureParameterSampler[i], Sampler, Texture[i]->TextureRHI);
			}

			SetShaderValue(RHICmdList, ShaderRHI, WeightsParameter, Weights[i], i);
		}

		SetShaderValue(RHICmdList, ShaderRHI, ColorScale, View.ColorScale);
		SetShaderValue(RHICmdList, ShaderRHI, OverlayColor, View.OverlayColor);
		ColorRemapShaderParameters.Set(RHICmdList, ShaderRHI);

		// White balance
		SetShaderValue( RHICmdList, ShaderRHI, WhiteTemp, Settings.WhiteTemp );
		SetShaderValue( RHICmdList, ShaderRHI, WhiteTint, Settings.WhiteTint );

		// Color grade
		SetShaderValue( RHICmdList, ShaderRHI, ColorSaturation,	Settings.ColorSaturation );
		SetShaderValue( RHICmdList, ShaderRHI, ColorContrast,	Settings.ColorContrast );
		SetShaderValue( RHICmdList, ShaderRHI, ColorGamma,		Settings.ColorGamma );
		SetShaderValue( RHICmdList, ShaderRHI, ColorGain,		Settings.ColorGain );
		SetShaderValue( RHICmdList, ShaderRHI, ColorOffset,		Settings.ColorOffset );

		SetShaderValue(RHICmdList, ShaderRHI, ColorSaturationShadows, Settings.ColorSaturationShadows);
		SetShaderValue(RHICmdList, ShaderRHI, ColorContrastShadows, Settings.ColorContrastShadows);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGammaShadows, Settings.ColorGammaShadows);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGainShadows, Settings.ColorGainShadows);
		SetShaderValue(RHICmdList, ShaderRHI, ColorOffsetShadows, Settings.ColorOffsetShadows);

		SetShaderValue(RHICmdList, ShaderRHI, ColorSaturationMidtones, Settings.ColorSaturationMidtones);
		SetShaderValue(RHICmdList, ShaderRHI, ColorContrastMidtones, Settings.ColorContrastMidtones);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGammaMidtones, Settings.ColorGammaMidtones);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGainMidtones, Settings.ColorGainMidtones);
		SetShaderValue(RHICmdList, ShaderRHI, ColorOffsetMidtones, Settings.ColorOffsetMidtones);

		SetShaderValue(RHICmdList, ShaderRHI, ColorSaturationHighlights, Settings.ColorSaturationHighlights);
		SetShaderValue(RHICmdList, ShaderRHI, ColorContrastHighlights, Settings.ColorContrastHighlights);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGammaHighlights, Settings.ColorGammaHighlights);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGainHighlights, Settings.ColorGainHighlights);
		SetShaderValue(RHICmdList, ShaderRHI, ColorOffsetHighlights, Settings.ColorOffsetHighlights);

		// Film
		SetShaderValue( RHICmdList, ShaderRHI, FilmSlope,		Settings.FilmSlope );
		SetShaderValue( RHICmdList, ShaderRHI, FilmToe,			Settings.FilmToe );
		SetShaderValue( RHICmdList, ShaderRHI, FilmShoulder,	Settings.FilmShoulder );
		SetShaderValue( RHICmdList, ShaderRHI, FilmBlackClip,	Settings.FilmBlackClip );
		SetShaderValue( RHICmdList, ShaderRHI, FilmWhiteClip,	Settings.FilmWhiteClip );

		{
			static TConsoleVariableData<int32>* CVar709 = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Tonemapper709"));
			static TConsoleVariableData<float>* CVarGamma = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.TonemapperGamma"));
			static TConsoleVariableData<int32>* CVar2084 = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Tonemapper2084"));

			int32 Rec709 = CVar709->GetValueOnRenderThread();
			int32 ST2084 = CVar2084->GetValueOnRenderThread();
			float Gamma = CVarGamma->GetValueOnRenderThread();

			if (PLATFORM_APPLE && Gamma == 0.0f)
			{
				Gamma = 2.2f;
			}

			int32 Value = 0;						// sRGB
			Value = Rec709 ? 1 : Value;	// Rec709
			Value = Gamma != 0.0f ? 2 : Value;	// Explicit gamma
			// ST-2084 (Dolby PQ) options 
			// 1 = ACES
			// 2 = Vanilla PQ for 200 nit input
			// 3 = Unreal FilmToneMap + Inverted ACES + PQ
			Value = ST2084 >= 1 ? ST2084 + 2 : Value;

			SetShaderValue(RHICmdList, ShaderRHI, OutputDevice, Value);

			static TConsoleVariableData<int32>* CVarOutputGamut = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.TonemapperOutputGamut"));
			int32 OutputGamutValue = CVarOutputGamut->GetValueOnRenderThread();
			SetShaderValue(RHICmdList, ShaderRHI, OutputGamut, OutputGamutValue);

			// The approach to use when applying the inverse ACES Output Transform
			static TConsoleVariableData<int32>* CVarACESInversion = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.TonemapperACESInversion"));
			int32 ACESInversionValue = CVarACESInversion->GetValueOnRenderThread();
			SetShaderValue(RHICmdList, ShaderRHI, ACESInversion, ACESInversionValue);

			FVector InvDisplayGammaValue;
			InvDisplayGammaValue.X = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Y = 2.2f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Z = 1.0f / FMath::Max(Gamma, 1.0f);
			SetShaderValue(RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			// Legacy tone mapper
			// TODO remove

			// Must insure inputs are in correct range (else possible generation of NaNs).
			float InExposure = 1.0f;
			FVector InWhitePoint(Settings.FilmWhitePoint);
			float InSaturation = FMath::Clamp(Settings.FilmSaturation, 0.0f, 2.0f);
			FVector InLuma = FVector(1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f);
			FVector InMatrixR(Settings.FilmChannelMixerRed);
			FVector InMatrixG(Settings.FilmChannelMixerGreen);
			FVector InMatrixB(Settings.FilmChannelMixerBlue);
			float InContrast = FMath::Clamp(Settings.FilmContrast, 0.0f, 1.0f) + 1.0f;
			float InDynamicRange = powf(2.0f, FMath::Clamp(Settings.FilmDynamicRange, 1.0f, 4.0f));
			float InToe = (1.0f - FMath::Clamp(Settings.FilmToeAmount, 0.0f, 1.0f)) * 0.18f;
			InToe = FMath::Clamp(InToe, 0.18f/8.0f, 0.18f * (15.0f/16.0f));
			float InHeal = 1.0f - (FMath::Max(1.0f/32.0f, 1.0f - FMath::Clamp(Settings.FilmHealAmount, 0.0f, 1.0f)) * (1.0f - 0.18f)); 
			FVector InShadowTint(Settings.FilmShadowTint);
			float InShadowTintBlend = FMath::Clamp(Settings.FilmShadowTintBlend, 0.0f, 1.0f) * 64.0f;

			// Shadow tint amount enables turning off shadow tinting.
			float InShadowTintAmount = FMath::Clamp(Settings.FilmShadowTintAmount, 0.0f, 1.0f);
			InShadowTint = InWhitePoint + (InShadowTint - InWhitePoint) * InShadowTintAmount;

			// Make sure channel mixer inputs sum to 1 (+ smart dealing with all zeros).
			InMatrixR.X += 1.0f / (256.0f*256.0f*32.0f);
			InMatrixG.Y += 1.0f / (256.0f*256.0f*32.0f);
			InMatrixB.Z += 1.0f / (256.0f*256.0f*32.0f);
			InMatrixR *= 1.0f / FVector::DotProduct(InMatrixR, FVector(1.0f));
			InMatrixG *= 1.0f / FVector::DotProduct(InMatrixG, FVector(1.0f));
			InMatrixB *= 1.0f / FVector::DotProduct(InMatrixB, FVector(1.0f));

			// Conversion from linear rgb to luma (using HDTV coef).
			FVector LumaWeights = FVector(0.2126f, 0.7152f, 0.0722f);

			// Make sure white point has 1.0 as luma (so adjusting white point doesn't change exposure).
			// Make sure {0.0,0.0,0.0} inputs do something sane (default to white).
			InWhitePoint += FVector(1.0f / (256.0f*256.0f*32.0f));
			InWhitePoint *= 1.0f / FVector::DotProduct(InWhitePoint, LumaWeights);
			InShadowTint += FVector(1.0f / (256.0f*256.0f*32.0f));
			InShadowTint *= 1.0f / FVector::DotProduct(InShadowTint, LumaWeights);

			// Grey after color matrix is applied.
			FVector ColorMatrixLuma = FVector(
			FVector::DotProduct(InLuma.X * FVector(InMatrixR.X, InMatrixG.X, InMatrixB.X), FVector(1.0f)),
			FVector::DotProduct(InLuma.Y * FVector(InMatrixR.Y, InMatrixG.Y, InMatrixB.Y), FVector(1.0f)),
			FVector::DotProduct(InLuma.Z * FVector(InMatrixR.Z, InMatrixG.Z, InMatrixB.Z), FVector(1.0f)));

			FVector OutMatrixR = FVector(0.0f);
			FVector OutMatrixG = FVector(0.0f);
			FVector OutMatrixB = FVector(0.0f);
			FVector OutColorShadow_Luma = LumaWeights * InShadowTintBlend;
			FVector OutColorShadow_Tint1 = InWhitePoint;
			FVector OutColorShadow_Tint2 = InShadowTint - InWhitePoint;

			// Final color matrix effected by saturation and exposure.
			OutMatrixR = (ColorMatrixLuma + ((InMatrixR - ColorMatrixLuma) * InSaturation)) * InExposure;
			OutMatrixG = (ColorMatrixLuma + ((InMatrixG - ColorMatrixLuma) * InSaturation)) * InExposure;
			OutMatrixB = (ColorMatrixLuma + ((InMatrixB - ColorMatrixLuma) * InSaturation)) * InExposure;

			// Line for linear section.
			float FilmLineOffset = 0.18f - 0.18f*InContrast;
			float FilmXAtY0 = -FilmLineOffset/InContrast;
			float FilmXAtY1 = (1.0f - FilmLineOffset) / InContrast;
			float FilmXS = FilmXAtY1 - FilmXAtY0;

			// Coordinates of linear section.
			float FilmHiX = FilmXAtY0 + InHeal*FilmXS;
			float FilmHiY = FilmHiX*InContrast + FilmLineOffset;
			float FilmLoX = FilmXAtY0 + InToe*FilmXS;
			float FilmLoY = FilmLoX*InContrast + FilmLineOffset;
			// Supported exposure range before clipping.
			float FilmHeal = InDynamicRange - FilmHiX;
			// Intermediates.
			float FilmMidXS = FilmHiX - FilmLoX;
			float FilmMidYS = FilmHiY - FilmLoY;
			float FilmSlopeS = FilmMidYS / (FilmMidXS);
			float FilmHiYS = 1.0f - FilmHiY;
			float FilmLoYS = FilmLoY;
			float FilmToeVal = FilmLoX;
			float FilmHiG = (-FilmHiYS + (FilmSlopeS*FilmHeal)) / (FilmSlopeS*FilmHeal);
			float FilmLoG = (-FilmLoYS + (FilmSlopeS*FilmToeVal)) / (FilmSlopeS*FilmToeVal);

			// Constants.
			float OutColorCurveCh1 = FilmHiYS/FilmHiG;
			float OutColorCurveCh2 = -FilmHiX*(FilmHiYS/FilmHiG);
			float OutColorCurveCh3 = FilmHiYS/(FilmSlopeS*FilmHiG) - FilmHiX;
			float OutColorCurveCh0Cm1 = FilmHiX;
			float OutColorCurveCm2 = FilmSlopeS;
			float OutColorCurveCm0Cd0 = FilmLoX;
			float OutColorCurveCd3Cm3 = FilmLoY - FilmLoX*FilmSlopeS;
			float OutColorCurveCd1 = 0.0f;
			float OutColorCurveCd2 = 1.0f;
			// Handle these separate in case of FilmLoG being 0.
			if(FilmLoG != 0.0f)
			{
				OutColorCurveCd1 = -FilmLoYS/FilmLoG;
				OutColorCurveCd2 = FilmLoYS/(FilmSlopeS*FilmLoG);
			}
			else
			{
				// FilmLoG being zero means dark region is a linear segment (so just continue the middle section).
				OutColorCurveCm0Cd0 = 0.0f;
				OutColorCurveCd3Cm3 = 0.0f;
			}

			FVector4 Constants[8];
			Constants[0] = FVector4(OutMatrixR, OutColorCurveCd1);
			Constants[1] = FVector4(OutMatrixG, OutColorCurveCd3Cm3);
			Constants[2] = FVector4(OutMatrixB, OutColorCurveCm2); 
			Constants[3] = FVector4(OutColorCurveCm0Cd0, OutColorCurveCd2, OutColorCurveCh0Cm1, OutColorCurveCh3); 
			Constants[4] = FVector4(OutColorCurveCh1, OutColorCurveCh2, 0.0f, 0.0f);
			Constants[5] = FVector4(OutColorShadow_Luma, 0.0f);
			Constants[6] = FVector4(OutColorShadow_Tint1, 0.0f);
			Constants[7] = FVector4(OutColorShadow_Tint2, CVarTonemapperFilm.GetValueOnRenderThread());

			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixR_ColorCurveCd1, Constants[0]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixG_ColorCurveCd3Cm3, Constants[1]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixB_ColorCurveCm2, Constants[2]); 
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3, Constants[3]); 
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Ch1_Ch2, Constants[4]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Luma, Constants[5]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint1, Constants[6]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint2, Constants[7]);
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("BLENDCOUNT"), BlendCount);
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeTextureLUT(Platform));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		for(uint32 i = 0; i < BlendCount; ++i)
		{
			Ar << TextureParameter[i];
			Ar << TextureParameterSampler[i];
		}
		Ar << WeightsParameter << ColorScale << OverlayColor;
		Ar << ColorRemapShaderParameters;
		Ar << InverseGamma;

		Ar << WhiteTemp;
		Ar << WhiteTint;

		Ar << ColorSaturation;
		Ar << ColorContrast;
		Ar << ColorGamma;
		Ar << ColorGain;
		Ar << ColorOffset;

		Ar << ColorSaturationShadows;
		Ar << ColorContrastShadows;
		Ar << ColorGammaShadows;
		Ar << ColorGainShadows;
		Ar << ColorOffsetShadows;

		Ar << ColorSaturationMidtones;
		Ar << ColorContrastMidtones;
		Ar << ColorGammaMidtones;
		Ar << ColorGainMidtones;
		Ar << ColorOffsetMidtones;

		Ar << ColorSaturationHighlights;
		Ar << ColorContrastHighlights;
		Ar << ColorGammaHighlights;
		Ar << ColorGainHighlights;
		Ar << ColorOffsetHighlights;

		Ar << OutputDevice;
		Ar << OutputGamut;
		Ar << ACESInversion;

		Ar << FilmSlope;
		Ar << FilmToe;
		Ar << FilmShoulder;
		Ar << FilmBlackClip;
		Ar << FilmWhiteClip;

		Ar	<< ColorMatrixR_ColorCurveCd1 << ColorMatrixG_ColorCurveCd3Cm3 << ColorMatrixB_ColorCurveCm2
			<< ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 << ColorCurve_Ch1_Ch2 << ColorShadow_Luma << ColorShadow_Tint1 << ColorShadow_Tint2;

		return bShaderHasOutdatedParameters;
	}

private: // ---------------------------------------------------

	// [0] is not used as it's the neutral one we do in the shader
	FShaderResourceParameter TextureParameter[GMaxLUTBlendCount];
	FShaderResourceParameter TextureParameterSampler[GMaxLUTBlendCount];
	FShaderParameter WeightsParameter;
	FShaderParameter ColorScale;
	FShaderParameter OverlayColor;
	FShaderParameter InverseGamma;
	FColorRemapShaderParameters ColorRemapShaderParameters;

	FShaderParameter WhiteTemp;
	FShaderParameter WhiteTint;

	FShaderParameter ColorSaturation;
	FShaderParameter ColorContrast;
	FShaderParameter ColorGamma;
	FShaderParameter ColorGain;
	FShaderParameter ColorOffset;

	FShaderParameter ColorSaturationShadows;
	FShaderParameter ColorContrastShadows;
	FShaderParameter ColorGammaShadows;
	FShaderParameter ColorGainShadows;
	FShaderParameter ColorOffsetShadows;

	FShaderParameter ColorSaturationMidtones;
	FShaderParameter ColorContrastMidtones;
	FShaderParameter ColorGammaMidtones;
	FShaderParameter ColorGainMidtones;
	FShaderParameter ColorOffsetMidtones;

	FShaderParameter ColorSaturationHighlights;
	FShaderParameter ColorContrastHighlights;
	FShaderParameter ColorGammaHighlights;
	FShaderParameter ColorGainHighlights;
	FShaderParameter ColorOffsetHighlights;

	FShaderParameter FilmSlope;
	FShaderParameter FilmToe;
	FShaderParameter FilmShoulder;
	FShaderParameter FilmBlackClip;
	FShaderParameter FilmWhiteClip;

	FShaderParameter OutputDevice;
	FShaderParameter OutputGamut;
	FShaderParameter ACESInversion;

	// Legacy
	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;
};


IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<1>,TEXT("PostProcessCombineLUTs"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<2>,TEXT("PostProcessCombineLUTs"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<3>,TEXT("PostProcessCombineLUTs"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<4>,TEXT("PostProcessCombineLUTs"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<5>,TEXT("PostProcessCombineLUTs"),TEXT("MainPS"),SF_Pixel);

void SetLUTBlenderShader(FRenderingCompositePassContext& Context, uint32 BlendCount, FTexture* Texture[], float Weights[], const FVolumeBounds& VolumeBounds)
{
	check(BlendCount > 0);

	FShader* LocalPixelShader = 0;
	FGlobalBoundShaderState* LocalBoundShaderState = 0;
	const FSceneView& View = Context.View;

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	// A macro to handle setting the filter shader for a specific number of samples.
#define CASE_COUNT(BlendCount) \
	case BlendCount: \
	{ \
		TShaderMapRef<FLUTBlenderPS<BlendCount> > PixelShader(ShaderMap); \
		static FGlobalBoundShaderState BoundShaderState; \
		LocalBoundShaderState = &BoundShaderState;\
		LocalPixelShader = *PixelShader;\
	}; \
	break;

	switch(BlendCount)
	{
		// starts at 1 as we always have at least the neutral one
		CASE_COUNT(1);
		CASE_COUNT(2);
		CASE_COUNT(3);
		CASE_COUNT(4);
		CASE_COUNT(5);
		//	default:
		//		UE_LOG(LogRenderer, Fatal,TEXT("Invalid number of samples: %u"),BlendCount);
	}
#undef CASE_COUNT
	check(LocalBoundShaderState != NULL);
	if(UseVolumeTextureLUT(Context.View.GetShaderPlatform()))
	{
		TShaderMapRef<FWriteToSliceVS> VertexShader(ShaderMap);
		TShaderMapRef<FWriteToSliceGS> GeometryShader(ShaderMap);

		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, *LocalBoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *VertexShader, LocalPixelShader, *GeometryShader);

		VertexShader->SetParameters(Context.RHICmdList, VolumeBounds, VolumeBounds.MaxX - VolumeBounds.MinX);
		GeometryShader->SetParameters(Context.RHICmdList, VolumeBounds);
	}
	else
	{
		TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, *LocalBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, LocalPixelShader);

		VertexShader->SetParameters(Context);
	}
#define CASE_COUNT(BlendCount) \
	case BlendCount: \
	{ \
	TShaderMapRef<FLUTBlenderPS<BlendCount> > PixelShader(ShaderMap); \
	PixelShader->SetParameters(Context.RHICmdList, View, Texture, Weights); \
	}; \
	break;

	switch(BlendCount)
	{
		// starts at 1 as we always have at least the neutral one
		CASE_COUNT(1);
		CASE_COUNT(2);
		CASE_COUNT(3);
		CASE_COUNT(4);
		CASE_COUNT(5);
		//	default:
		//		UE_LOG(LogRenderer, Fatal,TEXT("Invalid number of samples: %u"),BlendCount);
	}
#undef CASE_COUNT
}


uint32 FRCPassPostProcessCombineLUTs::FindIndex(const FFinalPostProcessSettings& Settings, UTexture* Tex) const
{
	for(uint32 i = 0; i < (uint32)Settings.ContributingLUTs.Num(); ++i)
	{
		if(Settings.ContributingLUTs[i].LUTTexture == Tex)
		{
			return i;
		}
	}

	return 0xffffffff;
}

uint32 FRCPassPostProcessCombineLUTs::GenerateFinalTable(const FFinalPostProcessSettings& Settings, FTexture* OutTextures[], float OutWeights[], uint32 MaxCount) const
{
	// find the n strongest contributors, drop small contributors
	// (inefficient implementation for many items but count should be small)

	uint32 LocalCount = 1;

	// add the neutral one (done in the shader) as it should be the first and always there
	OutTextures[0] = 0;
	{
		uint32 NeutralIndex = FindIndex(Settings, 0);

		OutWeights[0] = NeutralIndex == 0xffffffff ? 0.0f : Settings.ContributingLUTs[NeutralIndex].Weight;
	}

	float OutWeightsSum = OutWeights[0];
	for(; LocalCount < MaxCount; ++LocalCount)
	{
		uint32 BestIndex = 0xffffffff;
		// find the one with the strongest weight, add until full
		for(uint32 i = 0; i < (uint32)Settings.ContributingLUTs.Num(); ++i)
		{
			bool AlreadyInArray = false;
			{
				UTexture* LUTTexture = Settings.ContributingLUTs[i].LUTTexture; 
				FTexture* Internal = LUTTexture ? LUTTexture->Resource : 0; 
				for(uint32 e = 0; e < LocalCount; ++e)
				{
					if(Internal == OutTextures[e])
					{
						AlreadyInArray = true;
						break;
					}
				}
			}

			if(AlreadyInArray)
			{
				// we already have this one
				continue;
			}

			if(BestIndex != 0xffffffff
				&& Settings.ContributingLUTs[BestIndex].Weight > Settings.ContributingLUTs[i].Weight)
			{
				// we have a better ones, maybe add next time
				continue;
			}

			BestIndex = i;
		}

		if(BestIndex == 0xffffffff)
		{
			// no more elements to process 
			break;
		}

		float BestWeight = Settings.ContributingLUTs[BestIndex].Weight;

		if(BestWeight < 1.0f / 512.0f)
		{
			// drop small contributor 
			break;
		}

		UTexture* BestLUTTexture = Settings.ContributingLUTs[BestIndex].LUTTexture; 
		FTexture* BestInternal = BestLUTTexture ? BestLUTTexture->Resource : 0; 

		OutTextures[LocalCount] = BestInternal;
		OutWeights[LocalCount] = BestWeight;
		OutWeightsSum += BestWeight;
	}

	// normalize
	if(OutWeightsSum > 0.001f)
	{
		float InvOutWeightsSum = 1.0f / OutWeightsSum;

		for(uint32 i = 0; i < LocalCount; ++i)
		{
			OutWeights[i] *= InvOutWeightsSum;
		}
	}
	else
	{
		// neutral only is fully utilized
		OutWeights[0] = 1.0f;
		LocalCount = 1;
	}

	return LocalCount;
}

void FRCPassPostProcessCombineLUTs::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessCombineLUTs, TEXT("PostProcessCombineLUTs %dx%dx%d"), GLUTSize, GLUTSize, GLUTSize);

	FTexture* LocalTextures[GMaxLUTBlendCount];
	float LocalWeights[GMaxLUTBlendCount];

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	uint32 LocalCount = 1;

	// set defaults for no LUT
	LocalTextures[0] = 0;
	LocalWeights[0] = 1.0f;

	if(ViewFamily.EngineShowFlags.ColorGrading)
	{
		LocalCount = GenerateFinalTable(Context.View.FinalPostProcessSettings, LocalTextures, LocalWeights, GMaxLUTBlendCount);
	}

	// for a 3D texture, the viewport is 16x16 (per slice), for a 2D texture, it's unwrapped to 256x16
	FIntPoint DestSize(UseVolumeTextureLUT(ShaderPlatform) ? GLUTSize : GLUTSize * GLUTSize, GLUTSize);

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EUninitializedColorAndDepth);
	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	const FVolumeBounds VolumeBounds(GLUTSize);

	SetLUTBlenderShader(Context, LocalCount, LocalTextures, LocalWeights, VolumeBounds);

	if (UseVolumeTextureLUT(ShaderPlatform))
	{
		// use volume texture 16x16x16
		RasterizeToVolumeTexture(Context.RHICmdList, VolumeBounds);
	}
	else
	{
		// use unwrapped 2d texture 256x16
		TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

		DrawRectangle( 
			Context.RHICmdList,
			0, 0,										// XY
			GLUTSize * GLUTSize, GLUTSize,				// SizeXY
			0, 0,										// UV
			GLUTSize * GLUTSize, GLUTSize,				// SizeUV
			FIntPoint(GLUTSize * GLUTSize, GLUTSize),	// TargetSize
			FIntPoint(GLUTSize * GLUTSize, GLUTSize),	// TextureSize
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessCombineLUTs::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	EPixelFormat LUTPixelFormat = PF_A2B10G10R10;
	if (!GPixelFormats[LUTPixelFormat].Supported)
	{
		LUTPixelFormat = PF_R8G8B8A8;
	}
	
	FPooledRenderTargetDesc Ret = FPooledRenderTargetDesc::Create2DDesc(FIntPoint(GLUTSize * GLUTSize, GLUTSize), LUTPixelFormat, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_ShaderResource, false);

	if(UseVolumeTextureLUT(ShaderPlatform))
	{
		Ret.Extent = FIntPoint(GLUTSize, GLUTSize);
		Ret.Depth = GLUTSize;
	}

	Ret.DebugName = TEXT("CombineLUTs");

	return Ret;
}
