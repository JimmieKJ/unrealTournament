// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMotionBlur.cpp: Post process MotionBlur implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessAmbientOcclusion.h"
#include "PostProcessMotionBlur.h"
#include "PostProcessAmbientOcclusion.h"
#include "PostProcessing.h"
#include "SceneUtils.h"
#include "GPUSkinVertexFactory.h"
#include "../../Engine/Private/SkeletalRenderGPUSkin.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarMotionBlurFiltering(
	TEXT("r.MotionBlurFiltering"),
	0,
	TEXT("Useful developer variable\n")
	TEXT("0: off (default, expected by the shader for better quality)\n")
	TEXT("1: on"),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif

static TAutoConsoleVariable<float> CVarMotionBlur2ndScale(
	TEXT("r.MotionBlur2ndScale"),
	1.0f,
	TEXT(""),
	ECVF_Cheat | ECVF_RenderThreadSafe);


/** Encapsulates the post processing motion blur vertex shader. */
class FPostProcessMotionBlurSetupVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessMotionBlurSetupVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessMotionBlurSetupVS() {}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}

	/** to have a similar interface as all other shaders */
	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const auto ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessMotionBlurSetupVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessMotionBlurSetupVS,TEXT("PostProcessMotionBlur"),TEXT("SetupVS"),SF_Vertex);


/** Encapsulates the post processing motion blur pixel shader. */
class FPostProcessMotionBlurSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessMotionBlurSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessMotionBlurSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter VelocityScale;

	/** Initialization constructor. */
	FPostProcessMotionBlurSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		VelocityScale.Bind( Initializer.ParameterMap, TEXT("VelocityScale") );
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << VelocityScale;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		{
			const float SizeX = Context.View.ViewRect.Width();
			const float SizeY = Context.View.ViewRect.Height();
			const float InvAspectRatio = SizeY / SizeX;

			const FSceneViewState* ViewState = (FSceneViewState*) Context.View.State;
			const float MotionBlurTimeScale = ViewState ? ViewState->MotionBlurTimeScale : 1.0f;

			const float ViewMotionBlurScale = 0.5f * MotionBlurTimeScale * Context.View.FinalPostProcessSettings.MotionBlurAmount;

			// 0:no 1:full screen width
			float MaxVelocity = Context.View.FinalPostProcessSettings.MotionBlurMax / 100.0f;
			float InvMaxVelocity = 1.0f / MaxVelocity;
			float ObjectScaleX = ViewMotionBlurScale * InvMaxVelocity;
			float ObjectScaleY = ViewMotionBlurScale * InvMaxVelocity * InvAspectRatio;

			SetShaderValue(Context.RHICmdList, ShaderRHI, VelocityScale, FVector4(ObjectScaleX, -ObjectScaleY, 0, 0));
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessMotionBlurSetupPS, TEXT("PostProcessMotionBlur"), TEXT("SetupPS"), SF_Pixel);


void FRCPassPostProcessMotionBlurSetup::Process(FRenderingCompositePassContext& Context)
{
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
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;

	// Viewport size not even also causes issue
	FIntRect DestRect = FIntRect::DivideAndRoundUp(SrcRect, 2);

	SCOPED_DRAW_EVENTF(Context.RHICmdList, MotionBlurSetup, TEXT("MotionBlurSetup %dx%d"), DestRect.Width(), DestRect.Height());
	
	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = PassOutputs[1].RequestSurface(Context);

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[] =
	{
		DestRenderTarget0.TargetableTexture,
		DestRenderTarget1.TargetableTexture
	};
	SetRenderTargets(Context.RHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, FTextureRHIParamRef(), 0, NULL);

	// is optimized away if possible (RT size=view size, )
	FLinearColor ClearColors[2] = {FLinearColor(0,0,0,0), FLinearColor(0,0,0,0)};
	Context.RHICmdList.ClearMRT(true, 2, ClearColors, false, 1.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessMotionBlurSetupVS> VertexShader(Context.GetShaderMap());

	{
		TShaderMapRef<FPostProcessMotionBlurSetupPS > PixelShader(Context.GetShaderMap());
		static FGlobalBoundShaderState BoundShaderState;
		

		SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(Context);
		VertexShader->SetParameters(Context);
	}

	DrawPostProcessPass(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestSize,
		SrcSize,
		*VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessMotionBlurSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	if(InPassOutputId == ePId_Output0)
	{
		// downsampled velocity

		FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

		Ret.Reset();
		Ret.Extent /= 2;
		Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
		Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

		// we need at least a format in the range 0..1 with RGB channels, A is unused
		//	Ret.Format = PF_A2B10G10R10;
		// we need alpha to renormalize
		Ret.Format = PF_FloatRGBA;

		Ret.TargetableFlags &= ~TexCreate_UAV;
		Ret.TargetableFlags |= TexCreate_RenderTargetable;
		Ret.DebugName = TEXT("MotionBlurSetup0");

		return Ret;
	}
	else 
	{
		check(InPassOutputId == ePId_Output1);

		// scene color with depth in alpha
		FPooledRenderTargetDesc Ret = GetInput(ePId_Input1)->GetOutput()->RenderTargetDesc;

		Ret.Reset();
		Ret.Extent /= 2;
		Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
		Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

		Ret.Format = PF_FloatRGBA;

		Ret.TargetableFlags &= ~TexCreate_UAV;
		Ret.TargetableFlags |= TexCreate_RenderTargetable;
		Ret.DebugName = TEXT("MotionBlurSetup1");

		return Ret;
	}
}


/**
 * @param Quality 0: visualize, 1:low, 2:medium, 3:high, 4:very high
 * Encapsulates a MotionBlur pixel shader.
 */
template <uint32 Quality>
class FPostProcessMotionBlurPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessMotionBlurPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MOTION_BLUR_QUALITY"), Quality);
	}

	/** Default constructor. */
	FPostProcessMotionBlurPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter PrevViewProjMatrix;
	FShaderParameter TextureViewMad;
	FShaderParameter MotionBlurParameters;

	/** Initialization constructor. */
	FPostProcessMotionBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		PrevViewProjMatrix.Bind(Initializer.ParameterMap, TEXT("PrevViewProjMatrix"));
		TextureViewMad.Bind(Initializer.ParameterMap, TEXT("TextureViewMad"));
		MotionBlurParameters.Bind(Initializer.ParameterMap, TEXT("MotionBlurParameters"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << PrevViewProjMatrix << TextureViewMad << MotionBlurParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			bool bFiltered = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			bFiltered = CVarMotionBlurFiltering.GetValueOnRenderThread() != 0;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

			if(bFiltered)
			{
				PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Clamp>::GetRHI());
			}
			else
			{
				PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Clamp>::GetRHI());
			}
		}
		
		if( Context.View.Family->EngineShowFlags.CameraInterpolation )
		{
			// Instead of finding the world space position of the current pixel, calculate the world space position offset by the camera position, 
			// then translate by the difference between last frame's camera position and this frame's camera position,
			// then apply the rest of the transforms.  This effectively avoids precision issues near the extents of large levels whose world space position is very large.
			FVector ViewOriginDelta = Context.View.ViewMatrices.ViewOrigin - Context.View.PrevViewMatrices.ViewOrigin;
			SetShaderValue(Context.RHICmdList, ShaderRHI, PrevViewProjMatrix, FTranslationMatrix(ViewOriginDelta) * Context.View.PrevViewRotationProjMatrix);
		}
		else
		{
			SetShaderValue(Context.RHICmdList, ShaderRHI, PrevViewProjMatrix, Context.View.ViewMatrices.GetViewRotationProjMatrix());
		}

		TRefCountPtr<IPooledRenderTarget> InputPooledElement = Context.Pass->GetInput(ePId_Input0)->GetOutput()->RequestInput();

		// to mask out samples from outside of the view
		{
			FIntPoint BufferSize = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY();
			FVector2D InvBufferSize(1.0f / BufferSize.X, 1.0f / BufferSize.Y);

			FIntRect ClipRect = Context.View.ViewRect;

			// to avoid leaking in content from the outside because of bilinear filtering, shrink
			ClipRect.InflateRect(-1);

			FVector2D MinUV(ClipRect.Min.X * InvBufferSize.X, ClipRect.Min.Y * InvBufferSize.Y);
			FVector2D MaxUV(ClipRect.Max.X * InvBufferSize.X, ClipRect.Max.Y * InvBufferSize.Y);
			FVector2D SizeUV = MaxUV - MinUV;

			FVector2D Mul(1.0f / SizeUV.X, 1.0f / SizeUV.Y);
			FVector2D Add = - MinUV * Mul;
			FVector4 TextureViewMadValue(Mul.X, Mul.Y, Add.X, Add.Y);
			SetShaderValue(Context.RHICmdList, ShaderRHI, TextureViewMad, TextureViewMadValue);
		}

		{
			const float SizeX = Context.View.ViewRect.Width();
			const float SizeY = Context.View.ViewRect.Height();
			const float AspectRatio = SizeX / SizeY;
			const float InvAspectRatio = SizeY / SizeX;

			const FSceneViewState* ViewState = (FSceneViewState*) Context.View.State;
			float MotionBlurTimeScale = ViewState ? ViewState->MotionBlurTimeScale : 1.0f;

			float ViewMotionBlurScale = 0.5f * MotionBlurTimeScale * Context.View.FinalPostProcessSettings.MotionBlurAmount;

			// MotionBlurInstanceScale was needed to hack some cases where motion blur wasn't working well, this shouldn't be needed any more, can clean this up later
			float MotionBlurInstanceScale = 1;

			float ObjectMotionBlurScale	= MotionBlurInstanceScale * ViewMotionBlurScale;
			// 0:no 1:full screen width, percent conversion
			float MaxVelocity = Context.View.FinalPostProcessSettings.MotionBlurMax / 100.0f;
			float InvMaxVelocity = 1.0f / MaxVelocity;

			// *2 to convert to -1..1 -1..1 screen space
			// / MaxFraction to map screenpos to -1..1 normalized MaxFraction
			FVector4 MotionBlurParametersValue(
				ObjectMotionBlurScale * InvMaxVelocity,
				- ObjectMotionBlurScale * InvMaxVelocity * InvAspectRatio,
				MaxVelocity * 2,
				- MaxVelocity * 2 * AspectRatio);
			SetShaderValue(Context.RHICmdList, ShaderRHI, MotionBlurParameters, MotionBlurParametersValue);
		}
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessMotionBlur");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessMotionBlurPS<A> FPostProcessMotionBlurPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessMotionBlurPS##A, SF_Pixel);

VARIATION1(0)			VARIATION1(1)			VARIATION1(2)			VARIATION1(3)			VARIATION1(4)
#undef VARIATION1



// @param Quality 0: visualize, 1:low, 2:medium, 3:high, 4:very high
template <uint32 Quality>
static void SetMotionBlurShaderTempl(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessMotionBlurPS<Quality> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	
	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context);
}

FRCPassPostProcessMotionBlur::FRCPassPostProcessMotionBlur(uint32 InQuality) 
	: Quality(InQuality)
{
	// internal error
	check(Quality >= 1 && Quality <= 4);
}

void FRCPassPostProcessMotionBlur::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleFactor);

	SCOPED_DRAW_EVENTF(Context.RHICmdList, MotionBlur, TEXT("MotionBlur(Old) %dx%d"), SrcRect.Width(), SrcRect.Height());

	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	if(Quality == 1)
	{
		SetMotionBlurShaderTempl<1>(Context);
	}
	else if(Quality == 2)
	{
		SetMotionBlurShaderTempl<2>(Context);
	}
	else if(Quality == 3)
	{
		SetMotionBlurShaderTempl<3>(Context);
	}
	else
	{
		check(Quality == 4);
		SetMotionBlurShaderTempl<4>(Context);
	}

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

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

FPooledRenderTargetDesc FRCPassPostProcessMotionBlur::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("MotionBlur");
	Ret.AutoWritable = false;

	return Ret;
}

/** Encapsulates a MotionBlur recombine pixel shader. */
class FPostProcessMotionBlurRecombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessMotionBlurRecombinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessMotionBlurRecombinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessMotionBlurRecombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
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

	void SetParameters(const FRenderingCompositePassContext& Context, bool bBilinear)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		if(bBilinear)
		{
			PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Clamp>::GetRHI());
		}
		else
		{
			PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Border, AM_Border, AM_Clamp>::GetRHI());
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessMotionBlurRecombinePS,TEXT("PostProcessMotionBlur"),TEXT("MainRecombinePS"),SF_Pixel);

void FRCPassPostProcessMotionBlurRecombine::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, MotionBlurRecombine, TEXT("MotionBlurRecombine %dx%d"), DestRect.Width(), DestRect.Height());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessMotionBlurRecombinePS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	
	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	// with point filtering we can better debug in Visualize MotionBlur
	const bool bBilinear = !View.Family->EngineShowFlags.VisualizeMotionBlur;

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context, bBilinear);

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

FPooledRenderTargetDesc FRCPassPostProcessMotionBlurRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	// we don't need the alpha channel and 32bit is faster and costs less memory
	Ret.Format = PF_FloatRGB;
	Ret.AutoWritable = false;
	Ret.DebugName = TEXT("MotionBlurRecombine");

	return Ret;
}


FIntPoint GetNumTiles16x16( FIntPoint PixelExtent )
{
	uint32 TilesX = (PixelExtent.X + 15) / 16;
	uint32 TilesY = (PixelExtent.Y + 15) / 16;
	return FIntPoint( TilesX, TilesY );
}

FVector4 GetMotionBlurParameters( const FViewInfo& View, float Scale = 1.0f )
{
	const float TileSize = 16.0f;

	const float SizeX = View.ViewRect.Width();
	const float SizeY = View.ViewRect.Height();
	const float AspectRatio = SizeY / SizeX;

	const FSceneViewState* ViewState = (FSceneViewState*) View.State;
	float MotionBlurTimeScale = ViewState ? ViewState->MotionBlurTimeScale : 1.0f;
	float MotionBlurScale = 0.5f * MotionBlurTimeScale * View.FinalPostProcessSettings.MotionBlurAmount;

	// 0:no 1:full screen width, percent conversion
	float MaxVelocity = View.FinalPostProcessSettings.MotionBlurMax / 100.0f;

	// Scale by 0.5 due to blur samples going both ways
	float PixelScale = Scale * SizeX * 0.5f;

	FVector4 MotionBlurParameters(
		AspectRatio,
		PixelScale * MotionBlurScale,			// Scale for pixels
		PixelScale * MotionBlurScale / TileSize,// Scale for tiles
		FMath::Abs( PixelScale ) * MaxVelocity	// Max velocity pixels
	);

	return MotionBlurParameters;
}

class FPostProcessVelocityFlattenCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVelocityFlattenCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessVelocityFlattenCS() {}

public:
	FShaderParameter		OutVelocityFlat;		// UAV
	FShaderParameter		OutMaxTileVelocity;		// UAV

	FPostProcessVelocityFlattenCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		MotionBlurParameters.Bind(Initializer.ParameterMap, TEXT("MotionBlurParameters"));
		OutVelocityFlat.Bind(Initializer.ParameterMap, TEXT("OutVelocityFlat"));
		OutMaxTileVelocity.Bind(Initializer.ParameterMap, TEXT("OutMaxTileVelocity"));
	}

	void SetCS( FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context, const FSceneView& View )
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetCS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FCameraMotionParameters>(), CreateCameraMotionParametersUniformBuffer(Context.View));

		SetShaderValue(Context.RHICmdList, ShaderRHI, MotionBlurParameters, GetMotionBlurParameters( Context.View ) );
	}
	
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		Ar << MotionBlurParameters;
		Ar << OutVelocityFlat;
		Ar << OutMaxTileVelocity;
		return bShaderHasOutdatedParameters;
	}

private:
	FPostProcessPassParameters	PostprocessParameter;
	FShaderParameter			MotionBlurParameters;
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVelocityFlattenCS,TEXT("PostProcessVelocityFlatten"),TEXT("VelocityFlattenMain"),SF_Compute);


FRCPassPostProcessVelocityFlatten::FRCPassPostProcessVelocityFlatten()
	: AsyncJobFenceID(-1)
{}

void FRCPassPostProcessVelocityFlatten::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VelocityFlatten);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if (AsyncJobFenceID != -1)
	{
		// If we run with AsyncCompute we use the same node twice, once to start the AsyncCompute and once to wait for the result.
		// The later one is happening here.
		Context.RHICmdList.GraphicsWaitOnAsyncComputeJob(AsyncJobFenceID);
		AsyncJobFenceID = -1;
		return;
	}

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = PassOutputs[1].RequestSurface(Context);

	TShaderMapRef< FPostProcessVelocityFlattenCS > ComputeShader( Context.GetShaderMap() );

	const bool bAsyncComputeEnabled = IsAsyncComputeEnabled();

	SetRenderTarget(Context.RHICmdList, FTextureRHIRef(), FTextureRHIRef());

	if(bAsyncComputeEnabled)
	{
		// If AsyncCompute is enabled we start recording the commands for that
		Context.RHICmdList.BeginAsyncComputeJob_DrawThread(AsyncComputePriority_Default);
	}

	Context.SetViewportAndCallRHI( View.ViewRect );
	Context.RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	Context.RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutVelocityFlat.GetBaseIndex(), DestRenderTarget0.UAV);
	Context.RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutMaxTileVelocity.GetBaseIndex(), DestRenderTarget1.UAV);

	ComputeShader->SetCS(Context.RHICmdList, Context, View);

	FIntPoint ThreadGroupCountValue = GetNumTiles16x16(View.ViewRect.Size());
	DispatchComputeShader(Context.RHICmdList, *ComputeShader, ThreadGroupCountValue.X, ThreadGroupCountValue.Y, 1);

#if USE_ASYNC_COMPUTE_CONTEXT
	if ( AsyncJobFenceID != -1 )
	{
		Context.RHICmdList.GraphicsWaitOnAsyncComputeJob(AsyncJobFenceID);
		AsyncJobFenceID = -1;
	}
#endif

	//	void FD3D11DynamicRHI::RHIGraphicsWaitOnAsyncComputeJob( uint32 FenceIndex )
	Context.RHICmdList.FlushComputeShaderCache();

	// un-set destination
	Context.RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutVelocityFlat.GetBaseIndex(), NULL);
	Context.RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutMaxTileVelocity.GetBaseIndex(), NULL);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());

	// we want this pass to be executed another time
	// so we do this: CompositeContext.Process(VelocityFlattenPass, TEXT("VelocityFlattenPass"));
	// and this: bProcessWasCalled = false;
	if(bAsyncComputeEnabled)
	{
		// we end recording the commands for AsyncCompute and get back a number to wait for it with GraphicsWaitOnAsyncComputeJob()
		AsyncJobFenceID = Context.RHICmdList.EndAsyncComputeJob_DrawThread();

		// mark it not processed so it gets executed a second time
		bProcessWasCalled = false;

		// we run this Process() two times and want to not get the dependencies processed twice (could be moved into the ComposingGraph iteration, slower but easier coding)
		SetInput(ePId_Input0, FRenderingCompositeOutputRef());
		SetInput(ePId_Input1, FRenderingCompositeOutputRef());
	}
}

FPooledRenderTargetDesc FRCPassPostProcessVelocityFlatten::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	if( InPassOutputId == ePId_Output0 )
	{
		// Flattened velocity
		FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
		Ret.Reset();
		Ret.ClearValue = FClearValueBinding::None;
		Ret.Format = PF_FloatR11G11B10;
		Ret.TargetableFlags |= TexCreate_UAV;
		Ret.TargetableFlags |= TexCreate_RenderTargetable;
		Ret.DebugName = TEXT("VelocityFlat");

		return Ret;
	}
	else
	{
		// Max tile velocity
		FPooledRenderTargetDesc UnmodifiedRet = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
		UnmodifiedRet.Reset();

		FIntPoint PixelExtent = UnmodifiedRet.Extent;
		FIntPoint TileCount = GetNumTiles16x16(PixelExtent);

		FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(TileCount, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));

		Ret.DebugName = TEXT("MaxVelocity");

		return Ret;
	}
}







class FScatterQuadIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		const uint32 Size = sizeof(uint16) * 6 * 8;
		const uint32 Stride = sizeof(uint16);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer( Stride, Size, BUF_Static, CreateInfo, Buffer );
		uint16* Indices = (uint16*)Buffer;
		for (uint32 SpriteIndex = 0; SpriteIndex < 8; ++SpriteIndex)
		{
			Indices[SpriteIndex*6 + 0] = SpriteIndex*4 + 0;
			Indices[SpriteIndex*6 + 1] = SpriteIndex*4 + 3;
			Indices[SpriteIndex*6 + 2] = SpriteIndex*4 + 2;
			Indices[SpriteIndex*6 + 3] = SpriteIndex*4 + 0;
			Indices[SpriteIndex*6 + 4] = SpriteIndex*4 + 1;
			Indices[SpriteIndex*6 + 5] = SpriteIndex*4 + 3;
		}
		RHIUnlockIndexBuffer( IndexBufferRHI );
	}
};

TGlobalResource< FScatterQuadIndexBuffer > GScatterQuadIndexBuffer;


class FPostProcessVelocityScatterVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVelocityScatterVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessVelocityScatterVS() {}
	
public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter DrawMax;
	FShaderParameter MotionBlurParameters;

	/** Initialization constructor. */
	FPostProcessVelocityScatterVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DrawMax.Bind(Initializer.ParameterMap, TEXT("bDrawMax"));
		MotionBlurParameters.Bind( Initializer.ParameterMap, TEXT("MotionBlurParameters") );
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DrawMax << MotionBlurParameters;
		return bShaderHasOutdatedParameters;
	}

	/** to have a similar interface as all other shaders */
	void SetParameters(const FRenderingCompositePassContext& Context, int32 bDrawMax)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(Context.RHICmdList, ShaderRHI, DrawMax, bDrawMax);
		SetShaderValue(Context.RHICmdList, ShaderRHI, MotionBlurParameters, GetMotionBlurParameters( Context.View ) );
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessMotionBlur");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("VelocityScatterVS");
	}
};

class FPostProcessVelocityScatterPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVelocityScatterPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessVelocityScatterPS() {}

public:
	/** Initialization constructor. */
	FPostProcessVelocityScatterPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVelocityScatterVS,TEXT("PostProcessMotionBlur"),TEXT("VelocityScatterVS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FPostProcessVelocityScatterPS,TEXT("PostProcessMotionBlur"),TEXT("VelocityScatterPS"),SF_Pixel);


void FRCPassPostProcessVelocityScatter::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VelocityScatter);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	
	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	FIntPoint TileCount = GetNumTiles16x16( View.ViewRect.Size() );

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	TRefCountPtr<IPooledRenderTarget> DepthTarget;
	FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( DestSize, PF_ShadowDepth, FClearValueBinding::DepthOne, TexCreate_None, TexCreate_DepthStencilTargetable, false ) );
	GRenderTargetPool.FindFreeElement(Context.RHICmdList, Desc, DepthTarget, TEXT("VelocityScatterDepth") );

	// Set the view family's render target/viewport.
	FRHIRenderTargetView ColorView(DestRenderTarget.TargetableTexture, 0, -1, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
	FRHIDepthRenderTargetView DepthView(DepthTarget->GetRenderTargetItem().TargetableTexture, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
	FRHISetRenderTargetsInfo RTInfo(1, &ColorView, DepthView);

	// clear depth
	// Max >= Min so no need to clear on second pass
	Context.SetViewportAndCallRHI(0, 0, 0.0f, TileCount.X, TileCount.Y, 1.0f);
	Context.RHICmdList.SetRenderTargetsAndClear(RTInfo);			
	
	// Min,Max
	for( int i = 0; i < 2; i++ )
	{
		if( i == 0 )
		{
			// min
			Context.RHICmdList.SetBlendState( TStaticBlendStateWriteMask< CW_RGBA >::GetRHI() );
			Context.RHICmdList.SetRasterizerState( TStaticRasterizerState<>::GetRHI() );
			Context.RHICmdList.SetDepthStencilState( TStaticDepthStencilState< true, CF_Less >::GetRHI() );
		}
		else
		{
			// max
			Context.RHICmdList.SetBlendState( TStaticBlendStateWriteMask< CW_BA >::GetRHI() );
			Context.RHICmdList.SetRasterizerState( TStaticRasterizerState<>::GetRHI() );
			Context.RHICmdList.SetDepthStencilState( TStaticDepthStencilState< true, CF_Greater >::GetRHI() );
		}

		TShaderMapRef< FPostProcessVelocityScatterVS > VertexShader(Context.GetShaderMap());
		TShaderMapRef< FPostProcessVelocityScatterPS > PixelShader(Context.GetShaderMap());

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		VertexShader->SetParameters( Context, i );
		PixelShader->SetParameters( Context );

		// needs to be the same on shader side (faster on NVIDIA and AMD)
		int32 QuadsPerInstance = 8;

		Context.RHICmdList.SetStreamSource(0, NULL, 0, 0);
		Context.RHICmdList.DrawIndexedPrimitive(GScatterQuadIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 32, 0, 2 * QuadsPerInstance, FMath::DivideAndRoundUp(TileCount.X * TileCount.Y, QuadsPerInstance));
	}

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVelocityScatter::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Reset();

	Ret.DebugName = TEXT("ScatteredMaxVelocity");

	return Ret;
}


class FPostProcessVelocityGatherCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVelocityGatherCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessVelocityGatherCS() {}

public:
	FShaderParameter		OutScatteredMaxVelocity;

	/** Initialization constructor. */
	FPostProcessVelocityGatherCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		MotionBlurParameters.Bind(Initializer.ParameterMap, TEXT("MotionBlurParameters"));
		OutScatteredMaxVelocity.Bind(Initializer.ParameterMap, TEXT("OutScatteredMaxVelocity"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		Ar << MotionBlurParameters;
		Ar << OutScatteredMaxVelocity;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetCS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(Context.RHICmdList, ShaderRHI, MotionBlurParameters, GetMotionBlurParameters( Context.View ) );
	}
	
private:
	FPostProcessPassParameters	PostprocessParameter;
	FShaderParameter			MotionBlurParameters;
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVelocityGatherCS,TEXT("PostProcessVelocityFlatten"),TEXT("VelocityGatherCS"),SF_Compute);

void FRCPassPostProcessVelocityGather::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VelocityDilate);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	FIntPoint TileCount = GetNumTiles16x16( View.ViewRect.Size() );
	
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	SetRenderTarget(Context.RHICmdList, FTextureRHIRef(), FTextureRHIRef());
	Context.SetViewportAndCallRHI( 0, 0, 0.0f, TileCount.X, TileCount.Y, 1.0f );
	
	TShaderMapRef< FPostProcessVelocityGatherCS > ComputeShader( Context.GetShaderMap() );
	Context.RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	Context.RHICmdList.SetUAVParameter( ComputeShader->GetComputeShader(), ComputeShader->OutScatteredMaxVelocity.GetBaseIndex(), DestRenderTarget.UAV );

	ComputeShader->SetParameters( Context );

	FIntPoint GroupCount = GetNumTiles16x16( TileCount );
	DispatchComputeShader(Context.RHICmdList, *ComputeShader, GroupCount.X, GroupCount.Y, 1);

	// un-set destination
	Context.RHICmdList.SetUAVParameter( ComputeShader->GetComputeShader(), ComputeShader->OutScatteredMaxVelocity.GetBaseIndex(), NULL );

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVelocityGather::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;
	Ret.Reset();

	Ret.TargetableFlags |= TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;
	Ret.DebugName = TEXT("ScatteredMaxVelocity");

	return Ret;
}


/**
 * @param Quality 0: visualize, 1:low, 2:medium, 3:high, 4:very high
 */
template< uint32 Quality >
class FPostProcessMotionBlurNewPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessMotionBlurNewPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MOTION_BLUR_QUALITY"), Quality);
	}

	/** Default constructor. */
	FPostProcessMotionBlurNewPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter MotionBlurParameters;

	/** Initialization constructor. */
	FPostProcessMotionBlurNewPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		MotionBlurParameters.Bind(Initializer.ParameterMap, TEXT("MotionBlurParameters"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << MotionBlurParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context, float Scale)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			bool bFiltered = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			bFiltered = CVarMotionBlurFiltering.GetValueOnRenderThread() != 0;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

			if(bFiltered)
			{
				//PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Border,AM_Border,AM_Clamp>::GetRHI());

				FSamplerStateRHIParamRef Filters[] =
				{
					TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
					TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
					TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
					TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				};

				PostprocessParameter.SetPS( ShaderRHI, Context, 0, eFC_0000, Filters );
			}
			else
			{
				PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
			}
		}

		SetShaderValue(Context.RHICmdList, ShaderRHI, MotionBlurParameters, GetMotionBlurParameters( Context.View, Scale ) );
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessMotionBlur");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainNewPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessMotionBlurNewPS<A> FPostProcessMotionBlurNewPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessMotionBlurNewPS##A, SF_Pixel);

VARIATION1(1)			VARIATION1(2)			VARIATION1(3)			VARIATION1(4)
#undef VARIATION1



// @param Quality 0: visualize, 1:low, 2:medium, 3:high, 4:very high
template< uint32 Quality >
static void SetMotionBlurShaderNewTempl( const FRenderingCompositePassContext& Context, float Scale )
{
	TShaderMapRef< FPostProcessVS > VertexShader( Context.GetShaderMap() );
	TShaderMapRef< FPostProcessMotionBlurNewPS< Quality > >	PixelShader( Context.GetShaderMap() );

	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context, Scale);
}

void FRCPassPostProcessMotionBlurNew::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, MotionBlurNew, TEXT("MotionBlur(New) %dx%d"), SrcRect.Width(), SrcRect.Height());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(SrcRect);
	
	// is optimized away if possible (RT size=view size, )
	//Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, SrcRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	float BlurScaleLUT[] =
	{
		1.0f - 0.5f / 4.0f,
		1.0f - 0.5f / 6.0f,
		1.0f - 0.5f / 8.0f,
		1.0f - 0.5f / 16.0f,
		1.0f /  4.0f * CVarMotionBlur2ndScale.GetValueOnRenderThread(),
		1.0f /  6.0f * CVarMotionBlur2ndScale.GetValueOnRenderThread(),
		1.0f /  8.0f * CVarMotionBlur2ndScale.GetValueOnRenderThread(),
		1.0f / 16.0f * CVarMotionBlur2ndScale.GetValueOnRenderThread(),
	};
	float Scale = Pass >= 0 ? BlurScaleLUT[ (Pass * 4) + (Quality - 1) ] : 1.0f;
	
	if(Quality == 1)
	{
		SetMotionBlurShaderNewTempl<1>( Context, Scale );
	}
	else if(Quality == 2)
	{
		SetMotionBlurShaderNewTempl<2>( Context, Scale );
	}
	else if(Quality == 3 || Pass > 0 )
	{
		SetMotionBlurShaderNewTempl<3>( Context, Scale );
	}
	else
	{
		check(Quality == 4);
		SetMotionBlurShaderNewTempl<4>( Context, Scale );
	}

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	
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

FPooledRenderTargetDesc FRCPassPostProcessMotionBlurNew::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.Format = PF_FloatRGB;
	Ret.DebugName = TEXT("MotionBlur");
	Ret.AutoWritable = false;

	return Ret;
}


void FRCPassPostProcessVisualizeMotionBlur::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeMotionBlur);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint TexSize = InputDesc->Extent;

	// we assume the input and output is full resolution

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleFactor);
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	
	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, SrcRect);

	Context.SetViewportAndCallRHI(SrcRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	// Quality 0: visualize
	SetMotionBlurShaderTempl<0>(Context);

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y, 
		SrcRect.Width(), SrcRect.Height(),
		SrcRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

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

	float X = 20;
	float Y = 8;
	const float YStep = 14;
	const float ColumnWidth = 200;

	FString Line;

	Line = FString::Printf(TEXT("Visualize MotionBlur"));
	Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 0));
	
	static const auto MotionBlurDebugVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MotionBlurDebug"));
	const int32 MotionBlurDebug = MotionBlurDebugVar ? MotionBlurDebugVar->GetValueOnRenderThread() : 0;

	Line = FString::Printf(TEXT("%d, %d"), ViewFamily.FrameNumber, MotionBlurDebug);
	Canvas.DrawShadowedString(X, Y += YStep, TEXT("FrameNo, r.MotionBlurDebug:"), GetStatsFont(), FLinearColor(1, 1, 0));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 1, 0));

	static const auto VelocityTestVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VelocityTest"));
	const int32 VelocityTest = VelocityTestVar ? VelocityTestVar->GetValueOnRenderThread() : 0;
	
	extern bool IsParallelVelocity();

	Line = FString::Printf(TEXT("%d, %d, %d"), ViewFamily.bWorldIsPaused, VelocityTest, IsParallelVelocity());
	Canvas.DrawShadowedString(X, Y += YStep, TEXT("Paused, r.VelocityTest, Parallel:"), GetStatsFont(), FLinearColor(1, 1, 0));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 1, 0));

	const FScene* Scene = (const FScene*)View.Family->Scene;

	Canvas.DrawShadowedString(X, Y += YStep, TEXT("MotionBlurInfoData (per object):"), GetStatsFont(), FLinearColor(1, 1, 0));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Scene->MotionBlurInfoData.GetDebugString(), GetStatsFont(), FLinearColor(1, 1, 0));	

	const FSceneViewState *SceneViewState = (const FSceneViewState*)View.State;

	Line = FString::Printf(TEXT("View=%.4x PrevView=%.4x"),
		View.ViewMatrices.ViewMatrix.ComputeHash() & 0xffff,
		SceneViewState->PrevViewMatrices.ViewMatrix.ComputeHash() & 0xffff);
	Canvas.DrawShadowedString(X, Y += YStep, TEXT("ViewMatrix:"), GetStatsFont(), FLinearColor(1, 1, 0));
	Canvas.DrawShadowedString(X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 1, 0));

	Canvas.Flush_RenderThread(Context.RHICmdList);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeMotionBlur::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("MotionBlur");
	Ret.AutoWritable = false;

	return Ret;
}
