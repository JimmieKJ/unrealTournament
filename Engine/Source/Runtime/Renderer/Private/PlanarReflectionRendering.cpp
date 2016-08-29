// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 PlanarReflectionRendering.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessing.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "ScreenRendering.h"
#include "ShaderParameterUtils.h"
#include "LightRendering.h"
#include "SceneUtils.h"
#include "Components/PlanarReflectionComponent.h"
#include "PlanarReflectionSceneProxy.h"
#include "PlanarReflectionRendering.h"

template< bool bEnablePlanarReflectionPrefilter >
class FPrefilterPlanarReflectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPrefilterPlanarReflectionPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return bEnablePlanarReflectionPrefilter ? IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) : true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("ENABLE_PLANAR_REFLECTIONS_PREFILTER"), bEnablePlanarReflectionPrefilter);
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPrefilterPlanarReflectionPS() {}

	/** Initialization constructor. */
	FPrefilterPlanarReflectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		KernelRadiusY.Bind(Initializer.ParameterMap, TEXT("KernelRadiusY"));
		InvPrefilterRoughnessDistance.Bind(Initializer.ParameterMap, TEXT("InvPrefilterRoughnessDistance"));
		SceneColorInputTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorInputTexture"));
		SceneColorInputSampler.Bind(Initializer.ParameterMap, TEXT("SceneColorInputSampler"));
		DeferredParameters.Bind(Initializer.ParameterMap);
		PlanarReflectionParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FPlanarReflectionSceneProxy* ReflectionSceneProxy, FTextureRHIParamRef SceneColorInput)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View, ESceneRenderTargetsMode::SetTextures);
		PlanarReflectionParameters.SetParameters(RHICmdList, ShaderRHI, ReflectionSceneProxy);

		int32 RenderTargetSizeY = ReflectionSceneProxy->RenderTarget->GetSizeXY().Y;
		const float KernelRadiusYValue = FMath::Clamp(ReflectionSceneProxy->PrefilterRoughness, 0.0f, 0.04f) * RenderTargetSizeY;
		SetShaderValue(RHICmdList, ShaderRHI, KernelRadiusY, KernelRadiusYValue);

		SetShaderValue(RHICmdList, ShaderRHI, InvPrefilterRoughnessDistance, 1.0f / FMath::Max(ReflectionSceneProxy->PrefilterRoughnessDistance, DELTA));

		SetTextureParameter(RHICmdList, ShaderRHI, SceneColorInputTexture, SceneColorInputSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), SceneColorInput);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << KernelRadiusY;
		Ar << InvPrefilterRoughnessDistance;
		Ar << SceneColorInputTexture;
		Ar << SceneColorInputSampler;
		Ar << PlanarReflectionParameters;
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter KernelRadiusY;
	FShaderParameter InvPrefilterRoughnessDistance;
	FShaderResourceParameter SceneColorInputTexture;
	FShaderResourceParameter SceneColorInputSampler;
	FPlanarReflectionParameters PlanarReflectionParameters;
	FDeferredPixelShaderParameters DeferredParameters;
};

IMPLEMENT_SHADER_TYPE(template<>, FPrefilterPlanarReflectionPS<false>, TEXT("PlanarReflectionShaders"), TEXT("PrefilterPlanarReflectionPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FPrefilterPlanarReflectionPS<true>, TEXT("PlanarReflectionShaders"), TEXT("PrefilterPlanarReflectionPS"), SF_Pixel);

template<bool bEnablePlanarReflectionPrefilter>
void PrefilterPlanarReflection(FRHICommandListImmediate& RHICmdList, FViewInfo& View, const FPlanarReflectionSceneProxy* ReflectionSceneProxy, const FRenderTarget* Target)
{
	FTextureRHIParamRef SceneColorInput = FSceneRenderTargets::Get(RHICmdList).GetSceneColorTexture();

	if(View.FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		// Note: null velocity buffer, so dynamic object temporal AA will not be correct
		TRefCountPtr<IPooledRenderTarget> VelocityRT;
		TRefCountPtr<IPooledRenderTarget> FilteredSceneColor;
		GPostProcessing.ProcessPlanarReflection(RHICmdList, View, VelocityRT, FilteredSceneColor);

		if (FilteredSceneColor)
		{
			SceneColorInput = FilteredSceneColor->GetRenderTargetItem().ShaderResourceTexture;
		}
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, PrefilterPlanarReflection);

		FRHIRenderTargetView ColorView(Target->GetRenderTargetTexture(), 0, -1, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::EStore);
		FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView());
		RHICmdList.SetRenderTargetsAndClear(Info);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		TShaderMapRef<TDeferredLightVS<false> > VertexShader(View.ShaderMap);
		TShaderMapRef<FPrefilterPlanarReflectionPS<bEnablePlanarReflectionPrefilter> > PixelShader(View.ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(RHICmdList, View, ReflectionSceneProxy, SceneColorInput);
		VertexShader->SetSimpleLightParameters(RHICmdList, View, FSphere(0));

		DrawRectangle(
			RHICmdList,
			0, 0,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Size(),
			FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
}

extern float GetSceneColorClearAlpha();

static void UpdatePlanarReflectionContents_RenderThread(
	FRHICommandListImmediate& RHICmdList, 
	FSceneRenderer* MainSceneRenderer, 
	FSceneRenderer* SceneRenderer, 
	const FPlanarReflectionSceneProxy* SceneProxy,
	FRenderTarget* RenderTarget, 
	FTexture* RenderTargetTexture, 
	const FName OwnerName, 
	const FResolveParams& ResolveParams, 
	bool bUseSceneColorTexture)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderPlanarReflection);

	FViewInfo& MainView = MainSceneRenderer->Views[0];
	FBox PlanarReflectionBounds = SceneProxy->WorldBounds;

	if (MainView.ViewFrustum.IntersectBox(PlanarReflectionBounds.GetCenter(), PlanarReflectionBounds.GetExtent()))
	{
		FSceneViewState* MainViewState = MainView.ViewState;
		bool bIsVisible = true;

		if (MainViewState)
		{
			FIndividualOcclusionHistory& OcclusionHistory = MainViewState->PlanarReflectionOcclusionHistories.FindOrAdd(SceneProxy->PlanarReflectionId);

			// +1 to buffered frames because the query is submitted late into the main frame, but read at the beginning of a reflection capture frame
			const int32 NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames() + 1;
			// +1 to frame counter because we are operating before the main view's InitViews, which is where OcclusionFrameCounter is incremented
			uint32 OcclusionFrameCounter = MainViewState->OcclusionFrameCounter + 1;
			FRenderQueryRHIRef& PastQuery = OcclusionHistory.GetPastQuery(OcclusionFrameCounter, NumBufferedFrames);

			if (IsValidRef(PastQuery))
			{
				uint64 NumSamples = 0;
				QUICK_SCOPE_CYCLE_COUNTER(STAT_PlanarReflectionOcclusionQueryResults);

				if (RHIGetRenderQueryResult(PastQuery.GetReference(), NumSamples, true))
				{
					bIsVisible = NumSamples > 0;
				}
			}
		}
		
		if (bIsVisible)
		{
			FMemMark MemStackMark(FMemStack::Get());

			// update any resources that needed a deferred update
			FDeferredUpdateResource::UpdateResources(RHICmdList);

			{
#if WANTS_DRAW_MESH_EVENTS
				FString EventName;
				OwnerName.ToString(EventName);
				SCOPED_DRAW_EVENTF(RHICmdList, SceneCapture, TEXT("PlanarReflection %s"), *EventName);
#else
				SCOPED_DRAW_EVENT(RHICmdList, UpdatePlanarReflectionContent_RenderThread);
#endif

				FViewInfo& View = SceneRenderer->Views[0];

				const FRenderTarget* Target = SceneRenderer->ViewFamily.RenderTarget;
				SetRenderTarget(RHICmdList, Target->GetRenderTargetTexture(), NULL, true);

				// Note: relying on GBuffer SceneColor alpha being cleared to 1 in the main scene rendering
				check(GetSceneColorClearAlpha() == 1.0f);
				RHICmdList.Clear(true, FLinearColor(0, 0, 0, 1), false, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect());

				// Render the scene normally
				{
					SCOPED_DRAW_EVENT(RHICmdList, RenderScene);
					SceneRenderer->Render(RHICmdList);
				}

				if (MainSceneRenderer->Scene->GetShadingPath() == EShadingPath::Deferred)
				{
					PrefilterPlanarReflection<true>(RHICmdList, View, SceneProxy, Target);
					RHICmdList.CopyToResolveTarget(RenderTarget->GetRenderTargetTexture(), RenderTargetTexture->TextureRHI, false, ResolveParams);
				}
				else
				{
					PrefilterPlanarReflection<false>(RHICmdList, View, SceneProxy, Target);
					RHICmdList.CopyToResolveTarget(RenderTarget->GetRenderTargetTexture(), RenderTargetTexture->TextureRHI, false, ResolveParams);
				}
			}
			FSceneRenderer::WaitForTasksClearSnapshotsAndDeleteSceneRenderer(RHICmdList, SceneRenderer);
		}
	}
}

extern void BuildProjectionMatrix(FIntPoint RenderTargetSize, ECameraProjectionMode::Type ProjectionType, float FOV, float OrthoWidth, FMatrix& ProjectionMatrix);

extern FSceneRenderer* CreateSceneRendererForSceneCapture(
	FScene* Scene,
	USceneCaptureComponent* SceneCaptureComponent,
	FRenderTarget* RenderTarget,
	FIntPoint RenderTargetSize,
	const FMatrix& ViewRotationMatrix,
	const FVector& ViewLocation,
	const FMatrix& ProjectionMatrix,
	float MaxViewDistance,
	bool bCaptureSceneColour,
	bool bIsPlanarReflection,
	FPostProcessSettings* PostProcessSettings,
	float PostProcessBlendWeight);

void FScene::UpdatePlanarReflectionContents(UPlanarReflectionComponent* CaptureComponent, FSceneRenderer& MainSceneRenderer)
{
	check(CaptureComponent);

	{
		//@todo - splitscreen / VR
		const FSceneView& ParentView = *MainSceneRenderer.ViewFamily.Views[0];
		FVector2D DesiredPlanarReflectionTextureSizeFloat = FVector2D(ParentView.ViewRect.Size().X, ParentView.ViewRect.Size().Y) * .01f * FMath::Clamp(CaptureComponent->ScreenPercentage, 25, 100);
		FIntPoint DesiredPlanarReflectionTextureSize;
		DesiredPlanarReflectionTextureSize.X = FMath::Clamp(FMath::TruncToInt(DesiredPlanarReflectionTextureSizeFloat.X), 1, ParentView.ViewRect.Size().X);
		DesiredPlanarReflectionTextureSize.Y = FMath::Clamp(FMath::TruncToInt(DesiredPlanarReflectionTextureSizeFloat.Y), 1, ParentView.ViewRect.Size().Y);

		if (CaptureComponent->RenderTarget != NULL && CaptureComponent->RenderTarget->GetSizeXY() != DesiredPlanarReflectionTextureSize)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( 
				ReleaseRenderTargetCommand,
				FPlanarReflectionRenderTarget*, RenderTarget, CaptureComponent->RenderTarget,
			{
				RenderTarget->ReleaseResource();
				delete RenderTarget;
			});

			CaptureComponent->RenderTarget = NULL;
		}

		if (CaptureComponent->RenderTarget == NULL)
		{
			CaptureComponent->RenderTarget = new FPlanarReflectionRenderTarget(DesiredPlanarReflectionTextureSize);

			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER( 
				InitRenderTargetCommand,
				FPlanarReflectionRenderTarget*, RenderTarget, CaptureComponent->RenderTarget,
				FPlanarReflectionSceneProxy*, SceneProxy, CaptureComponent->SceneProxy,
			{
				RenderTarget->InitResource();
				SceneProxy->RenderTarget = RenderTarget;
			});
		}

		FMatrix ComponentTransform = CaptureComponent->ComponentToWorld.ToMatrixWithScale();
		FPlane MirrorPlane = FPlane(ComponentTransform.TransformPosition(FVector::ZeroVector), ComponentTransform.TransformVector(FVector(0, 0, 1)));

		// Create a mirror matrix and premultiply the view transform by it
		FMirrorMatrix MirrorMatrix(MirrorPlane);
		FMatrix ViewMatrix(MirrorMatrix * ParentView.ViewMatrices.ViewMatrix);
		FVector ViewLocation = ViewMatrix.InverseTransformPosition(FVector::ZeroVector);
		FMatrix ViewRotationMatrix = ViewMatrix.RemoveTranslation();
		float FOV = FMath::Atan(1.0f / ParentView.ViewMatrices.ProjMatrix.M[0][0]);

		FMatrix ProjectionMatrix;
		BuildProjectionMatrix(DesiredPlanarReflectionTextureSize, ECameraProjectionMode::Perspective, FOV + CaptureComponent->ExtraFOV * (float)PI / 180.0f, 1.0f, ProjectionMatrix);
		FPostProcessSettings PostProcessSettings;

		FSceneRenderer* SceneRenderer = CreateSceneRendererForSceneCapture(this, CaptureComponent, CaptureComponent->RenderTarget, DesiredPlanarReflectionTextureSize, ViewRotationMatrix, ViewLocation, ProjectionMatrix, CaptureComponent->MaxViewDistanceOverride, true, true, &PostProcessSettings, 1.0f);

		CaptureComponent->ProjectionWithExtraFOV = ProjectionMatrix;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateProxyCommand,
			FMatrix, ProjectionMatrix, ProjectionMatrix,
			FPlanarReflectionSceneProxy*, SceneProxy, CaptureComponent->SceneProxy,
		{
			SceneProxy->ProjectionWithExtraFOV = ProjectionMatrix;
		});
		
		SceneRenderer->Views[0].GlobalClippingPlane = MirrorPlane;
		// Jitter can't be removed completely due to the clipping plane
		// Also, this prevents the prefilter pass, which reads from jittered depth, from having to do special handling of it's depth-dependent input
		SceneRenderer->Views[0].bAllowTemporalJitter = false;
		SceneRenderer->Views[0].bRenderSceneTwoSided = CaptureComponent->bRenderSceneTwoSided;

		const FName OwnerName = CaptureComponent->GetOwner() ? CaptureComponent->GetOwner()->GetFName() : NAME_None;

		ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER( 
			CaptureCommand,
			FSceneRenderer*, MainSceneRenderer, &MainSceneRenderer,
			FSceneRenderer*, SceneRenderer, SceneRenderer,
			FPlanarReflectionSceneProxy*, SceneProxy, CaptureComponent->SceneProxy,
			FPlanarReflectionRenderTarget*, RenderTarget, CaptureComponent->RenderTarget,
			FName, OwnerName, OwnerName,
		{
			UpdatePlanarReflectionContents_RenderThread(RHICmdList, MainSceneRenderer, SceneRenderer, SceneProxy, RenderTarget, RenderTarget, OwnerName, FResolveParams(), true);
		});
	}
}

void FScene::AddPlanarReflection(UPlanarReflectionComponent* Component)
{
	check(Component->SceneProxy);
	PlanarReflections_GameThread.Add(Component);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FAddPlanarReflectionCommand,
		FPlanarReflectionSceneProxy*,SceneProxy,Component->SceneProxy,
		FScene*,Scene,this,
	{
		Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;
		Scene->PlanarReflections.Add(SceneProxy);
	});
}

void FScene::RemovePlanarReflection(UPlanarReflectionComponent* Component) 
{
	check(Component->SceneProxy);
	PlanarReflections_GameThread.Remove(Component);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FRemovePlanarReflectionCommand,
		FPlanarReflectionSceneProxy*,SceneProxy,Component->SceneProxy,
		FScene*,Scene,this,
	{
		Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;
		Scene->PlanarReflections.Remove(SceneProxy);
	});
}

void FScene::UpdatePlanarReflectionTransform(UPlanarReflectionComponent* Component)
{	
	check(Component->SceneProxy);

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		FUpdatePlanarReflectionCommand,
		FPlanarReflectionSceneProxy*,SceneProxy,Component->SceneProxy,
		FMatrix,Transform,Component->ComponentToWorld.ToMatrixWithScale(),
		FScene*,Scene,this,
	{
		Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;
		SceneProxy->UpdateTransform(Transform);
	});
}

class FPlanarReflectionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPlanarReflectionPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPlanarReflectionPS() {}

	/** Initialization constructor. */
	FPlanarReflectionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		PlanarReflectionParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FPlanarReflectionSceneProxy* ReflectionSceneProxy)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		PlanarReflectionParameters.SetParameters(RHICmdList, ShaderRHI, ReflectionSceneProxy);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PlanarReflectionParameters;
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FPlanarReflectionParameters PlanarReflectionParameters;
	FDeferredPixelShaderParameters DeferredParameters;
};

IMPLEMENT_SHADER_TYPE(,FPlanarReflectionPS,TEXT("PlanarReflectionShaders"),TEXT("PlanarReflectionPS"),SF_Pixel);

bool FDeferredShadingSceneRenderer::RenderDeferredPlanarReflections(FRHICommandListImmediate& RHICmdList, bool bLightAccumulationIsInUse, TRefCountPtr<IPooledRenderTarget>& Output)
{
	bool bAnyViewIsReflectionCapture = false;
	bool bAnyVisiblePlanarReflections = false;

	for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		for (int32 PlanarReflectionIndex = 0; PlanarReflectionIndex < Scene->PlanarReflections.Num(); PlanarReflectionIndex++)
		{
			FPlanarReflectionSceneProxy* ReflectionSceneProxy = Scene->PlanarReflections[PlanarReflectionIndex];

			if (View.ViewFrustum.IntersectBox(ReflectionSceneProxy->WorldBounds.GetCenter(), ReflectionSceneProxy->WorldBounds.GetExtent()))
			{
				bAnyVisiblePlanarReflections = true;
			}
		}

		bAnyViewIsReflectionCapture = bAnyViewIsReflectionCapture || View.bIsPlanarReflection || View.bIsReflectionCapture;
	}

	// Prevent reflection recursion, or view-dependent planar reflections being seen in reflection captures
	if (Scene->PlanarReflections.Num() > 0 && !bAnyViewIsReflectionCapture && bAnyVisiblePlanarReflections)
	{
		bool bSSRAsInput = true;

		if (Output == GSystemTextures.BlackDummy)
		{
			bSSRAsInput = false;
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			if (bLightAccumulationIsInUse)
			{
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_FloatRGBA, FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, Output, TEXT("PlanarReflectionComposite"));
			}
			else
			{
				Output = SceneContext.LightAccumulation;
			}
		}

		SCOPED_DRAW_EVENT(RHICmdList, CompositePlanarReflections);

		SetRenderTarget(RHICmdList, Output->GetRenderTargetItem().TargetableTexture, NULL);

		if (!bSSRAsInput)
		{
			RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, 0, false, 0, FIntRect());
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			// Blend over previous reflections in the output target (SSR or planar reflections that have already been rendered)
			// Planar reflections win over SSR and reflection environment
			//@todo - this is order dependent blending, but ordering is coming from registration order
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Max, BF_One, BF_One>::GetRHI());
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

			for (int32 PlanarReflectionIndex = 0; PlanarReflectionIndex < Scene->PlanarReflections.Num(); PlanarReflectionIndex++)
			{
				FPlanarReflectionSceneProxy* ReflectionSceneProxy = Scene->PlanarReflections[PlanarReflectionIndex];

				if (View.ViewFrustum.IntersectBox(ReflectionSceneProxy->WorldBounds.GetCenter(), ReflectionSceneProxy->WorldBounds.GetExtent()))
				{
					SCOPED_DRAW_EVENTF(RHICmdList, PlanarReflection, *ReflectionSceneProxy->OwnerName.ToString());

					TShaderMapRef<TDeferredLightVS<false> > VertexShader(View.ShaderMap);
					TShaderMapRef<FPlanarReflectionPS> PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

					PixelShader->SetParameters(RHICmdList, View, ReflectionSceneProxy);
					VertexShader->SetSimpleLightParameters(RHICmdList, View, FSphere(0));

					DrawRectangle(
						RHICmdList,
						0, 0,
						View.ViewRect.Width(), View.ViewRect.Height(),
						View.ViewRect.Min.X, View.ViewRect.Min.Y,
						View.ViewRect.Width(), View.ViewRect.Height(),
						View.ViewRect.Size(),
						FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
						*VertexShader,
						EDRF_UseTriangleOptimization);
				}
			}
		}

		RHICmdList.CopyToResolveTarget(Output->GetRenderTargetItem().TargetableTexture, Output->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());

		return true;
	}

	return false;
}

void FPlanarReflectionParameters::SetParameters(FRHICommandList& RHICmdList, FPixelShaderRHIParamRef ShaderRHI, const FPlanarReflectionSceneProxy* ReflectionSceneProxy)
{
	// Degenerate plane causes shader to branch around the reflection lookup
	FPlane ReflectionPlaneValue(FVector4(0, 0, 0, 0));
	FTexture* PlanarReflectionTextureValue = GBlackTexture;

	if (ReflectionSceneProxy && ReflectionSceneProxy->RenderTarget)
	{
		ReflectionPlaneValue = ReflectionSceneProxy->ReflectionPlane;
		PlanarReflectionTextureValue = ReflectionSceneProxy->RenderTarget;
			
		SetShaderValue(RHICmdList, ShaderRHI, PlanarReflectionOrigin, ReflectionSceneProxy->PlanarReflectionOrigin);
		SetShaderValue(RHICmdList, ShaderRHI, PlanarReflectionXAxis, ReflectionSceneProxy->PlanarReflectionXAxis);
		SetShaderValue(RHICmdList, ShaderRHI, PlanarReflectionYAxis, ReflectionSceneProxy->PlanarReflectionYAxis);
		SetShaderValue(RHICmdList, ShaderRHI, InverseTransposeMirrorMatrix, ReflectionSceneProxy->InverseTransposeMirrorMatrix);
		SetShaderValue(RHICmdList, ShaderRHI, PlanarReflectionParameters, ReflectionSceneProxy->PlanarReflectionParameters);
		SetShaderValue(RHICmdList, ShaderRHI, PlanarReflectionParameters2, ReflectionSceneProxy->PlanarReflectionParameters2);
		SetShaderValue(RHICmdList, ShaderRHI, ProjectionWithExtraFOV, ReflectionSceneProxy->ProjectionWithExtraFOV);
	}

	SetShaderValue(RHICmdList, ShaderRHI, ReflectionPlane, ReflectionPlaneValue);
	SetTextureParameter(RHICmdList, ShaderRHI, PlanarReflectionTexture, PlanarReflectionSampler, PlanarReflectionTextureValue);
}

