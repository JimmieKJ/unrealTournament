// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CompositionLighting.cpp: The center for all deferred lighting activities.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "CompositionLighting.h"
#include "PostProcessing.h"					// FPostprocessContext
#include "PostProcessInput.h"
#include "PostProcessAmbient.h"
#include "PostProcessLpvIndirect.h"
#include "PostProcessAmbientOcclusion.h"
#include "PostProcessDeferredDecals.h"
#include "BatchedElements.h"
#include "ScreenRendering.h"
#include "PostProcessPassThrough.h"
#include "ScreenSpaceReflections.h"
#include "PostProcessWeightedSampleSum.h"
#include "PostProcessTemporalAA.h"
#include "PostProcessSubsurface.h"
#include "SceneUtils.h"

/** The global center for all deferred lighting activities. */
FCompositionLighting GCompositionLighting;

// -------------------------------------------------------

static TAutoConsoleVariable<float> CVarSSSScale(
	TEXT("r.SSS.Scale"),
	1.0f,
	TEXT("Experimental screen space subsurface scattering pass")
	TEXT("(use shadingmodel SubsurfaceProfile, get near to the object as the default)\n")
	TEXT("is human skin which only scatters about 1.2cm)\n")
	TEXT(" 0: off (if there is no object on the screen using this pass it should automatically disable the post process pass)\n")
	TEXT("<1: scale scatter radius down (for testing)\n")
	TEXT(" 1: use given radius form the Subsurface scattering asset (default)\n")
	TEXT(">1: scale scatter radius up (for testing)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static bool IsAmbientCubemapPassRequired(FPostprocessContext& Context)
{
	FScene* Scene = (FScene*)Context.View.Family->Scene;

	return Context.View.FinalPostProcessSettings.ContributingCubemaps.Num() != 0 && !IsSimpleDynamicLightingEnabled();
}

static bool IsLpvIndirectPassRequired(FPostprocessContext& Context)
{
	FScene* Scene = (FScene*)Context.View.Family->Scene;

	const FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;

	if(ViewState)
	{
		FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume();

		if(LightPropagationVolume && Context.View.FinalPostProcessSettings.LPVIntensity > 0.01f)
		{
			return true; 
		}
	}

	return false;
}

static void AddPostProcessingLpvIndirect(FPostprocessContext& Context, FRenderingCompositePass* SSAO )
{
	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new FRCPassPostProcessLpvIndirect());
	Pass->SetInput(ePId_Input0, Context.FinalOutput);
	Pass->SetInput(ePId_Input1, SSAO );

	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
}

static bool IsReflectionEnvironmentActive(FPostprocessContext& Context)
{
	FScene* Scene = (FScene*)Context.View.Family->Scene;

	// LPV & Screenspace Reflections : Reflection Environment active if either LPV (assumed true if this was called), Reflection Captures or SSR active

	bool IsReflectingEnvironment = Context.View.Family->EngineShowFlags.ReflectionEnvironment;
	bool HasReflectionCaptures = (Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() > 0);
	bool HasSSR = Context.View.Family->EngineShowFlags.ScreenSpaceReflections;

	return (Scene->GetFeatureLevel() == ERHIFeatureLevel::SM5 && IsReflectingEnvironment && (HasReflectionCaptures || HasSSR) && !IsSimpleDynamicLightingEnabled());
}

static bool IsSkylightActive(FPostprocessContext& Context)
{
	FScene* Scene = (FScene*)Context.View.Family->Scene;
	return Scene->SkyLight 
		&& Scene->SkyLight->ProcessedTexture
		&& Context.View.Family->EngineShowFlags.SkyLighting;
}

static bool IsBasePassAmbientOcclusionRequired(FPostprocessContext& Context)
{
	// the BaseAO pass is only worth with some AO
	return Context.View.FinalPostProcessSettings.AmbientOcclusionStaticFraction >= 1 / 100.0f && !IsSimpleDynamicLightingEnabled();
}

// @return 0:off, 0..4
static uint32 ComputeAmbientOcclusionPassCount(FPostprocessContext& Context)
{
	uint32 Ret = 0;

	bool bEnabled = true;

	if(!IsLpvIndirectPassRequired(Context))
	{
		bEnabled = Context.View.FinalPostProcessSettings.AmbientOcclusionIntensity > 0 
			&& Context.View.FinalPostProcessSettings.AmbientOcclusionRadius >= 0.1f 
			&& (IsBasePassAmbientOcclusionRequired(Context) || IsAmbientCubemapPassRequired(Context) || IsReflectionEnvironmentActive(Context) || IsSkylightActive(Context) || Context.View.Family->EngineShowFlags.VisualizeBuffer )
			&& !IsSimpleDynamicLightingEnabled();
	}

	if(bEnabled)
	{
		static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AmbientOcclusionLevels"));
		Ret = FMath::Clamp(ICVar->GetValueOnRenderThread(), 0, 4);
	}

	return Ret;
}

static void AddPostProcessingAmbientCubemap(FPostprocessContext& Context, FRenderingCompositeOutputRef AmbientOcclusion)
{
	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbient());
	Pass->SetInput(ePId_Input0, Context.FinalOutput);
	Pass->SetInput(ePId_Input1, AmbientOcclusion);

	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
}

// @param Levels 0..4, how many different resolution levels we want to render
static FRenderingCompositeOutputRef AddPostProcessingAmbientOcclusion(FRHICommandListImmediate& RHICmdList, FPostprocessContext& Context, uint32 Levels)
{
	check(Levels >= 0 && Levels <= 4);

	FRenderingCompositePass* AmbientOcclusionInMip1 = 0;
	FRenderingCompositePass* AmbientOcclusionInMip2 = 0;
	FRenderingCompositePass* AmbientOcclusionInMip3 = 0;
	FRenderingCompositePass* AmbientOcclusionPassMip1 = 0; 
	FRenderingCompositePass* AmbientOcclusionPassMip2 = 0; 
	FRenderingCompositePass* AmbientOcclusionPassMip3 = 0; 

	// generate input in half, quarter, .. resolution

	AmbientOcclusionInMip1 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusionSetup());
	AmbientOcclusionInMip1->SetInput(ePId_Input0, Context.SceneDepth);

	if(Levels >= 3)
	{
		AmbientOcclusionInMip2 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusionSetup());
		AmbientOcclusionInMip2->SetInput(ePId_Input1, FRenderingCompositeOutputRef(AmbientOcclusionInMip1, ePId_Output0));
	}

	if(Levels >= 4)
	{
		AmbientOcclusionInMip3 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusionSetup());
		AmbientOcclusionInMip3->SetInput(ePId_Input1, FRenderingCompositeOutputRef(AmbientOcclusionInMip2, ePId_Output0));
	}

	FRenderingCompositePass* HZBInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( const_cast< FViewInfo& >( Context.View ).HZB ) );

	// upsample from lower resolution

	if(Levels >= 4)
	{
		AmbientOcclusionPassMip3 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion());
		AmbientOcclusionPassMip3->SetInput(ePId_Input0, AmbientOcclusionInMip3);
		AmbientOcclusionPassMip3->SetInput(ePId_Input1, AmbientOcclusionInMip3);
		AmbientOcclusionPassMip3->SetInput(ePId_Input3, HZBInput);
	}

	if(Levels >= 3)
	{
		AmbientOcclusionPassMip2 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion());
		AmbientOcclusionPassMip2->SetInput(ePId_Input0, AmbientOcclusionInMip2);
		AmbientOcclusionPassMip2->SetInput(ePId_Input1, AmbientOcclusionInMip2);
		AmbientOcclusionPassMip2->SetInput(ePId_Input2, AmbientOcclusionPassMip3);
		AmbientOcclusionPassMip2->SetInput(ePId_Input3, HZBInput);
	}

	if(Levels >= 2)
	{
		AmbientOcclusionPassMip1 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion());
		AmbientOcclusionPassMip1->SetInput(ePId_Input0, AmbientOcclusionInMip1);
		AmbientOcclusionPassMip1->SetInput(ePId_Input1, AmbientOcclusionInMip1);
		AmbientOcclusionPassMip1->SetInput(ePId_Input2, AmbientOcclusionPassMip2);
		AmbientOcclusionPassMip1->SetInput(ePId_Input3, HZBInput);
	}

	FRenderingCompositePass* GBufferA = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(GSceneRenderTargets.GBufferA));

	// finally full resolution

	FRenderingCompositePass* AmbientOcclusionPassMip0 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion(false));
	AmbientOcclusionPassMip0->SetInput(ePId_Input0, GBufferA);
	AmbientOcclusionPassMip0->SetInput(ePId_Input1, AmbientOcclusionInMip1);
	AmbientOcclusionPassMip0->SetInput(ePId_Input2, AmbientOcclusionPassMip1);
	AmbientOcclusionPassMip0->SetInput(ePId_Input3, HZBInput);

	// to make sure this pass is processed as well (before), needed to make process decals before computing AO
	AmbientOcclusionInMip1->AddDependency(Context.FinalOutput);

	Context.FinalOutput = FRenderingCompositeOutputRef(AmbientOcclusionPassMip0);

	GSceneRenderTargets.bScreenSpaceAOIsValid = true;

	if(IsBasePassAmbientOcclusionRequired(Context))
	{
		FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBasePassAO());
		Pass->AddDependency(Context.FinalOutput);

		Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
	}

	return FRenderingCompositeOutputRef(AmbientOcclusionPassMip0);
}

static void AddDeferredDecalsBeforeBasePass(FPostprocessContext& Context)
{
	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(0));
	Pass->SetInput(ePId_Input0, Context.FinalOutput);

	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
}

static void AddDeferredDecalsBeforeLighting(FPostprocessContext& Context)
{
	FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(1));
	Pass->SetInput(ePId_Input0, Context.FinalOutput);

	Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
}

void FCompositionLighting::ProcessBeforeBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());

	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(CompositeContext.Graph, View);

		// Add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ----------

		// decals are before AmbientOcclusion so the decal can output a normal that AO is affected by
		if (Context.View.Family->EngineShowFlags.Decals &&
			!Context.View.Family->EngineShowFlags.ShaderComplexity &&
			IsDBufferEnabled()) 
		{
			AddDeferredDecalsBeforeBasePass(Context);
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, CompositionBeforeBasePass);

		// you can add multiple dependencies
		CompositeContext.Root->AddDependency(Context.FinalOutput);

		CompositeContext.Process(TEXT("Composition_BeforeBasePass"));
	}
}

void FCompositionLighting::ProcessAfterBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	
	// might get renamed to refracted or ...WithAO
	GSceneRenderTargets.GetSceneColor()->SetDebugName(TEXT("SceneColor"));
	// to be able to observe results with VisualizeTexture

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.GetSceneColor());
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.GBufferA);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.GBufferB);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.GBufferC);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.GBufferD);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.GBufferE);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.GBufferVelocity);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.ScreenSpaceAO);
	
	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(CompositeContext.Graph, View);

		// Add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ----------

		// decals are before AmbientOcclusion so the decal can output a normal that AO is affected by
		if( Context.View.Family->EngineShowFlags.Decals &&
			!Context.View.Family->EngineShowFlags.VisualizeLightCulling)		// decal are distracting when looking at LightCulling
		{
			AddDeferredDecalsBeforeLighting(Context);
		}

		FRenderingCompositeOutputRef AmbientOcclusion;

		if(uint32 Levels = ComputeAmbientOcclusionPassCount(Context))
		{
			AmbientOcclusion = AddPostProcessingAmbientOcclusion(RHICmdList, Context, Levels);
		}

		if(IsAmbientCubemapPassRequired(Context))
		{
			AddPostProcessingAmbientCubemap(Context, AmbientOcclusion);
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, LightCompositionTasks_PreLighting);

		TRefCountPtr<IPooledRenderTarget>& SceneColor = GSceneRenderTargets.GetSceneColor();

		Context.FinalOutput.GetOutput()->RenderTargetDesc = SceneColor->GetDesc();
		Context.FinalOutput.GetOutput()->PooledRenderTarget = SceneColor;

		// you can add multiple dependencies
		CompositeContext.Root->AddDependency(Context.FinalOutput);

		CompositeContext.Process(TEXT("CompositionLighting_AfterBasePass"));
	}
}


void FCompositionLighting::ProcessLighting(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.ReflectiveShadowMapDiffuse);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.ReflectiveShadowMapNormal);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.ReflectiveShadowMapDepth);

	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(CompositeContext.Graph, View);

		FRenderingCompositeOutputRef AmbientOcclusion;

		if(IsLpvIndirectPassRequired(Context))
		{
			FRenderingCompositePass* SSAO = Context.Graph.RegisterPass(new FRCPassPostProcessInput(GSceneRenderTargets.ScreenSpaceAO));
			AddPostProcessingLpvIndirect( Context, SSAO );
		}

		// Screen Space Subsurface Scattering
		{
			float Radius = CVarSSSScale.GetValueOnRenderThread();

			bool bSimpleDynamicLighting = IsSimpleDynamicLightingEnabled();

			bool bScreenSpaceSubsurfacePassNeeded = (View.ShadingModelMaskInView & (1 << MSM_SubsurfaceProfile)) != 0;
			if (bScreenSpaceSubsurfacePassNeeded && Radius > 0 && !bSimpleDynamicLighting && View.Family->EngineShowFlags.SubsurfaceScattering &&
				//@todo-rco: Remove this when we fix the cross-compiler
				!IsOpenGLPlatform(View.GetShaderPlatform()))
			{
				// can be optimized out if we don't do split screen/stereo rendering (should be done after we some post process refactoring)
				FRenderingCompositePass* PassExtractSpecular = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceExtractSpecular());
				PassExtractSpecular->SetInput(ePId_Input0, Context.FinalOutput);

				FRenderingCompositePass* PassSetup = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceSetup(View));
				PassSetup->SetInput(ePId_Input0, Context.FinalOutput);

				FRenderingCompositePass* Pass0 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurface(0));
				Pass0->SetInput(ePId_Input0, PassSetup);

				FRenderingCompositePass* Pass1 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurface(1));
				Pass1->SetInput(ePId_Input0, Pass0);
				Pass1->SetInput(ePId_Input1, PassSetup);

				// full res composite pass, no blurring (Radius=0)
				FRenderingCompositePass* RecombinePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceRecombine());
				RecombinePass->SetInput(ePId_Input0, Pass1);
				RecombinePass->SetInput(ePId_Input1, PassExtractSpecular);
				CompositeContext.Root->AddDependency(RecombinePass);
			}
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, CompositionLighting);

		// we don't replace the final element with the scenecolor because this is what those passes should do by themself

		// you can add multiple dependencies
		CompositeContext.Root->AddDependency(Context.FinalOutput);

		CompositeContext.Process(TEXT("CompositionLighting_Lighting"));
	}

	// We only release the after the last view was processed (SplitScreen)
	if(View.Family->Views[View.Family->Views.Num() - 1] == &View)
	{
		// The RT should be released as early as possible to allow sharing of that memory for other purposes.
		// This becomes even more important with some limited VRam (XBoxOne).
		GSceneRenderTargets.SetLightAttenuation(0);
	}
}
