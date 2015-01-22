// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DirectionalLightComponent.cpp: DirectionalLightComponent implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"

static float GMaxCSMRadiusToAllowPerObjectShadows = 8000;
static FAutoConsoleVariableRef CVarMaxCSMRadiusToAllowPerObjectShadows(
	TEXT("r.MaxCSMRadiusToAllowPerObjectShadows"),
	GMaxCSMRadiusToAllowPerObjectShadows,
	TEXT("Only stationary lights with a CSM radius smaller than this will create per object shadows for dynamic objects.")
	);

static TAutoConsoleVariable<float> CVarUnbuiltWholeSceneDynamicShadowRadius(
	TEXT("r.Shadow.UnbuiltWholeSceneDynamicShadowRadius"),
	200000.0f,
	TEXT("WholeSceneDynamicShadowRadius to use when using CSM to preview unbuilt lighting from a directional light")
	);

static TAutoConsoleVariable<int32> CVarUnbuiltNumWholeSceneDynamicShadowCascades(
	TEXT("r.Shadow.UnbuiltNumWholeSceneDynamicShadowCascades"),
	4,
	TEXT("DynamicShadowCascades to use when using CSM to preview unbuilt lighting from a directional light"),
	ECVF_RenderThreadSafe);

/**
 * The directional light policy for TMeshLightingDrawingPolicy.
 */
class FDirectionalLightPolicy
{
public:
	typedef FLightSceneInfo SceneInfoType;
};

/**
 * The scene info for a directional light.
 */
class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:

	/** Whether to occlude fog and atmosphere inscattering with screenspace blurred occlusion from this light. */
	bool bEnableLightShaftOcclusion;

	/** 
	 * Controls how dark the occlusion masking is, a value of 1 results in no darkening term.
	 */
	float OcclusionMaskDarkness;

	/** Everything closer to the camera than this distance will occlude light shafts. */
	float OcclusionDepthRange;

	/** 
	 * Can be used to make light shafts come from somewhere other than the light's actual direction. 
	 * Will only be used when non-zero.
	 */
	FVector LightShaftOverrideDirection;

	/** 
	 * Radius of the whole scene dynamic shadow centered on the viewer, which replaces the precomputed shadows based on distance from the camera.  
	 * A Radius of 0 disables the dynamic shadow.
	 */
	float WholeSceneDynamicShadowRadius;

	/** 
	 * Number of cascades to split the view frustum into for the whole scene dynamic shadow.  
	 * More cascades result in better shadow resolution and allow WholeSceneDynamicShadowRadius to be further, but add rendering cost.
	 */
	int32 DynamicShadowCascades;

	/** 
	 * Exponent that is applied to the cascade transition distances as a fraction of WholeSceneDynamicShadowRadius.
	 * An exponent of 1 means that cascade transitions will happen at a distance proportional to their resolution.
	 * A value greater than 1 brings transitions closer to the camera.
	 */
	float CascadeDistributionExponent;

	/** see UDirectionalLightComponent::CascadeTransitionFraction */
	float CascadeTransitionFraction;

	/** see UDirectionalLightComponent::ShadowDistanceFadeoutFraction */
	float ShadowDistanceFadeoutFraction;

	bool bUseInsetShadowsForMovableObjects;

	/** If greater than WholeSceneDynamicShadowRadius, a cascade will be created to support ray traced distance field shadows covering up to this distance. */
	float DistanceFieldShadowDistance;

	/** Light source angle in degrees. */
	float LightSourceAngle;

	/** Initialization constructor. */
	FDirectionalLightSceneProxy(const UDirectionalLightComponent* Component):
		FLightSceneProxy(Component),
		bEnableLightShaftOcclusion(Component->bEnableLightShaftOcclusion),
		OcclusionMaskDarkness(Component->OcclusionMaskDarkness),
		OcclusionDepthRange(Component->OcclusionDepthRange),
		LightShaftOverrideDirection(Component->LightShaftOverrideDirection),
		DynamicShadowCascades(Component->DynamicShadowCascades),
		CascadeDistributionExponent(Component->CascadeDistributionExponent),
		CascadeTransitionFraction(Component->CascadeTransitionFraction),
		ShadowDistanceFadeoutFraction(Component->ShadowDistanceFadeoutFraction),
		bUseInsetShadowsForMovableObjects(Component->bUseInsetShadowsForMovableObjects),
		DistanceFieldShadowDistance(Component->bUseRayTracedDistanceFieldShadows ? Component->DistanceFieldShadowDistance : 0),
		LightSourceAngle(Component->LightSourceAngle)
	{
		LightShaftOverrideDirection.Normalize();

		if(Component->Mobility == EComponentMobility::Movable)
		{
			WholeSceneDynamicShadowRadius = Component->DynamicShadowDistanceMovableLight;
		}
		else
		{
			WholeSceneDynamicShadowRadius = Component->DynamicShadowDistanceStationaryLight;
		}
	}

	void UpdateLightShaftOverrideDirection_GameThread(const UDirectionalLightComponent* Component)
	{
		FVector NewLightShaftOverrideDirection = Component->LightShaftOverrideDirection;
		NewLightShaftOverrideDirection.Normalize();
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FUpdateLightShaftOverrideDirectionCommand,
			FDirectionalLightSceneProxy*,Proxy,this,
			FVector,NewLightShaftOverrideDirection,NewLightShaftOverrideDirection,
		{
			Proxy->UpdateLightShaftOverrideDirection_RenderThread(NewLightShaftOverrideDirection);
		});
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FVector4& LightPositionAndInvRadius, FVector4& LightColorAndFalloffExponent, FVector& NormalizedLightDirection, FVector2D& SpotAngles, float& LightSourceRadius, float& LightSourceLength, float& LightMinRoughness) const
	{
		LightPositionAndInvRadius = FVector4(0, 0, 0, 0);

		LightColorAndFalloffExponent = FVector4(
			GetColor().R,
			GetColor().G,
			GetColor().B,
			0);

		NormalizedLightDirection = -GetDirection();

		SpotAngles = FVector2D(0, 0);
		LightSourceRadius = 0.0f;
		LightSourceLength = 0.0f;
		LightMinRoughness = MinRoughness;
	}

	virtual float GetLightSourceAngle() const override
	{
		return LightSourceAngle;
	}

	virtual bool GetLightShaftOcclusionParameters(float& OutOcclusionMaskDarkness, float& OutOcclusionDepthRange) const override
	{
		OutOcclusionMaskDarkness = OcclusionMaskDarkness;
		OutOcclusionDepthRange = OcclusionDepthRange;
		return bEnableLightShaftOcclusion;
	}

	virtual FVector GetLightPositionForLightShafts(FVector ViewOrigin) const override
	{
		const FVector EffectiveDirection = LightShaftOverrideDirection.SizeSquared() > 0 ? LightShaftOverrideDirection : GetDirection();
		return ViewOrigin - EffectiveDirection * WORLD_MAX;
	}

	// FLightSceneInfo interface.

	/** Called when precomputed lighting has been determined to be invalid */
	virtual void InvalidatePrecomputedLighting(bool bIsEditor) 
	{
		// If the light is unbuilt and is not already using CSM, setup CSM settings to preview unbuilt lighting
		if (HasStaticShadowing())
		{
			CascadeDistributionExponent = 4;
			DynamicShadowCascades = CVarUnbuiltNumWholeSceneDynamicShadowCascades.GetValueOnAnyThread();
			WholeSceneDynamicShadowRadius = CVarUnbuiltWholeSceneDynamicShadowRadius.GetValueOnAnyThread();
		}
	}

	virtual bool ShouldCreatePerObjectShadowsForDynamicObjects() const 
	{
		return FLightSceneProxy::ShouldCreatePerObjectShadowsForDynamicObjects()
			// Only create per-object shadows for dynamic objects if the CSM range is under some threshold
			// When the CSM range is very small, CSM is just being used to provide high resolution / animating shadows near the player,
			// But dynamic objects outside the CSM range would not have a shadow (or ones inside the range that cast a shadow out of the CSM area of influence).
			&& WholeSceneDynamicShadowRadius < GMaxCSMRadiusToAllowPerObjectShadows
			&& bUseInsetShadowsForMovableObjects;
	}

	/** Returns the number of view dependent shadows this light will create. */
	virtual int32 GetNumViewDependentWholeSceneShadows(const FSceneView& View) const
	{ 
		int32 NumCascades = GetNumShadowMappedCascades(View.MaxShadowCascades);

		if (ShouldCreateRayTracedCascade(View.GetFeatureLevel()))
		{
			// Add a cascade for the ray traced shadows
			NumCascades++;
		}

		return NumCascades;
	}

	/**
	 * Sets up a projected shadow initializer that's dependent on the current view for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetViewDependentWholeSceneProjectedShadowInitializer(const FSceneView& View, int32 SplitIndex, FWholeSceneProjectedShadowInitializer& OutInitializer) const
	{
		const FMatrix& WorldToLight = GetWorldToLight();

		FShadowCascadeSettings CascadeSettings;

		FSphere Bounds = FDirectionalLightSceneProxy::GetShadowSplitBounds(View, SplitIndex, &CascadeSettings);
		OutInitializer.CascadeSettings = CascadeSettings;

		const float ShadowExtent = Bounds.W / FMath::Sqrt(3.0f);
		const FBoxSphereBounds SubjectBounds(Bounds.Center, FVector(ShadowExtent, ShadowExtent, ShadowExtent), Bounds.W);
		OutInitializer.bDirectionalLight = true;
		OutInitializer.bOnePassPointLightShadow = false;
		OutInitializer.PreShadowTranslation = -Bounds.Center;
		OutInitializer.WorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
		OutInitializer.Scales = FVector(1.0f,1.0f / Bounds.W,1.0f / Bounds.W);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector,SubjectBounds.BoxExtent,SubjectBounds.SphereRadius);
		OutInitializer.WAxis = FVector4(0,0,0,1);
		OutInitializer.MinLightW = -HALF_WORLD_MAX;
		// Reduce casting distance on a directional light
		// This is necessary to improve floating point precision in several places, especially when deriving frustum verts from InvReceiverMatrix
		OutInitializer.MaxDistanceToCastInLightW = HALF_WORLD_MAX / 32.0f;
		OutInitializer.SplitIndex = SplitIndex;
		// Last cascade is the ray traced shadow, if enabled
		OutInitializer.bRayTracedDistanceFieldShadow = ShouldCreateRayTracedCascade(View.GetFeatureLevel()) && SplitIndex == FDirectionalLightSceneProxy::GetNumViewDependentWholeSceneShadows(View) - 1;
		return true;
	}

	// reflective shadow map for Light Propagation Volume
	virtual bool GetViewDependentRsmWholeSceneProjectedShadowInitializer(
		const class FSceneView& View, 
		const FBox& LightPropagationVolumeBounds,
		class FWholeSceneProjectedShadowInitializer& OutInitializer ) const
	{
		const FMatrix& WorldToLight = GetWorldToLight();

		float LpvExtent = LightPropagationVolumeBounds.GetExtent().X; // LPV is a cube, so this should be valid

		OutInitializer.bDirectionalLight = true;
		OutInitializer.PreShadowTranslation = -LightPropagationVolumeBounds.GetCenter();
		OutInitializer.WorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
		OutInitializer.Scales = FVector(1.0f,1.0f / LpvExtent,1.0f / LpvExtent);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds( FVector::ZeroVector, LightPropagationVolumeBounds.GetExtent(), FMath::Sqrt( LpvExtent * LpvExtent * 3.0f ) );
		OutInitializer.WAxis = FVector4(0,0,0,1);
		OutInitializer.MinLightW = -HALF_WORLD_MAX;
		// Reduce casting distance on a directional light
		// This is necessary to improve floating point precision in several places, especially when deriving frustum verts from InvReceiverMatrix
		OutInitializer.MaxDistanceToCastInLightW = HALF_WORLD_MAX / 32.0f;

		// Compute the RSM bounds
		{
			FVector Centre = LightPropagationVolumeBounds.GetCenter();
			FVector Extent = LightPropagationVolumeBounds.GetExtent();
			FVector CascadeFrustumVerts[8];
			CascadeFrustumVerts[0] = Centre + Extent * FVector( -1.0f,-1.0f, 1.0f ); // 0 Near Top    Right
			CascadeFrustumVerts[1] = Centre + Extent * FVector( -1.0f,-1.0f,-1.0f ); // 1 Near Bottom Right
			CascadeFrustumVerts[2] = Centre + Extent * FVector(  1.0f,-1.0f, 1.0f ); // 2 Near Top    Left
			CascadeFrustumVerts[3] = Centre + Extent * FVector(  1.0f,-1.0f,-1.0f ); // 3 Near Bottom Left
			CascadeFrustumVerts[4] = Centre + Extent * FVector( -1.0f, 1.0f, 1.0f ); // 4 Far  Top    Right
			CascadeFrustumVerts[5] = Centre + Extent * FVector( -1.0f, 1.0f,-1.0f ); // 5 Far  Bottom Right
			CascadeFrustumVerts[6] = Centre + Extent * FVector(  1.0f, 1.0f, 1.0f ); // 6 Far  Top    Left
			CascadeFrustumVerts[7] = Centre + Extent * FVector(  1.0f, 1.0f,-1.0f ); // 7 Far  Bottom Left

			FPlane Far;
			FPlane Near;
			const FVector LightDirection = -GetDirection();
			ComputeShadowCullingVolume( CascadeFrustumVerts, LightDirection, OutInitializer.CascadeSettings.ShadowBoundsAccurate, Near, Far );
		}
		return true;
	}

	virtual FVector2D GetDirectionalLightDistanceFadeParameters(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		const float FarDistance = ComputeShadowDistance(InFeatureLevel);
        
		// The far distance for the dynamic to static fade is the range of the directional light.
		// The near distance is placed at a depth of 90% of the light's range.
		const float NearDistance = FarDistance - FarDistance * ShadowDistanceFadeoutFraction;
		return FVector2D(NearDistance, 1.0f / FMath::Max<float>(FarDistance - NearDistance, KINDA_SMALL_NUMBER));
	}

	virtual bool GetPerObjectProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,FPerObjectProjectedShadowInitializer& OutInitializer) const
	{
		OutInitializer.bDirectionalLight = true;
		OutInitializer.PreShadowTranslation = -SubjectBounds.Origin;
		OutInitializer.WorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
		OutInitializer.Scales = FVector(1.0f,1.0f / SubjectBounds.SphereRadius,1.0f / SubjectBounds.SphereRadius);
		OutInitializer.FaceDirection = FVector(1,0,0);
		OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector,SubjectBounds.BoxExtent,SubjectBounds.SphereRadius);
		OutInitializer.WAxis = FVector4(0,0,0,1);
		OutInitializer.MinLightW = -HALF_WORLD_MAX;
		// Reduce casting distance on a directional light
		// This is necessary to improve floating point precision in several places, especially when deriving frustum verts from InvReceiverMatrix
		OutInitializer.MaxDistanceToCastInLightW = HALF_WORLD_MAX / 32.0f;
		return true;
	}

private:

	int32 GetNumShadowMappedCascades(int32 MaxShadowCascades) const
	{
		int32 NumCascades = WholeSceneDynamicShadowRadius > 0.0f ? DynamicShadowCascades : 0;
		return FMath::Min<int32>(NumCascades, MaxShadowCascades);
	}

	float GetCSMMaxDistance() const
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.DistanceScale"));
		 
		float Scale = FMath::Clamp(CVar->GetValueOnRenderThread(), 0.1f, 2.0f);
		float Distance = WholeSceneDynamicShadowRadius * Scale;
		return Distance;
	}

	float GetDistanceFieldShadowDistance() const
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));

		if (CVar->GetValueOnRenderThread() == 0)
		{
			// Meshes must have distance fields generated
			return 0;
		}

		return DistanceFieldShadowDistance;
	}

	bool ShouldCreateRayTracedCascade(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		//@todo - handle View.MaxShadowCascades properly
		const int32 NumCascades = GetNumShadowMappedCascades(10);
		const float RaytracedShadowDistance = GetDistanceFieldShadowDistance();
		const bool bCreateWithCSM = NumCascades > 0 && RaytracedShadowDistance > GetCSMMaxDistance();
		const bool bCreateWithoutCSM = NumCascades == 0 && RaytracedShadowDistance > 0;
		return DoesPlatformSupportDistanceFieldShadowing(GShaderPlatformForFeatureLevel[InFeatureLevel]) && (bCreateWithCSM || bCreateWithoutCSM);
	}

	float ComputeShadowDistance(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		float Distance = GetCSMMaxDistance();

		if (ShouldCreateRayTracedCascade(InFeatureLevel))
		{
			Distance = GetDistanceFieldShadowDistance();
		}

		return Distance;
	}

	float GetShadowTransitionScale() const
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.CSM.TransitionScale"));
		float Scale = FMath::Clamp(CVar->GetValueOnRenderThread(), 0.0f, 2.0f);
		return Scale;
	}

	void UpdateLightShaftOverrideDirection_RenderThread(FVector NewLightShaftOverrideDirection)
	{
		LightShaftOverrideDirection = NewLightShaftOverrideDirection;
	}

	// Computes a shadow culling volume (convex hull) based on a set of 8 vertices and a light direction
	void ComputeShadowCullingVolume( const FVector *CascadeFrustumVerts, const FVector& LightDirection, FConvexVolume& ConvexVolumeOut, FPlane& NearPlaneOut, FPlane& FarPlaneOut ) const
	{
		// Pairs of plane indices from SubFrustumPlanes whose intersections
		// form the edges of the frustum.
		static const int32 AdjacentPlanePairs[12][2] =
		{
			{0,2}, {0,4}, {0,1}, {0,3},
			{2,3}, {4,2}, {1,4}, {3,1},
			{2,5}, {4,5}, {1,5}, {3,5}
		};
		// Maps a plane pair index to the index of the two frustum corners
		// which form the end points of the plane intersection.
		static const int32 LineVertexIndices[12][2] =
		{
			{0,1}, {1,3}, {3,2}, {2,0},
			{0,4}, {1,5}, {3,7}, {2,6},
			{4,5}, {5,7}, {7,6}, {6,4}
		};

		TArray<FPlane, TInlineAllocator<6>> Planes;

		// Find the view frustum subsection planes which face away from the light and add them to the bounding volume
		FPlane SubFrustumPlanes[6];
		SubFrustumPlanes[0] = FPlane(CascadeFrustumVerts[3], CascadeFrustumVerts[2], CascadeFrustumVerts[0]); // Near
		SubFrustumPlanes[1] = FPlane(CascadeFrustumVerts[7], CascadeFrustumVerts[6], CascadeFrustumVerts[2]); // Left
		SubFrustumPlanes[2] = FPlane(CascadeFrustumVerts[0], CascadeFrustumVerts[4], CascadeFrustumVerts[5]); // Right
		SubFrustumPlanes[3] = FPlane(CascadeFrustumVerts[2], CascadeFrustumVerts[6], CascadeFrustumVerts[4]); // Top
		SubFrustumPlanes[4] = FPlane(CascadeFrustumVerts[5], CascadeFrustumVerts[7], CascadeFrustumVerts[3]); // Bottom
		SubFrustumPlanes[5] = FPlane(CascadeFrustumVerts[4], CascadeFrustumVerts[6], CascadeFrustumVerts[7]); // Far

		NearPlaneOut = SubFrustumPlanes[0];
		FarPlaneOut = SubFrustumPlanes[5];

		// Add the planes from the camera's frustum which form the back face of the frustum when in light space.
		for (int32 i = 0; i < 6; i++)
		{
			FVector Normal(SubFrustumPlanes[i]);
			float d = Normal | LightDirection;
			if ( d < 0.0f )
			{
				Planes.Add(SubFrustumPlanes[i]);
			}
		}

		// Now add the planes which form the silhouette edges of the camera frustum in light space.
		for (int32 i = 0; i < 12; i++)
		{
			FVector NormalA(SubFrustumPlanes[AdjacentPlanePairs[i][0]]);
			FVector NormalB(SubFrustumPlanes[AdjacentPlanePairs[i][1]]);

			float DotA = NormalA | LightDirection;
			float DotB = NormalB | LightDirection;

			// If the signs of the dot product are different
			if ( DotA * DotB < 0.0f )
			{
				// Planes are opposing, so this is an edge. 
				// Extrude the plane along the light direction, and add it to the array.

				FVector A = CascadeFrustumVerts[LineVertexIndices[i][0]];
				FVector B = CascadeFrustumVerts[LineVertexIndices[i][1]];
				// Scale the 3rd vector by the length of AB for precision
				FVector C = A + LightDirection * (A - B).Size(); 

				// Account for winding
				if ( DotA >= 0.0f ) 
				{
					Planes.Add(FPlane(A, B, C));
				}
				else
				{
					Planes.Add(FPlane(B, A, C));
				}
			}
		}
		ConvexVolumeOut = FConvexVolume(Planes);
	}

	// Useful helper function to compute shadow map cascade distribution
	// @param Exponent >=1, 1:linear, 2:each cascade gets 2x in size, ...
	// @param CascadeIndex 0..CascadeCount
	// @param CascadeCount >0
	static float ComputeAccumulatedScale(float Exponent, int32 CascadeIndex, int32 CascadeCount)
	{
		if(CascadeIndex <= 0)
		{
			return 0.0f;
		}

		float CurrentScale = 1;
		float TotalScale = 0.0f;
		float Ret = 0.0f;

		// lame implementation for simplicity, CascadeIndex is small anyway
		for(int i = 0; i < CascadeCount; ++i)
		{
			if(i < CascadeIndex)
			{
				Ret += CurrentScale;
			}
			TotalScale += CurrentScale;
			CurrentScale *= Exponent;
		}

		return Ret / TotalScale;
	}

	// Given a particular split index and view near plane, returns the view space Z distance to the PSSM split plane.
	inline float GetSplitDistance(const FSceneView& View, int32 SplitIndex, int32 NumCascades, float ShadowNear, bool bIgnoreLastCascade) const
	{
		check(SplitIndex >= 0);

		float FarDistance = ComputeShadowDistance(View.GetFeatureLevel());

		if (bIgnoreLastCascade)
		{
			// Compute shadowmap cascade distances as if the ray traced cascade didn't exist
			NumCascades--;
			FarDistance = GetCSMMaxDistance();
		}

		return ShadowNear + ComputeAccumulatedScale(CascadeDistributionExponent, SplitIndex, NumCascades) * (FarDistance - ShadowNear);
	}

	virtual FSphere GetShadowSplitBounds(const FSceneView& View, int32 SplitIndex, FShadowCascadeSettings* OutCascadeSettings) const
	{
		// this checks for WholeSceneDynamicShadowRadius and DynamicShadowCascades
		int32 NumCascades = GetNumViewDependentWholeSceneShadows(View);

		const bool bHasRayTracedCascade = ShouldCreateRayTracedCascade(View.GetFeatureLevel());
		const bool bIsRayTracedCascade = bHasRayTracedCascade && SplitIndex == NumCascades - 1;

		const FMatrix ViewMatrix = View.ShadowViewMatrices.ViewMatrix;
		const FMatrix ProjectionMatrix = View.ShadowViewMatrices.ProjMatrix;
		const FVector4 ViewOrigin = View.ShadowViewMatrices.ViewOrigin;

		const FVector CameraDirection = ViewMatrix.GetColumn(2);
		const FVector LightDirection = -GetDirection();

		// Determine start and end distances to the current cascade's split planes
		// Presence of the ray traced cascade does not change depth ranges for the shadow-mapped cascades
		float SplitNear = GetSplitDistance(View, SplitIndex, NumCascades, View.NearClippingDistance, bHasRayTracedCascade);
		float SplitFar = GetSplitDistance(View, SplitIndex + 1, NumCascades, View.NearClippingDistance, bHasRayTracedCascade && !bIsRayTracedCascade);
		float FadePlane = SplitFar;

		float LocalCascadeTransitionFraction = CascadeTransitionFraction * GetShadowTransitionScale();

		float FadeExtension = (SplitFar - SplitNear) * LocalCascadeTransitionFraction;

		// Add the fade region to the end of the subfrustum, if this is not the last cascade.
		if (SplitIndex < (NumCascades - 1))
		{
			SplitFar += FadeExtension;			
		}

		if(OutCascadeSettings)
		{
			OutCascadeSettings->SplitFarFadeRegion = FadeExtension;

			OutCascadeSettings->SplitNearFadeRegion = 0.0f;
			if(SplitIndex >= 1)
			{
				// todo: optimize
				float BeforeSplitNear = GetSplitDistance(View, SplitIndex - 1, NumCascades, View.NearClippingDistance, bHasRayTracedCascade);
				float BeforeSplitFar = GetSplitDistance(View, SplitIndex, NumCascades, View.NearClippingDistance, bHasRayTracedCascade);

				OutCascadeSettings->SplitNearFadeRegion = (BeforeSplitFar - BeforeSplitNear) * LocalCascadeTransitionFraction;
			}
		}

		// Get FOV and AspectRatio from the view's projection matrix.
		float AspectRatio = ProjectionMatrix.M[1][1] / ProjectionMatrix.M[0][0];
		float HalfFOV = (ViewOrigin.W > 0.0f) ? FMath::Atan(1.0f / ProjectionMatrix.M[0][0]) : PI/4.0f;

		// Build the camera frustum for this cascade
		const float StartHorizontalLength = SplitNear * FMath::Tan(HalfFOV);
		const FVector StartCameraRightOffset = ViewMatrix.GetColumn(0) * StartHorizontalLength;
		const float StartVerticalLength = StartHorizontalLength / AspectRatio;
		const FVector StartCameraUpOffset = ViewMatrix.GetColumn(1) * StartVerticalLength;
		
		const float EndHorizontalLength = SplitFar * FMath::Tan(HalfFOV);
		const FVector EndCameraRightOffset = ViewMatrix.GetColumn(0) * EndHorizontalLength;
		const float EndVerticalLength = EndHorizontalLength / AspectRatio;
		const FVector EndCameraUpOffset = ViewMatrix.GetColumn(1) * EndVerticalLength;

		// Get the 8 corners of the cascade's camera frustum, in world space
		FVector CascadeFrustumVerts[8];
		CascadeFrustumVerts[0] = ViewOrigin + CameraDirection * SplitNear + StartCameraRightOffset + StartCameraUpOffset; // 0 Near Top    Right
		CascadeFrustumVerts[1] = ViewOrigin + CameraDirection * SplitNear + StartCameraRightOffset - StartCameraUpOffset; // 1 Near Bottom Right
		CascadeFrustumVerts[2] = ViewOrigin + CameraDirection * SplitNear - StartCameraRightOffset + StartCameraUpOffset; // 2 Near Top    Left
		CascadeFrustumVerts[3] = ViewOrigin + CameraDirection * SplitNear - StartCameraRightOffset - StartCameraUpOffset; // 3 Near Bottom Left
		CascadeFrustumVerts[4] = ViewOrigin + CameraDirection * SplitFar  + EndCameraRightOffset   + EndCameraUpOffset;   // 4 Far  Top    Right
		CascadeFrustumVerts[5] = ViewOrigin + CameraDirection * SplitFar  + EndCameraRightOffset   - EndCameraUpOffset;   // 5 Far  Bottom Right
		CascadeFrustumVerts[6] = ViewOrigin + CameraDirection * SplitFar  - EndCameraRightOffset   + EndCameraUpOffset;   // 6 Far  Top    Left
		CascadeFrustumVerts[7] = ViewOrigin + CameraDirection * SplitFar  - EndCameraRightOffset   - EndCameraUpOffset;   // 7 Far  Bottom Left

		// Fit a bounding sphere around the world space camera cascade frustum.
		// Compute the sphere ideal centre point given the FOV and near/far.
		float TanHalfFOVx = FMath::Tan(HalfFOV);
		float TanHalfFOVy = TanHalfFOVx / AspectRatio;
		float FrustumLength = SplitFar - SplitNear;

		float FarX = TanHalfFOVx * SplitFar;
		float FarY = TanHalfFOVy * SplitFar;
		float DiagonalASq = FarX * FarX + FarY * FarY;

		float NearX = TanHalfFOVx * SplitNear;
		float NearY = TanHalfFOVy * SplitNear;
		float DiagonalBSq = NearX * NearX + NearY * NearY;

		// Calculate the ideal bounding sphere for the subfrustum.
		// Fx  = (Db^2 - da^2) / 2Fl + Fl / 2 
		// (where Da is the far diagonal, and Db is the near, and Fl is the frustum length)
		float OptimalOffset = (DiagonalBSq - DiagonalASq) / (2.0f * FrustumLength) + FrustumLength * 0.5f;
		float CentreZ = SplitFar - OptimalOffset;
		CentreZ = FMath::Clamp( CentreZ, SplitNear, SplitFar );
		FSphere CascadeSphere(ViewOrigin + CameraDirection * CentreZ, 0);
		for (int32 Index = 0; Index < 8; Index++)
		{
			CascadeSphere.W = FMath::Max(CascadeSphere.W, FVector::DistSquared(CascadeFrustumVerts[Index], CascadeSphere.Center));
		}

		// Don't allow the bounds to reach 0 (INF)
		CascadeSphere.W = FMath::Max(FMath::Sqrt(CascadeSphere.W), 1.0f); 

		if (OutCascadeSettings)
		{
			// Pass out the split settings
			OutCascadeSettings->SplitFar = SplitFar;
			OutCascadeSettings->SplitNear = SplitNear;
			OutCascadeSettings->FadePlaneOffset = FadePlane;
			OutCascadeSettings->FadePlaneLength = SplitFar - FadePlane;

			// this function is needed, since it's also called by the LPV code.
 			ComputeShadowCullingVolume( CascadeFrustumVerts, LightDirection, OutCascadeSettings->ShadowBoundsAccurate, OutCascadeSettings->NearFrustumPlane, OutCascadeSettings->FarFrustumPlane );
		}

		return CascadeSphere;
	}
};

UDirectionalLightComponent::UDirectionalLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightDirectional"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightDirectionalMove"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	Intensity = 10;

	bEnableLightShaftOcclusion = false;
	OcclusionDepthRange = 100000.f;
	OcclusionMaskDarkness = 0.05f;

	WholeSceneDynamicShadowRadius_DEPRECATED = 20000.0f;
	DynamicShadowDistanceMovableLight = 20000.0f;
	DynamicShadowDistanceStationaryLight = 0.f;

	DistanceFieldShadowDistance = 30000.0f;
	LightSourceAngle = 1;

	DynamicShadowCascades = 3;
	CascadeDistributionExponent = 3.0f;
	CascadeTransitionFraction = 0.1f;
	ShadowDistanceFadeoutFraction = 0.1f;
	IndirectLightingIntensity = 1.0f;
	CastTranslucentShadows = true;
	bUseInsetShadowsForMovableObjects = true;
}

#if WITH_EDITOR
/**
 * Called after property has changed via e.g. property window or set command.
 *
 * @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
 */
void UDirectionalLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	LightmassSettings.LightSourceAngle = FMath::Max(LightmassSettings.LightSourceAngle, 0.0f);
	LightmassSettings.IndirectLightingSaturation = FMath::Max(LightmassSettings.IndirectLightingSaturation, 0.0f);
	LightmassSettings.ShadowExponent = FMath::Clamp(LightmassSettings.ShadowExponent, .5f, 8.0f);

	DynamicShadowDistanceMovableLight = FMath::Max(DynamicShadowDistanceMovableLight, 0.0f);
	DynamicShadowDistanceStationaryLight = FMath::Max(DynamicShadowDistanceStationaryLight, 0.0f);

	DynamicShadowCascades = FMath::Clamp(DynamicShadowCascades, 0, 10);
	CascadeDistributionExponent = FMath::Clamp(CascadeDistributionExponent, .1f, 10.0f);
	CascadeTransitionFraction = FMath::Clamp(CascadeTransitionFraction, 0.0f, 0.3f);
	ShadowDistanceFadeoutFraction = FMath::Clamp(ShadowDistanceFadeoutFraction, 0.0f, 1.0f);
	// max range is larger than UI
	ShadowBias = FMath::Clamp(ShadowBias, 0.0f, 10.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UDirectionalLightComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, bUseInsetShadowsForMovableObjects))
		{
			return CastShadows && CastDynamicShadows && DynamicShadowDistanceStationaryLight > 0 && Mobility == EComponentMobility::Stationary;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DynamicShadowCascades)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, CascadeDistributionExponent)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, CascadeTransitionFraction)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, ShadowDistanceFadeoutFraction)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, bUseInsetShadowsForMovableObjects))
		{
			return CastShadows 
				&& CastDynamicShadows 
				&& ((DynamicShadowDistanceMovableLight > 0 && Mobility == EComponentMobility::Movable)
					|| (DynamicShadowDistanceStationaryLight > 0 && Mobility == EComponentMobility::Stationary));
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, DistanceFieldShadowDistance)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, LightSourceAngle))
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
			bool bCanEdit = CastShadows && CastDynamicShadows && bUseRayTracedDistanceFieldShadows && Mobility != EComponentMobility::Static && CVar->GetValueOnGameThread() != 0;
			return bCanEdit;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, OcclusionMaskDarkness)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UDirectionalLightComponent, OcclusionDepthRange))
		{
			return bEnableLightShaftOcclusion;
		}
	}

	return Super::CanEditChange(InProperty);
}

#endif // WITH_EDITOR

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy() const
{
	return new FDirectionalLightSceneProxy(this);
}

FVector4 UDirectionalLightComponent::GetLightPosition() const
{
	return FVector4(-GetDirection() * WORLD_MAX, 0 );
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType UDirectionalLightComponent::GetLightType() const
{
	return LightType_Directional;
}

void UDirectionalLightComponent::SetDynamicShadowDistanceMovableLight(float NewValue)
{
	if (DynamicShadowDistanceMovableLight != NewValue)
	{
		DynamicShadowDistanceMovableLight = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetDynamicShadowDistanceStationaryLight(float NewValue)
{
	if (DynamicShadowDistanceStationaryLight != NewValue)
	{
		DynamicShadowDistanceStationaryLight = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetDynamicShadowCascades(int32 NewValue)
{
	if (DynamicShadowCascades != NewValue)
	{
		DynamicShadowCascades = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetCascadeDistributionExponent(float NewValue)
{
	if (CascadeDistributionExponent != NewValue)
	{
		CascadeDistributionExponent = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetCascadeTransitionFraction(float NewValue)
{
	if (CascadeTransitionFraction != NewValue)
	{
		CascadeTransitionFraction = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetShadowDistanceFadeoutFraction(float NewValue)
{
	if (ShadowDistanceFadeoutFraction != NewValue)
	{
		ShadowDistanceFadeoutFraction = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetEnableLightShaftOcclusion(bool bNewValue)
{
	if ((IsRunningUserConstructionScript() || !(IsRegistered() && Mobility == EComponentMobility::Static))
		&& bEnableLightShaftOcclusion != bNewValue)
	{
		bEnableLightShaftOcclusion = bNewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetOcclusionMaskDarkness(float NewValue)
{
	if ((IsRunningUserConstructionScript() || !(IsRegistered() && Mobility == EComponentMobility::Static))
		&& OcclusionMaskDarkness != NewValue)
	{
		OcclusionMaskDarkness = NewValue;
		MarkRenderStateDirty();
	}
}

void UDirectionalLightComponent::SetLightShaftOverrideDirection(FVector NewValue)
{
	if ((IsRunningUserConstructionScript() || !(IsRegistered() && Mobility == EComponentMobility::Static))
		&& LightShaftOverrideDirection != NewValue)
	{
		LightShaftOverrideDirection = NewValue;
		if (SceneProxy)
		{
			FDirectionalLightSceneProxy* DirectionalLightSceneProxy = (FDirectionalLightSceneProxy*)SceneProxy;
			DirectionalLightSceneProxy->UpdateLightShaftOverrideDirection_GameThread(this);
		}
	}
}

void UDirectionalLightComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.UE4Ver() < VER_UE4_REMOVE_LIGHT_MOBILITY_CLASSES)
	{
		// If outer is a DirectionalLight, we use the ADirectionalLight::LoadedFromAnotherClass path
		if( GetOuter() != NULL && !GetOuter()->IsA(ADirectionalLight::StaticClass()) )
		{
			if(Mobility == EComponentMobility::Movable)
			{
				DynamicShadowDistanceMovableLight = WholeSceneDynamicShadowRadius_DEPRECATED; 
			}
			else if(Mobility == EComponentMobility::Stationary)
			{
				DynamicShadowDistanceStationaryLight = WholeSceneDynamicShadowRadius_DEPRECATED;
			}
		}
	}
}

void UDirectionalLightComponent::InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly)
{
	// Directional lights don't care about translation
	if (!bTranslationOnly)
	{
		Super::InvalidateLightingCacheDetailed(bInvalidateBuildEnqueuedLighting, bTranslationOnly);
	}
}