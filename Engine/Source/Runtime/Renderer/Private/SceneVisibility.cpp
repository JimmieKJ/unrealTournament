// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneVisibility.cpp: Scene visibility determination.
=============================================================================*/

#include "RendererPrivate.h"
#include "Engine.h"
#include "ScenePrivate.h"
#include "FXSystem.h"
#include "../../Engine/Private/SkeletalRenderGPUSkin.h"		// GPrevPerBoneMotionBlur
#include "SceneUtils.h"
#include "PostProcessing.h"

/*------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------*/

static float GWireframeCullThreshold = 5.0f;
static FAutoConsoleVariableRef CVarWireframeCullThreshold(
	TEXT("r.WireframeCullThreshold"),
	GWireframeCullThreshold,
	TEXT("Threshold below which objects in ortho wireframe views will be culled."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForLights = 0.03f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForLights(
	TEXT("r.MinScreenRadiusForLights"),
	GMinScreenRadiusForLights,
	TEXT("Threshold below which lights will be culled."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForDepthPrepass = 0.03f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForDepthPrepass(
	TEXT("r.MinScreenRadiusForDepthPrepass"),
	GMinScreenRadiusForDepthPrepass,
	TEXT("Threshold below which meshes will be culled from depth only pass."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForCSMDepth = 0.01f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForCSMDepth(
	TEXT("r.MinScreenRadiusForCSMDepth"),
	GMinScreenRadiusForCSMDepth,
	TEXT("Threshold below which meshes will be culled from CSM depth pass."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTemporalAASamples(
	TEXT("r.TemporalAASamples"),
	8,
	TEXT("Number of jittered positions for temporal AA (4, 8=default, 16, 32, 64)."),
	ECVF_RenderThreadSafe);

#if PLATFORM_MAC // @todo: disabled until rendering problems with HZB occlusion in OpenGL are solved
static int32 GHZBOcclusion = 0;
#else
static int32 GHZBOcclusion = 0;
#endif
static FAutoConsoleVariableRef CVarHZBOcclusion(
	TEXT("r.HZBOcclusion"),
	GHZBOcclusion,
	TEXT("Defines which occlusion system is used.\n")
	TEXT(" 0: Hardware occlusion queries\n")
	TEXT(" 1: Use HZB occlusion system (default, less GPU and CPU cost, more conservative results)")
	TEXT(" 2: Force HZB occlusion system (overrides rendering platform preferences)"),
	ECVF_RenderThreadSafe
	);

static int32 GVisualizeOccludedPrimitives = 0;
static FAutoConsoleVariableRef CVarVisualizeOccludedPrimitives(
	TEXT("r.VisualizeOccludedPrimitives"),
	GVisualizeOccludedPrimitives,
	TEXT("Draw boxes for all occluded primitives"),
	ECVF_RenderThreadSafe | ECVF_Cheat
	);

static int32 GAllowSubPrimitiveQueries = 1;
static FAutoConsoleVariableRef CVarAllowSubPrimitiveQueries(
	TEXT("r.AllowSubPrimitiveQueries"),
	GAllowSubPrimitiveQueries,
	TEXT("Enables sub primitive queries, currently only used by hierarchical instanced static meshes. 1: Enable, 0 Disabled. When disabled, one query is used for the entire proxy."),
	ECVF_RenderThreadSafe
	);


static TAutoConsoleVariable<int32> CVarLightShaftQuality(
	TEXT("r.LightShaftQuality"),
	1,
	TEXT("Defines the light shaft quality.\n")
	TEXT("  0: off\n")
	TEXT("  1: on (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarStaticMeshLODDistanceScale(
	TEXT("r.StaticMeshLODDistanceScale"),
	1.0f,
	TEXT("Scale factor for the distance used in computing discrete LOD for static meshes. (0.25-1)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

/** Distance fade cvars */
static int32 GDisableLODFade = false;
static FAutoConsoleVariableRef CVarDisableLODFade( TEXT("r.DisableLODFade"), GDisableLODFade, TEXT("Disable fading for distance culling"), ECVF_RenderThreadSafe );

static float GFadeTime = 0.25f;
static FAutoConsoleVariableRef CVarLODFadeTime( TEXT("r.LODFadeTime"), GFadeTime, TEXT("How long LOD takes to fade (in seconds)."), ECVF_RenderThreadSafe );

static float GDistanceFadeMaxTravel = 1000.0f;
static FAutoConsoleVariableRef CVarDistanceFadeMaxTravel( TEXT("r.DistanceFadeMaxTravel"), GDistanceFadeMaxTravel, TEXT("Max distance that the player can travel during the fade time."), ECVF_RenderThreadSafe );

/*------------------------------------------------------------------------------
	Visibility determination.
------------------------------------------------------------------------------*/

/**
 * Update a primitive's fading state.
 * @param FadingState - State to update.
 * @param View - The view for which to update.
 * @param bVisible - Whether the primitive should be visible in the view.
 */
static void UpdatePrimitiveFadingState(FPrimitiveFadingState& FadingState, FViewInfo& View, bool bVisible)
{
	if (FadingState.bValid)
	{
		if (FadingState.bIsVisible != bVisible)
		{
			float CurrentRealTime = View.Family->CurrentRealTime;

			// Need to kick off a fade, so make sure that we have fading state for that
			if( !IsValidRef(FadingState.UniformBuffer) )
			{
				// Primitive is not currently fading.  Start a new fade!
				FadingState.EndTime = CurrentRealTime + GFadeTime;

				if( bVisible )
				{
					// Fading in
					// (Time - StartTime) / FadeTime
					FadingState.FadeTimeScaleBias.X = 1.0f / GFadeTime;
					FadingState.FadeTimeScaleBias.Y = -CurrentRealTime / GFadeTime;
				}
				else
				{
					// Fading out
					// 1 - (Time - StartTime) / FadeTime
					FadingState.FadeTimeScaleBias.X = -1.0f / GFadeTime;
					FadingState.FadeTimeScaleBias.Y = 1.0f + CurrentRealTime / GFadeTime;
				}

				FDistanceCullFadeUniformShaderParameters Uniforms;
				Uniforms.FadeTimeScaleBias = FadingState.FadeTimeScaleBias;
				FadingState.UniformBuffer = FDistanceCullFadeUniformBufferRef::CreateUniformBufferImmediate( Uniforms, UniformBuffer_MultiFrame );
			}
			else
			{
				// Reverse fading direction but maintain current opacity
				// Solve for d: a*x+b = -a*x+d
				FadingState.FadeTimeScaleBias.Y = 2.0f * CurrentRealTime * FadingState.FadeTimeScaleBias.X + FadingState.FadeTimeScaleBias.Y;
				FadingState.FadeTimeScaleBias.X = -FadingState.FadeTimeScaleBias.X;
				
				if( bVisible )
				{
					// Fading in
					// Solve for x: a*x+b = 1
					FadingState.EndTime = ( 1.0f - FadingState.FadeTimeScaleBias.Y ) / FadingState.FadeTimeScaleBias.X;
				}
				else
				{
					// Fading out
					// Solve for x: a*x+b = 0
					FadingState.EndTime = -FadingState.FadeTimeScaleBias.Y / FadingState.FadeTimeScaleBias.X;
				}

				FDistanceCullFadeUniformShaderParameters Uniforms;
				Uniforms.FadeTimeScaleBias = FadingState.FadeTimeScaleBias;
				FadingState.UniformBuffer = FDistanceCullFadeUniformBufferRef::CreateUniformBufferImmediate( Uniforms, UniformBuffer_MultiFrame );
			}
		}
	}

	FadingState.FrameNumber = View.Family->FrameNumber;
	FadingState.bIsVisible = bVisible;
	FadingState.bValid = true;
}

bool FViewInfo::IsDistanceCulled( float DistanceSquared, float MinDrawDistance, float InMaxDrawDistance, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
	float FadeRadius = GDisableLODFade ? 0.0f : GDistanceFadeMaxTravel;
	float MaxDrawDistance = InMaxDrawDistance * MaxDrawDistanceScale;

	// If cull distance is disabled, always show (except foliage)
	if (Family->EngineShowFlags.DistanceCulledPrimitives
		&& !PrimitiveSceneInfo->Proxy->IsDetailMesh())
	{
		return false;
	}

	// The primitive is always culled if it exceeds the max fade distance.
	if (DistanceSquared > FMath::Square(MaxDrawDistance + FadeRadius) ||
		DistanceSquared < FMath::Square(MinDrawDistance))
	{
		return true;
	}

	const bool bDistanceCulled = (DistanceSquared > FMath::Square(MaxDrawDistance));
	const bool bMayBeFading = (DistanceSquared > FMath::Square(MaxDrawDistance - FadeRadius));

	bool bStillFading = false;
	if( !GDisableLODFade && bMayBeFading && State != NULL && !bDisableDistanceBasedFadeTransitions )
	{
		// Update distance-based visibility and fading state if it has not already been updated.
		int32 PrimitiveIndex = PrimitiveSceneInfo->GetIndex();
		FRelativeBitReference PrimitiveBit(PrimitiveIndex);
		if (PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(PrimitiveBit) == false)
		{
			FPrimitiveFadingState& FadingState = ((FSceneViewState*)State)->PrimitiveFadingStates.FindOrAdd(PrimitiveSceneInfo->PrimitiveComponentId);
			UpdatePrimitiveFadingState(FadingState, *this, !bDistanceCulled);
			FUniformBufferRHIParamRef UniformBuffer = FadingState.UniformBuffer;
			bStillFading = (UniformBuffer != NULL);
			PrimitiveFadeUniformBuffers[PrimitiveIndex] = UniformBuffer;
			PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(PrimitiveBit) = true;
		}
	}

	// If we're still fading then make sure the object is still drawn, even if it's beyond the max draw distance
	return ( bDistanceCulled && !bStillFading );
}

/**
 * Frustum cull primitives in the scene against the view.
 */
template<bool UseCustomCulling>
static int32 FrustumCull(const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_FrustumCull);

	int32 NumCulledPrimitives = 0;
	FVector ViewOriginForDistanceCulling = View.ViewMatrices.ViewOrigin;
	float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
	float FadeRadius = GDisableLODFade ? 0.0f : GDistanceFadeMaxTravel;
	uint8 CustomVisibilityFlags = EOcclusionFlags::CanBeOccluded | EOcclusionFlags::HasPrecomputedVisibility;

	for (FSceneBitArray::FIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
	{
		const FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[BitIt.GetIndex()];
		float DistanceSquared = (Bounds.Origin - ViewOriginForDistanceCulling).SizeSquared();
		float MaxDrawDistance = Bounds.MaxDrawDistance * MaxDrawDistanceScale;
		int32 VisibilityId = INDEX_NONE;

		if (UseCustomCulling &&
			((Scene->PrimitiveOcclusionFlags[BitIt.GetIndex()] & CustomVisibilityFlags) == CustomVisibilityFlags))
		{
			VisibilityId = Scene->PrimitiveVisibilityIds[BitIt.GetIndex()].ByteIndex;
		}

		// If cull distance is disabled, always show (except foliage)
		if (View.Family->EngineShowFlags.DistanceCulledPrimitives
			&& !Scene->Primitives[BitIt.GetIndex()]->Proxy->IsDetailMesh())
		{
			MaxDrawDistance = FLT_MAX;
		}

		// The primitive is always culled if it exceeds the max fade distance or lay outside the view frustum.
		if (DistanceSquared > FMath::Square(MaxDrawDistance + FadeRadius) ||
			DistanceSquared < Bounds.MinDrawDistanceSq ||
			(UseCustomCulling && !View.CustomVisibilityQuery->IsVisible(VisibilityId, FBoxSphereBounds(Bounds.Origin, Bounds.BoxExtent, Bounds.SphereRadius))) ||
			View.ViewFrustum.IntersectSphere(Bounds.Origin, Bounds.SphereRadius) == false ||
			View.ViewFrustum.IntersectBox(Bounds.Origin, Bounds.BoxExtent) == false)
		{
			STAT(NumCulledPrimitives++);
			continue;
		}

		if (DistanceSquared > FMath::Square(MaxDrawDistance))
		{
			View.PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(BitIt) = true;
		}
		else
		{
			// The primitive is visible!
			BitIt.GetValue() = true;
			if (DistanceSquared > FMath::Square(MaxDrawDistance - FadeRadius))
			{
				View.PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(BitIt) = true;
			}
		}
	}

	return NumCulledPrimitives;
}

/**
 * Updated primitive fading states for the view.
 */
static void UpdatePrimitiveFading(const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveFading);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if (ViewState)
	{
		uint32 PrevFrameNumber = ViewState->PrevFrameNumber;
		float CurrentRealTime = View.Family->CurrentRealTime;

		// First clear any stale fading states.
		for (FPrimitiveFadingStateMap::TIterator It(ViewState->PrimitiveFadingStates); It; ++It)
		{
			FPrimitiveFadingState& FadingState = It.Value();
			if (FadingState.FrameNumber != PrevFrameNumber ||
				(IsValidRef(FadingState.UniformBuffer) && CurrentRealTime >= FadingState.EndTime))
			{
				It.RemoveCurrent();
			}
		}

		// Should we allow fading transitions at all this frame?  For frames where the camera moved
		// a large distance or where we haven't rendered a view in awhile, it's best to disable
		// fading so users don't see unexpected object transitions.
		if (!GDisableLODFade && !View.bDisableDistanceBasedFadeTransitions)
		{
			// Do a pass over potentially fading primitives and update their states.
			for (FSceneSetBitIterator BitIt(View.PotentiallyFadingPrimitiveMap); BitIt; ++BitIt)
			{
				bool bVisible = View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt);
				FPrimitiveFadingState& FadingState = ViewState->PrimitiveFadingStates.FindOrAdd(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
				UpdatePrimitiveFadingState(FadingState, View, bVisible);
				FUniformBufferRHIParamRef UniformBuffer = FadingState.UniformBuffer;
				if (UniformBuffer && !bVisible)
				{
					// If the primitive is fading out make sure it remains visible.
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = true;
				}
				View.PrimitiveFadeUniformBuffers[BitIt.GetIndex()] = UniformBuffer;
			}
		}
	}
}

/**
 * Cull occluded primitives in the view.
 */
static int32 OcclusionCull(FRHICommandListImmediate& RHICmdList, const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_OcclusionCull);

	// INITVIEWS_TODO: This could be more efficient if broken up in to separate concerns:
	// - What is occluded?
	// - For which primitives should we render occlusion queries?
	// - Generate occlusion query geometry.

	int32 NumOccludedPrimitives = 0;
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	
	// Disable HZB on OpenGL platforms to avoid rendering artefacts
	// It can be forced on by setting HZBOcclusion to 2
	bool bHZBOcclusion = (!IsOpenGLPlatform(GShaderPlatformForFeatureLevel[Scene->GetFeatureLevel()]) && GHZBOcclusion) || (GHZBOcclusion == 2);

	// Use precomputed visibility data if it is available.
	if (View.PrecomputedVisibilityData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_LookupPrecomputedVisibility);

		uint8 PrecomputedVisibilityFlags = EOcclusionFlags::CanBeOccluded | EOcclusionFlags::HasPrecomputedVisibility;
		for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
		{
			if ((Scene->PrimitiveOcclusionFlags[BitIt.GetIndex()] & PrecomputedVisibilityFlags) == PrecomputedVisibilityFlags)
			{
				FPrimitiveVisibilityId VisibilityId = Scene->PrimitiveVisibilityIds[BitIt.GetIndex()];
				if ((View.PrecomputedVisibilityData[VisibilityId.ByteIndex] & VisibilityId.BitMask) == 0)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					INC_DWORD_STAT_BY(STAT_StaticallyOccludedPrimitives,1);
					STAT(NumOccludedPrimitives++);
				}
			}
		}
	}

	float CurrentRealTime = View.Family->CurrentRealTime;
	if (ViewState)
	{
		if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4)
		{
			bool bClearQueries = !View.Family->EngineShowFlags.HitProxies;
			bool bSubmitQueries = !View.bDisableQuerySubmissions;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			bSubmitQueries = bSubmitQueries && !ViewState->HasViewParent() && !ViewState->bIsFrozen;
#endif

			if( bHZBOcclusion )
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_MapHZBResults);
				check(!ViewState->HZBOcclusionTests.IsValidFrame(ViewState->OcclusionFrameCounter));
				ViewState->HZBOcclusionTests.MapResults(RHICmdList);
			}

			FViewElementPDI OcclusionPDI(&View, NULL);

			QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitives);

			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				uint8 OcclusionFlags = Scene->PrimitiveOcclusionFlags[BitIt.GetIndex()];
				bool bCanBeOccluded = (OcclusionFlags & EOcclusionFlags::CanBeOccluded) != 0;

				if(GIsEditor)
				{
					FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[BitIt.GetIndex()];

					if(PrimitiveSceneInfo->Proxy->IsSelected())
					{
						// to render occluded outline for selected objects
						bCanBeOccluded = false;
					}
				}
				int32 NumSubQueries = 1;
				bool bSubQueries = false;
				const TArray<FBoxSphereBounds>* SubBounds = nullptr;
				TArray<bool, SceneRenderingAllocator> SubIsOccluded;

				if ((OcclusionFlags & EOcclusionFlags::HasSubprimitiveQueries) && GAllowSubPrimitiveQueries)
				{
					FPrimitiveSceneProxy* Proxy = Scene->Primitives[BitIt.GetIndex()]->Proxy;
					SubBounds = Proxy->GetOcclusionQueries(&View);
					NumSubQueries = SubBounds->Num();
					bSubQueries = true;
					if (!NumSubQueries)
					{
						View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
						continue;
					}
					SubIsOccluded.Reserve(NumSubQueries);
				}

				bool bAllSubOcclusionStateIsDefinite = true;
				bool bAllSubOccluded = true;
				FPrimitiveComponentId PrimitiveId = Scene->PrimitiveComponentIds[BitIt.GetIndex()];
				for (int32 SubQuery = 0; SubQuery < NumSubQueries; SubQuery++)
				{

					FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = ViewState->PrimitiveOcclusionHistorySet.Find(FPrimitiveOcclusionHistoryKey(PrimitiveId, SubQuery));
					bool bIsOccluded = false;
					bool bOcclusionStateIsDefinite = false;
					if (!PrimitiveOcclusionHistory)
					{
						// If the primitive doesn't have an occlusion history yet, create it.
						PrimitiveOcclusionHistory = &ViewState->PrimitiveOcclusionHistorySet[
							ViewState->PrimitiveOcclusionHistorySet.Add(FPrimitiveOcclusionHistory(PrimitiveId, SubQuery))
							];

						// If the primitive hasn't been visible recently enough to have a history, treat it as unoccluded this frame so it will be rendered as an occluder and its true occlusion state can be determined.
						// already set bIsOccluded = false;

						// Flag the primitive's occlusion state as indefinite, which will force it to be queried this frame.
						// The exception is if the primitive isn't occludable, in which case we know that it's definitely unoccluded.
						bOcclusionStateIsDefinite = bCanBeOccluded ? false : true;
					}
					else
					{
						if (View.bIgnoreExistingQueries)
						{
							// If the view is ignoring occlusion queries, the primitive is definitely unoccluded.
							// already set bIsOccluded = false;
							bOcclusionStateIsDefinite = View.bDisableQuerySubmissions;
						}
						else if (bCanBeOccluded)
						{
							if( bHZBOcclusion )
							{
								if( ViewState->HZBOcclusionTests.IsValidFrame(PrimitiveOcclusionHistory->HZBTestFrameNumber) )
								{
									bIsOccluded = !ViewState->HZBOcclusionTests.IsVisible( PrimitiveOcclusionHistory->HZBTestIndex );
									bOcclusionStateIsDefinite = true;
								}
							}
							else
							{
								// Read the occlusion query results.
								uint64 NumSamples = 0;
								FRenderQueryRHIRef& PastQuery = PrimitiveOcclusionHistory->GetPastQuery(ViewState->OcclusionFrameCounter);
								if (IsValidRef(PastQuery))
								{
									// NOTE: RHIGetOcclusionQueryResult should never fail when using a blocking call, rendering artifacts may show up.
									if (RHICmdList.GetRenderQueryResult(PastQuery, NumSamples, true))
									{
										// we render occlusion without MSAA
										uint32 NumPixels = (uint32)NumSamples;

										// The primitive is occluded if none of its bounding box's pixels were visible in the previous frame's occlusion query.
										bIsOccluded = (NumPixels == 0);
										if (!bIsOccluded)
										{
											checkSlow(View.OneOverNumPossiblePixels > 0.0f);
											PrimitiveOcclusionHistory->LastPixelsPercentage = NumPixels * View.OneOverNumPossiblePixels;
										}
										else
										{
											PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
										}


										// Flag the primitive's occlusion state as definite if it wasn't grouped.
										bOcclusionStateIsDefinite = !PrimitiveOcclusionHistory->bGroupedQuery;
									}
									else
									{
										// If the occlusion query failed, treat the primitive as visible.  
										// already set bIsOccluded = false;
									}
								}
								else
								{
									// If there's no occlusion query for the primitive, set it's visibility state to whether it has been unoccluded recently.
									bIsOccluded = (PrimitiveOcclusionHistory->LastVisibleTime + GEngine->PrimitiveProbablyVisibleTime < CurrentRealTime);
									if (bIsOccluded)
									{
										PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
									}
									else
									{
										PrimitiveOcclusionHistory->LastPixelsPercentage = GEngine->MaxOcclusionPixelsFraction;
									}

									// the state was definite last frame, otherwise we would have ran a query
									bOcclusionStateIsDefinite = true;
								}
							}

							if( GVisualizeOccludedPrimitives && bIsOccluded )
							{
								const FBoxSphereBounds& Bounds = bSubQueries ? (*SubBounds)[SubQuery] : Scene->PrimitiveOcclusionBounds[ BitIt.GetIndex() ];
								DrawWireBox( &OcclusionPDI, Bounds.GetBox(), FColor(50, 255, 50), SDPG_Foreground );
							}
						}
						else
						{
							// Primitives that aren't occludable are considered definitely unoccluded.
							// already set bIsOccluded = false;
							bOcclusionStateIsDefinite = true;
						}

						if (bClearQueries)
						{
							ViewState->OcclusionQueryPool.ReleaseQuery(RHICmdList, PrimitiveOcclusionHistory->GetPastQuery(ViewState->OcclusionFrameCounter));
						}
					}

					// Set the primitive's considered time to keep its occlusion history from being trimmed.
					PrimitiveOcclusionHistory->LastConsideredTime = CurrentRealTime;

					if (bSubmitQueries && bCanBeOccluded)
					{
						bool bAllowBoundsTest;
						const FBoxSphereBounds& OcclusionBounds = bSubQueries ? (*SubBounds)[SubQuery] : Scene->PrimitiveOcclusionBounds[ BitIt.GetIndex() ];
						if (View.bHasNearClippingPlane)
						{
							bAllowBoundsTest = View.NearClippingPlane.PlaneDot(OcclusionBounds.Origin) < 
								-(FVector::BoxPushOut(View.NearClippingPlane,OcclusionBounds.BoxExtent));
						}
						else if (!View.IsPerspectiveProjection())
						{
							// Transform parallel near plane
							static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check equation for culling!");
							bAllowBoundsTest = View.WorldToScreen(OcclusionBounds.Origin).Z - View.ViewMatrices.ProjMatrix.M[2][2] * OcclusionBounds.SphereRadius < 1;
						}
						else
						{
							bAllowBoundsTest = OcclusionBounds.SphereRadius < HALF_WORLD_MAX;
						}

						if (bAllowBoundsTest)
						{
							if( bHZBOcclusion )
							{
								// Always run
								PrimitiveOcclusionHistory->HZBTestIndex = ViewState->HZBOcclusionTests.AddBounds( OcclusionBounds.Origin, OcclusionBounds.BoxExtent );
								PrimitiveOcclusionHistory->HZBTestFrameNumber = ViewState->OcclusionFrameCounter;
							}
							else
							{
								// decide if a query should be run this frame
								bool bRunQuery,bGroupedQuery;

								if (!bSubQueries && // sub queries are never grouped, we assume the custom code knows what it is doing and will group internally if it wants
									(OcclusionFlags & EOcclusionFlags::AllowApproximateOcclusion))
								{
									if (bIsOccluded)
									{
										// Primitives that were occluded the previous frame use grouped queries.
										bGroupedQuery = true;
										bRunQuery = true;
									}
									else if (bOcclusionStateIsDefinite)
									{
										// If the primitive's is definitely unoccluded, only requery it occasionally.
										float FractionMultiplier = FMath::Max(PrimitiveOcclusionHistory->LastPixelsPercentage/GEngine->MaxOcclusionPixelsFraction, 1.0f);
										bRunQuery = (FractionMultiplier * GOcclusionRandomStream.GetFraction() < GEngine->MaxOcclusionPixelsFraction);
										bGroupedQuery = false;
									}
									else
									{
										bGroupedQuery = false;
										bRunQuery = true;
									}
								}
								else
								{
									// Primitives that need precise occlusion results use individual queries.
									bGroupedQuery = false;
									bRunQuery = true;
								}

								if (bRunQuery)
								{
									PrimitiveOcclusionHistory->SetCurrentQuery(ViewState->OcclusionFrameCounter, 
										bGroupedQuery ? 
										View.GroupedOcclusionQueries.BatchPrimitive(OcclusionBounds.Origin + View.ViewMatrices.PreViewTranslation,OcclusionBounds.BoxExtent) :
										View.IndividualOcclusionQueries.BatchPrimitive(OcclusionBounds.Origin + View.ViewMatrices.PreViewTranslation,OcclusionBounds.BoxExtent)
										);	
								}
								PrimitiveOcclusionHistory->bGroupedQuery = bGroupedQuery;
							}
						}
						else
						{
							// If the primitive's bounding box intersects the near clipping plane, treat it as definitely unoccluded.
							bIsOccluded = false;
							bOcclusionStateIsDefinite = true;
						}
					}

					if (bSubQueries)
					{
						SubIsOccluded.Add(bIsOccluded);
						if (!bIsOccluded)
						{
							bAllSubOccluded = false;
							if (bOcclusionStateIsDefinite)
							{
								PrimitiveOcclusionHistory->LastVisibleTime = CurrentRealTime;
							}
						}
						if (bIsOccluded || !bOcclusionStateIsDefinite)
						{
							bAllSubOcclusionStateIsDefinite = false;
						}
					}
					else
					{
						if (bIsOccluded)
						{
							View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
							STAT(NumOccludedPrimitives++);
						}
						else if (bOcclusionStateIsDefinite)
						{
							PrimitiveOcclusionHistory->LastVisibleTime = CurrentRealTime;
							View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
						}
					}
				}
				if (bSubQueries)
				{
					FPrimitiveSceneProxy* Proxy = Scene->Primitives[BitIt.GetIndex()]->Proxy;
					Proxy->AcceptOcclusionResults(&View, &SubIsOccluded[0], SubIsOccluded.Num());
					if (bAllSubOccluded)
					{
						View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
						STAT(NumOccludedPrimitives++);
					}
					else if (bAllSubOcclusionStateIsDefinite)
					{
						View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
					}
				}
			}

			if( bHZBOcclusion )
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_HZBUnmapResults);

				ViewState->HZBOcclusionTests.UnmapResults(RHICmdList);

				if( bSubmitQueries )
				{
					ViewState->HZBOcclusionTests.SetValidFrameNumber(ViewState->OcclusionFrameCounter);
				}
			}
		}
		else
		{
			// No occlusion queries, so mark primitives as not occluded
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
			}
		}
	}

	return NumOccludedPrimitives;
}

/** List of relevant static primitives. Indexes into Scene->Primitives. */
typedef TArray<int32, SceneRenderingAllocator> FRelevantStaticPrimitives;

/**
 * Computes view relevance for visible primitives in the view and adds them to
 * appropriate per-view rendering lists.
 * @param Scene - The scene being rendered.
 * @param View - The view for which to compute relevance.
 * @param ViewBit - Bit mask: 1 << ViewIndex where Views(ViewIndex) == View.
 * @param OutRelevantStaticPrimitives - Upon return contains a list of relevant
 *                                      static primitives.
 *                                callback for this view will have ViewBit set.
 */
static void ComputeRelevanceForView(
	FRHICommandListImmediate& RHICmdList,
	const FScene* Scene,
	FViewInfo& View,
	uint8 ViewBit,
	FRelevantStaticPrimitives& OutRelevantStaticPrimitives,
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks,
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks
	)
{
	SCOPE_CYCLE_COUNTER(STAT_ComputeViewRelevance);

	// INITVIEWS_TODO: This may be more efficient to break in to:
	// - Compute view relevance.
	// - Gather relevant primitives.
	// - Update last render time.
	// But it's hard to say since the view relevance is going to be available right now.

	check(OutHasDynamicMeshElementsMasks.Num() == Scene->Primitives.Num());

	float CurrentWorldTime = View.Family->CurrentWorldTime;
	float DeltaWorldTime = View.Family->DeltaWorldTime;

	for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
	{
		const int32 BitIndex = BitIt.GetIndex();
		FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[BitIndex];
		FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[BitIndex];
		ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);
		ViewRelevance.bInitializedThisFrame = true;

		const bool bStaticRelevance = ViewRelevance.bStaticRelevance;
		const bool bDrawRelevance = ViewRelevance.bDrawRelevance;
		const bool bDynamicRelevance = ViewRelevance.bDynamicRelevance;
		const bool bShadowRelevance = ViewRelevance.bShadowRelevance;
		const bool bEditorRelevance = ViewRelevance.bEditorPrimitiveRelevance;
		const bool bTranslucentRelevance = ViewRelevance.HasTranslucency();		

		if (bStaticRelevance && (bDrawRelevance || bShadowRelevance))
		{
			OutRelevantStaticPrimitives.Add(BitIt.GetIndex());
		}

		if (!bDrawRelevance)
		{
			View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
			continue;
		}

		if (bEditorRelevance)
		{
			// Editor primitives are rendered after post processing and composited onto the scene
			View.VisibleEditorPrimitives.Add(PrimitiveSceneInfo);

			if (GIsEditor)
			{
				OutHasDynamicEditorMeshElementsMasks[BitIt.GetIndex()] |= ViewBit;
			}
		}
		else if(bDynamicRelevance)
		{
			// Keep track of visible dynamic primitives.
			View.VisibleDynamicPrimitives.Add(PrimitiveSceneInfo);
			OutHasDynamicMeshElementsMasks[BitIt.GetIndex()] |= ViewBit;
		}

		if (ViewRelevance.HasTranslucency() && !bEditorRelevance && ViewRelevance.bRenderInMainPass)
		{
			// Add to set of dynamic translucent primitives
			View.TranslucentPrimSet.AddScenePrimitive(PrimitiveSceneInfo, View, ViewRelevance.bNormalTranslucencyRelevance, ViewRelevance.bSeparateTranslucencyRelevance);

			if (ViewRelevance.bDistortionRelevance)
			{
				// Add to set of dynamic distortion primitives
				View.DistortionPrimSet.AddScenePrimitive(PrimitiveSceneInfo->Proxy);
			}
		}
		
		View.ShadingModelMaskInView |= ViewRelevance.ShadingModelMaskRelevance;
		
		if (ViewRelevance.bRenderCustomDepth)
		{
			// Add to set of dynamic distortion primitives
			View.CustomDepthSet.AddScenePrimitive(PrimitiveSceneInfo->Proxy);
		}

		// INITVIEWS_TODO: Do this in a separate pass? There are no dependencies
		// here except maybe ParentPrimitives. This could be done in a 
		// low-priority background task and forgotten about.

		// If the primitive's last render time is older than last frame, consider
		// it newly visible and update its visibility change time
		if (PrimitiveSceneInfo->LastRenderTime < CurrentWorldTime - DeltaWorldTime - DELTA)
		{
			PrimitiveSceneInfo->LastVisibilityChangeTime = CurrentWorldTime;
		}
		PrimitiveSceneInfo->LastRenderTime = CurrentWorldTime;

		// If the primitive is definitely unoccluded or if in Wireframe mode and the primitive is estimated
		// to be unoccluded, then update the primitive components's LastRenderTime 
		// on the game thread. This signals that the primitive is visible.
		if (View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) || (View.Family->EngineShowFlags.Wireframe && View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt)))
		{
			// Update the PrimitiveComponent's LastRenderTime.
			*(PrimitiveSceneInfo->ComponentLastRenderTime) = CurrentWorldTime;
		}

		// Cache the nearest reflection proxy if needed
		if (PrimitiveSceneInfo->bNeedsCachedReflectionCaptureUpdate
			// During Forward Shading, the per-object reflection is used for everything
			// Otherwise it is just used on translucency
			&& (!Scene->ShouldUseDeferredRenderer() || bTranslucentRelevance))
		{
			PrimitiveSceneInfo->CachedReflectionCaptureProxy = Scene->FindClosestReflectionCapture(Scene->PrimitiveBounds[BitIt.GetIndex()].Origin);
			PrimitiveSceneInfo->bNeedsCachedReflectionCaptureUpdate = false;
		}

		PrimitiveSceneInfo->ConditionalUpdateStaticMeshes(RHICmdList);
	}
}

template<class T>
struct FRelevancePrimSet
{
	enum
	{
		MaxPrims = 127 //like 128, but we leave space for NumPrims
	};
	int32 NumPrims;
	T Prims[MaxPrims];
	FORCEINLINE FRelevancePrimSet()
		: NumPrims(0)
	{
		//FMemory::Memzero(Prims, sizeof(T) * MaxPrims);
	}
	FORCEINLINE void AddPrim(T Prim)
	{
		checkSlow(NumPrims < MaxPrims);
		Prims[NumPrims++] = Prim;
	}
	FORCEINLINE bool IsFull() const
	{
		return NumPrims >= MaxPrims;
	}
	template<class TARRAY>
	FORCEINLINE void AppendTo(TARRAY& Array)
	{
		Array.Append(Prims, NumPrims);
	}
};

struct FMarkRelevantStaticMeshesForViewData
{
	FVector ViewOrigin;
	float MaxDrawDistanceScaleSquared;
	int32 ForcedLODLevel;
	float LODScale;
	float InvLODScale;
	float MinScreenRadiusForCSMDepthSquared;
	float MinScreenRadiusForDepthPrepassSquared;
	bool bForceEarlyZPass;

	FMarkRelevantStaticMeshesForViewData(FViewInfo& View)
	{
		ViewOrigin = View.ViewMatrices.ViewOrigin;

		MaxDrawDistanceScaleSquared = GetCachedScalabilityCVars().ViewDistanceScaleSquared;

		// outside of the loop to be more efficient
		ForcedLODLevel = (View.Family->EngineShowFlags.LOD) ? GetCVarForceLOD() : 0;

		LODScale = CVarStaticMeshLODDistanceScale.GetValueOnRenderThread();
		InvLODScale = 1.0f / LODScale;

		MinScreenRadiusForCSMDepthSquared = GMinScreenRadiusForCSMDepth * GMinScreenRadiusForCSMDepth;
		MinScreenRadiusForDepthPrepassSquared = GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass;

		extern TAutoConsoleVariable<int32> CVarEarlyZPass;
		bForceEarlyZPass = CVarEarlyZPass.GetValueOnRenderThread() == 2;
	}
};

namespace EMarkMaskBits
{
	enum Type
	{
		StaticMeshShadowDepthMapMask = 0x1,
		StaticMeshVisibilityMapMask = 0x2,
		StaticMeshVelocityMapMask = 0x4,
		StaticMeshOccluderMapMask = 0x8,
	};
}

struct FRelevancePacket
{
	const float CurrentWorldTime;
	const float DeltaWorldTime;

	FRHICommandListImmediate& RHICmdList;
	const FScene* Scene;
	const FViewInfo& View;
	const uint8 ViewBit;
	const FMarkRelevantStaticMeshesForViewData& ViewData;
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks;
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks;
	uint8* RESTRICT MarkMasks;

	FRelevancePrimSet<int32> Input;
	FRelevancePrimSet<int32> RelevantStaticPrimitives;
	FRelevancePrimSet<int32> NotDrawRelevant;
	FRelevancePrimSet<FPrimitiveSceneInfo*> VisibleDynamicPrimitives;
	FRelevancePrimSet<FTranslucentPrimSet::FSortedPrim> SortedSeparateTranslucencyPrims;
	FRelevancePrimSet<FTranslucentPrimSet::FSortedPrim> SortedTranslucencyPrims;
	FRelevancePrimSet<FPrimitiveSceneProxy*> DistortionPrimSet;
	FRelevancePrimSet<FPrimitiveSceneProxy*> CustomDepthSet;
	FRelevancePrimSet<FPrimitiveSceneInfo*> UpdateStaticMeshes;
	FRelevancePrimSet<FPrimitiveSceneInfo*> VisibleEditorPrimitives;
	uint16 CombinedShadingModelMask;

	FRelevancePacket(
		FRHICommandListImmediate& InRHICmdList,
		const FScene* InScene, 
		const FViewInfo& InView, 
		uint8 InViewBit,
		const FMarkRelevantStaticMeshesForViewData& InViewData,
		FPrimitiveViewMasks& InOutHasDynamicMeshElementsMasks,
		FPrimitiveViewMasks& InOutHasDynamicEditorMeshElementsMasks,
		uint8* InMarkMasks)

		: CurrentWorldTime(InView.Family->CurrentWorldTime)
		, DeltaWorldTime(InView.Family->DeltaWorldTime)
		, RHICmdList(InRHICmdList)
		, Scene(InScene)
		, View(InView)
		, ViewBit(InViewBit)
		, ViewData(InViewData)
		, OutHasDynamicMeshElementsMasks(InOutHasDynamicMeshElementsMasks)
		, OutHasDynamicEditorMeshElementsMasks(InOutHasDynamicEditorMeshElementsMasks)
		, MarkMasks(InMarkMasks)
		, CombinedShadingModelMask(0)
	{
	}

	void AnyThreadTask()
	{
		ComputeRelevance();
		MarkRelevant();
	}

	void ComputeRelevance()
	{
		CombinedShadingModelMask = 0;
		SCOPE_CYCLE_COUNTER(STAT_ComputeViewRelevance);
		for (int32 Index = 0; Index < Input.NumPrims; Index++)
		{
			int32 BitIndex = Input.Prims[Index];
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[BitIndex];
			FPrimitiveViewRelevance& ViewRelevance = const_cast<FPrimitiveViewRelevance&>(View.PrimitiveViewRelevanceMap[BitIndex]);
			ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);
			ViewRelevance.bInitializedThisFrame = true;

			const bool bStaticRelevance = ViewRelevance.bStaticRelevance;
			const bool bDrawRelevance = ViewRelevance.bDrawRelevance;
			const bool bDynamicRelevance = ViewRelevance.bDynamicRelevance;
			const bool bShadowRelevance = ViewRelevance.bShadowRelevance;
			const bool bEditorRelevance = ViewRelevance.bEditorPrimitiveRelevance;
			const bool bTranslucentRelevance = ViewRelevance.HasTranslucency();

			if (bStaticRelevance && (bDrawRelevance || bShadowRelevance))
			{
				RelevantStaticPrimitives.AddPrim(BitIndex);
			}

			if (!bDrawRelevance)
			{
				NotDrawRelevant.AddPrim(BitIndex);
				continue;
			}

			if (bEditorRelevance)
			{
				// Editor primitives are rendered after post processing and composited onto the scene
				VisibleEditorPrimitives.AddPrim(PrimitiveSceneInfo);

				if (GIsEditor)
				{
					OutHasDynamicEditorMeshElementsMasks[BitIndex] |= ViewBit;
				}
			}
			else if(bDynamicRelevance)
			{
				// Keep track of visible dynamic primitives.
				VisibleDynamicPrimitives.AddPrim(PrimitiveSceneInfo);
				OutHasDynamicMeshElementsMasks[BitIndex] |= ViewBit;
			}

			if (ViewRelevance.HasTranslucency() && !bEditorRelevance && ViewRelevance.bRenderInMainPass)
			{
				// Add to set of dynamic translucent primitives
				FTranslucentPrimSet::PlaceScenePrimitive(PrimitiveSceneInfo, View, ViewRelevance.bNormalTranslucencyRelevance, ViewRelevance.bSeparateTranslucencyRelevance, 
					&SortedTranslucencyPrims.Prims[SortedTranslucencyPrims.NumPrims], SortedTranslucencyPrims.NumPrims,
					&SortedSeparateTranslucencyPrims.Prims[SortedSeparateTranslucencyPrims.NumPrims], SortedSeparateTranslucencyPrims.NumPrims
					);

				if (ViewRelevance.bDistortionRelevance)
				{
					// Add to set of dynamic distortion primitives
					DistortionPrimSet.AddPrim(PrimitiveSceneInfo->Proxy);
				}
			}

			CombinedShadingModelMask |= ViewRelevance.ShadingModelMaskRelevance;			

			if (ViewRelevance.bRenderCustomDepth)
			{
				// Add to set of dynamic distortion primitives
				CustomDepthSet.AddPrim(PrimitiveSceneInfo->Proxy);
			}

			// INITVIEWS_TODO: Do this in a separate pass? There are no dependencies
			// here except maybe ParentPrimitives. This could be done in a 
			// low-priority background task and forgotten about.

			// If the primitive's last render time is older than last frame, consider
			// it newly visible and update its visibility change time
			if (PrimitiveSceneInfo->LastRenderTime < CurrentWorldTime - DeltaWorldTime - DELTA)
			{
				PrimitiveSceneInfo->LastVisibilityChangeTime = CurrentWorldTime;
			}
			PrimitiveSceneInfo->LastRenderTime = CurrentWorldTime;

			// If the primitive is definitely unoccluded or if in Wireframe mode and the primitive is estimated
			// to be unoccluded, then update the primitive components's LastRenderTime 
			// on the game thread. This signals that the primitive is visible.
			if (View.PrimitiveDefinitelyUnoccludedMap[BitIndex] || (View.Family->EngineShowFlags.Wireframe && View.PrimitiveVisibilityMap[BitIndex]))
			{
				// Update the PrimitiveComponent's LastRenderTime.
				*(PrimitiveSceneInfo->ComponentLastRenderTime) = CurrentWorldTime;
			}

			// Cache the nearest reflection proxy if needed
			if (PrimitiveSceneInfo->bNeedsCachedReflectionCaptureUpdate
				// During Forward Shading, the per-object reflection is used for everything
				// Otherwise it is just used on translucency
				&& (!Scene->ShouldUseDeferredRenderer() || bTranslucentRelevance))
			{
				PrimitiveSceneInfo->CachedReflectionCaptureProxy = Scene->FindClosestReflectionCapture(Scene->PrimitiveBounds[BitIndex].Origin);
				PrimitiveSceneInfo->bNeedsCachedReflectionCaptureUpdate = false;
			}
			if (PrimitiveSceneInfo->NeedsUpdateStaticMeshes())
			{
				UpdateStaticMeshes.AddPrim(PrimitiveSceneInfo);
			}
		}
	}
	void MarkRelevant()
	{
		SCOPE_CYCLE_COUNTER(STAT_StaticRelevance);

		// using a local counter to reduce memory traffic
		int32 NumVisibleStaticMeshElements = 0;
		FViewInfo& WriteView = const_cast<FViewInfo&>(View);

		for (int32 StaticPrimIndex = 0, Num = RelevantStaticPrimitives.NumPrims; StaticPrimIndex < Num; ++StaticPrimIndex)
		{
			int32 PrimitiveIndex = RelevantStaticPrimitives.Prims[StaticPrimIndex];
			const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = Scene->Primitives[PrimitiveIndex];
			const FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[PrimitiveIndex];
			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];

			int8 LODToRender = ComputeLODForMeshes( PrimitiveSceneInfo->StaticMeshes, View, Bounds.Origin, Bounds.SphereRadius, ViewData.ForcedLODLevel, ViewData.LODScale);

			float DistanceSquared = (Bounds.Origin - ViewData.ViewOrigin).SizeSquared();
			const float LODFactorDistanceSquared = DistanceSquared * FMath::Square(View.LODDistanceFactor * ViewData.InvLODScale);
			const bool bDrawShadowDepth = FMath::Square(Bounds.SphereRadius) > ViewData.MinScreenRadiusForCSMDepthSquared * LODFactorDistanceSquared;
			const bool bDrawDepthOnly = ViewData.bForceEarlyZPass || FMath::Square(Bounds.SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared;

			const int32 NumStaticMeshes = PrimitiveSceneInfo->StaticMeshes.Num();
			for(int32 MeshIndex = 0;MeshIndex < NumStaticMeshes;MeshIndex++)
			{
				const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
				if (StaticMesh.LODIndex == LODToRender)
				{
					uint8 MarkMask = 0;
					bool bNeedsBatchVisibility = false;

					if (ViewRelevance.bShadowRelevance && bDrawShadowDepth && StaticMesh.CastShadow)
					{
						// Mark static mesh as visible in shadows.
						MarkMask |= EMarkMaskBits::StaticMeshShadowDepthMapMask;
						bNeedsBatchVisibility = true;
					}

					if(ViewRelevance.bDrawRelevance && !StaticMesh.bShadowOnly && (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth))
					{
						// Mark static mesh as visible for rendering
						MarkMask |= EMarkMaskBits::StaticMeshVisibilityMapMask;
						if (PrimitiveSceneInfo->ShouldRenderVelocity(View, false))
						{
							MarkMask |= EMarkMaskBits::StaticMeshVelocityMapMask;
						}
						++NumVisibleStaticMeshElements;

						// If the static mesh is an occluder, check whether it covers enough of the screen to be used as an occluder.
						if(	StaticMesh.bUseAsOccluder && bDrawDepthOnly )
						{
							MarkMask |= EMarkMaskBits::StaticMeshOccluderMapMask;
						}
						bNeedsBatchVisibility = true;
					}
					if (MarkMask)
					{
						MarkMasks[StaticMesh.Id] = MarkMask;
					}

					// Static meshes with a single element always draw, as if the mask were 0x1.
					if(bNeedsBatchVisibility && StaticMesh.Elements.Num() > 1)
					{
						WriteView.StaticMeshBatchVisibility[StaticMesh.Id] = StaticMesh.VertexFactory->GetStaticBatchElementVisibility(View, &StaticMesh);
					}
				}
			}
		}
		static_assert(sizeof(WriteView.NumVisibleStaticMeshElements) == sizeof(int32), "Atomic is the wrong size");
		FPlatformAtomics::InterlockedAdd((volatile int32*)&WriteView.NumVisibleStaticMeshElements, NumVisibleStaticMeshElements);
	}

	void RenderThreadFinalize()
	{
		FViewInfo& WriteView = const_cast<FViewInfo&>(View);
		
		for (int32 Index = 0; Index < NotDrawRelevant.NumPrims; Index++)
		{
			WriteView.PrimitiveVisibilityMap[NotDrawRelevant.Prims[Index]] = false;
		}
		WriteView.ShadingModelMaskInView |= CombinedShadingModelMask;
		VisibleEditorPrimitives.AppendTo(WriteView.VisibleEditorPrimitives);
		VisibleDynamicPrimitives.AppendTo(WriteView.VisibleDynamicPrimitives);
		WriteView.TranslucentPrimSet.AppendScenePrimitives(SortedTranslucencyPrims.Prims, SortedTranslucencyPrims.NumPrims, SortedSeparateTranslucencyPrims.Prims, SortedSeparateTranslucencyPrims.NumPrims);
		DistortionPrimSet.AppendTo(WriteView.DistortionPrimSet);
		CustomDepthSet.AppendTo(WriteView.CustomDepthSet);

		for (int32 Index = 0; Index < UpdateStaticMeshes.NumPrims; Index++)
		{
			UpdateStaticMeshes.Prims[Index]->UpdateStaticMeshes(RHICmdList);
		}
	}
};

class FRelevancePacketAnyThreadTask
{
	FRelevancePacket& Packet;
	ENamedThreads::Type ThreadToUse;
public:

	FRelevancePacketAnyThreadTask(FRelevancePacket& InPacket, ENamedThreads::Type InThreadToUse)
		: Packet(InPacket)
		, ThreadToUse(InThreadToUse)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRelevancePacketAnyThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ThreadToUse;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Packet.AnyThreadTask();
	}
};

class FRelevancePacketRenderThreadTask
{
	FRelevancePacket& Packet;
public:

	FRelevancePacketRenderThreadTask(FRelevancePacket& InPacket)
		: Packet(InPacket)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRelevancePacketRenderThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::RenderThread_Local;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Packet.RenderThreadFinalize();
	}
};

static TAutoConsoleVariable<int32> CVarParallelInitViews(
	TEXT("r.ParallelInitViews"),
#if WITH_EDITOR
	0,  
#else
	1,  
#endif
	TEXT("Toggles parallel init views."),
	ECVF_RenderThreadSafe
	);


/**
 * Computes view relevance for visible primitives in the view and adds them to
 * appropriate per-view rendering lists.
 * @param Scene - The scene being rendered.
 * @param View - The view for which to compute relevance.
 * @param ViewBit - Bit mask: 1 << ViewIndex where Views(ViewIndex) == View.
 * @param OutRelevantStaticPrimitives - Upon return contains a list of relevant
 *                                      static primitives.
 *                                callback for this view will have ViewBit set.
 */
static void ComputeAndMarkRelevanceForViewParallel(
	FRHICommandListImmediate& RHICmdList,
	const FScene* Scene,
	FViewInfo& View,
	uint8 ViewBit,
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks,
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks
	)
{
	check(OutHasDynamicMeshElementsMasks.Num() == Scene->Primitives.Num());

	const FMarkRelevantStaticMeshesForViewData ViewData(View);

	int32 NumMesh = View.StaticMeshVisibilityMap.Num();
	check(View.StaticMeshShadowDepthMap.Num() == NumMesh && View.StaticMeshVelocityMap.Num() == NumMesh && View.StaticMeshOccluderMap.Num() == NumMesh);
	uint8* RESTRICT MarkMasks = (uint8*)FMemStack::Get().Alloc(NumMesh + 31 , 8); // some padding to simplify the high speed transpose
	FMemory::Memzero(MarkMasks, NumMesh + 31);

	// I am going to do the render thread tasks in order, maybe that isn't necessary
	FGraphEventRef LastRenderThread;
	{
		FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap);
		if (BitIt)
		{
			int32 AnyThreadTasksPerRenderThreadTasks = 1;

			if (FApp::ShouldUseThreadingForPerformance() && CVarParallelInitViews.GetValueOnRenderThread() > 0)
			{
				AnyThreadTasksPerRenderThreadTasks = FTaskGraphInterface::Get().GetNumWorkerThreads() + 1; // the idea is to put the render thread to work
			}
			// else AnyThreadTasksPerRenderThreadTasks == 1 means we never use a task thread
			int32 WorkingAnyThreadTasksPerRenderThreadTasks = AnyThreadTasksPerRenderThreadTasks;
			

			FRelevancePacket* Packet = new(FMemStack::Get()) FRelevancePacket(
				RHICmdList,
				Scene, 
				View, 
				ViewBit,
				ViewData,
				OutHasDynamicMeshElementsMasks,
				OutHasDynamicEditorMeshElementsMasks,
				MarkMasks);

			while (1)
			{
				Packet->Input.AddPrim(BitIt.GetIndex());
				++BitIt;
				if (Packet->Input.IsFull() || !BitIt)
				{
					// submit task
					ENamedThreads::Type ThreadToUse = ENamedThreads::AnyThread;
					if (!--WorkingAnyThreadTasksPerRenderThreadTasks)
					{
						WorkingAnyThreadTasksPerRenderThreadTasks = AnyThreadTasksPerRenderThreadTasks;
						ThreadToUse = ENamedThreads::RenderThread_Local;
					}
					FGraphEventArray RenderPrereqs;
					if (LastRenderThread.GetReference())
					{
						RenderPrereqs.Add(LastRenderThread); // this puts the render thread ones in order
					}
					FGraphEventRef AnyThread = TGraphTask<FRelevancePacketAnyThreadTask>::CreateTask(nullptr, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(*Packet, ThreadToUse);
					RenderPrereqs.Add(AnyThread);
					LastRenderThread = TGraphTask<FRelevancePacketRenderThreadTask>::CreateTask(&RenderPrereqs, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(*Packet);
					if (!BitIt)
					{
						break;
					}
					else
					{
						Packet = new(FMemStack::Get()) FRelevancePacket(
							RHICmdList,
							Scene, 
							View, 
							ViewBit,
							ViewData,
							OutHasDynamicMeshElementsMasks,
							OutHasDynamicEditorMeshElementsMasks,
							MarkMasks);
					}
				}
			}
		}
	}
	if (LastRenderThread.GetReference())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_Wait);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(LastRenderThread, ENamedThreads::RenderThread_Local);
	}
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_TransposeMeshBits);
	check(View.StaticMeshVelocityMap.Num() == NumMesh && View.StaticMeshShadowDepthMap.Num() == NumMesh && View.StaticMeshVisibilityMap.Num() == NumMesh && View.StaticMeshOccluderMap.Num() == NumMesh);
	uint32* RESTRICT StaticMeshVisibilityMap_Words = View.StaticMeshVisibilityMap.GetData();
	uint32* RESTRICT StaticMeshVelocityMap_Words = View.StaticMeshVelocityMap.GetData();
	uint32* RESTRICT StaticMeshShadowDepthMap_Words = View.StaticMeshShadowDepthMap.GetData();
	uint32* RESTRICT StaticMeshOccluderMap_Words = View.StaticMeshOccluderMap.GetData();
	const uint64* RESTRICT MarkMasks64 = (const uint64* RESTRICT)MarkMasks;
	const uint8* RESTRICT MarkMasks8 = MarkMasks;
	for (int32 BaseIndex = 0; BaseIndex < NumMesh; BaseIndex += 32)
	{
		uint32 StaticMeshVisibilityMap_Word = 0;
		uint32 StaticMeshVelocityMap_Word = 0;
		uint32 StaticMeshShadowDepthMap_Word = 0;
		uint32 StaticMeshOccluderMap_Word = 0;
		uint32 Mask = 1;
		bool bAny = false;
		for (int32 QWordIndex = 0; QWordIndex < 4; QWordIndex++)
		{
			if (*MarkMasks64++)
			{
				for (int32 ByteIndex = 0; ByteIndex < 8; ByteIndex++, Mask <<= 1, MarkMasks8++)
				{
					uint8 MaskMask = *MarkMasks8;
					StaticMeshVisibilityMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshVisibilityMapMask) ? Mask : 0;
					StaticMeshVelocityMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshVelocityMapMask) ? Mask : 0;
					StaticMeshShadowDepthMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshShadowDepthMapMask) ? Mask : 0;
					StaticMeshOccluderMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshOccluderMapMask) ? Mask : 0;
				}
				bAny = true;
			}
			else
			{
				MarkMasks8 += 8;
				Mask <<= 8;
			}
		}
		if (bAny)
		{
			checkSlow(!*StaticMeshVisibilityMap_Words && !*StaticMeshVelocityMap_Words && !*StaticMeshShadowDepthMap_Words && !*StaticMeshOccluderMap_Words);
			*StaticMeshVisibilityMap_Words = StaticMeshVisibilityMap_Word;
			*StaticMeshVelocityMap_Words = StaticMeshVelocityMap_Word;
			*StaticMeshShadowDepthMap_Words = StaticMeshShadowDepthMap_Word;
			*StaticMeshOccluderMap_Words = StaticMeshOccluderMap_Word;
		}
		StaticMeshVisibilityMap_Words++;
		StaticMeshVelocityMap_Words++;
		StaticMeshShadowDepthMap_Words++;
		StaticMeshOccluderMap_Words++;
	}
}

void FSceneRenderer::GatherDynamicMeshElements(
	TArray<FViewInfo>& InViews, 
	const FScene* InScene, 
	const FSceneViewFamily& InViewFamily, 
	const FPrimitiveViewMasks& HasDynamicMeshElementsMasks, 
	const FPrimitiveViewMasks& HasDynamicEditorMeshElementsMasks, 
	FMeshElementCollector& Collector)
{
	SCOPE_CYCLE_COUNTER(STAT_GetDynamicMeshElements);

	int32 NumPrimitives = InScene->Primitives.Num();
	check(HasDynamicMeshElementsMasks.Num() == NumPrimitives);

	{
		Collector.ClearViewMeshArrays();

		for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ViewIndex++)
		{
			Collector.AddViewMeshArrays(&InViews[ViewIndex], &InViews[ViewIndex].DynamicMeshElements, &InViews[ViewIndex].SimpleElementCollector, InViewFamily.GetFeatureLevel());
		}

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			const uint8 ViewMask = HasDynamicMeshElementsMasks[PrimitiveIndex];

			if (ViewMask != 0)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = InScene->Primitives[PrimitiveIndex];
				Collector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);
				PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(InViewFamily.Views, InViewFamily, ViewMask, Collector);
			}
		}
	}

	if (GIsEditor)
	{
		Collector.ClearViewMeshArrays();

		for (int32 ViewIndex = 0; ViewIndex < InViews.Num(); ViewIndex++)
		{
			Collector.AddViewMeshArrays(&InViews[ViewIndex], &InViews[ViewIndex].DynamicEditorMeshElements, &InViews[ViewIndex].EditorSimpleElementCollector, InViewFamily.GetFeatureLevel());
		}

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			const uint8 ViewMask = HasDynamicEditorMeshElementsMasks[PrimitiveIndex];

			if (ViewMask != 0)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = InScene->Primitives[PrimitiveIndex];
				Collector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);
				PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(InViewFamily.Views, InViewFamily, ViewMask, Collector);
			}
		}
	}
}

static void MarkAllPrimitivesForReflectionProxyUpdate(FScene* Scene)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_MarkAllPrimitivesForReflectionProxyUpdate);

	if (Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged)
	{
		// Mark all primitives as needing an update
		// Note: Only visible primitives will actually update their reflection proxy
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->Primitives.Num(); PrimitiveIndex++)
		{
			Scene->Primitives[PrimitiveIndex]->bNeedsCachedReflectionCaptureUpdate = true;
		}

		Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = false;
	}
}

/**
 * Determines which static meshes in the scene are relevant to the view based on
 * for each primitive in RelevantStaticPrimitives.
 */
static void MarkRelevantStaticMeshesForView(
	const FScene* Scene,
	FViewInfo& View,
	const FRelevantStaticPrimitives& RelevantStaticPrimitives
	)
{
	SCOPE_CYCLE_COUNTER(STAT_StaticRelevance);

	const FMarkRelevantStaticMeshesForViewData ViewData(View);

	// using a local counter to reduce memory traffic
	int32 NumVisibleStaticMeshElements = 0;

	for (int32 StaticPrimIndex = 0, Num = RelevantStaticPrimitives.Num(); StaticPrimIndex < Num; ++StaticPrimIndex)
	{
		int32 PrimitiveIndex = RelevantStaticPrimitives[StaticPrimIndex];
		const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = Scene->Primitives[PrimitiveIndex];
		const FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[PrimitiveIndex];
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];

		int8 LODToRender = ComputeLODForMeshes( PrimitiveSceneInfo->StaticMeshes, View, Bounds.Origin, Bounds.SphereRadius, ViewData.ForcedLODLevel, ViewData.LODScale);

		float DistanceSquared = (Bounds.Origin - ViewData.ViewOrigin).SizeSquared();
		const float LODFactorDistanceSquared = DistanceSquared * FMath::Square(View.LODDistanceFactor * ViewData.InvLODScale);
		const bool bDrawShadowDepth = FMath::Square(Bounds.SphereRadius) > ViewData.MinScreenRadiusForCSMDepthSquared * LODFactorDistanceSquared;
		const bool bDrawDepthOnly = ViewData.bForceEarlyZPass || FMath::Square(Bounds.SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared;
		
		// We could compute this only if required by the following code. Not sure if that is worth.
		const bool bShouldRenderVelocity = PrimitiveSceneInfo->ShouldRenderVelocity(View);

		const int32 NumStaticMeshes = PrimitiveSceneInfo->StaticMeshes.Num();
		for(int32 MeshIndex = 0;MeshIndex < NumStaticMeshes;MeshIndex++)
		{
			const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
			if (StaticMesh.LODIndex == LODToRender)
			{
				bool bNeedsBatchVisibility = false;

				if (ViewRelevance.bShadowRelevance && bDrawShadowDepth && StaticMesh.CastShadow)
				{
					// Mark static mesh as visible in shadows.
					View.StaticMeshShadowDepthMap[StaticMesh.Id] = true;
					bNeedsBatchVisibility = true;
				}
				
				if(ViewRelevance.bDrawRelevance && !StaticMesh.bShadowOnly && (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth))
				{
					// Mark static mesh as visible for rendering
					View.StaticMeshVisibilityMap[StaticMesh.Id] = true;
					View.StaticMeshVelocityMap[StaticMesh.Id] = bShouldRenderVelocity;
					++NumVisibleStaticMeshElements;

					// If the static mesh is an occluder, check whether it covers enough of the screen to be used as an occluder.
					if(	StaticMesh.bUseAsOccluder && bDrawDepthOnly )
					{
						View.StaticMeshOccluderMap[StaticMesh.Id] = true;
					}
					bNeedsBatchVisibility = true;
				}

				// Static meshes with a single element always draw, as if the mask were 0x1.
				if(bNeedsBatchVisibility && StaticMesh.Elements.Num() > 1)
				{
					View.StaticMeshBatchVisibility[StaticMesh.Id] = StaticMesh.VertexFactory->GetStaticBatchElementVisibility(View, &StaticMesh);
				}
			}
		}
	}

	View.NumVisibleStaticMeshElements += NumVisibleStaticMeshElements;
}

/**
 * Helper for InitViews to detect large camera movement, in both angle and position.
 */
static bool IsLargeCameraMovement(FSceneView& View, const FMatrix& PrevViewMatrix, const FVector& PrevViewOrigin, float CameraRotationThreshold, float CameraTranslationThreshold)
{
	float RotationThreshold = FMath::Cos(CameraRotationThreshold * PI / 180.0f);
	float ViewRightAngle = View.ViewMatrices.ViewMatrix.GetColumn(0) | PrevViewMatrix.GetColumn(0);
	float ViewUpAngle = View.ViewMatrices.ViewMatrix.GetColumn(1) | PrevViewMatrix.GetColumn(1);
	float ViewDirectionAngle = View.ViewMatrices.ViewMatrix.GetColumn(2) | PrevViewMatrix.GetColumn(2);

	FVector Distance = FVector(View.ViewMatrices.ViewOrigin) - PrevViewOrigin;
	return 
		ViewRightAngle < RotationThreshold ||
		ViewUpAngle < RotationThreshold ||
		ViewDirectionAngle < RotationThreshold ||
		Distance.SizeSquared() > CameraTranslationThreshold * CameraTranslationThreshold;
}

float Halton( int32 Index, int32 Base )
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while( Index > 0 )
	{
		Result += ( Index % Base ) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

void FSceneRenderer::PreVisibilityFrameSetup(FRHICommandListImmediate& RHICmdList)
{
	// Notify the RHI we are beginning to render a scene.
	RHICmdList.BeginScene();

	// Notify the FX system that the scene is about to perform visibility checks.
	if (Scene->FXSystem)
	{
		Scene->FXSystem->PreInitViews();
	}

	// Draw lines to lights affecting this mesh if its selected.
	if (ViewFamily.EngineShowFlags.LightInfluences)
	{
		for (TArray<FPrimitiveSceneInfo*>::TConstIterator It(Scene->Primitives); It; ++It)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = *It;
			if (PrimitiveSceneInfo->Proxy->IsSelected())
			{
				FLightPrimitiveInteraction *LightList = PrimitiveSceneInfo->LightList;
				while (LightList)
				{
					const FLightSceneInfo* LightSceneInfo = LightList->GetLight();

					bool bDynamic = true;
					bool bRelevant = false;
					bool bLightMapped = true;
					bool bShadowMapped = false;
					PrimitiveSceneInfo->Proxy->GetLightRelevance(LightSceneInfo->Proxy, bDynamic, bRelevant, bLightMapped, bShadowMapped);

					if (bRelevant)
					{
						// Draw blue for light-mapped lights and orange for dynamic lights
						const FColor LineColor = bLightMapped ? FColor(0,140,255) : FColor(255,140,0);
						for (int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
						{
							FViewInfo& View = Views[ViewIndex];
							FViewElementPDI LightInfluencesPDI(&View,NULL);
							LightInfluencesPDI.DrawLine(PrimitiveSceneInfo->Proxy->GetBounds().Origin, LightSceneInfo->Proxy->GetLightToWorld().GetOrigin(), LineColor, SDPG_World);
						}
					}
					LightList = LightList->GetNextLight();
				}
			}
		}
	}

	// Setup motion blur parameters (also check for camera movement thresholds)
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*) View.State;
		static bool bEnableTimeScale = true;

		// Once per render increment the occlusion frame counter.
		if (ViewState)
		{
			ViewState->OcclusionFrameCounter++;
		}

		// HighResScreenshot should get best results so we don't do the occlusion optimization based on the former frame
		extern bool GIsHighResScreenshot;
		const bool bIsHitTesting = ViewFamily.EngineShowFlags.HitProxies;
		if (GIsHighResScreenshot || !DoOcclusionQueries(FeatureLevel) || bIsHitTesting)
		{
			View.bDisableQuerySubmissions = true;
			View.bIgnoreExistingQueries = true;
		}

		// set up the screen area for occlusion
		float NumPossiblePixels = GSceneRenderTargets.UseDownsizedOcclusionQueries() && IsValidRef(GSceneRenderTargets.GetSmallDepthSurface()) ? 
			(float)View.ViewRect.Width() / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor() * (float)View.ViewRect.Height() / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor() :
			View.ViewRect.Width() * View.ViewRect.Height();
		View.OneOverNumPossiblePixels = NumPossiblePixels > 0.0 ? 1.0f / NumPossiblePixels : 0.0f;

		// Still need no jitter to be set for temporal feedback on SSR (it is enabled even when temporal AA is off).
		View.TemporalJitterPixelsX = 0.0f;
		View.TemporalJitterPixelsY = 0.0f;

		if( View.FinalPostProcessSettings.AntiAliasingMethod == AAM_TemporalAA && ViewState )
		{
			// Subpixel jitter for temporal AA
			int32 TemporalAASamples = CVarTemporalAASamples.GetValueOnRenderThread();
		
			if( TemporalAASamples > 1 )
			{
				float SampleX, SampleY;

				if (Scene->GetFeatureLevel() < ERHIFeatureLevel::SM4)
				{
					// Only support 2 samples for mobile temporal AA.
					TemporalAASamples = 2;
				}

				if( TemporalAASamples == 2 )
				{
					#if 0
						// 2xMSAA
						// Pattern docs: http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx
						//   N.
						//   .S
						float SamplesX[] = { -4.0f/16.0f, 4.0/16.0f };
						float SamplesY[] = { -4.0f/16.0f, 4.0/16.0f };
					#else
						// This pattern is only used for mobile.
						// Shift to reduce blur.
						float SamplesX[] = { -8.0f/16.0f, 0.0/16.0f };
						float SamplesY[] = { /* - */ 0.0f/16.0f, 8.0/16.0f };
					#endif
					ViewState->SetupTemporalAA(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 3 )
				{
					// 3xMSAA
					//   A..
					//   ..B
					//   .C.
					// Rolling circle pattern (A,B,C).
					float SamplesX[] = { -2.0f/3.0f,  2.0/3.0f,  0.0/3.0f };
					float SamplesY[] = { -2.0f/3.0f,  0.0/3.0f,  2.0/3.0f };
					ViewState->SetupTemporalAA(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 4 )
				{
					// 4xMSAA
					// Pattern docs: http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx
					//   .N..
					//   ...E
					//   W...
					//   ..S.
					// Rolling circle pattern (N,E,S,W).
					float SamplesX[] = { -2.0f/16.0f,  6.0/16.0f, 2.0/16.0f, -6.0/16.0f };
					float SamplesY[] = { -6.0f/16.0f, -2.0/16.0f, 6.0/16.0f,  2.0/16.0f };
					ViewState->SetupTemporalAA(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 5 )
				{
					// Compressed 4 sample pattern on same vertical and horizontal line (less temporal flicker).
					// Compressed 1/2 works better than correct 2/3 (reduced temporal flicker).
					//   . N .
					//   W . E
					//   . S .
					// Rolling circle pattern (N,E,S,W).
					float SamplesX[] = {  0.0f/2.0f,  1.0/2.0f,  0.0/2.0f, -1.0/2.0f };
					float SamplesY[] = { -1.0f/2.0f,  0.0/2.0f,  1.0/2.0f,  0.0/2.0f };
					ViewState->SetupTemporalAA(ARRAY_COUNT(SamplesX), ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = SamplesX[ Index ];
					SampleY = SamplesY[ Index ];
				}
				else if( TemporalAASamples == 8 )
				{
					// This works better than various orderings of 8xMSAA.
					ViewState->SetupTemporalAA(8, ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = Halton( Index, 2 ) - 0.5f;
					SampleY = Halton( Index, 3 ) - 0.5f;
				}
				else
				{
					// More than 8 samples can improve quality.
					ViewState->SetupTemporalAA(TemporalAASamples, ViewFamily);
					uint32 Index = ViewState->GetCurrentTemporalAASampleIndex();
					SampleX = Halton( Index, 2 ) - 0.5f;
					SampleY = Halton( Index, 3 ) - 0.5f;
				}

				View.ViewMatrices.TemporalAASample.X = SampleX;
				View.ViewMatrices.TemporalAASample.Y = SampleY;

				View.ViewMatrices.ProjMatrix.M[2][0] += View.ViewMatrices.TemporalAASample.X * 2.0f / View.ViewRect.Width();
				View.ViewMatrices.ProjMatrix.M[2][1] += View.ViewMatrices.TemporalAASample.Y * 2.0f / View.ViewRect.Height();;

				// Compute the view projection matrix and its inverse.
				View.ViewProjectionMatrix = View.ViewMatrices.ViewMatrix * View.ViewMatrices.ProjMatrix;
				View.InvViewProjectionMatrix = View.ViewMatrices.GetInvProjMatrix() * View.InvViewMatrix;

				/** The view transform, starting from world-space points translated by -ViewOrigin. */
				FMatrix TranslatedViewMatrix = FTranslationMatrix(-View.ViewMatrices.PreViewTranslation) * View.ViewMatrices.ViewMatrix;

				// Compute a transform from view origin centered world-space to clip space.
				View.ViewMatrices.TranslatedViewProjectionMatrix = TranslatedViewMatrix * View.ViewMatrices.ProjMatrix;
				View.ViewMatrices.InvTranslatedViewProjectionMatrix = View.ViewMatrices.TranslatedViewProjectionMatrix.Inverse();
			}
		}
		else if(ViewState)
		{
			// no TemporalAA
			ViewState->SetupTemporalAA(1, ViewFamily);
		}

		if ( ViewState )
		{
			// In case world origin was rebased, reset previous view transformations
			if (View.bOriginOffsetThisFrame)
			{
				ViewState->PrevViewMatrices = View.ViewMatrices;
				ViewState->PendingPrevViewMatrices = View.ViewMatrices;
			}
			
			// determine if we are initializing or we should reset the persistent state
			const float DeltaTime = View.Family->CurrentRealTime - ViewState->LastRenderTime;
			const bool bFirstFrameOrTimeWasReset = DeltaTime < -0.0001f || ViewState->LastRenderTime < 0.0001f;

			// detect conditions where we should reset occlusion queries
			if (bFirstFrameOrTimeWasReset || 
				ViewState->LastRenderTime + GEngine->PrimitiveProbablyVisibleTime < View.Family->CurrentRealTime ||
				View.bCameraCut ||
				IsLargeCameraMovement(
					View, 
				    ViewState->PrevViewMatrixForOcclusionQuery, 
				    ViewState->PrevViewOriginForOcclusionQuery, 
				    GEngine->CameraRotationThreshold, GEngine->CameraTranslationThreshold))
			{
				View.bIgnoreExistingQueries = true;
				View.bDisableDistanceBasedFadeTransitions = true;
			}
			ViewState->PrevViewMatrixForOcclusionQuery = View.ViewMatrices.ViewMatrix;
			ViewState->PrevViewOriginForOcclusionQuery = View.ViewMatrices.ViewOrigin;
				
			// store old view matrix and detect conditions where we should reset motion blur 
			{
				bool bResetCamera = bFirstFrameOrTimeWasReset
					|| View.bCameraCut
					|| IsLargeCameraMovement(View, ViewState->PrevViewMatrices.ViewMatrix, ViewState->PrevViewMatrices.ViewOrigin, 45.0f, 10000.0f);

				if (bResetCamera)
				{
					ViewState->PrevViewMatrices = View.ViewMatrices;

					ViewState->PendingPrevViewMatrices = ViewState->PrevViewMatrices;

					// PT: If the motion blur shader is the last shader in the post-processing chain then it is the one that is
					//     adjusting for the viewport offset.  So it is always required and we can't just disable the work the
					//     shader does.  The correct fix would be to disable the effect when we don't need it and to properly mark
					//     the uber-postprocessing effect as the last effect in the chain.

					View.bPrevTransformsReset				= true;
				}
				else
				{
					ViewState->PrevViewMatrices = ViewState->PendingPrevViewMatrices;

					// check for pause so we can keep motion blur in paused mode (doesn't work in editor)
					if(!ViewFamily.bWorldIsPaused)
					{
						// pending is needed as we are in init view and still need to render.
						ViewState->PendingPrevViewMatrices = View.ViewMatrices;
					}
				}
				// we don't use DeltaTime as it can be 0 (in editor) and is computed by subtracting floats (loses precision over time)
				// Clamp DeltaWorldTime to reasonable values for the purposes of motion blur, things like TimeDilation can make it very small
				if (!ViewFamily.bWorldIsPaused)
				{
					ViewState->MotionBlurTimeScale			= bEnableTimeScale ? (1.0f / (FMath::Max(View.Family->DeltaWorldTime, .00833f) * 30.0f)) : 1.0f;
				}

				View.PrevViewMatrices = ViewState->PrevViewMatrices;

				View.PrevViewProjMatrix = ViewState->PrevViewMatrices.GetViewProjMatrix();
				View.PrevViewRotationProjMatrix = ViewState->PrevViewMatrices.GetViewRotationProjMatrix();
			}

			ViewState->PrevFrameNumber = ViewState->PendingPrevFrameNumber;
			ViewState->PendingPrevFrameNumber = View.Family->FrameNumber;

			// This finishes the update of view state
			ViewState->UpdateLastRenderTime(*View.Family);

			ViewState->UpdateTemporalLODTransition(View);
		}
	}
}

static TAutoConsoleVariable<int32> CVarLegacySingleThreadedRelevance(
	TEXT("r.LegacySingleThreadedRelevance"),
	0,  
	TEXT("Toggles the legacy codepath for view relevance."),
	ECVF_RenderThreadSafe
	);

void FSceneRenderer::ComputeViewVisibility(FRHICommandListImmediate& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime);

	STAT(int32 NumProcessedPrimitives = 0);
	STAT(int32 NumCulledPrimitives = 0);
	STAT(int32 NumOccludedPrimitives = 0);

	// Allocate the visible light info.
	if (Scene->Lights.GetMaxIndex() > 0)
	{
		VisibleLightInfos.AddZeroed(Scene->Lights.GetMaxIndex());
	}

	int32 NumPrimitives = Scene->Primitives.Num();
	float CurrentRealTime = ViewFamily.CurrentRealTime;

	FPrimitiveViewMasks HasDynamicMeshElementsMasks;
	HasDynamicMeshElementsMasks.AddZeroed(NumPrimitives);

	FPrimitiveViewMasks HasDynamicEditorMeshElementsMasks;

	if (GIsEditor)
	{
		HasDynamicEditorMeshElementsMasks.AddZeroed(NumPrimitives);
	}

	uint8 ViewBit = 0x1;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex, ViewBit <<= 1)
	{
		STAT(NumProcessedPrimitives += NumPrimitives);

		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = (FSceneViewState*)View.State;

		// Allocate the view's visibility maps.
		View.PrimitiveVisibilityMap.Init(false,Scene->Primitives.Num());
		View.PrimitiveDefinitelyUnoccludedMap.Init(false,Scene->Primitives.Num());
		View.PotentiallyFadingPrimitiveMap.Init(false,Scene->Primitives.Num());
		View.PrimitiveFadeUniformBuffers.AddZeroed(Scene->Primitives.Num());
		View.StaticMeshVisibilityMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshOccluderMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshVelocityMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshShadowDepthMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
		View.StaticMeshBatchVisibility.AddZeroed(Scene->StaticMeshes.GetMaxIndex());

		View.VisibleLightInfos.Empty(Scene->Lights.GetMaxIndex());

		for(int32 LightIndex = 0;LightIndex < Scene->Lights.GetMaxIndex();LightIndex++)
		{
			if( LightIndex+2 < Scene->Lights.GetMaxIndex() )
			{
				if (LightIndex > 2)
				{
					FLUSH_CACHE_LINE(&View.VisibleLightInfos(LightIndex-2));
				}
				// @todo optimization These prefetches cause asserts since LightIndex > View.VisibleLightInfos.Num() - 1
				//FPlatformMisc::Prefetch(&View.VisibleLightInfos[LightIndex+2]);
				//FPlatformMisc::Prefetch(&View.VisibleLightInfos[LightIndex+1]);
			}
			new(View.VisibleLightInfos) FVisibleLightViewInfo();
		}

		View.PrimitiveViewRelevanceMap.Empty(Scene->Primitives.Num());
		View.PrimitiveViewRelevanceMap.AddZeroed(Scene->Primitives.Num());

		// If this is the visibility-parent of other views, reset its ParentPrimitives list.
		const bool bIsParent = ViewState && ViewState->IsViewParent();
		if ( bIsParent )
		{
			// PVS-Studio does not understand the validation of ViewState above, so we're disabling
			// its warning that ViewState may be null:
			ViewState->ParentPrimitives.Empty(); //-V595
		}

		if (ViewState)
		{	
			SCOPE_CYCLE_COUNTER(STAT_DecompressPrecomputedOcclusion);
			View.PrecomputedVisibilityData = ViewState->GetPrecomputedVisibilityData(View, Scene);
		}
		else
		{
			View.PrecomputedVisibilityData = NULL;
		}

		if (View.PrecomputedVisibilityData)
		{
			bUsedPrecomputedVisibility = true;
		}

		bool bNeedsFrustumCulling = true;

		// Development builds sometimes override frustum culling, e.g. dependent views in the editor.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if( ViewState )
		{
#if WITH_EDITOR
			// For visibility child views, check if the primitive was visible in the parent view.
			const FSceneViewState* const ViewParent = (FSceneViewState*)ViewState->GetViewParent();
			if(ViewParent)
			{
				bNeedsFrustumCulling = false;
				for (FSceneBitArray::FIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					if (ViewParent->ParentPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
					{
						BitIt.GetValue() = true;
					}
				}
			}
#endif
			// For views with frozen visibility, check if the primitive is in the frozen visibility set.
			if(ViewState->bIsFrozen)
			{
				bNeedsFrustumCulling = false;
				for (FSceneBitArray::FIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					if (ViewState->FrozenPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
					{
						BitIt.GetValue() = true;
					}
				}
			}
		}
#endif

		// Most views use standard frustum culling.
		if (bNeedsFrustumCulling)
		{
			int32 NumCulledPrimitivesForView;
			if (View.CustomVisibilityQuery && View.CustomVisibilityQuery->Prepare())
			{
				NumCulledPrimitivesForView = FrustumCull<true>(Scene, View);
			}
			else
			{
				NumCulledPrimitivesForView = FrustumCull<false>(Scene, View);
			}
			STAT(NumCulledPrimitives += NumCulledPrimitivesForView);
			UpdatePrimitiveFading(Scene, View);			
		}

		// If any primitives are explicitly hidden, remove them now.
		if (View.HiddenPrimitives.Num())
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				if (View.HiddenPrimitives.Contains(Scene->PrimitiveComponentIds[BitIt.GetIndex()]))
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		if (View.bStaticSceneOnly)
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				// Reflection captures should only capture objects that won't move, since reflection captures won't update at runtime
				if (!Scene->Primitives[BitIt.GetIndex()]->Proxy->HasStaticLighting())
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		// Cull small objects in wireframe in ortho views
		// This is important for performance in the editor because wireframe disables any kind of occlusion culling
		if (View.Family->EngineShowFlags.Wireframe)
		{
			float ScreenSizeScale = FMath::Max(View.ViewMatrices.ProjMatrix.M[0][0] * View.ViewRect.Width(), View.ViewMatrices.ProjMatrix.M[1][1] * View.ViewRect.Height());
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				if (ScreenSizeScale * Scene->PrimitiveBounds[BitIt.GetIndex()].SphereRadius <= GWireframeCullThreshold)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		// Occlusion cull for all primitives in the view frustum, but not in wireframe.
		if (!View.Family->EngineShowFlags.Wireframe)
		{
			int32 NumOccludedPrimitivesInView = OcclusionCull(RHICmdList, Scene, View);
			STAT(NumOccludedPrimitives += NumOccludedPrimitivesInView);
		}

		// visibility test is done, so now build the hidden flags based on visibility set up
		bool bLODSceneTreeActive = Scene->SceneLODHierarchy.IsActive();
		FSceneBitArray PrimitiveHiddenByLODMap;
		if (bLODSceneTreeActive)
		{
			PrimitiveHiddenByLODMap.Init(false, View.PrimitiveVisibilityMap.Num());
			Scene->SceneLODHierarchy.PopulateHiddenFlags(View, PrimitiveHiddenByLODMap);

			// now iterate through turn off visibility if hidden by LOD
			for(FSceneSetBitIterator BitIt(PrimitiveHiddenByLODMap); BitIt; ++BitIt)
			{
				if(PrimitiveHiddenByLODMap.AccessCorrespondingBit(BitIt))
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
				}
			}
		}

		MarkAllPrimitivesForReflectionProxyUpdate(Scene);
		Scene->ConditionalMarkStaticMeshElementsForUpdate();

		{
			SCOPE_CYCLE_COUNTER(STAT_ViewRelevance);
			// Compute view relevance for all visible primitives.
			if (!CVarLegacySingleThreadedRelevance.GetValueOnRenderThread())
			{
				ComputeAndMarkRelevanceForViewParallel(RHICmdList, Scene, View, ViewBit, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks);
			}
			else
			{
				// This array contains a list of relevant static primities.
				FRelevantStaticPrimitives RelevantStaticPrimitives;
				ComputeRelevanceForView(RHICmdList, Scene, View, ViewBit, RelevantStaticPrimitives, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks);
				MarkRelevantStaticMeshesForView(Scene, View, RelevantStaticPrimitives);
			}

			if (bLODSceneTreeActive)
			{
				for (FSceneBitArray::FIterator BitIt(PrimitiveHiddenByLODMap); BitIt; ++BitIt)
				{
					View.PrimitiveViewRelevanceMap[BitIt.GetIndex()].bInitializedThisFrame = true;
				}
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Store the primitive for parent occlusion rendering.
		if (FPlatformProperties::SupportsWindowedMode() && ViewState && ViewState->IsViewParent())
		{
			for (FSceneDualSetBitIterator BitIt(View.PrimitiveVisibilityMap, View.PrimitiveDefinitelyUnoccludedMap); BitIt; ++BitIt)
			{
				ViewState->ParentPrimitives.Add(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
			}
		}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// if we are freezing the scene, then remember the primitives that are rendered.
		if (ViewState && ViewState->bIsFreezing)
		{
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
			{
				ViewState->FrozenPrimitives.Add(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
			}
		}
#endif
	}

	GatherDynamicMeshElements(Views, Scene, ViewFamily, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks, MeshCollector);

	INC_DWORD_STAT_BY(STAT_ProcessedPrimitives,NumProcessedPrimitives);
	INC_DWORD_STAT_BY(STAT_CulledPrimitives,NumCulledPrimitives);
	INC_DWORD_STAT_BY(STAT_OccludedPrimitives,NumOccludedPrimitives);
}

void FSceneRenderer::PostVisibilityFrameSetup()
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];

		// sort the translucent primitives
		View.TranslucentPrimSet.SortPrimitives();

		if (View.State)
		{
			((FSceneViewState*)View.State)->TrimHistoryRenderTargets(Scene);
		}
	}

	bool bCheckLightShafts = false;
	if (Scene->GetFeatureLevel() <= ERHIFeatureLevel::ES3_1)
	{
		// Clear the mobile light shaft data.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{		
			FViewInfo& View = Views[ViewIndex];
			View.bLightShaftUse = false;
			View.LightShaftCenter.X = 0.0f;
			View.LightShaftCenter.Y = 0.0f;
			View.LightShaftColorMask = FLinearColor(0.0f,0.0f,0.0f);
			View.LightShaftColorApply = FLinearColor(0.0f,0.0f,0.0f);
		}
		
		bCheckLightShafts = ViewFamily.EngineShowFlags.LightShafts && CVarLightShaftQuality.GetValueOnRenderThread() > 0;
	}

	if (ViewFamily.EngineShowFlags.HitProxies == 0)
	{
		Scene->IndirectLightingCache.UpdateCache(Scene, *this, true);
	}

	// determine visibility of each light
	for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		// view frustum cull lights in each view
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{		
			const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;
			FViewInfo& View = Views[ViewIndex];
			FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightIt.GetIndex()];
			// dir lights are always visible, and point/spot only if in the frustum
			if (Proxy->GetLightType() == LightType_Point  
				|| Proxy->GetLightType() == LightType_Spot)
			{
				const float Radius = Proxy->GetRadius();

				if (View.ViewFrustum.IntersectSphere(Proxy->GetOrigin(), Radius))
				{
					FSphere Bounds = Proxy->GetBoundingSphere();
					float DistanceSquared = (Bounds.Center - View.ViewMatrices.ViewOrigin).SizeSquared();
					const bool bDrawLight = FMath::Square( FMath::Min( 0.0002f, GMinScreenRadiusForLights / Bounds.W ) * View.LODDistanceFactor ) * DistanceSquared < 1.0f;
					VisibleLightViewInfo.bInViewFrustum = bDrawLight;
				}
			}
			else
			{
				VisibleLightViewInfo.bInViewFrustum = true;

				static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
				bool bNotMobileMSAA = !(CVarMobileMSAA ? CVarMobileMSAA->GetValueOnRenderThread() > 1 : false);

				// Setup single sun-shaft from direction lights for mobile.
				if(bCheckLightShafts && LightSceneInfo->bEnableLightShaftBloom)
				{
					// Find directional light for sun shafts.
					// Tweaked values from UE3 implementation.
					const float PointLightFadeDistanceIncrease = 200.0f;
					const float PointLightRadiusFadeFactor = 5.0f;

					const FVector WorldSpaceBlurOrigin = LightSceneInfo->Proxy->GetPosition();
					// Transform into post projection space
					FVector4 ProjectedBlurOrigin = View.WorldToScreen(WorldSpaceBlurOrigin);

					const float DistanceToBlurOrigin = (View.ViewMatrices.ViewOrigin - WorldSpaceBlurOrigin).Size() + PointLightFadeDistanceIncrease;

					// Don't render if the light's origin is behind the view
					if(ProjectedBlurOrigin.W >= 0.0f
						// Don't render point lights that have completely faded out
							&& (LightSceneInfo->Proxy->GetLightType() == LightType_Directional 
							|| DistanceToBlurOrigin < LightSceneInfo->Proxy->GetRadius() * PointLightRadiusFadeFactor))
					{
						View.bLightShaftUse = bNotMobileMSAA;
						View.LightShaftCenter.X = ProjectedBlurOrigin.X / ProjectedBlurOrigin.W;
						View.LightShaftCenter.Y = ProjectedBlurOrigin.Y / ProjectedBlurOrigin.W;
						// TODO: Might want to hookup different colors for these.
						View.LightShaftColorMask = LightSceneInfo->BloomTint;
						View.LightShaftColorApply = LightSceneInfo->BloomTint;
					}
				}
			}

			// Draw shapes for reflection captures
			if( View.bIsReflectionCapture && VisibleLightViewInfo.bInViewFrustum && Proxy->HasStaticLighting() && Proxy->GetLightType() != LightType_Directional )
			{
				FVector Origin = Proxy->GetOrigin();
				FVector ToLight = Origin - View.ViewMatrices.ViewOrigin;
				float DistanceSqr = ToLight | ToLight;
				float Radius = Proxy->GetRadius();

				if( DistanceSqr < Radius * Radius )
				{
					FVector4	PositionAndInvRadius;
					FVector4	ColorAndFalloffExponent;
					FVector		Direction;
					FVector2D	SpotAngles;
					float		SourceRadius;
					float		SourceLength;
					float		MinRoughness;
					Proxy->GetParameters( PositionAndInvRadius, ColorAndFalloffExponent, Direction, SpotAngles, SourceRadius, SourceLength, MinRoughness );

					// Force to be at least 0.75 pixels
					float CubemapSize = 128.0f;
					float Distance = FMath::Sqrt( DistanceSqr );
					float MinRadius = Distance * 0.75f / CubemapSize;
					SourceRadius = FMath::Max( MinRadius, SourceRadius );

					// Snap to cubemap pixel center to reduce aliasing
					FVector Scale = ToLight.GetAbs();
					int32 MaxComponent = Scale.X > Scale.Y ? ( Scale.X > Scale.Z ? 0 : 2 ) : ( Scale.Y > Scale.Z ? 1 : 2 );
					for( int32 k = 1; k < 3; k++ )
					{
						float Projected = ToLight[ (MaxComponent + k) % 3 ] / Scale[ MaxComponent ];
						float Quantized = ( FMath::RoundToFloat( Projected * (0.5f * CubemapSize) - 0.5f ) + 0.5f ) / (0.5f * CubemapSize);
						ToLight[ (MaxComponent + k) % 3 ] = Quantized * Scale[ MaxComponent ];
					}
					Origin = ToLight + View.ViewMatrices.ViewOrigin;
				
					FLinearColor Color( ColorAndFalloffExponent );

					if( Proxy->IsInverseSquared() )
					{
						float LightRadiusMask = FMath::Square( 1.0f - FMath::Square( DistanceSqr * FMath::Square( PositionAndInvRadius.W ) ) );
						Color *= LightRadiusMask;
						
						// Correction for lumen units
						Color *= 16.0f;
					}
					else
					{
						// Remove inverse square falloff
						Color *= DistanceSqr + 1.0f;

						// Apply falloff
						Color *= FMath::Pow( 1.0f - DistanceSqr * FMath::Square( PositionAndInvRadius.W ), ColorAndFalloffExponent.W ); 
					}

					// Spot falloff
					FVector L = ToLight.GetSafeNormal();
					Color *= FMath::Square( FMath::Clamp( ( (L | Direction) - SpotAngles.X ) * SpotAngles.Y, 0.0f, 1.0f ) );

					// Scale by visible area
					Color /= PI * FMath::Square( SourceRadius );
				
					// Always opaque
					Color.A = 1.0f;
				
					FViewElementPDI LightPDI( &View, NULL );
					FMaterialRenderProxy* const ColoredMeshInstance = new(FMemStack::Get()) FColoredMaterialRenderProxy( GEngine->DebugMeshMaterial->GetRenderProxy(false), Color );
					DrawSphere( &LightPDI, Origin, FVector( SourceRadius, SourceRadius, SourceRadius ), 8, 6, ColoredMeshInstance, SDPG_World );
				}
			}
		}
	}

	// Initialize the fog constants.
	InitFogConstants();
	InitAtmosphereConstants();
}

uint32 GetShadowQuality();

/**
 * Initialize scene's views.
 * Check visibility, sort translucent items, etc.
 */
void FDeferredShadingSceneRenderer::InitViews(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, InitViews);

	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];

		if (!GPostProcessing.AllowFullPostProcessing(View, FeatureLevel))
		{
			// Disable anti-aliasing if we are not going to be able to apply final post process effects
			View.FinalPostProcessSettings.AntiAliasingMethod = AAM_None;
		}
	}

	PreVisibilityFrameSetup(RHICmdList);
	ComputeViewVisibility(RHICmdList);
	PostVisibilityFrameSetup();

	FVector AverageViewPosition(0);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{		
		FViewInfo& View = Views[ViewIndex];
		AverageViewPosition += View.ViewMatrices.ViewOrigin / Views.Num();
	}

	SortBasePassStaticData(AverageViewPosition);

	bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;

	if (bDynamicShadows && !IsSimpleDynamicLightingEnabled())
	{
		// Setup dynamic shadows.
		InitDynamicShadows(RHICmdList);
	}

	if (ViewFamily.bWorldIsPaused)
	{
		// so we can freeze motion blur and TemporalAA in paused mode

		// per object velocity (static meshes)
		Scene->MotionBlurInfoData.RestoreForPausedMotionBlur();
		// per bone velocity (skeletal meshes)
		GPrevPerBoneMotionBlur.RestoreForPausedMotionBlur();
	}

	// initialize per-view uniform buffer.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		// Initialize the view's RHI resources.
		Views[ViewIndex].InitRHIResources(nullptr);
	}	

	OnStartFrame();
}

/*------------------------------------------------------------------------------
	FLODSceneTree Implementation
------------------------------------------------------------------------------*/
void FLODSceneTree::AddChildNode(const FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* ChildSceneInfo)
{
	if (NodeId.IsValid() && ChildSceneInfo)
	{
		FLODSceneNode* Node = SceneNodes.Find(NodeId);

		if(!Node)
		{
			Node = &SceneNodes.Add(NodeId, FLODSceneNode());

			// scene info can be added later depending on order of adding to the scene
			// but at least add componentId, that way when parent is added, it will add its info properly
			int32 ParentIndex = Scene->PrimitiveComponentIds.Find(NodeId);
			if(Scene->Primitives.IsValidIndex(ParentIndex))
			{
				Node->SceneInfo = Scene->Primitives[ParentIndex];
			}
		}

		Node->AddChild(ChildSceneInfo);
	}
}

void FLODSceneTree::RemoveChildNode(const FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* ChildSceneInfo)
{
	if(NodeId.IsValid() && ChildSceneInfo)
	{
		FLODSceneNode* Node = SceneNodes.Find(NodeId);
		if (Node)
		{
			Node->RemoveChild(ChildSceneInfo);

			// delete from scene if no children remains
			if(Node->ChildrenSceneInfos.Num() == 0)
			{
				SceneNodes.Remove(NodeId);
			}
		}
	}
}

void FLODSceneTree::UpdateNodeSceneInfo(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* SceneInfo)
{
	FLODSceneNode* Node = SceneNodes.Find(NodeId);
	if(Node)
	{
		Node->SceneInfo = SceneInfo;
	}
}

void FLODSceneTree::PopulateHiddenFlagsToChildren(FSceneBitArray& HiddenFlags, FLODSceneNode& Node)
{
	// if already updated, no reason to do this
	if(Node.LatestUpdateCount != UpdateCount)
	{
		Node.LatestUpdateCount = UpdateCount;
		// if node doesn't have scene info, that means it doesn't to populate, children is disconnected, so don't bother
		// in this case we still update children when this node is missing scene info
		// because parent is showing, so you don't have to show anyway anybody below
		// although this node might be MIA at this moment
		for(const auto& Child : Node.ChildrenSceneInfos)
		{
			const int32 ChildIndex = Child->GetIndex();

			// first update the flags
			FRelativeBitReference BitRef(ChildIndex);
			HiddenFlags.AccessCorrespondingBit(BitRef) = true;
			// find the node for it
			FLODSceneNode* ChildNode = SceneNodes.Find(Child->PrimitiveComponentId);
			// if you have child, populate it again, 
			if (ChildNode)
			{
				PopulateHiddenFlagsToChildren(HiddenFlags, *ChildNode);
			}
		}
	}
}

void FLODSceneTree::PopulateHiddenFlags(FViewInfo& View, FSceneBitArray& HiddenFlags)
{
	++UpdateCount;

	// @todo this is experimental code - hide the children if parent is showing
	for(auto Iter = SceneNodes.CreateIterator(); Iter; ++Iter)
	{
		FLODSceneNode& Node = Iter.Value();
		// if already updated, no reason to do this
		if(Node.LatestUpdateCount != UpdateCount)
		{
			Node.LatestUpdateCount = UpdateCount;
			// if node doesn't have scene info, that means it doesn't have any
			if(Node.SceneInfo)
			{
				int32 NodeIndex = Node.SceneInfo->GetIndex();
				// if this node is visible, children shouldn't show up
				if(View.PrimitiveVisibilityMap[NodeIndex])
				{
					for(const auto& Child : Node.ChildrenSceneInfos)
					{
						const int32 ChildIndex = Child->GetIndex();

						// first update the flags
						FRelativeBitReference BitRef(ChildIndex);
						HiddenFlags.AccessCorrespondingBit(BitRef) = true;

						// find the node for it
						FLODSceneNode* ChildNode = SceneNodes.Find(Child->PrimitiveComponentId);
						// if you have child, populate it again, 
						if(ChildNode)
						{
							PopulateHiddenFlagsToChildren(HiddenFlags, *ChildNode);
						}
					}
				}
				else
				{
					HiddenFlags.AccessCorrespondingBit(FRelativeBitReference(NodeIndex)) = true;
				}
			}
		}
	}
}