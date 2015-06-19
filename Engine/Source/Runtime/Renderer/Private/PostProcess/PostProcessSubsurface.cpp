// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessSubsurface.cpp: Screenspace subsurface scattering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessSubsurface.h"
#include "PostProcessing.h"
#include "SceneUtils.h"

ENGINE_API const IPooledRenderTarget* GetSubsufaceProfileTexture_RT(FRHICommandListImmediate& RHICmdList);


static TAutoConsoleVariable<int32> CVarSSSFilter(
	TEXT("r.SSS.Filter"),
	1,
	TEXT("0: point filter (useful for testing, could be cleaner)\n")
	TEXT("1: bilinear filter"),
	ECVF_RenderThreadSafe  | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarSSSSampleSet(
	TEXT("r.SSS.SampleSet"),
	2,
	TEXT("0: lowest quality\n")
	TEXT("1: medium quality\n")
	TEXT("2: high quality (default)"),
	ECVF_RenderThreadSafe  | ECVF_Scalability);

static TAutoConsoleVariable<int> CVarSubsurfaceQuality(
	TEXT("r.SubsurfaceQuality"),
	1,
	TEXT("Define the quality of the Screenspace subsurface scattering postprocess.\n")
	TEXT(" 0: low quality for speculars on subsurface materials\n")
	TEXT(" 1: higher quality as specular is separated before screenspace blurring (Only used if SceneColor has an alpha channel)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// -------------------------------------------------------------

float GetSubsurfaceRadiusScale()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.SSS.Scale"));
	check(CVar);
	float Ret = CVar->GetValueOnRenderThread();

	return FMath::Max(0.0f, Ret);
}

// -------------------------------------------------------------

/** Shared shader parameters needed for screen space subsurface scattering. */
class FSubsurfaceParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		SSSParams.Bind(ParameterMap, TEXT("SSSParams"));
		SSProfilesTexture.Bind(ParameterMap, TEXT("SSProfilesTexture"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context) const
	{
		{
			// from Separabale.usf: float distanceToProjectionWindow = 1.0 / tan(0.5 * radians(SSSS_FOVY))
			// can be extracted out of projection matrix

			float ScaleCorrectionX = Context.View.ViewRect.Width() / (float)GSceneRenderTargets.GetBufferSizeXY().X;
			float ScaleCorrectionY = Context.View.ViewRect.Height() / (float)GSceneRenderTargets.GetBufferSizeXY().Y;

			 // Divide by 3 as the kernels range from -3 to 3.
			const float KernelSize = 3.0f;

			// Calculate the sssWidth scale (1.0 for a unit plane sitting on the projection window):
			float DistanceToProjectionWindow = Context.View.ViewMatrices.ProjMatrix.M[0][0]; 

			float SSSScaleZ = DistanceToProjectionWindow * GetSubsurfaceRadiusScale();

			// * 0.5f: hacked in 0.5 - -1..1 to 0..1 but why this isn't in demo code?
			float SSSScaleX = SSSScaleZ / KernelSize * 0.5f;

			FVector4 ColorScale(SSSScaleX, SSSScaleZ, ScaleCorrectionX, ScaleCorrectionY);
			SetShaderValue(Context.RHICmdList, ShaderRHI, SSSParams, ColorScale);
		}

		{
			const IPooledRenderTarget* PooledRT = GetSubsufaceProfileTexture_RT(Context.RHICmdList);

			if(!PooledRT)
			{
				// no subsurface profile was used yet
				PooledRT = GSystemTextures.BlackDummy;
			}

			const FSceneRenderTargetItem& Item = PooledRT->GetRenderTargetItem();

			SetTextureParameter(Context.RHICmdList, ShaderRHI, SSProfilesTexture, Item.ShaderResourceTexture);
		}
	}

	friend FArchive& operator<<(FArchive& Ar,FSubsurfaceParameters& P)
	{
		Ar << P.SSSParams << P.SSProfilesTexture;
		return Ar;
	}

private:
	FShaderParameter SSSParams;
	FShaderResourceParameter SSProfilesTexture;
};

// ---------------------------------------------


/**
 * Encapsulates the post processing subsurface scattering pixel shader.
 */
class FPostProcessSubsurfaceVisualizePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSubsurfaceVisualizePS , Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessSubsurfaceVisualizePS () {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter MiniFontTexture;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	FPostProcessSubsurfaceVisualizePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		MiniFontTexture.Bind(Initializer.ParameterMap, TEXT("MiniFontTexture"));
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		SetTextureParameter(Context.RHICmdList, ShaderRHI, MiniFontTexture, GEngine->MiniFontTexture ? GEngine->MiniFontTexture->Resource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture);
		SubsurfaceParameters.SetParameters(Context.RHICmdList, ShaderRHI, Context);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << MiniFontTexture << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessSubsurface");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("VisualizePS");
	}
};

IMPLEMENT_SHADER_TYPE3(FPostProcessSubsurfaceVisualizePS, SF_Pixel);

void SetSubsurfaceVisualizeShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSubsurfaceVisualizePS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}

FRCPassPostProcessSubsurfaceVisualize::FRCPassPostProcessSubsurfaceVisualize()
{
	// we need the GBuffer, we release it Process()
	GSceneRenderTargets.AdjustGBufferRefCount(1);
}

void FRCPassPostProcessSubsurfaceVisualize::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SubsurfaceSetup);

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

	SetSubsurfaceVisualizeShader(Context);

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
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

	{
		// this is a helper class for FCanvas to be able to get screen size
		class FRenderTargetTemp : public FRenderTarget
		{
		public:
			const FSceneView& View;
			const FTexture2DRHIRef Texture;

			FRenderTargetTemp(const FSceneView& InView, const FTexture2DRHIRef InTexture)
				: View(InView), Texture(InTexture)
			{
			}
			virtual FIntPoint GetSizeXY() const
			{
				return View.ViewRect.Size();
			};
			virtual const FTexture2DRHIRef& GetRenderTargetTexture() const
			{
				return Texture;
			}
		} TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);

		FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

		float X = 30;
		float Y = 28;
		const float YStep = 14;

		FString Line;

		Line = FString::Printf(TEXT("Visualize Screen Space Subsurface Scattering"));
		Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

		Y += YStep;

		uint32 Index = 0;
		while (GSubsufaceProfileTextureObject.GetEntryString(Index++, Line))
		{
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		}

		Canvas.Flush_RenderThread(Context.RHICmdList);
	}

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// we no longer need the GBuffer
	GSceneRenderTargets.AdjustGBufferRefCount(-1);
}

FPooledRenderTargetDesc FRCPassPostProcessSubsurfaceVisualize::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GSceneRenderTargets.GetSceneColor()->GetDesc();

	Ret.Reset();
	Ret.DebugName = TEXT("SubsurfaceVisualize");
	// alpha is used to store depth and renormalize (alpha==0 means there is no subsurface scattering)
	Ret.Format = PF_FloatRGBA;

	return Ret;
}


// ---------------------------------------------

/**
 * Encapsulates the post processing subsurface scattering pixel shader.
 * @param SetupMode 0:without specular correction, 1:with specular correction
 */
template <uint32 SetupMode>
class FPostProcessSubsurfaceSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSubsurfaceSetupPS , Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SETUP_MODE"), SetupMode);
	}

	/** Default constructor. */
	FPostProcessSubsurfaceSetupPS () {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter MiniFontTexture;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	FPostProcessSubsurfaceSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		SubsurfaceParameters.SetParameters(Context.RHICmdList, ShaderRHI, Context);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessSubsurface");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("SetupPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessSubsurfaceSetupPS<A> FPostProcessSubsurfaceSetupPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessSubsurfaceSetupPS##A, SF_Pixel);

	VARIATION1(0) VARIATION1(1)

#undef VARIATION1


template <uint32 SetupMode>
void SetSubsurfaceSetupShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessSubsurfaceSetupPS<SetupMode> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}


static bool DoSpecularCorrection()
{
	bool CVarState = CVarSubsurfaceQuality.GetValueOnRenderThread() > 0;

	int SceneColorFormat;
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SceneColorFormat"));

		SceneColorFormat = CVar->GetInt();
	}

	// we need an alpha channel for this feature
	return CVarState && (SceneColorFormat >= 4);
}

// --------------------------------------


/**
 * Encapsulates the post processing subsurface scattering pixel shader.
 * @param SampleSet 0:low, 1:med, 2:high
 */
template <uint32 RecombineMethod, uint32 SampleSet>
class FPostProcessSubsurfaceExtractSpecularPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSubsurfaceExtractSpecularPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SSS_RECOMBINE_METHOD"), RecombineMethod);
		OutEnvironment.SetDefine(TEXT("SSS_SAMPLESET"), SampleSet);
	}

	/** Default constructor. */
	FPostProcessSubsurfaceExtractSpecularPS () {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	FPostProcessSubsurfaceExtractSpecularPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FFinalPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		SubsurfaceParameters.SetParameters(Context.RHICmdList, ShaderRHI, Context);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessSubsurface");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("ExtractSpecularPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)			VARIATION2(A,2)
#define VARIATION2(A, B) typedef FPostProcessSubsurfaceExtractSpecularPS<A, B> FPostProcessSubsurfaceExtractSpecularPS##A##B; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessSubsurfaceExtractSpecularPS##A##B, SF_Pixel);
	VARIATION1(0) VARIATION1(1)
#undef VARIATION1
#undef VARIATION2


// @param SampleSet 0:low, 1:med, 2:high
template <uint32 RecombineMethod>
void SetSubsurfaceExtractSpecular(const FRenderingCompositePassContext& Context, uint32 SampleSet)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	switch(SampleSet)
	{
		case 0:
			{
				TShaderMapRef<FPostProcessSubsurfaceExtractSpecularPS<RecombineMethod, 0> > PixelShader(Context.GetShaderMap());
				static FGlobalBoundShaderState BoundShaderState;

				SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(Context);
				break;
			}
		case 1:
			{
				TShaderMapRef<FPostProcessSubsurfaceExtractSpecularPS<RecombineMethod, 1> > PixelShader(Context.GetShaderMap());
				static FGlobalBoundShaderState BoundShaderState;

				SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(Context);
				break;
			}
		case 2:
			{
				TShaderMapRef<FPostProcessSubsurfaceExtractSpecularPS<RecombineMethod, 2> > PixelShader(Context.GetShaderMap());
				static FGlobalBoundShaderState BoundShaderState;

				SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(Context);
				break;
			} 
		default:
			check(0);
	}

	VertexShader->SetParameters(Context);
}

void FRCPassPostProcessSubsurfaceExtractSpecular::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc);

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	check(DestSize.X);
	check(DestSize.Y);
	check(SrcSize.X);
	check(SrcSize.Y);

	//	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;

	TRefCountPtr<IPooledRenderTarget> NewSceneColor;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	bool bDoSpecularCorrection = DoSpecularCorrection();

	SCOPED_DRAW_EVENTF(Context.RHICmdList, SubsurfacePass, TEXT("SubsurfacePassExtractSpecular#%d"), (int32)bDoSpecularCorrection);

	uint32 SampleSet = FMath::Clamp(CVarSSSSampleSet.GetValueOnRenderThread(), 0, 2);

	if(bDoSpecularCorrection)
	{
		SetSubsurfaceExtractSpecular<1>(Context, SampleSet);
	}
	else
	{
		SetSubsurfaceExtractSpecular<0>(Context, SampleSet);
	}

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


FPooledRenderTargetDesc FRCPassPostProcessSubsurfaceExtractSpecular::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GSceneRenderTargets.GetSceneColor()->GetDesc();

	Ret.Reset();
	Ret.DebugName = TEXT("SubsurfaceExtractSpecular");
	// alpha is not used
	Ret.Format = PF_FloatRGB;

	return Ret;
}


// --------------------------------------

FRCPassPostProcessSubsurfaceSetup::FRCPassPostProcessSubsurfaceSetup(FViewInfo& View)
	: ViewRect(View.ViewRect)
{
}

void FRCPassPostProcessSubsurfaceSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, SubsurfaceSetup);

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

	FIntRect DestRect = FIntRect(0, 0, DestSize.X, DestSize.Y);
	// upscale rectangle to not slightly scale (might miss a pixel)
	FIntRect SrcRect = DestRect * 2 + View.ViewRect.Min;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	// reconstruct specular and add it in final pass
	bool bSpecularCorrection = DoSpecularCorrection();

	if(bSpecularCorrection)
	{
		SetSubsurfaceSetupShader<1>(Context);
	}
	else
	{
		SetSubsurfaceSetupShader<0>(Context);
	}

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
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

FPooledRenderTargetDesc FRCPassPostProcessSubsurfaceSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GSceneRenderTargets.GetSceneColor()->GetDesc();

	Ret.Reset();
	Ret.DebugName = TEXT("SubsurfaceSetup");
	// alpha is used to store depth and renormalize (alpha==0 means there is no subsurface scattering)
	Ret.Format = PF_FloatRGBA;

	Ret.Extent = ViewRect.Size();

	// half res
	Ret.Extent  = FIntPoint::DivideAndRoundUp(Ret.Extent, 2);
	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);
	
	return Ret;
}


/** Encapsulates the post processing subsurface pixel shader. */
// @param Direction 0: horizontal, 1:vertical
// @param SampleSet 0:low, 1:med, 2:high
template <uint32 Direction, uint32 SampleSet>
class TPostProcessSubsurfacePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TPostProcessSubsurfacePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SSS_DIRECTION"), Direction);
		OutEnvironment.SetDefine(TEXT("SSS_SAMPLESET"), SampleSet);
	}

	/** Default constructor. */
	TPostProcessSubsurfacePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	TPostProcessSubsurfacePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		if(CVarSSSFilter.GetValueOnRenderThread())
		{
			PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Border>::GetRHI());
		}
		else
		{
			PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Border>::GetRHI());
		}
		SubsurfaceParameters.SetParameters(Context.RHICmdList, ShaderRHI, Context);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessSubsurface");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)			VARIATION2(A,2)
#define VARIATION2(A, B) typedef TPostProcessSubsurfacePS<A, B> TPostProcessSubsurfacePS##A##B; \
	IMPLEMENT_SHADER_TYPE2(TPostProcessSubsurfacePS##A##B, SF_Pixel);
	VARIATION1(0) VARIATION1(1) VARIATION1(2)
#undef VARIATION1
#undef VARIATION2


FRCPassPostProcessSubsurface::FRCPassPostProcessSubsurface(uint32 InDirection)
	: Direction(InDirection) 
{
	check(InDirection < 2);
}

template <uint32 Direction, uint32 SampleSet>
void SetSubsurfaceShader(const FRenderingCompositePassContext& Context, TShaderMapRef<FPostProcessVS> &VertexShader)
{
	TShaderMapRef<TPostProcessSubsurfacePS<Direction, SampleSet> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}

// 0:horizontal, 1: vertical
template <uint32 Direction>
void SetSubsurfaceShaderSampleSet(const FRenderingCompositePassContext& Context, TShaderMapRef<FPostProcessVS> &VertexShader, uint32 SampleSet)
{
	switch(SampleSet)
	{
		case 0: SetSubsurfaceShader<Direction, 0>(Context, VertexShader); break;
		case 1: SetSubsurfaceShader<Direction, 1>(Context, VertexShader); break;
		case 2: SetSubsurfaceShader<Direction, 2>(Context, VertexShader); break;

	default:
		check(0);
	}
}

void FRCPassPostProcessSubsurface::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc);

	{
		const IPooledRenderTarget* PooledRT = GetSubsufaceProfileTexture_RT(Context.RHICmdList);

		check(PooledRT);

		// for debugging
		GRenderTargetPool.VisualizeTexture.SetCheckPoint(Context.RHICmdList, PooledRT);
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	check(DestSize.X);
	check(DestSize.Y);
	check(SrcSize.X);
	check(SrcSize.Y);

	FIntRect SrcRect = FIntRect(0, 0, DestSize.X, DestSize.Y);
	FIntRect DestRect = SrcRect;

	TRefCountPtr<IPooledRenderTarget> NewSceneColor;

	const FSceneRenderTargetItem* DestRenderTarget;
	{
		DestRenderTarget = &PassOutputs[0].RequestSurface(Context);

		check(DestRenderTarget);
	}

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget->TargetableTexture, FTextureRHIRef());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	SCOPED_DRAW_EVENTF(Context.RHICmdList, SubsurfacePass, TEXT("SubsurfaceDirection#%d"), Direction);

	uint32 SampleSet = FMath::Clamp(CVarSSSSampleSet.GetValueOnRenderThread(), 0, 2);

	if (Direction == 0)
	{
		SetSubsurfaceShaderSampleSet<0>(Context, VertexShader, SampleSet);
	}
	else
	{
		SetSubsurfaceShaderSampleSet<1>(Context, VertexShader, SampleSet);
	}

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

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget->TargetableTexture, DestRenderTarget->ShaderResourceTexture, false, FResolveParams());
}


FPooledRenderTargetDesc FRCPassPostProcessSubsurface::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = (Direction == 0) ? TEXT("SubsurfaceX") : TEXT("SubsurfaceY");

	return Ret;
}






/** Encapsulates the post processing subsurafce recombine pixel shader. */
// @param SpecularCorrection 0: reconstruct specular
template <uint32 RecombineMethod>
class TPostProcessSubsurfaceRecombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TPostProcessSubsurfaceRecombinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SSS_RECOMBINE_METHOD"), RecombineMethod);
	}

	/** Default constructor. */
	TPostProcessSubsurfaceRecombinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FSubsurfaceParameters SubsurfaceParameters;

	/** Initialization constructor. */
	TPostProcessSubsurfaceRecombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		SubsurfaceParameters.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SubsurfaceParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Border>::GetRHI());
		SubsurfaceParameters.SetParameters(Context.RHICmdList, ShaderRHI, Context);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessSubsurface");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("SubsurfaceRecombinePS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef TPostProcessSubsurfaceRecombinePS<A> TPostProcessSubsurfaceRecombinePS##A; \
	IMPLEMENT_SHADER_TYPE2(TPostProcessSubsurfaceRecombinePS##A, SF_Pixel);
	VARIATION1(0) VARIATION1(1)
#undef VARIATION1


template <uint32 RecombineMethod>
void SetSubsurfaceRecombineShader(const FRenderingCompositePassContext& Context, TShaderMapRef<FPostProcessVS> &VertexShader)
{
	TShaderMapRef<TPostProcessSubsurfaceRecombinePS<RecombineMethod> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context);
	VertexShader->SetParameters(Context);
}

void FRCPassPostProcessSubsurfaceRecombine::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	check(InputDesc);

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = GSceneRenderTargets.GetBufferSizeXY();

	check(DestSize.X);
	check(DestSize.Y);
	check(SrcSize.X);
	check(SrcSize.Y);

	FIntRect SrcRect = FIntRect(0, 0, InputDesc->Extent.X, InputDesc->Extent.Y);
	FIntRect DestRect = View.ViewRect;

	TRefCountPtr<IPooledRenderTarget>& SceneColor = GSceneRenderTargets.GetSceneColor();

	const FSceneRenderTargetItem& DestRenderTarget = SceneColor->GetRenderTargetItem();

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	Context.RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA,BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	bool bDoSpecularCorrection = DoSpecularCorrection();

	SCOPED_DRAW_EVENTF(Context.RHICmdList, SubsurfacePass, TEXT("SubsurfacePassRecombine#%d"), (int32)bDoSpecularCorrection);

	if(bDoSpecularCorrection)
	{
		SetSubsurfaceRecombineShader<1>(Context, VertexShader);
	}
	else
	{
		SetSubsurfaceRecombineShader<0>(Context, VertexShader);
	}

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


FPooledRenderTargetDesc FRCPassPostProcessSubsurfaceRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("SceneColor");
	// we directly render to the HDR scene color
	return Ret;
}
