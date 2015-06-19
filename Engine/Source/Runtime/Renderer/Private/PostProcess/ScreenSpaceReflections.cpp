// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenSpaceReflections.cpp: Post processing Screen Space Reflections implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessing.h"
#include "ScreenSpaceReflections.h"
#include "PostProcessTemporalAA.h"
#include "PostProcessAmbientOcclusion.h"
#include "SceneUtils.h"

static TAutoConsoleVariable<int32> CVarSSRQuality(
	TEXT("r.SSR.Quality"),
	3,
	TEXT("Whether to use screen space reflections and at what quality setting.\n")
	TEXT("(limits the setting in the post process settings which has a different scale)\n")
	TEXT("(costs performance, adds more visual realism but the technique has limits)\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: low (no glossy)\n")
	TEXT(" 2: medium (no glossy)\n")
	TEXT(" 3: high (glossy/using roughness, few samples)\n")
	TEXT(" 4: very high (likely too slow for real-time)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSSRTemporal(
	TEXT("r.SSR.Temporal"),
	0,
	TEXT("Defines if we use the temporal smoothing for the screen space reflection\n")
	TEXT(" 0 is off (for debugging), 1 is on (default)"),
	ECVF_RenderThreadSafe);

bool DoScreenSpaceReflections(const FViewInfo& View)
{
	if(!View.Family->EngineShowFlags.ScreenSpaceReflections)
	{
		return false;
	}

	if(!View.State)
	{
		// not view state (e.g. thumbnail rendering?), no HZB (no screen space reflections or occlusion culling)
		return false;
	}

	int SSRQuality = CVarSSRQuality.GetValueOnRenderThread();

	if(SSRQuality <= 0)
	{
		return false;
	}

	if(View.FinalPostProcessSettings.ScreenSpaceReflectionIntensity < 1.0f)
	{
		return false;
	}

	return true;
}

/**
 * Encapsulates the post processing screen space reflections pixel shader.
 * @param SSRQuality 0:Visualize Mask
 */
template<uint32 PrevFrame, uint32 SSRQuality >
class FPostProcessScreenSpaceReflectionsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessScreenSpaceReflectionsPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("PREV_FRAME_COLOR"), PrevFrame );
		OutEnvironment.SetDefine( TEXT("SSR_QUALITY"), SSRQuality );
	}

	/** Default constructor. */
	FPostProcessScreenSpaceReflectionsPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter SSRParams;

	/** Initialization constructor. */
	FPostProcessScreenSpaceReflectionsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SSRParams.Bind(Initializer.ParameterMap, TEXT("SSRParams"));
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			float MaxRoughness = FMath::Clamp(Context.View.FinalPostProcessSettings.ScreenSpaceReflectionMaxRoughness, 0.01f, 1.0f);

			// f(x) = x * Scale + Bias
			// f(MaxRoughness) = 0
			// f(MaxRoughness/2) = 1

			float RoughnessMaskScale = -2.0f / MaxRoughness;
			RoughnessMaskScale *= SSRQuality < 3 ? 2.0f : 1.0f;

			FLinearColor Value(
				FMath::Clamp(Context.View.FinalPostProcessSettings.ScreenSpaceReflectionIntensity * 0.01f, 0.0f, 1.0f), 
				RoughnessMaskScale,
				0, 
				0);

			SetShaderValue(Context.RHICmdList, ShaderRHI, SSRParams, Value);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SSRParams;
		return bShaderHasOutdatedParameters;
	}
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(A, B) \
	typedef FPostProcessScreenSpaceReflectionsPS<A,B> FPostProcessScreenSpaceReflectionsPS##A##B; \
	IMPLEMENT_SHADER_TYPE(template<>,FPostProcessScreenSpaceReflectionsPS##A##B,TEXT("ScreenSpaceReflections"),TEXT("ScreenSpaceReflectionsPS"),SF_Pixel)

IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,0);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,1);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,1);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,2);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,2);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,3);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,3);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(0,4);
IMPLEMENT_REFLECTION_PIXELSHADER_TYPE(1,4);

// --------------------------------------------------------


// @param Quality usually in 0..100 range, default is 50
// @return see CVarSSRQuality, never 0
static int32 ComputeSSRQuality(float Quality)
{
	int32 Ret;

	if(Quality >= 60.0f)
	{
		Ret = (Quality >= 80.0f) ? 4 : 3;
	}
	else
	{
		Ret = (Quality >= 40.0f) ? 2 : 1;
	}

	int SSRQualityCVar = FMath::Max(0, CVarSSRQuality.GetValueOnRenderThread());

	return FMath::Min(Ret, SSRQualityCVar);
}

void FRCPassPostProcessScreenSpaceReflections::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, ScreenSpaceReflections);

	const FSceneView& View = Context.View;
	const auto FeatureLevel = Context.GetFeatureLevel();
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 1.0f, false, 0, FIntRect());
	Context.SetViewportAndCallRHI(View.ViewRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	int SSRQuality = ComputeSSRQuality(View.FinalPostProcessSettings.ScreenSpaceReflectionQuality);

	SSRQuality = FMath::Clamp(SSRQuality, 1, 4);
	

	uint32 iPreFrame = bPrevFrame ? 1 : 0;

	if (View.Family->EngineShowFlags.VisualizeSSR)
	{
		iPreFrame = 0;
		SSRQuality = 0;
	}

	TShaderMapRef< FPostProcessVS > VertexShader(Context.GetShaderMap());

	#define CASE(A, B) \
		case (A + 2 * (B + 3 * 0 )): \
		{ \
			TShaderMapRef< FPostProcessScreenSpaceReflectionsPS<A, B> > PixelShader(Context.GetShaderMap()); \
			static FGlobalBoundShaderState BoundShaderState; \
			SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader); \
			VertexShader->SetParameters(Context); \
			PixelShader->SetParameters(Context); \
		}; \
		break

	switch (iPreFrame + 2 * (SSRQuality + 3 * 0))
	{
		CASE(0,0);
		CASE(0,1);	CASE(1,1);
		CASE(0,2);	CASE(1,2);
		CASE(0,3);	CASE(1,3);
		CASE(0,4);	CASE(1,4);
		default:
			check(!"Missing case in FRCPassPostProcessScreenSpaceReflections");
	}
	#undef CASE


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

FPooledRenderTargetDesc FRCPassPostProcessScreenSpaceReflections::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(GSceneRenderTargets.GetBufferSizeXY(), PF_FloatRGBA, TexCreate_None, TexCreate_RenderTargetable, false));

	Ret.DebugName = TEXT("ScreenSpaceReflections");

	return Ret;
}

void ScreenSpaceReflections(FRHICommandListImmediate& RHICmdList, FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& SSROutput)
{
	FRenderingCompositePassContext CompositeContext(RHICmdList, View);
	FPostprocessContext Context( CompositeContext.Graph, View );

	FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

	FRenderingCompositePass* SceneColorInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( GSceneRenderTargets.GetSceneColor() ) );
	FRenderingCompositePass* HZBInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( View.HZB ) );

	bool bPrevFrame = 0;
	if( ViewState && ViewState->TemporalAAHistoryRT && !Context.View.bCameraCut )
	{
		SceneColorInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( ViewState->TemporalAAHistoryRT ) );
		bPrevFrame = 1;
	}

	{
		FRenderingCompositePass* TracePass = Context.Graph.RegisterPass( new FRCPassPostProcessScreenSpaceReflections( bPrevFrame ) );
		TracePass->SetInput( ePId_Input0, SceneColorInput );
		TracePass->SetInput( ePId_Input1, HZBInput );

		Context.FinalOutput = FRenderingCompositeOutputRef( TracePass );
	}

	const bool bTemporalFilter = View.FinalPostProcessSettings.AntiAliasingMethod != AAM_TemporalAA || CVarSSRTemporal.GetValueOnRenderThread() != 0;

	if( ViewState && bTemporalFilter )
	{
		{
			FRenderingCompositeOutputRef HistoryInput;
			if( ViewState && ViewState->SSRHistoryRT && !Context.View.bCameraCut )
			{
				HistoryInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( ViewState->SSRHistoryRT ) );
			}
			else
			{
				// No history, use black
				HistoryInput = Context.Graph.RegisterPass(new FRCPassPostProcessInput(GSystemTextures.BlackDummy));
			}

			FRenderingCompositePass* TemporalAAPass = Context.Graph.RegisterPass( new FRCPassPostProcessSSRTemporalAA );
			TemporalAAPass->SetInput( ePId_Input0, Context.FinalOutput );
			TemporalAAPass->SetInput( ePId_Input1, HistoryInput );
			//TemporalAAPass->SetInput( ePId_Input2, VelocityInput );

			Context.FinalOutput = FRenderingCompositeOutputRef( TemporalAAPass );
		}

		if( ViewState )
		{
			FRenderingCompositePass* HistoryOutput = Context.Graph.RegisterPass( new FRCPassPostProcessOutput( &ViewState->SSRHistoryRT ) );
			HistoryOutput->SetInput( ePId_Input0, Context.FinalOutput );

			Context.FinalOutput = FRenderingCompositeOutputRef( HistoryOutput );
		}
	}

	{
		FRenderingCompositePass* ReflectionOutput = Context.Graph.RegisterPass( new FRCPassPostProcessOutput( &SSROutput ) );
		ReflectionOutput->SetInput( ePId_Input0, Context.FinalOutput );

		Context.FinalOutput = FRenderingCompositeOutputRef( ReflectionOutput );
	}

	CompositeContext.Root->AddDependency( Context.FinalOutput );
	CompositeContext.Process(TEXT("ReflectionEnvironments"));
}
