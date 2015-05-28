// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBokehDOFRecombine.cpp: Post processing lens blur implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessPassThrough.h"
#include "PostProcessing.h"
#include "PostProcessBokehDOFRecombine.h"
#include "PostProcessBokehDOF.h"
#include "SceneUtils.h"


/**
 * Encapsulates a shader to combined Depth of field and separate translucency layers.
 * @param Method 1:DOF, 2:SeparateTranslucency, 3:DOF+SeparateTranslucency
 */
template <uint32 Method>
class FPostProcessBokehDOFRecombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBokehDOFRecombinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RECOMBINE_METHOD"), Method);
	}

	/** Default constructor. */
	FPostProcessBokehDOFRecombinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;

	/** Initialization constructor. */
	FPostProcessBokehDOFRecombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DepthOfFieldParams << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessBokehDOF");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainRecombinePS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessBokehDOFRecombinePS<A> FPostProcessBokehDOFRecombinePS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessBokehDOFRecombinePS##A, SF_Pixel);

VARIATION1(1)			VARIATION1(2)			VARIATION1(3)
#undef VARIATION1


template <uint32 Method>
void FRCPassPostProcessBokehDOFRecombine::SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBokehDOFRecombinePS<Method> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}

void FRCPassPostProcessBokehDOFRecombine::Process(FRenderingCompositePassContext& Context)
{
	uint32 Method = 2;

	if(GetInput(ePId_Input1)->GetPass())
	{
		if(GetInput(ePId_Input2)->GetPass())
		{
			Method = 3;
		}
		else
		{
			Method = 1;
		}
	}
	else
	{
		check(GetInput(ePId_Input2)->GetPass());
	}

	SCOPED_DRAW_EVENTF(Context.RHICmdList, BokehDOFRecombine, TEXT("BokehDOFRecombine#%d"), Method);

	const FPooledRenderTargetDesc* InputDesc0 = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc* InputDesc1 = GetInputDesc(ePId_Input1);
	
	const FSceneView& View = Context.View;

	FIntPoint TexSize = InputDesc1 ? InputDesc1->Extent : InputDesc0->Extent;

	// usually 1, 2, 4 or 8
	uint32 ScaleToFullRes = GSceneRenderTargets.GetBufferSizeXY().X / TexSize.X;

	FIntRect HalfResViewRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes);

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, View.ViewRect);

	Context.SetViewportAndCallRHI(View.ViewRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	switch(Method)
		{
		case 1: SetShader<1>(Context); break;
		case 2: SetShader<2>(Context); break;
		case 3: SetShader<3>(Context); break;
		default:
			check(0);
	}

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		HalfResViewRect.Min.X, HalfResViewRect.Min.Y, 
		HalfResViewRect.Width(), HalfResViewRect.Height(),
		View.ViewRect.Size(),
		TexSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBokehDOFRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("BokehDOFRecombine");

	return Ret;
}
