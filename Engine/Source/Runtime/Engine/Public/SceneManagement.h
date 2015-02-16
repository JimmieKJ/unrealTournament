// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneManagement.h: Scene manager definitions.
=============================================================================*/

#pragma once

// Includes the draw mesh macros
#include "UniformBuffer.h"
#include "ConvexVolume.h"
#include "Engine/TextureLightProfile.h"
#include "SceneTypes.h"
#include "SceneView.h"
#include "RHIDefinitions.h"
#include "ChunkedArray.h"
#include "BatchedElements.h"
#include "MeshBatch.h"
#include "RendererInterface.h"

// Forward declarations.
class FLightSceneInfo;
class ULightComponent;
class UDecalComponent;
class HHitProxy;
struct FDynamicMeshVertex;


DECLARE_LOG_CATEGORY_EXTERN(LogBufferVisualization, Log, All);

// -----------------------------------------------------------------------------


/**
 * struct to hold the temporal LOD state within a view state
 */
struct ENGINE_API FTemporalLODState
{
	/** The last two camera origin samples collected for stateless temporal LOD transitions */
	FVector	TemporalLODViewOrigin[2];
	/** The last two fov-like parameters from the projection matrix for stateless temporal LOD transitions */
	float	TemporalDistanceFactor[2];
	/** The last two time samples collected for stateless temporal LOD transitions */
	float	TemporalLODTime[2];
	/** If non-zero, then we are doing temporal LOD smoothing, this is the time interval. */
	float	TemporalLODLag;

	FTemporalLODState()
		: TemporalLODLag(0.0f) // nothing else is used if this is zero
	{

	}
	/** 
	 * Returns the blend factor between the last two LOD samples
	 */
	float GetTemporalLODTransition(float LastRenderTime) const
	{
		if (TemporalLODLag == 0.0)
		{
			return 0.0f; // no fade
		}
		return FMath::Clamp((LastRenderTime - TemporalLODLag - TemporalLODTime[0]) / (TemporalLODTime[1] - TemporalLODTime[0]), 0.0f, 1.0f);
	}

	void UpdateTemporalLODTransition(const class FViewInfo& View, float LastRenderTime);
};

/**
 * The scene manager's persistent view state.
 */
class FSceneViewStateInterface
{
public:
	FSceneViewStateInterface()
		:	ViewParent( NULL )
		,	NumChildren( 0 )
	{}
	
	/** Called in the game thread to destroy the view state. */
	virtual void Destroy() = 0;

public:
	/** Sets the view state's scene parent. */
	void SetViewParent(FSceneViewStateInterface* InViewParent)
	{
		if ( ViewParent )
		{
			// Assert that the existing parent does not have a parent.
			check( !ViewParent->HasViewParent() );
			// Decrement ref ctr of existing parent.
			--ViewParent->NumChildren;
		}

		if ( InViewParent && InViewParent != this )
		{
			// Assert that the incoming parent does not have a parent.
			check( !InViewParent->HasViewParent() );
			ViewParent = InViewParent;
			// Increment ref ctr of new parent.
			InViewParent->NumChildren++;
		}
		else
		{
			ViewParent = NULL;
		}
	}
	/** @return			The view state's scene parent, or NULL if none present. */
	FSceneViewStateInterface* GetViewParent()
	{
		return ViewParent;
	}
	/** @return			The view state's scene parent, or NULL if none present. */
	const FSceneViewStateInterface* GetViewParent() const
	{
		return ViewParent;
	}
	/** @return			true if the scene state has a parent, false otherwise. */
	bool HasViewParent() const
	{
		return GetViewParent() != NULL;
	}
	/** @return			true if this scene state is a parent, false otherwise. */
	bool IsViewParent() const
	{
		return NumChildren > 0;
	}
	
	virtual void AddReferencedObjects(FReferenceCollector& Collector) = 0;

	virtual SIZE_T GetSizeBytes() const { return 0; }

	/** called in InitViews() */
	virtual void OnStartFrame(FSceneView& CurrentView) = 0;

	/** Resets pool for GetReusableMID() */
	virtual void OnStartPostProcessing(FSceneView& CurrentView) = 0;
	/** Allows MIDs being created and released during view rendering without the overhead of creating and relasing objects */
	virtual UMaterialInstanceDynamic* GetReusableMID(class UMaterialInterface* ParentMaterial) = 0;
	/** Returns the temporal LOD struct from the viewstate */
	virtual FTemporalLODState& GetTemporalLODState() = 0;
	virtual const FTemporalLODState& GetTemporalLODState() const = 0;
	/** 
	 * Returns the blend factor between the last two LOD samples
	 */
	virtual float GetTemporalLODTransition() const = 0;
protected:
	// Don't allow direct deletion of the view state, Destroy should be called instead.
	virtual ~FSceneViewStateInterface() {}

private:
	/** This scene state's view parent; NULL if no parent present. */
	FSceneViewStateInterface*	ViewParent;
	/** Reference counts the number of children parented to this state. */
	int32							NumChildren;
};



/**
 * The types of interactions between a light and a primitive.
 */
enum ELightInteractionType
{
	LIT_CachedIrrelevant,
	LIT_CachedLightMap,
	LIT_Dynamic,
	LIT_CachedSignedDistanceFieldShadowMap2D
};

/**
 * Information about an interaction between a light and a mesh.
 */
class FLightInteraction
{
public:

	// Factory functions.
	static FLightInteraction Dynamic() { return FLightInteraction(LIT_Dynamic); }
	static FLightInteraction LightMap() { return FLightInteraction(LIT_CachedLightMap); }
	static FLightInteraction Irrelevant() { return FLightInteraction(LIT_CachedIrrelevant); }
	static FLightInteraction ShadowMap2D() { return FLightInteraction(LIT_CachedSignedDistanceFieldShadowMap2D); }

	// Accessors.
	ELightInteractionType GetType() const { return Type; }

private:

	/**
	 * Minimal initialization constructor.
	 */
	FLightInteraction(
		ELightInteractionType InType
		):
		Type(InType)
	{}

	ELightInteractionType Type;
};





/** The number of coefficients that are stored for each light sample. */ 
static const int32 NUM_STORED_LIGHTMAP_COEF = 4;

/** The number of directional coefficients which the lightmap stores for each light sample. */ 
static const int32 NUM_HQ_LIGHTMAP_COEF = 2;

/** The number of simple coefficients which the lightmap stores for each light sample. */ 
static const int32 NUM_LQ_LIGHTMAP_COEF = 2;

/** The index at which simple coefficients are stored in any array containing all NUM_STORED_LIGHTMAP_COEF coefficients. */ 
static const int32 LQ_LIGHTMAP_COEF_INDEX = 2;

/** Compile out low quality lightmaps to save memory */
// @todo-mobile: Need to fix this!
#define ALLOW_LQ_LIGHTMAPS (PLATFORM_DESKTOP || PLATFORM_IOS || PLATFORM_ANDROID || PLATFORM_HTML5 )

/** Compile out high quality lightmaps to save memory */
#define ALLOW_HQ_LIGHTMAPS 1

/** Make sure at least one is defined */
#if !ALLOW_LQ_LIGHTMAPS && !ALLOW_HQ_LIGHTMAPS
#error At least one of ALLOW_LQ_LIGHTMAPS and ALLOW_HQ_LIGHTMAPS needs to be defined!
#endif

/**
 * Information about an interaction between a light and a mesh.
 */
class FLightMapInteraction
{
public:

	// Factory functions.
	static FLightMapInteraction None()
	{
		FLightMapInteraction Result;
		Result.Type = LMIT_None;
		return Result;
	}
	static FLightMapInteraction Texture(
		const class ULightMapTexture2D* const* InTextures,
		const ULightMapTexture2D* InSkyOcclusionTexture,
		const FVector4* InCoefficientScales,
		const FVector4* InCoefficientAdds,
		const FVector2D& InCoordinateScale,
		const FVector2D& InCoordinateBias,
		bool bAllowHighQualityLightMaps);

	/** Default constructor. */
	FLightMapInteraction():
		SkyOcclusionTexture(NULL),
		Type(LMIT_None)
	{}

	// Accessors.
	ELightMapInteractionType GetType() const { return Type; }
	
	const ULightMapTexture2D* GetTexture(bool bHighQuality) const
	{
		check(Type == LMIT_Texture);
#if ALLOW_LQ_LIGHTMAPS && ALLOW_HQ_LIGHTMAPS
		return bHighQuality ? HighQualityTexture : LowQualityTexture;
#elif ALLOW_HQ_LIGHTMAPS
		return HighQualityTexture;
#else
		return LowQualityTexture;
#endif
	}

	const ULightMapTexture2D* GetSkyOcclusionTexture() const
	{
		check(Type == LMIT_Texture);
#if ALLOW_HQ_LIGHTMAPS
		return SkyOcclusionTexture;
#else
		return NULL;
#endif
	}

	const FVector4* GetScaleArray() const
	{
#if ALLOW_LQ_LIGHTMAPS && ALLOW_HQ_LIGHTMAPS
		return AllowsHighQualityLightmaps() ? HighQualityCoefficientScales : LowQualityCoefficientScales;
#elif ALLOW_HQ_LIGHTMAPS
		return HighQualityCoefficientScales;
#else
		return LowQualityCoefficientScales;
#endif
	}

	const FVector4* GetAddArray() const
	{
#if ALLOW_LQ_LIGHTMAPS && ALLOW_HQ_LIGHTMAPS
		return AllowsHighQualityLightmaps() ? HighQualityCoefficientAdds : LowQualityCoefficientAdds;
#elif ALLOW_HQ_LIGHTMAPS
		return HighQualityCoefficientAdds;
#else
		return LowQualityCoefficientAdds;
#endif
	}
	
	const FVector2D& GetCoordinateScale() const
	{
		check(Type == LMIT_Texture);
		return CoordinateScale;
	}
	const FVector2D& GetCoordinateBias() const
	{
		check(Type == LMIT_Texture);
		return CoordinateBias;
	}

	uint32 GetNumLightmapCoefficients() const
	{
#if ALLOW_LQ_LIGHTMAPS && ALLOW_HQ_LIGHTMAPS
#if PLATFORM_DESKTOP && (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)		// This is to allow for dynamic switching between simple and directional light maps in the PC editor
		if( !AllowsHighQualityLightmaps() )
		{
			return NUM_LQ_LIGHTMAP_COEF;
		}
#endif
		return NumLightmapCoefficients;
#elif ALLOW_HQ_LIGHTMAPS
		return NUM_HQ_LIGHTMAP_COEF;
#else
		return NUM_LQ_LIGHTMAP_COEF;
#endif
	}

	/**
	* @return true if high quality lightmaps are allowed
	*/
	FORCEINLINE bool AllowsHighQualityLightmaps() const
	{
#if ALLOW_LQ_LIGHTMAPS && ALLOW_HQ_LIGHTMAPS
		return bAllowHighQualityLightMaps;
#elif ALLOW_HQ_LIGHTMAPS
		return true;
#else
		return false;
#endif
	}

	/** These functions are used for the Dummy lightmap policy used in LightMap density view mode. */
	/** 
	 *	Set the type.
	 *
	 *	@param	InType				The type to set it to.
	 */
	void SetLightMapInteractionType(ELightMapInteractionType InType)
	{
		Type = InType;
	}
	/** 
	 *	Set the coordinate scale.
	 *
	 *	@param	InCoordinateScale	The scale to set it to.
	 */
	void SetCoordinateScale(const FVector2D& InCoordinateScale)
	{
		CoordinateScale = InCoordinateScale;
	}
	/** 
	 *	Set the coordinate bias.
	 *
	 *	@param	InCoordinateBias	The bias to set it to.
	 */
	void SetCoordinateBias(const FVector2D& InCoordinateBias)
	{
		CoordinateBias = InCoordinateBias;
	}

private:

#if ALLOW_HQ_LIGHTMAPS
	FVector4 HighQualityCoefficientScales[NUM_HQ_LIGHTMAP_COEF];
	FVector4 HighQualityCoefficientAdds[NUM_HQ_LIGHTMAP_COEF];
	const class ULightMapTexture2D* HighQualityTexture;
	const ULightMapTexture2D* SkyOcclusionTexture;
#endif

#if ALLOW_LQ_LIGHTMAPS
	FVector4 LowQualityCoefficientScales[NUM_LQ_LIGHTMAP_COEF];
	FVector4 LowQualityCoefficientAdds[NUM_LQ_LIGHTMAP_COEF];
	const class ULightMapTexture2D* LowQualityTexture;
#endif

#if ALLOW_LQ_LIGHTMAPS && ALLOW_HQ_LIGHTMAPS
	bool bAllowHighQualityLightMaps;
	uint32 NumLightmapCoefficients;
#endif

	ELightMapInteractionType Type;

	FVector2D CoordinateScale;
	FVector2D CoordinateBias;
};

/** Information about the static shadowing information for a primitive. */
class FShadowMapInteraction
{
public:

	// Factory functions.
	static FShadowMapInteraction None()
	{
		FShadowMapInteraction Result;
		Result.Type = SMIT_None;
		return Result;
	}
	static FShadowMapInteraction Texture(
		class UShadowMapTexture2D* InTexture,
		const FVector2D& InCoordinateScale,
		const FVector2D& InCoordinateBias,
		const bool* InChannelValid)
	{
		FShadowMapInteraction Result;
		Result.Type = SMIT_Texture;
		Result.ShadowTexture = InTexture;
		Result.CoordinateScale = InCoordinateScale;
		Result.CoordinateBias = InCoordinateBias;
		
		for (int Channel = 0; Channel < 4; Channel++)
		{
			Result.bChannelValid[Channel] = InChannelValid[Channel];
		}

		return Result;
	}

	/** Default constructor. */
	FShadowMapInteraction() :
		Type(SMIT_None),
		ShadowTexture(NULL)
	{
		for (int Channel = 0; Channel < ARRAY_COUNT(bChannelValid); Channel++)
		{
			bChannelValid[Channel] = false;
		}
	}

	// Accessors.
	EShadowMapInteractionType GetType() const { return Type; }

	UShadowMapTexture2D* GetTexture() const
	{
		checkSlow(Type == SMIT_Texture);
		return ShadowTexture;
	}

	const FVector2D& GetCoordinateScale() const
	{
		checkSlow(Type == SMIT_Texture);
		return CoordinateScale;
	}

	const FVector2D& GetCoordinateBias() const
	{
		checkSlow(Type == SMIT_Texture);
		return CoordinateBias;
	}

	bool GetChannelValid(int32 ChannelIndex) const
	{
		checkSlow(Type == SMIT_Texture);
		return bChannelValid[ChannelIndex];
	}

private:

	EShadowMapInteractionType Type;
	UShadowMapTexture2D* ShadowTexture;
	FVector2D CoordinateScale;
	FVector2D CoordinateBias;
	bool bChannelValid[4];
};

/**
 * An interface to cached lighting for a specific mesh.
 */
class FLightCacheInterface
{
public:
	virtual FLightInteraction GetInteraction(const class FLightSceneProxy* LightSceneProxy) const = 0;
	virtual FLightMapInteraction GetLightMapInteraction(ERHIFeatureLevel::Type InFeatureLevel) const = 0;
	virtual FShadowMapInteraction GetShadowMapInteraction() const { return FShadowMapInteraction::None(); }
};

// Information about a single shadow cascade.
class FShadowCascadeSettings
{
public:
	// The following 3 floats represent the view space depth of the split planes for this cascade.
	// SplitNear <= FadePlane <= SplitFar

	// The distance from the camera to the near split plane, in world units (linear).
	float SplitNear;

	// The distance from the camera to the far split plane, in world units (linear).
	float SplitFar;

	// in world units (linear).
	float SplitNearFadeRegion;

	// in world units (linear).
	float SplitFarFadeRegion;

	// ??
	// The distance from the camera to the start of the fade region, in world units (linear).
	// The area between the fade plane and the far split plane is blended to smooth between cascades.
	float FadePlaneOffset;

	// The length of the fade region (SplitFar - FadePlaneOffset), in world units (linear).
	float FadePlaneLength;

	// The accurate bounds of the cascade used for primitive culling.
	FConvexVolume ShadowBoundsAccurate;

	FPlane NearFrustumPlane;
	FPlane FarFrustumPlane;

	FShadowCascadeSettings()
		: SplitNear(0.0f)
		, SplitFar(WORLD_MAX)
		, SplitNearFadeRegion(0.0f)
		, SplitFarFadeRegion(0.0f)
		, FadePlaneOffset(SplitFar)
		, FadePlaneLength(SplitFar - FadePlaneOffset)
	{
	}
};

/** A projected shadow transform. */
class ENGINE_API FProjectedShadowInitializer
{
public:

	/** A translation that is applied to world-space before transforming by one of the shadow matrices. */
	FVector PreShadowTranslation;

	FMatrix WorldToLight;
	/** Non-uniform scale to be applied after WorldToLight. */
	FVector Scales;

	FVector FaceDirection;
	FBoxSphereBounds SubjectBounds;
	FVector4 WAxis;
	float MinLightW;
	float MaxDistanceToCastInLightW;

	/** Whether the shadow is for a directional light. */
	bool bDirectionalLight;

	/** Default constructor. */
	FProjectedShadowInitializer()
	:	bDirectionalLight(false)
	{}
};

/** Information needed to create a per-object projected shadow. */
class ENGINE_API FPerObjectProjectedShadowInitializer : public FProjectedShadowInitializer
{
public:

};

/** Information needed to create a whole scene projected shadow. */
class ENGINE_API FWholeSceneProjectedShadowInitializer : public FProjectedShadowInitializer
{
public:
	int32 SplitIndex;
	
	FShadowCascadeSettings CascadeSettings;

	/** Whether the shadow is a point light shadow that renders all faces of a cubemap in one pass. */
	bool bOnePassPointLightShadow;

	/** Whether the shadow will be computed by ray tracing the distance field. */
	bool bRayTracedDistanceFieldShadow;

	FWholeSceneProjectedShadowInitializer()
	:	SplitIndex(INDEX_NONE)
	,	bOnePassPointLightShadow(false)
	,	bRayTracedDistanceFieldShadow(false)
	{}	
};

inline bool DoesPlatformSupportDistanceFieldShadowing(EShaderPlatform Platform)
{
	// Hasn't been tested elsewhere yet
	return Platform == SP_PCD3D_SM5;
}

/** Represents a USkyLightComponent to the rendering thread. */
class ENGINE_API FSkyLightSceneProxy
{
public:

	/** Initialization constructor. */
	FSkyLightSceneProxy(const class USkyLightComponent* InLightComponent);

	const USkyLightComponent* LightComponent;
	FTexture* ProcessedTexture;
	float SkyDistanceThreshold;
	bool bCastShadows;
	bool bWantsStaticShadowing;
	bool bPrecomputedLightingIsValid;
	bool bHasStaticLighting;
	FLinearColor LightColor;
	FSHVectorRGB3 IrradianceEnvironmentMap;
	float OcclusionMaxDistance;
	float Contrast;
	float MinOcclusion;
	FLinearColor OcclusionTint;
};


/** Encapsulates the data which is used to render a light parallel to the game thread. */
class ENGINE_API FLightSceneProxy
{
public:

	/** Initialization constructor. */
	FLightSceneProxy(const ULightComponent* InLightComponent);

	virtual ~FLightSceneProxy() 
	{
	}

	/**
	 * Tests whether the light affects the given bounding volume.
	 * @param Bounds - The bounding volume to test.
	 * @return True if the light affects the bounding volume
	 */
	virtual bool AffectsBounds(const FBoxSphereBounds& Bounds) const
	{
		return true;
	}

	virtual FSphere GetBoundingSphere() const
	{
		// Directional lights will have a radius of WORLD_MAX
		return FSphere(GetPosition(), FMath::Min(GetRadius(), (float)WORLD_MAX));
	}

	/** @return radius of the light */
	virtual float GetRadius() const { return FLT_MAX; }
	virtual float GetOuterConeAngle() const { return 0.0f; }
	virtual float GetSourceRadius() const { return 0.0f; }
	virtual bool IsInverseSquared() const { return false; }
	virtual float GetLightSourceAngle() const { return 0.0f; }

	virtual FVector2D GetLightShaftConeParams() const
	{
		return FVector2D::ZeroVector;
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FVector4& LightPositionAndInvRadius, FVector4& LightColorAndFalloffExponent, FVector& NormalizedLightDirection, FVector2D& SpotAngles, float& LightSourceRadius, float& LightSourceLength, float& LightMinRoughness) const {}

	virtual FVector2D GetDirectionalLightDistanceFadeParameters(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		return FVector2D(0, 0);
	}

	virtual bool GetLightShaftOcclusionParameters(float& OutOcclusionMaskDarkness, float& OutOcclusionDepthRange) const
	{
		OutOcclusionMaskDarkness = 0;
		OutOcclusionDepthRange = 1;
		return false;
	}

	virtual FVector GetLightPositionForLightShafts(FVector ViewOrigin) const
	{
		return GetPosition();
	}

	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<class FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const
	{
		return false;
	}

	/** Called when precomputed lighting has been determined to be invalid */
	virtual void InvalidatePrecomputedLighting(bool bIsEditor) {}

	/** Whether this light should create per object shadows for dynamic objects. */
	virtual bool ShouldCreatePerObjectShadowsForDynamicObjects() const;

	virtual int32 GetNumViewDependentWholeSceneShadows(const FSceneView& View) const { return 0; }

	/**
	 * Sets up a projected shadow initializer that's dependent on the current view for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetViewDependentWholeSceneProjectedShadowInitializer(
		const class FSceneView& View, 
		int32 SplitIndex,
		class FWholeSceneProjectedShadowInitializer& OutInitializer) const
	{
		return false;
	}

	/**
	 * Sets up a projected shadow initializer for a reflective shadow map that's dependent on the current view for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetViewDependentRsmWholeSceneProjectedShadowInitializer(
		const class FSceneView& View, 
		const FBox& LightPropagationVolumeBounds,
		class FWholeSceneProjectedShadowInitializer& OutInitializer ) const
	{
		return false;
	}

	/**
	 * Sets up a projected shadow initializer for the given subject.
	 * @param SubjectBounds - The bounding volume of the subject.
	 * @param OutInitializer - Upon successful return, contains the initialization parameters for the shadow.
	 * @return True if a projected shadow should be cast by this subject-light pair.
	 */
	virtual bool GetPerObjectProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds,class FPerObjectProjectedShadowInitializer& OutInitializer) const
	{
		return false;
	}

	// @param OutCascadeSettings can be 0
	virtual FSphere GetShadowSplitBounds(const class FSceneView& View, int32 SplitIndex, FShadowCascadeSettings* OutCascadeSettings) const { return FSphere(FVector::ZeroVector, 0); }

	virtual bool GetScissorRect(FIntRect& ScissorRect, const FSceneView& View) const
	{
		ScissorRect = View.ViewRect;
		return false;
	}

	virtual void SetScissorRect(FRHICommandList& RHICmdList, const FSceneView& View) const
	{
	}

	// Accessors.
	float GetUserShadowBias() const { return ShadowBias; }

	/** 
	 * Note: The Rendering thread must not dereference UObjects!  
	 * The game thread owns UObject state and may be writing to them at any time.
	 * Mirror the data in the scene proxy and access that instead.
	 */
	inline const ULightComponent* GetLightComponent() const { return LightComponent; }
	inline FLightSceneInfo* GetLightSceneInfo() const { return LightSceneInfo; }
	inline const FMatrix& GetWorldToLight() const { return WorldToLight; }
	inline const FMatrix& GetLightToWorld() const { return LightToWorld; }
	inline FVector GetDirection() const { return FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]); }
	inline FVector GetOrigin() const { return LightToWorld.GetOrigin(); }
	inline FVector4 GetPosition() const { return Position; }
	inline const FLinearColor& GetColor() const { return Color; }
	inline float GetIndirectLightingScale() const { return IndirectLightingScale; }
	inline FGuid GetLightGuid() const { return LightGuid; }
	inline float GetShadowSharpen() const { return ShadowSharpen; }
	inline FVector GetLightFunctionScale() const { return LightFunctionScale; }
	inline float GetLightFunctionFadeDistance() const { return LightFunctionFadeDistance; }
	inline float GetLightFunctionDisabledBrightness() const { return LightFunctionDisabledBrightness; }
	inline UTextureLightProfile* GetIESTexture() const { return IESTexture; }
	inline FTexture* GetIESTextureResource() const { return IESTexture ? IESTexture->Resource : 0; }
	inline const FMaterialRenderProxy* GetLightFunctionMaterial() const { return LightFunctionMaterial; }
	inline bool HasStaticLighting() const { return bStaticLighting; }
	inline bool HasStaticShadowing() const { return bStaticShadowing; }
	inline bool CastsDynamicShadow() const { return bCastDynamicShadow; }
	inline bool CastsStaticShadow() const { return bCastStaticShadow; }
	inline bool CastsTranslucentShadows() const { return bCastTranslucentShadows; }
	inline bool AffectsTranslucentLighting() const { return bAffectTranslucentLighting; }
	inline bool UseRayTracedDistanceFieldShadows() const { return bUseRayTracedDistanceFieldShadows; }
	inline uint8 GetLightType() const { return LightType; }
	inline FName GetComponentName() const { return ComponentName; }
	inline FName GetLevelName() const { return LevelName; }
	FORCEINLINE TStatId GetStatId() const 
	{ 
		return StatId; 
	}	
	inline int32 GetShadowMapChannel() const { return ShadowMapChannel; }
	inline bool IsUsedAsAtmosphereSunLight() const { return bUsedAsAtmosphereSunLight; }
	inline int32 GetPreviewShadowMapChannel() const { return PreviewShadowMapChannel; }

	inline bool HasReflectiveShadowMap() const { return bHasReflectiveShadowMap; }
	inline bool NeedsLPVInjection() const { return bAffectDynamicIndirectLighting; }
	inline const class FStaticShadowDepthMap* GetStaticShadowDepthMap() const { return StaticShadowDepthMap; }

	/**
	 * Shifts light position and all relevant data by an arbitrary delta.
	 * Called on world origin changes
	 * @param InOffset - The delta to shift by
	 */
	virtual void ApplyWorldOffset(FVector InOffset);

protected:

	friend class FScene;

	/** The light component. */
	const ULightComponent* LightComponent;

	/** The light's scene info. */
	class FLightSceneInfo* LightSceneInfo;

	/** A transform from world space into light space. */
	FMatrix WorldToLight;

	/** A transform from light space into world space. */
	FMatrix LightToWorld;

	/** The homogenous position of the light. */
	FVector4 Position;

	/** The light color. */
	FLinearColor Color;

	/** Scale for indirect lighting from this light.  When 0, indirect lighting is disabled. */
	float IndirectLightingScale;

	/** User setting from light component, 0:no bias, 0.5:reasonable, larger object might appear to float */
	float ShadowBias;

	/** Sharpen shadow filtering */
	float ShadowSharpen;

	/** Min roughness */
	float MinRoughness;

	/** The light's persistent shadowing GUID. */
	FGuid LightGuid;

	/** 
	 * Shadow map channel which is used to match up with the appropriate static shadowing during a deferred shading pass.
	 * This is generated during a lighting build.
	 */
	int32 ShadowMapChannel;

	/** Transient shadowmap channel used to preview the results of stationary light shadowmap packing. */
	int32 PreviewShadowMapChannel;

	const class FStaticShadowDepthMap* StaticShadowDepthMap;

	/** Light function parameters. */
	FVector	LightFunctionScale;
	float LightFunctionFadeDistance;
	float LightFunctionDisabledBrightness;
	const FMaterialRenderProxy* LightFunctionMaterial;

	/**
	 * IES texture (light profiles from real world measured data)
	 * We are safe to store a U pointer as those objects get deleted deferred, storing an FTexture pointer would crash if we recreate the texture 
	 */
	UTextureLightProfile* IESTexture;

	/**
	 * Return True if a light's parameters as well as its position is static during gameplay, and can thus use static lighting.
	 * A light with HasStaticLighting() == true will always have HasStaticShadowing() == true as well.
	 */
	const uint32 bStaticLighting : 1;

	/** 
	 * Whether the light has static direct shadowing.  
	 * The light may still have dynamic brightness and color. 
	 * The light may or may not also have static lighting.
	 */
	const uint32 bStaticShadowing : 1;

	/** True if the light casts dynamic shadows. */
	const uint32 bCastDynamicShadow : 1;

	/** True if the light casts static shadows. */
	const uint32 bCastStaticShadow : 1;

	/** Whether the light is allowed to cast dynamic shadows from translucency. */
	const uint32 bCastTranslucentShadows : 1;

	/** Whether the light affects translucency or not.  Disabling this can save GPU time when there are many small lights. */
	const uint32 bAffectTranslucentLighting : 1;

	/** Whether to consider light as a sunlight for atmospheric scattering and exponential height fog. */
	const uint32 bUsedAsAtmosphereSunLight : 1;

	/** Does the light have dynamic GI? */
	const uint32 bAffectDynamicIndirectLighting : 1;
	const uint32 bHasReflectiveShadowMap : 1;

	/** Whether to use ray traced distance field area shadows. */
	const uint32 bUseRayTracedDistanceFieldShadows : 1;

	/** The light type (ELightComponentType) */
	const uint8 LightType;

	/** The name of the light component. */
	FName ComponentName;

	/** The name of the level the light is in. */
	FName LevelName;

	/** Used for dynamic stats */
	TStatId StatId;

	/**
	 * Updates the light proxy's cached transforms.
	 * @param InLightToWorld - The new light-to-world transform.
	 * @param InPosition - The new position of the light.
	 */
	void SetTransform(const FMatrix& InLightToWorld,const FVector4& InPosition);

	/** Updates the light's color. */
	void SetColor(const FLinearColor& InColor);
};


/** Encapsulates the data which is used to render a decal parallel to the game thread. */
class ENGINE_API FDeferredDecalProxy
{
public:
	/** constructor */
	FDeferredDecalProxy(const UDecalComponent* InComponent);

	/**
	 * Updates the decal proxy's cached transform.
	 * @param InComponentToWorld - The new component-to-world transform.
	 */
	void SetTransform(const FTransform& InComponentToWorld);

	/** Pointer back to the game thread decal component. */
	const UDecalComponent* Component;

	UMaterialInterface* DecalMaterial;

	/** Used to compute the projection matrix on the render thread side  */
	FTransform ComponentTrans;

	/** 
	 * Whether the decal should be drawn or not
	 * This has to be passed to the rendering thread to handle G mode in the editor, where there is no game world, but we don't want to show components with HiddenGame set. 
	 */
	bool DrawInGame;

	bool bOwnerSelected;

	/** Larger values draw later (on top). */
	int32 SortOrder;
};

/** Reflection capture shapes. */
namespace EReflectionCaptureShape
{
	enum Type
	{
		Sphere,
		Box,
		Plane,
		Num
	};
}

/** Represents a reflection capture to the renderer. */
class ENGINE_API FReflectionCaptureProxy
{
public:
	const class UReflectionCaptureComponent* Component;

	int32 PackedIndex;

	/** Used in Feature level SM4 */
	FTexture* SM4FullHDRCubemap;

	/** Used in Feature level ES2 */
	FTexture* EncodedHDRCubemap;

	EReflectionCaptureShape::Type Shape;

	// Properties shared among all shapes
	FVector Position;
	float InfluenceRadius;
	float Brightness;
	uint32 Guid;

	// Box properties
	FMatrix BoxTransform;
	FVector BoxScales;
	float BoxTransitionDistance;

	// Plane properties
	FPlane ReflectionPlane;
	FVector4 ReflectionXAxisAndYScale;

	FReflectionCaptureProxy(const class UReflectionCaptureComponent* InComponent);

	void SetTransform(const FMatrix& InTransform);
};

/** Represents a wind source component to the scene manager in the rendering thread. */
class ENGINE_API FWindSourceSceneProxy
{
public:

	/** Initialization constructor. */
	FWindSourceSceneProxy(const FVector& InDirection,float InStrength,float InSpeed):
	  Position(FVector::ZeroVector),
		  Direction(InDirection),
		  Strength(InStrength),
		  Speed(InSpeed),
		  Radius(0),
		  bIsPointSource(false)
	  {}

	  /** Initialization constructor. */
	  FWindSourceSceneProxy(const FVector& InPosition,float InStrength,float InSpeed,float InRadius):
	  Position(InPosition),
		  Direction(FVector::ZeroVector),
		  Strength(InStrength),
		  Speed(InSpeed),
		  Radius(InRadius),
		  bIsPointSource(true)
	  {}

	  bool GetWindParameters(const FVector& EvaluatePosition, FVector4& WindDirectionAndSpeed, float& Strength) const;
	  bool GetDirectionalWindParameters(FVector4& WindDirectionAndSpeed, float& Strength) const;
	  void ApplyWorldOffset(FVector InOffset);

private:

	FVector Position;
	FVector	Direction;
	float Strength;
	float Speed;
	float Radius;
	bool bIsPointSource;
};




/**
 * An interface implemented by dynamic resources which need to be initialized and cleaned up by the rendering thread.
 */
class FDynamicPrimitiveResource
{
public:

	virtual void InitPrimitiveResource() = 0;
	virtual void ReleasePrimitiveResource() = 0;
};

/**
 * The base interface used to query a primitive for its dynamic elements.
 */
class FPrimitiveDrawInterface
{
public:

	const FSceneView* const View;

	/** Initialization constructor. */
	FPrimitiveDrawInterface(const FSceneView* InView):
		View(InView)
	{}

	virtual bool IsHitTesting() = 0;
	virtual void SetHitProxy(HHitProxy* HitProxy) = 0;

	virtual void RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource) = 0;

	virtual void AddReserveLines(uint8 DepthPriorityGroup, int32 NumLines, bool bDepthBiased = false, bool bThickLines = false) = 0;

	virtual void DrawSprite(
		const FVector& Position,
		float SizeX,
		float SizeY,
		const FTexture* Sprite,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float U,
		float UL,
		float V,
		float VL,
		uint8 BlendMode = 1 /*SE_BLEND_Masked*/
		) = 0;

	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float Thickness = 0.0f,
		float DepthBias = 0.0f,
		bool bScreenSpace = false
		) = 0;

	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		float PointSize,
		uint8 DepthPriorityGroup
		) = 0;

	/**
	 * Draw a mesh element.
	 * This should only be called through the DrawMesh function.
	 *
	 * @return Number of passes rendered for the mesh
	 */
	virtual int32 DrawMesh(const FMeshBatch& Mesh) = 0;
};

/**
 * An interface to a scene interaction.
 */
class ENGINE_API FViewElementDrawer
{
public:

	/**
	 * Draws the interaction using the given draw interface.
	 */
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) {}
};

/**
 * An interface used to query a primitive for its static elements.
 */
class FStaticPrimitiveDrawInterface
{
public:
	virtual void SetHitProxy(HHitProxy* HitProxy) = 0;
	virtual void DrawMesh(
		const FMeshBatch& Mesh,
		float ScreenSize,
		bool bShadowOnly = false
		) = 0;
};

/** Primitive draw interface implementation used to store primitives requested to be drawn when gathering dynamic mesh elements. */
class ENGINE_API FSimpleElementCollector : public FPrimitiveDrawInterface
{
public:

	FSimpleElementCollector();
	~FSimpleElementCollector();

	virtual void SetHitProxy(HHitProxy* HitProxy);
	virtual void AddReserveLines(uint8 DepthPriorityGroup, int32 NumLines, bool bDepthBiased = false, bool bThickLines = false) {}

	virtual void DrawSprite(
		const FVector& Position,
		float SizeX,
		float SizeY,
		const FTexture* Sprite,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float U,
		float UL,
		float V,
		float VL,
		uint8 BlendMode = SE_BLEND_Masked
		) override;

	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		uint8 DepthPriorityGroup,
		float Thickness = 0.0f,
		float DepthBias = 0.0f,
		bool bScreenSpace = false
		) override;

	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		float PointSize,
		uint8 DepthPriorityGroup
		) override;

	virtual void RegisterDynamicResource(FDynamicPrimitiveResource* DynamicResource) override;

	// Not supported
	virtual bool IsHitTesting() 
	{ 
		static bool bTriggered = false;

		if (!bTriggered)
		{
			bTriggered = true;
			ensureMsg(false, TEXT("FSimpleElementCollector::DrawMesh called"));
		}

		return false; 
	}

	// Not supported
	virtual int32 DrawMesh(const FMeshBatch& Mesh) 
	{
		static bool bTriggered = false;

		if (!bTriggered)
		{
			bTriggered = true;
			ensureMsg(false, TEXT("FSimpleElementCollector::DrawMesh called"));
		}

		return 0;
	}

	void DrawBatchedElements(FRHICommandList& RHICmdList, const FSceneView& View, FTexture2DRHIRef DepthTexture, EBlendModeFilter::Type Filter) const;

	/** The batched simple elements. */
	FBatchedElements BatchedElements;

private:

	FHitProxyId HitProxyId;

	bool bIsMobileHDR;

	/** The dynamic resources which have been registered with this drawer. */
	TArray<FDynamicPrimitiveResource*,SceneRenderingAllocator> DynamicResources;

	friend class FMeshElementCollector;
};

/** 
 * Base class for a resource allocated from a FMeshElementCollector with AllocateOneFrameResource, which the collector releases.
 * This is useful for per-frame structures which are referenced by a mesh batch given to the FMeshElementCollector.
 */
class FOneFrameResource
{
public:

	virtual ~FOneFrameResource() {}
};

/** 
 * A reference to a mesh batch that is added to the collector, together with some cached relevance flags. 
 */
struct FMeshBatchAndRelevance
{
	const FMeshBatch* Mesh;

	/** The render info for the primitive which created this mesh, required. */
	const FPrimitiveSceneProxy* PrimitiveSceneProxy;

	/** 
	 * Cached usage information to speed up traversal in the most costly passes (depth-only, base pass, shadow depth), 
	 * This is done so the Mesh does not have to be dereferenced to determine pass relevance. 
	 */
	uint32 bHasOpaqueOrMaskedMaterial : 1;
	uint32 bRenderInMainPass : 1;

	FMeshBatchAndRelevance(const FMeshBatch& InMesh, const FPrimitiveSceneProxy* InPrimitiveSceneProxy, ERHIFeatureLevel::Type FeatureLevel);
};

/** 
 * Encapsulates the gathering of meshes from the various FPrimitiveSceneProxy classes. 
 */
class FMeshElementCollector
{
public:

	/** Accesses the PDI for drawing lines, sprites, etc. */
	inline FPrimitiveDrawInterface* GetPDI(int32 ViewIndex)
	{
		return SimpleElementCollectors[ViewIndex];
	}

	/** 
	 * Allocates an FMeshBatch that can be safely referenced by the collector (lifetime will be long enough).
	 * Returns a reference that will not be invalidated due to further AllocateMesh() calls.
	 */
	inline FMeshBatch& AllocateMesh()
	{
		return *(new (MeshBatchStorage) FMeshBatch());
	}

	/** 
	 * Adds a mesh batch to the collector for the specified view so that it can be rendered.
	 */
	ENGINE_API void AddMesh(int32 ViewIndex, FMeshBatch& MeshBatch);

	/** Add a material render proxy that will be cleaned up automatically */
	void RegisterOneFrameMaterialProxy(FMaterialRenderProxy* Proxy)
	{
		TemporaryProxies.Add(Proxy);
	}

	/** Allocates a temporary resource that is safe to be referenced by an FMeshBatch added to the collector. */
	template<typename T>
	T& AllocateOneFrameResource()
	{
		T* OneFrameResource = new (FMemStack::Get()) T();
		OneFrameResources.Add(OneFrameResource);
		return *OneFrameResource;
	}

private:

	FMeshElementCollector() :
		PrimitiveSceneProxy(NULL),
		FeatureLevel(ERHIFeatureLevel::Num)
	{}

	~FMeshElementCollector()
	{
		for (int32 ProxyIndex = 0; ProxyIndex < TemporaryProxies.Num(); ProxyIndex++)
		{
			delete TemporaryProxies[ProxyIndex];
		}

		// SceneRenderingAllocator does not handle destructors
		for (int32 ResourceIndex = 0; ResourceIndex < OneFrameResources.Num(); ResourceIndex++)
		{
			OneFrameResources[ResourceIndex]->~FOneFrameResource();
		}
	}

	void SetPrimitive(const FPrimitiveSceneProxy* InPrimitiveSceneProxy, FHitProxyId DefaultHitProxyId)
	{
		check(InPrimitiveSceneProxy);
		PrimitiveSceneProxy = InPrimitiveSceneProxy;

		for (int32 ViewIndex = 0; ViewIndex < SimpleElementCollectors.Num(); ViewIndex++)
		{
			SimpleElementCollectors[ViewIndex]->HitProxyId = DefaultHitProxyId;
		}
	}

	void ClearViewMeshArrays()
	{
		Views.Empty();
		MeshBatches.Empty();
		SimpleElementCollectors.Empty();
	}

	void AddViewMeshArrays(
		FSceneView* InView, 
		TArray<FMeshBatchAndRelevance,SceneRenderingAllocator>* ViewMeshes,
		FSimpleElementCollector* ViewSimpleElementCollector, 
		ERHIFeatureLevel::Type InFeatureLevel)
	{
		Views.Add(InView);
		MeshBatches.Add(ViewMeshes);
		SimpleElementCollectors.Add(ViewSimpleElementCollector);
		FeatureLevel = InFeatureLevel;
	}

	/** 
	 * Using TChunkedArray which will never realloc as new elements are added
	 * @todo - use mem stack
	 */
	TChunkedArray<FMeshBatch> MeshBatchStorage;

	/** Meshes to render */
	TArray<TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>*, TInlineAllocator<2> > MeshBatches;

	/** PDIs */
	TArray<FSimpleElementCollector*, TInlineAllocator<2> > SimpleElementCollectors;

	/** Views being collected for */
	TArray<FSceneView*, TInlineAllocator<2> > Views;

	/** Material proxies that will be deleted at the end of the frame. */
	TArray<FMaterialRenderProxy*, SceneRenderingAllocator> TemporaryProxies;

	/** Resources that will be deleted at the end of the frame. */
	TArray<FOneFrameResource*, SceneRenderingAllocator> OneFrameResources;

	/** Current primitive being gathered. */
	const FPrimitiveSceneProxy* PrimitiveSceneProxy;

	ERHIFeatureLevel::Type FeatureLevel;

	friend class FSceneRenderer;
	friend class FProjectedShadowInfo;
};


/**
 *	Helper structure for storing motion blur information for a primitive
 */
struct FMotionBlurInfo
{
	FMotionBlurInfo(FPrimitiveComponentId InComponentId, FPrimitiveSceneInfo* InPrimitiveSceneInfo)
		: ComponentId(InComponentId), MBPrimitiveSceneInfo(InPrimitiveSceneInfo), bKeepAndUpdateThisFrame(true)
	{
	}

	/**  */
	void UpdateMotionBlurInfo();

	/** Call if you want to keep the existing motionblur */
	void RestoreForPausedMotionBlur();

	void SetKeepAndUpdateThisFrame(bool bValue = true)
	{
		if(bValue)
		{
			// we update right away so when it comes to HasVelocity this frame we detect no movement and next frame we actually render it with correct velocity
			UpdateMotionBlurInfo();
		}

		bKeepAndUpdateThisFrame = bValue;
	}

	bool GetKeepAndUpdateThisFrame() const
	{
		return bKeepAndUpdateThisFrame; 
	}

	FMatrix GetPreviousLocalToWorld() const
	{
		return PreviousLocalToWorld;
	}

	FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const
	{
		return MBPrimitiveSceneInfo;
	}

	void SetPrimitiveSceneInfo(FPrimitiveSceneInfo* Value)
	{
		MBPrimitiveSceneInfo = Value;
	}

	void ApplyOffset(FVector InOffset)
	{
		PreviousLocalToWorld.SetOrigin(PreviousLocalToWorld.GetOrigin() + InOffset);
		PausedLocalToWorld.SetOrigin(PausedLocalToWorld.GetOrigin() + InOffset);
	}

private:
	/** The component this info represents. */
	FPrimitiveComponentId ComponentId;
	/** The primitive scene info for the component.	*/
	FPrimitiveSceneInfo* MBPrimitiveSceneInfo;
	/** The previous LocalToWorld of the component.	*/
	FMatrix	PreviousLocalToWorld;
	/** Used in case when Pause is activate. */
	FMatrix	PausedLocalToWorld;
	/** if true then the PreviousLocalToWorld has already been updated for the current frame */
	bool bKeepAndUpdateThisFrame;
};

// stored in the scene, can be shared for multiple views
class FMotionBlurInfoData
{
public:

	// constructor
	FMotionBlurInfoData();
	/** 
	 *	Set the primitives motion blur info
	 * 
	 *	@param PrimitiveSceneInfo The primitive to add, must not be 0
	 */
	void UpdatePrimitiveMotionBlur(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** 
	 *	Unsets the primitives motion blur info
	 * 
	 *	@param PrimitiveSceneInfo The primitive to remove, must not be 0
	 */
	void RemovePrimitiveMotionBlur(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/**
	 * Creates any needed motion blur infos if needed and saves the transforms of the frame we just completed
	 * called in RenderFinish()
	 * @param InScene must not be 0
	 */
	void UpdateMotionBlurCache(class FScene* InScene);

	/**
	 * Call if you want to keep the existing motionblur
	 */
	void RestoreForPausedMotionBlur();

	/** 
	 *	Get the primitives motion blur info
	 * 
	 *	@param	PrimitiveSceneInfo	The primitive to retrieve the motion blur info for
	 *
	 *	@return	bool				true if the primitive info was found and set
	 */
	bool GetPrimitiveMotionBlurInfo(const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMatrix& OutPreviousLocalToWorld);

	/** Request to clear all stored motion blur data for this scene. */
	void SetClearMotionBlurInfo();

	/**
	 * Shifts motion blur data by arbitrary delta
	 */
	void ApplyOffset(FVector InOffset);

private:
	/** The motion blur info entries for the frame. Accessed on Renderthread only! */
	TMap<FPrimitiveComponentId, FMotionBlurInfo> MotionBlurInfos;
	/** */
	bool bShouldClearMotionBlurInfo;

	/**
	 * O(n) with the amount of motion blurred objects but that number should be low
	 * @return 0 if not found, otherwise pointer into MotionBlurInfos, don't store for longer
	 */
	FMotionBlurInfo* FindMBInfoIndex(FPrimitiveComponentId ComponentId);
};



//
// Primitive drawing utility functions.
//

// Solid shape drawing utility functions. Not really designed for speed - more for debugging.
// These utilities functions are implemented in UnScene.cpp using GetTRI.

// 10x10 tessellated plane at x=-1..1 y=-1...1 z=0
extern ENGINE_API void DrawPlane10x10(class FPrimitiveDrawInterface* PDI,const FMatrix& ObjectToWorld,float Radii,FVector2D UVMin, FVector2D UVMax,const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriority);
extern ENGINE_API void DrawBox(class FPrimitiveDrawInterface* PDI,const FMatrix& BoxToWorld,const FVector& Radii,const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriority);
extern ENGINE_API void DrawSphere(class FPrimitiveDrawInterface* PDI,const FVector& Center,const FVector& Radii,int32 NumSides,int32 NumRings,const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriority,bool bDisableBackfaceCulling=false);
extern ENGINE_API void DrawCone(class FPrimitiveDrawInterface* PDI,const FMatrix& ConeToWorld, float Angle1, float Angle2, int32 NumSides, bool bDrawSideLines, const FLinearColor& SideLineColor, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority);

extern ENGINE_API void DrawCylinder(class FPrimitiveDrawInterface* PDI,const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis,
	float Radius, float HalfHeight, int32 Sides, const FMaterialRenderProxy* MaterialInstance, uint8 DepthPriority);

extern ENGINE_API void DrawCylinder(class FPrimitiveDrawInterface* PDI, const FMatrix& CylToWorld, const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis,
	float Radius, float HalfHeight, int32 Sides, const FMaterialRenderProxy* MaterialInstance, uint8 DepthPriority);

extern ENGINE_API void GetBoxMesh(const FMatrix& BoxToWorld,const FVector& Radii,const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriority,int32 ViewIndex,FMeshElementCollector& Collector);
extern ENGINE_API void GetSphereMesh(const FVector& Center, const FVector& Radii, int32 NumSides, int32 NumRings, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority, bool bDisableBackfaceCulling, int32 ViewIndex, FMeshElementCollector& Collector);
extern ENGINE_API void GetHalfSphereMesh(const FVector& Center, const FVector& Radii, int32 NumSides, int32 NumRings, float StartAngle, float EndAngle, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority, bool bDisableBackfaceCulling, int32 ViewIndex, FMeshElementCollector& Collector);
extern ENGINE_API void GetCylinderMesh(const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis,
									float Radius, float HalfHeight, int32 Sides, const FMaterialRenderProxy* MaterialInstance, uint8 DepthPriority, int32 ViewIndex, FMeshElementCollector& Collector);
extern ENGINE_API void GetCylinderMesh(const FMatrix& CylToWorld, const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis,
									float Radius, float HalfHeight, int32 Sides, const FMaterialRenderProxy* MaterialInstance, uint8 DepthPriority, int32 ViewIndex, FMeshElementCollector& Collector);
extern ENGINE_API void GetConeMesh(const FMatrix& LocalToWorld, float AngleWidth, float AngleHeight, int32 NumSides,
									const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority, int32 ViewIndex, FMeshElementCollector& Collector);
extern ENGINE_API void GetCapsuleMesh(const FVector& Origin, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis, const FLinearColor& Color, float Radius, float HalfHeight, int32 NumSides,
									const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority, bool bDisableBackfaceCulling, int32 ViewIndex, FMeshElementCollector& Collector);


extern ENGINE_API void DrawDisc(class FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& XAxis,const FVector& YAxis,FColor Color,float Radius,int32 NumSides, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority);
extern ENGINE_API void DrawFlatArrow(class FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& XAxis,const FVector& YAxis,FColor Color,float Length,int32 Width, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority);

// Line drawing utility functions.
extern ENGINE_API void DrawWireBox(class FPrimitiveDrawInterface* PDI, const FBox& Box, const FLinearColor& Color, uint8 DepthPriority, float Thickness = 0, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawCircle(class FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, const FLinearColor& Color, float Radius, int32 NumSides, uint8 DepthPriority, float Thickness = 0, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawArc(FPrimitiveDrawInterface* PDI, const FVector Base, const FVector X, const FVector Y, const float MinAngle, const float MaxAngle, const float Radius, const int32 Sections, const FLinearColor& Color, uint8 DepthPriority);
extern ENGINE_API void DrawWireSphere(class FPrimitiveDrawInterface* PDI, const FVector& Base, const FLinearColor& Color, float Radius, int32 NumSides, uint8 DepthPriority, float Thickness = 0, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawWireSphere(class FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FLinearColor& Color, float Radius, int32 NumSides, uint8 DepthPriority, float Thickness = 0, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawWireSphereAutoSides(class FPrimitiveDrawInterface* PDI, const FVector& Base, const FLinearColor& Color, float Radius, uint8 DepthPriority, float Thickness = 0, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawWireSphereAutoSides(class FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FLinearColor& Color, float Radius, uint8 DepthPriority, float Thickness = 0, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawWireCylinder(class FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, const FVector& Z, const FLinearColor& Color, float Radius, float HalfHeight, int32 NumSides, uint8 DepthPriority, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawWireCapsule(class FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, const FVector& Z, const FLinearColor& Color, float Radius, float HalfHeight, int32 NumSides, uint8 DepthPriority, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawWireChoppedCone(class FPrimitiveDrawInterface* PDI,const FVector& Base,const FVector& X,const FVector& Y,const FVector& Z,const FLinearColor& Color,float Radius,float TopRadius,float HalfHeight,int32 NumSides,uint8 DepthPriority);
extern ENGINE_API void DrawWireCone(class FPrimitiveDrawInterface* PDI, TArray<FVector>& Verts, const FMatrix& Transform, float ConeRadius, float ConeAngle, int32 ConeSides, const FLinearColor& Color, uint8 DepthPriority, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawWireCone(class FPrimitiveDrawInterface* PDI, TArray<FVector>& Verts, const FTransform& Transform, float ConeRadius, float ConeAngle, int32 ConeSides, const FLinearColor& Color, uint8 DepthPriority, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawWireSphereCappedCone(FPrimitiveDrawInterface* PDI, const FTransform& Transform, float ConeRadius, float ConeAngle, int32 ConeSides, int32 ArcFrequency, int32 CapSegments, const FLinearColor& Color, uint8 DepthPriority);
extern ENGINE_API void DrawOrientedWireBox(class FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, const FVector& Z, FVector Extent, const FLinearColor& Color, uint8 DepthPriority, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false);
extern ENGINE_API void DrawDirectionalArrow(class FPrimitiveDrawInterface* PDI,const FMatrix& ArrowToWorld,const FLinearColor& InColor,float Length,float ArrowSize,uint8 DepthPriority);
extern ENGINE_API void DrawConnectedArrow(class FPrimitiveDrawInterface* PDI, const FMatrix& ArrowToWorld, const FLinearColor& Color, float ArrowHeight, float ArrowWidth, uint8 DepthPriority, float Thickness = 0.5f, int32 NumSpokes = 6);
extern ENGINE_API void DrawWireStar(class FPrimitiveDrawInterface* PDI,const FVector& Position, float Size, const FLinearColor& Color,uint8 DepthPriority);
extern ENGINE_API void DrawDashedLine(class FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const FLinearColor& Color, float DashSize, uint8 DepthPriority, float DepthBias = 0.0f);
extern ENGINE_API void DrawWireDiamond(class FPrimitiveDrawInterface* PDI,const FMatrix& DiamondMatrix, float Size, const FLinearColor& InColor,uint8 DepthPriority);
extern ENGINE_API void DrawCoordinateSystem(FPrimitiveDrawInterface* PDI, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, uint8 DepthPriority);

/**
 * Draws a wireframe of the bounds of a frustum as defined by a transform from clip-space into world-space.
 * @param PDI - The interface to draw the wireframe.
 * @param FrustumToWorld - A transform from clip-space to world-space that defines the frustum.
 * @param Color - The color to draw the wireframe with.
 * @param DepthPriority - The depth priority group to draw the wireframe with.
 */
extern ENGINE_API void DrawFrustumWireframe(
	FPrimitiveDrawInterface* PDI,
	const FMatrix& WorldToFrustum,
	FColor Color,
	uint8 DepthPriority
	);

void BuildConeVerts(float Angle1, float Angle2, float Scale, float XOffset, int32 NumSides, TArray<FDynamicMeshVertex>& OutVerts, TArray<int32>& OutIndices);

void BuildCylinderVerts(const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis, float Radius, float HalfHeight, int32 Sides, TArray<FDynamicMeshVertex>& OutVerts, TArray<int32>& OutIndices);


/**
 * Given a base color and a selection state, returns a color which accounts for the selection state.
 * @param BaseColor - The base color of the object.
 * @param bSelected - The selection state of the object.
 * @param bHovered - True if the object has hover focus
 * @param bUseOverlayIntensity - True if the selection color should be modified by the selection intensity
 * @return The color to draw the object with, accounting for the selection state
 */
extern ENGINE_API FLinearColor GetSelectionColor(const FLinearColor& BaseColor,bool bSelected,bool bHovered, bool bUseOverlayIntensity = true);
extern ENGINE_API FLinearColor GetViewSelectionColor(const FLinearColor& BaseColor, const FSceneView& View, bool bSelected, bool bHovered, bool bUseOverlayIntensity, bool bIndividuallySelected);


/** Vertex Color view modes */
namespace EVertexColorViewMode
{
	enum Type
	{
		/** Invalid or undefined */
		Invalid,

		/** Color only */
		Color,
		
		/** Alpha only */
		Alpha,

		/** Red only */
		Red,

		/** Green only */
		Green,

		/** Blue only */
		Blue,
	};
}


/** Global vertex color view mode setting when SHOW_VertexColors show flag is set */
extern ENGINE_API EVertexColorViewMode::Type GVertexColorViewMode;

/**
 * Returns true if the given view is "rich", and all primitives should be forced down the dynamic drawing path so that ApplyViewModeOverrides can implement the rich view feature.
 * A view is rich if is missing the EngineShowFlags.Materials showflag, or has any of the render mode affecting showflags.
 */
extern ENGINE_API bool IsRichView(const FSceneViewFamily& ViewFamily);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/**
	 * true if we debug material names with SCOPED_DRAW_EVENT.
	 * Toggle with "ShowMaterialDrawEvents" console command.
	 */
	extern ENGINE_API bool GShowMaterialDrawEvents;
	extern ENGINE_API void EmitMeshDrawEvents_Inner(FRHICommandList& RHICmdList, const class FPrimitiveSceneProxy* PrimitiveSceneProxy, const struct FMeshBatch& Mesh);
#endif

/** Emits draw events for a given FMeshBatch and the PrimitiveSceneProxy corresponding to that mesh element. */
FORCEINLINE void EmitMeshDrawEvents(FRHICommandList& RHICmdList, const class FPrimitiveSceneProxy* PrimitiveSceneProxy, const struct FMeshBatch& Mesh)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ( GShowMaterialDrawEvents )
	{
		EmitMeshDrawEvents_Inner(RHICmdList, PrimitiveSceneProxy, Mesh);
	}
#endif
}

extern ENGINE_API void ApplyViewModeOverrides(
	int32 ViewIndex,
	const FEngineShowFlags& EngineShowFlags,
	ERHIFeatureLevel::Type FeatureLevel,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	bool bSelected,
	struct FMeshBatch& Mesh,
	FMeshElementCollector& Collector
	);

/** Draws the UV layout of the supplied asset (either StaticMeshRenderData OR SkeletalMeshRenderData, not both!) */
extern ENGINE_API void DrawUVs(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos, const int32 LODLevel, int32 UVChannel, TArray<FVector2D> SelectedEdgeTexCoords, class FStaticMeshRenderData* StaticMeshRenderData, class FStaticLODModel* SkeletalMeshRenderData );

/** Returns true if the Material and Vertex Factory combination require adjacency information. */
ENGINE_API bool RequiresAdjacencyInformation(class UMaterialInterface* Material, const class FVertexFactoryType* VertexFactoryType, ERHIFeatureLevel::Type InFeatureLevel);

/**
 * Computes the screen size of a given sphere bounds in the given view
 * @param Origin - Origin of the bounds in world space
 * @param SphereRadius - Radius of the sphere to use to calculate screen coverage
 * @param View - The view to calculate the display factor for
 * @return float - The screen size calculated
 */
float ENGINE_API ComputeBoundsScreenSize(const FVector4& Origin, const float SphereRadius, const FSceneView& View);

/**
 * Computes the LOD level for the given static meshes render data in the given view.
 * @param RenderData - Render data for the mesh
 * @param Origin - Origin of the bounds of the mesh in world space
 * @param SphereRadius - Radius of the sphere to use to calculate screen coverage
 * @param View - The view to calculate the LOD level for
 */
int8 ENGINE_API ComputeStaticMeshLOD(const FStaticMeshRenderData* RenderData, const FVector4& Origin, const float SphereRadius, const FSceneView& View, float FactorScale = 1.0f);

/**
 * Computes the LOD to render for the list of static meshes in the given view.
 * @param StaticMeshes - List of static meshes.
 * @param View - The view to render the LOD level for 
 * @param Origin - Origin of the bounds of the mesh in world space
 * @param SphereRadius - Radius of the sphere to use to calculate screen coverage
 */
int8 ENGINE_API ComputeLODForMeshes(const TIndirectArray<class FStaticMesh>& StaticMeshes, const FSceneView& View, const FVector4& Origin, float SphereRadius, int32 ForcedLODLevel, float ScreenSizeScale = 1.0f);

class FSharedSamplerState : public FRenderResource
{
public:
	FSamplerStateRHIRef SamplerStateRHI;
	bool bWrap;

	FSharedSamplerState(bool bInWrap) :
		bWrap(bInWrap)
	{}

	virtual void InitRHI() override;

	virtual void ReleaseRHI() override
	{
		SamplerStateRHI.SafeRelease();
	}
};

/** Sampler state using Wrap addressing and taking filter mode from the world texture group. */
extern ENGINE_API FSharedSamplerState* Wrap_WorldGroupSettings;

/** Sampler state using Clamp addressing and taking filter mode from the world texture group. */
extern ENGINE_API FSharedSamplerState* Clamp_WorldGroupSettings;

/** Initializes the shared sampler states. */
extern ENGINE_API void InitializeSharedSamplerStates();