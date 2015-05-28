// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBusyWait.cpp: Post processing busy wait implementation. For Debugging GPU timing.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessBusyWait.h"
#include "PostProcessing.h"
#include "SceneUtils.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<float> CVarSetGPUBusyWait(
	TEXT("r.GPUBusyWait"),
	0.0f,
	TEXT("<=0:off, >0: keep the GPU busy with n units of some fixed amount of work, independent on the resolution\n")
	TEXT("This can be useful to make GPU timing experiments. The value should roughly represent milliseconds.\n")
	TEXT("Clamped at 500."),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif

/** Encapsulates the post processing busy wait pixel shader. */
class FPostProcessBusyWaitPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBusyWaitPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessBusyWaitPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter GPUBusyWait;

	/** Initialization constructor. */
	FPostProcessBusyWaitPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		GPUBusyWait.Bind(Initializer.ParameterMap,TEXT("GPUBusyWait"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		{
			uint32 PixelCount = Context.View.ViewRect.Size().X * Context.View.ViewRect.Size().Y;

			float CVarValue = FMath::Clamp(CVarSetGPUBusyWait.GetValueOnRenderThread(), 0.0f, 500.0f);

			// multiply with large number to get more human friendly number range
			// calibrated on a NV580 to be roughly a millisecond
			// divide by viewport pixel count
			uint32 Value = (uint32)(CVarValue * 1000000000.0 / 6.12 / PixelCount);

			SetShaderValue(Context.RHICmdList, ShaderRHI, GPUBusyWait, Value);
		}
#endif
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << GPUBusyWait;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBusyWaitPS,TEXT("PostProcessBusyWait"),TEXT("MainPS"),SF_Pixel);

void FRCPassPostProcessBusyWait::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, BusyWait);

	const FSceneView& View = Context.View;
	
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.UnscaledViewRect;
	
	GSceneRenderTargets.BeginRenderingLightAttenuation(Context.RHICmdList);
	
	const FSceneRenderTargetItem& DestRenderTarget = GSceneRenderTargets.GetLightAttenuation()->GetRenderTargetItem();

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(DestRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBusyWaitPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetPS(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcRect.Size(),
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	GSceneRenderTargets.SetLightAttenuation(0);
}

FPooledRenderTargetDesc FRCPassPostProcessBusyWait::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("BusyWait");

	return FPooledRenderTargetDesc();
}

bool FRCPassPostProcessBusyWait::IsPassRequired()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.GPUBusyWait"));

	float Value = CVar->GetValueOnAnyThread();

	return Value > 0;
#else
	return false;
#endif
}
