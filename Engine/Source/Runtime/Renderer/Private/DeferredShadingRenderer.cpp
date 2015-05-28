// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
#include "PostProcess/PostProcessing.h"
#include "DistanceFieldAtlas.h"
#include "../../Engine/Private/SkeletalRenderGPUSkin.h"		// GPrevPerBoneMotionBlur

TAutoConsoleVariable<int32> CVarEarlyZPass(
	TEXT("r.EarlyZPass"),
	3,	
	TEXT("Whether to use a depth only pass to initialize Z culling for the base pass. Cannot be changed at runtime.\n")
	TEXT("Note: also look at r.EarlyZPassMovable\n")
	TEXT("  0: off\n")
	TEXT("  1: only if not masked, and only if large on the screen\n")
	TEXT("  2: all opaque (including masked)\n")
	TEXT("  x: use built in heuristic (default is 3)"),
	ECVF_Default);


int32 GEarlyZPassMovable = 0;

/** Affects static draw lists so must reload level to propagate. */
static FAutoConsoleVariableRef CVarEarlyZPassMovable(
	TEXT("r.EarlyZPassMovable"),
	GEarlyZPassMovable,
	TEXT("Whether to render movable objects into the depth only pass.  Movable objects are typically not good occluders so this defaults to off.\n")
	TEXT("Note: also look at r.EarlyZPass"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarVisualizeTexturePool(
	TEXT("r.VisualizeTexturePool"),
	0,
	TEXT("Allows to enable the visualize the texture pool (currently only on console).\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on"),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif

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
	FLinearColor ClearColors[6] = 
		{bClearBlack ? FLinearColor(0,0,0,0) : Views[0].BackgroundColor, FLinearColor(0.5f,0.5f,0.5f,0), FLinearColor(0,0,0,1), FLinearColor(0,0,0,0), FLinearColor(0,1,1,1), FLinearColor(1,1,1,1)};

	uint32 NumActiveRenderTargets = GSceneRenderTargets.GetNumGBufferTargets();
	
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
		SetShaderValueArray(RHICmdList, PixelShader->GetPixelShader(),PixelShader->ColorParameter, ClearColors, NumActiveRenderTargets);

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
	{
		// Draw the scene's base pass draw lists.
		FScene::EBasePassDrawListType MaskedDrawType = FScene::EBasePass_Masked;
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticMaskedNoLightmap);
			bDirty |= Scene->BasePassNoLightMapDrawList[MaskedDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassSimpleDynamicLightingDrawList[MaskedDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassCachedVolumeIndirectLightingDrawList[MaskedDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassCachedPointIndirectLightingDrawList[MaskedDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
		}
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticMaskedLightmapped);
			bDirty |= Scene->BasePassHighQualityLightMapDrawList[MaskedDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassDistanceFieldShadowMapLightMapDrawList[MaskedDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassLowQualityLightMapDrawList[MaskedDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
		}
	}
	return bDirty;
}

void FDeferredShadingSceneRenderer::RenderBasePassStaticDataMaskedParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	// Draw the scene's base pass draw lists.
	FScene::EBasePassDrawListType MaskedDrawType = FScene::EBasePass_Masked;
	{
		// we can't insert this event on the parent command list; need to pass it along to the ParallelCommandListSet
		//SCOPED_DRAW_EVENT(ParentCmdList, StaticMaskedNoLightmap);
		Scene->BasePassNoLightMapDrawList[MaskedDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassSimpleDynamicLightingDrawList[MaskedDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassCachedVolumeIndirectLightingDrawList[MaskedDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassCachedPointIndirectLightingDrawList[MaskedDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
	}

	{
		// we can't insert this event on the parent command list; need to pass it along to the ParallelCommandListSet
		//SCOPED_DRAW_EVENT(ParentCmdList, StaticMaskedLightmapped);
		Scene->BasePassHighQualityLightMapDrawList[MaskedDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassDistanceFieldShadowMapLightMapDrawList[MaskedDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassLowQualityLightMapDrawList[MaskedDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
	}
}

bool FDeferredShadingSceneRenderer::RenderBasePassStaticDataDefault(FRHICommandList& RHICmdList, FViewInfo& View)
{
	bool bDirty = false;
	{
		FScene::EBasePassDrawListType OpaqueDrawType = FScene::EBasePass_Default;
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticOpaqueNoLightmap);
			bDirty |= Scene->BasePassNoLightMapDrawList[OpaqueDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassSimpleDynamicLightingDrawList[OpaqueDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassCachedVolumeIndirectLightingDrawList[OpaqueDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassCachedPointIndirectLightingDrawList[OpaqueDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
		}
		{
			SCOPED_DRAW_EVENT(RHICmdList, StaticOpaqueLightmapped);
			bDirty |= Scene->BasePassHighQualityLightMapDrawList[OpaqueDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassDistanceFieldShadowMapLightMapDrawList[OpaqueDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
			bDirty |= Scene->BasePassLowQualityLightMapDrawList[OpaqueDrawType].DrawVisible(RHICmdList, View,View.StaticMeshVisibilityMap,View.StaticMeshBatchVisibility);
		}
	}

	return bDirty;
}

void FDeferredShadingSceneRenderer::RenderBasePassStaticDataDefaultParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	FScene::EBasePassDrawListType OpaqueDrawType = FScene::EBasePass_Default;
	{
		// we can't insert this event on the parent command list; need to pass it along to the ParallelCommandListSet
		//SCOPED_DRAW_EVENT(ParentCmdList, StaticOpaqueNoLightmap);
		Scene->BasePassNoLightMapDrawList[OpaqueDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassSimpleDynamicLightingDrawList[OpaqueDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassCachedVolumeIndirectLightingDrawList[OpaqueDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassCachedPointIndirectLightingDrawList[OpaqueDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
	}

	{
		// we can't insert this event on the parent command list; need to pass it along to the ParallelCommandListSet
		//SCOPED_DRAW_EVENT(ParentCmdList, StaticOpaqueLightmapped);
		Scene->BasePassHighQualityLightMapDrawList[OpaqueDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassDistanceFieldShadowMapLightMapDrawList[OpaqueDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
		Scene->BasePassLowQualityLightMapDrawList[OpaqueDrawType].DrawVisibleParallel(ParallelCommandListSet.View.StaticMeshVisibilityMap, ParallelCommandListSet.View.StaticMeshBatchVisibility, ParallelCommandListSet);
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
			Scene->BasePassNoLightMapDrawList[DrawType].SortFrontToBack(ViewPosition);
			Scene->BasePassSimpleDynamicLightingDrawList[DrawType].SortFrontToBack(ViewPosition);
			Scene->BasePassCachedVolumeIndirectLightingDrawList[DrawType].SortFrontToBack(ViewPosition);
			Scene->BasePassCachedPointIndirectLightingDrawList[DrawType].SortFrontToBack(ViewPosition);
			Scene->BasePassHighQualityLightMapDrawList[DrawType].SortFrontToBack(ViewPosition);
			Scene->BasePassDistanceFieldShadowMapLightMapDrawList[DrawType].SortFrontToBack(ViewPosition);
			Scene->BasePassLowQualityLightMapDrawList[DrawType].SortFrontToBack(ViewPosition);
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
			FBasePassOpaqueDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
		}
	}

	View.SimpleElementCollector.DrawBatchedElements(RHICmdList, View, FTexture2DRHIRef(), EBlendModeFilter::OpaqueAndMasked);

	if( !View.Family->EngineShowFlags.CompositeEditorPrimitives )
	{
		const auto ShaderPlatform = View.GetShaderPlatform();
		const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(ShaderPlatform);

		// Draw the base pass for the view's batched mesh elements.
		bDirty = DrawViewElements<FBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, FBasePassOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), SDPG_World, true) || bDirty;

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty = View.BatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false) || bDirty;
		
		// Draw foreground objects last
		bDirty = DrawViewElements<FBasePassOpaqueDrawingPolicyFactory>(RHICmdList, View, FBasePassOpaqueDrawingPolicyFactory::ContextType(false, ESceneRenderTargetsMode::DontSet), SDPG_Foreground, true) || bDirty;

		// Draw the view's batched simple elements(lines, sprites, etc).
		bDirty = View.TopBatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), false) || bDirty;

	}

	// this little bit of code is required because multiple threads might be writing bOutDirty...this you cannot use || bDirty - type things.
	if (bDirty)
	{
		bOutDirty = true;
	}
}


class FRenderBasePassDynamicDataThreadTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	bool& OutDirty;

public:

	FRenderBasePassDynamicDataThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		bool& InOutDirty
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, OutDirty(InOutDirty)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderBasePassDynamicDataThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ThisRenderer.RenderBasePassDynamicData(RHICmdList, View, OutDirty);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

void FDeferredShadingSceneRenderer::RenderBasePassDynamicDataParallel(FParallelCommandListSet& ParallelCommandListSet)
{
	FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();
	FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderBasePassDynamicDataThreadTask>::CreateTask(nullptr, ENamedThreads::RenderThread)
		.ConstructAndDispatchWhenReady(*this, *CmdList, ParallelCommandListSet.View, ParallelCommandListSet.OutDirty);

	ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
}

static void SetupBasePassView(FRHICommandList& RHICmdList, const FIntRect& ViewRect, bool bShaderComplexity)
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
	RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0, ViewRect.Max.X, ViewRect.Max.Y, 1);
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
}

class FBasePassParallelCommandListSet : public FParallelCommandListSet
{
public:
	const FSceneViewFamily& ViewFamily;

	FBasePassParallelCommandListSet(const FViewInfo& InView, FRHICommandList& InParentCmdList, bool* InOutDirty, bool bInParallelExecute, const FSceneViewFamily& InViewFamily)
		: FParallelCommandListSet(InView, InParentCmdList, InOutDirty, bInParallelExecute)
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
		GSceneRenderTargets.BeginRenderingGBuffer(CmdList, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad);
		SetupBasePassView(CmdList, View.ViewRect, !!ViewFamily.EngineShowFlags.ShaderComplexity);
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdBasePassDeferredContexts(
	TEXT("r.RHICmdBasePassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize base pass command list execution."));

void FDeferredShadingSceneRenderer::RenderBasePassViewParallel(FViewInfo& View, FRHICommandList& ParentCmdList, bool& OutDirty)
{
	FBasePassParallelCommandListSet ParallelSet(View, ParentCmdList, &OutDirty, CVarRHICmdBasePassDeferredContexts.GetValueOnRenderThread() > 0, ViewFamily);

	RenderBasePassStaticDataParallel(ParallelSet);
	RenderBasePassDynamicDataParallel(ParallelSet);
}

bool FDeferredShadingSceneRenderer::RenderBasePassView(FRHICommandListImmediate& RHICmdList, FViewInfo& View)
{
	bool bDirty = false; 
	SetupBasePassView(RHICmdList, View.ViewRect, ViewFamily.EngineShowFlags.ShaderComplexity);
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

	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(TexturePoolVisualizerSizeX, TexturePoolVisualizerSizeY), PF_B8G8R8A8, TexCreate_None, TexCreate_None, false));
	GRenderTargetPool.FindFreeElement(Desc, VisualizeTexturePool, TEXT("VisualizeTexturePool"));
	
	uint32 Pitch;
	FColor* TextureData = (FColor*)RHICmdList.LockTexture2D((FTexture2DRHIRef&)VisualizeTexturePool->GetRenderTargetItem().ShaderResourceTexture, 0, RLM_WriteOnly, Pitch, false);
	if(TextureData)
	{
		// clear with grey to get reliable background color
		FMemory::Memset(TextureData, 0x88, TexturePoolVisualizerSizeX * TexturePoolVisualizerSizeY * 4);
		RHICmdList.GetTextureMemoryVisualizeData(TextureData, TexturePoolVisualizerSizeX, TexturePoolVisualizerSizeY, Pitch, 4096);
	}

	RHICmdList.UnlockTexture2D((FTexture2DRHIRef&)VisualizeTexturePool->GetRenderTargetItem().ShaderResourceTexture, 0, false);

	FIntPoint RTExtent = GSceneRenderTargets.GetBufferSizeXY();

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

	FSceneRenderer::RenderFinish(RHICmdList);

	// Some RT should be released as early as possible to allow sharing of that memory for other purposes.
	// SceneColor is be released in tone mapping, if not we want to get access to the HDR scene color after this pass so we keep it.
	// This becomes even more important with some limited VRam (XBoxOne).
	GSceneRenderTargets.SetLightAttenuation(0);
}

void BuildHZB( FRHICommandListImmediate& RHICmdList, FViewInfo& View );

/** 
* Renders the view family. 
*/

DECLARE_STATS_GROUP(TEXT("Command List Markers"), STATGROUP_CommandListMarkers, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("PrePass"), STAT_CLM_PrePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterPrePass"), STAT_CLM_AfterPrePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("BasePass"), STAT_CLM_BasePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterBasePass"), STAT_CLM_AfterBasePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Lighting"), STAT_CLM_Lighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterLighting"), STAT_CLM_AfterLighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Translucency"), STAT_CLM_Translucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterTranslucency"), STAT_CLM_AfterTranslucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Velocity"), STAT_CLM_Velocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterVelocity"), STAT_CLM_AfterVelocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterFrame"), STAT_CLM_AfterFrame, STATGROUP_CommandListMarkers);

FGraphEventRef FDeferredShadingSceneRenderer::OcclusionSubmittedFence;

/**
 * Returns true if the depth Prepass needs to run
 */
static FORCEINLINE bool NeedsPrePass(const FDeferredShadingSceneRenderer* Renderer)
{
	return (RHIHasTiledGPU(Renderer->ViewFamily.GetShaderPlatform()) == false) && 
		(Renderer->EarlyZPassMode != DDM_None || GEarlyZPassMovable != 0);
}

static void SetAndClearViewGBuffer(FRHICommandListImmediate& RHICmdList, FViewInfo& View, bool bClearDepth)
{
	// if we didn't to the prepass above, then we will need to clear now, otherwise, it's already been cleared and rendered to
	ERenderTargetLoadAction DepthLoadAction = bClearDepth ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad;

	const bool bClearBlack = View.Family->EngineShowFlags.ShaderComplexity || View.Family->EngineShowFlags.StationaryLightOverlap;
	const FLinearColor ClearColor = (bClearBlack ? FLinearColor(0, 0, 0, 0) : View.BackgroundColor);

	// clearing the GBuffer
	GSceneRenderTargets.BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::EClear, DepthLoadAction, ClearColor);
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
		{
			// Update the quarter-sized depth buffer with the current contents of the scene depth texture.
			// This needs to happen before occlusion tests, which makes use of the small depth buffer.
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_UpdateDownsampledDepthSurface);
			UpdateDownsampledDepthSurface(RHICmdList);
		}

		
		if (bRenderHZB)
		{			
			static const auto ICVarAO		= IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AmbientOcclusionLevels"));
			static const auto ICVarHZBOcc	= IConsoleManager::Get().FindConsoleVariable(TEXT("r.HZBOcclusion"));
			bool bSSAO						= ICVarAO->GetValueOnRenderThread() != 0;
			bool bHzbOcclusion				= ICVarHZBOcc->GetInt() != 0;

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const uint32 bSSR = DoScreenSpaceReflections(Views[ViewIndex]);
				
				if (bSSAO || bHzbOcclusion || bSSR)
				{
					BuildHZB(RHICmdList, Views[ViewIndex]);
				}
			}
		}

		// Issue occlusion queries
		// This is done after the downsampled depth buffer is created so that it can be used for issuing queries
		BeginOcclusionTests(RHICmdList, bRenderQueries, bRenderHZB);


		// Hint to the RHI to submit commands up to this point to the GPU if possible.  Can help avoid CPU stalls next frame waiting
		// for these query results on some platforms.
		RHICmdList.SubmitCommandsHint();

		if (bRenderQueries && GRHIThread)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_OcclusionSubmittedFence_Dispatch);
			OcclusionSubmittedFence = FRHICommandListExecutor::RHIThreadFence();
		}
	}
}

void FDeferredShadingSceneRenderer::Render(FRHICommandListImmediate& RHICmdList)
{
	bool bDBuffer = IsDBufferEnabled();	
	if (GRHIThread)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_OcclusionSubmittedFence_Wait);
		FRHICommandListExecutor::WaitOnRHIThreadFence(OcclusionSubmittedFence);
	}

	if(!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}
	SCOPED_DRAW_EVENT(RHICmdList, Scene);
	
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Render_Init);

		// Initialize global system textures (pass-through if already initialized).
		GSystemTextures.InitializeTextures(RHICmdList, FeatureLevel);

		// Allocate the maximum scene render target space for the current view family.
		GSceneRenderTargets.Allocate(ViewFamily);
	}
	// Find the visible primitives.
	InitViews(RHICmdList);

	if (GRHIThread)
	{
		// we will probably stall on occlusion queries, so might as well have the RHI thread and GPU work while we wait.
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostInitViews_FlushDel);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	}

	if (ShouldPrepareForDistanceFieldAO() || ShouldPrepareForDistanceFieldShadows())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DistanceFieldAO_Init);
		GDistanceFieldVolumeTextureAtlas.UpdateAllocations();
		UpdateGlobalDistanceFieldObjectBuffers(RHICmdList);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			Views[ViewIndex].HeightfieldLightingViewInfo.SetupVisibleHeightfields(Views[ViewIndex], RHICmdList);
		}	
	}

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	static const auto ClearMethodCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ClearSceneMethod"));
	bool bRequiresRHIClear = true;
	bool bRequiresFarZQuadClear = false;

	static const auto GBufferCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GBuffer"));
	bool bGBuffer = GBufferCVar ? (GBufferCVar->GetValueOnRenderThread() != 0) : true;
	if(ViewFamily.EngineShowFlags.ForceGBuffer)
	{
		bGBuffer = true;
	}

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
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit);
		FGlobalDynamicVertexBuffer::Get().Commit();
		FGlobalDynamicIndexBuffer::Get().Commit();
	}

	// Notify the FX system that the scene is about to be rendered.
	if (Scene->FXSystem)
	{
		Scene->FXSystem->PreRender(RHICmdList);
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("EarlyZPass"));

	// Draw the scene pre-pass / early z pass, populating the scene depth buffer and HiZ
	bool bDepthWasCleared = false;
	const bool bNeedsPrePass = NeedsPrePass(this);
	if (bNeedsPrePass)
	{
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_PrePass));
		RenderPrePass(RHICmdList);
		// at this point, the depth was cleared
		bDepthWasCleared = true;
	}
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterPrePass));

	//occlusion can't run before basepass if there's no prepass to fill in some depth to occlude against.
	bool bOcclusionBeforeBasePass = (CVarOcclusionQueryLocation.GetValueOnRenderThread() == 1) && bNeedsPrePass;	
	bool bHZBBeforeBasePass = false;
	RenderOcclusion(RHICmdList, bOcclusionBeforeBasePass, bHZBBeforeBasePass);
	
	const bool bShouldRenderVelocities = ShouldRenderVelocities();
	const bool bUseVelocityGBuffer = FVelocityRendering::OutputsToGBuffer();

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AllocGBufferTargets);
		GSceneRenderTargets.PreallocGBufferTargets(bShouldRenderVelocities && bUseVelocityGBuffer);
		GSceneRenderTargets.AllocGBufferTargets();
	}
	
	// Clear LPVs for all views
	if ( FeatureLevel >= ERHIFeatureLevel::SM5 )
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ClearLPVs);
		ClearLPVs(RHICmdList);
	}

	// only temporarily available after early z pass and until base pass
	check(!GSceneRenderTargets.DBufferA);
	check(!GSceneRenderTargets.DBufferB);
	check(!GSceneRenderTargets.DBufferC);

	if(bDBuffer)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DBuffer);
		GSceneRenderTargets.ResolveSceneDepthTexture(RHICmdList);
		GSceneRenderTargets.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);

		// e.g. DBuffer deferred decals
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{	
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView,Views.Num() > 1, TEXT("View%d"), ViewIndex);

			GCompositionLighting.ProcessBeforeBasePass(RHICmdList, Views[ViewIndex]);
		}
	}

	// Clear the G Buffer render targets
	bool bIsGBufferCurrent = false;
	if (bRequiresRHIClear)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_SetAndClearViewGBuffer);
		// set GBuffer to be current, and clear it
		SetAndClearViewGBuffer(RHICmdList, Views[0], !bDepthWasCleared);

		// depth was cleared now no matter what
		bDepthWasCleared = true;
		bIsGBufferCurrent = true;
	}

	if(bIsWireframe && FSceneRenderer::ShouldCompositeEditorPrimitives(Views[0]))
	{
		// In Editor we want wire frame view modes to be MSAA for better quality. Resolve will be done with EditorPrimitives
		SetRenderTarget(RHICmdList, GSceneRenderTargets.GetEditorPrimitivesColor(), GSceneRenderTargets.GetEditorPrimitivesDepth());
		RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), true, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect());
	}
	else if (!bIsGBufferCurrent)
	{
		// make sure the GBuffer is set, in case we didn't need to clear above
		ERenderTargetLoadAction DepthLoadAction = bDepthWasCleared ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::EClear;
		GSceneRenderTargets.BeginRenderingGBuffer(RHICmdList, ERenderTargetLoadAction::ENoAction, DepthLoadAction);
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("BasePass"));

	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_BasePass));
	RenderBasePass(RHICmdList);
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterBasePass));

	  if(ViewFamily.EngineShowFlags.VisualizeLightCulling)
	  {
		  // clear out emissive and baked lighting (not too efficient but simple and only needed for this debug view)
		  GSceneRenderTargets.BeginRenderingSceneColor(RHICmdList);
		  RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), false, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect());
	  }

	  GSceneRenderTargets.DBufferA.SafeRelease();
	  GSceneRenderTargets.DBufferB.SafeRelease();
	  GSceneRenderTargets.DBufferC.SafeRelease();

	  // only temporarily available after early z pass and until base pass
	  check(!GSceneRenderTargets.DBufferA);
	  check(!GSceneRenderTargets.DBufferB);
	  check(!GSceneRenderTargets.DBufferC);

	if (bRequiresFarZQuadClear)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_ClearGBufferAtMaxZ);
		// Clears view by drawing quad at maximum Z
		// TODO: if all the platforms have fast color clears, we can replace this with an RHICmdList.Clear.
		ClearGBufferAtMaxZ(RHICmdList);

		bRequiresFarZQuadClear = false;
	}
	
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Resolve_After_Basepass);

		GSceneRenderTargets.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));
		GSceneRenderTargets.ResolveSceneDepthTexture(RHICmdList);
		GSceneRenderTargets.ResolveSceneDepthToAuxiliaryTexture(RHICmdList);

		GSceneRenderTargets.FinishRenderingGBuffer(RHICmdList);

		RenderCustomDepthPass(RHICmdList);
	}
	// Notify the FX system that opaque primitives have been rendered and we now have a valid depth buffer.
	if (Scene->FXSystem && Views.IsValidIndex(0))
	{
		Scene->FXSystem->PostRenderOpaque(
			RHICmdList,
			Views.GetData(),
			GSceneRenderTargets.GetSceneDepthTexture(),
			GSceneRenderTargets.GetGBufferATexture()
			);
	}

	bool bOcclusionAfterBasePass = bIsOcclusionTesting && !bOcclusionBeforeBasePass;
	bool bHZBAfterBasePass = true;
	RenderOcclusion(RHICmdList, bOcclusionAfterBasePass, bHZBAfterBasePass);

	TRefCountPtr<IPooledRenderTarget> VelocityRT;

	if (bUseVelocityGBuffer)
	{
		VelocityRT = GSceneRenderTargets.GetGBufferVelocityRT();
	}
	else if (bShouldRenderVelocities)
	{
		// Render the velocities of movable objects
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Velocity));
		RenderVelocities(RHICmdList, VelocityRT);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterVelocity));
	}

	// Pre-lighting composition lighting stage
	// e.g. deferred decals
	if (FeatureLevel >= ERHIFeatureLevel::SM4
		&& bGBuffer)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AfterBasePass);

		GRenderTargetPool.AddPhaseEvent(TEXT("AfterBasePass"));

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
			GCompositionLighting.ProcessAfterBasePass(RHICmdList, Views[ViewIndex]);
		}
	}

	// Render lighting.
	if (ViewFamily.EngineShowFlags.Lighting
		&& FeatureLevel >= ERHIFeatureLevel::SM4
		&& ViewFamily.EngineShowFlags.DeferredLighting
		&& bGBuffer
		)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Lighting);

		GRenderTargetPool.AddPhaseEvent(TEXT("Lighting"));

		// Clear the translucent lighting volumes before we accumulate
		ClearTranslucentVolumeLighting(RHICmdList);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Lighting));
		RenderLights(RHICmdList);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterLighting));

		GRenderTargetPool.AddPhaseEvent(TEXT("AfterRenderLights"));

		InjectAmbientCubemapTranslucentVolumeLighting(RHICmdList);

		// Filter the translucency lighting volume now that it is complete
		FilterTranslucentVolumeLighting(RHICmdList);

		// Clear LPVs for all views
		if ( FeatureLevel >= ERHIFeatureLevel::SM5 )
		{
			PropagateLPVs(RHICmdList);
		}

		TRefCountPtr<IPooledRenderTarget> DynamicBentNormalAO;
		RenderDynamicSkyLighting(RHICmdList, VelocityRT, DynamicBentNormalAO);

		//SSR and SSS need the SceneColor finalized as an SRV.
		GSceneRenderTargets.FinishRenderingSceneColor(RHICmdList, true);

		// Render reflections that only operate on opaque pixels
		RenderDeferredReflections(RHICmdList, DynamicBentNormalAO);

		// Post-lighting composition lighting stage
		// e.g. ambient cubemaps, ambient occlusion, LPV indirect
		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{	
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView,Views.Num() > 1, TEXT("View%d"), ViewIndex);
			GCompositionLighting.ProcessLighting(RHICmdList, Views[ViewIndex]);
		}
	}

	if( ViewFamily.EngineShowFlags.StationaryLightOverlap &&
		FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		RenderStationaryLightOverlap(RHICmdList);
	}

	FLightShaftsOutput LightShaftOutput;

	// Draw Lightshafts
	if (ViewFamily.EngineShowFlags.LightShafts)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderLightShaftOcclusion);
		LightShaftOutput = RenderLightShaftOcclusion(RHICmdList);
	}

	// Draw atmosphere
	if(ShouldRenderAtmosphere(ViewFamily))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderAtmosphere);
		if (Scene->AtmosphericFog)
		{
			// Update RenderFlag based on LightShaftTexture is valid or not
			if (LightShaftOutput.bRendered)
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
		}
	}

	GRenderTargetPool.AddPhaseEvent(TEXT("Fog"));

	// Draw fog.
	if(ShouldRenderFog(ViewFamily))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderFog);
		RenderFog(RHICmdList, LightShaftOutput);
	}

	// No longer needed, release
	LightShaftOutput.LightShaftOcclusion = NULL;

	GRenderTargetPool.AddPhaseEvent(TEXT("Translucency"));

	// Draw translucency.
	if(ViewFamily.EngineShowFlags.Translucency)
	{
		SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_Translucency));
		RenderTranslucency(RHICmdList);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterTranslucency));

		if (ViewFamily.EngineShowFlags.Refraction)
		{
			// To apply refraction effect by distorting the scene color.
			// After non separate translucency as that is considered at scene depth anyway
			// It allows skybox translucency (set to non separate translucency) to be refracted.
			RenderDistortion(RHICmdList);
		}
	}
	if (ViewFamily.EngineShowFlags.LightShafts)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderLightShaftBloom);
		RenderLightShaftBloom(RHICmdList);
	}

	if (ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO || ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI)
	{
		// Use the skylight's max distance if there is one, to be consistent with DFAO shadowing on the skylight
		const float OcclusionMaxDistance = Scene->SkyLight && !Scene->SkyLight->bWantsStaticShadowing ? Scene->SkyLight->OcclusionMaxDistance : 600;
		TRefCountPtr<IPooledRenderTarget> DummyOutput;
		RenderDistanceFieldAOSurfaceCache(RHICmdList, FDistanceFieldAOParameters(OcclusionMaxDistance), VelocityRT, DummyOutput, DummyOutput, ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO, ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI);
	}

	if (ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields)
	{
		RenderMeshDistanceFieldVisualization(RHICmdList, FDistanceFieldAOParameters(600));
	}

	// Resolve the scene color for post processing.
	GSceneRenderTargets.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

	// Finish rendering for each view.
	if(ViewFamily.bResolveScene)
	{
		SCOPED_DRAW_EVENT(RHICmdList, PostProcessing);
		SCOPE_CYCLE_COUNTER(STAT_FinishRenderViewTargetTime);

		for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			GPostProcessing.Process(RHICmdList, Views[ ViewIndex ], VelocityRT);

			// we rendered to it during the frame, seems we haven't made use of it, because it should be released
			FSceneViewState* ViewState = (FSceneViewState*)Views[ ViewIndex ].State;
			check( !ViewState || !ViewState->SeparateTranslucencyRT );
		}
	}
	else
	{
		// Release the original reference on the scene render targets
		GSceneRenderTargets.AdjustGBufferRefCount(-1);
	}

	//grab the new transform out of the proxies for next frame
	if (VelocityRT)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_UpdateMotionBlurCache);
		Scene->MotionBlurInfoData.UpdateMotionBlurCache(Scene);
	}

	VelocityRT.SafeRelease();

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderFinish);
		RenderFinish(RHICmdList);
		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_AfterFrame));
	}
}

bool FDeferredShadingSceneRenderer::RenderPrePassViewDynamic(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	FDepthDrawingPolicyFactory::ContextType Context(EarlyZPassMode);

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
				FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
			}
		}
	}

	return true;
}

static void SetupPrePassView(FRHICommandList& RHICmdList, const FIntRect& ViewRect)
{
	// Disable color writes, enable depth tests and writes.
	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true,CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0, ViewRect.Max.X, ViewRect.Max.Y, 1);
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
}

bool FDeferredShadingSceneRenderer::RenderPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	bool bDirty = false;

	SetupPrePassView(RHICmdList, View.ViewRect);

	// Draw the static occluder primitives using a depth drawing policy.
	{
		// Draw opaque occluders which support a separate position-only
		// vertex buffer to minimize vertex fetch bandwidth, which is
		// often the bottleneck during the depth only pass.
		SCOPED_DRAW_EVENT(RHICmdList, PosOnlyOpaque);
		bDirty |= Scene->PositionOnlyDepthDrawList.DrawVisible(RHICmdList, View,View.StaticMeshOccluderMap,View.StaticMeshBatchVisibility);
	}
	{
		// Draw opaque occluders, using double speed z where supported.
		SCOPED_DRAW_EVENT(RHICmdList, Opaque);
		bDirty |= Scene->DepthDrawList.DrawVisible(RHICmdList, View,View.StaticMeshOccluderMap,View.StaticMeshBatchVisibility);
	}

	if(EarlyZPassMode >= DDM_AllOccluders)
	{
		// Draw opaque occluders with masked materials
		SCOPED_DRAW_EVENT(RHICmdList, Opaque);
		bDirty |= Scene->MaskedDepthDrawList.DrawVisible(RHICmdList, View,View.StaticMeshOccluderMap,View.StaticMeshBatchVisibility);
	}

	bDirty |= RenderPrePassViewDynamic(RHICmdList, View);
	return bDirty;
}

class FRenderPrepassDynamicDataThreadTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	bool& OutDirty;

public:

	FRenderPrepassDynamicDataThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		bool& InOutDirty
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, OutDirty(InOutDirty)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderPrepassDynamicDataThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (ThisRenderer.RenderPrePassViewDynamic(RHICmdList, View))
		{
			OutDirty = true;
		}
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

class FPrePassParallelCommandListSet : public FParallelCommandListSet
{
public:
	FPrePassParallelCommandListSet(const FViewInfo& InView, FRHICommandList& InParentCmdList, bool* InOutDirty, bool bInParallelExecute)
		: FParallelCommandListSet(InView, InParentCmdList, InOutDirty, bInParallelExecute)
	{
		SetStateOnCommandList(ParentCmdList);
	}

	virtual ~FPrePassParallelCommandListSet()
	{
		Dispatch();
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		GSceneRenderTargets.BeginRenderingPrePass(CmdList, false);
		SetupPrePassView(CmdList, View.ViewRect);
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdPrePassDeferredContexts(
	TEXT("r.RHICmdPrePassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize prepass command list execution."));

void FDeferredShadingSceneRenderer::RenderPrePassViewParallel(const FViewInfo& View, FRHICommandList& ParentCmdList, bool& OutDirty)
{
	FPrePassParallelCommandListSet ParallelCommandListSet(View, ParentCmdList, &OutDirty, CVarRHICmdPrePassDeferredContexts.GetValueOnRenderThread() > 0);


	// Draw the static occluder primitives using a depth drawing policy.
	{
		// Draw opaque occluders which support a separate position-only
		// vertex buffer to minimize vertex fetch bandwidth, which is
		// often the bottleneck during the depth only pass.
		// we can't insert this event on the parent command list; need to pass it along to the ParallelCommandListSet
		//SCOPED_DRAW_EVENT(ParentCmdList, PosOnlyOpaque);
		Scene->PositionOnlyDepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);
	}
	{
		// Draw opaque occluders, using double speed z where supported.
		// we can't insert this event on the parent command list; need to pass it along to the ParallelCommandListSet
		//SCOPED_DRAW_EVENT(ParentCmdList, Opaque);
		Scene->DepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap,View.StaticMeshBatchVisibility, ParallelCommandListSet);
	}

	if(EarlyZPassMode >= DDM_AllOccluders)
	{
		// Draw opaque occluders with masked materials
		// we can't insert this event on the parent command list; need to pass it along to the ParallelCommandListSet
		//SCOPED_DRAW_EVENT(ParentCmdList, Opaque);
		Scene->MaskedDepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap,View.StaticMeshBatchVisibility, ParallelCommandListSet);
	}
	{
		FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

		FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderPrepassDynamicDataThreadTask>::CreateTask(nullptr, ENamedThreads::RenderThread)
			.ConstructAndDispatchWhenReady(*this, *CmdList, View, OutDirty);

		ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
	}
}

static TAutoConsoleVariable<int32> CVarParallelPrePass(
	TEXT("r.ParallelPrePass"),
	1,
	TEXT("Toggles parallel zprepass rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);

/** Renders the scene's prepass and occlusion queries */
bool FDeferredShadingSceneRenderer::RenderPrePass(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, PrePass);
	SCOPE_CYCLE_COUNTER(STAT_DepthDrawTime);

	bool bDirty = false;

	GSceneRenderTargets.BeginRenderingPrePass(RHICmdList, true);

	// Draw a depth pass to avoid overdraw in the other passes.
	if(EarlyZPassMode != DDM_None)
	{
		if (GRHICommandList.UseParallelAlgorithms() && CVarParallelPrePass.GetValueOnRenderThread())
		{
			FScopedCommandListWaitForTasks Flusher(RHICmdList);

			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				const FViewInfo& View = Views[ViewIndex];
				RenderPrePassViewParallel(View, RHICmdList, bDirty);
			}
		}
		else
		{
			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				const FViewInfo& View = Views[ViewIndex];
				bDirty |= RenderPrePassView(RHICmdList, View);
			}
		}
	}

	GSceneRenderTargets.FinishRenderingPrePass(RHICmdList);

	return bDirty;
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

	if (FVelocityRendering::OutputsToGBuffer())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderBasePass_GPrevPerBoneMotionBlur_LockData);
		GPrevPerBoneMotionBlur.StartAppend(ViewFamily.bWorldIsPaused);
	}

	if(ViewFamily.EngineShowFlags.LightMapDensity && AllowDebugViewmodes())
	{
		// Override the base pass with the lightmap density pass if the viewmode is enabled.
		bDirty = RenderLightMapDensities(RHICmdList);
	}
	else
	{
		SCOPED_DRAW_EVENT(RHICmdList, BasePass);
		SCOPE_CYCLE_COUNTER(STAT_BasePassDrawTime);

		if (GRHICommandList.UseParallelAlgorithms() && CVarParallelBasePass.GetValueOnRenderThread())
		{
			FScopedCommandListWaitForTasks Flusher(RHICmdList);
			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				FViewInfo& View = Views[ViewIndex];
				RenderBasePassViewParallel(View, RHICmdList, bDirty);
			}
		}
		else
		{
			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				FViewInfo& View = Views[ViewIndex];

				bDirty |= RenderBasePassView(RHICmdList, View);
			}
		}

	}

	if (FVelocityRendering::OutputsToGBuffer())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderBasePass_GPrevPerBoneMotionBlur_UnlockData);
		GPrevPerBoneMotionBlur.EndAppend();
	}

	return bDirty;
}

void FDeferredShadingSceneRenderer::ClearLPVs(FRHICommandListImmediate& RHICmdList)
{
	// clear light propagation volumes

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		FSceneViewState* ViewState = (FSceneViewState*)Views[ViewIndex].State;
		if(ViewState)
		{
			FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume();

			if(LightPropagationVolume)
			{
				SCOPED_DRAW_EVENT(RHICmdList, ClearLPVs);
				SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);
				LightPropagationVolume->InitSettings(RHICmdList, Views[ViewIndex]);
				LightPropagationVolume->Clear(RHICmdList, View);
			}
		}
	}
}

void FDeferredShadingSceneRenderer::PropagateLPVs(FRHICommandListImmediate& RHICmdList)
{
	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		FSceneViewState* ViewState = (FSceneViewState*)Views[ViewIndex].State;
		if(ViewState)
		{
			FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume();

			if(LightPropagationVolume)
			{
				SCOPED_DRAW_EVENT(RHICmdList, UpdateLPVs);
				SCOPE_CYCLE_COUNTER(STAT_UpdateLPVs);
				
				LightPropagationVolume->Propagate(RHICmdList, View);
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
	}
	FDownsampleSceneDepthPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);

		// Used to remap view space Z (which is stored in scene color alpha) into post projection z and w so we can write z/w into the downsized depth buffer
		const FVector2D ProjectionScaleBiasValue(View.ViewMatrices.ProjMatrix.M[2][2], View.ViewMatrices.ProjMatrix.M[3][2]);
		SetShaderValue(RHICmdList, GetPixelShader(), ProjectionScaleBias, ProjectionScaleBiasValue);

		FIntPoint BufferSize = GSceneRenderTargets.GetBufferSizeXY();

		const uint32 DownsampledBufferSizeX = BufferSize.X / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor();
		const uint32 DownsampledBufferSizeY = BufferSize.Y / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor();

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
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter ProjectionScaleBias;
	FShaderParameter SourceTexelOffsets01;
	FShaderParameter SourceTexelOffsets23;
	FSceneTextureShaderParameters SceneTextureParameters;
};

IMPLEMENT_SHADER_TYPE(,FDownsampleSceneDepthPS,TEXT("DownsampleDepthPixelShader"),TEXT("Main"),SF_Pixel);

FGlobalBoundShaderState DownsampleDepthBoundShaderState;

/** Updates the downsized depth buffer with the current full resolution depth buffer. */
void FDeferredShadingSceneRenderer::UpdateDownsampledDepthSurface(FRHICommandList& RHICmdList)
{
	if (GSceneRenderTargets.UseDownsizedOcclusionQueries() && (FeatureLevel >= ERHIFeatureLevel::SM4))
	{
		SetRenderTarget(RHICmdList, NULL, GSceneRenderTargets.GetSmallDepthSurface());

		SCOPED_DRAW_EVENT(RHICmdList, DownsampleDepth);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			// Set shaders and texture
			TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
			TShaderMapRef<FDownsampleSceneDepthPS> PixelShader(View.ShaderMap);

			extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

			SetGlobalBoundShaderState(RHICmdList, FeatureLevel, DownsampleDepthBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

			RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());

			PixelShader->SetParameters(RHICmdList, View);

			const uint32 DownsampledX = FMath::TruncToInt(View.ViewRect.Min.X / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const uint32 DownsampledY = FMath::TruncToInt(View.ViewRect.Min.Y / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const uint32 DownsampledSizeX = FMath::TruncToInt(View.ViewRect.Width() / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const uint32 DownsampledSizeY = FMath::TruncToInt(View.ViewRect.Height() / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());

			RHICmdList.SetViewport(DownsampledX, DownsampledY, 0.0f, DownsampledX + DownsampledSizeX, DownsampledY + DownsampledSizeY, 1.0f);

			DrawRectangle(
				RHICmdList,
				0, 0,
				DownsampledSizeX, DownsampledSizeY,
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(DownsampledSizeX, DownsampledSizeY),
				GSceneRenderTargets.GetBufferSizeXY(),
				*ScreenVertexShader,
				EDRF_UseTriangleOptimization);
		}
	}
}
