// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessBokehDOF.cpp: Post processing lens blur implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessPassThrough.h"
#include "PostProcessing.h"
#include "PostProcessBokehDOF.h"
#include "PostProcessCircleDOF.h"
#include "SceneUtils.h"



/**
 * Index buffer for drawing an individual sprite.
 */
class FBokehIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		const uint32 Size = sizeof(uint16) * 6 * 8;
		const uint32 Stride = sizeof(uint16);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(Stride, Size, BUF_Static, CreateInfo, Buffer);
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

/** Global Bokeh index buffer. */
TGlobalResource< FBokehIndexBuffer > GBokehIndexBuffer;

/** Encapsulates the post processing depth of field setup pixel shader. */
class PostProcessVisualizeDOFPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(PostProcessVisualizeDOFPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	PostProcessVisualizeDOFPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;
	FShaderParameter VisualizeColors;
	FShaderParameter CursorPos;
	FShaderResourceParameter MiniFontTexture;
	
	/** Initialization constructor. */
	PostProcessVisualizeDOFPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		DepthOfFieldParams.Bind(Initializer.ParameterMap, TEXT("DepthOfFieldParams"));
		MiniFontTexture.Bind(Initializer.ParameterMap, TEXT("MiniFontTexture"));
		VisualizeColors.Bind(Initializer.ParameterMap, TEXT("VisualizeColors"));
		CursorPos.Bind(Initializer.ParameterMap, TEXT("CursorPos"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << MiniFontTexture << DepthOfFieldParams << VisualizeColors << CursorPos;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context, const FDepthOfFieldStats& DepthOfFieldStats)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		SetTextureParameter(Context.RHICmdList, ShaderRHI, MiniFontTexture, GEngine->MiniFontTexture ? GEngine->MiniFontTexture->Resource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture);

		{
			FVector4 DepthOfFieldParamValues[2];

			// in rendertarget pixels (half res to scene color)
			FRenderingCompositeOutput* Output = Context.Pass->GetOutput(ePId_Output0);

			FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(Context, DepthOfFieldParamValues);

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, DepthOfFieldParams, DepthOfFieldParamValues, 2);
		}

		{
			// a negative values disables the cross hair feature
			FIntPoint CursorPosValue(-100,-100);
			
			if(Context.View.FinalPostProcessSettings.DepthOfFieldMethod == DOFM_CircleDOF)
			{
				CursorPosValue = Context.View.CursorPos;
			}

			SetShaderValue(Context.RHICmdList, ShaderRHI, CursorPos, CursorPosValue);
		}

		{
			FLinearColor Colors[2] = { FLinearColor(0.1f, 0.1f, 0.1f, 0), FLinearColor(0.1f, 0.1f, 0.1f, 0) };

			if(DepthOfFieldStats.bNear)
			{
				Colors[0] = FLinearColor(0, 0.8f, 0, 0);
			}
			if(DepthOfFieldStats.bFar)
			{
				Colors[1] = FLinearColor(0, 0, 0.8f, 0);
			}

			SetShaderValueArray(Context.RHICmdList, ShaderRHI, VisualizeColors, Colors, 2);
		}
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessVisualizeDOF");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("VisualizeDOFPS");
	}
};

IMPLEMENT_SHADER_TYPE3(PostProcessVisualizeDOFPS, SF_Pixel);

void FRCPassPostProcessVisualizeDOF::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeDOF);

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

	FIntRect SrcRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleFactor);
	FIntRect DestRect = SrcRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// can be optimized (don't clear areas we overwrite, don't clear when full screen),
	// needed when a camera (matinee) has black borders or with multiple viewports
	// focal distance depth is stored in the alpha channel to avoid DOF artifacts
	Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, View.FinalPostProcessSettings.DepthOfFieldFocalDistance), false, 0.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	// setup shader
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	
	{
		TShaderMapRef<PostProcessVisualizeDOFPS> PixelShader(Context.GetShaderMap());

		static FGlobalBoundShaderState BoundShaderState;

		SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context, DepthOfFieldStats);
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


	{
		FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
		FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

		float X = 30;
		float Y = 18;
		const float YStep = 14;
		const float ColumnWidth = 250;

		FString Line;

		Line = FString::Printf(TEXT("Visualize Depth of Field"));
		Canvas.DrawShadowedString(20, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 0));
		Y += YStep;

		EDepthOfFieldMethod MethodId = View.FinalPostProcessSettings.DepthOfFieldMethod;

		if(MethodId == DOFM_BokehDOF)
		{
			Line = FString::Printf(TEXT("Method: BokehDOF (blue is far, green is near, black is in focus)"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Y += YStep;
			Line = FString::Printf(TEXT("FocalDistance: %.2f"), View.FinalPostProcessSettings.DepthOfFieldFocalDistance);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("FocalRegion (Artificial, avoid): %.2f"), View.FinalPostProcessSettings.DepthOfFieldFocalRegion);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Y += YStep;
			Line = FString::Printf(TEXT("Scale: %.2f"), View.FinalPostProcessSettings.DepthOfFieldScale);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("MaxBokehSize: %.2f"), View.FinalPostProcessSettings.DepthOfFieldMaxBokehSize);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("NearTransitionRegion: %.2f"), View.FinalPostProcessSettings.DepthOfFieldNearTransitionRegion);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("FarTransitionRegion: %.2f"), View.FinalPostProcessSettings.DepthOfFieldFarTransitionRegion);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("ColorThreshold: %.2f"), View.FinalPostProcessSettings.DepthOfFieldColorThreshold);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("SizeThreshold: %.2f"), View.FinalPostProcessSettings.DepthOfFieldSizeThreshold);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Occlusion: %.2f"), View.FinalPostProcessSettings.DepthOfFieldOcclusion);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		}
		else if(MethodId == DOFM_Gaussian)
		{
			Line = FString::Printf(TEXT("Method: GaussianDOF (blue is far, green is near, grey is disabled, black is in focus)"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Y += YStep;
			Line = FString::Printf(TEXT("FocalDistance: %.2f"), View.FinalPostProcessSettings.DepthOfFieldFocalDistance);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("FocalRegion (Artificial, avoid): %.2f"), View.FinalPostProcessSettings.DepthOfFieldFocalRegion);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Y += YStep;
			Line = FString::Printf(TEXT("NearTransitionRegion: %.2f"), View.FinalPostProcessSettings.DepthOfFieldNearTransitionRegion);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("FarTransitionRegion: %.2f"), View.FinalPostProcessSettings.DepthOfFieldFarTransitionRegion);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("NearBlurSize: %.2f"), View.FinalPostProcessSettings.DepthOfFieldNearBlurSize);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("FarBlurSize: %.2f"), View.FinalPostProcessSettings.DepthOfFieldFarBlurSize);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Occlusion: %.2f"), View.FinalPostProcessSettings.DepthOfFieldOcclusion);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("SkyFocusDistance: %.2f"), View.FinalPostProcessSettings.DepthOfFieldSkyFocusDistance);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("VignetteRadius: %.2f"), View.FinalPostProcessSettings.DepthOfFieldVignetteSize);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Y += YStep;
			Line = FString::Printf(TEXT("Near:%d Far:%d"), DepthOfFieldStats.bNear ? 1 : 0, DepthOfFieldStats.bFar ? 1 : 0);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		}
		else if(MethodId == DOFM_CircleDOF)
		{
			Line = FString::Printf(TEXT("Method: CircleDOF (blue is far, green is near, black is in focus, cross hair shows Depth and CoC radius in pixel)"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Y += YStep;
			Line = FString::Printf(TEXT("FocalDistance: %.2f"), View.FinalPostProcessSettings.DepthOfFieldFocalDistance);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Aperture F-stop: %.2f"), View.FinalPostProcessSettings.DepthOfFieldFstop);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("Aperture: f/%.2f"), View.FinalPostProcessSettings.DepthOfFieldFstop);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Y += YStep;
			Line = FString::Printf(TEXT("DepthBlur (not related to Depth of Field, due to light traveling long distances in atmosphere)"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("  km for 50%: %.2f"), View.FinalPostProcessSettings.DepthOfFieldDepthBlurAmount);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Line = FString::Printf(TEXT("  Radius (pixels in 1920x): %.2f"), View.FinalPostProcessSettings.DepthOfFieldDepthBlurRadius);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
			Y += YStep;

			FVector2D Fov = View.ViewMatrices.GetHalfFieldOfViewPerAxis(); 
			
			float FocalLength = ComputeFocalLengthFromFov(View);

			Line = FString::Printf(TEXT("Field Of View in deg. (computed): %.1f x %.1f"), FMath::RadiansToDegrees(Fov.X) * 2.0f, FMath::RadiansToDegrees(Fov.Y) * 2.0f);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(0.5, 0.5, 1));
			Line = FString::Printf(TEXT("Focal Length (computed): %.1f"), FocalLength);
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(0.5, 0.5, 10));
			Line = FString::Printf(TEXT("Sensor: APS-C 24.576 mm sensor, crop-factor 1.61x"));
			Canvas.DrawShadowedString(X, Y += YStep, *Line, GetStatsFont(), FLinearColor(0.5, 0.5, 1));
		}

		Canvas.Flush_RenderThread(Context.RHICmdList);
	}

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeDOF::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.Format = PF_B8G8R8A8;
	Ret.DebugName = TEXT("VisualizeDOF");

	return Ret;
}


/** Encapsulates the post processing depth of field setup pixel shader. */
class PostProcessBokehDOFSetupPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(PostProcessBokehDOFSetupPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	/** Default constructor. */
	PostProcessBokehDOFSetupPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DepthOfFieldParams;

	/** Initialization constructor. */
	PostProcessBokehDOFSetupPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
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
		
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		{
			FVector4 DepthOfFieldParamValues[2];

			// in rendertarget pixels (half res to scene color)
			FRenderingCompositeOutput* Output = Context.Pass->GetOutput(ePId_Output0);

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
		return TEXT("MainSetupPS");
	}
};

IMPLEMENT_SHADER_TYPE3(PostProcessBokehDOFSetupPS, SF_Pixel);

void FRCPassPostProcessBokehDOFSetup::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, BokehDOFSetup);

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

	FIntRect SrcRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleFactor);
	FIntRect DestRect = FIntRect::DivideAndRoundUp(SrcRect, 2);

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	
	// can be optimized (don't clear areas we overwrite, don't clear when full screen),
	// needed when a camera (matinee) has black borders or with multiple viewports
	// focal distance depth is stored in the alpha channel to avoid DOF artifacts
	Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, View.FinalPostProcessSettings.DepthOfFieldFocalDistance), false, 0.0f, false, 0, DestRect);

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	// setup shader
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	{
		TShaderMapRef<PostProcessBokehDOFSetupPS> PixelShader(Context.GetShaderMap());

		static FGlobalBoundShaderState BoundShaderState;

		SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context);
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

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBokehDOFSetup::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.Extent /= 2;
	Ret.Extent.X = FMath::Max(1, Ret.Extent.X);
	Ret.Extent.Y = FMath::Max(1, Ret.Extent.Y);
	Ret.Format = PF_FloatRGBA;
	Ret.DebugName = TEXT("BokehDOFSetup");

	return Ret;
}

/** Encapsulates the post processing vertex shader. */
template <uint32 DOFMethod>
class FPostProcessBokehDOFVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBokehDOFVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DOF_METHOD"), DOFMethod);
	}

	/** Default constructor. */
	FPostProcessBokehDOFVS() {}
	
public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter TileCountAndSize;
	FShaderParameter KernelSize;
	FShaderParameter DepthOfFieldParams;
	FShaderParameter DepthOfFieldThresholds;
	FDeferredPixelShaderParameters DeferredParameters;

	/** Initialization constructor. */
	FPostProcessBokehDOFVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		TileCountAndSize.Bind(Initializer.ParameterMap, TEXT("TileCountAndSize"));
		KernelSize.Bind(Initializer.ParameterMap, TEXT("KernelSize"));
		DepthOfFieldParams.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldParams"));
		DepthOfFieldThresholds.Bind(Initializer.ParameterMap,TEXT("DepthOfFieldThresholds"));		
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << TileCountAndSize << KernelSize << DepthOfFieldParams << DepthOfFieldThresholds << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

	/** to have a similar interface as all other shaders */
	void SetParameters(const FRenderingCompositePassContext& Context, FIntPoint TileCountValue, uint32 TileSize, float PixelKernelSize, FIntPoint LeftTop)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
//		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		{
			FIntRect TileCountAndSizeValue(TileCountValue, FIntPoint(TileSize, TileSize));

			SetShaderValue(Context.RHICmdList, ShaderRHI, TileCountAndSize, TileCountAndSizeValue);
		}

		{
			FVector4 KernelSizeValue(PixelKernelSize, PixelKernelSize, LeftTop.X, LeftTop.Y);

			SetShaderValue(Context.RHICmdList, ShaderRHI, KernelSize, KernelSizeValue);
		}

		{
			FVector4 Value(
				Context.View.FinalPostProcessSettings.DepthOfFieldColorThreshold,
				Context.View.FinalPostProcessSettings.DepthOfFieldSizeThreshold,0);

			SetShaderValue(Context.RHICmdList, ShaderRHI, DepthOfFieldThresholds, Value);
		}

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
		return TEXT("MainVS");
	}
};

/** Encapsulates a simple copy pixel shader. */
class FPostProcessBokehDOFPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBokehDOFPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessBokehDOFPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter LensTexture;
	FShaderResourceParameter LensTextureSampler;

	/** Initialization constructor. */
	FPostProcessBokehDOFPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LensTexture.Bind(Initializer.ParameterMap,TEXT("LensTexture"));
		LensTextureSampler.Bind(Initializer.ParameterMap,TEXT("LensTextureSampler"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LensTexture << LensTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context, float PixelKernelSize)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
//		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
				
		{
			FTextureRHIParamRef TextureRHI = GWhiteTexture->TextureRHI;

			if(GEngine->DefaultBokehTexture)
			{
				TextureRHI = GEngine->DefaultBokehTexture->Resource->TextureRHI;
			}

			if(Context.View.FinalPostProcessSettings.DepthOfFieldBokehShape)
			{
				TextureRHI = Context.View.FinalPostProcessSettings.DepthOfFieldBokehShape->Resource->TextureRHI;
			}

			SetTextureParameter(Context.RHICmdList, ShaderRHI, LensTexture, LensTextureSampler, TStaticSamplerState<SF_Trilinear, AM_Border, AM_Border, AM_Clamp>::GetRHI(), TextureRHI);
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBokehDOFPS,TEXT("PostProcessBokehDOF"),TEXT("MainPS"),SF_Pixel);

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessBokehDOFVS<A> FPostProcessBokehDOFVS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessBokehDOFVS##A, SF_Vertex);

VARIATION1(0)			VARIATION1(1)			VARIATION1(2)
#undef VARIATION1

template <uint32 DOFMethod>
void FRCPassPostProcessBokehDOF::SetShaderTempl(const FRenderingCompositePassContext& Context, FIntPoint LeftTop, FIntPoint TileCount, uint32 TileSize, float PixelKernelSize)
{
	TShaderMapRef<FPostProcessBokehDOFVS<DOFMethod> > VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessBokehDOFPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GEmptyVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetParameters(Context, TileCount, TileSize, PixelKernelSize, LeftTop);
	PixelShader->SetParameters(Context, PixelKernelSize);
}

void FRCPassPostProcessBokehDOF::ComputeDepthOfFieldParams(const FRenderingCompositePassContext& Context, FVector4 Out[2])
{
	uint32 FullRes = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().Y;
	uint32 HalfRes = FMath::DivideAndRoundUp(FullRes, (uint32)2);
	uint32 BokehLayerSizeY = HalfRes * 2 + SafetyBorder;

	float SkyFocusDistance = Context.View.FinalPostProcessSettings.DepthOfFieldSkyFocusDistance;
	
	// *2 to go to account for Radius/Diameter, 100 for percent
	float DepthOfFieldVignetteSize = FMath::Max(0.0f, Context.View.FinalPostProcessSettings.DepthOfFieldVignetteSize / 100.0f * 2);
	// doesn't make much sense to expose this property as the effect is very non linear and it would cost some performance to fix that
	float DepthOfFieldVignetteFeather = 10.0f / 100.0f;

	float DepthOfFieldVignetteMul = 1.0f / DepthOfFieldVignetteFeather;
	float DepthOfFieldVignetteAdd = (0.5f - DepthOfFieldVignetteSize) * DepthOfFieldVignetteMul;

	Out[0] = FVector4(
		(SkyFocusDistance > 0) ? SkyFocusDistance : 100000000.0f,			// very large if <0 to not mask out skybox, can be optimized to disable feature completely
		DepthOfFieldVignetteMul,
		DepthOfFieldVignetteAdd,
		Context.View.FinalPostProcessSettings.DepthOfFieldOcclusion);

	FIntPoint ViewSize = Context.View.ViewRect.Size();

	float MaxBokehSizeInPixel = FMath::Max(0.0f, Context.View.FinalPostProcessSettings.DepthOfFieldMaxBokehSize) / 100.0f * ViewSize.X;
	
	// Scale and offset to put two views in one texture with safety border
	float UsedYDivTextureY = HalfRes / (float)BokehLayerSizeY;
	float YOffsetInPixel = HalfRes + SafetyBorder;
	float YOffsetInUV = (HalfRes + SafetyBorder) / (float)BokehLayerSizeY;

	Out[1] = FVector4(MaxBokehSizeInPixel, YOffsetInUV, UsedYDivTextureY, YOffsetInPixel);
}


void FRCPassPostProcessBokehDOF::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PassPostProcessBokehDOF);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	
	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	FIntPoint TexSize = InputDesc->Extent;

	// usually 1, 2, 4 or 8
	uint32 ScaleToFullRes = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / TexSize.X;

	// don't use DivideAndRoundUp as this could cause cause lookups into areas we don't have setup 
	FIntRect LocalViewRect = View.ViewRect / ScaleToFullRes;

	// contract by one half res pixel to avoid using samples outside of the input (SV runs at quarter resolution with 4 quads at once)
	// this can lead to missing content - if needed this can be made less conservative
	LocalViewRect.InflateRect(-2);

	FIntPoint LocalViewSize = LocalViewRect.Size();

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	// This clean is required to make the accumulation working
	Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 1.0f, false, 0, FIntRect());

	// we need to output to the whole rendertarget
	Context.SetViewportAndCallRHI(0, 0, 0.0f, PassOutputs[0].RenderTargetDesc.Extent.X, PassOutputs[0].RenderTargetDesc.Extent.Y, 1.0f);

	// set the state (additive blending)
	Context.RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DepthOfFieldQuality"));
	check(CVar);
	int32 DOFQuality = CVar->GetValueOnRenderThread();
	check( DOFQuality > 0 );

	bool bHighQuality = DOFQuality > 1;

	// 1: one quad per 1 half res texel
	// 2: one quad per 4 half res texel (faster, can alias - need to verify that with bilinear filtering)
	uint32 TileSize = bHighQuality ? 1 : 2;

	// input is half res, don't process last pixel line where we don't have input
	LocalViewSize.X &= ~1;
	LocalViewSize.Y &= ~1;

	FIntPoint TileCount = LocalViewSize / TileSize;

	float PixelKernelSize = Context.View.FinalPostProcessSettings.DepthOfFieldMaxBokehSize / 100.0f * LocalViewSize.X;

	FIntPoint LeftTop = LocalViewRect.Min;
	
	if(bHighQuality)
	{
		if(View.Family->EngineShowFlags.VisualizeAdaptiveDOF)
		{
			// high quality, visualize in red and green where we spend more performance
			SetShaderTempl<2>(Context, LeftTop, TileCount, TileSize, PixelKernelSize);
		}
		else
		{
			// high quality
			SetShaderTempl<1>(Context, LeftTop, TileCount, TileSize, PixelKernelSize);
		}
	}
	else
	{
		// low quality
		SetShaderTempl<0>(Context, LeftTop, TileCount, TileSize, PixelKernelSize);
	}

	// needs to be the same on shader side (faster on NVIDIA and AMD)
	int32 QuadsPerInstance = 8;

	Context.RHICmdList.SetStreamSource(0, NULL, 0, 0);
	Context.RHICmdList.DrawIndexedPrimitive(GBokehIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 32, 0, 2 * QuadsPerInstance, FMath::DivideAndRoundUp(TileCount.X * TileCount.Y, QuadsPerInstance));

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBokehDOF::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	// more precision for additive blending
	Ret.Format = PF_FloatRGBA;

	uint32 FullRes = FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY().Y;
	uint32 HalfRes = FMath::DivideAndRoundUp(FullRes, (uint32)2);

	// we need space for the front part and the back part
	Ret.Extent.Y = HalfRes * 2 + SafetyBorder;
	Ret.DebugName = TEXT("BokehDOF");

	return Ret;
}
