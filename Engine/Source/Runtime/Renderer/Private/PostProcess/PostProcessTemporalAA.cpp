// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTemporalAA.cpp: Post process MotionBlur implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessTemporalAA.h"
#include "PostProcessAmbientOcclusion.h"
#include "PostProcessEyeAdaptation.h"
#include "PostProcessTonemap.h"
#include "PostProcessing.h"
#include "SceneUtils.h"

static float TemporalHalton( int32 Index, int32 Base )
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

static void TemporalRandom(FVector2D* RESTRICT const Constant, uint32 FrameNumber)
{
	Constant->X = TemporalHalton(FrameNumber & 1023, 2);
	Constant->Y = TemporalHalton(FrameNumber & 1023, 3);
}

static TAutoConsoleVariable<float> CVarTemporalAASharpness(
	TEXT("r.TemporalAASharpness"),
	1.0f,
	TEXT("Sharpness of temporal AA (0.0 = smoother, 1.0 = sharper)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTemporalAAPauseCorrect(
	TEXT("r.TemporalAAPauseCorrect"),
	1,
	TEXT("Correct temporal AA in pause. This holds onto render targets longer preventing reuse and consumes more memory."),
	ECVF_RenderThreadSafe);

static float CatmullRom( float x )
{
	float ax = FMath::Abs(x);
	if( ax > 1.0f )
		return ( ( -0.5f * ax + 2.5f ) * ax - 4.0f ) *ax + 2.0f;
	else
		return ( 1.5f * ax - 2.5f ) * ax*ax + 1.0f;
}

/** Encapsulates a TemporalAA pixel shader. */
template< uint32 Type, uint32 Responsive >
class FPostProcessTemporalAAPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTemporalAAPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("RESPONSIVE"), Responsive );
	}

	/** Default constructor. */
	FPostProcessTemporalAAPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter SampleWeights;
	FShaderParameter LowpassWeights;
	FShaderParameter PlusWeights;
	FShaderParameter RandomOffset;
	FShaderParameter DitherScale;
	FShaderParameter VelocityScaling;

	/** Initialization constructor. */
	FPostProcessTemporalAAPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SampleWeights.Bind(Initializer.ParameterMap, TEXT("SampleWeights"));
		LowpassWeights.Bind(Initializer.ParameterMap, TEXT("LowpassWeights"));
		PlusWeights.Bind(Initializer.ParameterMap, TEXT("PlusWeights"));
		RandomOffset.Bind(Initializer.ParameterMap, TEXT("RandomOffset"));
		DitherScale.Bind(Initializer.ParameterMap, TEXT("DitherScale"));
		VelocityScaling.Bind(Initializer.ParameterMap, TEXT("VelocityScaling"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SampleWeights << LowpassWeights << PlusWeights << RandomOffset << DitherScale << VelocityScaling;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context, bool bUseDither)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		
		FSamplerStateRHIParamRef FilterTable[4];
		FilterTable[0] = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		FilterTable[1] = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		FilterTable[2] = FilterTable[0];
		FilterTable[3] = FilterTable[0];

		PostprocessParameter.SetPS(ShaderRHI, Context, 0, eFC_0000, FilterTable);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

		float JitterX = Context.View.TemporalJitterPixelsX *  0.5f;
		float JitterY = Context.View.TemporalJitterPixelsY * -0.5f;

		{
			const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

			static const float SampleOffsets[9][2] =
			{
				{ -1.0f, -1.0f },
				{  0.0f, -1.0f },
				{  1.0f, -1.0f },
				{ -1.0f,  0.0f },
				{  0.0f,  0.0f },
				{  1.0f,  0.0f },
				{ -1.0f,  1.0f },
				{  0.0f,  1.0f },
				{  1.0f,  1.0f },
			};
		
			float Sharpness = CVarTemporalAASharpness.GetValueOnRenderThread();

			float Weights[9];
			float WeightsLow[9];
			float WeightsPlus[5];
			float TotalWeight = 0.0f;
			float TotalWeightLow = 0.0f;
			float TotalWeightPlus = 0.0f;
			for( int32 i = 0; i < 9; i++ )
			{
				float PixelOffsetX = SampleOffsets[i][0] - JitterX;
				float PixelOffsetY = SampleOffsets[i][1] - JitterY;

				if( Sharpness > 1.0f )
				{
					Weights[i] = CatmullRom( PixelOffsetX ) * CatmullRom( PixelOffsetY );
					TotalWeight += Weights[i];
				}
				else
				{
					// Exponential fit to Blackman-Harris 3.3
					PixelOffsetX *= 1.0f + Sharpness * 0.5f;
					PixelOffsetY *= 1.0f + Sharpness * 0.5f;
					Weights[i] = FMath::Exp( -2.29f * ( PixelOffsetX * PixelOffsetX + PixelOffsetY * PixelOffsetY ) );
					TotalWeight += Weights[i];
				}

				// Lowpass.
				PixelOffsetX = SampleOffsets[i][0] - JitterX;
				PixelOffsetY = SampleOffsets[i][1] - JitterY;
				PixelOffsetX *= 0.25f;
				PixelOffsetY *= 0.25f;
				PixelOffsetX *= 1.0f + Sharpness * 0.5f;
				PixelOffsetY *= 1.0f + Sharpness * 0.5f;
				WeightsLow[i] = FMath::Exp( -2.29f * ( PixelOffsetX * PixelOffsetX + PixelOffsetY * PixelOffsetY ) );
				TotalWeightLow += WeightsLow[i];
			}

			WeightsPlus[0] = Weights[1];
			WeightsPlus[1] = Weights[3];
			WeightsPlus[2] = Weights[4];
			WeightsPlus[3] = Weights[5];
			WeightsPlus[4] = Weights[7];
			TotalWeightPlus = Weights[1] + Weights[3] + Weights[4] + Weights[5] + Weights[7];
			
			for( int32 i = 0; i < 9; i++ )
			{
				SetShaderValue(Context.RHICmdList, ShaderRHI, SampleWeights, Weights[i] / TotalWeight, i );
				SetShaderValue(Context.RHICmdList, ShaderRHI, LowpassWeights, WeightsLow[i] / TotalWeightLow, i );
			}

			for( int32 i = 0; i < 5; i++ )
			{
				SetShaderValue(Context.RHICmdList, ShaderRHI, PlusWeights, WeightsPlus[i] / TotalWeightPlus, i );
			}
			
			FVector2D RandomOffsetValue;
			TemporalRandom(&RandomOffsetValue, Context.View.Family->FrameNumber);
			SetShaderValue(Context.RHICmdList, ShaderRHI, RandomOffset, RandomOffsetValue);

		}

		SetShaderValue(Context.RHICmdList, ShaderRHI, DitherScale, bUseDither ? 1.0f : 0.0f);

		const bool bIgnoreVelocity = (ViewState && ViewState->bSequencerIsPaused);
		SetShaderValue(Context.RHICmdList, ShaderRHI, VelocityScaling, bIgnoreVelocity ? 0.0f : 1.0f);

		SetUniformBufferParameter(Context.RHICmdList, ShaderRHI, GetUniformBufferParameter<FCameraMotionParameters>(), CreateCameraMotionParametersUniformBuffer(Context.View));
	}
};

// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(A, B, EntryName) \
	typedef FPostProcessTemporalAAPS<A,B> FPostProcessTemporalAAPS##A##B; \
	IMPLEMENT_SHADER_TYPE(template<>,FPostProcessTemporalAAPS##A##B,TEXT("PostProcessTemporalAA"),EntryName,SF_Pixel);

IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(0, 0, TEXT("DOFTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(1, 0, TEXT("MainTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(1, 1, TEXT("MainTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(2, 0, TEXT("SSRTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(3, 0, TEXT("LightShaftTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(4, 0, TEXT("MainFastTemporalAAPS"));
IMPLEMENT_TEMPORALAA_PIXELSHADER_TYPE(4, 1, TEXT("MainFastTemporalAAPS"));

void FRCPassPostProcessSSRTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SSRTemporalAA);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, (float)ERHIZBuffer::FarPlane, false, 0, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
	TShaderMapRef< FPostProcessTemporalAAPS<2, 0> >	PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetParameters(Context, false);

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Size(),
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSSRTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.DebugName = TEXT("SSRTemporalAA");
	Ret.AutoWritable = false;
	return Ret;
}

void FRCPassPostProcessDOFTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, DOFTemporalAA);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = Context.ViewState;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, (float)ERHIZBuffer::FarPlane, false, 0, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
	TShaderMapRef< FPostProcessTemporalAAPS<0, 0> >	PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetParameters(Context, false);

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Size(),
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	ViewState->DOFHistoryRT = PassOutputs[0].PooledRenderTarget;
	check( ViewState->DOFHistoryRT );
}

FPooledRenderTargetDesc FRCPassPostProcessDOFTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.AutoWritable = false;
	Ret.DebugName = TEXT("BokehDOFTemporalAA");

	return Ret;
}


void FRCPassPostProcessDOFTemporalAANear::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, DOFTemporalAANear);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = Context.ViewState;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, (float)ERHIZBuffer::FarPlane, false, 0, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
	TShaderMapRef< FPostProcessTemporalAAPS<0, 0> >	PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;


	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetParameters(Context, false);

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Size(),
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	ViewState->DOFHistoryRT2 = PassOutputs[0].PooledRenderTarget;
	check( ViewState->DOFHistoryRT2 );
}

FPooledRenderTargetDesc FRCPassPostProcessDOFTemporalAANear::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.DebugName = TEXT("BokehDOFTemporalAANear");

	return Ret;
}



void FRCPassPostProcessLightShaftTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = Context.ViewState;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, (float)ERHIZBuffer::FarPlane, false, 0, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
	TShaderMapRef< FPostProcessTemporalAAPS<3, 0> >	PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetParameters(Context, false);

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Size(),
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessLightShaftTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.DebugName = TEXT("LightShaftTemporalAA");

	return Ret;
}


void FRCPassPostProcessTemporalAA::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	const FViewInfo& View = Context.View;
	FSceneViewState* ViewState = Context.ViewState;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = SceneContext.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, TemporalAA, TEXT("TemporalAA %dx%d"), SrcRect.Width(), SrcRect.Height());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	//Context.SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, SceneContext.GetSceneDepthTexture(), ESimpleRenderTargetMode::EUninitializedColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, (float)ERHIZBuffer::FarPlane, false, 0, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);


	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessAAQuality")); 
	uint32 Quality = FMath::Clamp(CVar->GetValueOnRenderThread(), 1, 6);
	bool bUseFast = Quality == 3;

	// Only use dithering if we are outputting to a low precision format
	const bool bUseDither = PassOutputs[0].RenderTargetDesc.Format != PF_FloatRGBA;

	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());

	if(Context.View.bCameraCut)
	{
		// On camera cut this turns on responsive everywhere.
		
		// Normal temporal feedback
		Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

		TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
		if (bUseFast)
		{
			TShaderMapRef< FPostProcessTemporalAAPS<4, 1> >	PixelShader(Context.GetShaderMap());

			static FGlobalBoundShaderState BoundShaderState;
			

			SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

			VertexShader->SetVS(Context);
			PixelShader->SetParameters(Context, bUseDither);
		}
		else
		{
			TShaderMapRef< FPostProcessTemporalAAPS<1, 1> >	PixelShader(Context.GetShaderMap());

			static FGlobalBoundShaderState BoundShaderState;
			

			SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

			VertexShader->SetVS(Context);
			PixelShader->SetParameters(Context, bUseDither);
		}
	
		DrawPostProcessPass(
			Context.RHICmdList,
			0, 0,
			SrcRect.Width(), SrcRect.Height(),
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			SrcRect.Size(),
			SrcSize,
			*VertexShader,
			View.StereoPass,
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);
	}
	else
	{
		{	
			// Normal temporal feedback
			// Draw to pixels where stencil == 0
			Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always,true,CF_Equal,SO_Keep,SO_Keep,SO_Keep>::GetRHI(), 0);
	
			TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
			if (bUseFast)
			{
				TShaderMapRef< FPostProcessTemporalAAPS<4, 0> >	PixelShader(Context.GetShaderMap());
	
				static FGlobalBoundShaderState BoundShaderState;
				
	
				SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
	
				VertexShader->SetVS(Context);
				PixelShader->SetParameters(Context, bUseDither);
			}
			else
			{
				TShaderMapRef< FPostProcessTemporalAAPS<1, 0> >	PixelShader(Context.GetShaderMap());
	
				static FGlobalBoundShaderState BoundShaderState;
				
	
				SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
	
				VertexShader->SetVS(Context);
				PixelShader->SetParameters(Context, bUseDither);
			}
		
			DrawPostProcessPass(
				Context.RHICmdList,
				0, 0,
				SrcRect.Width(), SrcRect.Height(),
				SrcRect.Min.X, SrcRect.Min.Y,
				SrcRect.Width(), SrcRect.Height(),
				SrcRect.Size(),
				SrcSize,
				*VertexShader,
				View.StereoPass,
				Context.HasHmdMesh(),
				EDRF_UseTriangleOptimization);
		}
	
		{	// Responsive feedback for tagged pixels
			// Draw to pixels where stencil != 0
			Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always,true,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep>::GetRHI(), 0);
			
			TShaderMapRef< FPostProcessTonemapVS >			VertexShader(Context.GetShaderMap());
			if(bUseFast)
			{
				TShaderMapRef< FPostProcessTemporalAAPS<4, 1> >	PixelShader(Context.GetShaderMap());
	
				static FGlobalBoundShaderState BoundShaderState;
				
	
				SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
	
				VertexShader->SetVS(Context);
				PixelShader->SetParameters(Context, bUseDither);
			}
			else
			{
				TShaderMapRef< FPostProcessTemporalAAPS<1, 1> >	PixelShader(Context.GetShaderMap());
	
				static FGlobalBoundShaderState BoundShaderState;
				
	
				SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
	
				VertexShader->SetVS(Context);
				PixelShader->SetParameters(Context, bUseDither);
			}
	
			DrawPostProcessPass(
				Context.RHICmdList,
				0, 0,
				SrcRect.Width(), SrcRect.Height(),
				SrcRect.Min.X, SrcRect.Min.Y,
				SrcRect.Width(), SrcRect.Height(),
				SrcRect.Size(),
				SrcSize,
				*VertexShader,
				View.StereoPass,
				Context.HasHmdMesh(),
				EDRF_UseTriangleOptimization);
		}
	}

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	if( CVarTemporalAAPauseCorrect.GetValueOnRenderThread() )
	{
		ViewState->PendingTemporalAAHistoryRT = PassOutputs[0].PooledRenderTarget;
	}
	else
	{
		ViewState->TemporalAAHistoryRT = PassOutputs[0].PooledRenderTarget;
	}

	if (FSceneRenderer::ShouldCompositeEditorPrimitives(Context.View))
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::WaitForOutstandingTasksOnly);
		// because of the flush it's ok to remove the const, this is not ideal as the flush can cost performance
		FViewInfo& NonConstView = (FViewInfo&)Context.View;

		// Remove jitter
		NonConstView.ViewMatrices.RemoveTemporalJitter();

		// Compute the view projection matrix and its inverse.
		NonConstView.ViewProjectionMatrix = NonConstView.ViewMatrices.ViewMatrix * NonConstView.ViewMatrices.ProjMatrix;
		NonConstView.InvViewProjectionMatrix = NonConstView.ViewMatrices.GetInvProjMatrix() * NonConstView.InvViewMatrix;

		// Compute a transform from view origin centered world-space to clip space.
		NonConstView.ViewMatrices.TranslatedViewProjectionMatrix = NonConstView.ViewMatrices.TranslatedViewMatrix * NonConstView.ViewMatrices.ProjMatrix;
		NonConstView.ViewMatrices.InvTranslatedViewProjectionMatrix = NonConstView.ViewMatrices.GetInvProjMatrix() * NonConstView.ViewMatrices.TranslatedViewMatrix.Inverse();

		NonConstView.InitRHIResources();
	}
}

FPooledRenderTargetDesc FRCPassPostProcessTemporalAA::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Reset();
	//regardless of input type, PF_FloatRGBA is required to properly accumulate between frames for a good result.
	Ret.Format = PF_FloatRGBA;
	Ret.DebugName = TEXT("TemporalAA");
	Ret.AutoWritable = false;

	return Ret;
}
