// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessUpscale.cpp: Post processing Upscale implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessUpscale.h"
#include "PostProcessing.h"
#include "PostProcessHistogram.h"
#include "PostProcessEyeAdaptation.h"
#include "SceneUtils.h"

static TAutoConsoleVariable<float> CVarUpscaleSoftness(
	TEXT("r.Upscale.Softness"),
	0.3f,
	TEXT("To scale up with higher quality losing some sharpness\n")
	TEXT(" 0..1 (0.3 is good for ScreenPercentage 90"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

/** Encapsulates the upscale vertex shader. */
class FPostProcessUpscaleVS : public FPostProcessVS
{
	DECLARE_SHADER_TYPE(FPostProcessUpscaleVS,Global);

	/** Default constructor. */
	FPostProcessUpscaleVS() {}

public:
	FShaderParameter DistortionParams;

	/** Initialization constructor. */
	FPostProcessUpscaleVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPostProcessVS(Initializer)
	{
		DistortionParams.Bind(Initializer.ParameterMap,TEXT("DistortionParams"));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPostProcessVS::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TESS_RECT_X"), FTesselatedScreenRectangleIndexBuffer::Width);
		OutEnvironment.SetDefine(TEXT("TESS_RECT_Y"), FTesselatedScreenRectangleIndexBuffer::Height);
	}

	// @param InCylinderDistortion 0=none..1=full in percent, must be in that range
	void SetParameters(const FRenderingCompositePassContext& Context, float InCylinderDistortion)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		{
			float HalfFOV = FMath::Atan(1.0f / Context.View.ViewMatrices.ProjMatrix.M[0][0]);
			float TanHalfFov = 1.0f / Context.View.ViewMatrices.ProjMatrix.M[0][0];
			float InvHalfFov = 1.0f / HalfFOV;

			// compute Correction to scale Y the same as X is scaled in the center
			float SmallX = 0.01f;
			float Correction = atan(SmallX * TanHalfFov) * InvHalfFov / SmallX;

			FVector4 Value(HalfFOV, TanHalfFov, InCylinderDistortion, Correction);

			SetShaderValue(Context.RHICmdList, ShaderRHI, DistortionParams, Value);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FPostProcessVS::Serialize(Ar);
		Ar << DistortionParams;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessUpscaleVS,TEXT("PostProcessUpscale"),TEXT("MainVS"),SF_Vertex);

/** Encapsulates the post processing upscale pixel shader. */
template <uint32 Method>
class FPostProcessUpscalePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessUpscalePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// Always allow simple bilinear upscale. (Provides upscaling for ES2 emulation)
		if (Method == 1)
		{
			return true;
		}

		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("METHOD"), Method);
	}

	/** Default constructor. */
	FPostProcessUpscalePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter UpscaleSoftness;

	/** Initialization constructor. */
	FPostProcessUpscalePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		UpscaleSoftness.Bind(Initializer.ParameterMap,TEXT("UpscaleSoftness"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		FSamplerStateRHIParamRef FilterTable[2];
		FilterTable[0] = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		FilterTable[1] = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
			
		PostprocessParameter.SetPS(ShaderRHI, Context, 0, false, FilterTable);
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			float UpscaleSoftnessValue = FMath::Clamp(CVarUpscaleSoftness.GetValueOnRenderThread(), 0.0f, 1.0f);

			SetShaderValue(Context.RHICmdList, ShaderRHI, UpscaleSoftness, UpscaleSoftnessValue);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << UpscaleSoftness;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessUpscale");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};


// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessUpscalePS<A> FPostProcessUpscalePS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessUpscalePS##A, SF_Pixel);

VARIATION1(0)
VARIATION1(1)
VARIATION1(2)
VARIATION1(3)

#undef VARIATION1

FRCPassPostProcessUpscale::FRCPassPostProcessUpscale(uint32 InUpscaleQuality, float InCylinderDistortion)
	: UpscaleQuality(InUpscaleQuality)
	, CylinderDistortion(FMath::Clamp(InCylinderDistortion, 0.0f, 1.0f))
{
}

template <uint32 Method, uint32 bTesselatedQuad>
FShader* FRCPassPostProcessUpscale::SetShader(const FRenderingCompositePassContext& Context, float InCylinderDistortion)
{
	if(bTesselatedQuad)
	{
		check(InCylinderDistortion > 0.0f);

		TShaderMapRef<FPostProcessUpscaleVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessUpscalePS<Method> > PixelShader(Context.GetShaderMap());

		static FGlobalBoundShaderState BoundShaderState;

		SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetPS(Context);
		VertexShader->SetParameters(Context, InCylinderDistortion);
		return *VertexShader;
	}
	else
	{
		check(InCylinderDistortion == 0.0f);

		TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessUpscalePS<Method> > PixelShader(Context.GetShaderMap());

		static FGlobalBoundShaderState BoundShaderState;

		SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetPS(Context);
		VertexShader->SetParameters(Context);
		return *VertexShader;
	}
}

void FRCPassPostProcessUpscale::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessUpscale);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.UnscaledViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(DestRect);

	bool bTessellatedQuad = CylinderDistortion >= 0.01f;

	// with distortion (bTessellatedQuad) we need to clear the background
	FIntRect ExcludeRect = bTessellatedQuad ? FIntRect() : View.ViewRect;

	Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, ExcludeRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	FShader* VertexShader = 0;

	if(bTessellatedQuad)
	{
		switch (UpscaleQuality)
		{
			case 0:	VertexShader = SetShader<0, 1>(Context, CylinderDistortion); break;
			case 1:	VertexShader = SetShader<1, 1>(Context, CylinderDistortion); break;
			case 2:	VertexShader = SetShader<2, 1>(Context, CylinderDistortion); break;
			case 3:	VertexShader = SetShader<3, 1>(Context, CylinderDistortion); break;
			default:
				checkNoEntry();
				break;
		}
	}
	else
	{
		switch (UpscaleQuality)
		{
			case 0:	VertexShader = SetShader<0, 0>(Context); break;
			case 1:	VertexShader = SetShader<1, 0>(Context); break;
			case 2:	VertexShader = SetShader<2, 0>(Context); break;
			case 3:	VertexShader = SetShader<3, 0>(Context); break;
			default:
				checkNoEntry();
				break;
		}
	}

	// Draw a quad, a triangle or a tessellated quad
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		VertexShader,
		bTessellatedQuad ? EDRF_UseTesselatedIndexBuffer: EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessUpscale::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("Upscale");

	return Ret;
}
