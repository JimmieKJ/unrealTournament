// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.h: Scene rendering definitions.
=============================================================================*/

#pragma once

#include "DepthRendering.h"		// EDepthDrawingMode

class FLightShaftsOutput
{
public:
	bool bRendered;
	TRefCountPtr<IPooledRenderTarget> LightShaftOcclusion;

	FLightShaftsOutput() :
		bRendered(false)
	{}
};

/**
 * Scene renderer that implements a deferred shading pipeline and associated features.
 */
class FDeferredShadingSceneRenderer : public FSceneRenderer
{
public:

	/** Defines which objects we want to render in the EarlyZPass. */
	EDepthDrawingMode EarlyZPassMode;

	/** 
	 * Layout used to track translucent self shadow residency from the per-light shadow passes, 
	 * So that shadow maps can be re-used in the translucency pass where possible.
	 */
	FTextureLayout TranslucentSelfShadowLayout;
	int32 CachedTranslucentSelfShadowLightId;

	FDeferredShadingSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer);

	/** Clears a view */
	void ClearView(FRHICommandListImmediate& RHICmdList);

	/** Clears gbuffer where Z is still at the maximum value (ie no geometry rendered) */
	void ClearGBufferAtMaxZ(FRHICommandList& RHICmdList);

	/** Clears LPVs for all views */
	void ClearLPVs(FRHICommandListImmediate& RHICmdList);

	/** Propagates LPVs for all views */
	void PropagateLPVs(FRHICommandListImmediate& RHICmdList);

	/**
	 * Renders the dynamic scene's prepass for a particular view
	 * @return true if anything was rendered
	 */
	bool RenderPrePassViewDynamic(FRHICommandList& RHICmdList, const FViewInfo& View);

	/**
	 * Renders the scene's prepass for a particular view
	 * @return true if anything was rendered
	 */
	bool RenderPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View);

	/**
	 * Renders the scene's prepass for a particular view in parallel
	 */
	void RenderPrePassViewParallel(const FViewInfo& View, FRHICommandList& ParentCmdList, bool& OutDirty);

	/** Renders the basepass for the static data of a given View. */
	bool RenderBasePassStaticData(FRHICommandList& RHICmdList, FViewInfo& View);
	bool RenderBasePassStaticDataMasked(FRHICommandList& RHICmdList, FViewInfo& View);
	bool RenderBasePassStaticDataDefault(FRHICommandList& RHICmdList, FViewInfo& View);

	/** Renders the basepass for the static data of a given View. Parallel versions.*/
	void RenderBasePassStaticDataParallel(FParallelCommandListSet& ParallelCommandListSet);
	void RenderBasePassStaticDataMaskedParallel(FParallelCommandListSet& ParallelCommandListSet);
	void RenderBasePassStaticDataDefaultParallel(FParallelCommandListSet& ParallelCommandListSet);

	/** Sorts base pass draw lists front to back for improved GPU culling. */
	void SortBasePassStaticData(FVector ViewPosition);

	/** Renders the basepass for the dynamic data of a given View. */
	void RenderBasePassDynamicData(FRHICommandList& RHICmdList, const FViewInfo& View, bool& bOutDirty);

	/** Renders the basepass for the dynamic data of a given View, in parallel. */
	void RenderBasePassDynamicDataParallel(FParallelCommandListSet& ParallelCommandListSet);

	/** Renders the basepass for a given View, in parallel */
	void RenderBasePassViewParallel(FViewInfo& View, FRHICommandList& ParentCmdList, bool& OutDirty);

	/** Renders the basepass for a given View. */
	bool RenderBasePassView(FRHICommandListImmediate& RHICmdList, FViewInfo& View);
	
	/** 
	* Renders the scene's base pass 
	* @return true if anything was rendered
	*/
	bool RenderBasePass(FRHICommandListImmediate& RHICmdList);

	/** Finishes the view family rendering. */
	void RenderFinish(FRHICommandListImmediate& RHICmdList);

	void RenderOcclusion(FRHICommandListImmediate& RHICmdList, bool bRenderQueries, bool bRenderHZB);

	/** Renders the view family. */
	virtual void Render(FRHICommandListImmediate& RHICmdList) override;

	/** Render the view family's hit proxies. */
	virtual void RenderHitProxies(FRHICommandListImmediate& RHICmdList) override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void RenderVisualizeTexturePool(FRHICommandListImmediate& RHICmdList);
#endif


	/** bound shader state for occlusion test prims */
	static FGlobalBoundShaderState OcclusionTestBoundShaderState;

private:

	static FGraphEventRef OcclusionSubmittedFence;

	/** Creates a per object projected shadow for the given interaction. */
	void CreatePerObjectProjectedShadow(
		FRHICommandListImmediate& RHICmdList,
		FLightPrimitiveInteraction* Interaction,
		bool bCreateTranslucentObjectShadow,
		bool bCreateInsetObjectShadow,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& OutPreShadows);

	/**
	* Creates a projected shadow for all primitives affected by a light.
	* @param LightSceneInfo - The light to create a shadow for.
	*/
	void CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo);

	/** Updates the preshadow cache, allocating new preshadows that can fit and evicting old ones. */
	void UpdatePreshadowCache();

	/** Finds a matching cached preshadow, if one exists. */
	TRefCountPtr<FProjectedShadowInfo> GetCachedPreshadow(
		const FLightPrimitiveInteraction* InParentInteraction,
		const FProjectedShadowInitializer& Initializer,
		const FBoxSphereBounds& Bounds,
		uint32 InResolutionX);

	/** Creates shadows for the given interaction. */
	void SetupInteractionShadows(
		FRHICommandListImmediate& RHICmdList,
		FLightPrimitiveInteraction* Interaction,
		FVisibleLightInfo& VisibleLightInfo,
		bool bStaticSceneOnly,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& PreShadows);

	/** Finds the visible dynamic shadows for each view. */
	void InitDynamicShadows(FRHICommandListImmediate& RHICmdList);

	/**
	* Used by RenderLights to figure out if light functions need to be rendered to the attenuation buffer.
	*
	* @param LightSceneInfo Represents the current light
	* @return true if anything got rendered
	*/
	bool CheckForLightFunction(const FLightSceneInfo* LightSceneInfo) const;

	/** Determines which primitives are visible for each view. */
	void InitViews(FRHICommandListImmediate& RHICmdList);

	/**
	 * Renders the scene's prepass and occlusion queries.
	 * @return true if anything was rendered
	 */
	bool RenderPrePass(FRHICommandListImmediate& RHICmdList);

	/** Issues occlusion queries. */
	void BeginOcclusionTests(FRHICommandListImmediate& RHICmdList, bool bRenderQueries, bool bRenderHZB);

	/** Renders the scene's fogging. */
	bool RenderFog(FRHICommandListImmediate& RHICmdList, FLightShaftsOutput LightShaftsOutput);

	/** Renders the scene's atmosphere. */
	void RenderAtmosphere(FRHICommandListImmediate& RHICmdList, FLightShaftsOutput LightShaftsOutput);

	/** Renders reflections that can be done in a deferred pass. */
	void RenderDeferredReflections(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO);

	/** Render dynamic sky lighting from Movable sky lights. */
	void RenderDynamicSkyLighting(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& VelocityTexture, TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO);

	/** Render Ambient Occlusion using mesh distance fields and the surface cache, which supports dynamic rigid meshes. */
	bool RenderDistanceFieldAOSurfaceCache(
		FRHICommandListImmediate& RHICmdList, 
		const class FDistanceFieldAOParameters& Parameters, 
		const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
		TRefCountPtr<IPooledRenderTarget>& OutDynamicBentNormalAO, 
		TRefCountPtr<IPooledRenderTarget>& OutDynamicIrradiance,
		bool bVisualizeAmbientOcclusion,
		bool bVisualizeGlobalIllumination);

	void RenderMeshDistanceFieldVisualization(FRHICommandListImmediate& RHICmdList, const FDistanceFieldAOParameters& Parameters);

	/** Whether tiled deferred is supported and can be used at all. */
	bool CanUseTiledDeferred() const;

	/** Whether to use tiled deferred shading given a number of lights that support it. */
	bool ShouldUseTiledDeferred(int32 NumUnshadowedLights, int32 NumSimpleLights) const;

	/** Renders the lights in SortedLights in the range [0, NumUnshadowedLights) using tiled deferred shading. */
	void RenderTiledDeferredLighting(FRHICommandListImmediate& RHICmdList, const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights, int32 NumUnshadowedLights, const FSimpleLightArray& SimpleLights);

	/** Renders the scene's lighting. */
	void RenderLights(FRHICommandListImmediate& RHICmdList);

	/** Renders an array of lights for the stationary light overlap viewmode. */
	void RenderLightArrayForOverlapViewmode(FRHICommandListImmediate& RHICmdList, const TSparseArray<FLightSceneInfoCompact>& LightArray);

	/** Render stationary light overlap as complexity to scene color. */
	void RenderStationaryLightOverlap(FRHICommandListImmediate& RHICmdList);
	
	/** 
	 * Renders the scene's translucency, parallel version
	 */
	void RenderTranslucencyParallel(FRHICommandListImmediate& RHICmdList);

	/**
	* Renders the scene's translucency.
	*/
	void RenderTranslucency(FRHICommandListImmediate& RHICmdList);

	/** Renders the scene's light shafts */
	FLightShaftsOutput RenderLightShaftOcclusion(FRHICommandListImmediate& RHICmdList);

	void RenderLightShaftBloom(FRHICommandListImmediate& RHICmdList);

	/** Reuses an existing translucent shadow map if possible or re-renders one if necessary. */
	const FProjectedShadowInfo* PrepareTranslucentShadowMap(FRHICommandList& RHICmdList, const FViewInfo& View, FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bSeparateTranslucencyPass);

	bool ShouldRenderVelocities() const;

	/** Renders the velocities of movable objects for the motion blur effect. */
	void RenderVelocities(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

	/** Renders the velocities for a subset of movable objects for the motion blur effect. */
	friend class FRenderVelocityDynamicThreadTask;
	void RenderDynamicVelocitiesMeshElementsInner(FRHICommandList& RHICmdList, const FViewInfo& View, int32 FirstIndex, int32 LastIndex);

	/** Renders the velocities of movable objects for the motion blur effect. */
	void RenderVelocitiesInner(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT);
	void RenderVelocitiesInnerParallel(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

	/** Renders world-space lightmap density instead of the normal color. */
	bool RenderLightMapDensities(FRHICommandListImmediate& RHICmdList);

	/** Updates the downsized depth buffer with the current full resolution depth buffer. */
	void UpdateDownsampledDepthSurface(FRHICommandList& RHICmdList);

	/** Renders one pass point light shadows. */
	bool RenderOnePassPointLightShadows(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bRenderedTranslucentObjectShadows, bool& bInjectedTranslucentVolume);

	/** Renders the projections of the given Shadows to the appropriate color render target. */
	void RenderProjections(
		FRHICommandListImmediate& RHICmdList,
		const FLightSceneInfo* LightSceneInfo, 
		const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& Shadows
		);

	/** Renders the shadowmaps of translucent shadows and their projections onto opaque surfaces. */
	bool RenderTranslucentProjectedShadows(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo);

	/** Renders reflective shadowmaps for LPVs */
	bool RenderReflectiveShadowMaps(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo);

	/**
	  * Used by RenderLights to render projected shadows to the attenuation buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @return true if anything got rendered
	  */
	bool RenderProjectedShadows(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bRenderedTranslucentObjectShadows, bool& bInjectedTranslucentVolume);

	/** Caches the depths of any preshadows that should be cached, and renders their projections. */
	bool RenderCachedPreshadows(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo);

	/**
	  * Used by RenderLights to render a light function to the attenuation buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @param LightIndex The light's index into FScene::Lights
	  */
	bool RenderLightFunction(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bLightAttenuationCleared);

	/** Renders a light function indicating that whole scene shadowing being displayed is for previewing only, and will go away in game. */
	bool RenderPreviewShadowsIndicator(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bLightAttenuationCleared);

	/** Renders a light function with the given material. */
	bool RenderLightFunctionForMaterial(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, const FMaterialRenderProxy* MaterialProxy, bool bLightAttenuationCleared, bool bRenderingPreviewShadowsIndicator);

	/**
	  * Used by RenderLights to render a light to the scene color buffer.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @param LightIndex The light's index into FScene::Lights
	  * @return true if anything got rendered
	  */
	void RenderLight(FRHICommandList& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bRenderOverlap, bool bIssueDrawEvent);

	/** Renders an array of simple lights using standard deferred shading. */
	void RenderSimpleLightsStandardDeferred(FRHICommandListImmediate& RHICmdList, const FSimpleLightArray& SimpleLights);

	/** Clears the translucency lighting volumes before light accumulation. */
	void ClearTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList);

	/** Add AmbientCubemap to the lighting volumes. */
	void InjectAmbientCubemapTranslucentVolumeLighting(FRHICommandList& RHICmdList);

	/** Clears the volume texture used to accumulate per object shadows for translucency. */
	void ClearTranslucentVolumePerObjectShadowing(FRHICommandList& RHICmdList);

	/** Accumulates the per object shadow's contribution for translucency. */
	void AccumulateTranslucentVolumeObjectShadowing(FRHICommandList& RHICmdList, const FProjectedShadowInfo* InProjectedShadowInfo, bool bClearVolume);

	/** Accumulates direct lighting for the given light.  InProjectedShadowInfo can be NULL in which case the light will be unshadowed. */
	void InjectTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo& LightSceneInfo, const FProjectedShadowInfo* InProjectedShadowInfo);

	/** Accumulates direct lighting for an array of unshadowed lights. */
	void InjectTranslucentVolumeLightingArray(FRHICommandListImmediate& RHICmdList, const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights, int32 NumLights);

	/** Accumulates direct lighting for simple lights. */
	void InjectSimpleTranslucentVolumeLightingArray(FRHICommandListImmediate& RHICmdList, const FSimpleLightArray& SimpleLights);

	/** Filters the translucency lighting volumes to reduce aliasing. */
	void FilterTranslucentVolumeLighting(FRHICommandListImmediate& RHICmdList);

	/** Output SpecularColor * IndirectDiffuseGI for metals so they are not black in reflections */
	void RenderReflectionCaptureSpecularBounceForAllViews(FRHICommandListImmediate& RHICmdList);

	/** Render image based reflections (SSR, Env, SkyLight) with compute shaders */
	void RenderTiledDeferredImageBasedReflections(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO);

	/** Render image based reflections (SSR, Env, SkyLight) without compute shaders */
	void RenderStandardDeferredImageBasedReflections(FRHICommandListImmediate& RHICmdList, bool bReflectionEnv, const TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO);

	bool ShouldDoReflectionEnvironment() const;
	
	bool ShouldRenderDistanceFieldAO() const;

	/** Whether distance field global data structures should be prepared for features that use it. */
	bool ShouldPrepareForDistanceFieldShadows() const;
	bool ShouldPrepareForDistanceFieldAO() const;

	void UpdateGlobalDistanceFieldObjectBuffers(FRHICommandListImmediate& RHICmdList);

	friend class FTranslucentPrimSet;
};
