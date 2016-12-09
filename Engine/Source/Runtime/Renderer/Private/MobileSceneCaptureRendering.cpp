// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MobileSceneCaptureRendering.cpp - Mobile specific scene capture code.
=============================================================================*/

#include "MobileSceneCaptureRendering.h"
#include "Misc/MemStack.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "SceneInterface.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "TextureResource.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRendering.h"
#include "PostProcess/RenderTargetPool.h"
#include "PostProcess/SceneFilterRendering.h"
#include "ScreenRendering.h"


/**
* Shader set for the copy of scene color to capture target, decoding mosaic or RGBE encoded HDR image as part of a
* copy operation. Alpha channel will contain opacity information. (Determined from depth buffer content)
*/

// Use same defines as deferred for capture source defines,
extern const TCHAR* GShaderSourceModeDefineName[];

template<bool bDemosaic, ESceneCaptureSource CaptureSource>
class FMobileSceneCaptureCopyPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileSceneCaptureCopyPS, Global)
public:

	static bool ShouldCache(EShaderPlatform Platform) { return IsMobilePlatform(Platform); }

	FMobileSceneCaptureCopyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
		SceneTextureParameters.Bind(Initializer.ParameterMap);
	}
	FMobileSceneCaptureCopyPS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("MOBILE_FORCE_DEPTH_TEXTURE_READS"), 1u);
		OutEnvironment.SetDefine(TEXT("DECODING_MOSAIC"), bDemosaic ? 1u : 0u);
		const TCHAR* DefineName = GShaderSourceModeDefineName[CaptureSource];
		if (DefineName)
		{
			OutEnvironment.SetDefine(DefineName, 1);
		}
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << InTextureSampler;
		Ar << SceneTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FSceneTextureShaderParameters SceneTextureParameters;
};

/**
* A vertex shader for rendering a textured screen element.
* Additional texcoords are used when demosaic is required.
*/
template<bool bDemosaic>
class FMobileSceneCaptureCopyVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMobileSceneCaptureCopyVS, Global)
public:

	static bool ShouldCache(EShaderPlatform Platform) { return IsMobilePlatform(Platform); }

	FMobileSceneCaptureCopyVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InvTexSizeParameter.Bind(Initializer.ParameterMap, TEXT("InvTexSize"));
	}
	FMobileSceneCaptureCopyVS() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DECODING_MOSAIC"), bDemosaic ? 1u : 0u);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FIntPoint& SourceTexSize)
	{
		FGlobalShader::SetParameters(RHICmdList, GetVertexShader(), View);
		if (InvTexSizeParameter.IsBound())
		{
			FVector2D InvTexSize(1.0f / SourceTexSize.X, 1.0f / SourceTexSize.Y);
			SetShaderValue(RHICmdList, GetVertexShader(), InvTexSizeParameter, InvTexSize);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InvTexSizeParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter InvTexSizeParameter;
};

#define IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCENETYPE) \
typedef FMobileSceneCaptureCopyPS<false,SCENETYPE> FMobileSceneCaptureCopyPS##SCENETYPE;\
typedef FMobileSceneCaptureCopyPS<true,SCENETYPE> FMobileSceneCaptureCopyPS_Mosaic##SCENETYPE;\
IMPLEMENT_SHADER_TYPE(template<>, FMobileSceneCaptureCopyPS##SCENETYPE, TEXT("MobileSceneCapture"), TEXT("MainCopyPS"), SF_Pixel); \
IMPLEMENT_SHADER_TYPE(template<>, FMobileSceneCaptureCopyPS_Mosaic##SCENETYPE, TEXT("MobileSceneCapture"), TEXT("MainCopyPS"), SF_Pixel);

IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCS_SceneColorHDR);
IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCS_SceneColorHDRNoAlpha);
IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCS_SceneColorSceneDepth);
IMPLEMENT_MOBILE_SCENE_CAPTURECOPY(SCS_SceneDepth);
IMPLEMENT_SHADER_TYPE(template<>, FMobileSceneCaptureCopyVS<false>, TEXT("MobileSceneCapture"), TEXT("MainCopyVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, FMobileSceneCaptureCopyVS<true>, TEXT("MobileSceneCapture"), TEXT("MainCopyVS"), SF_Vertex);
 

template <bool bDemosaic, ESceneCaptureSource CaptureSource>
static FShader* SetCaptureToTargetShaders(FRHICommandListImmediate& RHICmdList, FViewInfo& View, const FIntPoint& SourceTexSize, FTextureRHIParamRef SourceTextureRHI)
{
	TShaderMapRef<FMobileSceneCaptureCopyVS<bDemosaic>> VertexShader(View.ShaderMap);
	TShaderMapRef<FMobileSceneCaptureCopyPS<bDemosaic, CaptureSource>> PixelShader(View.ShaderMap);
	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetParameters(RHICmdList, View, SourceTexSize);
	PixelShader->SetParameters(RHICmdList, View, TStaticSamplerState<SF_Point>::GetRHI(), SourceTextureRHI);

	return *VertexShader;
}

template <bool bDemosaic>
static FShader* SetCaptureToTargetShaders(FRHICommandListImmediate& RHICmdList, ESceneCaptureSource CaptureSource, FViewInfo& View, const FIntPoint& SourceTexSize, FTextureRHIParamRef SourceTextureRHI)
{
	switch (CaptureSource)
	{
		case SCS_SceneColorHDR:
			return SetCaptureToTargetShaders<bDemosaic, SCS_SceneColorHDR>(RHICmdList, View, SourceTexSize, SourceTextureRHI);
		case SCS_FinalColorLDR:
		case SCS_SceneColorHDRNoAlpha:
			return SetCaptureToTargetShaders<bDemosaic, SCS_SceneColorHDRNoAlpha>(RHICmdList, View, SourceTexSize, SourceTextureRHI);
		case SCS_SceneColorSceneDepth:
			return SetCaptureToTargetShaders<bDemosaic, SCS_SceneColorSceneDepth>(RHICmdList, View, SourceTexSize, SourceTextureRHI);
		case SCS_SceneDepth:
			return SetCaptureToTargetShaders<bDemosaic, SCS_SceneDepth>(RHICmdList, View, SourceTexSize, SourceTextureRHI);
		default:
			checkNoEntry();
			return nullptr;
	}
}

// Copies into render target, optionally flipping it in the Y-axis
static void CopyCaptureToTarget(
	FRHICommandListImmediate& RHICmdList, 
	const FRenderTarget* Target, 
	const FIntPoint& TargetSize, 
	FViewInfo& View, 
	const FIntRect& ViewRect, 
	FTextureRHIParamRef SourceTextureRHI, 
	bool bNeedsFlippedRenderTarget,
	FSceneRenderer* SceneRenderer)
{
	FDrawingPolicyRenderState DrawRenderState(&RHICmdList, View);
	ESceneCaptureSource CaptureSource = View.Family->SceneCaptureSource;
	ESceneCaptureCompositeMode CaptureCompositeMode = View.Family->SceneCaptureCompositeMode;

	// Normal and BaseColor not supported on mobile, fall back to scene colour.
	if (CaptureSource == SCS_Normal || CaptureSource == SCS_BaseColor)
	{
		CaptureSource = SCS_SceneColorHDR;
	}

	ERenderTargetLoadAction RTLoadAction;
	if (CaptureSource == SCS_SceneColorHDR && CaptureCompositeMode == SCCM_Composite)
	{
		// Blend with existing render target color. Scene capture color is already pre-multiplied by alpha.
		DrawRenderState.SetBlendState(RHICmdList, TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI());
		RTLoadAction = ERenderTargetLoadAction::ELoad;
	}
	else if (CaptureSource == SCS_SceneColorHDR && CaptureCompositeMode == SCCM_Additive)
	{
		// Add to existing render target color. Scene capture color is already pre-multiplied by alpha.
		DrawRenderState.SetBlendState(RHICmdList, TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI());
		RTLoadAction = ERenderTargetLoadAction::ELoad;
	}
	else
	{
		RTLoadAction = ERenderTargetLoadAction::ENoAction;
		DrawRenderState.SetBlendState(RHICmdList, TStaticBlendState<>::GetRHI());
	}

	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	DrawRenderState.SetDepthStencilState(RHICmdList, TStaticDepthStencilState<false, CF_Always>::GetRHI());

	FRHIRenderTargetView ColorView(Target->GetRenderTargetTexture(), 0, -1, RTLoadAction, ERenderTargetStoreAction::EStore);
	FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView());
	RHICmdList.SetRenderTargetsAndClear(Info);

	const bool bUsingDemosaic = IsMobileHDRMosaic();
	FShader* VertexShader;
	FIntPoint SourceTexSize = bNeedsFlippedRenderTarget ? TargetSize : FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
	if (bUsingDemosaic)
	{
		VertexShader = SetCaptureToTargetShaders<true>(RHICmdList, CaptureSource, View, SourceTexSize, SourceTextureRHI);
	}
	else
	{
		VertexShader = SetCaptureToTargetShaders<false>(RHICmdList, CaptureSource, View, SourceTexSize, SourceTextureRHI);
	}

	if (bNeedsFlippedRenderTarget)
	{
		DrawRectangle(
			RHICmdList,
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, ViewRect.Height() - ViewRect.Min.Y,
			ViewRect.Width(), -ViewRect.Height(),
			TargetSize,
			SourceTexSize,
			VertexShader,
			EDRF_UseTriangleOptimization);
	}
	else
	{
		DrawRectangle(
			RHICmdList,
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			TargetSize,
			SourceTexSize,
			VertexShader,
			EDRF_UseTriangleOptimization);
	}

	// if opacity is needed.
	if (CaptureSource == SCS_SceneColorHDR)
	{
		// render translucent opacity. (to scene color)
		check(View.Family->Scene->GetShadingPath() == EShadingPath::Mobile);
		FMobileSceneRenderer* MobileSceneRenderer = (FMobileSceneRenderer*)SceneRenderer;
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EClearColorExistingDepth);
		MobileSceneRenderer->RenderInverseOpacity(RHICmdList, View, DrawRenderState);

		// Set capture target.
		FRHIRenderTargetView OpacityView(Target->GetRenderTargetTexture(), 0, -1, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore);
		FRHISetRenderTargetsInfo OpacityInfo(1, &OpacityView, FRHIDepthRenderTargetView());
		RHICmdList.SetRenderTargetsAndClear(OpacityInfo);

		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
		DrawRenderState.SetDepthStencilState(RHICmdList, TStaticDepthStencilState<false, CF_Always>::GetRHI());
		// Note lack of inverse, both the target and source images are already inverted.
		DrawRenderState.SetBlendState(RHICmdList, TStaticBlendState<CW_ALPHA, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI());

		// Combine translucent opacity pass to earlier opaque pass to build final inverse opacity.
		TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(View.ShaderMap);
		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

		ScreenVertexShader->SetParameters(RHICmdList, View);
		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTextureRHI);

		DrawRectangle(
			RHICmdList,
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			ViewRect.Min.X, ViewRect.Min.Y,
			ViewRect.Width(), ViewRect.Height(),
			TargetSize,
			SourceTexSize,
			*ScreenVertexShader,
			EDRF_UseTriangleOptimization);
	}
}

void UpdateSceneCaptureContentMobile_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FSceneRenderer* SceneRenderer,
	FRenderTarget* RenderTarget,
	FTexture* RenderTargetTexture,
	const FName OwnerName,
	const FResolveParams& ResolveParams)
{
	FMemMark MemStackMark(FMemStack::Get());

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources(RHICmdList);
	bool bUseSceneTextures = SceneRenderer->ViewFamily.SceneCaptureSource != SCS_FinalColorLDR;

	{
#if WANTS_DRAW_MESH_EVENTS
		FString EventName;
		OwnerName.ToString(EventName);
		SCOPED_DRAW_EVENTF(RHICmdList, SceneCaptureMobile, TEXT("SceneCaptureMobile %s"), *EventName);
#else
		SCOPED_DRAW_EVENT(RHICmdList, UpdateSceneCaptureContentMobile_RenderThread);
#endif

		const bool bIsMobileHDR = IsMobileHDR();
		const bool bRHINeedsFlip = RHINeedsToSwitchVerticalAxis(GMaxRHIShaderPlatform);
		// note that GLES will flip the image during post processing. this needs flipping again so it is correct for texture addressing.
		const bool bNeedsFlippedRenderTarget = (!bIsMobileHDR || !bUseSceneTextures) && bRHINeedsFlip;

		// Intermediate render target that will need to be flipped (needed on !IsMobileHDR())
		TRefCountPtr<IPooledRenderTarget> FlippedPooledRenderTarget;

		const FRenderTarget* Target = SceneRenderer->ViewFamily.RenderTarget;
		if (bNeedsFlippedRenderTarget)
		{
			// We need to use an intermediate render target since the result will be flipped
			auto& RenderTargetRHI = Target->GetRenderTargetTexture();
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(Target->GetSizeXY(),
				RenderTargetRHI.GetReference()->GetFormat(),
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_RenderTargetable,
				false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, FlippedPooledRenderTarget, TEXT("SceneCaptureFlipped"));
		}

		// Helper class to allow setting render target
		struct FRenderTargetOverride : public FRenderTarget
		{
			FRenderTargetOverride(FRHITexture2D* In)
			{
				RenderTargetTextureRHI = In;
			}

			virtual FIntPoint GetSizeXY() const { return FIntPoint(RenderTargetTextureRHI->GetSizeX(), RenderTargetTextureRHI->GetSizeY()); }

			FTexture2DRHIRef GetTextureParamRef() { return RenderTargetTextureRHI; }
		} FlippedRenderTarget(
			FlippedPooledRenderTarget.GetReference()
			? FlippedPooledRenderTarget.GetReference()->GetRenderTargetItem().TargetableTexture->GetTexture2D()
			: nullptr);
		FViewInfo& View = SceneRenderer->Views[0];
		FIntRect ViewRect = View.ViewRect;
		FIntRect UnconstrainedViewRect = View.UnconstrainedViewRect;
		auto& RenderTargetRHI = Target->GetRenderTargetTexture();
		SetRenderTarget(RHICmdList, RenderTargetRHI, NULL, true);
		RHICmdList.ClearColorTexture(RenderTargetRHI, FLinearColor::Black, ViewRect);

		// Render the scene normally
		{
			SCOPED_DRAW_EVENT(RHICmdList, RenderScene);

			if (bNeedsFlippedRenderTarget)
			{
				// Hijack the render target
				SceneRenderer->ViewFamily.RenderTarget = &FlippedRenderTarget; //-V506
			}

			SceneRenderer->Render(RHICmdList);

			if (bNeedsFlippedRenderTarget)
			{
				// And restore it
				SceneRenderer->ViewFamily.RenderTarget = Target;
			}
		}

		const FIntPoint TargetSize(UnconstrainedViewRect.Width(), UnconstrainedViewRect.Height());
		if (bNeedsFlippedRenderTarget)
		{
			// We need to flip this texture upside down (since we depended on tonemapping to fix this on the hdr path)
			SCOPED_DRAW_EVENT(RHICmdList, FlipCapture);
			CopyCaptureToTarget(RHICmdList, Target, TargetSize, View, ViewRect, FlippedRenderTarget.GetTextureParamRef(), true, SceneRenderer);
		}
		else if(bUseSceneTextures)
		{
			// Copy the captured scene into the destination texture
			SCOPED_DRAW_EVENT(RHICmdList, CaptureSceneColor);
			CopyCaptureToTarget(RHICmdList, Target, TargetSize, View, ViewRect, FSceneRenderTargets::Get(RHICmdList).GetSceneColorTexture(), false, SceneRenderer);
		}

		RHICmdList.CopyToResolveTarget(RenderTarget->GetRenderTargetTexture(), RenderTargetTexture->TextureRHI, false, ResolveParams);
	}
	FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(RHICmdList, SceneRenderer);
}
