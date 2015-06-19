// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDOF.cpp: Post process Depth of Field implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessBokehDOF.h"
#include "PostProcessDOF.h"
#include "PostProcessing.h"
#include "SceneUtils.h"


/** Encapsulates the DOF setup pixel shader. */
template <uint32 NearBlurEnable>
class FPostProcessDOFSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDOFSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_NEAR_BLUR"), NearBlurEnable);
	}

	/** Default constructor. */
	FPostProcessDOFSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;
	
	/** Initialization constructor. */
	FPostProcessDOFSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
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
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldParams;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Clamp>::GetRHI());

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}
	}
};

IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDOFSetupPS<0>,TEXT("PostProcessDOF"),TEXT("SetupPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDOFSetupPS<1>,TEXT("PostProcessDOF"),TEXT("SetupPS"),SF_Pixel);

void FRCPassPostProcessDOFSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, DOFSetup);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	uint32 NumRenderTargets = bNearBlurEnabled ? 2 : 1;

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = GSceneRenderTargets.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect / 2;

	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = bNearBlurEnabled ? PassOutputs[1].RequestSurface(Context) : FSceneRenderTargetItem();

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[2] =
	{
		DestRenderTarget0.TargetableTexture,
		DestRenderTarget1.TargetableTexture
	};
	SetRenderTargets(Context.RHICmdList, NumRenderTargets, RenderTargets, FTextureRHIParamRef(), 0, NULL);
	
	FLinearColor ClearColors[2] = 
	{
		FLinearColor(0, 0, 0, 0),
		FLinearColor(0, 0, 0, 0)
	};
	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.ClearMRT(true, NumRenderTargets, ClearColors, false, 1.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	
	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

	if (bNearBlurEnabled)
	{
		static FGlobalBoundShaderState BoundShaderState;
		

		TShaderMapRef< FPostProcessDOFSetupPS<1> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
		
		PixelShader->SetParameters(Context);
	}
	else
	{
		static FGlobalBoundShaderState BoundShaderState;
		
		TShaderMapRef< FPostProcessDOFSetupPS<0> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(Context);
	}

	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width() + 1, DestRect.Height() + 1,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width() + 1, SrcRect.Height() + 1,
		DestSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessDOFSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

//	Ret.Extent = FIntPoint::DivideAndRoundUp(ret.Extent, 2);
	Ret.Extent /= 2;
	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	Ret.Reset();
	Ret.TargetableFlags &= ~(uint32)TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;

	Ret.DebugName = (InPassOutputId == ePId_Output0) ? TEXT("DOFSetup0") : TEXT("DOFSetup1");

	// more precision for additive blending and we need the alpha channel
	Ret.Format = PF_FloatRGBA;

	return Ret;
}




/** Encapsulates  the DOF recombine pixel shader. */
template <uint32 NearBlurEnable>
class FPostProcessDOFRecombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDOFRecombinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_NEAR_BLUR"), NearBlurEnable);
	}

	/** Default constructor. */
	FPostProcessDOFRecombinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldUVLimit;

	/** Initialization constructor. */
	FPostProcessDOFRecombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldUVLimit.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldUVLimit"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldUVLimit;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		
		// Compute out of bounds UVs in the source texture.
		FVector4 Bounds;
		Bounds.X = (((float)((Context.View.ViewRect.Min.X + 1) & (~1))) + 3.0f) / ((float)(GSceneRenderTargets.GetBufferSizeXY().X));
		Bounds.Y = (((float)((Context.View.ViewRect.Min.Y + 1) & (~1))) + 3.0f) / ((float)(GSceneRenderTargets.GetBufferSizeXY().Y));
		Bounds.Z = (((float)(Context.View.ViewRect.Max.X & (~1))) - 3.0f) / ((float)(GSceneRenderTargets.GetBufferSizeXY().X));
		Bounds.W = (((float)(Context.View.ViewRect.Max.Y & (~1))) - 3.0f) / ((float)(GSceneRenderTargets.GetBufferSizeXY().Y));

		SetShaderValue(Context.RHICmdList, ShaderRHI, DepthOfFieldUVLimit, Bounds);
	}
};

IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDOFRecombinePS<0>,TEXT("PostProcessDOF"),TEXT("MainRecombinePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDOFRecombinePS<1>,TEXT("PostProcessDOF"),TEXT("MainRecombinePS"),SF_Pixel);

void FRCPassPostProcessDOFRecombine::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, DOFRecombine);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input1);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint TexSize = InputDesc->Extent;

	// usually 1, 2, 4 or 8
	uint32 ScaleToFullRes = GSceneRenderTargets.GetBufferSizeXY().X / TexSize.X;

	FIntRect HalfResViewRect = View.ViewRect / ScaleToFullRes;

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

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

	if (bNearBlurEnabled)
	{
		static FGlobalBoundShaderState BoundShaderState;
		
		TShaderMapRef< FPostProcessDOFRecombinePS<1> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
		PixelShader->SetParameters(Context);
	}
	else
	{
		static FGlobalBoundShaderState BoundShaderState;
		
		TShaderMapRef< FPostProcessDOFRecombinePS<0> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
		PixelShader->SetParameters(Context);
	}

	VertexShader->SetParameters(Context);

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

FPooledRenderTargetDesc FRCPassPostProcessDOFRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("DOFRecombine");

	return Ret;
}







// Convert f-stop and focal distance into projected size in half resolution pixels.
// Setup depth based blur.
static FVector CircleDofCoc(const FRenderingCompositePassContext& Context)
{
	// Convert FOV to focal length,
	// 
	// fov = 2 * atan(d/(2*f))
	// where,
	//   d = sensor dimension (APS-C 24.576 mm)
	//   f = focal length
	// 
	// f = 0.5 * d * (1/tan(fov/2))
	float HalfFOV = FMath::Atan(1.0f / Context.View.ViewMatrices.ProjMatrix.M[0][0]);
	float FocalLength = 0.5f * 24.576f * (1.0f/FMath::Tan(HalfFOV));
	 
	// Convert focal distance in world position to mm.
	// Conversion is 1 world position = 1 cm.
	float Distance = Context.View.FinalPostProcessSettings.DepthOfFieldFocalDistance;
	Distance *= 10.0f;

	// Convert f-stop, focal length, and focal distance to
	// projected circle of confusion size at infinity in mm.
	//
	// coc = f*f / (n * (d - f))
	// where,
	//   f = focal length
	//   d = focal distance
	//   n = fstop (where n is the "n" in "f/n")
	float Radius = FocalLength * FocalLength / (Context.View.FinalPostProcessSettings.DepthOfFieldFstop * (Distance - FocalLength));

	// Scale so that APS-C 24.576 mm = full frame.
	// Convert mm to pixels.
	float Width = (float)Context.GetViewport().Size().X;
	Radius = Radius * Width * (1.0f/24.576f);

	// Convert diameter to radius at half resolution (algorithm radius is at half resolution).
	Radius *= 0.25f;

	// Comment out for now, allowing settings which the algorithm cannot cleanly do.
	#if 0
		// Limit to algorithm max size.
		if(Radius > 6.0f) 
		{
			Radius = 6.0f; 
		}
	#endif

	// The DepthOfFieldDepthBlurAmount = km at which depth blur is 50%.
	// Need to convert to cm here.
	return FVector(
		Radius, 
		1.0f/(Context.View.FinalPostProcessSettings.DepthOfFieldDepthBlurAmount * 100000.0),
		Context.View.FinalPostProcessSettings.DepthOfFieldDepthBlurRadius * Width / 1920.0f);
}

/** Encapsulates the Circle DOF setup pixel shader. */
template <uint32 NearBlurEnable>
class FPostProcessCircleDOFSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessCircleDOFSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_NEAR_BLUR"), NearBlurEnable);
	}

	/** Default constructor. */
	FPostProcessCircleDOFSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;
	FShaderParameter CircleDofParams;

	/** Initialization constructor. */
	FPostProcessCircleDOFSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
		CircleDofParams.Bind(Initializer.ParameterMap,TEXT("CircleDofParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldParams << CircleDofParams;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Clamp>::GetRHI());

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}

		SetShaderValue(Context.RHICmdList, ShaderRHI, CircleDofParams, CircleDofCoc(Context));
	}
};

IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFSetupPS<0>,TEXT("PostProcessDOF"),TEXT("CircleSetupPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFSetupPS<1>,TEXT("PostProcessDOF"),TEXT("CircleSetupPS"),SF_Pixel);

void FRCPassPostProcessCircleDOFSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CircleDOFSetup);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	uint32 NumRenderTargets = bNearBlurEnabled ? 2 : 1;

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = GSceneRenderTargets.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect / 2;

	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = bNearBlurEnabled ? PassOutputs[1].RequestSurface(Context) : FSceneRenderTargetItem();

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[2] =
	{
		DestRenderTarget0.TargetableTexture,
		DestRenderTarget1.TargetableTexture
	};
	SetRenderTargets(Context.RHICmdList, NumRenderTargets, RenderTargets, FTextureRHIParamRef(), 0, NULL);

	FLinearColor ClearColors[2] = 
	{
		FLinearColor(0, 0, 0, 0),
		FLinearColor(0, 0, 0, 0)
	};
	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.ClearMRT(true, NumRenderTargets, ClearColors, false, 1.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

	if (bNearBlurEnabled)
	{
		static FGlobalBoundShaderState BoundShaderState;


		TShaderMapRef< FPostProcessCircleDOFSetupPS<1> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(Context);
	}
	else
	{
		static FGlobalBoundShaderState BoundShaderState;

		TShaderMapRef< FPostProcessCircleDOFSetupPS<0> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(Context);
	}

	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width() + 1, DestRect.Height() + 1,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width() + 1, SrcRect.Height() + 1,
		DestSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessCircleDOFSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	//	Ret.Extent = FIntPoint::DivideAndRoundUp(ret.Extent, 2);
	Ret.Extent /= 2;
	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	Ret.Reset();
	Ret.TargetableFlags &= ~(uint32)TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;

	Ret.DebugName = (InPassOutputId == ePId_Output0) ? TEXT("CircleDOFSetup0") : TEXT("CircleDOFSetup1");

	// more precision for additive blending and we need the alpha channel
	Ret.Format = PF_FloatRGBA;

	return Ret;
}




/** Encapsulates the Circle DOF Dilate pixel shader. */
template <uint32 NearBlurEnable>
class FPostProcessCircleDOFDilatePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessCircleDOFDilatePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_NEAR_BLUR"), NearBlurEnable);
	}

	/** Default constructor. */
	FPostProcessCircleDOFDilatePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;
	FShaderParameter CircleDofParams;

	/** Initialization constructor. */
	FPostProcessCircleDOFDilatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
		CircleDofParams.Bind(Initializer.ParameterMap,TEXT("CircleDofParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldParams << CircleDofParams;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Clamp>::GetRHI());

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}

		SetShaderValue(Context.RHICmdList, ShaderRHI, CircleDofParams, CircleDofCoc(Context));
	}
};

IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFDilatePS<0>,TEXT("PostProcessDOF"),TEXT("CircleDilatePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFDilatePS<1>,TEXT("PostProcessDOF"),TEXT("CircleDilatePS"),SF_Pixel);

void FRCPassPostProcessCircleDOFDilate::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CircleDOFNear);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	uint32 NumRenderTargets = 1;

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = GSceneRenderTargets.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect / 2;

	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = FSceneRenderTargetItem();

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[2] =
	{
		DestRenderTarget0.TargetableTexture,
		DestRenderTarget1.TargetableTexture
	};
	SetRenderTargets(Context.RHICmdList, NumRenderTargets, RenderTargets, FTextureRHIParamRef(), 0, NULL);

	FLinearColor ClearColors[2] = 
	{
		FLinearColor(0, 0, 0, 0),
		FLinearColor(0, 0, 0, 0)
	};
	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.ClearMRT(true, NumRenderTargets, ClearColors, false, 1.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

	if (false)
	{
		static FGlobalBoundShaderState BoundShaderState;


		TShaderMapRef< FPostProcessCircleDOFDilatePS<1> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(Context);
	}
	else
	{
		static FGlobalBoundShaderState BoundShaderState;

		TShaderMapRef< FPostProcessCircleDOFDilatePS<0> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(Context);
	}

	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width() + 1, DestRect.Height() + 1,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width() + 1, SrcRect.Height() + 1,
		DestSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessCircleDOFDilate::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	//	Ret.Extent = FIntPoint::DivideAndRoundUp(ret.Extent, 2);
	Ret.Extent /= 2;
	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	Ret.Reset();
	Ret.TargetableFlags &= ~(uint32)TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;

	Ret.DebugName = (InPassOutputId == ePId_Output0) ? TEXT("CircleDOFDilate0") : TEXT("CircleDOFDilate1");

	// more precision for additive blending and we need the alpha channel
	Ret.Format = PF_FloatRGBA;

	return Ret;
}









/** Encapsulates the Circle DOF pixel shader. */

static float TemporalHalton2( int32 Index, int32 Base )
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

static void TemporalRandom2(FVector2D* RESTRICT const Constant, uint32 FrameNumber)
{
	Constant->X = TemporalHalton2(FrameNumber & 1023, 2);
	Constant->Y = TemporalHalton2(FrameNumber & 1023, 3);
}

template <uint32 NearBlurEnable>
class FPostProcessCircleDOFPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessCircleDOFPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_NEAR_BLUR"), NearBlurEnable);
	}

	/** Default constructor. */
	FPostProcessCircleDOFPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;
	FShaderParameter RandomOffset;
	FShaderParameter CircleDofParams;

	/** Initialization constructor. */
	FPostProcessCircleDOFPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
		RandomOffset.Bind(Initializer.ParameterMap, TEXT("RandomOffset"));
		CircleDofParams.Bind(Initializer.ParameterMap,TEXT("CircleDofParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldParams << RandomOffset << CircleDofParams;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Clamp>::GetRHI());

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			FVector4 DepthOfFieldParamValues[2];

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}

		FVector2D RandomOffsetValue;
		TemporalRandom2(&RandomOffsetValue, Context.View.Family->FrameNumber);
		SetShaderValue(Context.RHICmdList, ShaderRHI, RandomOffset, RandomOffsetValue);

		SetShaderValue(Context.RHICmdList, ShaderRHI, CircleDofParams, CircleDofCoc(Context));
	}
};

IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFPS<0>,TEXT("PostProcessDOF"),TEXT("CirclePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFPS<1>,TEXT("PostProcessDOF"),TEXT("CirclePS"),SF_Pixel);

void FRCPassPostProcessCircleDOF::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CircleDOFApply);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	uint32 NumRenderTargets = bNearBlurEnabled ? 2 : 1;

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;

	// e.g. 4 means the input texture is 4x smaller than the buffer size
	uint32 ScaleFactor = GSceneRenderTargets.GetBufferSizeXY().X / SrcSize.X;

	FIntRect SrcRect = View.ViewRect / ScaleFactor;
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget0 = PassOutputs[0].RequestSurface(Context);
	const FSceneRenderTargetItem& DestRenderTarget1 = bNearBlurEnabled ? PassOutputs[1].RequestSurface(Context) : FSceneRenderTargetItem();

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[2] =
	{
		DestRenderTarget0.TargetableTexture,
		DestRenderTarget1.TargetableTexture
	};
	SetRenderTargets(Context.RHICmdList, NumRenderTargets, RenderTargets, FTextureRHIParamRef(), 0, NULL);

	FLinearColor ClearColors[2] = 
	{
		FLinearColor(0, 0, 0, 0),
		FLinearColor(0, 0, 0, 0)
	};
	// is optimized away if possible (RT size=view size, )
	Context.RHICmdList.ClearMRT(true, NumRenderTargets, ClearColors, false, 1.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

	if (bNearBlurEnabled)
	{
		static FGlobalBoundShaderState BoundShaderState;


		TShaderMapRef< FPostProcessCircleDOFPS<1> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(Context);
	}
	else
	{
		static FGlobalBoundShaderState BoundShaderState;

		TShaderMapRef< FPostProcessCircleDOFPS<0> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(Context);
	}

	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width() + 1, DestRect.Height() + 1,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width() + 1, SrcRect.Height() + 1,
		DestSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget0.TargetableTexture, DestRenderTarget0.ShaderResourceTexture, false, FResolveParams());
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget1.TargetableTexture, DestRenderTarget1.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessCircleDOF::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);

	Ret.Reset();
	Ret.TargetableFlags &= ~(uint32)TexCreate_UAV;
	Ret.TargetableFlags |= TexCreate_RenderTargetable;

	Ret.DebugName = (InPassOutputId == ePId_Output0) ? TEXT("CircleDOF0") : TEXT("CircleDOF1");

	// more precision for additive blending and we need the alpha channel
	Ret.Format = PF_FloatRGBA;

	return Ret;
}


/** Encapsulates  the Circle DOF recombine pixel shader. */
template <uint32 NearBlurEnable>
class FPostProcessCircleDOFRecombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessCircleDOFRecombinePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_NEAR_BLUR"), NearBlurEnable);
	}

	/** Default constructor. */
	FPostProcessCircleDOFRecombinePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldUVLimit;
	FShaderParameter RandomOffset;
	FShaderParameter CircleDofParams;

	/** Initialization constructor. */
	FPostProcessCircleDOFRecombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldUVLimit.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldUVLimit"));
		RandomOffset.Bind(Initializer.ParameterMap, TEXT("RandomOffset"));
		CircleDofParams.Bind(Initializer.ParameterMap,TEXT("CircleDofParams"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << DepthOfFieldUVLimit << RandomOffset << CircleDofParams;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		// Compute out of bounds UVs in the source texture.
		FVector4 Bounds;
		Bounds.X = (((float)((Context.View.ViewRect.Min.X + 1) & (~1))) + 3.0f) / ((float)(GSceneRenderTargets.GetBufferSizeXY().X));
		Bounds.Y = (((float)((Context.View.ViewRect.Min.Y + 1) & (~1))) + 3.0f) / ((float)(GSceneRenderTargets.GetBufferSizeXY().Y));
		Bounds.Z = (((float)(Context.View.ViewRect.Max.X & (~1))) - 3.0f) / ((float)(GSceneRenderTargets.GetBufferSizeXY().X));
		Bounds.W = (((float)(Context.View.ViewRect.Max.Y & (~1))) - 3.0f) / ((float)(GSceneRenderTargets.GetBufferSizeXY().Y));

		SetShaderValue(Context.RHICmdList, ShaderRHI, DepthOfFieldUVLimit, Bounds);

		FVector2D RandomOffsetValue;
		TemporalRandom2(&RandomOffsetValue, Context.View.Family->FrameNumber);
		SetShaderValue(Context.RHICmdList, ShaderRHI, RandomOffset, RandomOffsetValue);

		SetShaderValue(Context.RHICmdList, ShaderRHI, CircleDofParams, CircleDofCoc(Context));
	}
};

IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFRecombinePS<0>,TEXT("PostProcessDOF"),TEXT("MainCircleRecombinePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessCircleDOFRecombinePS<1>,TEXT("PostProcessDOF"),TEXT("MainCircleRecombinePS"),SF_Pixel);

void FRCPassPostProcessCircleDOFRecombine::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CircleDOFRecombine);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint TexSize = InputDesc->Extent;

	// usually 1, 2, 4 or 8
	uint32 ScaleToFullRes = GSceneRenderTargets.GetBufferSizeXY().X / TexSize.X;

	FIntRect HalfResViewRect = View.ViewRect / ScaleToFullRes;

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

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

	if (bNearBlurEnabled)
	{
		static FGlobalBoundShaderState BoundShaderState;

		TShaderMapRef< FPostProcessCircleDOFRecombinePS<1> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
		PixelShader->SetParameters(Context);
	}
	else
	{
		static FGlobalBoundShaderState BoundShaderState;

		TShaderMapRef< FPostProcessCircleDOFRecombinePS<0> > PixelShader(ShaderMap);
		SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
		PixelShader->SetParameters(Context);
	}

	VertexShader->SetParameters(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		TexSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessCircleDOFRecombine::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("CircleDOFRecombine");

	return Ret;
}
