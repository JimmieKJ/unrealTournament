// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRendering.h: Scene rendering definitions.
=============================================================================*/

#pragma once

#include "TextureLayout.h"
#include "DistortionRendering.h"
#include "CustomDepthRendering.h"
#include "HeightfieldLighting.h"

// Forward declarations.
class FPostprocessContext;

/** Information about a visible light which is specific to the view it's visible in. */
class FVisibleLightViewInfo
{
public:

	/** The dynamic primitives which are both visible and affected by this light. */
	TArray<FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleDynamicLitPrimitives;
	
	/** Whether each shadow in the corresponding FVisibleLightInfo::AllProjectedShadows array is visible. */
	FSceneBitArray ProjectedShadowVisibilityMap;

	/** The view relevance of each shadow in the corresponding FVisibleLightInfo::AllProjectedShadows array. */
	TArray<FPrimitiveViewRelevance,SceneRenderingAllocator> ProjectedShadowViewRelevanceMap;

	/** true if this light in the view frustum (dir/sky lights always are). */
	uint32 bInViewFrustum : 1;

	/** Initialization constructor. */
	FVisibleLightViewInfo()
	:	bInViewFrustum(false)
	{}
};

/** Information about a visible light which isn't view-specific. */
class FVisibleLightInfo
{
public:

	/** Projected shadows allocated on the scene rendering mem stack. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> MemStackProjectedShadows;

	/** All visible projected shadows. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> AllProjectedShadows;

	/** All visible reflective shadow maps. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> ReflectiveShadowMaps;

	/** All visible projected preshdows.  These are not allocated on the mem stack so they are refcounted. */
	TArray<TRefCountPtr<FProjectedShadowInfo>,SceneRenderingAllocator> ProjectedPreShadows;

	/** A list of per-object shadows that were occluded. We need to track these so we can issue occlusion queries for them. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> OccludedPerObjectShadows;
};


/** 
* Set of sorted translucent scene prims  
*/
class FTranslucentPrimSet
{
public:

	/** 
	* Iterate over the sorted list of prims and draw them
	* @param View - current view used to draw items
	* @param PhaseSortedPrimitives - array with the primitives we want to draw
	* @param bSeparateTranslucencyPass
	*/
	void DrawPrimitives(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View, class FDeferredShadingSceneRenderer& Renderer, bool bSeparateTranslucencyPass) const;

	/**
	* Iterate over the sorted list of prims and draw them
	* @param View - current view used to draw items
	* @param PhaseSortedPrimitives - array with the primitives we want to draw
	* @param bSeparateTranslucencyPass
	* @param FirstIndex, range of elements to render
	* @param LastIndex, range of elements to render
	*/
	void DrawPrimitivesParallel(FRHICommandList& RHICmdList, const class FViewInfo& View, class FDeferredShadingSceneRenderer& Renderer, bool bSeparateTranslucencyPass, int32 FirstIndex, int32 LastIndex) const;

	/**
	* Draw a single primitive...this is used when we are rendering in parallel and we need to handlke a translucent shadow
	* @param View - current view used to draw items
	* @param PhaseSortedPrimitives - array with the primitives we want to draw
	* @param bSeparateTranslucencyPass
	* @param Index, element to render
	*/
	void DrawAPrimitive(FRHICommandList& RHICmdList, const class FViewInfo& View, class FDeferredShadingSceneRenderer& Renderer, bool bSeparateTranslucencyPass, int32 Index) const;

	/** Draw all the primitives in this set for the forward shading pipeline. */
	void DrawPrimitivesForForwardShading(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View, class FSceneRenderer& Renderer) const;

	/**
	* Add a new primitive to the list of sorted prims
	* @param PrimitiveSceneInfo - primitive info to add. Origin of bounds is used for sort.
	* @param ViewInfo - used to transform bounds to view space
	*/
	void AddScenePrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& ViewInfo, bool bUseNormalTranslucency, bool bUseSeparateTranslucency);

	/**
	* Similar to AddScenePrimitive, but we are doing placement news and increasing counts when that happens
	*/
	static void PlaceScenePrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& ViewInfo, bool bUseNormalTranslucency, bool bUseSeparateTranslucency, void *NormalPlace, int32& NormalNum, void* SeparatePlace, int32& SeparateNum);

	/**
	* Sort any primitives that were added to the set back-to-front
	*/
	void SortPrimitives();

	/** 
	* @return number of prims to render
	*/
	int32 NumPrims() const
	{
		return SortedPrims.Num() + SortedSeparateTranslucencyPrims.Num();
	}

	/** 
	* @return number of prims that render as separate translucency
	*/
	int32 NumSeparateTranslucencyPrims() const
	{
		return SortedSeparateTranslucencyPrims.Num();
	}

	/** 
	* @return the interface to a primitive which render in separate translucency
	*/
	const FPrimitiveSceneInfo* GetSeparateTranslucencyPrim(int32 i)const
	{
		check(i>=0 && i<NumSeparateTranslucencyPrims());
		return SortedSeparateTranslucencyPrims[i].PrimitiveSceneInfo;
	}

	/** contains a sort key */
	struct FDepthSortedPrim
	{
		/** Default constructor. */
		FDepthSortedPrim() {}

		FDepthSortedPrim(FPrimitiveSceneInfo* InPrimitiveSceneInfo, float InSortKey)
			:	PrimitiveSceneInfo(InPrimitiveSceneInfo)
			,	SortKey(InSortKey)
		{
		}

		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		float SortKey;
	};

	/** contains a scene prim and its sort key */
	struct FSortedPrim :public FDepthSortedPrim
	{
		/** Default constructor. */
		FSortedPrim() {}

		FSortedPrim(FPrimitiveSceneInfo* InPrimitiveSceneInfo,float InSortKey,int32 InSortPriority)
			:	FDepthSortedPrim(InPrimitiveSceneInfo, InSortKey)
			,	SortPriority(InSortPriority)
		{
		}

		int32 SortPriority;
	};

	/**
	* Adds primitives originally created with PlaceScenePrimitive
	*/
	void AppendScenePrimitives(FSortedPrim* Normal, int32 NumNormal, FSortedPrim* Separate, int32 NumSeparate);


private:

	/** sortkey compare class */
	struct FCompareFDepthSortedPrim
	{
		FORCEINLINE bool operator()( const FDepthSortedPrim& A, const FDepthSortedPrim& B ) const
		{
			return B.SortKey < A.SortKey;
		}
	};
	/** sortkey compare class */
	struct FCompareFSortedPrim
	{
		FORCEINLINE bool operator()( const FSortedPrim& A, const FSortedPrim& B ) const
		{
			// If priorities are equal sort normally from back to front
			// otherwise lower sort priorities should render first
			return ( A.SortPriority == B.SortPriority ) ? ( B.SortKey < A.SortKey ) : ( A.SortPriority < B.SortPriority );
		}
	};

	/** list of sorted translucent primitives */
	TArray<FSortedPrim,SceneRenderingAllocator> SortedPrims;
	/** list of sorted translucent primitives that render in separate translucency. Those are not blurred by Depth of Field and don't affect bloom. */
	TArray<FSortedPrim,SceneRenderingAllocator> SortedSeparateTranslucencyPrims;

	/** Renders a single primitive for the deferred shading pipeline. */
	void RenderPrimitive(FRHICommandList& RHICmdList, const FViewInfo& View, FPrimitiveSceneInfo* PrimitiveSceneInfo, const FPrimitiveViewRelevance& ViewRelevance, const FProjectedShadowInfo* TranslucentSelfShadow, bool bSeparateTranslucencyPass) const;
};

template <> struct TIsPODType<FTranslucentPrimSet::FDepthSortedPrim> { enum { Value = true }; };
template <> struct TIsPODType<FTranslucentPrimSet::FSortedPrim> { enum { Value = true }; };

/** A batched occlusion primitive. */
struct FOcclusionPrimitive
{
	FVector Center;
	FVector Extent;
};

/**
 * Combines consecutive primitives which use the same occlusion query into a single DrawIndexedPrimitive call.
 */
class FOcclusionQueryBatcher
{
public:

	/** The maximum number of consecutive previously occluded primitives which will be combined into a single occlusion query. */
	enum { OccludedPrimitiveQueryBatchSize = 8 };

	/** Initialization constructor. */
	FOcclusionQueryBatcher(class FSceneViewState* ViewState,uint32 InMaxBatchedPrimitives);

	/** Destructor. */
	~FOcclusionQueryBatcher();

	/** Renders the current batch and resets the batch state. */
	void Flush(FRHICommandListImmediate& RHICmdList);

	/**
	 * Batches a primitive's occlusion query for rendering.
	 * @param Bounds - The primitive's bounds.
	 */
	FRenderQueryRHIParamRef BatchPrimitive(const FVector& BoundsOrigin,const FVector& BoundsBoxExtent);

private:

	struct FOcclusionBatch
	{
		FRenderQueryRHIRef Query;
		FGlobalDynamicVertexBuffer::FAllocation VertexAllocation;
	};

	/** The pending batches. */
	TArray<FOcclusionBatch,SceneRenderingAllocator> BatchOcclusionQueries;

	/** The batch new primitives are being added to. */
	FOcclusionBatch* CurrentBatchOcclusionQuery;

	/** The maximum number of primitives in a batch. */
	const uint32 MaxBatchedPrimitives;

	/** The number of primitives in the current batch. */
	uint32 NumBatchedPrimitives;

	/** The pool to allocate occlusion queries from. */
	class FRenderQueryPool* OcclusionQueryPool;
};

class FHZBOcclusionTester : public FRenderResource
{
public:
					FHZBOcclusionTester();
					~FHZBOcclusionTester() {}

	// FRenderResource interface
					virtual void	InitDynamicRHI() override;
					virtual void	ReleaseDynamicRHI() override;
	
	uint32			GetNum() const { return Primitives.Num(); }

	uint32			AddBounds( const FVector& BoundsOrigin, const FVector& BoundsExtent );
	void			Submit(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);

	void			MapResults(FRHICommandListImmediate& RHICmdList);
	void			UnmapResults(FRHICommandListImmediate& RHICmdList);
	bool			IsVisible( uint32 Index ) const;

	bool IsValidFrame(uint32 FrameNumber) const;

	void SetValidFrameNumber(uint32 FrameNumber);

private:
	enum { SizeX = 256 };
	enum { SizeY = 256 };
	enum { FrameNumberMask = 0x7fffffff };
	enum { InvalidFrameNumber = 0xffffffff };

	TArray< FOcclusionPrimitive, SceneRenderingAllocator >	Primitives;

	TRefCountPtr<IPooledRenderTarget>	ResultsTextureCPU;
	const uint8*						ResultsBuffer;


	bool IsInvalidFrame() const;

	// set ValidFrameNumber to a number that cannot be set by SetValidFrameNumber so IsValidFrame will return false for any frame number
	void SetInvalidFrameNumber();

	uint32 ValidFrameNumber;
};

class FParallelCommandListSet
{
public:
	const FViewInfo& View;
	int32 Width;
	FRHICommandList& ParentCmdList;
private:
	bool OutDirtyIfIgnored;
public:
	bool& OutDirty;
	TArray<FRHICommandList*,SceneRenderingAllocator> CommandLists;
	TArray<FGraphEventRef,SceneRenderingAllocator> Events;
protected:
	//this must be called by deriving classes virtual destructor because it calls the virtual SetStateOnCommandList.
	//C++ will not do dynamic dispatch of virtual calls from destructors so we can't call it in the base class.
	void Dispatch();
	FRHICommandList* AllocCommandList();
	bool bParallelExecute;
public:
	FParallelCommandListSet(const FViewInfo& InView, FRHICommandList& InParentCmdList, bool* InOutDirty, bool bInParallelExecute);
	virtual ~FParallelCommandListSet();
	FRHICommandList* NewParallelCommandList();
	void AddParallelCommandList(FRHICommandList* CmdList, FGraphEventRef& CompletionEvent);	

	virtual void SetStateOnCommandList(FRHICommandList& CmdList)
	{

	}
};

/** A FSceneView with additional state used by the scene renderer. */
class FViewInfo : public FSceneView
{
public:

	/** A map from primitive ID to a boolean visibility value. */
	FSceneBitArray PrimitiveVisibilityMap;

	/** Bit set when a primitive is known to be unoccluded. */
	FSceneBitArray PrimitiveDefinitelyUnoccludedMap;

	/** A map from primitive ID to a boolean is fading value. */
	FSceneBitArray PotentiallyFadingPrimitiveMap;

	/** Primitive fade uniform buffers, indexed by packed primitive index. */
	TArray<FUniformBufferRHIParamRef,SceneRenderingAllocator> PrimitiveFadeUniformBuffers;

	/** A map from primitive ID to the primitive's view relevance. */
	TArray<FPrimitiveViewRelevance,SceneRenderingAllocator> PrimitiveViewRelevanceMap;

	/** A map from static mesh ID to a boolean visibility value. */
	FSceneBitArray StaticMeshVisibilityMap;

	/** A map from static mesh ID to a boolean occluder value. */
	FSceneBitArray StaticMeshOccluderMap;

	/** A map from static mesh ID to a boolean velocity visibility value. */
	FSceneBitArray StaticMeshVelocityMap;

	/** A map from static mesh ID to a boolean shadow depth visibility value. */
	FSceneBitArray StaticMeshShadowDepthMap;

	/** An array of batch element visibility masks, valid only for meshes
	 set visible in either StaticMeshVisibilityMap or StaticMeshShadowDepthMap. */
	TArray<uint64,SceneRenderingAllocator> StaticMeshBatchVisibility;

	/** The dynamic primitives visible in this view. */
	TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleDynamicPrimitives;

	/** The dynamic editor primitives visible in this view. */
	TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleEditorPrimitives;

	/** Set of translucent prims for this view */
	FTranslucentPrimSet TranslucentPrimSet;

	/** Set of distortion prims for this view */
	FDistortionPrimSet DistortionPrimSet;
	
	/** Set of CustomDepth prims for this view */
	FCustomDepthPrimSet CustomDepthSet;

	/** A map from light ID to a boolean visibility value. */
	TArray<FVisibleLightViewInfo,SceneRenderingAllocator> VisibleLightInfos;

	/** The view's batched elements. */
	FBatchedElements BatchedViewElements;

	/** The view's batched elements, above all other elements, for gizmos that should never be occluded. */
	FBatchedElements TopBatchedViewElements;

	/** The view's mesh elements. */
	TIndirectArray<FMeshBatch> ViewMeshElements;

	/** The view's mesh elements for the foreground (editor gizmos and primitives )*/
	TIndirectArray<FMeshBatch> TopViewMeshElements;

	/** The dynamic resources used by the view elements. */
	TArray<FDynamicPrimitiveResource*> DynamicResources;

	/** Gathered in initviews from all the primitives with dynamic view relevance, used in each mesh pass. */
	TArray<FMeshBatchAndRelevance,SceneRenderingAllocator> DynamicMeshElements;

	TArray<FMeshBatchAndRelevance,SceneRenderingAllocator> DynamicEditorMeshElements;

	FSimpleElementCollector SimpleElementCollector;

	FSimpleElementCollector EditorSimpleElementCollector;

	/** Parameters for exponential height fog. */
	FVector4 ExponentialFogParameters;
	FVector ExponentialFogColor;
	float FogMaxOpacity;

	/** Parameters for directional inscattering of exponential height fog. */
	bool bUseDirectionalInscattering;
	float DirectionalInscatteringExponent;
	float DirectionalInscatteringStartDistance;
	FVector InscatteringLightDirection;
	FLinearColor DirectionalInscatteringColor;

	/** Translucency lighting volume properties. */
	FVector TranslucencyLightingVolumeMin[TVC_MAX];
	float TranslucencyVolumeVoxelSize[TVC_MAX];
	FVector TranslucencyLightingVolumeSize[TVC_MAX];

	/** true if the view has at least one mesh with a translucent material. */
	uint32 bHasTranslucentViewMeshElements : 1;
	/** Indicates whether previous frame transforms were reset this frame for any reason. */
	uint32 bPrevTransformsReset : 1;
	/** Whether we should ignore queries from last frame (useful to ignoring occlusions on the first frame after a large camera movement). */
	uint32 bIgnoreExistingQueries : 1;
	/** Whether we should submit new queries this frame. (used to disable occlusion queries completely. */
	uint32 bDisableQuerySubmissions : 1;
	/** Whether we should disable distance-based fade transitions for this frame (usually after a large camera movement.) */
	uint32 bDisableDistanceBasedFadeTransitions : 1;
	/** Bitmask of all shading models used by primitives in this view */
	uint16 ShadingModelMaskInView;

	FViewMatrices PrevViewMatrices;

	/** Last frame's view and projection matrices */
	FMatrix	PrevViewProjMatrix;

	/** Last frame's view rotation and projection matrices */
	FMatrix	PrevViewRotationProjMatrix;

	/** An intermediate number of visible static meshes.  Doesn't account for occlusion until after FinishOcclusionQueries is called. */
	int32 NumVisibleStaticMeshElements;

	/** Precomputed visibility data, the bits are indexed by VisibilityId of a primitive component. */
	const uint8* PrecomputedVisibilityData;

	FOcclusionQueryBatcher IndividualOcclusionQueries;
	FOcclusionQueryBatcher GroupedOcclusionQueries;

	// Hierarchical Z Buffer
	TRefCountPtr<IPooledRenderTarget> HZB;

	/** Used by occlusion for percent unoccluded calculations. */
	float OneOverNumPossiblePixels;

	// Mobile gets one light-shaft, this light-shaft.
	FVector4 LightShaftCenter; 
	FLinearColor LightShaftColorMask;
	FLinearColor LightShaftColorApply;
	bool bLightShaftUse;

	FHeightfieldLightingViewInfo HeightfieldLightingViewInfo;

	TShaderMap<FGlobalShaderType>* ShaderMap;

	/** Custom visibility query for view */
	ICustomVisibilityQuery* CustomVisibilityQuery;

	/** 
	 * Initialization constructor. Passes all parameters to FSceneView constructor
	 */
	FViewInfo(const FSceneViewInitOptions& InitOptions);

	/** 
	* Initialization constructor. 
	* @param InView - copy to init with
	*/
	explicit FViewInfo(const FSceneView* InView);

	/** 
	* Destructor. 
	*/
	~FViewInfo();

	/** Creates the view's uniform buffer given a set of view transforms. */
	TUniformBufferRef<FViewUniformShaderParameters> CreateUniformBuffer(
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>* DirectionalLightShadowInfo,
		const FMatrix& EffectiveTranslatedViewMatrix, 
		const FMatrix& EffectiveViewToTranslatedWorld, 
		FBox* OutTranslucentCascadeBoundsArray, 
		int32 NumTranslucentCascades) const;

	/** Initializes the RHI resources used by this view. */
	void InitRHIResources(const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>* DirectionalLightShadowInfo);

	/** Determines distance culling and fades if the state changes */
	bool IsDistanceCulled(float DistanceSquared, float MaxDrawDistance, float MinDrawDistance, const FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** Gets the eye adaptation render target for this view. */
	IPooledRenderTarget* GetEyeAdaptation() const;

	/** Create acceleration data structure and information to do forward lighting with dynamic branching. */
	void CreateLightGrid();

private:

	/** Initialization that is common to the constructors. */
	void Init();

	/** Calculates bounding boxes for the translucency lighting volume cascades. */
	void CalcTranslucencyLightingVolumeBounds(FBox* InOutCascadeBoundsArray, int32 NumCascades) const;

	/** Sets the sky SH irradiance map coefficients. */
	void SetupSkyIrradianceEnvironmentMapConstants(FVector4* OutSkyIrradianceEnvironmentMap) const;

	/** All light sources available for forward shading. Can be indexed in the shader.*/
	void CreateForwardLightDataUniformBuffer(FForwardLightData& Out) const;
};


/**
 * Used to hold combined stats for a shadow. In the case of projected shadows the shadows
 * for the preshadow and subject are combined in this stat and so are primitives with a shadow parent.
 */
struct FCombinedShadowStats
{
	/** Array of shadow subjects. The first one is the shadow parent in the case of multiple entries.	*/
	FProjectedShadowInfo::PrimitiveArrayType	SubjectPrimitives;
	/** Array of preshadow primitives in the case of projected shadows.									*/
	FProjectedShadowInfo::PrimitiveArrayType	PreShadowPrimitives;
	/** Shadow resolution in the case of projected shadows												*/
	int32									ShadowResolution;
	/** Shadow pass number in the case of projected shadows												*/
	int32									ShadowPassNumber;

	/**
	 * Default constructor. 
	 */
	FCombinedShadowStats()
	:	ShadowResolution(INDEX_NONE)
	,	ShadowPassNumber(INDEX_NONE)
	{}
};

/**
 * Masks indicating for which views a primitive needs to have a certain operation on.
 * One entry per primitive in the scene.
 */
typedef TArray<uint8, SceneRenderingAllocator> FPrimitiveViewMasks;

/**
 * Used as the scope for scene rendering functions.
 * It is initialized in the game thread by FSceneViewFamily::BeginRender, and then passed to the rendering thread.
 * The rendering thread calls Render(), and deletes the scene renderer when it returns.
 */
class FSceneRenderer
{
public:

	/** The scene being rendered. */
	FScene* Scene;

	/** The view family being rendered.  This references the Views array. */
	FSceneViewFamily ViewFamily;

	/** The views being rendered. */
	TArray<FViewInfo> Views;

	FMeshElementCollector MeshCollector;

	/** Information about the visible lights. */
	TArray<FVisibleLightInfo,SceneRenderingAllocator> VisibleLightInfos;

	/** If a freeze request has been made */
	bool bHasRequestedToggleFreeze;

	/** True if precomputed visibility was used when rendering the scene. */
	bool bUsedPrecomputedVisibility;

	/** Feature level being rendered */
	ERHIFeatureLevel::Type FeatureLevel;

public:

	FSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer);
	virtual ~FSceneRenderer();

	// FSceneRenderer interface

	virtual void Render(FRHICommandListImmediate& RHICmdList) = 0;
	virtual void RenderHitProxies(FRHICommandListImmediate& RHICmdList) {}

	/** Creates a scene renderer based on the current feature level. */
	static FSceneRenderer* CreateSceneRenderer(const FSceneViewFamily* InViewFamily, FHitProxyConsumer* HitProxyConsumer);

	bool DoOcclusionQueries(ERHIFeatureLevel::Type InFeatureLevel) const;

	/**
	* Whether or not to composite editor objects onto the scene as a post processing step
	*
	* @param View The view to test against
	*
	* @return true if compositing is needed
	*/
	static bool ShouldCompositeEditorPrimitives(const FViewInfo& View);

protected:

	// Shared functionality between all scene renderers

	/** Generates FProjectedShadowInfos for all wholesceneshadows on the given light.*/
	void AddViewDependentWholeSceneShadowsForView(
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfos, 
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfosThatNeedCulling, 
		FVisibleLightInfo& VisibleLightInfo, 
		FLightSceneInfo& LightSceneInfo);

	/**
	* Used by RenderLights to figure out if projected shadows need to be rendered to the attenuation buffer.
	* Or to render a given shadowdepth map for forward rendering.
	*
	* @param LightSceneInfo Represents the current light
	* @return true if anything needs to be rendered
	*/
	bool CheckForProjectedShadows(const FLightSceneInfo* LightSceneInfo) const;

	/** Returns whether a per object shadow should be created due to the light being a stationary light. */
	bool ShouldCreateObjectShadowForStationaryLight(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy, bool bInteractionShadowMapped) const;

	/** Gathers the list of primitives used to draw various shadow types */
	void GatherShadowPrimitives(
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& PreShadows,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
		bool bReflectionCaptureScene);

	/**
	* Checks to see if this primitive is affected by various shadow types
	*
	* @param PrimitiveSceneInfoCompact The primitive to check for shadow interaction
	* @param PreShadows The list of pre-shadows to check against
	*/
	void GatherShadowsForPrimitiveInner(const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& PreShadows,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
		bool bReflectionCaptureScene);

	/** Gets a readable light name for use with a draw event. */
	static void GetLightNameForDrawEvent(const FLightSceneProxy* LightProxy, FString& LightNameWithLevel);

	/** Gathers simple lights from visible primtives in the passed in views. */
	static void GatherSimpleLights(const FSceneViewFamily& ViewFamily, const TArray<FViewInfo>& Views, FSimpleLightArray& SimpleLights);

	/** Calculates projected shadow visibility. */
	void InitProjectedShadowVisibility(FRHICommandListImmediate& RHICmdList);	

	/** Gathers dynamic mesh elements for all shadows. */
	void GatherShadowDynamicMeshElements();

	/** Performs once per frame setup prior to visibility determination. */
	void PreVisibilityFrameSetup(FRHICommandListImmediate& RHICmdList);

	/** Computes which primitives are visible and relevant for each view. */
	void ComputeViewVisibility(FRHICommandListImmediate& RHICmdList);

	/** Performs once per frame setup after to visibility determination. */
	void PostVisibilityFrameSetup();

	void GatherDynamicMeshElements(
		TArray<FViewInfo>& Views, 
		const FScene* Scene, 
		const FSceneViewFamily& ViewFamily, 
		const FPrimitiveViewMasks& HasDynamicMeshElementsMasks, 
		const FPrimitiveViewMasks& HasDynamicEditorMeshElementsMasks, 
		FMeshElementCollector& Collector);

	/** Initialized the fog constants for each view. */
	void InitFogConstants();

	/** Initialized the atmopshere constants for each view. */
	void InitAtmosphereConstants();

	/** Returns whether there are translucent primitives to be renderered. */
	bool ShouldRenderTranslucency() const;

	/** TODO: REMOVE if no longer needed: Copies scene color to the viewport's render target after applying gamma correction. */
	void GammaCorrectToViewportRenderTarget(FRHICommandList& RHICmdList, const FViewInfo* View, float OverrideGamma);

	/** Updates state for the end of the frame. */
	void RenderFinish(FRHICommandListImmediate& RHICmdList);

	void RenderCustomDepthPass(FRHICommandListImmediate& RHICmdList);

	void OnStartFrame();

	/** Renders the scene's distortion */
	void RenderDistortion(FRHICommandListImmediate& RHICmdList);	
};


/**
 * Renderer that implements simple forward shading and associated features.
 */
class FForwardShadingSceneRenderer : public FSceneRenderer
{
public:

	FForwardShadingSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer);

	// FSceneRenderer interface

	virtual void Render(FRHICommandListImmediate& RHICmdList) override;

	virtual void RenderHitProxies(FRHICommandListImmediate& RHICmdList) override;

protected:

	void InitViews(FRHICommandListImmediate& RHICmdList);

	/** Finds the visible dynamic shadows for each view. */
	void InitDynamicShadows(FRHICommandListImmediate& RHICmdList);

	/** Renders the opaque base pass for forward shading. */
	void RenderForwardShadingBasePass(FRHICommandListImmediate& RHICmdList);

	/** Makes a copy of scene alpha so PC can emulate ES2 framebuffer fetch. */
	void CopySceneAlpha(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);

	/** Renders the base pass for translucency. */
	void RenderTranslucency(FRHICommandListImmediate& RHICmdList);

	/** Renders any necessary shadowmaps. */
	void RenderShadowDepthMaps(FRHICommandListImmediate& RHICmdList);

	/** Perform upscaling when post process is not used. */
	void BasicPostProcess(FRHICommandListImmediate& RHICmdList, FViewInfo &View, bool bDoUpscale, bool bDoEditorPrimitives);

	/**
	  * Used by RenderShadowDepthMaps to render shadowmap for the given light.
	  *
	  * @param LightSceneInfo Represents the current light
	  * @return true if anything got rendered
	  */
	bool RenderShadowDepthMap(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo);
};
