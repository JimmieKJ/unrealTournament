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
#include "LightPropagationVolumeBlendable.h"

/** The global center for all deferred lighting activities. */
FCompositionLighting GCompositionLighting;

// -------------------------------------------------------

static TAutoConsoleVariable<float> CVarSSSScale(
	TEXT("r.SSS.Scale"),
	1.0f,
	TEXT("Affects the Screen space subsurface scattering pass")
	TEXT("(use shadingmodel SubsurfaceProfile, get near to the object as the default)\n")
	TEXT("is human skin which only scatters about 1.2cm)\n")
	TEXT(" 0: off (if there is no object on the screen using this pass it should automatically disable the post process pass)\n")
	TEXT("<1: scale scatter radius down (for testing)\n")
	TEXT(" 1: use given radius form the Subsurface scattering asset (default)\n")
	TEXT(">1: scale scatter radius up (for testing)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarAmbientOcclusionLevels(
	TEXT("r.AmbientOcclusionLevels"),
	-1,
	TEXT("Defines how many mip levels are using during the ambient occlusion calculation. This is useful when tweaking the algorithm.\n")
	TEXT("<0: decide based on the quality setting in the postprocess settings/volume and r.AmbientOcclusionMaxQuality (default)\n")
	TEXT(" 0: none (disable AmbientOcclusion)\n")
	TEXT(" 1: one\n")
	TEXT(" 2: two (costs extra performance, soft addition)\n")
	TEXT(" 3: three (larger radius cost less but can flicker)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSSSHalfRes(
	TEXT("r.SSS.HalfRes"),
	1,
	TEXT(" 0: full quality (not optimized, as reference)\n")
	TEXT(" 1: parts of the algorithm runs in half resolution which is lower quality but faster (default)"),
	ECVF_RenderThreadSafe  | ECVF_Scalability);

static bool IsAmbientCubemapPassRequired(FPostprocessContext& Context)
{
	FScene* Scene = (FScene*)Context.View.Family->Scene;

	return Context.View.FinalPostProcessSettings.ContributingCubemaps.Num() != 0 && !IsSimpleDynamicLightingEnabled();
}

bool IsLpvIndirectPassRequired(const FViewInfo& View)
{
	FScene* Scene = (FScene*)View.Family->Scene;

	const FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if(ViewState)
	{
		// This check should be inclusive to stereo views
		const bool bIncludeStereoViews = true;

		FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel(), bIncludeStereoViews);

		if(LightPropagationVolume)
		{
			const FLightPropagationVolumeSettings& LPVSettings = View.FinalPostProcessSettings.BlendableManager.GetSingleFinalDataConst<FLightPropagationVolumeSettings>();

			if(LPVSettings.LPVIntensity > 0.0f)
			{
				return true;
			}
		}
	}

	return false;
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

// @return 0:off, 0..3
static uint32 ComputeAmbientOcclusionPassCount(FPostprocessContext& Context)
{
	// 0:off / 1 / 2 / 3
	uint32 Ret = 0;

	bool bEnabled = true;

	if(!IsLpvIndirectPassRequired(Context.View))
	{
		bEnabled = Context.View.FinalPostProcessSettings.AmbientOcclusionIntensity > 0 
			&& Context.View.FinalPostProcessSettings.AmbientOcclusionRadius >= 0.1f 
			&& !Context.View.Family->EngineShowFlags.ShaderComplexity 
			&& (IsBasePassAmbientOcclusionRequired(Context) || IsAmbientCubemapPassRequired(Context) || IsReflectionEnvironmentActive(Context) || IsSkylightActive(Context) || Context.View.Family->EngineShowFlags.VisualizeBuffer )
			&& !IsSimpleDynamicLightingEnabled();
	}

	if(bEnabled)
	{
		// usually in the range 0..100
		float QualityPercent = GetAmbientOcclusionQualityRT(Context.View);

		// don't expose 0 as the lowest quality should still render
		Ret = 1 +
			(QualityPercent > 70.0f) +
			(QualityPercent > 35.0f);

		int32 CVarLevel = CVarAmbientOcclusionLevels.GetValueOnRenderThread();

		if(CVarLevel >= 0)
		{
			// cvar can override (for scalability or to profile/test)
			Ret = CVarLevel;
		}

		// bring into valid range
		Ret = FMath::Min<uint32>(Ret, 3);
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

// @param Levels 0..3, how many different resolution levels we want to render
static FRenderingCompositeOutputRef AddPostProcessingAmbientOcclusion(FRHICommandListImmediate& RHICmdList, FPostprocessContext& Context, uint32 Levels)
{
	check(Levels >= 0 && Levels <= 3);

	FRenderingCompositePass* AmbientOcclusionInMip1 = 0;
	FRenderingCompositePass* AmbientOcclusionInMip2 = 0;
	FRenderingCompositePass* AmbientOcclusionPassMip1 = 0; 
	FRenderingCompositePass* AmbientOcclusionPassMip2 = 0; 

	// generate input in half, quarter, .. resolution

	if(Levels >= 2)
	{
		AmbientOcclusionInMip1 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusionSetup());
		AmbientOcclusionInMip1->SetInput(ePId_Input0, Context.SceneDepth);
	}

	if(Levels >= 3)
	{
		AmbientOcclusionInMip2 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusionSetup());
		AmbientOcclusionInMip2->SetInput(ePId_Input1, FRenderingCompositeOutputRef(AmbientOcclusionInMip1, ePId_Output0));
	}

	FRenderingCompositePass* HZBInput = Context.Graph.RegisterPass( new FRCPassPostProcessInput( const_cast< FViewInfo& >( Context.View ).HZB ) );

	// upsample from lower resolution

	if(Levels >= 3)
	{
		AmbientOcclusionPassMip2 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion(Context.View));
		AmbientOcclusionPassMip2->SetInput(ePId_Input0, AmbientOcclusionInMip2);
		AmbientOcclusionPassMip2->SetInput(ePId_Input1, AmbientOcclusionInMip2);
		AmbientOcclusionPassMip2->SetInput(ePId_Input3, HZBInput);
	}

	if(Levels >= 2)
	{
		AmbientOcclusionPassMip1 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion(Context.View));
		AmbientOcclusionPassMip1->SetInput(ePId_Input0, AmbientOcclusionInMip1);
		AmbientOcclusionPassMip1->SetInput(ePId_Input1, AmbientOcclusionInMip1);
		AmbientOcclusionPassMip1->SetInput(ePId_Input2, AmbientOcclusionPassMip2);
		AmbientOcclusionPassMip1->SetInput(ePId_Input3, HZBInput);
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	FRenderingCompositePass* GBufferA = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessInput(SceneContext.GBufferA));

	// finally full resolution

	FRenderingCompositePass* AmbientOcclusionPassMip0 = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessAmbientOcclusion(Context.View, false));
	AmbientOcclusionPassMip0->SetInput(ePId_Input0, GBufferA);
	AmbientOcclusionPassMip0->SetInput(ePId_Input1, AmbientOcclusionInMip1);
	AmbientOcclusionPassMip0->SetInput(ePId_Input2, AmbientOcclusionPassMip1);
	AmbientOcclusionPassMip0->SetInput(ePId_Input3, HZBInput);

	// to make sure this pass is processed as well (before), needed to make process decals before computing AO
	if(AmbientOcclusionInMip1)
	{
		AmbientOcclusionInMip1->AddDependency(Context.FinalOutput);
	}
	else
	{
		AmbientOcclusionPassMip0->AddDependency(Context.FinalOutput);
	}

	Context.FinalOutput = FRenderingCompositeOutputRef(AmbientOcclusionPassMip0);

	SceneContext.bScreenSpaceAOIsValid = true;

	if(IsBasePassAmbientOcclusionRequired(Context))
	{
		FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessBasePassAO());
		Pass->AddDependency(Context.FinalOutput);

		Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
	}

	return FRenderingCompositeOutputRef(AmbientOcclusionPassMip0);
}

void FCompositionLighting::ProcessBeforeBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());

	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

		// Add the passes we want to add to the graph (commenting a line means the pass is not inserted into the graph) ----------

		// decals are before AmbientOcclusion so the decal can output a normal that AO is affected by
		if (!Context.View.Family->EngineShowFlags.ShaderComplexity &&
			IsDBufferEnabled()) 
		{
			FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(DRS_BeforeBasePass));
			Pass->SetInput(ePId_Input0, Context.FinalOutput);

			Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, CompositionBeforeBasePass);

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("Composition_BeforeBasePass"));
	}
}

void FCompositionLighting::ProcessAfterBasePass(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	// might get renamed to refracted or ...WithAO
	SceneContext.GetSceneColor()->SetDebugName(TEXT("SceneColor"));
	// to be able to observe results with VisualizeTexture

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GetSceneColor());
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferA);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferB);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferC);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferD);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferE);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GBufferVelocity);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.ScreenSpaceAO);
	
	// so that the passes can register themselves to the graph
	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);

		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

		// Add the passes we want to add to the graph ----------
		
		if(Context.View.Family->EngineShowFlags.Decals && !Context.View.Family->EngineShowFlags.ShaderComplexity)
		{
			// DRS_AfterBasePass is for Volumetric decals which don't support ShaderComplexity yet
			FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(DRS_AfterBasePass));
			Pass->SetInput(ePId_Input0, Context.FinalOutput);

			Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
		}

		// decals are before AmbientOcclusion so the decal can output a normal that AO is affected by
		if( Context.View.Family->EngineShowFlags.Decals &&
			!Context.View.Family->EngineShowFlags.VisualizeLightCulling)		// decal are distracting when looking at LightCulling
		{
			FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessDeferredDecals(DRS_BeforeLighting));
			Pass->SetInput(ePId_Input0, Context.FinalOutput);

			Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
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

		TRefCountPtr<IPooledRenderTarget>& SceneColor = SceneContext.GetSceneColor();

		Context.FinalOutput.GetOutput()->RenderTargetDesc = SceneColor->GetDesc();
		Context.FinalOutput.GetOutput()->PooledRenderTarget = SceneColor;

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("CompositionLighting_AfterBasePass"));
	}
}


void FCompositionLighting::ProcessLpvIndirect(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	
	FMemMark Mark(FMemStack::Get());
	FRenderingCompositePassContext CompositeContext(RHICmdList, View);
	FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);

	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		FRenderingCompositePass* SSAO = Context.Graph.RegisterPass(new FRCPassPostProcessInput(SceneContext.ScreenSpaceAO));

		FRenderingCompositePass* Pass = Context.Graph.RegisterPass(new FRCPassPostProcessLpvIndirect());
		Pass->SetInput(ePId_Input0, Context.FinalOutput);
		Pass->SetInput(ePId_Input1, SSAO );

		Context.FinalOutput = FRenderingCompositeOutputRef(Pass);
	}

	// The graph setup should be finished before this line ----------------------------------------

	SCOPED_DRAW_EVENT(RHICmdList, CompositionLpvIndirect);

	// we don't replace the final element with the scenecolor because this is what those passes should do by themself

	CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("CompositionLighting"));
}

void FCompositionLighting::ProcessAfterLighting(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	check(IsInRenderingThread());
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.ReflectiveShadowMapDiffuse);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.ReflectiveShadowMapNormal);
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.ReflectiveShadowMapDepth);

	{
		FMemMark Mark(FMemStack::Get());
		FRenderingCompositePassContext CompositeContext(RHICmdList, View);
		FPostprocessContext Context(RHICmdList, CompositeContext.Graph, View);
		FRenderingCompositeOutputRef AmbientOcclusion;

		// Screen Space Subsurface Scattering
		{
			float Radius = CVarSSSScale.GetValueOnRenderThread();

			bool bSimpleDynamicLighting = IsSimpleDynamicLightingEnabled();

			bool bScreenSpaceSubsurfacePassNeeded = (View.ShadingModelMaskInView & (1 << MSM_SubsurfaceProfile)) != 0;
			if (bScreenSpaceSubsurfacePassNeeded && !bSimpleDynamicLighting)
			{
				bool bHalfRes = CVarSSSHalfRes.GetValueOnRenderThread() != 0;
				bool bSingleViewportMode = View.Family->Views.Num() == 1;

				if(Radius > 0 && View.Family->EngineShowFlags.SubsurfaceScattering)
				{
					FRenderingCompositePass* PassSetup = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceSetup(View, bHalfRes));
					PassSetup->SetInput(ePId_Input0, Context.FinalOutput);

					FRenderingCompositePass* PassX = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurface(0, bHalfRes));
					PassX->SetInput(ePId_Input0, PassSetup);

					FRenderingCompositePass* PassY = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurface(1, bHalfRes));
					PassY->SetInput(ePId_Input0, PassX);
					PassY->SetInput(ePId_Input1, PassSetup);

					// full res composite pass, no blurring (Radius=0), replaces SceneColor
					FRenderingCompositePass* RecombinePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceRecombine(bHalfRes, bSingleViewportMode));
					RecombinePass->SetInput(ePId_Input0, Context.FinalOutput);
					RecombinePass->SetInput(ePId_Input1, PassY);
					RecombinePass->SetInput(ePId_Input2, PassSetup);
					Context.FinalOutput = FRenderingCompositeOutputRef(RecombinePass);
				}
				else
				{
					// needed for Scalability
					FRenderingCompositePass* RecombinePass = Context.Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessSubsurfaceRecombine(bHalfRes, bSingleViewportMode));
					RecombinePass->SetInput(ePId_Input0, Context.FinalOutput);
					Context.FinalOutput = FRenderingCompositeOutputRef(RecombinePass);
				}
			}
		}

		// The graph setup should be finished before this line ----------------------------------------

		SCOPED_DRAW_EVENT(RHICmdList, CompositionAfterLighting);

		// we don't replace the final element with the scenecolor because this is what those passes should do by themself

		CompositeContext.Process(Context.FinalOutput.GetPass(), TEXT("CompositionLighting"));
	}

	// We only release the after the last view was processed (SplitScreen)
	if(View.Family->Views[View.Family->Views.Num() - 1] == &View)
	{
		// The RT should be released as early as possible to allow sharing of that memory for other purposes.
		// This becomes even more important with some limited VRam (XBoxOne).
		SceneContext.SetLightAttenuation(0);
	}
}
