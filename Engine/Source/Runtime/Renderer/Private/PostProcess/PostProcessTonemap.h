// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTonemap.h: Post processing tone mapping implementation, can add bloom.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"


static float GrainHalton( int32 Index, int32 Base )
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while( Index > 0 )
	{
		Result += ( Index % Base ) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

static void GrainRandomFromFrame(FVector* RESTRICT const Constant, uint32 FrameNumber)
{
	Constant->X = GrainHalton(FrameNumber & 1023, 2);
	Constant->Y = GrainHalton(FrameNumber & 1023, 3);
}


void FilmPostSetConstants(FVector4* RESTRICT const Constants, const uint32 TonemapperType16, const FPostProcessSettings* RESTRICT const FinalPostProcessSettings, bool bMobile);

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: BloomCombined (not needed for bDoGammaOnly)
// ePId_Input2: EyeAdaptation (not needed for bDoGammaOnly)
// ePId_Input3: LUTsCombined (not needed for bDoGammaOnly)
class FRCPassPostProcessTonemap : public TRenderingCompositePassBase<4, 1>
{
public:
	// constructor
	FRCPassPostProcessTonemap(const FViewInfo& View, bool bInDoGammaOnly = false);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	bool bDoGammaOnly;
	// set in constructor
	uint32 ConfigIndexPC;

	void SetShader(const FRenderingCompositePassContext& Context);
};

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
// ePId_Input0: SceneColor
// ePId_Input1: BloomCombined (not needed for bDoGammaOnly)
// ePId_Input2: Dof (not needed for bDoGammaOnly)
class FRCPassPostProcessTonemapES2 : public TRenderingCompositePassBase<3, 1>
{
public:
	FRCPassPostProcessTonemapES2(const FViewInfo& View, FIntRect InViewRect, FIntPoint InDestSize, bool bInUsedFramebufferFetch);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

private:
	FIntRect ViewRect;
	FIntPoint DestSize;
	bool bUsedFramebufferFetch;
	// set in constructor
	uint32 ConfigIndexMobile;

	void SetShader(const FRenderingCompositePassContext& Context);
};


/** Encapsulates the post processing tone map vertex shader. */
class FPostProcessTonemapVS : public FGlobalShader
{
	// This class is in the header so that Temporal AA can share this vertex shader.
	DECLARE_SHADER_TYPE(FPostProcessTonemapVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FPostProcessTonemapVS(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter GrainRandomFull;
	FShaderParameter FringeUVParams;

	/** Initialization constructor. */
	FPostProcessTonemapVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
		GrainRandomFull.Bind(Initializer.ParameterMap, TEXT("GrainRandomFull"));
		FringeUVParams.Bind(Initializer.ParameterMap, TEXT("FringeUVParams"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector GrainRandomFullValue;
		GrainRandomFromFrame(&GrainRandomFullValue, Context.View.Family->FrameNumber);
		SetShaderValue(Context.RHICmdList, ShaderRHI, GrainRandomFull, GrainRandomFullValue);

		if(EyeAdaptation.IsBound())
		{
			IPooledRenderTarget* EyeAdaptationRT = Context.View.GetEyeAdaptation();

			if(EyeAdaptationRT)
			{
				SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationRT->GetRenderTargetItem().TargetableTexture);
			}
			else
			{
				// some views don't have a state, thumbnail rendering?
				SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, GWhiteTexture->TextureRHI);
			}
		}

		{
			// for scene color fringe
			// from percent to fraction
			float Offset = Context.View.FinalPostProcessSettings.SceneFringeIntensity * 0.01f;
			//FVector4 Value(1.0f - Offset * 0.5f, 1.0f - Offset, 0.0f, 0.0f);

			// Wavelength of primaries in nm
			const float PrimaryR = 611.3f;
			const float PrimaryG = 549.1f;
			const float PrimaryB = 464.3f;

			// Simple lens chromatic aberration is roughly linear in wavelength
			float ScaleR = 0.007f * ( PrimaryR - PrimaryB );
			float ScaleG = 0.007f * ( PrimaryG - PrimaryB );
			FVector4 Value( 1.0f / ( 1.0f + Offset * ScaleG ), 1.0f / ( 1.0f + Offset * ScaleR ), 0.0f, 0.0f);

			// we only get bigger to not leak in content from outside
			SetShaderValue(Context.RHICmdList, ShaderRHI, FringeUVParams, Value);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << GrainRandomFull << EyeAdaptation << FringeUVParams;
		return bShaderHasOutdatedParameters;
	}
};
