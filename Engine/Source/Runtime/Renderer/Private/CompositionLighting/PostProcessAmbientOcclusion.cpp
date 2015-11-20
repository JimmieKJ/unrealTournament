// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbientOcclusion.cpp: Post processing ambient occlusion implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessing.h"
#include "PostProcessAmbientOcclusion.h"
#include "SceneUtils.h"

static TAutoConsoleVariable<float> CVarAmbientOcclusionMaxQuality(
	TEXT("r.AmbientOcclusionMaxQuality"),
	100.0f,
	TEXT("Defines the max clamping value from the post process volume's quality level for ScreenSpace Ambient Occlusion\n")
	TEXT("     100: don't override quality level from the post process volume (default)\n")
	TEXT("   0..99: clamp down quality level from the post process volume to the maximum set by this cvar\n")
	TEXT(" -100..0: Enforces a different quality (the absolute value) even if the postprocessvolume asks for a lower quality."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarAmbientOcclusionStepMipLevelFactor(
	TEXT("r.AmbientOcclusionMipLevelFactor"),
	0.5f,
	TEXT("Controle mipmap level according to the SSAO step id\n")
	TEXT(" 0: always look into the HZB mipmap level 0 (memory cache trashing)\n")
	TEXT(" 0.5: sample count depends on post process settings (default)\n")
	TEXT(" 1: Go into higher mipmap level (quality loss)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FCameraMotionParameters,TEXT("CameraMotion"));

/** Encapsulates the post processing ambient occlusion pixel shader. */
template <uint32 bInitialPass>
class FPostProcessAmbientOcclusionSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessAmbientOcclusionSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INITIAL_PASS"), bInitialPass);
	}

	/** Default constructor. */
	FPostProcessAmbientOcclusionSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter AmbientOcclusionSetupParams;

	/** Initialization constructor. */
	FPostProcessAmbientOcclusionSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		AmbientOcclusionSetupParams.Bind(Initializer.ParameterMap, TEXT("AmbientOcclusionSetupParams"));
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		// e.g. 4 means the input texture is 4x smaller than the buffer size
		uint32 ScaleToFullRes = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / Context.Pass->GetOutput(ePId_Output0)->RenderTargetDesc.Extent.X;

		// /1000 to be able to define the value in that distance
		FVector4 AmbientOcclusionSetupParamsValue = FVector4(ScaleToFullRes, Settings.AmbientOcclusionMipThreshold / ScaleToFullRes, 0, 0);
		SetShaderValue(Context.RHICmdList, ShaderRHI, AmbientOcclusionSetupParams, AmbientOcclusionSetupParamsValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << AmbientOcclusionSetupParams;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessAmbientOcclusion");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainSetupPS");
	}
};


// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessAmbientOcclusionSetupPS<A> FPostProcessAmbientOcclusionSetupPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessAmbientOcclusionSetupPS##A, SF_Pixel);

	VARIATION1(0)			VARIATION1(1)
#undef VARIATION1

// --------------------------------------------------------


FArchive& operator<<(FArchive& Ar, FScreenSpaceAOandSSRShaderParameters& This)
{
	Ar << This.ScreenSpaceAOandSSRShaderParams;

	return Ar;
}

// --------------------------------------------------------

/**
 * Encapsulates the post processing ambient occlusion pixel shader.
 * @param bAOSetupAsInput true:use AO setup instead of full resolution depth and normal
 * @param bDoUpsample true:we have lower resolution pass data we need to upsample, false otherwise
 * @param ShaderQuality 0..4, 0:low 4:high
 */
template<uint32 bTAOSetupAsInput, uint32 bDoUpsample, uint32 ShaderQuality>
class FPostProcessAmbientOcclusionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessAmbientOcclusionPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);

		OutEnvironment.SetDefine(TEXT("USE_UPSAMPLE"), bDoUpsample);
		OutEnvironment.SetDefine(TEXT("USE_AO_SETUP_AS_INPUT"), bTAOSetupAsInput);
		OutEnvironment.SetDefine(TEXT("SHADER_QUALITY"), ShaderQuality);
	}

	/** Default constructor. */
	FPostProcessAmbientOcclusionPS() {}

public:
	FShaderParameter HzbUvAndStepMipLevelFactor;
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FScreenSpaceAOandSSRShaderParameters ScreenSpaceAOandSSRShaderParams;
	FShaderResourceParameter RandomNormalTexture;
	FShaderResourceParameter RandomNormalTextureSampler;

	/** Initialization constructor. */
	FPostProcessAmbientOcclusionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		ScreenSpaceAOandSSRShaderParams.Bind(Initializer.ParameterMap);
		RandomNormalTexture.Bind(Initializer.ParameterMap, TEXT("RandomNormalTexture"));
		RandomNormalTextureSampler.Bind(Initializer.ParameterMap, TEXT("RandomNormalTextureSampler"));
		HzbUvAndStepMipLevelFactor.Bind(Initializer.ParameterMap, TEXT("HzbUvAndStepMipLevelFactor"));
	}

	void SetParameters(const FRenderingCompositePassContext& Context, FIntPoint InputTextureSize)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		// SF_Point is better than bilinear to avoid halos around objects
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		const FSceneRenderTargetItem& SSAORandomization = GSystemTextures.SSAORandomization->GetRenderTargetItem();

		SetTextureParameter(Context.RHICmdList, ShaderRHI, RandomNormalTexture, RandomNormalTextureSampler, TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI(), SSAORandomization.ShaderResourceTexture);

		ScreenSpaceAOandSSRShaderParams.Set(Context.RHICmdList, Context.View, ShaderRHI, InputTextureSize);
		
		const float HzbStepMipLevelFactorValue = FMath::Clamp(CVarAmbientOcclusionStepMipLevelFactor.GetValueOnRenderThread(), 0.0f, 1.0f);
		const FVector HzbUvAndStepMipLevelFactorValue(
			float(Context.View.ViewRect.Width()) / float(2 * Context.View.HZBMipmap0Size.X),
			float(Context.View.ViewRect.Height()) / float(2 * Context.View.HZBMipmap0Size.Y),
			HzbStepMipLevelFactorValue
			);
		
		SetShaderValue(Context.RHICmdList, ShaderRHI, HzbUvAndStepMipLevelFactor, HzbUvAndStepMipLevelFactorValue);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << HzbUvAndStepMipLevelFactor << PostprocessParameter << DeferredParameters << ScreenSpaceAOandSSRShaderParams << RandomNormalTexture << RandomNormalTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessAmbientOcclusion");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};


// #define avoids a lot of code duplication
#define VARIATION0(C)	    VARIATION1(0, C) VARIATION1(1, C)
#define VARIATION1(A, C)	VARIATION2(A, 0, C) VARIATION2(A, 1, C)
#define VARIATION2(A, B, C) typedef FPostProcessAmbientOcclusionPS<A, B, C> FPostProcessAmbientOcclusionPS##A##B##C; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessAmbientOcclusionPS##A##B##C, SF_Pixel);

	VARIATION0(0)
	VARIATION0(1)
	VARIATION0(2)
	VARIATION0(3)
	VARIATION0(4)
	
#undef VARIATION0
#undef VARIATION1
#undef VARIATION2


// --------------------------------------------------------


/** Encapsulates the post processing ambient occlusion pixel shader. */
class FPostProcessBasePassAOPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBasePassAOPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessBasePassAOPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FScreenSpaceAOandSSRShaderParameters ScreenSpaceAOandSSRShaderParams;

	/** Initialization constructor. */
	FPostProcessBasePassAOPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		ScreenSpaceAOandSSRShaderParams.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FRenderingCompositePassContext& Context, FIntPoint InputTextureSize)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		ScreenSpaceAOandSSRShaderParams.Set(Context.RHICmdList, Context.View, ShaderRHI, InputTextureSize);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << ScreenSpaceAOandSSRShaderParams;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBasePassAOPS,TEXT("PostProcessAmbientOcclusion"),TEXT("BasePassAOPS"),SF_Pixel);

// --------------------------------------------------------

template <uint32 bInitialSetup>
FShader* FRCPassPostProcessAmbientOcclusionSetup::SetShaderSetupTempl(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessAmbientOcclusionSetupPS<bInitialSetup> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context);

	return *VertexShader;
}

void FRCPassPostProcessAmbientOcclusionSetup::Process(FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / DestSize.X;

	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = SrcRect  / ScaleFactor;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, AmbientOcclusionSetup, TEXT("AmbientOcclusionSetup %dx%d"), DestRect.Width(), DestRect.Height());

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef());

	Context.SetViewportAndCallRHI(DestRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	FShader* VertexShader = 0;

	if(IsInitialPass())
	{
		VertexShader = SetShaderSetupTempl<1>(Context);
	}
	else
	{
		VertexShader = SetShaderSetupTempl<0>(Context);
	}

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y, 
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY(),
		VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessAmbientOcclusionSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	
	if(IsInitialPass())
	{
		Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	}
	else
	{
		Ret = GetInput(ePId_Input1)->GetOutput()->RenderTargetDesc;
	}

	Ret.Reset();
	Ret.Format = PF_FloatRGBA;
	Ret.ClearValue = FClearValueBinding::None;
	Ret.TargetableFlags &= ~TexCreate_DepthStencilTargetable;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.Extent = FIntPoint::DivideAndRoundUp(Ret.Extent, 2);

	Ret.DebugName = TEXT("AmbientOcclusionSetup");
	
	return Ret;
}

bool FRCPassPostProcessAmbientOcclusionSetup::IsInitialPass() const
{
	const FPooledRenderTargetDesc* InputDesc0 = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc* InputDesc1 = GetInputDesc(ePId_Input1);

	if(!InputDesc0 && InputDesc1)
	{
		return false;
	}
	if(InputDesc0 && !InputDesc1)
	{
		return true;
	}
	// internal error, SetInput() was done wrong
	check(0);
	return false;
}


// ---------------------------------
template <uint32 bTAOSetupAsInput, uint32 bDoUpsample, uint32 ShaderQuality>
FShader* FRCPassPostProcessAmbientOcclusion::SetShaderTempl(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessAmbientOcclusionPS<bTAOSetupAsInput, bDoUpsample, ShaderQuality> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	const FPooledRenderTargetDesc* InputDesc0 = GetInputDesc(ePId_Input0);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context, InputDesc0->Extent);

	return *VertexShader;
}

float GetAmbientOcclusionQualityRT(const FSceneView& View)
{
	float CVarValue = CVarAmbientOcclusionMaxQuality.GetValueOnRenderThread();

	if(CVarValue < 0)
	{
		return FMath::Clamp(-CVarValue, 0.0f, 100.0f);
	}
	else
	{
		return FMath::Min(CVarValue, View.FinalPostProcessSettings.AmbientOcclusionQuality);
	}
}

void FRCPassPostProcessAmbientOcclusion::Process(FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;

	const FPooledRenderTargetDesc* InputDesc0 = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc* InputDesc2 = GetInputDesc(ePId_Input2);

	const FSceneRenderTargetItem* DestRenderTarget = 0;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	if(bAOSetupAsInput)
	{
		DestRenderTarget = &PassOutputs[0].RequestSurface(Context);
	}
	else
	{
		DestRenderTarget = &SceneContext.ScreenSpaceAO->GetRenderTargetItem();
	}

	ensure(InputDesc0);

	FIntPoint TexSize = InputDesc0->Extent;

	// usually 1, 2, 4 or 8
	uint32 ScaleToFullRes = SceneContext.GetBufferSizeXY().X / TexSize.X;

	FIntRect ViewRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget->TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(ViewRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	float QualityPercent = GetAmbientOcclusionQualityRT(Context.View);
	
	// 0..4, 0:low 4:high
	const int32 ShaderQuality = 
		(QualityPercent > 75.0f) +
		(QualityPercent > 55.0f) +
		(QualityPercent > 25.0f) +
		(QualityPercent > 5.0f);

	bool bDoUpsample = (InputDesc2 != 0);
	
	SCOPED_DRAW_EVENTF(Context.RHICmdList, AmbientOcclusion, TEXT("AmbientOcclusion %dx%d SetupAsInput=%d Upsample=%d ShaderQuality=%d"), 
		ViewRect.Width(), ViewRect.Height(), bAOSetupAsInput, bDoUpsample, ShaderQuality);

	FShader* VertexShader = 0;

#define SET_SHADER_CASE(ShaderQualityCase) \
	case ShaderQualityCase: \
	if(bAOSetupAsInput) \
    { \
		if(bDoUpsample) VertexShader = SetShaderTempl<1, 1, ShaderQualityCase>(Context); \
		else VertexShader = SetShaderTempl<1, 0, ShaderQualityCase>(Context); \
	} \
	else \
    { \
		if(bDoUpsample) VertexShader = SetShaderTempl<0, 1, ShaderQualityCase>(Context); \
		else VertexShader = SetShaderTempl<0, 0, ShaderQualityCase>(Context); \
	} \
	break

	switch(ShaderQuality)
	{
		SET_SHADER_CASE(0);
		SET_SHADER_CASE(1);
		SET_SHADER_CASE(2);
		SET_SHADER_CASE(3);
		SET_SHADER_CASE(4);
		default:
			break;
	};

#undef SET_SHADER_CASE

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		ViewRect.Width(), ViewRect.Height(),
		ViewRect.Min.X, ViewRect.Min.Y,
		ViewRect.Width(), ViewRect.Height(),
		ViewRect.Size(),
		TexSize,
		VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);
	
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget->TargetableTexture, DestRenderTarget->ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessAmbientOcclusion::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	if(!bAOSetupAsInput)
	{
		FPooledRenderTargetDesc Ret;

		Ret.DebugName = TEXT("AmbientOcclusionDirect");

		// we render directly to the buffer, no need for an intermediate target, we output in a single channel
		return Ret;
	}

	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	// R:AmbientOcclusion, GBA:used for normal
	Ret.Format = PF_B8G8R8A8;
	Ret.TargetableFlags &= ~TexCreate_DepthStencilTargetable;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.DebugName = TEXT("AmbientOcclusion");

	return Ret;
}

// --------------------------------------------------------

void FRCPassPostProcessBasePassAO::Process(FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, ApplyAOToBasePassSceneColor, TEXT("ApplyAOToBasePassSceneColor %dx%d"), View.ViewRect.Width(), View.ViewRect.Height());

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	const FSceneRenderTargetItem& DestRenderTarget = SceneContext.GetSceneColor()->GetRenderTargetItem();

	// Set the view family's render target/viewport.
	Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, DestRenderTarget.TargetableTexture);
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture,	FTextureRHIParamRef(), ESimpleRenderTargetMode::EExistingColorAndDepth);
	Context.SetViewportAndCallRHI(View.ViewRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_DestAlpha, BF_Zero>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBasePassAOPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context, SceneContext.GetBufferSizeXY());

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		SceneContext.GetBufferSizeXY(),
		*VertexShader,
		View.StereoPass, 
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBasePassAO::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// we assume this pass is additively blended with the scene color so this data is not needed
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("SceneColorWithAO");

	return Ret;
}

TUniformBufferRef<FCameraMotionParameters> CreateCameraMotionParametersUniformBuffer(const FSceneView& View)
{
	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	FMatrix Proj = View.ViewMatrices.GetProjNoAAMatrix();
	FMatrix PrevProj = ViewState->PrevViewMatrices.GetProjNoAAMatrix();

	FVector DeltaTranslation = ViewState->PrevViewMatrices.PreViewTranslation - View.ViewMatrices.PreViewTranslation;
	FMatrix ViewProj = ( View.ViewMatrices.TranslatedViewMatrix * Proj ).GetTransposed();
	FMatrix PrevViewProj = ( FTranslationMatrix(DeltaTranslation) * ViewState->PrevViewMatrices.TranslatedViewMatrix * PrevProj ).GetTransposed();

	double InvViewProj[16];
	Inverse4x4( InvViewProj, (float*)ViewProj.M );

	const float* p = (float*)PrevViewProj.M;

	const double cxx = InvViewProj[ 0]; const double cxy = InvViewProj[ 1]; const double cxz = InvViewProj[ 2]; const double cxw = InvViewProj[ 3];
	const double cyx = InvViewProj[ 4]; const double cyy = InvViewProj[ 5]; const double cyz = InvViewProj[ 6]; const double cyw = InvViewProj[ 7];
	const double czx = InvViewProj[ 8]; const double czy = InvViewProj[ 9]; const double czz = InvViewProj[10]; const double czw = InvViewProj[11];
	const double cwx = InvViewProj[12]; const double cwy = InvViewProj[13]; const double cwz = InvViewProj[14]; const double cww = InvViewProj[15];

	const double pxx = (double)(p[ 0]); const double pxy = (double)(p[ 1]); const double pxz = (double)(p[ 2]); const double pxw = (double)(p[ 3]);
	const double pyx = (double)(p[ 4]); const double pyy = (double)(p[ 5]); const double pyz = (double)(p[ 6]); const double pyw = (double)(p[ 7]);
	const double pwx = (double)(p[12]); const double pwy = (double)(p[13]); const double pwz = (double)(p[14]); const double pww = (double)(p[15]);

	FCameraMotionParameters LocalCameraMotion;

	LocalCameraMotion.Value[0] = FVector4(
		(float)(4.0*(cwx*pww + cxx*pwx + cyx*pwy + czx*pwz)),
		(float)((-4.0)*(cwy*pww + cxy*pwx + cyy*pwy + czy*pwz)),
		(float)(2.0*(cwz*pww + cxz*pwx + cyz*pwy + czz*pwz)),
		(float)(2.0*(cww*pww - cwx*pww + cwy*pww + (cxw - cxx + cxy)*pwx + (cyw - cyx + cyy)*pwy + (czw - czx + czy)*pwz)));

	LocalCameraMotion.Value[1] = FVector4(
		(float)(( 4.0)*(cwy*pww + cxy*pwx + cyy*pwy + czy*pwz)),
		(float)((-2.0)*(cwz*pww + cxz*pwx + cyz*pwy + czz*pwz)),
		(float)((-2.0)*(cww*pww + cwy*pww + cxw*pwx - 2.0*cxx*pwx + cxy*pwx + cyw*pwy - 2.0*cyx*pwy + cyy*pwy + czw*pwz - 2.0*czx*pwz + czy*pwz - cwx*(2.0*pww + pxw) - cxx*pxx - cyx*pxy - czx*pxz)),
		(float)(-2.0*(cyy*pwy + czy*pwz + cwy*(pww + pxw) + cxy*(pwx + pxx) + cyy*pxy + czy*pxz)));

	LocalCameraMotion.Value[2] = FVector4(
		(float)((-4.0)*(cwx*pww + cxx*pwx + cyx*pwy + czx*pwz)),
		(float)(cyz*pwy + czz*pwz + cwz*(pww + pxw) + cxz*(pwx + pxx) + cyz*pxy + czz*pxz),
		(float)(cwy*pww + cwy*pxw + cww*(pww + pxw) - cwx*(pww + pxw) + (cxw - cxx + cxy)*(pwx + pxx) + (cyw - cyx + cyy)*(pwy + pxy) + (czw - czx + czy)*(pwz + pxz)),
		(float)(0));

	LocalCameraMotion.Value[3] = FVector4(
		(float)((-4.0)*(cwx*pww + cxx*pwx + cyx*pwy + czx*pwz)),
		(float)((-2.0)*(cwz*pww + cxz*pwx + cyz*pwy + czz*pwz)),
		(float)(2.0*((-cww)*pww + cwx*pww - 2.0*cwy*pww - cxw*pwx + cxx*pwx - 2.0*cxy*pwx - cyw*pwy + cyx*pwy - 2.0*cyy*pwy - czw*pwz + czx*pwz - 2.0*czy*pwz + cwy*pyw + cxy*pyx + cyy*pyy + czy*pyz)),
		(float)(2.0*(cyx*pwy + czx*pwz + cwx*(pww - pyw) + cxx*(pwx - pyx) - cyx*pyy - czx*pyz)));

	LocalCameraMotion.Value[4] = FVector4(
		(float)(4.0*(cwy*pww + cxy*pwx + cyy*pwy + czy*pwz)),
		(float)(cyz*pwy + czz*pwz + cwz*(pww - pyw) + cxz*(pwx - pyx) - cyz*pyy - czz*pyz),
		(float)(cwy*pww + cww*(pww - pyw) - cwy*pyw + cwx*((-pww) + pyw) + (cxw - cxx + cxy)*(pwx - pyx) + (cyw - cyx + cyy)*(pwy - pyy) + (czw - czx + czy)*(pwz - pyz)),
		(float)(0));

	return TUniformBufferRef<FCameraMotionParameters>::CreateUniformBufferImmediate(LocalCameraMotion, UniformBuffer_SingleFrame);
}
