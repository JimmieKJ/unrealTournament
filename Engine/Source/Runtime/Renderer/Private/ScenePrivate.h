// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScenePrivate.h: Private scene manager definitions.
=============================================================================*/

#ifndef __SCENEPRIVATE_H__
#define __SCENEPRIVATE_H__

class SceneRenderingAllocator;
class USceneCaptureComponent;
class UTextureRenderTarget;

class SceneRenderingBitArrayAllocator
	: public TInlineAllocator<4,SceneRenderingAllocator>
{
};

class SceneRenderingSparseArrayAllocator
	: public TSparseArrayAllocator<SceneRenderingAllocator,SceneRenderingBitArrayAllocator>
{
};

class SceneRenderingSetAllocator
	: public TSetAllocator<SceneRenderingSparseArrayAllocator,TInlineAllocator<1,SceneRenderingAllocator> >
{
};

typedef TBitArray<SceneRenderingBitArrayAllocator> FSceneBitArray;
typedef TConstSetBitIterator<SceneRenderingBitArrayAllocator> FSceneSetBitIterator;
typedef TConstDualSetBitIterator<SceneRenderingBitArrayAllocator,SceneRenderingBitArrayAllocator> FSceneDualSetBitIterator;

// Forward declarations.
class FScene;
class FLightPropagationVolume;

/** True if HDR is enabled for the mobile renderer. */
bool IsMobileHDR();

/** True if the mobile renderer is emulating HDR in a 32bpp render target. */
bool IsMobileHDR32bpp();

// Dependencies.
#include "StaticBoundShaderState.h"
#include "BatchedElements.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GenericOctree.h"
#include "SceneCore.h"
#include "PrimitiveSceneInfo.h"
#include "LightSceneInfo.h"
#include "ShaderBaseClasses.h"
#include "DrawingPolicy.h"
#include "DepthRendering.h"
#include "SceneHitProxyRendering.h"
#include "ShaderComplexityRendering.h"
#include "ShadowRendering.h"
#include "SceneRendering.h"
#include "StaticMeshDrawList.h"
#include "DeferredShadingRenderer.h"
#include "FogRendering.h"
#include "BasePassRendering.h"
#include "ForwardBasePassRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "TranslucentRendering.h"
#include "VelocityRendering.h"
#include "LightMapDensityRendering.h"
#include "TextureLayout.h"
#include "TextureLayout3d.h"
#include "ScopedPointer.h"
#include "ClearQuad.h"
#include "AtmosphereRendering.h"

#if WITH_SLI || PLATFORM_SHOULD_BUFFER_QUERIES
#define BUFFERED_OCCLUSION_QUERIES 1
#endif

/** Factor by which to grow occlusion tests **/
#define OCCLUSION_SLOP (1.0f)

class FOcclusionQueryHelpers
{
public:

	// get the system-wide number of frames of buffered occlusion queries.
	static int32 GetNumBufferedFrames()
	{
		int32 NumBufferedFrames = 1;

#if BUFFERED_OCCLUSION_QUERIES		
#	if WITH_SLI
		// If we're running with SLI, assume throughput is more important than latency, and buffer an extra frame
		NumBufferedFrames = GNumActiveGPUsForRendering == 1 ? 1 : GNumActiveGPUsForRendering;
#	else
		static const auto NumBufferedQueriesVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.NumBufferedOcclusionQueries"));
		NumBufferedFrames = NumBufferedQueriesVar->GetValueOnAnyThread();
#	endif		
#endif
		return NumBufferedFrames;
	}

	// get the index of the oldest query based on the current frame and number of buffered frames.
	static uint32 GetQueryLookupIndex(int32 CurrentFrame, int32 NumBufferedFrames)
	{
		// queries are currently always requested earlier in the frame than they are issued.
		// thus we can always overwrite the oldest query with the current one as we never need them
		// to coexist.  This saves us a buffer entry.
		const uint32 QueryIndex = CurrentFrame % NumBufferedFrames;
		return QueryIndex;
	}

	// get the index of the query to overwrite for new queries.
	static uint32 GetQueryIssueIndex(int32 CurrentFrame, int32 NumBufferedFrames)
	{
		// queries are currently always requested earlier in the frame than they are issued.
		// thus we can always overwrite the oldest query with the current one as we never need them
		// to coexist.  This saves us a buffer entry.
		const uint32 QueryIndex = CurrentFrame % NumBufferedFrames;
		return QueryIndex;
	}
};

/** Holds information about a single primitive's occlusion. */
class FPrimitiveOcclusionHistory
{
public:
	/** The primitive the occlusion information is about. */
	FPrimitiveComponentId PrimitiveId;

#if BUFFERED_OCCLUSION_QUERIES
	/** The occlusion query which contains the primitive's pending occlusion results. */
	TArray<FRenderQueryRHIRef, TInlineAllocator<1> > PendingOcclusionQuery;
#else
	/** The occlusion query which contains the primitive's pending occlusion results. */
	FRenderQueryRHIRef PendingOcclusionQuery;
#endif

	uint32 HZBTestIndex;
	uint32 HZBTestFrameNumber;

	/** The last time the primitive was visible. */
	float LastVisibleTime;

	/** The last time the primitive was in the view frustum. */
	float LastConsideredTime;

	/** 
	 *	The pixels that were rendered the last time the primitive was drawn.
	 *	It is the ratio of pixels unoccluded to the resolution of the scene.
	 */
	float LastPixelsPercentage;

	/** whether or not this primitive was grouped the last time it was queried */
	bool bGroupedQuery;

	/** 
	 * Number of frames to buffer the occlusion queries. 
	 * Larger numbers allow better SLI scaling but introduce latency in the results.
	 */
	int32 NumBufferedFrames;

	/** 
	 * For things that have subqueries (folaige), this is the non-zero
	 */
	int32 CustomIndex;

	/** Initialization constructor. */
	FORCEINLINE FPrimitiveOcclusionHistory(FPrimitiveComponentId InPrimitiveId, int32 SubQuery)
		: PrimitiveId(InPrimitiveId)
		, HZBTestIndex(0)
		, HZBTestFrameNumber(~0u)
		, LastVisibleTime(0.0f)
		, LastConsideredTime(0.0f)
		, LastPixelsPercentage(0.0f)
		, bGroupedQuery(false)
		, CustomIndex(SubQuery)
	{
#if BUFFERED_OCCLUSION_QUERIES
		NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames();
		PendingOcclusionQuery.Empty(NumBufferedFrames);
		PendingOcclusionQuery.AddZeroed(NumBufferedFrames);
#endif
	}

	/** Destructor. Note that the query should have been released already. */
	~FPrimitiveOcclusionHistory()
	{
//		check( !IsValidRef(PendingOcclusionQuery) );
	}

	template<class TOcclusionQueryPool> // here we use a template just to allow this to be inlined without sorting out the header order
	FORCEINLINE void ReleaseQueries(FRHICommandListImmediate& RHICmdList, TOcclusionQueryPool& Pool)
	{
#if BUFFERED_OCCLUSION_QUERIES
		for (int32 QueryIndex = 0; QueryIndex < NumBufferedFrames; QueryIndex++)
		{
			Pool.ReleaseQuery(RHICmdList, PendingOcclusionQuery[QueryIndex]);
		}
#else
		Pool.ReleaseQuery(RHICmdList, PendingOcclusionQuery);
#endif
	}

	FORCEINLINE FRenderQueryRHIRef& GetPastQuery(uint32 FrameNumber)
	{
#if BUFFERED_OCCLUSION_QUERIES
		// Get the oldest occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryLookupIndex(FrameNumber, NumBufferedFrames);
		return PendingOcclusionQuery[QueryIndex];
#else
		return PendingOcclusionQuery;
#endif
	}

	FORCEINLINE void SetCurrentQuery(uint32 FrameNumber, FRenderQueryRHIParamRef NewQuery)
	{
#if BUFFERED_OCCLUSION_QUERIES
		// Get the current occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(FrameNumber, NumBufferedFrames);
		PendingOcclusionQuery[QueryIndex] = NewQuery;
#else
		PendingOcclusionQuery = NewQuery;
#endif
	}
};

struct FPrimitiveOcclusionHistoryKey
{
	FPrimitiveComponentId PrimitiveId;
	int32 CustomIndex;

	FPrimitiveOcclusionHistoryKey(const FPrimitiveOcclusionHistory& Element)
		: PrimitiveId(Element.PrimitiveId)
		, CustomIndex(Element.CustomIndex)
	{
	}
	FPrimitiveOcclusionHistoryKey(FPrimitiveComponentId InPrimitiveId, int32 InCustomIndex)
		: PrimitiveId(InPrimitiveId)
		, CustomIndex(InCustomIndex)
	{
	}
};

/** Defines how the hash set indexes the FPrimitiveOcclusionHistory objects. */
struct FPrimitiveOcclusionHistoryKeyFuncs : BaseKeyFuncs<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKey>
{
	typedef FPrimitiveOcclusionHistoryKey KeyInitType;

	static KeyInitType GetSetKey(const FPrimitiveOcclusionHistory& Element)
	{
		return FPrimitiveOcclusionHistoryKey(Element);
	}

	static bool Matches(KeyInitType A,KeyInitType B)
	{
		return A.PrimitiveId == B.PrimitiveId && A.CustomIndex == B.CustomIndex;
	}

	static uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key.PrimitiveId.PrimIDValue) ^ (GetTypeHash(Key.CustomIndex) >> 20);
	}
};


/**
 * A pool of render (e.g. occlusion/timer) queries which are allocated individually, and returned to the pool as a group.
 */
class FRenderQueryPool
{
public:
	FRenderQueryPool(ERenderQueryType InQueryType) :QueryType(InQueryType) { }
	virtual ~FRenderQueryPool();

	/** Releases all the render queries in the pool. */
	void Release();

	/** Allocates an render query from the pool. */
	FRenderQueryRHIRef AllocateQuery();

	/** De-reference an render query, returning it to the pool instead of deleting it when the refcount reaches 0. */
	void ReleaseQuery(FRHICommandListImmediate& RHICmdList, FRenderQueryRHIRef &Query);

private:
	/** Container for available render queries. */
	TArray<FRenderQueryRHIRef> Queries;

	ERenderQueryType QueryType;
};

/**
 * Distance cull fading uniform buffer containing faded in
 */
class FGlobalDistanceCullFadeUniformBuffer : public TUniformBuffer< FDistanceCullFadeUniformShaderParameters >
{
public:
	/** Default constructor. */
	FGlobalDistanceCullFadeUniformBuffer()
	{
		FDistanceCullFadeUniformShaderParameters Uniforms;
		Uniforms.FadeTimeScaleBias.X = 0.0f;
		Uniforms.FadeTimeScaleBias.Y = 1.0f;
		SetContents( Uniforms );
	}
};

/** Global primitive uniform buffer resource containing faded in */
extern TGlobalResource< FGlobalDistanceCullFadeUniformBuffer > GDistanceCullFadedInUniformBuffer;

/**
 * Stores fading state for a single primitive in a single view
 */
class FPrimitiveFadingState
{
public:
	FPrimitiveFadingState()
		: FrameNumber(0)
		, EndTime(0.0f)
		, FadeTimeScaleBias(0.0f,0.0f)
		, bIsVisible(false)
		, bValid(false)
	{
	}

	/** Frame number when last updated */
	uint32 FrameNumber;
	
	/** Time when fade will be finished. */
	float EndTime;
	
	/** Scale and bias to use on time to calculate fade opacity */
	FVector2D FadeTimeScaleBias;

	/** The uniform buffer for the fade parameters */
	FDistanceCullFadeUniformBufferRef UniformBuffer;

	/** Currently visible? */
	bool bIsVisible;

	/** Valid? */
	bool bValid;
};

/** Maps a single primitive to it's per-view fading state data */
typedef TMap<FPrimitiveComponentId, FPrimitiveFadingState> FPrimitiveFadingStateMap;

class FOcclusionRandomStream
{
	enum {NumSamples = 3571};
public:

	/** Default constructor - should set seed prior to use. */
	FOcclusionRandomStream()
		: CurrentSample(0)
	{
		FRandomStream RandomStream(0x83246);
		for (int32 Index = 0; Index < NumSamples; Index++)
		{
			Samples[Index] = RandomStream.GetFraction();
		}
		Samples[0] = 0.0f; // we want to make sure we have at least a few zeros
		Samples[NumSamples/3] = 0.0f; // we want to make sure we have at least a few zeros
		Samples[(NumSamples*2)/3] = 0.0f; // we want to make sure we have at least a few zeros
	}

	/** @return A random number between 0 and 1. */
	FORCEINLINE float GetFraction()
	{
		if (CurrentSample >= NumSamples)
		{
			CurrentSample = 0;
		}
		return Samples[CurrentSample++];
	}
private:

	/** Index of the last sample we produced **/
	uint32 CurrentSample;
	/** A list of float random samples **/
	float Samples[NumSamples];
};

/** Random table for occlusion **/
extern FOcclusionRandomStream GOcclusionRandomStream;

/**
 * The scene manager's private implementation of persistent view state.
 * This class is associated with a particular camera across multiple frames by the game thread.
 * The game thread calls FRendererModule::AllocateViewState to create an instance of this private implementation.
 */
class FSceneViewState : public FSceneViewStateInterface, public FDeferredCleanupInterface, public FRenderResource
{
public:

	class FProjectedShadowKey
	{
	public:

		FORCEINLINE bool operator == (const FProjectedShadowKey &Other) const
		{
			return (PrimitiveId == Other.PrimitiveId && Light == Other.Light && ShadowSplitIndex == Other.ShadowSplitIndex && bTranslucentShadow == Other.bTranslucentShadow);
		}

		FProjectedShadowKey(const FProjectedShadowInfo& ProjectedShadowInfo)
			: PrimitiveId(ProjectedShadowInfo.GetParentSceneInfo() ? ProjectedShadowInfo.GetParentSceneInfo()->PrimitiveComponentId : FPrimitiveComponentId())
			, Light(ProjectedShadowInfo.GetLightSceneInfo().Proxy->GetLightComponent())
			, ShadowSplitIndex(ProjectedShadowInfo.CascadeSettings.ShadowSplitIndex)
			, bTranslucentShadow(ProjectedShadowInfo.bTranslucentShadow)
		{
		}

		FProjectedShadowKey(FPrimitiveComponentId InPrimitiveId, const ULightComponent* InLight, int32 InSplitIndex, bool bInTranslucentShadow)
			: PrimitiveId(InPrimitiveId)
			, Light(InLight)
			, ShadowSplitIndex(InSplitIndex)
			, bTranslucentShadow(bInTranslucentShadow)
		{
		}

		friend FORCEINLINE uint32 GetTypeHash(const FSceneViewState::FProjectedShadowKey& Key)
		{
			return PointerHash(Key.Light,GetTypeHash(Key.PrimitiveId));
		}

	private:
		FPrimitiveComponentId PrimitiveId;
		const ULightComponent* Light;
		int32 ShadowSplitIndex;
		bool bTranslucentShadow;
	};

	int32 NumBufferedFrames;
	uint32 UniqueID;
	typedef TMap<FSceneViewState::FProjectedShadowKey, FRenderQueryRHIRef> ShadowKeyOcclusionQueryMap;
#if BUFFERED_OCCLUSION_QUERIES
	TArray<ShadowKeyOcclusionQueryMap> ShadowOcclusionQueryMaps;
#else
	ShadowKeyOcclusionQueryMap ShadowOcclusionQueryMap;
#endif

	/** The view's occlusion query pool. */
	FRenderQueryPool OcclusionQueryPool;

	FHZBOcclusionTester HZBOcclusionTests;

	/** Storage to which compressed visibility chunks are uncompressed at runtime. */
	TArray<uint8> DecompressedVisibilityChunk;

	/** Cached visibility data from the last call to GetPrecomputedVisibilityData. */
	const TArray<uint8>* CachedVisibilityChunk;
	int32 CachedVisibilityHandlerId;
	int32 CachedVisibilityBucketIndex;
	int32 CachedVisibilityChunkIndex;

	/** Parameter to keep track of previous frame. Managed by the rendering thread. */
	FViewMatrices PrevViewMatrices;
	FViewMatrices PendingPrevViewMatrices;

	uint32		PendingPrevFrameNumber;
	uint32		PrevFrameNumber;
	float		LastRenderTime;
	float		LastRenderTimeDelta;
	float		MotionBlurTimeScale;
	FMatrix		PrevViewMatrixForOcclusionQuery;
	FVector		PrevViewOriginForOcclusionQuery;

	// A counter incremented once each time this view is rendered.
	uint32 OcclusionFrameCounter;

	/** Used by states that have IsViewParent() == true to store primitives for child states. */
	TSet<FPrimitiveComponentId> ParentPrimitives;

	/** For this view, the set of primitives that are currently fading, either in or out. */
	FPrimitiveFadingStateMap PrimitiveFadingStates;

	FIndirectLightingCacheAllocation* TranslucencyLightingCacheAllocations[TVC_MAX];

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Are we currently in the state of freezing rendering? (1 frame where we gather what was rendered) */
	bool bIsFreezing;

	/** Is rendering currently frozen? */
	bool bIsFrozen;

	/** The set of primitives that were rendered the frame that we froze rendering */
	TSet<FPrimitiveComponentId> FrozenPrimitives;
#endif

private:
	// to implement eye adaptation changes over time
	TRefCountPtr<IPooledRenderTarget> EyeAdaptationRT;

	// used by the Postprocess Material Blending system to avoid recreation and garbage collection of MIDs
	TArray<UMaterialInstanceDynamic*> MIDPool;
	uint32 MIDUsedCount;

	// if TemporalAA is on this cycles through 0..TemporalAASampleCount-1
	uint8 TemporalAASampleIndex;
	// >= 1, 1 means there is no TemporalAA
	uint8 TemporalAASampleCount;

	// can be 0
	FLightPropagationVolume* LightPropagationVolume;

public:

	FHeightfieldLightingAtlas* HeightfieldLightingAtlas;

	// If Translucency should be rendered into a separate RT and composited without DepthOfField, can be disabled in the materials (affects sorting)
	TRefCountPtr<IPooledRenderTarget> SeparateTranslucencyRT;
	// Temporal AA result of last frame
	TRefCountPtr<IPooledRenderTarget> TemporalAAHistoryRT;
	// Temporal AA result for DOF of last frame
	TRefCountPtr<IPooledRenderTarget> DOFHistoryRT;
	TRefCountPtr<IPooledRenderTarget> DOFHistoryRT2;
	// Temporal AA result for SSR
	TRefCountPtr<IPooledRenderTarget> SSRHistoryRT;
	// Temporal AA result for light shafts of last frame
	TRefCountPtr<IPooledRenderTarget> LightShaftOcclusionHistoryRT;
	// Temporal AA result for light shafts of last frame
	TMap<const ULightComponent*, TRefCountPtr<IPooledRenderTarget> > LightShaftBloomHistoryRTs;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldAOHistoryRT;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldIrradianceHistoryRT;
	// Mobile temporal AA surfaces.
	TRefCountPtr<IPooledRenderTarget> MobileAaBloomSunVignette0;
	TRefCountPtr<IPooledRenderTarget> MobileAaBloomSunVignette1;
	TRefCountPtr<IPooledRenderTarget> MobileAaColor0;
	TRefCountPtr<IPooledRenderTarget> MobileAaColor1;

	// cache for selection outline to a avoid reallocations of the SRV, Key is to detect if the object has changed
	FTextureRHIRef SelectionOutlineCacheKey;
	TRefCountPtr<FRHIShaderResourceView> SelectionOutlineCacheValue;

	/** Distance field AO tile intersection GPU resources.  Last frame's state is not used, but they must be sized exactly to the view so stored here. */
	class FTileIntersectionResources* AOTileIntersectionResources;

	// Is DOFHistoryRT set from Bokeh DOF?
	bool bBokehDOFHistory;
	bool bBokehDOFHistory2;

	FTemporalLODState TemporalLODState;

	// call after SetupTemporalAA()
	uint32 GetCurrentTemporalAASampleIndex() const
	{
		return TemporalAASampleIndex;
	}

	// call after SetupTemporalAA()
	uint32 GetCurrentTemporalAASampleCount() const
	{
		return TemporalAASampleCount;
	}

	// @param SampleCount 0 or 1 for no TemporalAA 
	void SetupTemporalAA(uint32 SampleCount, const FSceneViewFamily& Family)
	{
		if(!SampleCount)
		{
			SampleCount = 1;
		}

		TemporalAASampleCount = FMath::Min(SampleCount, (uint32)255);
		
		if (!Family.bWorldIsPaused)
		{
			TemporalAASampleIndex++;
		}

		if(TemporalAASampleIndex >= TemporalAASampleCount)
		{
			TemporalAASampleIndex = 0;
		}
	}

	void FreeSeparateTranslucency()
	{
		SeparateTranslucencyRT.SafeRelease();

		check(!SeparateTranslucencyRT);
	}

	// call only if not yet created
	void CreateLightPropagationVolumeIfNeeded(ERHIFeatureLevel::Type InFeatureLevel);

	// @return can return 0 (if globally disabled)
	FLightPropagationVolume* GetLightPropagationVolume() const { return LightPropagationVolume; }

	/** Default constructor. */
	FSceneViewState();

	void DestroyLightPropagationVolume();

	virtual ~FSceneViewState();

	// called every frame after the view state was updated
	void UpdateLastRenderTime(const FSceneViewFamily& Family)
	{
		// The editor can trigger multiple update calls within a frame
		if(Family.CurrentRealTime != LastRenderTime)
		{
			LastRenderTimeDelta = Family.CurrentRealTime - LastRenderTime;
			LastRenderTime = Family.CurrentRealTime;
		}
	}

	void TrimHistoryRenderTargets(const FScene* Scene);

	/** 
	 * Called every frame after UpdateLastRenderTime, sets up the information for the lagged temporal LOD transition
	 */
	void UpdateTemporalLODTransition(const FViewInfo& View)
	{
		TemporalLODState.UpdateTemporalLODTransition(View, LastRenderTime);
	}

	/** 
	 * Returns an array of visibility data for the given view, or NULL if none exists. 
	 * The data bits are indexed by VisibilityId of each primitive in the scene.
	 * This method decompresses data if necessary and caches it based on the bucket and chunk index in the view state.
	 */
	const uint8* GetPrecomputedVisibilityData(FViewInfo& View, const FScene* Scene);

	/**
	 * Cleans out old entries from the primitive occlusion history, and resets unused pending occlusion queries.
	 * @param MinHistoryTime - The occlusion history for any primitives which have been visible and unoccluded since
	 *							this time will be kept.  The occlusion history for any primitives which haven't been
	 *							visible and unoccluded since this time will be discarded.
	 * @param MinQueryTime - The pending occlusion queries older than this will be discarded.
	 */
	void TrimOcclusionHistory(FRHICommandListImmediate& RHICmdList, float MinHistoryTime, float MinQueryTime, int32 FrameNumber);

	/**
	 * Checks whether a shadow is occluded this frame.
	 * @param Primitive - The shadow subject.
	 * @param Light - The shadow source.
	 */
	bool IsShadowOccluded(FRHICommandListImmediate& RHICmdList, FPrimitiveComponentId PrimitiveId, const ULightComponent* Light, int32 SplitIndex, bool bTranslucentShadow) const;

	TRefCountPtr<IPooledRenderTarget>& GetEyeAdaptation()
	{
		// Create the texture needed for EyeAdaptation
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(1, 1), PF_R32_FLOAT, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(Desc, EyeAdaptationRT, TEXT("EyeAdaptation"));

		return EyeAdaptationRT;
	}

	TRefCountPtr<IPooledRenderTarget>& GetSeparateTranslucency(const FViewInfo& View)
	{
		// Create the SeparateTranslucency render target (alpha is needed to lerping)
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(GSceneRenderTargets.GetBufferSizeXY(), PF_FloatRGBA, TexCreate_None, TexCreate_RenderTargetable, false));
		GRenderTargetPool.FindFreeElement(Desc, SeparateTranslucencyRT, TEXT("SeparateTranslucency"));

		return SeparateTranslucencyRT;
	}

	// FRenderResource interface.
	virtual void InitDynamicRHI() override
	{
		HZBOcclusionTests.InitDynamicRHI();
	}

	virtual void ReleaseDynamicRHI() override
	{
#if BUFFERED_OCCLUSION_QUERIES
		for (int i = 0; i < ShadowOcclusionQueryMaps.Num(); ++i)
		{
			ShadowOcclusionQueryMaps[i].Reset();
		}
#else
		ShadowOcclusionQueryMap.Reset();
#endif
		PrimitiveOcclusionHistorySet.Empty();
		PrimitiveFadingStates.Empty();
		OcclusionQueryPool.Release();
		HZBOcclusionTests.ReleaseDynamicRHI();
		EyeAdaptationRT.SafeRelease();
		SeparateTranslucencyRT.SafeRelease();
		TemporalAAHistoryRT.SafeRelease();
		DOFHistoryRT.SafeRelease();
		DOFHistoryRT2.SafeRelease();
		SSRHistoryRT.SafeRelease();
		LightShaftOcclusionHistoryRT.SafeRelease();
		LightShaftBloomHistoryRTs.Empty();
		DistanceFieldAOHistoryRT.SafeRelease();
		DistanceFieldIrradianceHistoryRT.SafeRelease();
		MobileAaBloomSunVignette0.SafeRelease();
		MobileAaBloomSunVignette1.SafeRelease();
		MobileAaColor0.SafeRelease();
		MobileAaColor1.SafeRelease();
		SelectionOutlineCacheKey.SafeRelease();
		SelectionOutlineCacheValue.SafeRelease();
	}

	// FSceneViewStateInterface
	RENDERER_API virtual void Destroy() override;
	
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		uint32 Count = MIDPool.Num();

		for(uint32 i = 0; i < Count; ++i)
		{
			UMaterialInstanceDynamic* MID = MIDPool[i];

			Collector.AddReferencedObject(MID);
		}
	}

	/** called in InitViews() */
	virtual void OnStartFrame(FSceneView& CurrentView) override
	{
		check(IsInRenderingThread());

		if(!(CurrentView.FinalPostProcessSettings.IndirectLightingColor * CurrentView.FinalPostProcessSettings.IndirectLightingIntensity).IsAlmostBlack())
		{
			CreateLightPropagationVolumeIfNeeded(CurrentView.GetFeatureLevel());
		}
	}

	// needed for GetReusableMID()
	virtual void OnStartPostProcessing(FSceneView& CurrentView) override
	{
		check(IsInGameThread());
		
		// Needs to be done once for all viewstates.  If multiple FSceneViews are sharing the same ViewState, this will cause problems.
		// Sharing should be illegal right now though.
		MIDUsedCount = 0;
	}

	// Note: OnStartPostProcessing() needs to be called each frame for each view
	virtual UMaterialInstanceDynamic* GetReusableMID(class UMaterialInterface* InParentMaterial) override
	{		
		check(IsInGameThread());
		check(InParentMaterial);

		auto ParentAsMaterialInstance = Cast<UMaterialInstanceDynamic>(InParentMaterial);

		// fixup MID parents as this is not allowed, take the next MIC or Material.
		UMaterialInterface* ParentMaterial = ParentAsMaterialInstance ? ParentAsMaterialInstance->Parent : InParentMaterial;

		// this is not allowed and would cause an error later in the code
		check(!ParentMaterial->IsA(UMaterialInstanceDynamic::StaticClass()));

		if(MIDUsedCount < (uint32)MIDPool.Num())
		{
			UMaterialInstanceDynamic* MID = MIDPool[MIDUsedCount];

			if(MID->Parent != ParentMaterial)
			{
				// create a new one
				// garbage collector will remove the old one
				// this should not happen too often
				MID = UMaterialInstanceDynamic::Create(ParentMaterial, 0);
				MIDPool[MIDUsedCount] = MID;
			}
		}
		else
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMaterial, 0);
			check(MID);

			MIDPool.Add(MID);
		}

		UMaterialInstanceDynamic* Ret = MIDPool[MIDUsedCount++];

		check(Ret->GetRenderProxy(false));

		return Ret;
	}

	virtual FTemporalLODState& GetTemporalLODState() override
	{
		return TemporalLODState;
	}
	virtual const FTemporalLODState& GetTemporalLODState() const override
	{
		return TemporalLODState;
	}
	float GetTemporalLODTransition() const override
	{
		return TemporalLODState.GetTemporalLODTransition(LastRenderTime);
	}
	uint32 GetViewKey() const override
	{
		return UniqueID;
	}



	// FDeferredCleanupInterface
	virtual void FinishCleanup() override
	{
		delete this;
	}

	virtual SIZE_T GetSizeBytes() const override;


	/** Information about visibility/occlusion states in past frames for individual primitives. */
	TSet<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKeyFuncs> PrimitiveOcclusionHistorySet;
};

/** Rendering resource class that manages a cubemap array for reflections. */
class FReflectionEnvironmentCubemapArray : public FRenderResource
{
public:

	FReflectionEnvironmentCubemapArray(ERHIFeatureLevel::Type InFeatureLevel) : 
		FRenderResource(InFeatureLevel)
	,	MaxCubemaps(0)
	{}

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	/** 
	 * Updates the maximum number of cubemaps that this array is allocated for.
	 * This reallocates the resource but does not copy over the old contents. 
	 */
	void UpdateMaxCubemaps(uint32 InMaxCubemaps);
	int32 GetMaxCubemaps() const { return MaxCubemaps; }
	bool IsValid() const { return IsValidRef(ReflectionEnvs); }
	FSceneRenderTargetItem& GetRenderTarget() const { return ReflectionEnvs->GetRenderTargetItem(); }

protected:
	uint32 MaxCubemaps;
	TRefCountPtr<IPooledRenderTarget> ReflectionEnvs;

	void ReleaseCubeArray();
};

/** Per-component reflection capture state that needs to persist through a re-register. */
class FCaptureComponentSceneState
{
public:
	/** Index of the cubemap in the array for this capture component. */
	int32 CaptureIndex;

	FCaptureComponentSceneState(int32 InCaptureIndex) :
		CaptureIndex(InCaptureIndex)
	{}

	bool operator==(const FCaptureComponentSceneState& Other) const 
	{
		return CaptureIndex == Other.CaptureIndex;
	}
};

/** Scene state used to manage the reflection environment feature. */
class FReflectionEnvironmentSceneData
{
public:
	
	/** 
	 * Set to true for one frame whenever RegisteredReflectionCaptures or the transforms of any registered reflection proxy has changed,
	 * Which allows one frame to update cached proxy associations.
	 */
	bool bRegisteredReflectionCapturesHasChanged;

	/** The rendering thread's list of visible reflection captures in the scene. */
	TArray<FReflectionCaptureProxy*> RegisteredReflectionCaptures;
	TArray<FVector> RegisteredReflectionCapturePositions;

	/** 
	 * Cubemap array resource which contains the captured scene for each reflection capture.
	 * This is indexed by the value of AllocatedReflectionCaptureState.CaptureIndex.
	 */
	FReflectionEnvironmentCubemapArray CubemapArray;

	/** Rendering thread map from component to scene state.  This allows storage of RT state that needs to persist through a component re-register. */
	TMap<const UReflectionCaptureComponent*, FCaptureComponentSceneState> AllocatedReflectionCaptureState;

	/** 
	 * Game thread list of reflection components that have been allocated in the cubemap array. 
	 * These are not necessarily all visible or being rendered, but their scene state is stored in the cubemap array.
	 */
	TSparseArray<UReflectionCaptureComponent*> AllocatedReflectionCapturesGameThread;

	/** Game thread tracking of what size this scene has allocated for the cubemap array. */
	int32 MaxAllocatedReflectionCubemapsGameThread;

	FReflectionEnvironmentSceneData(ERHIFeatureLevel::Type InFeatureLevel) :
		bRegisteredReflectionCapturesHasChanged(true),
		CubemapArray(InFeatureLevel),
		MaxAllocatedReflectionCubemapsGameThread(0)
	{}
};

class FPrimitiveAndInstance
{
public:

	FPrimitiveAndInstance(FPrimitiveSceneInfo* InPrimitive, int32 InInstanceIndex) :
		Primitive(InPrimitive),
		InstanceIndex(InInstanceIndex)
	{}

	FPrimitiveSceneInfo* Primitive;
	int32 InstanceIndex;
};

class FPrimitiveSurfelFreeEntry
{
public:
	FPrimitiveSurfelFreeEntry(int32 InOffset, int32 InNumSurfels) :
		Offset(InOffset),
		NumSurfels(InNumSurfels)
	{}

	FPrimitiveSurfelFreeEntry() :
		Offset(0),
		NumSurfels(0)
	{}

	int32 Offset;
	int32 NumSurfels;
};

class FPrimitiveSurfelAllocation
{
public:
	FPrimitiveSurfelAllocation(int32 InOffset, int32 InNumLOD0, int32 InNumSurfels, int32 InNumInstances) :
		Offset(InOffset),
		NumLOD0(InNumLOD0),
		NumSurfels(InNumSurfels),
		NumInstances(InNumInstances)
	{}

	FPrimitiveSurfelAllocation() :
		Offset(0),
		NumLOD0(0),
		NumSurfels(0),
		NumInstances(1)
	{}

	int32 GetTotalNumSurfels() const
	{
		return NumSurfels * NumInstances;
	}

	int32 Offset;
	int32 NumLOD0;
	int32 NumSurfels;
	int32 NumInstances;
};

class FPrimitiveRemoveInfo
{
public:
	FPrimitiveRemoveInfo(const FPrimitiveSceneInfo* InPrimitive) :
		Primitive(InPrimitive),
		DistanceFieldInstanceIndices(Primitive->DistanceFieldInstanceIndices)
	{}

	/** 
	 * Must not be dereferenced after creation, the primitive was removed from the scene and deleted
	 * Value of the pointer is still useful for map lookups
	 */
	const FPrimitiveSceneInfo* Primitive;

	TArray<int32, TInlineAllocator<1>> DistanceFieldInstanceIndices;
};

class FSurfelBufferAllocator
{
public:

	FSurfelBufferAllocator() : NumSurfelsInBuffer(0) {}

	int32 GetNumSurfelsInBuffer() const { return NumSurfelsInBuffer; }
	void AddPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo, int32 PrimitiveLOD0Surfels, int32 PrimitiveNumSurfels, int32 NumInstances);
	void RemovePrimitive(const FPrimitiveSceneInfo* Primitive);

	const FPrimitiveSurfelAllocation* FindAllocation(const FPrimitiveSceneInfo* Primitive)
	{
		return Allocations.Find(Primitive);
	}

private:

	int32 NumSurfelsInBuffer;
	TMap<const FPrimitiveSceneInfo*, FPrimitiveSurfelAllocation> Allocations;
	TArray<FPrimitiveSurfelFreeEntry> FreeList;
};

/** Scene data used to manage distance field object buffers on the GPU. */
class FDistanceFieldSceneData
{
public:

	FDistanceFieldSceneData(EShaderPlatform ShaderPlatform);
	~FDistanceFieldSceneData();

	void AddPrimitive(FPrimitiveSceneInfo* InPrimitive);
	void UpdatePrimitive(FPrimitiveSceneInfo* InPrimitive);
	void RemovePrimitive(FPrimitiveSceneInfo* InPrimitive);
	void Release();
	void VerifyIntegrity();

	bool HasPendingOperations() const
	{
		return PendingAddOperations.Num() > 0 || PendingUpdateOperations.Num() > 0 || PendingRemoveOperations.Num() > 0;
	}

	bool HasPendingRemovePrimitive(const FPrimitiveSceneInfo* Primitive) const
	{
		for (int32 RemoveIndex = 0; RemoveIndex < PendingRemoveOperations.Num(); RemoveIndex++)
		{
			if (PendingRemoveOperations[RemoveIndex].Primitive == Primitive)
			{
				return true;
			}
		}

		return false;
	}

	int32 NumObjectsInBuffer;
	class FDistanceFieldObjectBuffers* ObjectBuffers;

	/** Stores the primitive and instance index of every entry in the object buffer. */
	TArray<FPrimitiveAndInstance> PrimitiveInstanceMapping;
	TArray<const FPrimitiveSceneInfo*> HeightfieldPrimitives;

	class FSurfelBuffers* SurfelBuffers;
	FSurfelBufferAllocator SurfelAllocations;

	class FInstancedSurfelBuffers* InstancedSurfelBuffers;
	FSurfelBufferAllocator InstancedSurfelAllocations;

	/** Pending operations on the object buffers to be processed next frame. */
	TArray<FPrimitiveSceneInfo*> PendingAddOperations;
	TSet<FPrimitiveSceneInfo*> PendingUpdateOperations;
	TArray<FPrimitiveRemoveInfo> PendingRemoveOperations;

	/** Used to detect atlas reallocations, since objects store UVs into the atlas and need to be updated when it changes. */
	int32 AtlasGeneration;

	bool bTrackPrimitives;
};

/** Stores data for an allocation in the FIndirectLightingCache. */
class FIndirectLightingCacheBlock
{
public:

	FIndirectLightingCacheBlock() :
		MinTexel(FIntVector(0, 0, 0)),
		TexelSize(0),
		Min(FVector(0, 0, 0)),
		Size(FVector(0, 0, 0)),
		bHasEverBeenUpdated(false)
	{}

	FIntVector MinTexel;
	int32 TexelSize;
	FVector Min;
	FVector Size;
	bool bHasEverBeenUpdated;
};

/** Stores information about an indirect lighting cache block to be updated. */
class FBlockUpdateInfo
{
public:

	FBlockUpdateInfo(const FIndirectLightingCacheBlock& InBlock, FIndirectLightingCacheAllocation* InAllocation) :
		Block(InBlock),
		Allocation(InAllocation)
	{}

	FIndirectLightingCacheBlock Block;
	FIndirectLightingCacheAllocation* Allocation;
};

/** 
 * Implements a volume texture atlas for caching indirect lighting on a per-object basis.
 * The indirect lighting is interpolated from Lightmass SH volume lighting samples.
 */
class FIndirectLightingCache : public FRenderResource
{
public:

	/** true for the editor case where we want a better preview for object that have no valid lightmaps */
	FIndirectLightingCache(ERHIFeatureLevel::Type InFeatureLevel);

	// FRenderResource interface
	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

	/** Allocates a block in the volume texture atlas for a primitive. */
	FIndirectLightingCacheAllocation* AllocatePrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bUnbuiltPreview);

	/** Releases the indirect lighting allocation for the given primitive. */
	void ReleasePrimitive(FPrimitiveComponentId PrimitiveId);

	FIndirectLightingCacheAllocation* FindPrimitiveAllocation(FPrimitiveComponentId PrimitiveId);

	/** Updates indirect lighting in the cache based on visibility. */
	void UpdateCache(FScene* Scene, FSceneRenderer& Renderer, bool bAllowUnbuiltPreview);

	/** Force all primitive allocations to be re-interpolated. */
	void SetLightingCacheDirty();

	// Accessors
	FSceneRenderTargetItem& GetTexture0() { return Texture0->GetRenderTargetItem(); }
	FSceneRenderTargetItem& GetTexture1() { return Texture1->GetRenderTargetItem(); }
	FSceneRenderTargetItem& GetTexture2() { return Texture2->GetRenderTargetItem(); }

private:

	/** Internal helper which adds an entry to the update lists for this allocation, if needed (due to movement, etc). */
	void UpdateCacheAllocation(
		const FBoxSphereBounds& Bounds, 
		int32 BlockSize,
		bool bPointSample,
		FIndirectLightingCacheAllocation*& Allocation, 
		TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate,
		TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate);

	/** 
	 * Creates a new allocation if needed, caches the result in PrimitiveSceneInfo->IndirectLightingCacheAllocation, 
	 * And adds an entry to the update lists when an update is needed. 
	 */
	void UpdateCachePrimitive(
		FScene* Scene, 
		FPrimitiveSceneInfo* PrimitiveSceneInfo,
		bool bAllowUnbuiltPreview, 
		bool bOpaqueRelevance, 
		TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate, 
		TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate);

	/** Updates the contents of the volume texture blocks in BlocksToUpdate. */
	void UpdateBlocks(FScene* Scene, FViewInfo* DebugDrawingView, TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate);

	/** Updates any outstanding transitions with a new delta time. */
	void UpdateTransitionsOverTime(const TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate, float DeltaWorldTime) const;

	/** Creates an allocation to be used outside the indirect lighting cache and a block to be used internally. */
	FIndirectLightingCacheAllocation* CreateAllocation(int32 BlockSize, const FBoxSphereBounds& Bounds, bool bPointSample);

	/** Block accessors. */
	FIndirectLightingCacheBlock& FindBlock(FIntVector TexelMin);
	const FIndirectLightingCacheBlock& FindBlock(FIntVector TexelMin) const;

	/** Block operations. */
	void DeallocateBlock(FIntVector Min, int32 Size);
	bool AllocateBlock(int32 Size, FIntVector& OutMin);

	/**
	 * Updates an allocation block in the cache, by re-interpolating values and uploading to the cache volume texture.
	 * @param DebugDrawingView can be 0
	 */
	void UpdateBlock(FScene* Scene, FViewInfo* DebugDrawingView, FBlockUpdateInfo& Block);

	/** Interpolates a single SH sample from all levels. */
	void InterpolatePoint(
		FScene* Scene, 
		const FIndirectLightingCacheBlock& Block,
		float& OutDirectionalShadowing, 
		FSHVectorRGB2& OutIncidentRadiance,
		FVector& OutSkyBentNormal);

	/** Interpolates SH samples for a block from all levels. */
	void InterpolateBlock(
		FScene* Scene, 
		const FIndirectLightingCacheBlock& Block, 
		TArray<float>& AccumulatedWeight, 
		TArray<FSHVectorRGB2>& AccumulatedIncidentRadiance,
		TArray<FVector>& AccumulatedSkyBentNormal);

	/** 
	 * Normalizes, adjusts for SH ringing, and encodes SH samples into a texture format.
	 * @param DebugDrawingView can be 0
	 */
	void EncodeBlock(
		FViewInfo* DebugDrawingView,
		const FIndirectLightingCacheBlock& Block, 
		const TArray<float>& AccumulatedWeight, 
		const TArray<FSHVectorRGB2>& AccumulatedIncidentRadiance,
		const TArray<FVector>& AccumulatedSkyBentNormal,
		TArray<FFloat16Color>& Texture0Data,
		TArray<FFloat16Color>& Texture1Data,
		TArray<FFloat16Color>& Texture2Data,
		FSHVectorRGB2& SingleSample,
		FVector& SkyBentNormal);

	/** Helper that calculates an effective world position min and size given a bounds. */
	void CalculateBlockPositionAndSize(const FBoxSphereBounds& Bounds, int32 TexelSize, FVector& OutMin, FVector& OutSize) const;

	/** Helper that calculates a scale and add to convert world space position into volume texture UVs for a given block. */
	void CalculateBlockScaleAndAdd(FIntVector InTexelMin, int32 AllocationTexelSize, FVector InMin, FVector InSize, FVector& OutScale, FVector& OutAdd, FVector& OutMinUV, FVector& OutMaxUV) const;

	/** true: next rendering we update all entries no matter if they are visible to avoid further hitches */
	bool bUpdateAllCacheEntries;

	/** Size of the volume texture cache. */
	int32 CacheSize;

	/** Volume textures that store SH indirect lighting, interpolated from Lightmass volume samples. */
	TRefCountPtr<IPooledRenderTarget> Texture0;
	TRefCountPtr<IPooledRenderTarget> Texture1;
	TRefCountPtr<IPooledRenderTarget> Texture2;

	/** Tracks the allocation state of the atlas. */
	TMap<FIntVector, FIndirectLightingCacheBlock> VolumeBlocks;

	/** Tracks used sections of the volume texture atlas. */
	FTextureLayout3d BlockAllocator;

	/** Tracks primitive allocations by component, so that they persist across re-registers. */
	TMap<FPrimitiveComponentId, FIndirectLightingCacheAllocation*> PrimitiveAllocations;
};

/**
 * Bounding information used to cull primitives in the scene.
 */
struct FPrimitiveBounds
{
	/** Origin of the primitive. */
	FVector Origin;
	/** Radius of the bounding sphere. */
	float SphereRadius;
	/** Extents of the axis-aligned bounding box. */
	FVector BoxExtent;
	/** Square of the minimum draw distance for the primitive. */
	float MinDrawDistanceSq;
	/** Maximum draw distance for the primitive. */
	float MaxDrawDistance;
};

/**
 * Precomputed primitive visibility ID.
 */
struct FPrimitiveVisibilityId
{
	/** Index in to the byte where precomputed occlusion data is stored. */
	int32 ByteIndex;
	/** Mast of the bit where precomputed occlusion data is stored. */
	uint8 BitMask;
};

/**
 * Flags that affect how primitives are occlusion culled.
 */
namespace EOcclusionFlags
{
	enum Type
	{
		/** No flags. */
		None = 0x0,
		/** Indicates the primitive can be occluded. */
		CanBeOccluded = 0x1,
		/** Allow the primitive to be batched with others to determine occlusion. */
		AllowApproximateOcclusion = 0x4,
		/** Indicates the primitive has a valid ID for precomputed visibility. */
		HasPrecomputedVisibility = 0x8,
		/** Indicates the primitive has a valid ID for precomputed visibility. */
		HasSubprimitiveQueries = 0x10,
	};
};

/** Information about the primitives that are attached together. */
class FAttachmentGroupSceneInfo
{
public:

	/** The parent primitive, which is the root of the attachment tree. */
	FPrimitiveSceneInfo* ParentSceneInfo;

	/** The primitives in the attachment group. */
	TArray<FPrimitiveSceneInfo*> Primitives;

	FAttachmentGroupSceneInfo() :
		ParentSceneInfo(nullptr)
	{}
};

class FLODSceneTree
{
public:
	FLODSceneTree(FScene* InScene)
		: Scene(InScene)
		, UpdateCount(0)
	{}

	/** Information about the primitives that are attached together. */
	struct FLODSceneNode
	{
		/** The primitive. */
		FPrimitiveSceneInfo* SceneInfo;

		/** Children scene infos. */
		TArray<FPrimitiveSceneInfo*> ChildrenSceneInfos;

		/** Last updated FrameCount */
		int32 LatestUpdateCount;

		FLODSceneNode() :
			SceneInfo(nullptr), 
			LatestUpdateCount(INDEX_NONE)
		{}

		void AddChild(FPrimitiveSceneInfo * NewChild)
		{
			if(NewChild && !ChildrenSceneInfos.Contains(NewChild))
			{
				ChildrenSceneInfos.Add(NewChild);
			}
		}

		void RemoveChild(FPrimitiveSceneInfo * ChildToDelete)
		{
			if(ChildToDelete && ChildrenSceneInfos.Contains(ChildToDelete))
			{
				ChildrenSceneInfos.Remove(ChildToDelete);
			}
		}
	};

	void AddChildNode(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* ChildSceneInfo);
	void RemoveChildNode(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* ChildSceneInfo);

	void UpdateNodeSceneInfo(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* SceneInfo);
	void PopulateHiddenFlags(FViewInfo& View, FSceneBitArray& HiddenFlags);

	bool IsActive() { return SceneNodes.Num() > 0; }

private:
	/** Scene this Tree belong to */
	FScene* Scene;

	/** The LOd groups in the scene.  The map key is the current primitive who has children. */
	TMap<FPrimitiveComponentId, FLODSceneNode> SceneNodes;

	/**  Update Count. This is used to skip Child node that has been updated */
	int32 UpdateCount;

	/** Populate Hidden Flags to the children **/
	void PopulateHiddenFlagsToChildren(FSceneBitArray& HiddenFlags, FLODSceneNode& Node);
};

typedef TMap<FMaterial*, FMaterialShaderMap*> FMaterialsToUpdateMap;

/** 
 * Renderer scene which is private to the renderer module.
 * Ordinarily this is the renderer version of a UWorld, but an FScene can be created for previewing in editors which don't have a UWorld as well.
 * The scene stores renderer state that is independent of any view or frame, with the primary actions being adding and removing of primitives and lights.
 */
class FScene : public FSceneInterface
{
public:

	/** An optional world associated with the scene. */
	UWorld* World;

	/** An optional FX system associated with the scene. */
	class FFXSystemInterface* FXSystem;

	enum EBasePassDrawListType
	{
		EBasePass_Default=0,
		EBasePass_Masked,
		EBasePass_MAX
	};

	// various static draw lists for this DPG

	/** position-only opaque depth draw list */
	TStaticMeshDrawList<FPositionOnlyDepthDrawingPolicy> PositionOnlyDepthDrawList;
	/** opaque depth draw list */
	TStaticMeshDrawList<FDepthDrawingPolicy> DepthDrawList;
	/** masked depth draw list */
	TStaticMeshDrawList<FDepthDrawingPolicy> MaskedDepthDrawList;
	/** Base pass draw list - no light map */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FNoLightMapPolicy> > BasePassNoLightMapDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassDrawingPolicy<FCachedVolumeIndirectLightingPolicy> > BasePassCachedVolumeIndirectLightingDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassDrawingPolicy<FCachedPointIndirectLightingPolicy> > BasePassCachedPointIndirectLightingDrawList[EBasePass_MAX];
	/** Base pass draw list - no light map */
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSimpleDynamicLightingPolicy> > BasePassSimpleDynamicLightingDrawList[EBasePass_MAX];
	/** Base pass draw list - HQ light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy< TLightMapPolicy<HQ_LIGHTMAP> > > BasePassHighQualityLightMapDrawList[EBasePass_MAX];
	/** Base pass draw list - HQ light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy< TDistanceFieldShadowsAndLightMapPolicy<HQ_LIGHTMAP> > > BasePassDistanceFieldShadowMapLightMapDrawList[EBasePass_MAX];
	/** Base pass draw list - LQ light maps */
	TStaticMeshDrawList<TBasePassDrawingPolicy< TLightMapPolicy<LQ_LIGHTMAP> > > BasePassLowQualityLightMapDrawList[EBasePass_MAX];
	/** Base pass draw list - self shadowed translucency*/
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSelfShadowedTranslucencyPolicy> > BasePassSelfShadowedTranslucencyDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassDrawingPolicy<FSelfShadowedCachedPointIndirectLightingPolicy> > BasePassSelfShadowedCachedPointIndirectTranslucencyDrawList[EBasePass_MAX];

	/** hit proxy draw list (includes both opaque and translucent objects) */
	TStaticMeshDrawList<FHitProxyDrawingPolicy> HitProxyDrawList;

	/** hit proxy draw list, with only opaque objects */
	TStaticMeshDrawList<FHitProxyDrawingPolicy> HitProxyDrawList_OpaqueOnly;

	/** draw list for motion blur velocities */
	TStaticMeshDrawList<FVelocityDrawingPolicy> VelocityDrawList;

	/** Draw list used for rendering whole scene shadow depths. */
	TStaticMeshDrawList<FShadowDepthDrawingPolicy<false> > WholeSceneShadowDepthDrawList;

	/** Draw list used for rendering whole scene reflective shadow maps.  */
	TStaticMeshDrawList<FShadowDepthDrawingPolicy<true> > WholeSceneReflectiveShadowMapDrawList;

	/** Maps a light-map type to the appropriate base pass draw list. */
	template<typename LightMapPolicyType>
	TStaticMeshDrawList<TBasePassDrawingPolicy<LightMapPolicyType> >& GetBasePassDrawList(EBasePassDrawListType DrawType);

	/** Forward shading base pass draw lists */
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<FNoLightMapPolicy> > BasePassForForwardShadingNoLightMapDrawList[EBasePass_MAX];
	TStaticMeshDrawList< TBasePassForForwardShadingDrawingPolicy< TLightMapPolicy<LQ_LIGHTMAP> > >							BasePassForForwardShadingLowQualityLightMapDrawList[EBasePass_MAX];
	TStaticMeshDrawList< TBasePassForForwardShadingDrawingPolicy< TDistanceFieldShadowsAndLightMapPolicy<LQ_LIGHTMAP> > >	BasePassForForwardShadingDistanceFieldShadowMapLightMapDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy< FSimpleDirectionalLightAndSHIndirectPolicy > >				BasePassForForwardShadingDirectionalLightAndSHIndirectDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy< FSimpleDirectionalLightAndSHDirectionalIndirectPolicy > >		BasePassForForwardShadingDirectionalLightAndSHDirectionalIndirectDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy< FSimpleDirectionalLightAndSHDirectionalCSMIndirectPolicy > >	BasePassForForwardShadingDirectionalLightAndSHDirectionalCSMIndirectDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy< FMovableDirectionalLightLightingPolicy > >					BasePassForForwardShadingMovableDirectionalLightDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy< FMovableDirectionalLightCSMLightingPolicy > >				BasePassForForwardShadingMovableDirectionalLightCSMDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy< FMovableDirectionalLightWithLightmapLightingPolicy> >		BasePassForForwardShadingMovableDirectionalLightLightmapDrawList[EBasePass_MAX];
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy< FMovableDirectionalLightCSMWithLightmapLightingPolicy> >	BasePassForForwardShadingMovableDirectionalLightCSMLightmapDrawList[EBasePass_MAX];

	/** Maps a light-map type to the appropriate base pass draw list. */
	template<typename LightMapPolicyType>
	TStaticMeshDrawList<TBasePassForForwardShadingDrawingPolicy<LightMapPolicyType> >& GetForwardShadingBasePassDrawList(EBasePassDrawListType DrawType);

	/**
	 * The following arrays are densely packed primitive data needed by various
	 * rendering passes. PrimitiveSceneInfo->PackedIndex maintains the index
	 * where data is stored in these arrays for a given primitive.
	 */

	/** Packed array of primitives in the scene. */
	TArray<FPrimitiveSceneInfo*> Primitives;
	/** Packed array of primitive bounds. */
	TArray<FPrimitiveBounds> PrimitiveBounds;
	/** Packed array of precomputed primitive visibility IDs. */
	TArray<FPrimitiveVisibilityId> PrimitiveVisibilityIds;
	/** Packed array of primitive occlusion flags. See EOcclusionFlags. */
	TArray<uint8> PrimitiveOcclusionFlags;
	/** Packed array of primitive occlusion bounds. */
	TArray<FBoxSphereBounds> PrimitiveOcclusionBounds;
	/** Packed array of primitive components associated with the primitive. */
	TArray<FPrimitiveComponentId> PrimitiveComponentIds;

	/** The lights in the scene. */
	TSparseArray<FLightSceneInfoCompact> Lights;

	/** 
	 * Lights in the scene which are invisible, but still needed by the editor for previewing. 
	 * Lights in this array cannot be in the Lights array.  They also are not fully set up, as AddLightSceneInfo_RenderThread is not called for them.
	 */
	TSparseArray<FLightSceneInfoCompact> InvisibleLights;

	/** The mobile quality level for which static draw lists have been built. */
	bool bStaticDrawListsMobileHDR;
	bool bStaticDrawListsMobileHDR32bpp;

	/** Whether the early Z pass was force enabled when static draw lists were built. */
	int32 StaticDrawListsEarlyZPassMode;

	/** True if a change to SkyLight / Lighting has occurred that requires static draw lists to be updated. */
	bool bScenesPrimitivesNeedStaticMeshElementUpdate;	

	/** The scene's sky light, if any. */
	FSkyLightSceneProxy* SkyLight;

	/** Used to track the order that skylights were enabled in. */
	TArray<FSkyLightSceneProxy*> SkyLightStack;

	/** The directional light to use for simple dynamic lighting, if any. */
	FLightSceneInfo* SimpleDirectionalLight;

	/** The sun light for atmospheric effect, if any. */
	FLightSceneInfo* SunLight;

	/** The decals in the scene. */
	TSparseArray<FDeferredDecalProxy*> Decals;

	/** State needed for the reflection environment feature. */
	FReflectionEnvironmentSceneData ReflectionSceneData;

	/** 
	 * Precomputed lighting volumes in the scene, used for interpolating dynamic object lighting.
	 * These are typically one per streaming level and they store volume lighting samples computed by Lightmass. 
	 */
	TArray<const FPrecomputedLightVolume*> PrecomputedLightVolumes;

	/** Interpolates and caches indirect lighting for dynamic objects. */
	FIndirectLightingCache IndirectLightingCache;

	/** Scene state of distance field AO.  NULL if the scene doesn't use the feature. */
	class FSurfaceCacheResources* SurfaceCacheResources;

	/** Distance field object scene data. */
	FDistanceFieldSceneData DistanceFieldSceneData;

	/** Preshadows that are currently cached in the PreshadowCache render target. */
	TArray<TRefCountPtr<FProjectedShadowInfo> > CachedPreshadows;

	/** Texture layout that tracks current allocations in the PreshadowCache render target. */
	FTextureLayout PreshadowCacheLayout;

	/** The static meshes in the scene. */
	TSparseArray<FStaticMesh*> StaticMeshes;

	/** The exponential fog components in the scene. */
	TArray<FExponentialHeightFogSceneInfo> ExponentialFogs;

	/** The atmospheric fog components in the scene. */
	FAtmosphericFogSceneInfo* AtmosphericFog;

	/** The wind sources in the scene. */
	TArray<class FWindSourceSceneProxy*> WindSources;

	/** SpeedTree wind objects in the scene. FLocalVertexFactoryShaderParameters needs to lookup by FVertexFactory, but wind objects are per tree (i.e. per UStaticMesh)*/
	TMap<const UStaticMesh*, struct FSpeedTreeWindComputation*> SpeedTreeWindComputationMap;
	TMap<FVertexFactory*, const UStaticMesh*> SpeedTreeVertexFactoryMap;

	/** The attachment groups in the scene.  The map key is the attachment group's root primitive. */
	TMap<FPrimitiveComponentId,FAttachmentGroupSceneInfo> AttachmentGroups;

	/** Precomputed visibility data for the scene. */
	const FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler;

	/** An octree containing the shadow casting lights in the scene. */
	FSceneLightOctree LightOctree;

	/** An octree containing the primitives in the scene. */
	FScenePrimitiveOctree PrimitiveOctree;

	/** Indicates whether this scene requires hit proxy rendering. */
	bool bRequiresHitProxies;

	/** Whether this is an editor scene. */
	bool bIsEditorScene;

	/** Set by the rendering thread to signal to the game thread that the scene needs a static lighting build. */
	volatile mutable int32 NumUncachedStaticLightingInteractions;

	FLinearColor UpperDynamicSkylightColor;
	FLinearColor LowerDynamicSkylightColor;

	FMotionBlurInfoData MotionBlurInfoData;

	/** Uniform buffers for parameter collections with the corresponding Ids. */
	TMap<FGuid, FUniformBufferRHIRef> ParameterCollections;

	/** LOD Tree Holder for massive LOD system */
	FLODSceneTree SceneLODHierarchy;

	/** Initialization constructor. */
	FScene(UWorld* InWorld, bool bInRequiresHitProxies,bool bInIsEditorScene, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel);

	virtual ~FScene();

	// FSceneInterface interface.
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) override;
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) override;
	virtual void ReleasePrimitive(UPrimitiveComponent* Primitive) override;
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) override;
	virtual void UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive) override;
	virtual void AddLight(ULightComponent* Light) override;
	virtual void RemoveLight(ULightComponent* Light) override;
	virtual void AddInvisibleLight(ULightComponent* Light) override;
	virtual void SetSkyLight(FSkyLightSceneProxy* Light) override;
	virtual void DisableSkyLight(FSkyLightSceneProxy* Light) override;
	virtual void AddDecal(UDecalComponent* Component) override;
	virtual void RemoveDecal(UDecalComponent* Component) override;
	virtual void UpdateDecalTransform(UDecalComponent* Decal) override;
	virtual void AddReflectionCapture(UReflectionCaptureComponent* Component) override;
	virtual void RemoveReflectionCapture(UReflectionCaptureComponent* Component) override;
	virtual void GetReflectionCaptureData(UReflectionCaptureComponent* Component, class FReflectionCaptureFullHDRDerivedData& OutDerivedData) override;
	virtual void UpdateReflectionCaptureTransform(UReflectionCaptureComponent* Component) override;
	virtual void ReleaseReflectionCubemap(UReflectionCaptureComponent* CaptureComponent) override;
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponent2D* CaptureComponent) override;
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponentCube* CaptureComponent) override;
	virtual void AllocateReflectionCaptures(const TArray<UReflectionCaptureComponent*>& NewCaptures) override;
	virtual void UpdateSkyCaptureContents(const USkyLightComponent* CaptureComponent, bool bCaptureEmissiveOnly, FTexture* OutProcessedTexture, FSHVectorRGB3& OutIrradianceEnvironmentMap) override;
	virtual void AddPrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) override;
	virtual void RemovePrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) override;
	virtual void UpdateLightTransform(ULightComponent* Light) override;
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light) override;
	virtual void UpdateDynamicSkyLight(const FLinearColor& UpperColor, const FLinearColor& LowerColor) override;
	virtual void AddExponentialHeightFog(UExponentialHeightFogComponent* FogComponent) override;
	virtual void RemoveExponentialHeightFog(UExponentialHeightFogComponent* FogComponent) override;
	virtual void AddAtmosphericFog(UAtmosphericFogComponent* FogComponent) override;
	virtual void RemoveAtmosphericFog(UAtmosphericFogComponent* FogComponent) override;
	virtual void RemoveAtmosphericFogResource_RenderThread(FRenderResource* FogResource) override;
	virtual FAtmosphericFogSceneInfo* GetAtmosphericFogSceneInfo() override { return AtmosphericFog; }
	virtual void AddWindSource(UWindDirectionalSourceComponent* WindComponent) override;
	virtual void RemoveWindSource(UWindDirectionalSourceComponent* WindComponent) override;
	virtual const TArray<FWindSourceSceneProxy*>& GetWindSources_RenderThread() const override;
	virtual void GetWindParameters(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override;
	virtual void GetDirectionalWindParameters(FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override;
	virtual void AddSpeedTreeWind(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh) override;
	virtual void RemoveSpeedTreeWind(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh) override;
	virtual void RemoveSpeedTreeWind_RenderThread(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh) override;
	virtual void UpdateSpeedTreeWind(double CurrentTime) override;
	virtual FUniformBufferRHIParamRef GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory) override;
	virtual void DumpUnbuiltLightIteractions( FOutputDevice& Ar ) const override;
	virtual void DumpStaticMeshDrawListStats() const override;
	virtual void SetClearMotionBlurInfoGameThread() override;
	virtual void UpdateParameterCollections(const TArray<FMaterialParameterCollectionInstanceResource*>& InParameterCollections) override;

	/** Determines whether the scene has dynamic sky lighting. */
	bool HasDynamicSkyLighting() const
	{
		return (!UpperDynamicSkylightColor.Equals(FLinearColor::Black) || !LowerDynamicSkylightColor.Equals(FLinearColor::Black));
	}

	/** Determines whether the scene has atmospheric fog and sun light. */
	bool HasAtmosphericFog() const
	{
		return (AtmosphericFog != NULL); // Use default value when Sun Light is not existing
	}

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const override;

	/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVisibility(const FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler) override;

	/** Sets shader maps on the specified materials without blocking. */
	virtual void SetShaderMapsOnMaterialResources(const TMap<FMaterial*, class FMaterialShaderMap*>& MaterialsToUpdate) override;

	/** Updates static draw lists for the given set of materials. */
	virtual void UpdateStaticDrawListsForMaterials(const TArray<const FMaterial*>& Materials) override;

	virtual void Release() override;
	virtual UWorld* GetWorld() const override { return World; }

	/** Finds the closest reflection capture to a point in space. */
	const FReflectionCaptureProxy* FindClosestReflectionCapture(FVector Position) const;

	/** 
	 * Gets the scene's cubemap array and index into that array for the given reflection proxy. 
	 * If the proxy was not found in the scene's reflection state, the outputs are not written to.
	 */
	void GetCaptureParameters(const FReflectionCaptureProxy* ReflectionProxy, FTextureRHIParamRef& ReflectionCubemapArray, int32& ArrayIndex) const;

	/**
	 * Marks static mesh elements as needing an update if necessary.
	 */
	void ConditionalMarkStaticMeshElementsForUpdate();

	/**
	 * @return		true if hit proxies should be rendered in this scene.
	 */
	virtual bool RequiresHitProxies() const override;

	SIZE_T GetSizeBytes() const;

	/**
	* Return the scene to be used for rendering
	*/
	virtual class FScene* GetRenderScene() override
	{
		return this;
	}

	/**
	 * Sets the FX system associated with the scene.
	 */
	virtual void SetFXSystem( class FFXSystemInterface* InFXSystem ) override;

	/**
	 * Get the FX system associated with the scene.
	 */
	virtual class FFXSystemInterface* GetFXSystem() override;

	/**
	 * Exports the scene.
	 *
	 * @param	Ar		The Archive used for exporting.
	 **/
	virtual void Export( FArchive& Ar ) const override;

	FUniformBufferRHIParamRef GetParameterCollectionBuffer(const FGuid& InId) const
	{
		const FUniformBufferRHIRef* ExistingUniformBuffer = ParameterCollections.Find(InId);

		if (ExistingUniformBuffer)
		{
			return *ExistingUniformBuffer;
		}

		return FUniformBufferRHIParamRef();
	}

	virtual void ApplyWorldOffset(FVector InOffset) override;

	virtual void OnLevelAddedToWorld(FName InLevelName) override;

	virtual bool HasAnyLights() const override 
	{ 
		check(IsInGameThread());
		return NumVisibleLights_GameThread > 0 || NumEnabledSkylights_GameThread > 0; 
	}

	virtual bool IsEditorScene() const override { return bIsEditorScene; }

	virtual ERHIFeatureLevel::Type GetFeatureLevel() const override { return FeatureLevel; }

	bool ShouldRenderSkylight() const
	{
		return SkyLight && !SkyLight->bHasStaticLighting && GSupportsRenderTargetFormat_PF_FloatRGBA;
	}

	virtual TArray<FPrimitiveComponentId> GetScenePrimitiveComponentIds() const override
	{
		return PrimitiveComponentIds;
	}

private:

	/**
	 * Ensures the packed primitive arrays contain the same number of elements.
	 */
	void CheckPrimitiveArrays();

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 * Render thread version of function.
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	void GetRelevantLights_RenderThread( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const;

	/**
	 * Adds a primitive to the scene.  Called in the rendering thread by AddPrimitive.
	 * @param PrimitiveSceneInfo - The primitive being added.
	 */
	void AddPrimitiveSceneInfo_RenderThread(FRHICommandListImmediate& RHICmdList, FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/**
	 * Removes a primitive from the scene.  Called in the rendering thread by RemovePrimitive.
	 * @param PrimitiveSceneInfo - The primitive being removed.
	 */
	void RemovePrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** Updates a primitive's transform, called on the rendering thread. */
	void UpdatePrimitiveTransform_RenderThread(FRHICommandListImmediate& RHICmdList, FPrimitiveSceneProxy* PrimitiveSceneProxy, const FBoxSphereBounds& WorldBounds, const FBoxSphereBounds& LocalBounds, const FMatrix& LocalToWorld, const FVector& OwnerPosition);

	/** Updates a single primitive's lighting attachment root. */
	void UpdatePrimitiveLightingAttachmentRoot(UPrimitiveComponent* Primitive);

	/**
	 * Adds a light to the scene.  Called in the rendering thread by AddLight.
	 * @param LightSceneInfo - The light being added.
	 */
	void AddLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo);
	/**
	 * Adds a decal to the scene.  Called in the rendering thread by AddDecal or RemoveDecal.
	 * @param Component - The object that should being added or removed.
	 * @param bAdd true:add, FASLE:remove
	 */
	void AddOrRemoveDecal_RenderThread(FDeferredDecalProxy* Component, bool bAdd);

	/**
	 * Removes a light from the scene.  Called in the rendering thread by RemoveLight.
	 * @param LightSceneInfo - The light being removed.
	 */
	void RemoveLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo);

	void UpdateLightTransform_RenderThread(FLightSceneInfo* LightSceneInfo, const struct FUpdateLightTransformParameters& Parameters);

	/** 
	* Updates the contents of the given reflection capture by rendering the scene. 
	* This must be called on the game thread.
	*/
	void UpdateReflectionCaptureContents(UReflectionCaptureComponent* CaptureComponent);

	/** Updates the contents of all reflection captures in the scene.  Must be called from the game thread. */
	void UpdateAllReflectionCaptures();

	/** Sets shader maps on the specified materials without blocking. */
	void SetShaderMapsOnMaterialResources_RenderThread(FRHICommandListImmediate& RHICmdList, const FMaterialsToUpdateMap& MaterialsToUpdate);

	/** Updates static draw lists for the given materials. */
	void UpdateStaticDrawListsForMaterials_RenderThread(FRHICommandListImmediate& RHICmdList, const TArray<const FMaterial*>& Materials);

	/**
	 * Shifts scene data by provided delta
	 * Called on world origin changes
	 * 
	 * @param	InOffset	Delta to shift scene by
	 */
	void ApplyWorldOffset_RenderThread(FVector InOffset);

	/**
	 * Notification from game thread that level was added to a world
	 *
	 * @param	InLevelName		Level name
	 */
	void OnLevelAddedToWorld_RenderThread(FName InLevelName);

	/**
	 * Builds a FSceneRenderer instance for a given view and render target. Helper function for Scene capture code.
	 *
	 * @param	SceneCaptureComponent - The scene capture component for which to create a scene renderer.
	 * @param	TextureTarget - render target to draw to.
	 * @param	ViewRotationMatrix - Camera rotation matrix.
	 * @param	ViewLocation - Camera location.
	 * @param	FOV - Camera field of view.
	 * @param	MaxViewDistance - Far draw distance.
	 * @param	bCaptureSceneColour if true the returned scenerenderer will ignore post process operations as only the raw scene color is used.
	 * @param	PostProcessSettings - pointer to post process structure, NULL for no effect.
	 * @param	PostProcessBlendWeight - Blendweight for PostProcessSettings.
	 * @return	pointer to a configured SceneRenderer instance.
	 */
	FSceneRenderer* CreateSceneRenderer(USceneCaptureComponent* SceneCaptureComponent, UTextureRenderTarget* TextureTarget, const FMatrix& ViewRotationMatrix, const FVector& ViewLocation, float FOV, float MaxViewDistance, bool bCaptureSceneColour = true, FPostProcessSettings* PostProcessSettings = NULL, float PostProcessBlendWeight = 0);

private:
	/** 
	 * The number of visible lights in the scene
	 * Note: This is tracked on the game thread!
	 */
	int32 NumVisibleLights_GameThread;

	/** 
	 * Whether the scene has a valid sky light.
	 * Note: This is tracked on the game thread!
	 */
	int32 NumEnabledSkylights_GameThread;

	/** This scene's feature level */
	ERHIFeatureLevel::Type FeatureLevel;
};

#include "BasePassRendering.inl"

#endif // __SCENEPRIVATE_H__
