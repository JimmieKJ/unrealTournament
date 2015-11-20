// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GammaCorrection.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessing.h"
#include "SceneUtils.h"

/** Encapsulates the gamma correction pixel shader. */
class FGammaCorrectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGammaCorrectionPS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FGammaCorrectionPS() {}

public:

	FShaderResourceParameter SceneTexture;
	FShaderResourceParameter SceneTextureSampler;
	FShaderParameter InverseGamma;
	FShaderParameter ColorScale;
	FShaderParameter OverlayColor;

	/** Initialization constructor. */
	FGammaCorrectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneTexture.Bind(Initializer.ParameterMap,TEXT("SceneColorTexture"));
		SceneTextureSampler.Bind(Initializer.ParameterMap,TEXT("SceneColorTextureSampler"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));
		ColorScale.Bind(Initializer.ParameterMap,TEXT("ColorScale"));
		OverlayColor.Bind(Initializer.ParameterMap,TEXT("OverlayColor"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		
		Ar << SceneTexture << SceneTextureSampler << InverseGamma << ColorScale << OverlayColor;
		return bShaderHasOutdatedParameters;
	}
};

/** Encapsulates the gamma correction vertex shader. */
class FGammaCorrectionVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGammaCorrectionVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FGammaCorrectionVS() {}

public:

	/** Initialization constructor. */
	FGammaCorrectionVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
	}
};

IMPLEMENT_SHADER_TYPE(,FGammaCorrectionPS,TEXT("GammaCorrection"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FGammaCorrectionVS,TEXT("GammaCorrection"),TEXT("MainVS"),SF_Vertex);

// TODO: REMOVE if no longer needed:
void FSceneRenderer::GammaCorrectToViewportRenderTarget(FRHICommandList& RHICmdList, const FViewInfo* View, float OverrideGamma)
{
	// Set the view family's render target/viewport.
	SetRenderTarget(RHICmdList, ViewFamily.RenderTarget->GetRenderTargetTexture(), FTextureRHIRef());

	// Deferred the clear until here so the garbage left in the non rendered regions by the post process effects do not show up
	if( ViewFamily.bDeferClear )
	{
		RHICmdList.Clear(true, FLinearColor::Black, false, 0.0f, false, 0, FIntRect());
		ViewFamily.bDeferClear = false;
	}

	SCOPED_DRAW_EVENT(RHICmdList, GammaCorrection);

	// turn off culling and blending
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

	// turn off depth reads/writes
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FGammaCorrectionVS> VertexShader(View->ShaderMap);
	TShaderMapRef<FGammaCorrectionPS> PixelShader(View->ShaderMap);

	static FGlobalBoundShaderState PostProcessBoundShaderState;
	
	SetGlobalBoundShaderState(RHICmdList, View->GetFeatureLevel(), PostProcessBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	float InvDisplayGamma = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();

	if (OverrideGamma != 0)
	{
		InvDisplayGamma = 1 / OverrideGamma;
	}

	const FPixelShaderRHIParamRef ShaderRHI = PixelShader->GetPixelShader();

	SetShaderValue(
		RHICmdList, 
		ShaderRHI,
		PixelShader->InverseGamma,
		InvDisplayGamma
		);
	SetShaderValue(RHICmdList, ShaderRHI,PixelShader->ColorScale,View->ColorScale);
	SetShaderValue(RHICmdList, ShaderRHI,PixelShader->OverlayColor,View->OverlayColor);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const FTextureRHIRef DesiredSceneColorTexture = SceneContext.GetSceneColorTexture();

	SetTextureParameter(
		RHICmdList, 
		ShaderRHI,
		PixelShader->SceneTexture,
		PixelShader->SceneTextureSampler,
		TStaticSamplerState<SF_Bilinear>::GetRHI(),
		DesiredSceneColorTexture
		);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		RHICmdList,
		View->UnscaledViewRect.Min.X,View->UnscaledViewRect.Min.Y,
		View->UnscaledViewRect.Width(),View->UnscaledViewRect.Height(),
		View->ViewRect.Min.X,View->ViewRect.Min.Y,
		View->ViewRect.Width(),View->ViewRect.Height(),
		ViewFamily.RenderTarget->GetSizeXY(),
		SceneContext.GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);
}
