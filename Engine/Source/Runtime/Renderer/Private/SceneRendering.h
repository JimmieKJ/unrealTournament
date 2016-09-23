// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRendering.h: Scene rendering definitions.
=============================================================================*/

#pragma once

#include "TextureLayout.h"
#include "DistortionRendering.h"
#include "CustomDepthRendering.h"
#include "HeightfieldLighting.h"
#include "GlobalDistanceFieldParameters.h"

// Forward declarations.
class FPostprocessContext;
struct FILCUpdatePrimTaskData;

/** Mobile only. Information used to determine whether static meshes will be rendered with CSM shaders or not. */
class FMobileCSMVisibilityInfo
{
public:
	/** true if there are any primitives affected by CSM subjects */
	uint32 bMobileDynamicCSMInUse : 1;

	/** Visibility lists for static meshes that will use expensive CSM shaders. */
	FSceneBitArray MobilePrimitiveCSMReceiverVisibilityMap;
	FSceneBitArray MobileCSMStaticMeshVisibilityMap;
	TArray<uint64, SceneRenderingAllocator> MobileCSMStaticBatchVisibility;

	/** Visibility lists for static meshes that will use the non CSM shaders. */
	FSceneBitArray MobileNonCSMStaticMeshVisibilityMap;
	TArray<uint64, SceneRenderingAllocator> MobileNonCSMStaticBatchVisibility;

	/** Initialization constructor. */
	FMobileCSMVisibilityInfo() : bMobileDynamicCSMInUse(false)
	{}
};

/** Stores a list of CSM shadow casters. Used by mobile renderer for culling primitives receiving static + CSM shadows. */
class FMobileCSMSubjectPrimitives
{
public:
	/** Adds a subject primitive */
	void AddSubjectPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo, int32 PrimitiveId)
	{
		checkSlow(PrimitiveSceneInfo->GetIndex() == PrimitiveId);
		const int32 PrimitiveIndex = PrimitiveSceneInfo->GetIndex();
		if (!ShadowSubjectPrimitivesEncountered[PrimitiveId])
		{
			ShadowSubjectPrimitives.Add(PrimitiveSceneInfo);
			ShadowSubjectPrimitivesEncountered[PrimitiveId] = true;
		}
	}

	/** Returns the list of subject primitives */
	const TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator>& GetShadowSubjectPrimitives() const
	{
		return ShadowSubjectPrimitives;
	}

	/** Used to initialize the ShadowSubjectPrimitivesEncountered bit array
	  * to prevent shadow primitives being added more than once. */
	void InitShadowSubjectPrimitives(int32 PrimitiveCount)
	{
		ShadowSubjectPrimitivesEncountered.Init(false, PrimitiveCount);
	}

protected:
	/** List of this light's shadow subject primitives. */
	FSceneBitArray ShadowSubjectPrimitivesEncountered;
	TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowSubjectPrimitives;
};

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

	/** List of CSM shadow casters. Used by mobile renderer for culling primitives receiving static + CSM shadows */
	FMobileCSMSubjectPrimitives MobileCSMSubjectPrimitives;

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

	/** All visible projected shadows, output of shadow setup.  Not all of these will be rendered. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> AllProjectedShadows;

	/** Shadows to project for each feature that needs special handling. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> ShadowsToProject;
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> CapsuleShadowsToProject;
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> RSMsToProject;

	/** All visible projected preshdows.  These are not allocated on the mem stack so they are refcounted. */
	TArray<TRefCountPtr<FProjectedShadowInfo>,SceneRenderingAllocator> ProjectedPreShadows;

	/** A list of per-object shadows that were occluded. We need to track these so we can issue occlusion queries for them. */
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> OccludedPerObjectShadows;
};

// enum instead of bool to get better visibility when we pass around multiple bools, also allows for easier extensions
namespace ETranslucencyPass
{
	enum Type
	{
		TPT_StandardTranslucency,
		TPT_SeparateTranslucency,

		TPT_MAX
	};
};

// Stores the primitive count of each translucency pass (redundant, could be computed after sorting but this way we touch less memory)
struct FTranslucenyPrimCount
{
private:
	uint32 Count[ETranslucencyPass::TPT_MAX];

public:
	// constructor
	FTranslucenyPrimCount()
	{
		for(uint32 i = 0; i < ETranslucencyPass::TPT_MAX; ++i)
		{
			Count[i] = 0;
		}
	}

	// interface similar to TArray but here we only store the count of Prims per pass
	void Append(const FTranslucenyPrimCount& InSrc)
	{
		for(uint32 i = 0; i < ETranslucencyPass::TPT_MAX; ++i)
		{
			Count[i] += InSrc.Count[i];
		}
	}

	// interface similar to TArray but here we only store the count of Prims per pass
	void Add(ETranslucencyPass::Type InPass)
	{
		++Count[InPass];
	}

	// @return range in SortedPrims[] after sorting
	FInt32Range GetPassRange(ETranslucencyPass::Type InPass) const
	{
		checkSlow(InPass < ETranslucencyPass::TPT_MAX);

		// can be optimized (if needed)

		// inclusive
		int32 Start = 0;

		uint32 i = 0;

		for(; i < (uint32)InPass; ++i)
		{
			Start += Count[i];
		}

		// exclusive
		int32 End = Start + Count[i];
		
		return FInt32Range(Start, End);
	}

	int32 Num(ETranslucencyPass::Type InPass) const
	{
		return Count[InPass];
	}
};


/** 
* Set of sorted scene prims  
*/
template <class TKey>
class FSortedPrimSet
{
public:
	// contains a scene prim and its sort key
	struct FSortedPrim
	{
		// Default constructor
		FSortedPrim() {}

		FSortedPrim(FPrimitiveSceneInfo* InPrimitiveSceneInfo, const TKey InSortKey)
			:	PrimitiveSceneInfo(InPrimitiveSceneInfo)
			,	SortKey(InSortKey)
		{
		}

		FORCEINLINE bool operator<( const FSortedPrim& rhs ) const
		{
			return SortKey < rhs.SortKey;
		}

		//
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		//
		TKey SortKey;
	};

	/**
	* Sort any primitives that were added to the set back-to-front
	*/
	void SortPrimitives()
	{
		Prims.Sort();
	}

	/** 
	* @return number of prims to render
	*/
	int32 NumPrims() const
	{
		return Prims.Num();
	}

	/** list of primitives, sorted after calling Sort() */
	TArray<FSortedPrim, SceneRenderingAllocator> Prims;
};

template <> struct TIsPODType<FSortedPrimSet<uint32>::FSortedPrim> { enum { Value = true }; };

class FMeshDecalPrimSet : public FSortedPrimSet<uint32>
{
public:
	typedef FSortedPrimSet<uint32>::FSortedPrim KeyType;

	static KeyType GenerateKey(FPrimitiveSceneInfo* PrimitiveSceneInfo)
	{
		return KeyType(PrimitiveSceneInfo, 0);
	}
};

/** 
* Set of sorted translucent scene prims  
*/
class FTranslucentPrimSet
{
public:
	/** contains a scene prim and its sort key */
	struct FTranslucentSortedPrim
	{
		/** Default constructor. */
		FTranslucentSortedPrim() {}

		// @param InPass (first we sort by this)
		// @param InSortPriority SHRT_MIN .. SHRT_MAX (then we sort by this)
		// @param InSortKey from UPrimitiveComponent::TranslucencySortPriority e.g. SortByDistance/SortAlongAxis (then by this)
		FTranslucentSortedPrim(FPrimitiveSceneInfo* InPrimitiveSceneInfo, ETranslucencyPass::Type InPass, int16 InSortPriority, float InSortKey)
			:	PrimitiveSceneInfo(InPrimitiveSceneInfo)
			,	SortKey(InSortKey)
		{
			SetSortOrder(InPass, InSortPriority);
		}

		void SetSortOrder(ETranslucencyPass::Type InPass, int16 InSortPriority)
		{
			uint32 UpperShort = (uint32)InPass;
			// 0 .. 0xffff
			int32 SortPriorityWithoutSign = (int32)InSortPriority - (int32)SHRT_MIN;
			uint32 LowerShort = SortPriorityWithoutSign;

			check(LowerShort <= 0xffff);

			// top 8 bits are currently unused
			SortOrder = (UpperShort << 16) | LowerShort;
		}

		//
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		// single 32bit sort order containing Pass and SortPriority (first we sort by this)
		uint32 SortOrder;
		// from UPrimitiveComponent::TranslucencySortPriority (then by this)
		float SortKey;
	};

	/** 
	* Iterate over the sorted list of prims and draw them
	* @param View - current view used to draw items
	* @param PhaseSortedPrimitives - array with the primitives we want to draw
	* @param TranslucenyPassType
	*/
	void DrawPrimitives(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View, class FDeferredShadingSceneRenderer& Renderer, ETranslucencyPass::Type TranslucenyPassType) const;

	/**
	* Iterate over the sorted list of prims and draw them
	* @param View - current view used to draw items
	* @param PhaseSortedPrimitives - array with the primitives we want to draw
	* @param TranslucenyPassType
	* @param FirstPrimIdx, range of elements to render (included), index into SortedPrims[] after sorting
	* @param LastPrimIdx, range of elements to render (included), index into SortedPrims[] after sorting
	*/
	void DrawPrimitivesParallel(FRHICommandList& RHICmdList, const class FViewInfo& View, class FDeferredShadingSceneRenderer& Renderer, ETranslucencyPass::Type TranslucenyPassType, int32 FirstPrimIdx, int32 LastPrimIdx) const;

	/**
	* Draw a single primitive...this is used when we are rendering in parallel and we need to handlke a translucent shadow
	* @param View - current view used to draw items
	* @param PhaseSortedPrimitives - array with the primitives we want to draw
	* @param TranslucenyPassType
	* @param PrimIdx in SortedPrims[]
	*/
	void DrawAPrimitive(FRHICommandList& RHICmdList, const class FViewInfo& View, class FDeferredShadingSceneRenderer& Renderer, ETranslucencyPass::Type TranslucenyPassType, int32 PrimIdx) const;

	/** 
	* Draw all the primitives in this set for the mobile pipeline. 
	* @param bRenderSeparateTranslucency - If false, only primitives with materials without mobile separate translucency enabled are rendered. Opposite if true.
	*/
	void DrawPrimitivesForMobile(FRHICommandListImmediate& RHICmdList, const class FViewInfo& View, const bool bRenderSeparateTranslucency) const;

	/**
	* Insert a primitive to the translucency rendering list[s]
	*/
	
	static void PlaceScenePrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& ViewInfo, bool bUseNormalTranslucency, bool bUseSeparateTranslucency, bool bUseMobileSeparateTranslucency,
		FTranslucentSortedPrim* InArrayStart, int32& InOutArrayNum, FTranslucenyPrimCount& OutCount);

	/**
	* Sort any primitives that were added to the set back-to-front
	*/
	void SortPrimitives();

	/** 
	* @return number of prims to render
	*/
	int32 NumPrims() const
	{
		return SortedPrims.Num();
	}

	/**
	* Adds primitives originally created with PlaceScenePrimitive
	*/
	void AppendScenePrimitives(FTranslucentSortedPrim* Elements, int32 Num, const FTranslucenyPrimCount& TranslucentPrimitiveCountPerPass);

	// belongs to SortedPrims
	FTranslucenyPrimCount SortedPrimsNum;

private:

	/** sortkey compare class */
	struct FCompareFTranslucentSortedPrim
	{
		FORCEINLINE bool operator()( const FTranslucentSortedPrim& A, const FTranslucentSortedPrim& B ) const
		{
			// If priorities are equal sort normally from back to front
			// otherwise lower sort priorities should render first
			return ( A.SortOrder == B.SortOrder ) ? ( B.SortKey < A.SortKey ) : ( A.SortOrder < B.SortOrder );
		}
	};

	/** list of translucent primitives, sorted after calling Sort() */
	TArray<FTranslucentSortedPrim,SceneRenderingAllocator> SortedPrims;


	/** Renders a single primitive for the deferred shading pipeline. */
	void RenderPrimitive(FRHICommandList& RHICmdList, const FViewInfo& View, FPrimitiveSceneInfo* PrimitiveSceneInfo, const FPrimitiveViewRelevance& ViewRelevance, const FProjectedShadowInfo* TranslucentSelfShadow, ETranslucencyPass::Type TranslucenyPassType) const;
};

template <> struct TIsPODType<FTranslucentPrimSet::FTranslucentSortedPrim> { enum { Value = true }; };

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

DECLARE_STATS_GROUP(TEXT("Parallel Command List Markers"), STATGROUP_ParallelCommandListMarkers, STATCAT_Advanced);

class FParallelCommandListSet
{
public:
	const FViewInfo& View;
	FRHICommandListImmediate& ParentCmdList;
	FSceneRenderTargets* Snapshot;
	TStatId	ExecuteStat;
	int32 Width;
	int32 NumAlloc;
	int32 MinDrawsPerCommandList;
	// see r.RHICmdBalanceParallelLists
	bool bBalanceCommands;
	// see r.RHICmdSpewParallelListBalance
	bool bSpewBalance;
	bool bBalanceCommandsWithLastFrame;
public:
	TArray<FRHICommandList*,SceneRenderingAllocator> CommandLists;
	TArray<FGraphEventRef,SceneRenderingAllocator> Events;
	// number of draws in this commandlist if known, -1 if not known. Overestimates are better than nothing.
	TArray<int32,SceneRenderingAllocator> NumDrawsIfKnown;
protected:
	//this must be called by deriving classes virtual destructor because it calls the virtual SetStateOnCommandList.
	//C++ will not do dynamic dispatch of virtual calls from destructors so we can't call it in the base class.
	void Dispatch(bool bHighPriority = false);
	FRHICommandList* AllocCommandList();
	bool bParallelExecute;
	bool bCreateSceneContext;
public:
	FParallelCommandListSet(TStatId InExecuteStat, const FViewInfo& InView, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext);
	virtual ~FParallelCommandListSet();
	int32 NumParallelCommandLists() const
	{
		return CommandLists.Num();
	}
	FRHICommandList* NewParallelCommandList();
	FORCEINLINE FGraphEventArray* GetPrereqs()
	{
		return nullptr;
	}
	void AddParallelCommandList(FRHICommandList* CmdList, FGraphEventRef& CompletionEvent, int32 InNumDrawsIfKnown = -1);	

	virtual void SetStateOnCommandList(FRHICommandList& CmdList)
	{

	}
	static void WaitForTasks();
private:
	void WaitForTasksInternal();
};

class FVolumeUpdateRegion
{
public:
	/** World space bounds. */
	FBox Bounds;

	/** Number of texels in each dimension to update. */
	FIntVector CellsSize;
};

class FGlobalDistanceFieldClipmap
{
public:
	/** World space bounds. */
	FBox Bounds;

	/** Offset applied to UVs so that only new or dirty areas of the volume texture have to be updated. */
	FVector ScrollOffset;

	/** Regions in the volume texture to update. */
	TArray<FVolumeUpdateRegion, TInlineAllocator<3> > UpdateRegions;

	/** Volume texture for this clipmap. */
	TRefCountPtr<IPooledRenderTarget> RenderTarget;
};

class FGlobalDistanceFieldInfo
{
public:

	bool bInitialized;
	TArray<FGlobalDistanceFieldClipmap> Clipmaps;
	FGlobalDistanceFieldParameterData ParameterData;

	void UpdateParameterData(float MaxOcclusionDistance);

	FGlobalDistanceFieldInfo() :
		bInitialized(false)
	{}
};

BEGIN_UNIFORM_BUFFER_STRUCT_WITH_CONSTRUCTOR(FForwardGlobalLightData,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,NumLocalLights)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,HasDirectionalLight)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FIntVector,CulledGridSize)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,MaxCulledLightsPerCell)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,LightGridPixelSizeShift)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,LightGridZParams)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,DirectionalLightDirection)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,DirectionalLightColor)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,DirectionalLightShadowMapChannelMask)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D,DirectionalLightDistanceFadeMAD)
END_UNIFORM_BUFFER_STRUCT(FForwardGlobalLightData)

class FForwardLightingViewResources
{
public:
	TUniformBufferRef<FForwardGlobalLightData> ForwardGlobalLightData;
	FDynamicReadBuffer ForwardLocalLightBuffer;
	FRWBuffer NumCulledLightsGrid;
	FRWBuffer CulledLightDataGrid;
	FRWBuffer NextCulledLightLink;
	FRWBuffer StartOffsetGrid;
	FRWBuffer CulledLightLinks;
	FRWBuffer NextCulledLightData;

	void Release()
	{
		ForwardGlobalLightData.SafeRelease();
		ForwardLocalLightBuffer.Release();
		NumCulledLightsGrid.Release();
		CulledLightDataGrid.Release();
		NextCulledLightLink.Release();
		StartOffsetGrid.Release();
		CulledLightLinks.Release();
		NextCulledLightData.Release();
	}
};

/** A FSceneView with additional state used by the scene renderer. */
class FViewInfo : public FSceneView
{
public:

	/** 
	 * The view's state, or NULL if no state exists.
	 * This should be used internally to the renderer module to avoid having to cast View.State to an FSceneViewState*
	 */
	FSceneViewState* ViewState;

	/** Cached view uniform shader parameters, to allow recreating the view uniform buffer without having to fill out the entire struct. */
	TScopedPointer<FViewUniformShaderParameters> CachedViewUniformShaderParameters;

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

	/** A map from static mesh ID to a boolean dithered LOD fade out value. */
	FSceneBitArray StaticMeshFadeOutDitheredLODMap;

	/** A map from static mesh ID to a boolean dithered LOD fade in value. */
	FSceneBitArray StaticMeshFadeInDitheredLODMap;

#if WITH_EDITOR
	/** A map from static mesh ID to editor selection visibility (whether or not it is selected AND should be drawn).  */
	FSceneBitArray StaticMeshEditorSelectionMap;
#endif

	/** An array of batch element visibility masks, valid only for meshes
	 set visible in either StaticMeshVisibilityMap or StaticMeshShadowDepthMap. */
	TArray<uint64,SceneRenderingAllocator> StaticMeshBatchVisibility;

	/** The dynamic primitives visible in this view. */
	TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleDynamicPrimitives;

	/** The dynamic editor primitives visible in this view. */
	TArray<const FPrimitiveSceneInfo*,SceneRenderingAllocator> VisibleEditorPrimitives;

	/** List of visible primitives with dirty precomputed lighting buffers */
	TArray<FPrimitiveSceneInfo*,SceneRenderingAllocator> DirtyPrecomputedLightingBufferPrimitives;

	/** View dependent global distance field clipmap info. */
	FGlobalDistanceFieldInfo GlobalDistanceFieldInfo;

	/** Set of translucent prims for this view */
	FTranslucentPrimSet TranslucentPrimSet;

	/** Set of distortion prims for this view */
	FDistortionPrimSet DistortionPrimSet;
	
	/** Set of mesh decal prims for this view */
	FMeshDecalPrimSet MeshDecalPrimSet;
	
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

	// [PrimitiveIndex] = end index index in DynamicMeshElements[], to support GetDynamicMeshElementRange()
	TArray<uint32,SceneRenderingAllocator> DynamicMeshEndIndices;

	TArray<FMeshBatchAndRelevance,SceneRenderingAllocator> DynamicEditorMeshElements;

	FSimpleElementCollector SimpleElementCollector;

	FSimpleElementCollector EditorSimpleElementCollector;

	// Used by mobile renderer to determine whether static meshes will be rendered with CSM shaders or not.
	FMobileCSMVisibilityInfo MobileCSMVisibilityInfo;

	/** Parameters for exponential height fog. */
	FVector4 ExponentialFogParameters;
	FVector ExponentialFogColor;
	float FogMaxOpacity;
	FVector2D ExponentialFogParameters3;

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
	/** Whether the view has any materials that use the global distance field. */
	uint32 bUsesGlobalDistanceField : 1;
	uint32 bUsesLightingChannels : 1;
	uint32 bTranslucentSurfaceLighting : 1;
	/** 
	 * true if the scene has at least one decal. Used to disable stencil operations in the mobile base pass when the scene has no decals.
	 * TODO: Right now decal visibility is computed right before rendering them. Ideally it should be done in InitViews and this flag should be replaced with list of visible decals  
	 */
	uint32 bSceneHasDecals : 1;
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

	/** Points to the view state's resources if a view state exists. */
	FForwardLightingViewResources* ForwardLightingResources;

	/** Used when there is no view state, buffers reallocate every frame. */
	FForwardLightingViewResources ForwardLightingResourcesStorage;

	// Size of the HZB's mipmap 0
	// NOTE: the mipmap 0 is downsampled version of the depth buffer
	FIntPoint HZBMipmap0Size;

	/** Used by occlusion for percent unoccluded calculations. */
	float OneOverNumPossiblePixels;

	// Mobile gets one light-shaft, this light-shaft.
	FVector4 LightShaftCenter; 
	FLinearColor LightShaftColorMask;
	FLinearColor LightShaftColorApply;
	bool bLightShaftUse;

	FHeightfieldLightingViewInfo HeightfieldLightingViewInfo;

	TShaderMap<FGlobalShaderType>* ShaderMap;

	bool bIsSnapshot;

	// Optional stencil dithering optimization during prepasses
	bool bAllowStencilDither;

	/** Custom visibility query for view */
	ICustomVisibilityQuery* CustomVisibilityQuery;

	TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator> IndirectShadowPrimitives;

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

	/** Creates ViewUniformShaderParameters given a set of view transforms. */
	void SetupUniformBufferParameters(
		FSceneRenderTargets& SceneContext,
		const FMatrix& EffectiveTranslatedViewMatrix, 
		const FMatrix& EffectiveViewToTranslatedWorld, 
		FBox* OutTranslucentCascadeBoundsArray, 
		int32 NumTranslucentCascades,
		FViewUniformShaderParameters& ViewUniformShaderParameters) const;

	void SetupViewRectUniformBufferParameters(
		FIntPoint BufferSize,
		FIntRect EffectiveViewRect,
		FViewUniformShaderParameters& ViewUniformShaderParameters) const;

	void SetupDefaultGlobalDistanceFieldUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters) const;
	void SetupGlobalDistanceFieldUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters) const;

	/** Initializes the RHI resources used by this view. */
	void InitRHIResources();

	/** Determines distance culling and fades if the state changes */
	bool IsDistanceCulled(float DistanceSquared, float MaxDrawDistance, float MinDrawDistance, const FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** Gets the eye adaptation render target for this view. Same as GetEyeAdaptationRT */
	IPooledRenderTarget* GetEyeAdaptation(FRHICommandList& RHICmdList) const;

	/** Gets one of two eye adaptation render target for this view.
	* NB: will return null in the case that the internal view state pointer
	* (for the left eye in the stereo case) is null.
	*/
	IPooledRenderTarget* GetEyeAdaptationRT(FRHICommandList& RHICmdList) const;
	IPooledRenderTarget* GetLastEyeAdaptationRT(FRHICommandList& RHICmdList) const;

	/**Swap the order of the two eye adaptation targets in the double buffer system */
	void SwapEyeAdaptationRTs() const;

	/** Tells if the eyeadaptation texture exists without attempting to allocate it. */
	bool HasValidEyeAdaptation() const;

	/** Informs sceneinfo that eyedaptation has queued commands to compute it at least once */
	void SetValidEyeAdaptation() const;

	/** Instanced stereo only needs to render the left eye. */
	bool ShouldRenderView() const 
	{
		if (!bIsInstancedStereoEnabled)
		{
			return true;
		}
		else if (StereoPass != eSSP_RIGHT_EYE)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	FORCEINLINE_DEBUGGABLE FMeshDrawingRenderState GetDitheredLODTransitionState(const FStaticMesh& Mesh, const bool bAllowStencil = false) const
	{
		FMeshDrawingRenderState DrawRenderState(EDitheredLODState::None, bAllowStencil);

		if (Mesh.bDitheredLODTransition)
		{
			if (StaticMeshFadeOutDitheredLODMap[Mesh.Id])
			{
				if (bAllowStencil)
				{
					DrawRenderState.DitheredLODState = EDitheredLODState::FadeOut;
				}
				else
				{
					DrawRenderState.DitheredLODTransitionAlpha = GetTemporalLODTransition();
				}
			}
			else if (StaticMeshFadeInDitheredLODMap[Mesh.Id])
			{
				if (bAllowStencil)
				{
					DrawRenderState.DitheredLODState = EDitheredLODState::FadeIn;
			}
				else
				{
					DrawRenderState.DitheredLODTransitionAlpha = GetTemporalLODTransition() - 1.0f;
		}
			}
		}

		return DrawRenderState;
	}

	inline FVector GetPrevViewDirection() const { return PrevViewMatrices.ViewMatrix.GetColumn(2); }

	/** Create a snapshot of this view info on the scene allocator. */
	FViewInfo* CreateSnapshot() const;

	/** Destroy all snapshots before we wipe the scene allocator. */
	static void DestroyAllSnapshots();
	
	// Get the range in DynamicMeshElements[] for a given PrimitiveIndex
	// @return range (start is inclusive, end is exclusive)
	FInt32Range GetDynamicMeshElementRange(uint32 PrimitiveIndex) const
	{
		// inclusive
		int32 Start = (PrimitiveIndex == 0) ? 0 : DynamicMeshEndIndices[PrimitiveIndex - 1];
		// exclusive
		int32 AfterEnd = DynamicMeshEndIndices[PrimitiveIndex];
		
		return FInt32Range(Start, AfterEnd);
	}

private:

	FSceneViewState* GetEffectiveViewState() const;

	/** Initialization that is common to the constructors. */
	void Init();

	/** Calculates bounding boxes for the translucency lighting volume cascades. */
	void CalcTranslucencyLightingVolumeBounds(FBox* InOutCascadeBoundsArray, int32 NumCascades) const;

	/** Sets the sky SH irradiance map coefficients. */
	void SetupSkyIrradianceEnvironmentMapConstants(FVector4* OutSkyIrradianceEnvironmentMap) const;
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

class FShadowMapRenderTargetsRefCounted
{
public:
	TArray<TRefCountPtr<IPooledRenderTarget>, SceneRenderingAllocator> ColorTargets;
	TRefCountPtr<IPooledRenderTarget> DepthTarget;

	bool IsValid() const
	{
		if (DepthTarget)
		{
			return true;
		}
		else 
		{
			return ColorTargets.Num() > 0;
		}
	}

	FIntPoint GetSize() const
	{
		const FPooledRenderTargetDesc* Desc = NULL;

		if (DepthTarget)
		{
			Desc = &DepthTarget->GetDesc();
		}
		else 
		{
			check(ColorTargets.Num() > 0);
			Desc = &ColorTargets[0]->GetDesc();
		}

		if (Desc->IsCubemap())
		{
			return FIntPoint(Desc->Extent.X, Desc->Extent.X);
		}
		else
		{
			return Desc->Extent;
		}
	}

	int64 ComputeMemorySize() const
	{
		int64 MemorySize = 0;

		for (int32 i = 0; i < ColorTargets.Num(); i++)
		{
			MemorySize += ColorTargets[i]->ComputeMemorySize();
		}

		if (DepthTarget)
		{
			MemorySize += DepthTarget->ComputeMemorySize();
		}

		return MemorySize;
	}

	void Release()
	{
		for (int32 i = 0; i < ColorTargets.Num(); i++)
		{
			ColorTargets[i] = NULL;
		}

		ColorTargets.Empty();

		DepthTarget = NULL;
	}
};

struct FSortedShadowMapAtlas
{
	FShadowMapRenderTargetsRefCounted RenderTargets;
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
};

struct FSortedShadowMaps
{
	/** Visible shadows sorted by their shadow depth map render target. */
	TArray<FSortedShadowMapAtlas,SceneRenderingAllocator> ShadowMapAtlases;

	TArray<FSortedShadowMapAtlas,SceneRenderingAllocator> RSMAtlases;

	TArray<FSortedShadowMapAtlas,SceneRenderingAllocator> ShadowMapCubemaps;

	FSortedShadowMapAtlas PreshadowCache;

	TArray<FSortedShadowMapAtlas,SceneRenderingAllocator> TranslucencyShadowMapAtlases;

	void Release();

	int64 ComputeMemorySize() const
	{
		int64 MemorySize = 0;

		for (int i = 0; i < ShadowMapAtlases.Num(); i++)
		{
			MemorySize += ShadowMapAtlases[i].RenderTargets.ComputeMemorySize();
		}

		for (int i = 0; i < RSMAtlases.Num(); i++)
		{
			MemorySize += RSMAtlases[i].RenderTargets.ComputeMemorySize();
		}

		for (int i = 0; i < ShadowMapCubemaps.Num(); i++)
		{
			MemorySize += ShadowMapCubemaps[i].RenderTargets.ComputeMemorySize();
		}

		MemorySize += PreshadowCache.RenderTargets.ComputeMemorySize();

		for (int i = 0; i < TranslucencyShadowMapAtlases.Num(); i++)
		{
			MemorySize += TranslucencyShadowMapAtlases[i].RenderTargets.ComputeMemorySize();
		}

		return MemorySize;
	}
};

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

	FSortedShadowMaps SortedShadowsForShadowDepthPass;

	/** If a freeze request has been made */
	bool bHasRequestedToggleFreeze;

	/** True if precomputed visibility was used when rendering the scene. */
	bool bUsedPrecomputedVisibility;

	/** Lights added if wholescenepointlight shadow would have been rendered (ignoring r.SupportPointLightWholeSceneShadows). Used for warning about unsupported features. */	
	TArray<FName, SceneRenderingAllocator> UsedWholeScenePointLightNames;

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

	/** the last thing we do with a scene renderer, lots of cleanup related to the threading **/
	static void WaitForTasksClearSnapshotsAndDeleteSceneRenderer(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer);

protected:

	// Shared functionality between all scene renderers

	void InitDynamicShadows(FRHICommandListImmediate& RHICmdList);

	bool RenderShadowProjections(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bProjectingForForwardShading, bool bMobileModulatedProjections);

	/** Finds a matching cached preshadow, if one exists. */
	TRefCountPtr<FProjectedShadowInfo> GetCachedPreshadow(
		const FLightPrimitiveInteraction* InParentInteraction,
		const FProjectedShadowInitializer& Initializer,
		const FBoxSphereBounds& Bounds,
		uint32 InResolutionX);

	/** Creates a per object projected shadow for the given interaction. */
	void CreatePerObjectProjectedShadow(
		FRHICommandListImmediate& RHICmdList,
		FLightPrimitiveInteraction* Interaction,
		bool bCreateTranslucentObjectShadow,
		bool bCreateInsetObjectShadow,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& OutPreShadows);

	/** Creates shadows for the given interaction. */
	void SetupInteractionShadows(
		FRHICommandListImmediate& RHICmdList,
		FLightPrimitiveInteraction* Interaction,
		FVisibleLightInfo& VisibleLightInfo,
		bool bStaticSceneOnly,
		const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& PreShadows);

	/** Generates FProjectedShadowInfos for all wholesceneshadows on the given light.*/
	void AddViewDependentWholeSceneShadowsForView(
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfos, 
		TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& ShadowInfosThatNeedCulling, 
		FVisibleLightInfo& VisibleLightInfo, 
		FLightSceneInfo& LightSceneInfo);

	void AllocateShadowDepthTargets(FRHICommandListImmediate& RHICmdList);
	
	void AllocatePerObjectShadowDepthTargets(FRHICommandListImmediate& RHICmdList, TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& Shadows);

	void AllocateCachedSpotlightShadowDepthTargets(FRHICommandListImmediate& RHICmdList, TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& CachedShadows);

	void AllocateCSMDepthTargets(FRHICommandListImmediate& RHICmdList, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& WholeSceneDirectionalShadows);

	void AllocateRSMDepthTargets(FRHICommandListImmediate& RHICmdList, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& RSMShadows);

	void AllocateOnePassPointLightDepthTargets(FRHICommandListImmediate& RHICmdList, const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& WholeScenePointShadows);

	void AllocateTranslucentShadowDepthTargets(FRHICommandListImmediate& RHICmdList, TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& TranslucentShadows);

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

	void RenderShadowDepthMaps(FRHICommandListImmediate& RHICmdList);
	void RenderShadowDepthMapAtlases(FRHICommandListImmediate& RHICmdList);

	/**
	* Creates a projected shadow for all primitives affected by a light.
	* @param LightSceneInfo - The light to create a shadow for.
	*/
	void CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo);

	/** Updates the preshadow cache, allocating new preshadows that can fit and evicting old ones. */
	void UpdatePreshadowCache(FSceneRenderTargets& SceneContext);

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
	void PostVisibilityFrameSetup(FILCUpdatePrimTaskData& OutILCTaskData);

	void GatherDynamicMeshElements(
		TArray<FViewInfo>& InViews, 
		const FScene* InScene, 
		const FSceneViewFamily& InViewFamily, 
		const FPrimitiveViewMasks& HasDynamicMeshElementsMasks, 
		const FPrimitiveViewMasks& HasDynamicEditorMeshElementsMasks, 
		FMeshElementCollector& Collector);

	/** Initialized the fog constants for each view. */
	void InitFogConstants();

	/** Returns whether there are translucent primitives to be rendered. */
	bool ShouldRenderTranslucency() const;

	/** TODO: REMOVE if no longer needed: Copies scene color to the viewport's render target after applying gamma correction. */
	void GammaCorrectToViewportRenderTarget(FRHICommandList& RHICmdList, const FViewInfo* View, float OverrideGamma);

	/** Updates state for the end of the frame. */
	void RenderFinish(FRHICommandListImmediate& RHICmdList);

	void RenderCustomDepthPassAtLocation(FRHICommandListImmediate& RHICmdList, int32 Location);
	void RenderCustomDepthPass(FRHICommandListImmediate& RHICmdList);

	void OnStartFrame();

	/** Renders the scene's distortion */
	void RenderDistortion(FRHICommandListImmediate& RHICmdList);
	void RenderDistortionES2(FRHICommandListImmediate& RHICmdList);

	static int32 GetRefractionQuality(const FSceneViewFamily& ViewFamily);

	void UpdatePrimitivePrecomputedLightingBuffers();
	void ClearPrimitiveSingleFramePrecomputedLightingBuffers();

	void RenderPlanarReflection(class FPlanarReflectionSceneProxy* ReflectionSceneProxy);
};

/**
 * Renderer that implements simple forward shading and associated features.
 */
class FMobileSceneRenderer : public FSceneRenderer
{
public:

	FMobileSceneRenderer(const FSceneViewFamily* InViewFamily,FHitProxyConsumer* HitProxyConsumer);

	// FSceneRenderer interface

	virtual void Render(FRHICommandListImmediate& RHICmdList) override;

	virtual void RenderHitProxies(FRHICommandListImmediate& RHICmdList) override;

protected:
	/** Finds the visible dynamic shadows for each view. */
	void InitDynamicShadows(FRHICommandListImmediate& RHICmdList);

	/** Build visibility lists on CSM receivers and non-csm receivers. */
	void BuildCombinedStaticAndCSMVisibilityState(FLightSceneInfo* LightSceneInfo);

	void InitViews(FRHICommandListImmediate& RHICmdList);

	/** Renders the opaque base pass for mobile. */
	void RenderMobileBasePass(FRHICommandListImmediate& RHICmdList);

	/** Render modulated shadow projections in to the scene, loops over any unrendered shadows until all are processed.*/
	void RenderModulatedShadowProjections(FRHICommandListImmediate& RHICmdList);

	/** Makes a copy of scene alpha so PC can emulate ES2 framebuffer fetch. */
	void CopySceneAlpha(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);

	/** Resolves scene depth in case hardware does not support reading depth in the shader */
	void ConditionalResolveSceneDepth(FRHICommandListImmediate& RHICmdList);

	/** Renders decals. */
	void RenderDecals(FRHICommandListImmediate& RHICmdList);

	/** Renders the base pass for translucency. */
	void RenderTranslucency(FRHICommandListImmediate& RHICmdList);

	/** Perform upscaling when post process is not used. */
	void BasicPostProcess(FRHICommandListImmediate& RHICmdList, FViewInfo &View, bool bDoUpscale, bool bDoEditorPrimitives);

	/** Creates uniform buffers with the mobile directional light parameters, for each lighting channel. Called by InitViews */
	void CreateDirectionalLightUniformBuffers(FSceneView& SceneView);

private:
	bool bModulatedShadowsInUse;
};

// The noise textures need to be set in Slate too.
RENDERER_API void UpdateNoiseTextureParameters(FViewUniformShaderParameters& ViewUniformShaderParameters);

inline FTextureRHIParamRef OrBlack2DIfNull(FTextureRHIParamRef Tex)
{
	FTextureRHIParamRef Result = Tex ? Tex : GBlackTexture->TextureRHI.GetReference();
	check(Result);
	return Result;
}

inline FTextureRHIParamRef OrBlack3DIfNull(FTextureRHIParamRef Tex)
{
	// we fall back to 2D which are unbound es2 parameters
	return OrBlack2DIfNull(Tex ? Tex : GBlackVolumeTexture->TextureRHI.GetReference());
}

inline void SetBlack2DIfNull(FTextureRHIParamRef& Tex)
{
	if (!Tex)
	{
		Tex = GBlackTexture->TextureRHI.GetReference();
		check(Tex);
	}
}

inline void SetBlack3DIfNull(FTextureRHIParamRef& Tex)
{
	if (!Tex)
	{
		Tex = GBlackVolumeTexture->TextureRHI.GetReference();
		// we fall back to 2D which are unbound es2 parameters
		SetBlack2DIfNull(Tex);
	}
}