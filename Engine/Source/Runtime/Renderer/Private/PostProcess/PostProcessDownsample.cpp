// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDownsample.cpp: Post processing down sample implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessDownsample.h"
#include "PostProcessing.h"
#include "PostProcessWeightedSampleSum.h"
#include "SceneUtils.h"

/** Encapsulates the post processing down sample pixel shader. */
template <uint32 Method>
class FPostProcessDownsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDownsamplePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return Method != 2 || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("METHOD"), Method);
	}

	/** Default constructor. */
	FPostProcessDownsamplePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;

	/** Initialization constructor. */
	FPostProcessDownsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessDownsample");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessDownsamplePS<A> FPostProcessDownsamplePS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessDownsamplePS##A, SF_Pixel);

VARIATION1(0)			VARIATION1(1)			VARIATION1(2)
#undef VARIATION1


/** Encapsulates the post processing down sample vertex shader. */
class FPostProcessDownsampleVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDownsampleVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FPostProcessDownsampleVS() {}
	
	/** Initialization constructor. */
	FPostProcessDownsampleVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}

	/** Serializer */
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

		if(!InputDesc)
		{
			// input is not hooked up correctly
			return;
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDownsampleVS,TEXT("PostProcessDownsample"),TEXT("MainDownsampleVS"),SF_Vertex);


FRCPassPostProcessDownsample::FRCPassPostProcessDownsample(EPixelFormat InOverrideFormat, uint32 InQuality, const TCHAR *InDebugName)
	: OverrideFormat(InOverrideFormat)
	, Quality(InQuality)
	, DebugName(InDebugName)
{
}


template <uint32 Method>
void FRCPassPostProcessDownsample::SetShader(const FRenderingCompositePassContext& Context)
{
	auto ShaderMap = Context.GetShaderMap();
	TShaderMapRef<FPostProcessDownsampleVS> VertexShader(ShaderMap);
	TShaderMapRef<FPostProcessDownsamplePS<Method> > PixelShader(ShaderMap);

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}


void FRCPassPostProcessDownsample::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, Downsample);

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

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	// InflateSize increases the size of the source/dest rectangle to compensate for bilinear reads and UIBlur pass requirements.
	int32 InflateSize;
	// if second input is hooked up
	if (IsDepthInputAvailable())
	{
		// also put depth in alpha
		InflateSize = 2;
		SetShader<2>(Context);
	}
	else
	{
		if (Quality == 0)
		{
			SetShader<0>(Context);
			InflateSize = 1;
		}
		else
		{
			SetShader<1>(Context);
			InflateSize = 2;
		}
	}

	bool bHasCleared = false;

	// check if we have to clear the whole surface.
	// Otherwise perform the clear when the dest rectangle has been computed.
	auto FeatureLevel = Context.View.GetFeatureLevel();
	if (FeatureLevel == ERHIFeatureLevel::ES2 || FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 1.0f, false, 0, FIntRect());
		bHasCleared = true;
	}

	TShaderMapRef<FPostProcessDownsampleVS> VertexShader(Context.GetShaderMap());

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = FIntRect::DivideAndRoundUp(SrcRect, 2);
	SrcRect = DestRect * 2;

	if (!bHasCleared)
	{
		Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 1.0f, false, 0, DestRect);
	}

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

bool FRCPassPostProcessDownsample::IsDepthInputAvailable() const
{
	// remove const
	FRCPassPostProcessDownsample *This = (FRCPassPostProcessDownsample*)this;

	return This->GetInputDesc(ePId_Input1) != 0;
}

FPooledRenderTargetDesc FRCPassPostProcessDownsample::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();

	Ret.Extent  = FIntPoint::DivideAndRoundUp(Ret.Extent, 2);

	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	if(OverrideFormat != PF_Unknown)
	{
		Ret.Format = OverrideFormat;
	}

	Ret.TargetableFlags &= ~TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.DebugName = DebugName;

	return Ret;
}
