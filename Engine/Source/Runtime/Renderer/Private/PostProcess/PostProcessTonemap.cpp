// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTonemap.cpp: Post processing tone mapping implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessTonemap.h"
#include "PostProcessing.h"
#include "PostProcessEyeAdaptation.h"
#include "PostProcessCombineLUTs.h"
#include "SceneUtils.h"


//
// TONEMAPPER PERMUTATION CONTROL
//

// Tonemapper option bitmask.
// Adjusting this requires adjusting TonemapperCostTab[].
typedef enum {
	TonemapperGammaOnly         = (1<<0),
	TonemapperColorMatrix       = (1<<1),
	TonemapperShadowTint        = (1<<2),
	TonemapperContrast          = (1<<3),
	TonemapperGrainJitter       = (1<<4),
	TonemapperGrainIntensity    = (1<<5),
	TonemapperGrainQuantization = (1<<6),
	TonemapperBloom             = (1<<7),
	TonemapperDOF               = (1<<8),
	TonemapperVignette          = (1<<9),
	TonemapperLightShafts       = (1<<10),
	TonemapperMosaic            = (1<<11),
	TonemapperColorFringe       = (1<<12),
	TonemapperMsaa              = (1<<13),
} TonemapperOption;

// Tonemapper option cost (0 = no cost, 255 = max cost).
// Suggested cost should be 
// These need a 1:1 mapping with TonemapperOption enum,
static uint8 TonemapperCostTab[] = { 
	1, //TonemapperGammaOnly
	1, //TonemapperColorMatrix
	1, //TonemapperShadowTint
	1, //TonemapperContrast
	1, //TonemapperGrainJitter
	1, //TonemapperGrainIntensity
	1, //TonemapperGrainQuantization
	1, //TonemapperBloom
	1, //TonemapperDOF
	1, //TonemapperVignette
	1, //TonemapperLightShafts
	1, //TonemapperMosaic
	1, //TonemapperColorFringe
	1, //TonemapperMsaa
};

// Edit the following to add and remove configurations.
// This is a white list of the combinations which are compiled.
// Place most common first (faster when searching in TonemapperFindLeastExpensive()).

// List of configurations compiled for PC.
static uint32 TonemapperConfBitmaskPC[7] = { 

	TonemapperBloom +
	TonemapperGrainJitter +
	TonemapperGrainIntensity +
	TonemapperGrainQuantization +
	TonemapperVignette +
	TonemapperColorFringe +
	0,

	TonemapperBloom + 
	TonemapperVignette +
	TonemapperGrainQuantization +
	TonemapperColorFringe +
	0,

	TonemapperBloom + 
	TonemapperGrainQuantization +
	0,

	// same without TonemapperGrainQuantization

	TonemapperBloom +
	TonemapperGrainJitter +
	TonemapperGrainIntensity +
	TonemapperVignette +
	TonemapperColorFringe +
	0,

	TonemapperBloom + 
	TonemapperVignette +
	TonemapperColorFringe +
	0,

	TonemapperBloom + 
	0,

	//

	TonemapperGammaOnly +
	0,
};

// List of configurations compiled for Mobile.
static uint32 TonemapperConfBitmaskMobile[39] = { 

	// 
	//  15 for NON-MOSAIC 
	//

	TonemapperGammaOnly +
	0,

	// Not supporting grain jitter or grain quantization on mobile.

	// Bloom, LightShafts, Vignette all off.
	TonemapperContrast +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	0,

	// Bloom, LightShafts, Vignette, and Vignette Color all use the same shader code in the tonemapper.
	TonemapperContrast +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	0,

	// DOF enabled.
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperDOF +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperDOF +
	0,

	// Same with grain.

	TonemapperContrast +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	// DOF enabled.
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperDOF +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperDOF +
	TonemapperGrainIntensity +
	0,


	//
	// 14 for MOSAIC PATH
	//

	// This is mosaic without film post.
	TonemapperMosaic +
	TonemapperGammaOnly +
	0,

	TonemapperMosaic + 
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperColorMatrix +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperVignette +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperVignette +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperVignette +
	0,

	// With grain

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperGrainIntensity +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperGrainIntensity +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperGrainIntensity +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperMosaic + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,


	// 
	//  10 for MSAA
	//

	TonemapperMsaa +
	TonemapperContrast +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperBloom +
	TonemapperVignette +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperVignette +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperVignette +
	0,

	// Same with grain.
	TonemapperMsaa +
	TonemapperContrast +
	TonemapperGrainIntensity +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperGrainIntensity +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperBloom +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

};

// Returns 1 if option is defined otherwise 0.
static uint32 TonemapperIsDefined(uint32 ConfigBitmask, TonemapperOption Option)
{
	return (ConfigBitmask & Option) ? 1 : 0;
}

// This finds the least expensive configuration which supports all selected options in bitmask.
static uint32 TonemapperFindLeastExpensive(uint32* RESTRICT Table, uint32 TableEntries, uint8* RESTRICT CostTable, uint32 RequiredOptionsBitmask) 
{
	// Custom logic to insure fail cases do not happen.
	uint32 MustNotHaveBitmask = 0;
	MustNotHaveBitmask += ((RequiredOptionsBitmask & TonemapperDOF) == 0) ? TonemapperDOF : 0;
	MustNotHaveBitmask += ((RequiredOptionsBitmask & TonemapperMosaic) == 0) ? TonemapperMosaic : 0;
	MustNotHaveBitmask += ((RequiredOptionsBitmask & TonemapperMsaa) == 0) ? TonemapperMsaa : 0;

	// Search for exact match first.
	uint32 Index;
	for(Index = 0; Index < TableEntries; ++Index) 
	{
		if(Table[Index] == RequiredOptionsBitmask) 
		{
			return Index;
		}
	}
	// Search through list for best entry.
	uint32 BestIndex = TableEntries;
	uint32 BestCost = ~0;
	uint32 NotRequiredOptionsBitmask = ~RequiredOptionsBitmask;
	for(Index = 0; Index < TableEntries; ++Index) 
	{
		uint32 Bitmask = Table[Index];
		if((Bitmask & MustNotHaveBitmask) != 0)
		{
			continue; 
		}
		if((Bitmask & RequiredOptionsBitmask) != RequiredOptionsBitmask) 
		{
			// A match requires a minimum set of bits set.
			continue;
		}
		uint32 BitExtra = Bitmask & NotRequiredOptionsBitmask;
		uint32 Cost = 0;
		while(BitExtra) 
		{
			uint32 Bit = FMath::FloorLog2(BitExtra);
			Cost += CostTable[Bit];
			if(Cost > BestCost)
			{
				// Poor match.
				goto PoorMatch;
			}
			BitExtra &= ~(1<<Bit);
		}
		// Better match.
		BestCost = Cost;
		BestIndex = Index;
	PoorMatch:
		;
	}
	// Fail returns 0, the gamma only shader.
	if(BestIndex == TableEntries) BestIndex = 0;
	return BestIndex;
}

// Common conversion of engine settings into a bitmask which describes the shader options required.
static uint32 TonemapperGenerateBitmask(const FViewInfo* RESTRICT View, bool bGammaOnly, bool bMobile)
{
	check(View);

	bGammaOnly |= !IsMobileHDR();

	const FSceneViewFamily* RESTRICT Family = View->Family;
	if(
		bGammaOnly ||
		(Family->EngineShowFlags.Tonemapper == 0) || 
		(Family->EngineShowFlags.PostProcessing == 0))
	{
		return TonemapperGammaOnly;
	}

	uint32 Bitmask = 0;

	const FPostProcessSettings* RESTRICT Settings = &(View->FinalPostProcessSettings);

	FVector MixerR(Settings->FilmChannelMixerRed);
	FVector MixerG(Settings->FilmChannelMixerGreen);
	FVector MixerB(Settings->FilmChannelMixerBlue);
	uint32 useColorMatrix = 0;
	if(
		(Settings->FilmSaturation != 1.0f) || 
		((MixerR - FVector(1.0f,0.0f,0.0f)).GetAbsMax() != 0.0f) || 
		((MixerG - FVector(0.0f,1.0f,0.0f)).GetAbsMax() != 0.0f) || 
		((MixerB - FVector(0.0f,0.0f,1.0f)).GetAbsMax() != 0.0f))
	{
		Bitmask += TonemapperColorMatrix;
	}

	FVector Tint(Settings->FilmWhitePoint);
	FVector TintShadow(Settings->FilmShadowTint);

	Bitmask += (Settings->FilmShadowTintAmount > 0.0f) ? TonemapperShadowTint     : 0;	
	Bitmask += (Settings->FilmContrast > 0.0f)         ? TonemapperContrast       : 0;
	Bitmask += (Settings->GrainIntensity > 0.0f)       ? TonemapperGrainIntensity : 0;
	Bitmask += (Settings->VignetteIntensity > 0.0f)    ? TonemapperVignette       : 0;

	return Bitmask;
}

// Common post.
// These are separated because mosiac mode doesn't support them.
static uint32 TonemapperGenerateBitmaskPost(const FViewInfo* RESTRICT View)
{
	const FPostProcessSettings* RESTRICT Settings = &(View->FinalPostProcessSettings);
	const FSceneViewFamily* RESTRICT Family = View->Family;

	uint32 Bitmask = (Settings->GrainJitter > 0.0f) ? TonemapperGrainJitter : 0;

	Bitmask += (Settings->BloomIntensity > 0.0f) ? TonemapperBloom : 0;

	return Bitmask;
}

// PC only.
static uint32 TonemapperGenerateBitmaskPC(const FViewInfo* RESTRICT View, bool bGammaOnly)
{
	uint32 Bitmask = TonemapperGenerateBitmask(View, bGammaOnly, false);

	// Must early exit if gamma only.
	if(Bitmask == TonemapperGammaOnly) 
	{
		return Bitmask;
	}

	{
		static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.TonemapperQuality"));
		int32 Value = CVar->GetValueOnRenderThread();

		if(Value > 0)
		{
			Bitmask |= TonemapperGrainQuantization;
		}
	}

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SceneColorFringeQuality")); 

	int32 FringeQuality = CVar->GetValueOnRenderThread();
	if(View->Family->EngineShowFlags.SceneColorFringe
		// Removed this from the camera imperfections toggle because this no longer takes an extra pass.
		// && Context->View.Family->EngineShowFlags.CameraImperfections
		&& View->FinalPostProcessSettings.SceneFringeIntensity > 0.01f
		&& FringeQuality > 0)
	{
		Bitmask |= TonemapperColorFringe;
	}

	return Bitmask + TonemapperGenerateBitmaskPost(View);
}

// Mobile only.
static uint32 TonemapperGenerateBitmaskMobile(const FViewInfo* RESTRICT View, bool bGammaOnly)
{
	check(View);

	uint32 Bitmask = TonemapperGenerateBitmask(View, bGammaOnly, true);

	bool bUseMosaic = IsMobileHDR32bpp();

	// Must early exit if gamma only.
	if(Bitmask == TonemapperGammaOnly)
	{
		return Bitmask + (bUseMosaic ? TonemapperMosaic : 0);
	}

	// Check if mosaic mode is on and exit if on.
	if(bUseMosaic)
	{
		return Bitmask + TonemapperMosaic;
	}

	static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
	if ((GShaderPlatformForFeatureLevel[View->GetFeatureLevel()] == SP_METAL) && (CVarMobileMSAA ? CVarMobileMSAA->GetValueOnAnyThread() > 1 : false))
	{
		Bitmask += TonemapperMsaa;
	}

	// Only add mobile post if FP16 is supported.
	if(GSupportsRenderTargetFormat_PF_FloatRGBA)
	{
		Bitmask += TonemapperGenerateBitmaskPost(View);
		Bitmask += (View->FinalPostProcessSettings.DepthOfFieldScale > 0.0f) ? TonemapperDOF         : 0;
		Bitmask += (View->bLightShaftUse)                                    ? TonemapperLightShafts : 0;

		// Mobile is not supporting grain quantization and grain jitter currently.
		Bitmask &= ~(TonemapperGrainQuantization | TonemapperGrainJitter);
	}
	return Bitmask;
}

void GrainPostSettings(FVector* RESTRICT const Constant, const FPostProcessSettings* RESTRICT const Settings)
{
	float GrainJitter = Settings->GrainJitter;
	float GrainIntensity = Settings->GrainIntensity;
	Constant->X = GrainIntensity;
	Constant->Y = 1.0f + (-0.5f * GrainIntensity);
	Constant->Z = GrainJitter;
}

// This code is shared by PostProcessTonemap and VisualizeHDR.
void FilmPostSetConstants(FVector4* RESTRICT const Constants, const uint32 ConfigBitmask, const FPostProcessSettings* RESTRICT const FinalPostProcessSettings, bool bMobile)
{
	uint32 UseColorMatrix = TonemapperIsDefined(ConfigBitmask, TonemapperColorMatrix);
	uint32 UseShadowTint  = TonemapperIsDefined(ConfigBitmask, TonemapperShadowTint);
	uint32 UseContrast    = TonemapperIsDefined(ConfigBitmask, TonemapperContrast);

	// Must insure inputs are in correct range (else possible generation of NaNs).
	float InExposure = 1.0f;
	FVector InWhitePoint(FinalPostProcessSettings->FilmWhitePoint);
	float InSaturation = FMath::Clamp(FinalPostProcessSettings->FilmSaturation, 0.0f, 2.0f);
	FVector InLuma = FVector(1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f);
	FVector InMatrixR(FinalPostProcessSettings->FilmChannelMixerRed);
	FVector InMatrixG(FinalPostProcessSettings->FilmChannelMixerGreen);
	FVector InMatrixB(FinalPostProcessSettings->FilmChannelMixerBlue);
	float InContrast = FMath::Clamp(FinalPostProcessSettings->FilmContrast, 0.0f, 1.0f) + 1.0f;
	float InDynamicRange = powf(2.0f, FMath::Clamp(FinalPostProcessSettings->FilmDynamicRange, 1.0f, 4.0f));
	float InToe = (1.0f - FMath::Clamp(FinalPostProcessSettings->FilmToeAmount, 0.0f, 1.0f)) * 0.18f;
	InToe = FMath::Clamp(InToe, 0.18f/8.0f, 0.18f * (15.0f/16.0f));
	float InHeal = 1.0f - (FMath::Max(1.0f/32.0f, 1.0f - FMath::Clamp(FinalPostProcessSettings->FilmHealAmount, 0.0f, 1.0f)) * (1.0f - 0.18f)); 
	FVector InShadowTint(FinalPostProcessSettings->FilmShadowTint);
	float InShadowTintBlend = FMath::Clamp(FinalPostProcessSettings->FilmShadowTintBlend, 0.0f, 1.0f) * 64.0f;

	// Shadow tint amount enables turning off shadow tinting.
	float InShadowTintAmount = FMath::Clamp(FinalPostProcessSettings->FilmShadowTintAmount, 0.0f, 1.0f);
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

	if(UseColorMatrix)
	{
		// Final color matrix effected by saturation and exposure.
		OutMatrixR = (ColorMatrixLuma + ((InMatrixR - ColorMatrixLuma) * InSaturation)) * InExposure;
		OutMatrixG = (ColorMatrixLuma + ((InMatrixG - ColorMatrixLuma) * InSaturation)) * InExposure;
		OutMatrixB = (ColorMatrixLuma + ((InMatrixB - ColorMatrixLuma) * InSaturation)) * InExposure;
		if(UseShadowTint == 0)
		{
			OutMatrixR = OutMatrixR * InWhitePoint.X;
			OutMatrixG = OutMatrixG * InWhitePoint.Y;
			OutMatrixB = OutMatrixB * InWhitePoint.Z;
		}
	}
	else
	{
		// No color matrix fast path.
		if(UseShadowTint == 0)
		{
			OutMatrixB = InExposure * InWhitePoint;
		}
		else
		{
			// Need to drop exposure in.
			OutColorShadow_Luma *= InExposure;
			OutColorShadow_Tint1 *= InExposure;
			OutColorShadow_Tint2 *= InExposure;
		}
	}

	// Curve constants.
	float OutColorCurveCh3;
	float OutColorCurveCh0Cm1;
	float OutColorCurveCd2;
	float OutColorCurveCm0Cd0;
	float OutColorCurveCh1;
	float OutColorCurveCh2;
	float OutColorCurveCd1;
	float OutColorCurveCd3Cm3;
	float OutColorCurveCm2;

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
	float FilmSlope = FilmMidYS / (FilmMidXS);
	float FilmHiYS = 1.0f - FilmHiY;
	float FilmLoYS = FilmLoY;
	float FilmToe = FilmLoX;
	float FilmHiG = (-FilmHiYS + (FilmSlope*FilmHeal)) / (FilmSlope*FilmHeal);
	float FilmLoG = (-FilmLoYS + (FilmSlope*FilmToe)) / (FilmSlope*FilmToe);

	if(UseContrast)
	{
		// Constants.
		OutColorCurveCh1 = FilmHiYS/FilmHiG;
		OutColorCurveCh2 = -FilmHiX*(FilmHiYS/FilmHiG);
		OutColorCurveCh3 = FilmHiYS/(FilmSlope*FilmHiG) - FilmHiX;
		OutColorCurveCh0Cm1 = FilmHiX;
		OutColorCurveCm2 = FilmSlope;
		OutColorCurveCm0Cd0 = FilmLoX;
		OutColorCurveCd3Cm3 = FilmLoY - FilmLoX*FilmSlope;
		// Handle these separate in case of FilmLoG being 0.
		if(FilmLoG != 0.0f)
		{
			OutColorCurveCd1 = -FilmLoYS/FilmLoG;
			OutColorCurveCd2 = FilmLoYS/(FilmSlope*FilmLoG);
		}
		else
		{
			// FilmLoG being zero means dark region is a linear segment (so just continue the middle section).
			OutColorCurveCd1 = 0.0f;
			OutColorCurveCd2 = 1.0f;
			OutColorCurveCm0Cd0 = 0.0f;
			OutColorCurveCd3Cm3 = 0.0f;
		}
	}
	else
	{
		// Simplified for no dark segment.
		OutColorCurveCh1 = FilmHiYS/FilmHiG;
		OutColorCurveCh2 = -FilmHiX*(FilmHiYS/FilmHiG);
		OutColorCurveCh3 = FilmHiYS/(FilmSlope*FilmHiG) - FilmHiX;
		OutColorCurveCh0Cm1 = FilmHiX;
		// Not used.
		OutColorCurveCm2 = 0.0f;
		OutColorCurveCm0Cd0 = 0.0f;
		OutColorCurveCd3Cm3 = 0.0f;
		OutColorCurveCd1 = 0.0f;
		OutColorCurveCd2 = 0.0f;
	}

	Constants[0] = FVector4(OutMatrixR, OutColorCurveCd1);
	Constants[1] = FVector4(OutMatrixG, OutColorCurveCd3Cm3);
	Constants[2] = FVector4(OutMatrixB, OutColorCurveCm2); 
	Constants[3] = FVector4(OutColorCurveCm0Cd0, OutColorCurveCd2, OutColorCurveCh0Cm1, OutColorCurveCh3); 
	Constants[4] = FVector4(OutColorCurveCh1, OutColorCurveCh2, 0.0f, 0.0f);
	Constants[5] = FVector4(OutColorShadow_Luma, 0.0f);
	Constants[6] = FVector4(OutColorShadow_Tint1, 0.0f);
	Constants[7] = FVector4(OutColorShadow_Tint2, 0.0f);
}


BEGIN_UNIFORM_BUFFER_STRUCT(FBloomDirtMaskParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,Tint)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D,Mask)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState,MaskSampler)
END_UNIFORM_BUFFER_STRUCT(FBloomDirtMaskParameters)
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FBloomDirtMaskParameters,TEXT("BloomDirtMask"));


static TAutoConsoleVariable<float> CVarGamma(
	TEXT("r.Gamma"),
	1.0f,
	TEXT("Gamma on output"),
	ECVF_RenderThreadSafe);

/**
 * Encapsulates the post processing tonemapper pixel shader.
 */
template<uint32 ConfigIndex>
class FPostProcessTonemapPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTonemapPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		uint32 ConfigBitmask = TonemapperConfBitmaskPC[ConfigIndex];

		OutEnvironment.SetDefine(TEXT("USE_GAMMA_ONLY"),         TonemapperIsDefined(ConfigBitmask, TonemapperGammaOnly));
		OutEnvironment.SetDefine(TEXT("USE_COLOR_MATRIX"),       TonemapperIsDefined(ConfigBitmask, TonemapperColorMatrix));
		OutEnvironment.SetDefine(TEXT("USE_SHADOW_TINT"),        TonemapperIsDefined(ConfigBitmask, TonemapperShadowTint));
		OutEnvironment.SetDefine(TEXT("USE_CONTRAST"),           TonemapperIsDefined(ConfigBitmask, TonemapperContrast));
		OutEnvironment.SetDefine(TEXT("USE_BLOOM"),              TonemapperIsDefined(ConfigBitmask, TonemapperBloom));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_JITTER"),       TonemapperIsDefined(ConfigBitmask, TonemapperGrainJitter));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_INTENSITY"),    TonemapperIsDefined(ConfigBitmask, TonemapperGrainIntensity));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_QUANTIZATION"), TonemapperIsDefined(ConfigBitmask, TonemapperGrainQuantization));
		OutEnvironment.SetDefine(TEXT("USE_VIGNETTE"),           TonemapperIsDefined(ConfigBitmask, TonemapperVignette));
		OutEnvironment.SetDefine(TEXT("USE_COLOR_FRINGE"),		 TonemapperIsDefined(ConfigBitmask, TonemapperColorFringe));
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeTextureLUT(Platform));

		if( !IsFeatureLevelSupported(Platform,ERHIFeatureLevel::SM5) )
		{
			//Need to hack in exposure scale for < SM5
			OutEnvironment.SetDefine(TEXT("NO_EYEADAPTATION_EXPOSURE_FIX"), 1);
		}
	}

	/** Default constructor. */
	FPostProcessTonemapPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ColorScale0;
	FShaderParameter ColorScale1;
	FShaderResourceParameter NoiseTexture;
	FShaderResourceParameter NoiseTextureSampler;
	FShaderParameter TexScale;
	FShaderParameter VignetteIntensity;
	FShaderParameter GrainScaleBiasJitter;
	FShaderResourceParameter ColorGradingLUT;
	FShaderResourceParameter ColorGradingLUTSampler;
	FShaderParameter InverseGamma;

	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;

	//@HACK
	FShaderParameter OverlayColor;

	/** Initialization constructor. */
	FPostProcessTonemapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		ColorScale0.Bind(Initializer.ParameterMap, TEXT("ColorScale0"));
		ColorScale1.Bind(Initializer.ParameterMap, TEXT("ColorScale1"));
		NoiseTexture.Bind(Initializer.ParameterMap,TEXT("NoiseTexture"));
		NoiseTextureSampler.Bind(Initializer.ParameterMap,TEXT("NoiseTextureSampler"));
		TexScale.Bind(Initializer.ParameterMap, TEXT("TexScale"));
		VignetteIntensity.Bind(Initializer.ParameterMap, TEXT("VignetteIntensity"));
		GrainScaleBiasJitter.Bind(Initializer.ParameterMap, TEXT("GrainScaleBiasJitter"));
		ColorGradingLUT.Bind(Initializer.ParameterMap, TEXT("ColorGradingLUT"));
		ColorGradingLUTSampler.Bind(Initializer.ParameterMap, TEXT("ColorGradingLUTSampler"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));

		ColorMatrixR_ColorCurveCd1.Bind(Initializer.ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(Initializer.ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(Initializer.ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint2"));
		
		OverlayColor.Bind(Initializer.ParameterMap, TEXT("OverlayColor"));
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << PostprocessParameter << ColorScale0 << ColorScale1 << InverseGamma << NoiseTexture << NoiseTextureSampler
			<< TexScale << VignetteIntensity << GrainScaleBiasJitter
			<< ColorGradingLUT << ColorGradingLUTSampler
			<< ColorMatrixR_ColorCurveCd1 << ColorMatrixG_ColorCurveCd3Cm3 << ColorMatrixB_ColorCurveCm2 << ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 << ColorCurve_Ch1_Ch2 << ColorShadow_Luma << ColorShadow_Tint1 << ColorShadow_Tint2
			<< OverlayColor;
		
		return bShaderHasOutdatedParameters;
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
			
		SetShaderValue(Context.RHICmdList, ShaderRHI, OverlayColor, Context.View.OverlayColor);

		{
			FLinearColor Col = Settings.SceneColorTint;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorScale0, ColorScale);
		}
		
		{
			FLinearColor Col = FLinearColor::White * Settings.BloomIntensity;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorScale1, ColorScale);
		}

		{
			UTexture2D* NoiseTextureValue = GEngine->HighFrequencyNoiseTexture;

			SetTextureParameter(Context.RHICmdList, ShaderRHI, NoiseTexture, NoiseTextureSampler, TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI(), NoiseTextureValue->Resource->TextureRHI);
		}

		{
			const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

			// we assume the this pass runs in 1:1 pixel
			FVector2D TexScaleValue = FVector2D(InputDesc->Extent) / FVector2D(Context.View.ViewRect.Size());

			SetShaderValue(Context.RHICmdList, ShaderRHI, TexScale, TexScaleValue);
		}
		
		SetShaderValue(Context.RHICmdList, ShaderRHI, VignetteIntensity, Settings.VignetteIntensity);

		FVector GrainValue;
		GrainPostSettings(&GrainValue, &Settings);
		SetShaderValue(Context.RHICmdList, ShaderRHI, GrainScaleBiasJitter, GrainValue);

		const TShaderUniformBufferParameter<FBloomDirtMaskParameters>& BloomDirtMaskParam = GetUniformBufferParameter<FBloomDirtMaskParameters>();
		if (BloomDirtMaskParam.IsBound())
		{
			FBloomDirtMaskParameters BloomDirtMaskParams;

			float ExposureScale = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);
			FLinearColor Col = Settings.BloomDirtMaskTint * Settings.BloomDirtMaskIntensity;
			BloomDirtMaskParams.Tint = FVector4(Col.R, Col.G, Col.B, ExposureScale);

			BloomDirtMaskParams.Mask = GSystemTextures.BlackDummy->GetRenderTargetItem().TargetableTexture;
			if(Settings.BloomDirtMask && Settings.BloomDirtMask->Resource)
			{
				BloomDirtMaskParams.Mask = Settings.BloomDirtMask->Resource->TextureRHI;
			}
			BloomDirtMaskParams.MaskSampler = TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI();

			FUniformBufferRHIRef BloomDirtMaskUB = TUniformBufferRef<FBloomDirtMaskParameters>::CreateUniformBufferImmediate(BloomDirtMaskParams, UniformBuffer_SingleDraw);
			SetUniformBufferParameter(Context.RHICmdList, ShaderRHI, BloomDirtMaskParam, BloomDirtMaskUB);
		}
		
		// volume texture LUT
		{
			FRenderingCompositeOutputRef* OutputRef = Context.Pass->GetInput(ePId_Input3);

			if(OutputRef)
			{
				FRenderingCompositeOutput* Input = OutputRef->GetOutput();

				if(Input)
				{
					TRefCountPtr<IPooledRenderTarget> InputPooledElement = Input->RequestInput();

					if(InputPooledElement)
					{
						check(!InputPooledElement->IsFree());

						const FTextureRHIRef& SrcTexture = InputPooledElement->GetRenderTargetItem().ShaderResourceTexture;
					
						SetTextureParameter(Context.RHICmdList, ShaderRHI, ColorGradingLUT, ColorGradingLUTSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), SrcTexture);
					}
				}
			}
		}

		{
			FVector InvDisplayGammaValue;
			InvDisplayGammaValue.X = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Y = 2.2f / ViewFamily.RenderTarget->GetDisplayGamma();
			{
				static TConsoleVariableData<float>* CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.TonemapperGamma"));
				float Value = CVar->GetValueOnRenderThread();
				if(Value < 1.0f)
				{
					Value = 1.0f;
				}
				InvDisplayGammaValue.Z = 1.0f / Value;
			}
			SetShaderValue(Context.RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			FVector4 Constants[8];
			FilmPostSetConstants(Constants, TonemapperConfBitmaskPC[ConfigIndex], &Context.View.FinalPostProcessSettings, false);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorMatrixR_ColorCurveCd1, Constants[0]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorMatrixG_ColorCurveCd3Cm3, Constants[1]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorMatrixB_ColorCurveCm2, Constants[2]); 
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3, Constants[3]); 
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorCurve_Ch1_Ch2, Constants[4]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorShadow_Luma, Constants[5]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorShadow_Tint1, Constants[6]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorShadow_Tint2, Constants[7]);
		}
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessTonemap");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessTonemapPS<A> FPostProcessTonemapPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessTonemapPS##A, SF_Pixel);

	VARIATION1(0)  VARIATION1(1)  VARIATION1(2)  VARIATION1(3)  VARIATION1(4)  VARIATION1(5) VARIATION1(6)

#undef VARIATION1


IMPLEMENT_SHADER_TYPE(,FPostProcessTonemapVS,TEXT("PostProcessTonemap"),TEXT("MainVS"),SF_Vertex);


FRCPassPostProcessTonemap::FRCPassPostProcessTonemap(const FViewInfo& View, bool bInDoGammaOnly)
	: bDoGammaOnly(bInDoGammaOnly)
{
	uint32 ConfigBitmask = TonemapperGenerateBitmaskPC(&View, bDoGammaOnly);
	ConfigIndexPC = TonemapperFindLeastExpensive(TonemapperConfBitmaskPC, sizeof(TonemapperConfBitmaskPC)/4, TonemapperCostTab, ConfigBitmask);
}

template <uint32 ConfigIndex>
static void SetShaderTempl(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessTonemapVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessTonemapPS<ConfigIndex> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessTonemap::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessTonemap, TEXT("Tonemapper#%d%s"), ConfigIndexPC, bDoGammaOnly ? TEXT(" GammaOnly") : TEXT(""));

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}
	
	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	
	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef());

	if( ViewFamily.RenderTarget->GetRenderTargetTexture() != DestRenderTarget.TargetableTexture )
	{
		// needed to not have PostProcessAA leaking in content (e.g. Matinee black borders), is optimized away if possible (RT size=view size, )
		Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, View.ViewRect);
	}

	Context.SetViewportAndCallRHI(View.ViewRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	switch(ConfigIndexPC)
	{
		case 0:	SetShaderTempl<0>(Context); break;
		case 1:	SetShaderTempl<1>(Context);	break;
		case 2: SetShaderTempl<2>(Context); break;
		case 3: SetShaderTempl<3>(Context); break;
		case 4: SetShaderTempl<4>(Context); break;
		case 5: SetShaderTempl<5>(Context); break;
		case 6: SetShaderTempl<6>(Context); break;
		default:
			check(0);
	}

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessTonemapVS> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y, 
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		GSceneRenderTargets.GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// We only release the SceneColor after the last view was processed (SplitScreen)
	if(Context.View.Family->Views[Context.View.Family->Views.Num() - 1] == &Context.View && !GIsEditor)
	{
		// The RT should be released as early as possible to allow sharing of that memory for other purposes.
		// This becomes even more important with some limited VRam (XBoxOne).
		GSceneRenderTargets.SetSceneColor(0);
	}
}

FPooledRenderTargetDesc FRCPassPostProcessTonemap::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	// RGB is the color in LDR, A is the luminance for PostprocessAA
	Ret.Format = PF_B8G8R8A8;
	Ret.DebugName = TEXT("Tonemap");

	return Ret;
}





// ES2 version

/**
 * Encapsulates the post processing tonemapper pixel shader.
 */
template<uint32 ConfigIndex>
class FPostProcessTonemapPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTonemapPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// This is only used on ES2.
		// TODO: Make this only compile on PC/Mobile (and not console).
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		uint32 ConfigBitmask = TonemapperConfBitmaskMobile[ConfigIndex];

		OutEnvironment.SetDefine(TEXT("USE_GAMMA_ONLY"),         TonemapperIsDefined(ConfigBitmask, TonemapperGammaOnly));
		OutEnvironment.SetDefine(TEXT("USE_COLOR_MATRIX"),       TonemapperIsDefined(ConfigBitmask, TonemapperColorMatrix));
		OutEnvironment.SetDefine(TEXT("USE_SHADOW_TINT"),        TonemapperIsDefined(ConfigBitmask, TonemapperShadowTint));
		OutEnvironment.SetDefine(TEXT("USE_CONTRAST"),           TonemapperIsDefined(ConfigBitmask, TonemapperContrast));
		OutEnvironment.SetDefine(TEXT("USE_HDR_MOSAIC"),         TonemapperIsDefined(ConfigBitmask, TonemapperMosaic));
		OutEnvironment.SetDefine(TEXT("USE_BLOOM"),              TonemapperIsDefined(ConfigBitmask, TonemapperBloom));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_JITTER"),       TonemapperIsDefined(ConfigBitmask, TonemapperGrainJitter));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_INTENSITY"),    TonemapperIsDefined(ConfigBitmask, TonemapperGrainIntensity));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_QUANTIZATION"), TonemapperIsDefined(ConfigBitmask, TonemapperGrainQuantization));
		OutEnvironment.SetDefine(TEXT("USE_VIGNETTE"),           TonemapperIsDefined(ConfigBitmask, TonemapperVignette));
		OutEnvironment.SetDefine(TEXT("USE_LIGHT_SHAFTS"),       TonemapperIsDefined(ConfigBitmask, TonemapperLightShafts));
		OutEnvironment.SetDefine(TEXT("USE_DOF"),                TonemapperIsDefined(ConfigBitmask, TonemapperDOF));
		OutEnvironment.SetDefine(TEXT("USE_MSAA"),               TonemapperIsDefined(ConfigBitmask, TonemapperMsaa));

		//Need to hack in exposure scale for < SM5
		OutEnvironment.SetDefine(TEXT("NO_EYEADAPTATION_EXPOSURE_FIX"), 1);
	}

	/** Default constructor. */
	FPostProcessTonemapPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ColorScale0;
	FShaderParameter ColorScale1;
	FShaderParameter TexScale;
	FShaderParameter GrainScaleBiasJitter;
	FShaderParameter InverseGamma;
	FShaderParameter VignetteIntensity;

	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;

	FShaderParameter OverlayColor;

	/** Initialization constructor. */
	FPostProcessTonemapPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		ColorScale0.Bind(Initializer.ParameterMap, TEXT("ColorScale0"));
		ColorScale1.Bind(Initializer.ParameterMap, TEXT("ColorScale1"));
		TexScale.Bind(Initializer.ParameterMap, TEXT("TexScale"));
		VignetteIntensity.Bind(Initializer.ParameterMap, TEXT("VignetteIntensity"));
		GrainScaleBiasJitter.Bind(Initializer.ParameterMap, TEXT("GrainScaleBiasJitter"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));

		ColorMatrixR_ColorCurveCd1.Bind(Initializer.ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(Initializer.ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(Initializer.ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint2"));

		OverlayColor.Bind(Initializer.ParameterMap, TEXT("OverlayColor"));
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << PostprocessParameter << ColorScale0 << ColorScale1 << InverseGamma
			<< TexScale << GrainScaleBiasJitter << VignetteIntensity
			<< ColorMatrixR_ColorCurveCd1 << ColorMatrixG_ColorCurveCd3Cm3 << ColorMatrixB_ColorCurveCm2 << ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 << ColorCurve_Ch1_Ch2 << ColorShadow_Luma << ColorShadow_Tint1 << ColorShadow_Tint2
			<< OverlayColor;

		return bShaderHasOutdatedParameters;
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		const uint32 ConfigBitmask = TonemapperConfBitmaskMobile[ConfigIndex];
			
		if (TonemapperIsDefined(ConfigBitmask, TonemapperMosaic))
		{
			PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		}
		else
		{
			PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		}
			
		SetShaderValue(Context.RHICmdList, ShaderRHI, OverlayColor, Context.View.OverlayColor);

		{
			FLinearColor Col = Settings.SceneColorTint;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorScale0, ColorScale);
		}
		
		{
			FLinearColor Col = FLinearColor::White * Settings.BloomIntensity;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorScale1, ColorScale);
		}

		{
			const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

			// we assume the this pass runs in 1:1 pixel
			FVector2D TexScaleValue = FVector2D(InputDesc->Extent) / FVector2D(Context.View.ViewRect.Size());

			SetShaderValue(Context.RHICmdList, ShaderRHI, TexScale, TexScaleValue);
		}

		SetShaderValue(Context.RHICmdList, ShaderRHI, VignetteIntensity, Settings.VignetteIntensity);

		FVector GrainValue;
		GrainPostSettings(&GrainValue, &Settings);
		SetShaderValue(Context.RHICmdList, ShaderRHI, GrainScaleBiasJitter, GrainValue);

		{
			FVector InvDisplayGammaValue;
			InvDisplayGammaValue.X = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Y = 2.2f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Z = 1.0; // Unused on mobile.
			SetShaderValue(Context.RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			FVector4 Constants[8];
			FilmPostSetConstants(Constants, TonemapperConfBitmaskMobile[ConfigIndex], &Context.View.FinalPostProcessSettings, true);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorMatrixR_ColorCurveCd1, Constants[0]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorMatrixG_ColorCurveCd3Cm3, Constants[1]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorMatrixB_ColorCurveCm2, Constants[2]); 
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3, Constants[3]); 
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorCurve_Ch1_Ch2, Constants[4]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorShadow_Luma, Constants[5]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorShadow_Tint1, Constants[6]);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorShadow_Tint2, Constants[7]);
		}
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessTonemap");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS_ES2");
	}
};

// #define avoids a lot of code duplication
#define VARIATION2(A) typedef FPostProcessTonemapPS_ES2<A> FPostProcessTonemapPS_ES2##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessTonemapPS_ES2##A, SF_Pixel);

	VARIATION2(0)  VARIATION2(1)  VARIATION2(2)  VARIATION2(3)	VARIATION2(4)  VARIATION2(5)  VARIATION2(6)  VARIATION2(7)  VARIATION2(8)  VARIATION2(9)  
	VARIATION2(10) VARIATION2(11) VARIATION2(12) VARIATION2(13)	VARIATION2(14) VARIATION2(15) VARIATION2(16) VARIATION2(17) VARIATION2(18) VARIATION2(19)  
	VARIATION2(20) VARIATION2(21) VARIATION2(22) VARIATION2(23)	VARIATION2(24) VARIATION2(25) VARIATION2(26) VARIATION2(27) VARIATION2(28) VARIATION2(29)
	VARIATION2(30) VARIATION2(31) VARIATION2(32) VARIATION2(33)	VARIATION2(34) VARIATION2(35) VARIATION2(36) VARIATION2(37) VARIATION2(38)


#undef VARIATION2
	

class FPostProcessTonemapVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTonemapVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FPostProcessTonemapVS_ES2() { }

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter GrainRandomFull;
	bool bUsedFramebufferFetch;

	FPostProcessTonemapVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		GrainRandomFull.Bind(Initializer.ParameterMap, TEXT("GrainRandomFull"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector GrainRandomFullValue;
		GrainRandomFromFrame(&GrainRandomFullValue, Context.View.Family->FrameNumber);
		// TODO: Don't use full on mobile with framebuffer fetch.
		GrainRandomFullValue.Z = bUsedFramebufferFetch ? 0.0f : 1.0f;
		SetShaderValue(Context.RHICmdList, ShaderRHI, GrainRandomFull, GrainRandomFullValue);
	}
	
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << GrainRandomFull;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessTonemapVS_ES2,TEXT("PostProcessTonemap"),TEXT("MainVS_ES2"),SF_Vertex);


template <uint32 ConfigIndex>
static void SetShaderTemplES2(const FRenderingCompositePassContext& Context, bool bUsedFramebufferFetch)
{
	TShaderMapRef<FPostProcessTonemapVS_ES2> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessTonemapPS_ES2<ConfigIndex> > PixelShader(Context.GetShaderMap());

	VertexShader->bUsedFramebufferFetch = bUsedFramebufferFetch;

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}


FRCPassPostProcessTonemapES2::FRCPassPostProcessTonemapES2(const FViewInfo& View, FIntRect InViewRect, FIntPoint InDestSize, bool bInUsedFramebufferFetch) 
	:
	ViewRect(InViewRect),
	DestSize(InDestSize),
	bUsedFramebufferFetch(bInUsedFramebufferFetch)
{
	uint32 ConfigBitmask = TonemapperGenerateBitmaskMobile(&View, false);
	ConfigIndexMobile = TonemapperFindLeastExpensive(TonemapperConfBitmaskMobile, sizeof(TonemapperConfBitmaskMobile)/4, TonemapperCostTab, ConfigBitmask);
}

void FRCPassPostProcessTonemapES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessTonemap, TEXT("Tonemapper#%d%s"), ConfigIndexMobile, bUsedFramebufferFetch ? TEXT(" FramebufferFetch=0") : TEXT("FramebufferFetch=1"));

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}
	
	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	const FPooledRenderTargetDesc& OutputDesc = PassOutputs[0].RenderTargetDesc;

	FIntRect SrcRect = ViewRect;
	FIntRect DestRect = View.UnscaledViewRect; // Simple upscaling, ES2 post process does not currently have a specific upscaling pass.
	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DstSize = OutputDesc.Extent;

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef());

	// Full clear to avoid restore
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(DestRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	switch(ConfigIndexMobile)
	{
		case 0:	SetShaderTemplES2<0>(Context, bUsedFramebufferFetch); break;
		case 1:	SetShaderTemplES2<1>(Context, bUsedFramebufferFetch); break;
		case 2:	SetShaderTemplES2<2>(Context, bUsedFramebufferFetch); break;
		case 3:	SetShaderTemplES2<3>(Context, bUsedFramebufferFetch); break;
		case 4:	SetShaderTemplES2<4>(Context, bUsedFramebufferFetch); break;
		case 5:	SetShaderTemplES2<5>(Context, bUsedFramebufferFetch); break;
		case 6:	SetShaderTemplES2<6>(Context, bUsedFramebufferFetch); break;
		case 7:	SetShaderTemplES2<7>(Context, bUsedFramebufferFetch); break;
		case 8:	SetShaderTemplES2<8>(Context, bUsedFramebufferFetch); break;
		case 9:	SetShaderTemplES2<9>(Context, bUsedFramebufferFetch); break;
		case 10: SetShaderTemplES2<10>(Context, bUsedFramebufferFetch); break;
		case 11: SetShaderTemplES2<11>(Context, bUsedFramebufferFetch); break;
		case 12: SetShaderTemplES2<12>(Context, bUsedFramebufferFetch); break;
		case 13: SetShaderTemplES2<13>(Context, bUsedFramebufferFetch); break;
		case 14: SetShaderTemplES2<14>(Context, bUsedFramebufferFetch); break;
		case 15: SetShaderTemplES2<15>(Context, bUsedFramebufferFetch); break;
		case 16: SetShaderTemplES2<16>(Context, bUsedFramebufferFetch); break;
		case 17: SetShaderTemplES2<17>(Context, bUsedFramebufferFetch); break;
		case 18: SetShaderTemplES2<18>(Context, bUsedFramebufferFetch); break;
		case 19: SetShaderTemplES2<19>(Context, bUsedFramebufferFetch); break;
		case 20: SetShaderTemplES2<20>(Context, bUsedFramebufferFetch); break;
		case 21: SetShaderTemplES2<21>(Context, bUsedFramebufferFetch); break;
		case 22: SetShaderTemplES2<22>(Context, bUsedFramebufferFetch); break;
		case 23: SetShaderTemplES2<23>(Context, bUsedFramebufferFetch); break;
		case 24: SetShaderTemplES2<24>(Context, bUsedFramebufferFetch); break;
		case 25: SetShaderTemplES2<25>(Context, bUsedFramebufferFetch); break;
		case 26: SetShaderTemplES2<26>(Context, bUsedFramebufferFetch); break;
		case 27: SetShaderTemplES2<27>(Context, bUsedFramebufferFetch); break;
		case 28: SetShaderTemplES2<28>(Context, bUsedFramebufferFetch); break;
		case 29: SetShaderTemplES2<29>(Context, bUsedFramebufferFetch); break;
		case 30: SetShaderTemplES2<30>(Context, bUsedFramebufferFetch); break;
		case 31: SetShaderTemplES2<31>(Context, bUsedFramebufferFetch); break;
		case 32: SetShaderTemplES2<32>(Context, bUsedFramebufferFetch); break;
		case 33: SetShaderTemplES2<33>(Context, bUsedFramebufferFetch); break;
		case 34: SetShaderTemplES2<34>(Context, bUsedFramebufferFetch); break;
		case 35: SetShaderTemplES2<35>(Context, bUsedFramebufferFetch); break;
		case 36: SetShaderTemplES2<36>(Context, bUsedFramebufferFetch); break;
		case 37: SetShaderTemplES2<37>(Context, bUsedFramebufferFetch); break;
		case 38: SetShaderTemplES2<38>(Context, bUsedFramebufferFetch); break;
		default:
			check(0);
	}

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessTonemapVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstSize.X, DstSize.Y,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// Double buffer tonemapper output for temporal AA.
	if(Context.View.FinalPostProcessSettings.AntiAliasingMethod == AAM_TemporalAA)
	{
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		if(ViewState) 
		{
			ViewState->MobileAaColor0 = PassOutputs[0].PooledRenderTarget;
		}
	}
}

FPooledRenderTargetDesc FRCPassPostProcessTonemapES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.Format = PF_B8G8R8A8;
	Ret.DebugName = TEXT("Tonemap");
	Ret.Extent = DestSize;
	return Ret;
}
