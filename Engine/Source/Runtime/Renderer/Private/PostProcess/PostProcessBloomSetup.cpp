// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBloomSetup.cpp: Post processing bloom threshold pass implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessBloomSetup.h"
#include "PostProcessing.h"
#include "PostProcessHistogram.h"
#include "PostProcessEyeAdaptation.h"
#include "SceneUtils.h"

/** Encapsulates the post processing bloom threshold pixel shader. */
class FPostProcessBloomSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}	
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		if( !IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) )
		{
			//Need to hack in exposure scale for < SM5
			OutEnvironment.SetDefine(TEXT("NO_EYEADAPTATION_EXPOSURE_FIX"), 1);
		}
	}

	/** Default constructor. */
	FPostProcessBloomSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomThreshold;

	/** Initialization constructor. */
	FPostProcessBloomSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomThreshold.Bind(Initializer.ParameterMap, TEXT("BloomThreshold"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		float ExposureScale = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);

		FVector4 BloomThresholdValue(Settings.BloomThreshold, 0, 0, ExposureScale);
		SetShaderValue(Context.RHICmdList, ShaderRHI, BloomThreshold, BloomThresholdValue);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomThreshold;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupPS,TEXT("PostProcessBloom"),TEXT("MainPS"),SF_Pixel);


/** Encapsulates the post processing tone map vertex shader. */
class FPostProcessBloomSetupVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessBloomSetupVS(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;


	/** Initialization constructor. */
	FPostProcessBloomSetupVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
	}
	
	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		if(EyeAdaptation.IsBound())
		{
			IPooledRenderTarget* EyeAdaptationRT = Context.View.GetEyeAdaptation();

			if(EyeAdaptationRT)
			{
				SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationRT->GetRenderTargetItem().TargetableTexture);
			}
			else
			{
				SetTextureParameter(Context.RHICmdList, ShaderRHI, EyeAdaptation, GWhiteTexture->TextureRHI);
			}
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << EyeAdaptation;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupVS,TEXT("PostProcessBloom"),TEXT("MainVS"),SF_Vertex);

void FRCPassPostProcessBloomSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessBloomSetup);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = GSceneRenderTargets.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessBloomSetupVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBloomSetupPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("BloomSetup");

	return Ret;
}







/** Encapsulates the visualize bloom setup pixel shader. */
class FPostProcessVisualizeBloomSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeBloomSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessVisualizeBloomSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessVisualizeBloomSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Border>::GetRHI());
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVisualizeBloomSetupPS,TEXT("PostProcessBloom"),TEXT("VisualizeBloomSetupPS"),SF_Pixel);




void FRCPassPostProcessVisualizeBloomSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeBloomSetup);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc && "Input is not hooked up correctly");

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = GSceneRenderTargets.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 1.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestRect.Width(), DestRect.Height(), 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeBloomSetupPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}


FPooledRenderTargetDesc FRCPassPostProcessVisualizeBloomSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.TargetableFlags &= ~TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.DebugName = TEXT("VisualizeBloomSetup");

	return Ret;
}







/** Encapsulates the visualize bloom overlay pixel shader. */
class FPostProcessVisualizeBloomOverlayPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeBloomOverlayPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessVisualizeBloomOverlayPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ColorScale1;

	/** Initialization constructor. */
	FPostProcessVisualizeBloomOverlayPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		ColorScale1.Bind(Initializer.ParameterMap, TEXT("ColorScale1"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << ColorScale1;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Border>::GetRHI());

		{
			FLinearColor Col = FLinearColor::White * Settings.BloomIntensity;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorScale1, ColorScale);
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVisualizeBloomOverlayPS,TEXT("PostProcessBloom"),TEXT("VisualizeBloomOverlayPS"),SF_Pixel);




void FRCPassPostProcessVisualizeBloomOverlay::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeBloomOverlay);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc && "Input is not hooked up correctly");

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = GSceneRenderTargets.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 1.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestRect.Width(), DestRect.Height(), 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeBloomOverlayPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}


FPooledRenderTargetDesc FRCPassPostProcessVisualizeBloomOverlay::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.TargetableFlags &= ~TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.DebugName = TEXT("VisualizeBloomOverlay");

	return Ret;
}

