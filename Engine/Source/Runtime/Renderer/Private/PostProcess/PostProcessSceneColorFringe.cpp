// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessSceneColorFringe.cpp: Post processing scene color fringe implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessAA.h"
#include "PostProcessing.h"
#include "PostProcessSceneColorFringe.h"
#include "SceneUtils.h"


/*-----------------------------------------------------------------------------
	FFXAAVertexShader
-----------------------------------------------------------------------------*/
class FPostProcessSceneColorFringeVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSceneColorFringeVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessSceneColorFringeVS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter FringeUVParams;

	/** Initialization constructor. */
	FPostProcessSceneColorFringeVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		FringeUVParams.Bind(Initializer.ParameterMap, TEXT("FringeUVParams"));
	}

	/** Serializer */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		Ar << PostprocessParameter << FringeUVParams;

		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		const FSceneView& View = Context.View;

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

		if(!InputDesc)
		{
			// input is not hooked up correctly
			return;
		}

		{
			// from percent to fraction
			float Offset = View.FinalPostProcessSettings.SceneFringeIntensity * 0.01f;
			// we only get bigger to not leak in content from outside
			FVector4 Value(1.0f, 1.0f - Offset * 0.5f, 1.0f - Offset, 0);

			SetShaderValue(Context.RHICmdList, ShaderRHI, FringeUVParams, Value);
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSceneColorFringeVS,TEXT("PostProcessSceneColorFringe"),TEXT("MainVS"),SF_Vertex);


/** Encapsulates the post processing scene color fringe pixel shader. */
class FPostProcessSceneColorFringePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSceneColorFringePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessSceneColorFringePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter FringeColorParams;

	/** Initialization constructor. */
	FPostProcessSceneColorFringePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		FringeColorParams.Bind(Initializer.ParameterMap, TEXT("FringeColorParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << FringeColorParams;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FSceneView& View = Context.View;

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		{
			FVector4 Value[3];

			float Alpha = View.FinalPostProcessSettings.SceneFringeSaturation;
			
			float t = 1.0f / 3.0f;
			Value[0] = FMath::Lerp(FVector4(t, t, t, 0), FVector4(1, 0, 0, 0), Alpha);
			Value[1] = FMath::Lerp(FVector4(t, t, t, 0), FVector4(0, 1, 0, 0), Alpha);
			Value[2] = FMath::Lerp(FVector4(t, t, t, 0), FVector4(0, 0, 1, 0), Alpha);
			SetShaderValueArray(Context.RHICmdList, ShaderRHI, FringeColorParams, Value, 3);
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSceneColorFringePS,TEXT("PostProcessSceneColorFringe"),TEXT("MainPS"),SF_Pixel);

void FRCPassPostProcessSceneColorFringe::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SceneColorFringe);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(View.ViewRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	
	TShaderMapRef<FPostProcessSceneColorFringeVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSceneColorFringePS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
	
	// Draw a quad mapping scene color to the view's render target
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
}


FPooledRenderTargetDesc FRCPassPostProcessSceneColorFringe::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("PostProcessAA");

	return Ret;
}
