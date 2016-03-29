// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.cpp: Top level rendering loop for deferred shading
=============================================================================*/

#include "RendererPrivate.h"
#include "Engine.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"
#include "ScreenSpaceReflections.h"
#include "VisualizeTexture.h"
#include "CompositionLighting.h"
#include "FXSystem.h"
#include "OneColorShader.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "LightPropagationVolume.h"
#include "DeferredShadingRenderer.h"
#include "SceneUtils.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "GlobalDistanceField.h"
#include "PostProcess/PostProcessing.h"
#include "DistanceFieldAtlas.h"
#include "EngineModule.h"
#include "IHeadMountedDisplay.h"
#include "GPUSkinCache.h"

TAutoConsoleVariable<int32> CVarEarlyZPass(
	TEXT("r.EarlyZPass"),
	3,	
	TEXT("Whether to use a depth only pass to initialize Z culling for the base pass. Cannot be changed at runtime.\n")
	TEXT("Note: also look at r.EarlyZPassMovable\n")
	TEXT("  0: off\n")
	TEXT("  1: only if not masked, and only if large on the screen\n")
	TEXT("  2: all opaque (including masked)\n")
	TEXT("  x: use built in heuristic (default is 3)"),
	ECVF_ReadOnly);

int32 GEarlyZPassMovable = 0;

/** Affects static draw lists so must reload level to propagate. */
static FAutoConsoleVariableRef CVarEarlyZPassMovable(
	TEXT("r.EarlyZPassMovable"),
	GEarlyZPassMovable,
	TEXT("Whether to render movable objects into the depth only pass.  Movable objects are typically not good occluders so this defaults to off.\n")
	TEXT("Note: also look at r.EarlyZPass"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

TAutoConsoleVariable<int32> CVarCustomDepthOrder(
	TEXT("r.CustomDepth.Order"),
	1,	
	TEXT("When CustomDepth (and CustomStencil) is getting rendered\n")
	TEXT("  0: Before GBuffer (can be more efficient with AsyncCompute, allows using it in DBuffer pass, no GBuffer blending decals allow GBuffer compression)\n")
	TEXT("  1: After Base Pass (default)"),
	ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarVisualizeTexturePool(
	TEXT("r.VisualizeTexturePool"),
	0,
	TEXT("Allows to enable the visualize the texture pool (currently only on console).\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif

static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksBasePass(
	TEXT("r.RHICmdFlushRenderThreadTasksBasePass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the base pass. A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksBasePass is > 0 we will flush."));

static TAutoConsoleVariable<int32> CVarFXSystemPreRenderAfterPrepass(
	TEXT("r.FXSystemPreRenderAfterPrepass"),
	0,
	TEXT("If > 0, then do the FX prerender after the prepass. This improves pipelining for greater performance. Experiemental option."),
	ECVF_RenderThreadSafe
	);

DECLARE_CYCLE_STAT(TEXT("PostInitViews FlushDel"), STAT_PostInitViews_FlushDel, STATGROUP_InitViews);
DECLARE_CYCLE_STAT(TEXT("InitViews Intentional Stall"), STAT_InitViews_Intentional_Stall, STATGROUP_InitViews);

DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer AsyncSortBasePassStaticData"), STAT_FDeferredShadingSceneRenderer_AsyncSortBasePassStaticData, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer UpdateDownsampledDepthSurface"), STAT_FDeferredShadingSceneRenderer_UpdateDownsampledDepthSurface, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Render Init"), STAT_FDeferredShadingSceneRenderer_Render_Init, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Render ServiceLocalQueue"), STAT_FDeferredShadingSceneRenderer_Render_ServiceLocalQueue, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer DistanceFieldAO Init"), STAT_FDeferredShadingSceneRenderer_DistanceFieldAO_Init, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FGlobalDynamicVertexBuffer Commit"), STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer MotionBlurStartFrame"), STAT_FDeferredShadingSceneRenderer_MotionBlurStartFrame, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FXSystem PreRender"), STAT_FDeferredShadingSceneRenderer_FXSystem_PreRender, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer AllocGBufferTargets"), STAT_FDeferredShadingSceneRenderer_AllocGBufferTargets, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ClearLPVs"), STAT_FDeferredShadingSceneRenderer_ClearLPVs, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer DBuffer"), STAT_FDeferredShadingSceneRenderer_DBuffer, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer SetAndClearViewGBuffer"), STAT_FDeferredShadingSceneRenderer_SetAndClearViewGBuffer, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ClearGBufferAtMaxZ"), STAT_FDeferredShadingSceneRenderer_ClearGBufferAtMaxZ, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ResolveDepth After Basepass"), STAT_FDeferredShadingSceneRenderer_ResolveDepth_After_Basepass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Resolve After Basepass"), STAT_FDeferredShadingSceneRenderer_Resolve_After_Basepass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FXSystem PostRenderOpaque"), STAT_FDeferredShadingSceneRenderer_FXSystem_PostRenderOpaque, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer AfterBasePass"), STAT_FDeferredShadingSceneRenderer_AfterBasePass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Lighting"), STAT_FDeferredShadingSceneRenderer_Lighting, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderLightShaftOcclusion"), STAT_FDeferredShadingSceneRenderer_RenderLightShaftOcclusion, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderAtmosphere"), STAT_FDeferredShadingSceneRenderer_RenderAtmosphere, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderFog"), STAT_FDeferredShadingSceneRenderer_RenderFog, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderLightShaftBloom"), STAT_FDeferredShadingSceneRenderer_RenderLightShaftBloom, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer UpdateMotionBlurCache"), STAT_FDeferredShadingSceneRenderer_UpdateMotionBlurCache, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderFinish"), STAT_FDeferredShadingSceneRenderer_RenderFinish, STATGROUP_SceneRendering);

DECLARE_CYCLE_STAT(TEXT("OcclusionSubmittedFence Dispatch"), STAT_OcclusionSubmittedFence_Dispatch, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("OcclusionSubmittedFence Wait"), STAT_OcclusionSubmittedFence_Wait, STATGROUP_SceneRendering);


/*-----------------------------------------------------------------------------
	FDeferredShadingSceneRenderer
-----------------------------------------------------------------------------*/

FDeferredShadingSceneRenderer::FDeferredShadingSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer)
	: FSceneRenderer(InViewFamily, HitProxyConsumer)
	, EarlyZPassMode(DDM_NonMaskedOnly)
	, TranslucentSelfShadowLayout(0, 0, 0, 0)
	, CachedTranslucentSelfShadowLightId(INDEX_NONE)
{
	if (FPlatformProperties::SupportsWindowedMode())
	{
		// Use a depth only pass if we are using full blown HQ lightmaps
		// Otherwise base pass pixel shaders will be cheap and there will be little benefit to rendering a depth only pass
		if (AllowHighQualityLightmaps(FeatureLevel) || !ViewFamily.EngineShowFlags.Lighting)
		{
			EarlyZPassMode = DDM_None;
		}
	}

	// developer override, good for profiling, can be useful as project setting
	{
		static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.EarlyZPass"));
		const int32 CVarValue = ICVar->GetValueOnGameThread();

		switch(CVarValue)
		{
			case 0: EarlyZPassMode = DDM_None; break;
			case 1: EarlyZPassMode = DDM_NonMaskedOnly; break;
			case 2: EarlyZPassMode = DDM_AllOccluders; break;
			case 3: break;	// Note: 3 indicates "default behavior" and does not specify an override
		}
	}

	// Shader complexity requires depth only pass to display masked material cost correctly
	if (ViewFamily.EngineShowFlags.ShaderComplexity)
	{
		EarlyZPassMode = DDM_AllOpaque;
	}

	// When possible use a stencil optimization for dithering in pre-pass
	bDitheredLODTransitionsUseStencil = (EarlyZPassMode == DDM_AllOccluders);
}

extern FGlobalBoundShaderState GClearMRTBoundShaderState[8];

/** 
* Clears view where Z is still at the maximum value (ie no geometry rendered)
*/
void FDeferredShadingSceneRenderer::ClearGBufferAtMaxZ(FRHICommandList& RHICmdList)
{
	// Assumes BeginRenderingSceneColor() has been called before this function
	SCOPED_DRAW_EVENT(RHICmdList, ClearGBufferAtMaxZ);

	// Clear the G Buffer render targets
	const bool bClearBlack = Views[0].Family->EngineShowFlags.ShaderComplexity || Views[0].Family->EngineShowFlags.StationaryLightOverlap;
	// Same clear color from RHIClearMRT
	FLinearColor ClearColors[MaxSimultaneousRenderTargets] = 
		{bClearBlack ? FLinearColor(0,0,0,0) : Views[0].BackgroundColor, FLinearColor(0.5f,0.5f,0.5f,0), FLinearColor(0,0,0,1), FLinearColor(0,0,0,0), FLinearColor(0,1,1,1), FLinearColor(1,1,1,1), FLinearColor::Transparent, FLinearColor::Transparent};

	uint32 NumActiveRenderTargets = FSceneRenderTargets::Get(RHICmdList).GetNumGBufferTargets();
	
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

	TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
	FOneColorPS* PixelShader = NULL; 

	// Assume for now all code path supports SM4, otherwise render target numbers are changed
	switch(NumActiveRenderTargets)
	{
	case 5:
		{
			TShaderMapRef<TOneColorPixelShaderMRT<5> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		break;
	case 6:
		{
			TShaderMapRef<TOneColorPixelShaderMRT<6> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		break;
	default:
	case 1:
		{
			TShaderMapRef<TOneColorPixelShaderMRT<1> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		break;
	}

	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, GClearMRTBoundShaderState[NumActiveRenderTargets - 1], GetVertexDeclarationFVector4(), *VertexShader, PixelShader);

	// Opaque rendering, depth test but no depth writes
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendStateWriteMask<>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

	// Clear each viewport by drawing background color at MaxZ depth
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("ClearView%d"), ViewIndex);

		FViewInfo& View = Views[ViewIndex];

		// Set viewport for this view
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

		// Setup PS
		PixelShader->SetColors(RHICmdList, ClearColors, NumActiveRenderTargets);

		// Render quad
		static const FVector4 ClearQuadVertices[4] = 
		{
			FVector4( -1.0f,	  1.0f, (float)ERHIZBuffer::FarPlane, 1.0f),
			FVector4(  1.0f,  1.0f, (float)ERHIZBuffer::FarPlane, 1.0f ),
			FVector4( -1.0f, -1.0f, (float)ERHIZBuffer::FarPlane, 1.0f ),
			FVector4(  1.0f, -1.0f, (float)ERHIZBuffer::FarPlane, 1.0f )
		};
		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, ClearQuadVertices, sizeof(ClearQuadVertices[0]));
	}
}

bool FDeferredShadingSceneRenderer::RenderBasePassStaticDataMasked(FRHICommandList& RHICmdList, FViewInfo& View)
{
	bool bDirty = false;
	const FScene::EBasePassDrawListType MaskedDrawType = FScene::EBasePass_Masked;
	if (!View.IsInstancedStereoPass())
	{
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticMasked);
			bDirty |= Scene->BasePassUniformLightMapPolicyDrawList[MaskedDrawType].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
		}
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshVisibilityMap, Views[1].StaticMeshVisibilityMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticMasked);
			bDirty |= Scene->BasePassUniformLightMapPolicyDrawList[MaskedDrawType].DrawVisibleInstancedStereo(RHICmdList, StereoView);
		}
	}

	return bDirty;
}

void FDeferredShadingSceneRenderer::RenderBasePassStaticDataMaskedParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	const FScene::EBasePassDrawListType MaskedDrawType = FScene::EBasePass_Masked;
	if (!ParallelCommandListSet.View.IsInstancedStereoPass())
	{
		Scene->BasePassUniformLightMapPolicyDrawList[MaskedDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshVisibilityMap, Views[1].StaticMeshVisibilityMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
		Scene->BasePassUniformLightMapPolicyDrawList[MaskedDrawType].DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);
	}
}

bool FDeferredShadingSceneRenderer::RenderBasePassStaticDataDefault(FRHICommandList& RHICmdList, FViewInfo& View)
{
	bool bDirty = false;
	const FScene::EBasePassDrawListType OpaqueDrawType = FScene::EBasePass_Default;
	if (!View.IsInstancedStereoPass())
	{
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticOpaque);
			bDirty |= Scene->BasePassUniformLightMapPolicyDrawList[OpaqueDrawType].DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
		}
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshVisibilityMap, Views[1].StaticMeshVisibilityMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticOpaque);
			bDirty |= Scene->BasePassUniformLightMapPolicyDrawList[OpaqueDrawType].DrawVisibleInstancedStereo(RHICmdList, StereoView);
		}
	}

	return bDirty;
}

void FDeferredShadingSceneRenderer::RenderBasePassStaticDataDefaultParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	const FScene::EBasePassDrawListType OpaqueDrawType = FScene::EBasePass_Default;
	if (!ParallelCommandListSet.View.IsInstancedStereoPass())
	{
		Scene->BasePassUniformLightMapPolicyDrawList[OpaqueDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshVisibilityMap, Views[1].StaticMeshVisibilityMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
		Scene->BasePassUniformLightMapPolicyDrawList[OpaqueDrawType].DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);
	}
}

FAutoConsoleTaskPriority CPrio_FSortFrontToBackTask(
	TEXT("TaskGraph.TaskPriorities.SortFrontToBackTask"),
	TEXT("Task and thread priority for FSortFrontToBackTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

template<typename StaticMeshDrawList>
class FSortFrontToBackTask
{
private:
	StaticMeshDrawList * const StaticMeshDrawListToSort;
	const FVector ViewPosition;

public:
	FSortFrontToBackTask(StaticMeshDrawList * const InStaticMeshDrawListToSort, const FVector InViewPosition)
		: StaticMeshDrawListToSort(InStaticMeshDrawListToSort)
		, ViewPosition(InViewPosition)
	{

	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSortFrontToBackTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_FSortFrontToBackTask.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		StaticMeshDrawListToSort->SortFrontToBack(ViewPosition);
	}
};

void FDeferredShadingSceneRenderer::AsyncSortBasePassStaticData(const FVector InViewPosition, FGraphEventArray &OutSortEvents)
{
	// If we're not using a depth only pass, sort the static draw list buckets roughly front to back, to maximize HiZ culling
	// Note that this is only a very rough sort, since it does not interfere with state sorting, and each list is sorted separately
	if (EarlyZPassMode != DDM_None)
		return;
	SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AsyncSortBasePassStaticData);

	for (int32 DrawType = 0; DrawType < FScene::EBasePass_MAX; ++DrawType)
	{
			OutSortEvents.Add(TGraphTask<FSortFrontToBackTask<TStaticMeshDrawList<TBasePassDrawingPolicy<FUniformLightMapPolicy> > > >::CreateTask(
				nullptr, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(&(Scene->BasePassUniformLightMapPolicyDrawList[DrawType]), InViewPosition));
	}
}

void FDeferredShadingSceneRenderer::SortBasePassStaticData(FVector ViewPosition)
{
	// If we're not using a depth only pass, sort the static draw list buckets roughly front to back, to maximize HiZ culling
	// Note that this is only a very rough sort, since it does not interfere with state sorting, and each list is sorted separately
	if (EarlyZPassMode == DDM_None)
	{
		SCOPE_CYCLE_COUNTER(STAT_SortStaticDrawLists);

		for (int32 DrawType = 0; DrawType < FScene::EBasePass_MAX; DrawType++)
		{
				Scene->BasePassUniformLightMapPolicyDrawList[DrawType].SortFrontToBack(ViewPosition);
		}
	}
}

/**
* Renders the basepass for the static data of a given View.
*
* @return true if anything was rendered to scene color
*/
bool FDeferredShadingSceneRenderer::RenderBasePassStaticData(FRHICommandList& RHICmdList, FViewInfo& View)
{
	bool bDirty = false;

	SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);

	// When using a depth-only pass, the default opaque geometry's depths are already
	// in the depth buffer at this point, so rendering masked next will already cull
	// as efficiently as it can, while also increasing the ZCull efficiency when
	// rendering the default opaque geometry afterward.
	if (EarlyZPassMode != DDM_None)
	{
		bDirty |= RenderBasePassStaticDataMasked(RHICmdList, View);
		bDirty |= RenderBasePassStaticDataDefault(RHICmdList, View);
	}
	else
	{
		// Otherwise, in the case where we're not using a depth-only pre-pass, there
		// is an advantage to rendering default opaque first to help cull the more
		// expensive masked geometry.
		bDirty |= RenderBasePassStaticDataDefault(RHICmdList, View);
		bDirty |= RenderBasePassStaticDataMasked(RHICmdList, View);
	}
	return bDirty;
}

void FDeferredShadingSceneRenderer::RenderBasePassStaticDataParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	SCOPE_CYCLE_COUNTER(STAT_StaticDrawListDrawTime);

	// When using a depth-only pass, the default opaque geometry's depths are already
	// in the depth buffer at this point, so rendering masked next will already cull
	// as efficiently as it can, while also increasing the ZCull efficiency when
	// rendering the default opaque geometry afterward.
	if (EarlyZPassMode != DDM_None)
	{
		RenderBasePassStaticDataMaskedParallel(ParallelCommandListSet);
		RenderBasePassStaticDataDefaultParallel(ParallelCommandListSet);
	}
	else
	{
		// Otherwise, in the case where we're not using a depth-only pre-pass, there
		// is an advantage to rendering default opaque first to help cull the more
		// expensive masked geometry.
		RenderBasePassStaticDataDefaultParallel(ParallelCommandListSet);
		RenderBasePassStaticDataMaskedParallel(ParallelCommandListSet);
	}
}

/**
* Renders the basepass for the dynamic data of a given DPG and View.
*
* @return true if anything was rendered to scene color
*/

void FDeferredShadingSceneRenderer::RenderBasePassDynamicData(FRHICommandList& RHICmdList, const FViewInfo& View, bool& bOutDirty)
{
	bool bDirty = false;

	SCOPE_CYCLE_COUNTER(STAT_DynamicPrimitiveDrawTime);
	SCOPED_DRAW_EVENT(RHICmdList, Dynamic);

	FBasePassOpaqueDrawingPolicyFactory::ContextType Context(false, ESceneRenderTargetsMode::DontSet);

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

		if ((MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial || ViewFamily.EngineShowFlags.Wireframe)
			&& MeshBatchAndRelevance.bRenderInMainPass)
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, View.IsInstancedStereoPass());
		}
	}

	if (bDirty)
	{
		bOutDirty = true;
	}
}

class FRenderBasePassDynamicDataThreadTask : public FRenderTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;

public:

	FRenderBasePassDynamicDataThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderBasePassDynamicDataThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		bool OutDirty = false;
		ThisRenderer.RenderBasePassDynamicData(RHICmdList, View, OutDirty);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

void FDeferredShadingSceneRenderer::RenderBasePassDynamicDataParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();
	FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderBasePassDynamicDataThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
		.ConstructAndDispatchWhenReady(*this, *CmdList, ParallelCommandListSet.View);

	ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
}

static void SetupBasePassView(FRHICommandList& RHICmdList, const FViewInfo& View, const bool bShaderComplexity, const bool bIsEditorPrimitivePass = false)
{
	if (bShaderComplexity)
	{
		// Additive blending when shader complexity viewmode is enabled.
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA,BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
		// Disable depth writes as we have a full depth prepass.
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());
	}
	else
	{
		// Opaque blending for all G buffer targets, depth tests and writes.
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BasePassOutputsVelocityDebug"));
		if (CVar && CVar->GetValueOnRenderThread() == 2)
		{
			RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA, CW_NONE>::GetRHI());
		}
		else
		{
			RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA, CW_RGBA, CW_RGBA>::GetRHI());
		}

		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true,CF_DepthNearOrEqual>::GetRHI());
	}
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	if (!View.IsInstancedStereoPass() || bIsEditorPrimitivePass)
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
	}
	else
	{
		// When rendering with instanced stereo, render the full frame.
		RHICmdList.SetViewport(0, 0, 0, View.Family->FamilySizeX, View.ViewRect.Max.Y, 1);
	}

	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
}

DECLARE_CYCLE_STAT(TEXT("Basepass"), STAT_CLP_Basepass, STATGROUP_ParallelCommandListMarkers);

class FBasePassParallelCommandListSet : public FParallelCommandListSet
{
public:
	const FSceneViewFamily& ViewFamily;

	FBasePassParallelCommandListSet(const FViewInfo& InView, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext, const FSceneViewFamily& InViewFamily)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Basepass), InView, InParentCmdList, bInParallelExecute, bInCreateSceneContext)
		, ViewFamily(InViewFamily)
	{
		SetStateOnCommandList(ParentCmdList);
	}

	virtual ~FBasePassParallelCommandListSet()
	{
		Dispatch();
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FSceneRenderTargets::Get(CmdList).BeginRenderingGBuffer(CmdList, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, ViewFamily.EngineShowFlags.ShaderComplexity);
		SetupBasePassView(CmdList, View, !!ViewFamily.EngineShowFlags.ShaderComplexity);
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdBasePassDeferredContexts(
	TEXT("r.RHICmdBasePassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize base pass command list execution."));

void FDeferredShadingSceneRenderer::RenderBasePassViewParallel(FViewInfo& View, FRHICommandListImmediate& ParentCmdList)
{
	FBasePassParallelCommandListSet ParallelSet(View, ParentCmdList, 
		CVarRHICmdBasePassDeferredContexts.GetValueOnRenderThread() > 0, 
		CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0,
		ViewFamily);

	RenderBasePassStaticDataParallel(ParallelSet);
	RenderBasePassDynamicDataParallel(ParallelSet);
}

void FDeferredShadingSceneRenderer::RenderEditorPrimitives(FRHICommandList& RHICmdList, const FViewInfo& View, bool& bOutDirty) {
	SetupBasePassView(RHICmdList, View, ViewFamily.EngineShowFlags.ShaderComplexity, true);

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, View, FTexture2DRHIRef(), EBlendModeFilter::OpaqueAndMasked);

	bool bDirty = false;
	if (!View.Family->EngineShowFlags.CompositeEditorPrimitives)
	{
		const auto ShaderPlatform = View.GetShaderPlatform();
		const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(ShaderPlatform);

		// Draw the base pass for the view's batched mesh elements.
		bDirty |= DrawViewElements<FBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, FBasePassOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), SDPG_World, true) || bDirty;

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty |= View.BatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false) || bDirty;

		// Draw foreground objects last
		bDirty |= DrawViewElements<FBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, FBasePassOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), SDPG_Foreground, true) || bDirty;

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty |= View.TopBatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false) || bDirty;

	}

	if (bDirty)
	{
		bOutDirty = true;
	}
}

bool FDeferredShadingSceneRenderer::RenderBasePassView(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	bool bDirty = false; 
	SetupBasePassView(RHICmdList, View, ViewFamily.EngineShowFlags.ShaderComplexity);
	bDirty |= RenderBasePassStaticData(RHICmdList, View);
	RenderBasePassDynamicData(RHICmdList, View, bDirty);

	return bDirty;
}

/** Render the TexturePool texture */
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void FDeferredShadingSceneRenderer::RenderVisualizeTexturePool(FRHICommandListImmediate& RHICmdList)
{
	TRefCountPtr<IPooledRenderTarget> VisualizeTexturePool;

	/** Resolution for the texture pool visualizer texture. */
	enum
	{
		TexturePoolVisualizerSizeX = 280,
		TexturePoolVisualizerSizeY = 140,
	};

	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(TexturePoolVisualizerSizeX, TexturePoolVisualizerSizeY), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_None, false));
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VisualizeTexturePool, TEXT("VisualizeTexturePool"));
	
	uint32 Pitch;
	FColor* TextureData = (FColor*)RHICmdList.LockTexture2D((FTexture2DRHIRef&)VisualizeTexturePool->GetRenderTargetItem().ShaderResourceTexture, 0, RLM_WriteOnly, Pitch, false);
	if(TextureData)
	{
		// clear with grey to get reliable background color
		FMemory::Memset(TextureData, 0x88, TexturePoolVisualizerSizeX * TexturePoolVisualizerSizeY * 4);
		RHICmdList.GetTextureMemoryVisualizeData(TextureData, TexturePoolVisualizerSizeX, TexturePoolVisualizerSizeY, Pitch, 4096);
	}

	RHICmdList.UnlockTexture2D((FTexture2DRHIRef&)VisualizeTexturePool->GetRenderTargetItem().ShaderResourceTexture, 0, false);

	FIntPoint RTExtent = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();

	FVector2D Tex00 = FVector2D(0, 0);
	FVector2D Tex11 = FVector2D(1, 1);

//todo	VisualizeTexture(*VisualizeTexturePool, ViewFamily.RenderTarget, FIntRect(0, 0, RTExtent.X, RTExtent.Y), RTExtent, 1.0f, 0.0f, 0.0f, Tex00, Tex11, 1.0f, false);
}
#endif


/** 
* Finishes the view family rendering.
*/
void FDeferredShadingSceneRenderer::RenderFinish(FRHICommandListImmediate& RHICmdList)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		if(CVarVisualizeTexturePool.GetValueOnRenderThread())
		{
			RenderVisualizeTexturePool(RHICmdList);
		}
	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	if (GEnableGPUSkinCache)
	{
		GGPUSkinCache.TransitionToWriteable(RHICmdList);
	}

	FSceneRenderer::RenderFinish(RHICmdList);

	// Some RT should be released as early as possible to allow sharing of that memory for other purposes.
	// SceneColor is be released in tone mapping, if not we want to get access to the HDR scene color after this pass so we keep it.
	// This becomes even more important with some limited VRam (XBoxOne).
	FSceneRenderTargets::Get(RHICmdList).SetLightAttenuation(0);
}

void BuildHZB( FRHICommandListImmediate& RHICmdList, FViewInfo& View );

/** 
* Renders the view family. 
*/

DECLARE_STATS_GROUP(TEXT("Command List Markers"), STATGROUP_CommandListMarkers, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("PrePass"), STAT_CLM_PrePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender"), STAT_CLM_FXPreRender, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterPrePass"), STAT_CLM_AfterPrePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("BasePass"), STAT_CLM_BasePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterBasePass"), STAT_CLM_AfterBasePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Lighting"), STAT_CLM_Lighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterLighting"), STAT_CLM_AfterLighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Translucency"), STAT_CLM_Translucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RenderDistortion"), STAT_CLM_RenderDistortion, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterTranslucency"), STAT_CLM_AfterTranslucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RenderDistanceFieldLighting"), STAT_CLM_RenderDistanceFieldLighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("LightShaftBloom"), STAT_CLM_LightShaftBloom, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("PostProcessing"), STAT_CLM_PostProcessing, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Velocity"), STAT_CLM_Velocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterVelocity"), STAT_CLM_AfterVelocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RenderFinish"), STAT_CLM_RenderFinish, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterFrame"), STAT_CLM_AfterFrame, STATGROUP_CommandListMarkers);

FGraphEventRef FDeferredShadingSceneRenderer::OcclusionSubmittedFence[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames];
FGraphEventRef FDeferredShadingSceneRenderer::TranslucencyTimestampQuerySubmittedFence[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames + 1];

/**
 * Returns true if the depth Prepass needs to run
 */
static FORCEINLINE bool NeedsPrePass(const FDeferredShadingSceneRenderer* Renderer)
{
	return (RHIHasTiledGPU(Renderer->ViewFamily.GetShaderPlatform()) == false) && 
		(Renderer->EarlyZPassMode != DDM_None || GEarlyZPassMovable != 0);
}

/**
 * Returns true if there's a hidden area mask available
 */
static FORCEINLINE bool HasHiddenAreaMask()
{
	static const auto* const HiddenAreaMaskCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.HiddenAreaMask"));
	return (HiddenAreaMaskCVar != nullptr &&
		HiddenAreaMaskCVar->GetValueOnRenderThread() == 1 &&
		GEngine &&
		GEngine->HMDDevice.IsValid() &&
		GEngine->HMDDevice->HasHiddenAreaMesh());
}

static void SetAndClearViewGBuffer(FRHICommandListImmediate& RHICmdList, FViewInfo& View, bool bClearDepth)
{
	// if we didn't to the prepass above, then we will need to clear now, otherwise, it's already been cleared and rendered to
	ERenderTargetLoadAction DepthLoadAction = bClearDepth ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad;

	const bool bClearBlack = View.Family->EngineShowFlags.ShaderComplexity || View.Family->EngineShowFlags.StationaryLightOverlap;
	const FLinearColor ClearColor = (bClearBlack ? FLinearColor(0, 0, 0, 0) : View.BackgroundColor);

	// clearing the GBuffer
	FSceneRenderTargets::Get(RHICmdList).BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::EClear, DepthLoadAction, View.Family->EngineShowFlags.ShaderComplexity, ClearColor);
}

static TAutoConsoleVariable<int32> CVarOcclusionQueryLocation(
	TEXT("r.OcclusionQueryLocation"),
	0,
	TEXT("Controls when occlusion queries are rendered.  Rendering before the base pass may give worse occlusion (because not all occluders generally render in the earlyzpass).  ")
	TEXT("However, it may reduce CPU waiting for query result stalls on some platforms and increase overall performance.")
	TEXT("0: After BasePass.")
	TEXT("1: After EarlyZPass, but before BasePass."));

void FDeferredShadingSceneRenderer::RenderOcclusion(FRHICommandListImmediate& RHICmdList, bool bRenderQueries, bool bRenderHZB)
{		
	if (bRenderQueries || bRenderHZB)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		{
			// Update the quarter-sized depth buffer with the current contents of the scene depth texture.
			// This needs to happen before occlusion tests, which makes use of the small depth buffer.
			SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_UpdateDownsampledDepthSurface);
			UpdateDownsampledDepthSurface(RHICmdList);
		}
		
		// Issue occlusion queries
		// This is done after the downsampled depth buffer is created so that it can be used for issuing queries
		BeginOcclusionTests(RHICmdList, bRenderQueries);

		if (bRenderHZB)
		{
			RHICmdList.TransitionResource( EResourceTransitionAccess::EReadable, SceneContext.GetSceneDepthSurface() );

			static const auto ICVarAO		= IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AmbientOcclusionLevels"));
			static const auto ICVarHZBOcc	= IConsoleManager::Get().FindConsoleVariable(TEXT("r.HZBOcclusion"));
			bool bSSAO						= ICVarAO->GetValueOnRenderThread() != 0;
			bool bHZBOcclusion				= ICVarHZBOcc->GetInt() != 0;

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				FViewInfo& View = Views[ViewIndex];
				FSceneViewState* ViewState = (FSceneViewState*)View.State;
				
				const uint32 bSSR = DoScreenSpaceReflections( View );
				
				if (bSSAO || bHZBOcclusion || bSSR)
				{
					BuildHZB(RHICmdList, Views[ViewIndex]);
				}

				if (bHZBOcclusion && ViewState && ViewState->HZBOcclusionTests.GetNum() != 0)
				{
					check(ViewState->HZBOcclusionTests.IsValidFrame(ViewState->OcclusionFrameCounter));

					SCOPED_DRAW_EVENT(RHICmdList, HZB);
					ViewState->HZBOcclusionTests.Submit(RHICmdList, View);
				}
			}

			//async ssao only requires HZB and depth as inputs so get started ASAP
			if (GCompositionLighting.CanProcessAsyncSSAO(Views))
			{				
				GCompositionLighting.ProcessAsyncSSAO(RHICmdList, Views);
			}
		}

		// Hint to the RHI to submit commands up to this point to the GPU if possible.  Can help avoid CPU stalls next frame waiting
		// for these query results on some platforms.
		RHICmdList.SubmitCommandsHint();

		if (bRenderQueries && GRHIThread)
		{
			SCOPE_CYCLE_COUNTER(STAT_OcclusionSubmittedFence_Dispatch);
			int32 NumFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();
			for (int32 Dest = 1; Dest < NumFrames; Dest++)
			{
				OcclusionSubmittedFence[Dest] = OcclusionSubmittedFence[Dest - 1];
			}
			OcclusionSubmittedFence[0] = RHICmdList.RHIThreadFence();
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}
}

// The render thread is involved in sending stuff to the RHI, so we will periodically service that queue
void ServiceLocalQueue()
{
	SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Render_ServiceLocalQueue);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::RenderThread_Local);
}

// @return 0/1
static int32 GetCustomDepthPassLocation()
{		
	return FMath::Clamp(CVarCustomDepthOrder.GetValueOnRenderThread(), 0, 1);
}

extern bool IsLpvIndirectPassRequired(const FViewInfo& View);

static TAutoConsoleVariable<float> CVarStallInitViews(
	TEXT("CriticalPathStall.AfterInitViews"),
	0.0f,
	TEXT("Sleep for the given time after InitViews. Time is given in ms. This is a debug option used for critical path analysis and forcing a change in the critical path."));

void FDeferredShadingSceneRenderer::Render(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	//make sure all the targets we're going to use will be safely writable.
	GRenderTargetPool.TransitionTargetsWritable(RHICmdList);

	// this way we make sure the SceneColor format is the correct one and not the one from the end of frame before
	SceneContext.ReleaseSceneColor();

	bool bDBuffer = IsDBufferEnabled();	

	if (GRHIThread)
	{
		SCOPE_CYCLE_COUNTER(STAT_OcclusionSubmittedFence_Wait);
		int32 BlockFrame = FOcclusionQueryHelpers::GetNumBufferedFrames() - 1;
		FRHICommandListExecutor::WaitOnRHIThreadFence(OcclusionSubmittedFence[BlockFrame]);
		OcclusionSubmittedFence[BlockFrame] = nullptr;
	}

	if (!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}
	SCOPED_DRAW_EVENT(RHICmdList, Scene);
	
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Render_Init);

		// Initialize global system textures (pass-through if already initialized).
		GSystemTextures.InitializeTextures(RHICmdList, FeatureLevel);

		// Allocate the maximum scene render target space for the current view family.
		SceneContext.Allocate(RHICmdList, ViewFamily);
	}

	FGraphEventArray SortEvents;
	FILCUpdatePrimTaskData ILCTaskData;

	// Find the visible primitives.
	bool bDoInitViewAftersPrepass = InitViews(RHICmdList, ILCTaskData, SortEvents);

	TGuardValue<bool> LockDrawLists(GDrawListsLocked, true);
#if !UE_BUILD_SHIPPING
	if (CVarStallInitViews.GetValueOnRenderThread() > 0.0f)
	{
		SCOPE_CYCLE_COUNTER(STAT_InitViews_Intentional_Stall);
		FPlatformProcess::Sleep(CVarStallInitViews.GetValueOnRenderThread() / 1000.0f);
	}
#endif

	if (GRHICommandList.UseParallelAlgorithms())
	{
		// there are dynamic attempts to get this target during parallel rendering
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			Views[ViewIndex].GetEyeAdaptation(RHICmdList);
		}	
	}

	if (ShouldPrepareDistanceFields())
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DistanceFieldAO_Init);
		GDistanceFieldVolumeTextureAtlas.UpdateAllocations();
		UpdateGlobalDistanceFieldObjectBuffers(RHICmdList);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			Views[ViewIndex].HeightfieldLightingViewInfo.SetupVisibleHeightfields(Views[ViewIndex], RHICmdList);

			if (UseGlobalDistanceField())
			{
				// Use the skylight's max distance if there is one
				const float OcclusionMaxDistance = Scene->SkyLight && !Scene->SkyLight->bWantsStaticShadowing ? Scene->SkyLight->OcclusionMaxDistance : GDefaultDFAOMaxOcclusionDistance;
				UpdateGlobalDistanceFieldVolume(RHICmdList, Views[ViewIndex], Scene, OcclusionMaxDistance, Views[ViewIndex].GlobalDistanceFieldInfo);
			}
		}	
	}

	if (GRHIThread)
	{
		// we will probably stall on occlusion queries, so might as well have the RHI thread and GPU work while we wait.
		SCOPE_CYCLE_COUNTER(STAT_PostInitViews_FlushDel);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	static const auto ClearMethodCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ClearSceneMethod"));
	bool bRequiresRHIClear = true;
	bool bRequiresFarZQuadClear = false;

	static const auto GBufferCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GBuffer"));
	bool bGBuffer = GBufferCVar ? (GBufferCVar->GetValueOnRenderThread() != 0) : true;

	if (ClearMethodCVar)
	{
		int32 clearMethod = ClearMethodCVar->GetValueOnRenderThread();

		if (clearMethod == 0 && !ViewFamily.EngineShowFlags.Game)
		{
			// Do not clear the scene only if the view family is in game mode.
			clearMethod = 1;
		}

		switch (clearMethod)
		{
		case 0: // No clear
			{
				bRequiresRHIClear = false;
				bRequiresFarZQuadClear = false;
				break;
			}
		
		case 1: // RHICmdList.Clear
			{
				bRequiresRHIClear = true;
				bRequiresFarZQuadClear = false;
				break;
			}

		case 2: // Clear using far-z quad
			{
				bRequiresFarZQuadClear = true;
				bRequiresRHIClear = false;
				break;
			}
		}
	}

	// Always perform a full buffer clear for wireframe, shader complexity view mode, and stationary light overlap viewmode.
	if (bIsWireframe || ViewFamily.EngineShowFlags.ShaderComplexity || ViewFamily.EngineShowFlags.StationaryLightOverlap)
	{
		bRequiresRHIClear = true;
	}

	// force using occ queries for wireframe if rendering is parented or frozen in the first view
	check(Views.Num());
	#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const bool bIsViewFrozen = false;
		const bool bHasViewParent = false;
	#else
		const bool bIsViewFrozen = Views[0].State && ((FSceneViewState*)Views[0].State)->bIsFrozen;
		const bool bHasViewParent = Views[0].State && ((FSceneViewState*)Views[0].State)->HasViewParent();
	#endif

	
	const bool bIsOcclusionTesting = DoOcclusionQueries(FeatureLevel) && (!bIsWireframe || bIsViewFrozen || bHasViewParent);

	// Dynamic vertex and index buffers need to be committed before rendering.
	if (!bDoInitViewAftersPrepass)
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit);
			FGlobalDynamicVertexBuffer::Get().Commit();
			FGlobalDynamicIndexBuffer::Get().Commit();
		}
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_MotionBlurStartFrame);
		Scene->MotionBlurInfoData.StartFrame(ViewFamily.bWorldIsPaused);
	}

	// Notify the FX system that the scene is about to be rendered.
	bool bLateFXPrerender = CVarFXSystemPreRenderAfterPrepass.GetValueOnRenderThread() > 0;
	bool bDoFXPrerender = Scene->FXSystem && Views.IsValidIndex(0);
	if (!bLateFXPrerender && bDoFXPrerender)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FXSystem_PreRender);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender));
		Scene->FXSystem->PreRender(RHICmdList, &Views[0].GlobalDistanceFieldInfo.ParameterData);
	}

	if (GEnableGPUSkinCache)
	{
		GGPUSkinCache.TransitionToReadable(RHICmdList);
	}

	bool bDidAfterTaskWork = false;
	auto AfterTasksAreStarted = [&bDidAfterTaskWork, bDoInitViewAftersPrepass, this, &RHICmdList, &ILCTaskData, &SortEvents, bLateFXPrerender, bDoFXPrerender]()
	{
		if (!bDidAfterTaskWork)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AfterPrepassTasksWork);
			bDidAfterTaskWork = true; // only do this once
			if (bDoInitViewAftersPrepass)
			{
				InitViewsPossiblyAfterPrepass(RHICmdList, ILCTaskData, SortEvents);
				{
					SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit);
					FGlobalDynamicVertexBuffer::Get().Commit();
					FGlobalDynamicIndexBuffer::Get().Commit();
				}
				ServiceLocalQueue();
			}
			if (bLateFXPrerender && bDoFXPrerender)
			{
				SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FXSystem_PreRender);
				RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender));
				Scene->FXSystem->PreRender(RHICmdList, &Views[0].GlobalDistanceFieldInfo.ParameterData);
				ServiceLocalQueue();
			}
		}
	};

	// Draw the scene pre-pass / early z pass, populating the scene depth buffer and HiZ
	GRenderTargetPool.AddPhaseEvent(TEXT("EarlyZPass"));
	const bool bNeedsPrePass = NeedsPrePass(this);
	bool bDepthWasCleared;
	if (bNeedsPrePass)
	{
		bDepthWasCleared = RenderPrePass(RHICmdList, AfterTasksAreStarted);
	}
	else
	{
		// we didn't do the prepass, but we still want the HMD mask if there is one
		AfterTasksAreStarted();
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_PrePass));
		bDepthWasCleared = RenderPrePassHMD(RHICmdList);
	}
	check(bDidAfterTaskWork);
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterPrePass));
	ServiceLocalQueue();


	const bool bShouldRenderVelocities = ShouldRenderVelocities();
	const bool bUseVelocityGBuffer = FVelocityRendering::OutputsToGBuffer();
	const bool bUseSelectiveBasePassOutputs = UseSelectiveBasePassOutputs();
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AllocGBufferTargets);
		SceneContext.PreallocGBufferTargets(bUseVelocityGBuffer); // Even if !bShouldRenderVelocities, the velocity buffer must be bound because it's a compile time option for the shader.
		SceneContext.AllocGBufferTargets(RHICmdList);
	}

	//occlusion can't run before basepass if there's no prepass to fill in some depth to occlude against.
	bool bOcclusionBeforeBasePass = (CVarOcclusionQueryLocation.GetValueOnRenderThread() == 1) && bNeedsPrePass;	
	bool bHZBBeforeBasePass = bOcclusionBeforeBasePass && (EarlyZPassMode == EDepthDrawingMode::DDM_AllOccluders);

	RenderOcclusion(RHICmdList, bOcclusionBeforeBasePass, bHZBBeforeBasePass);
	ServiceLocalQueue();

	// Clear LPVs for all views
	if (FeatureLevel >= ERHIFeatureLevel::SM5)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ClearLPVs);
		ClearLPVs(RHICmdList);
		ServiceLocalQueue();
	}

	if(GetCustomDepthPassLocation() == 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_CustomDepthPass0);
		RenderCustomDepthPassAtLocation(RHICmdList, 0);
	}

	// only temporarily available after early z pass and until base pass
	check(!SceneContext.DBufferA);
	check(!SceneContext.DBufferB);
	check(!SceneContext.DBufferC);

	if (bDBuffer)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DBuffer);
		SceneContext.ResolveSceneDepthTexture(RHICmdList);
		SceneContext.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);

		// e.g. DBuffer deferred decals
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{	
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView,Views.Num() > 1, TEXT("View%d"), ViewIndex);

			GCompositionLighting.ProcessBeforeBasePass(RHICmdList, Views[ViewIndex]);
		}
		//GBuffer pass will want to write to SceneDepthZ
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, SceneContext.GetSceneDepthTexture());
		ServiceLocalQueue();
	}

	// Clear the G Buffer render targets
	bool bIsGBufferCurrent = false;
	if (bRequiresRHIClear)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_SetAndClearViewGBuffer);
		// set GBuffer to be current, and clear it
		SetAndClearViewGBuffer(RHICmdList, Views[0], !bDepthWasCleared);

		// depth was cleared now no matter what
		bDepthWasCleared = true;
		bIsGBufferCurrent = true;
		ServiceLocalQueue();
	}

	if (bIsWireframe && FSceneRenderer::ShouldCompositeEditorPrimitives(Views[0]))
	{
		// In Editor we want wire frame view modes to be MSAA for better quality. Resolve will be done with EditorPrimitives
		SetRenderTarget(RHICmdList, SceneContext.GetEditorPrimitivesColor(RHICmdList), SceneContext.GetEditorPrimitivesDepth(RHICmdList), ESimpleRenderTargetMode::EClearColorAndDepth);
	}
	else if (!bIsGBufferCurrent)
	{
		// make sure the GBuffer is set, in case we didn't need to clear above
		ERenderTargetLoadAction DepthLoadAction = bDepthWasCleared ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::EClear;
		SceneContext.BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::ENoAction, DepthLoadAction, ViewFamily.EngineShowFlags.ShaderComplexity);
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("BasePass"));

	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_BasePass));
	RenderBasePass(RHICmdList);
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterBasePass));
	ServiceLocalQueue();

	if (ViewFamily.EngineShowFlags.VisualizeLightCulling)
	{
		// clear out emissive and baked lighting (not too efficient but simple and only needed for this debug view)
		SceneContext.BeginRenderingSceneColor(RHICmdList);
		RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect());
	}

	SceneContext.DBufferA.SafeRelease();
	SceneContext.DBufferB.SafeRelease();
	SceneContext.DBufferC.SafeRelease();

	// only temporarily available after early z pass and until base pass
	check(!SceneContext.DBufferA);
	check(!SceneContext.DBufferB);
	check(!SceneContext.DBufferC);

	if (bRequiresFarZQuadClear)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ClearGBufferAtMaxZ);
		// Clears view by drawing quad at maximum Z
		// TODO: if all the platforms have fast color clears, we can replace this with an RHICmdList.Clear.
		ClearGBufferAtMaxZ(RHICmdList);
		ServiceLocalQueue();

		bRequiresFarZQuadClear = false;
	}
	
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ResolveDepth_After_Basepass);
		SceneContext.ResolveSceneDepthTexture(RHICmdList);
		SceneContext.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);
	}

	bool bOcclusionAfterBasePass = bIsOcclusionTesting && !bOcclusionBeforeBasePass;
	bool bHZBAfterBasePass = !bHZBBeforeBasePass;
	RenderOcclusion(RHICmdList, bOcclusionAfterBasePass, bHZBAfterBasePass);
	ServiceLocalQueue();

	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Resolve_After_Basepass);
		SceneContext.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));
		SceneContext.FinishRenderingGBuffer(RHICmdList);
	}

	if(GetCustomDepthPassLocation() == 1)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_CustomDepthPass1);
		RenderCustomDepthPassAtLocation(RHICmdList, 1);
	}

	ServiceLocalQueue();

	// Notify the FX system that opaque primitives have been rendered and we now have a valid depth buffer.
	if (Scene->FXSystem && Views.IsValidIndex(0))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FXSystem_PostRenderOpaque);
		Scene->FXSystem->PostRenderOpaque(
			RHICmdList,
			Views.GetData(),
			SceneContext.GetSceneDepthTexture(),
			SceneContext.GetGBufferATexture()
			);
		ServiceLocalQueue();
	}

	TRefCountPtr<IPooledRenderTarget> VelocityRT;

	if (bUseVelocityGBuffer)
	{
		VelocityRT = SceneContext.GetGBufferVelocityRT();
	}
	
	if (bShouldRenderVelocities && (!bUseVelocityGBuffer || bUseSelectiveBasePassOutputs))
	{
		// Render the velocities of movable objects
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Velocity));
		RenderVelocities(RHICmdList, VelocityRT);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterVelocity));
		ServiceLocalQueue();
	}

	// Copy lighting channels out of stencil before deferred decals which overwrite those values
	CopyStencilToLightingChannelTexture(RHICmdList);

	// Pre-lighting composition lighting stage
	// e.g. deferred decals, SSAO
	if (FeatureLevel >= ERHIFeatureLevel::SM4
		&& bGBuffer)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AfterBasePass);

		GRenderTargetPool.AddPhaseEvent(TEXT("AfterBasePass"));

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
			GCompositionLighting.ProcessAfterBasePass(RHICmdList, Views[ViewIndex]);
		}
		ServiceLocalQueue();
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, ClearStencilFromBasePass);

		FRHISetRenderTargetsInfo Info(0, NULL, FRHIDepthRenderTargetView(
			SceneContext.GetSceneDepthSurface(),
			ERenderTargetLoadAction::ENoAction,
			ERenderTargetStoreAction::ENoAction,
			ERenderTargetLoadAction::EClear,
			ERenderTargetStoreAction::EStore,
			FExclusiveDepthStencil::DepthNop_StencilWrite));

		// Clear stencil to 0 now that deferred decals are done using what was setup in the base pass
		// Shadow passes and other users of stencil assume it is cleared to 0 going in
		RHICmdList.SetRenderTargetsAndClear(Info);

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.GetSceneDepthSurface());
	}

	{
		GCompositionLighting.GfxWaitForAsyncSSAO(RHICmdList);
	}

	// Render lighting.
	if (ViewFamily.EngineShowFlags.Lighting
		&& FeatureLevel >= ERHIFeatureLevel::SM4
		&& ViewFamily.EngineShowFlags.DeferredLighting
		&& bGBuffer
		)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Lighting);

		GRenderTargetPool.AddPhaseEvent(TEXT("Lighting"));

		// These modulate the scenecolor output from the basepass, which is assumed to be indirect lighting
		RenderIndirectCapsuleShadows(RHICmdList);

		TRefCountPtr<IPooledRenderTarget> DynamicBentNormalAO;
		// These modulate the scenecolor output from the basepass, which is assumed to be indirect lighting
		RenderDFAOAsIndirectShadowing(RHICmdList, VelocityRT, DynamicBentNormalAO);

		// Clear the translucent lighting volumes before we accumulate
		ClearTranslucentVolumeLighting(RHICmdList);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Lighting));
		RenderLights(RHICmdList);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterLighting));
		ServiceLocalQueue();

		GRenderTargetPool.AddPhaseEvent(TEXT("AfterRenderLights"));

		InjectAmbientCubemapTranslucentVolumeLighting(RHICmdList);
		ServiceLocalQueue();

		// Filter the translucency lighting volume now that it is complete
		FilterTranslucentVolumeLighting(RHICmdList);
		ServiceLocalQueue();

		// Pre-lighting composition lighting stage
		// e.g. LPV indirect
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			FViewInfo& View = Views[ViewIndex]; 

			if(IsLpvIndirectPassRequired(View))
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView,Views.Num() > 1, TEXT("View%d"), ViewIndex);

				GCompositionLighting.ProcessLpvIndirect(RHICmdList, View);
				ServiceLocalQueue();
			}
		}

		RenderDynamicSkyLighting(RHICmdList, VelocityRT, DynamicBentNormalAO);
		ServiceLocalQueue();

		// SSS need the SceneColor finalized as an SRV.
		SceneContext.FinishRenderingSceneColor(RHICmdList, true);

		// Render reflections that only operate on opaque pixels
		RenderDeferredReflections(RHICmdList, DynamicBentNormalAO);
		ServiceLocalQueue();

		// Post-lighting composition lighting stage
		// e.g. ScreenSpaceSubsurfaceScattering
		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{	
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView,Views.Num() > 1, TEXT("View%d"), ViewIndex);
			GCompositionLighting.ProcessAfterLighting(RHICmdList, Views[ViewIndex]);
		}
		ServiceLocalQueue();
	}

	if (ViewFamily.EngineShowFlags.StationaryLightOverlap &&
		FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		RenderStationaryLightOverlap(RHICmdList);
		ServiceLocalQueue();
	}

	FLightShaftsOutput LightShaftOutput;

	// Draw Lightshafts
	if (ViewFamily.EngineShowFlags.LightShafts)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderLightShaftOcclusion);
		RenderLightShaftOcclusion(RHICmdList, LightShaftOutput);
		ServiceLocalQueue();
	}

	// Draw atmosphere
	if (ShouldRenderAtmosphere(ViewFamily))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderAtmosphere);
		if (Scene->AtmosphericFog)
		{
			// Update RenderFlag based on LightShaftTexture is valid or not
			if (LightShaftOutput.LightShaftOcclusion)
			{
				Scene->AtmosphericFog->RenderFlag &= EAtmosphereRenderFlag::E_LightShaftMask;
			}
			else
			{
				Scene->AtmosphericFog->RenderFlag |= EAtmosphereRenderFlag::E_DisableLightShaft;
			}
#if WITH_EDITOR
			if (Scene->bIsEditorScene)
			{
				// Precompute Atmospheric Textures
				Scene->AtmosphericFog->PrecomputeTextures(RHICmdList, Views.GetData(), &ViewFamily);
			}
#endif
			RenderAtmosphere(RHICmdList, LightShaftOutput);
			ServiceLocalQueue();
		}
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("Fog"));

	// Draw fog.
	if (ShouldRenderFog(ViewFamily))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderFog);
		RenderFog(RHICmdList, LightShaftOutput);
		ServiceLocalQueue();
	}

	if (GetRendererModule().HasPostOpaqueExtentions())
	{
		SceneContext.BeginRenderingSceneColor(RHICmdList);
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			const FViewInfo& View = Views[ViewIndex];
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			GetRendererModule().RenderPostOpaqueExtensions(View, RHICmdList, SceneContext);
		}

		SceneContext.FinishRenderingSceneColor(RHICmdList, true);
	}

	// No longer needed, release
	LightShaftOutput.LightShaftOcclusion = NULL;

	GRenderTargetPool.AddPhaseEvent(TEXT("Translucency"));

	// Draw translucency.
	if (ViewFamily.EngineShowFlags.Translucency)
	{
		SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Translucency));
		RenderTranslucency(RHICmdList);
		ServiceLocalQueue();

		if(GetRefractionQuality(ViewFamily) > 0)
		{
			// To apply refraction effect by distorting the scene color.
			// After non separate translucency as that is considered at scene depth anyway
			// It allows skybox translucency (set to non separate translucency) to be refracted.
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_RenderDistortion));
			RenderDistortion(RHICmdList);
			ServiceLocalQueue();
		}
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterTranslucency));
	}

	if (ViewFamily.EngineShowFlags.LightShafts)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderLightShaftBloom);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_LightShaftBloom));
		RenderLightShaftBloom(RHICmdList);
		ServiceLocalQueue();
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		GetRendererModule().RenderOverlayExtensions(View, RHICmdList, SceneContext);
	}

	if (ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO || ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI)
	{
		// Use the skylight's max distance if there is one, to be consistent with DFAO shadowing on the skylight
		const float OcclusionMaxDistance = Scene->SkyLight && !Scene->SkyLight->bWantsStaticShadowing ? Scene->SkyLight->OcclusionMaxDistance : GDefaultDFAOMaxOcclusionDistance;
		TRefCountPtr<IPooledRenderTarget> DummyOutput;
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_RenderDistanceFieldLighting));
		RenderDistanceFieldLighting(RHICmdList, FDistanceFieldAOParameters(OcclusionMaxDistance), VelocityRT, DummyOutput, DummyOutput, false, ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO, ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI); 
		ServiceLocalQueue();
	}

	if (ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields)
	{
		RenderMeshDistanceFieldVisualization(RHICmdList, FDistanceFieldAOParameters(GDefaultDFAOMaxOcclusionDistance));
		ServiceLocalQueue();
	}

	// Resolve the scene color for post processing.
	SceneContext.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

	// Finish rendering for each view.
	if (ViewFamily.bResolveScene)
	{
		SCOPED_DRAW_EVENT(RHICmdList, PostProcessing);
		SCOPE_CYCLE_COUNTER(STAT_FinishRenderViewTargetTime);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_PostProcessing));
		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			GPostProcessing.Process(RHICmdList, Views[ ViewIndex ], VelocityRT);

		}
		// we rendered to it during the frame, seems we haven't made use of it, because it should be released
		check(!FSceneRenderTargets::Get(RHICmdList).SeparateTranslucencyRT);
	}
	else
	{
		// Release the original reference on the scene render targets
		SceneContext.AdjustGBufferRefCount(RHICmdList, -1);
	}

	//grab the new transform out of the proxies for next frame
	if (VelocityRT)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_UpdateMotionBlurCache);
		Scene->MotionBlurInfoData.UpdateMotionBlurCache(Scene);
	}

	VelocityRT.SafeRelease();

	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderFinish);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_RenderFinish));
		RenderFinish(RHICmdList);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterFrame));
	}
	ServiceLocalQueue();
}

bool FDeferredShadingSceneRenderer::RenderPrePassViewDynamic(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	FDepthDrawingPolicyFactory::ContextType Context(EarlyZPassMode, true);

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

		if (MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial && MeshBatchAndRelevance.bRenderInMainPass)
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			const FPrimitiveSceneProxy* PrimitiveSceneProxy = MeshBatchAndRelevance.PrimitiveSceneProxy;
			bool bShouldUseAsOccluder = true;

			if (EarlyZPassMode < DDM_AllOccluders)
			{
				extern float GMinScreenRadiusForDepthPrepass;
				//@todo - move these proxy properties into FMeshBatchAndRelevance so we don't have to dereference the proxy in order to reject a mesh
				const float LODFactorDistanceSquared = (PrimitiveSceneProxy->GetBounds().Origin - View.ViewMatrices.ViewOrigin).SizeSquared() * FMath::Square(View.LODDistanceFactor);

				// Only render primitives marked as occluders
				bShouldUseAsOccluder = PrimitiveSceneProxy->ShouldUseAsOccluder()
					// Only render static objects unless movable are requested
					&& (!PrimitiveSceneProxy->IsMovable() || GEarlyZPassMovable)
					&& (FMath::Square(PrimitiveSceneProxy->GetBounds().SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared);
			}

			if (bShouldUseAsOccluder)
			{
				FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, View.IsInstancedStereoPass());
			}
		}
	}

	return true;
}

static void SetupPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	// Disable color writes, enable depth tests and writes.
	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	if (!View.IsInstancedStereoPass())
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	}
	else
	{
		// When rendering with instanced stereo, render the full frame.
		RHICmdList.SetViewport(0, 0, 0, View.Family->FamilySizeX, View.ViewRect.Max.Y, 1);
	}	
}

static void RenderHiddenAreaMaskView(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	const auto FeatureLevel = GMaxRHIFeatureLevel;
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, nullptr);
	GEngine->HMDDevice->DrawHiddenAreaMesh_RenderThread(RHICmdList, View.StereoPass);
}

bool FDeferredShadingSceneRenderer::RenderPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	bool bDirty = false;

	SetupPrePassView(RHICmdList, View);

	// Draw the static occluder primitives using a depth drawing policy.

	if (!View.IsInstancedStereoPass())
	{
		{
			// Draw opaque occluders which support a separate position-only
			// vertex buffer to minimize vertex fetch bandwidth, which is
			// often the bottleneck during the depth only pass.
			SCOPED_DRAW_EVENT(RHICmdList, PosOnlyOpaque);
			bDirty |= Scene->PositionOnlyDepthDrawList.DrawVisible(RHICmdList, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}
		{
			// Draw opaque occluders, using double speed z where supported.
			SCOPED_DRAW_EVENT(RHICmdList, Opaque);
			bDirty |= Scene->DepthDrawList.DrawVisible(RHICmdList, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}

		if (EarlyZPassMode >= DDM_AllOccluders)
		{
			// Draw opaque occluders with masked materials
			SCOPED_DRAW_EVENT(RHICmdList, Masked);
			bDirty |= Scene->MaskedDepthDrawList.DrawVisible(RHICmdList, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshOccluderMap, Views[1].StaticMeshOccluderMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
		{
			SCOPED_DRAW_EVENT(RHICmdList, PosOnlyOpaque);
			bDirty |= Scene->PositionOnlyDepthDrawList.DrawVisibleInstancedStereo(RHICmdList, StereoView);
		}
		{
			SCOPED_DRAW_EVENT(RHICmdList, Opaque);
			bDirty |= Scene->DepthDrawList.DrawVisibleInstancedStereo(RHICmdList, StereoView);
		}

		if (EarlyZPassMode >= DDM_AllOccluders)
		{
			SCOPED_DRAW_EVENT(RHICmdList, Masked);
			bDirty |= Scene->MaskedDepthDrawList.DrawVisibleInstancedStereo(RHICmdList, StereoView);
		}
	}
	
	{
		SCOPED_DRAW_EVENT(RHICmdList, Dynamic);
		bDirty |= RenderPrePassViewDynamic(RHICmdList, View);
	}

	return bDirty;
}

class FRenderPrepassDynamicDataThreadTask : public FRenderTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;

public:

	FRenderPrepassDynamicDataThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderPrepassDynamicDataThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ThisRenderer.RenderPrePassViewDynamic(RHICmdList, View);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

DECLARE_CYCLE_STAT(TEXT("Prepass"), STAT_CLP_Prepass, STATGROUP_ParallelCommandListMarkers);

class FPrePassParallelCommandListSet : public FParallelCommandListSet
{
public:
	FPrePassParallelCommandListSet(const FViewInfo& InView, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Prepass), InView, InParentCmdList, bInParallelExecute, bInCreateSceneContext)
	{
		// Do not copy-paste. this is a very unusual FParallelCommandListSet because it is a prepass and we want to do some work after starting some tasks
	}

	virtual ~FPrePassParallelCommandListSet()
	{
		// Do not copy-paste. this is a very unusual FParallelCommandListSet because it is a prepass and we want to do some work after starting some tasks
		SetStateOnCommandList(ParentCmdList);
		Dispatch(true);
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FSceneRenderTargets::Get(CmdList).BeginRenderingPrePass(CmdList, false);
		SetupPrePassView(CmdList, View);
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdPrePassDeferredContexts(
	TEXT("r.RHICmdPrePassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize prepass command list execution."));
static TAutoConsoleVariable<int32> CVarParallelPrePass(
	TEXT("r.ParallelPrePass"),
	1,
	TEXT("Toggles parallel zprepass rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksPrePass(
	TEXT("r.RHICmdFlushRenderThreadTasksPrePass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the pre pass.  A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksPrePass is > 0 we will flush."));

bool FDeferredShadingSceneRenderer::RenderPrePassViewParallel(const FViewInfo& View, FRHICommandListImmediate& ParentCmdList, TFunctionRef<void()> AfterTasksAreStarted, bool bDoPrePre)
{
	bool bDepthWasCleared = false;
	FPrePassParallelCommandListSet ParallelCommandListSet(View, ParentCmdList,
		CVarRHICmdPrePassDeferredContexts.GetValueOnRenderThread() > 0, 
		CVarRHICmdFlushRenderThreadTasksPrePass.GetValueOnRenderThread() == 0  && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0);

	if (!View.IsInstancedStereoPass())
	{
		// Draw the static occluder primitives using a depth drawing policy.
		// Draw opaque occluders which support a separate position-only
		// vertex buffer to minimize vertex fetch bandwidth, which is
		// often the bottleneck during the depth only pass.
		Scene->PositionOnlyDepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);

		// Draw opaque occluders, using double speed z where supported.
		Scene->DepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);

		// Draw opaque occluders with masked materials
		if (EarlyZPassMode >= DDM_AllOccluders)
		{			
			Scene->MaskedDepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);
		}
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshOccluderMap, Views[1].StaticMeshOccluderMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);

		Scene->PositionOnlyDepthDrawList.DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);
		Scene->DepthDrawList.DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);

		if (EarlyZPassMode >= DDM_AllOccluders)
		{
			Scene->MaskedDepthDrawList.DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);
		}
	}

	// we do this step here (awkwardly) so that the above tasks can be in flight while we get the particles (which must be dynamic) setup.
	if (bDoPrePre)
	{
		AfterTasksAreStarted();
		bDepthWasCleared = PreRenderPrePass(ParentCmdList);
	}

	// Dynamic
	FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

	FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderPrepassDynamicDataThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
		.ConstructAndDispatchWhenReady(*this, *CmdList, View);

	ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);

	return bDepthWasCleared;
}

/** A pixel shader used to fill the stencil buffer with the current dithered transition mask. */
class FDitheredTransitionStencilPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDitheredTransitionStencilPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FDitheredTransitionStencilPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
	{
		DitheredTransitionFactorParameter.Bind(Initializer.ParameterMap, TEXT("DitheredTransitionFactor"));
	}

	FDitheredTransitionStencilPS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);

		const float DitherFactor = View.GetTemporalLODTransition();
		SetShaderValue(RHICmdList, GetPixelShader(), DitheredTransitionFactorParameter, DitherFactor);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DitheredTransitionFactorParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter DitheredTransitionFactorParameter;
};

IMPLEMENT_SHADER_TYPE(, FDitheredTransitionStencilPS, TEXT("DitheredTransitionStencil"), TEXT("Main"), SF_Pixel);
FGlobalBoundShaderState DitheredTransitionStencilBoundShaderState;

/** Possibly do the FX prerender and setup the prepass*/
bool FDeferredShadingSceneRenderer::PreRenderPrePass(FRHICommandListImmediate& RHICmdList)
{
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_PrePass));
	bool bDepthWasCleared = RenderPrePassHMD(RHICmdList);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SceneContext.BeginRenderingPrePass(RHICmdList, !bDepthWasCleared);
	bDepthWasCleared = true;

	// Dithered transition stencil mask fill
	if (bDitheredLODTransitionsUseStencil)
	{
		SCOPED_DRAW_EVENT(RHICmdList, DitheredStencilPrePass);
		FIntPoint BufferSizeXY = SceneContext.GetBufferSizeXY();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			// Set shaders, states
			TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
			TShaderMapRef<FDitheredTransitionStencilPS> PixelShader(View.ShaderMap);
			extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
			SetGlobalBoundShaderState(RHICmdList, FeatureLevel, DitheredTransitionStencilBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

			PixelShader->SetParameters(RHICmdList, View);

			RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK>::GetRHI(), STENCIL_SANDBOX_MASK);

			DrawRectangle(
				RHICmdList,
				0, 0,
				BufferSizeXY.X, BufferSizeXY.Y,
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				BufferSizeXY,
				BufferSizeXY,
				*ScreenVertexShader,
				EDRF_UseTriangleOptimization);
		}
	}
	return bDepthWasCleared;
}

bool FDeferredShadingSceneRenderer::RenderPrePass(FRHICommandListImmediate& RHICmdList, TFunctionRef<void()> AfterTasksAreStarted)
{
	bool bDepthWasCleared = false;
	SCOPED_DRAW_EVENT(RHICmdList, PrePass);
	SCOPE_CYCLE_COUNTER(STAT_DepthDrawTime);

	bool bDirty = false;
	bool bDidPrePre = false;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	bool bParallel = GRHICommandList.UseParallelAlgorithms() && CVarParallelPrePass.GetValueOnRenderThread();

	if (!bParallel)
	{
		// nothing to be gained by delaying this.
		AfterTasksAreStarted();
		bDepthWasCleared = PreRenderPrePass(RHICmdList);
		bDidPrePre = true;
	}
	else
	{
		SceneContext.GetSceneDepthSurface(); // this probably isn't needed, but if there was some lazy allocation of the depth surface going on, we want it allocated now before we go wide. We may not have called BeginRenderingPrePass yet if bDoFXPrerender is true
	}

	// Draw a depth pass to avoid overdraw in the other passes.
	if(EarlyZPassMode != DDM_None)
	{
		if (bParallel)
		{
			FScopedCommandListWaitForTasks Flusher(CVarRHICmdFlushRenderThreadTasksPrePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0, RHICmdList);
			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				const FViewInfo& View = Views[ViewIndex];
				if (View.ShouldRenderView())
				{
					bDepthWasCleared = RenderPrePassViewParallel(View, RHICmdList, AfterTasksAreStarted, !bDidPrePre) || bDepthWasCleared;
					bDirty = true; // assume dirty since we are not going to wait
					bDidPrePre = true;
				}
			}
		}
		else
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				const FViewInfo& View = Views[ViewIndex];
				if (View.ShouldRenderView())
				{
					bDirty |= RenderPrePassView(RHICmdList, View);
				}
			}
		}
	}
	if (!bDidPrePre)
	{
		// For some reason we haven't done this yet. Best do it now for consistency with the old code.
		AfterTasksAreStarted();
		bDepthWasCleared = PreRenderPrePass(RHICmdList);
		bDidPrePre = true;
	}

	// Dithered transition stencil mask clear
	if(bDitheredLODTransitionsUseStencil)
	{
		RHICmdList.Clear(false, FLinearColor::Black, false, 0.f, true, 0, FIntRect());
	}

	SceneContext.FinishRenderingPrePass(RHICmdList);

	return bDepthWasCleared;
}

bool FDeferredShadingSceneRenderer::RenderPrePassHMD(FRHICommandListImmediate& RHICmdList)
{

	// Early out before we change any state if there's not a mask to render
	if (!HasHiddenAreaMask())
	{
		return false;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingPrePass(RHICmdList, true);

	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		if (View.StereoPass != eSSP_FULL)
		{
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			RenderHiddenAreaMaskView(RHICmdList, View);
		}
	}

	SceneContext.FinishRenderingPrePass(RHICmdList);

	return true;
}

static TAutoConsoleVariable<int32> CVarParallelBasePass(
	TEXT("r.ParallelBasePass"),
	1,
	TEXT("Toggles parallel base pass rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);

/**
 * Renders the scene's base pass 
 * @return true if anything was rendered
 */
bool FDeferredShadingSceneRenderer::RenderBasePass(FRHICommandListImmediate& RHICmdList)
{
	bool bDirty = false;

	if (ViewFamily.EngineShowFlags.LightMapDensity && AllowDebugViewmodes())
	{
		// Override the base pass with the lightmap density pass if the viewmode is enabled.
		bDirty = RenderLightMapDensities(RHICmdList);
	}
	else if (ViewFamily.EngineShowFlags.VertexDensities && AllowDebugViewmodes())
	{
		// Override the base pass with the lightmap density pass if the viewmode is enabled.
		bDirty = RenderVertexDensities(RHICmdList);
	}
	else
	{
		SCOPED_DRAW_EVENT(RHICmdList, BasePass);
		SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);

		if (GRHICommandList.UseParallelAlgorithms() && CVarParallelBasePass.GetValueOnRenderThread())
		{
			FScopedCommandListWaitForTasks Flusher(CVarRHICmdFlushRenderThreadTasksBasePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0, RHICmdList);
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				FViewInfo& View = Views[ViewIndex];
				if (View.ShouldRenderView())
				{
					RenderBasePassViewParallel(View, RHICmdList);
				}

				RenderEditorPrimitives(RHICmdList, View, bDirty);
			}

			bDirty = true; // assume dirty since we are not going to wait
		}
		else
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				FViewInfo& View = Views[ViewIndex];
				if (View.ShouldRenderView())
				{
					bDirty |= RenderBasePassView(RHICmdList, View);
				}

				RenderEditorPrimitives(RHICmdList, View, bDirty);
			}
		}	
	}

	return bDirty;
}

void FDeferredShadingSceneRenderer::ClearLPVs(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, ClearLPVs);
	SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);

	// clear light propagation volumes

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

		FViewInfo& View = Views[ViewIndex];

		FSceneViewState* ViewState = (FSceneViewState*)Views[ViewIndex].State;
		if(ViewState)
		{
			FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());

			if(LightPropagationVolume)
			{
//				SCOPED_DRAW_EVENT(RHICmdList, ClearLPVs);
//				SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);
				LightPropagationVolume->InitSettings(RHICmdList, Views[ViewIndex]);
				LightPropagationVolume->Clear(RHICmdList, View);
			}
		}
	}
}

void FDeferredShadingSceneRenderer::UpdateLPVs(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, UpdateLPVs);
	SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*)Views[ViewIndex].State;

		if(ViewState)
		{
			FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());

			if(LightPropagationVolume)
			{
//				SCOPED_DRAW_EVENT(RHICmdList, UpdateLPVs);
//				SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);
				
				LightPropagationVolume->Update(RHICmdList, View);
			}
		}
	}
}

/** A simple pixel shader used on PC to read scene depth from scene color alpha and write it to a downsized depth buffer. */
class FDownsampleSceneDepthPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDownsampleSceneDepthPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FDownsampleSceneDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		ProjectionScaleBias.Bind(Initializer.ParameterMap,TEXT("ProjectionScaleBias"));
		SourceTexelOffsets01.Bind(Initializer.ParameterMap,TEXT("SourceTexelOffsets01"));
		SourceTexelOffsets23.Bind(Initializer.ParameterMap,TEXT("SourceTexelOffsets23"));
		UseMaxDepth.Bind(Initializer.ParameterMap, TEXT("UseMaxDepth"));
	}
	FDownsampleSceneDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, bool bUseMaxDepth)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		// Used to remap view space Z (which is stored in scene color alpha) into post projection z and w so we can write z/w into the downsized depth buffer
		const FVector2D ProjectionScaleBiasValue(View.ViewMatrices.ProjMatrix.M[2][2], View.ViewMatrices.ProjMatrix.M[3][2]);
		SetShaderValue(RHICmdList, GetPixelShader(), ProjectionScaleBias, ProjectionScaleBiasValue);
		SetShaderValue(RHICmdList, GetPixelShader(), UseMaxDepth, (bUseMaxDepth ? 1.0f : 0.0f));

		FIntPoint BufferSize = SceneContext.GetBufferSizeXY();

		const uint32 DownsampledBufferSizeX = BufferSize.X / SceneContext.GetSmallColorDepthDownsampleFactor();
		const uint32 DownsampledBufferSizeY = BufferSize.Y / SceneContext.GetSmallColorDepthDownsampleFactor();

		// Offsets of the four full resolution pixels corresponding with a low resolution pixel
		const FVector4 Offsets01(0.0f, 0.0f, 1.0f / DownsampledBufferSizeX, 0.0f);
		SetShaderValue(RHICmdList, GetPixelShader(), SourceTexelOffsets01, Offsets01);
		const FVector4 Offsets23(0.0f, 1.0f / DownsampledBufferSizeY, 1.0f / DownsampledBufferSizeX, 1.0f / DownsampledBufferSizeY);
		SetShaderValue(RHICmdList, GetPixelShader(), SourceTexelOffsets23, Offsets23);
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ProjectionScaleBias;
		Ar << SourceTexelOffsets01;
		Ar << SourceTexelOffsets23;
		Ar << SceneTextureParameters;
		Ar << UseMaxDepth;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter ProjectionScaleBias;
	FShaderParameter SourceTexelOffsets01;
	FShaderParameter SourceTexelOffsets23;
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderParameter UseMaxDepth;
};

IMPLEMENT_SHADER_TYPE(,FDownsampleSceneDepthPS,TEXT("DownsampleDepthPixelShader"),TEXT("Main"),SF_Pixel);

FGlobalBoundShaderState DownsampleDepthBoundShaderState;

/** Updates the downsized depth buffer with the current full resolution depth buffer. */
void FDeferredShadingSceneRenderer::UpdateDownsampledDepthSurface(FRHICommandList& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	if (SceneContext.UseDownsizedOcclusionQueries() && (FeatureLevel >= ERHIFeatureLevel::SM4))
	{
		RHICmdList.TransitionResource( EResourceTransitionAccess::EReadable, SceneContext.GetSceneDepthSurface() );

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			DownsampleDepthSurface(RHICmdList, SceneContext.GetSmallDepthSurface(), View, 1.0f / SceneContext.GetSmallColorDepthDownsampleFactor(), true);
		}
	}
}

/** Downsample the scene depth with a specified scale factor to a specified render target
*/
void FDeferredShadingSceneRenderer::DownsampleDepthSurface(FRHICommandList& RHICmdList, const FTexture2DRHIRef& RenderTarget, const FViewInfo& View, float ScaleFactor, bool bUseMaxDepth)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SetRenderTarget(RHICmdList, NULL, RenderTarget);
	SCOPED_DRAW_EVENT(RHICmdList, DownsampleDepth);

	// Set shaders and texture
	TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
	TShaderMapRef<FDownsampleSceneDepthPS> PixelShader(View.ShaderMap);

	extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, DownsampleDepthBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());

	PixelShader->SetParameters(RHICmdList, View, bUseMaxDepth);
	const uint32 DownsampledX = FMath::TruncToInt(View.ViewRect.Min.X * ScaleFactor);
	const uint32 DownsampledY = FMath::TruncToInt(View.ViewRect.Min.Y * ScaleFactor);
	const uint32 DownsampledSizeX = FMath::TruncToInt(View.ViewRect.Width() * ScaleFactor);
	const uint32 DownsampledSizeY = FMath::TruncToInt(View.ViewRect.Height() * ScaleFactor);

	RHICmdList.SetViewport(DownsampledX, DownsampledY, 0.0f, DownsampledX + DownsampledSizeX, DownsampledY + DownsampledSizeY, 1.0f);

	DrawRectangle(
		RHICmdList,
		0, 0,
		DownsampledSizeX, DownsampledSizeY,
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		FIntPoint(DownsampledSizeX, DownsampledSizeY),
		SceneContext.GetBufferSizeXY(),
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, RenderTarget);
}

/** */
class FCopyStencilToLightingChannelsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyStencilToLightingChannelsPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("STENCIL_LIGHTING_CHANNELS_SHIFT"), STENCIL_LIGHTING_CHANNELS_BIT_ID);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_R16_UINT);
	}

	FCopyStencilToLightingChannelsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		SceneStencilTexture.Bind(Initializer.ParameterMap,TEXT("SceneStencilTexture"));
	}
	FCopyStencilToLightingChannelsPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SetSRVParameter(RHICmdList, GetPixelShader(), SceneStencilTexture, SceneContext.SceneStencilSRV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneStencilTexture;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter SceneStencilTexture;
};

IMPLEMENT_SHADER_TYPE(,FCopyStencilToLightingChannelsPS,TEXT("DownsampleDepthPixelShader"),TEXT("CopyStencilToLightingChannelsPS"),SF_Pixel);

FGlobalBoundShaderState CopyStencilBoundShaderState;

void FDeferredShadingSceneRenderer::CopyStencilToLightingChannelTexture(FRHICommandList& RHICmdList)
{
	bool bAnyViewUsesLightingChannels = false;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		bAnyViewUsesLightingChannels = bAnyViewUsesLightingChannels || View.bUsesLightingChannels;
	}

	if (bAnyViewUsesLightingChannels)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		SCOPED_DRAW_EVENT(RHICmdList, CopyStencilToLightingChannels);
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.GetSceneDepthTexture());

		SceneContext.AllocateLightingChannelTexture(RHICmdList);

		// Set the light attenuation surface as the render target, and the scene depth buffer as the depth-stencil surface.
		SetRenderTarget(RHICmdList, SceneContext.LightingChannels->GetRenderTargetItem().TargetableTexture, nullptr, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop, true);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			// Set shaders and texture
			TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
			TShaderMapRef<FCopyStencilToLightingChannelsPS> PixelShader(View.ShaderMap);

			extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

			SetGlobalBoundShaderState(RHICmdList, FeatureLevel, CopyStencilBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

			PixelShader->SetParameters(RHICmdList, View);

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Min.X + View.ViewRect.Width(), View.ViewRect.Min.Y + View.ViewRect.Height(), 1.0f);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
				SceneContext.GetBufferSizeXY(),
				*ScreenVertexShader,
				EDRF_UseTriangleOptimization);
		}

		FResolveParams ResolveParams;

		RHICmdList.CopyToResolveTarget(
			SceneContext.LightingChannels->GetRenderTargetItem().TargetableTexture, 
			SceneContext.LightingChannels->GetRenderTargetItem().TargetableTexture,
			true,
			ResolveParams);
	}
}