// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/RenderCore/Public/UniformBuffer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "ConvexVolume.h"
#include "FinalPostProcessSettings.h"
#include "SceneInterface.h"
#include "SceneTypes.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"

class FSceneViewStateInterface;
class FViewUniformShaderParameters;
class FViewElementDrawer;
class FSceneViewFamily;

// Projection data for a FSceneView
struct FSceneViewProjectionData
{
	/** The view origin. */
	FVector ViewOrigin;

	/** Rotation matrix transforming from world space to view space. */
	FMatrix ViewRotationMatrix;

	/** UE4 projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix ProjectionMatrix;

protected:
	//The unconstrained (no aspect ratio bars applied) view rectangle (also unscaled)
	FIntRect ViewRect;

	// The constrained view rectangle (identical to UnconstrainedUnscaledViewRect if aspect ratio is not constrained)
	FIntRect ConstrainedViewRect;

public:
	void SetViewRectangle(const FIntRect& InViewRect)
	{
		ViewRect = InViewRect;
		ConstrainedViewRect = InViewRect;
	}

	void SetConstrainedViewRectangle(const FIntRect& InViewRect)
	{
		ConstrainedViewRect = InViewRect;
	}

	bool IsValidViewRectangle() const
	{
		return (ConstrainedViewRect.Min.X >= 0) &&
			(ConstrainedViewRect.Min.Y >= 0) &&
			(ConstrainedViewRect.Width() > 0) &&
			(ConstrainedViewRect.Height() > 0);
	}

	const FIntRect& GetViewRect() const { return ViewRect; }
	const FIntRect& GetConstrainedViewRect() const { return ConstrainedViewRect; }

	FMatrix ComputeViewProjectionMatrix() const
	{
		return FTranslationMatrix(-ViewOrigin) * ViewRotationMatrix * ProjectionMatrix;
	}
};

// Construction parameters for a FSceneView
struct FSceneViewInitOptions : public FSceneViewProjectionData
{
	const FSceneViewFamily* ViewFamily;
	FSceneViewStateInterface* SceneViewStateInterface;
	const AActor* ViewActor;
	FViewElementDrawer* ViewElementDrawer;

	FLinearColor BackgroundColor;
	FLinearColor OverlayColor;
	FLinearColor ColorScale;

	/** For stereoscopic rendering, whether or not this is a full pass, or a left / right eye pass */
	EStereoscopicPass StereoPass;

	/** Conversion from world units (uu) to meters, so we can scale motion to the world appropriately */
	float WorldToMetersScale;

	TSet<FPrimitiveComponentId> HiddenPrimitives;

	// -1,-1 if not setup
	FIntPoint CursorPos;

	float LODDistanceFactor;

	/** If > 0, overrides the view's far clipping plane with a plane at the specified distance. */
	float OverrideFarClippingPlaneDistance;

	// Was there a camera cut this frame?
	bool bInCameraCut;

	// Whether world origin was rebased this frame?
	bool bOriginOffsetThisFrame;

	// Whether to use FOV when computing mesh LOD.
	bool bUseFieldOfViewForLOD;

#if WITH_EDITOR
	// default to 0'th view index, which is a bitfield of 1
	uint64 EditorViewBitflag;

	// this can be specified for ortho views so that it's min draw distance/LOD parenting etc, is controlled by a perspective viewport
	FVector OverrideLODViewOrigin;

	// In case of ortho, generate a fake view position that has a non-zero W component. The view position will be derived based on the view matrix.
	bool bUseFauxOrthoViewPos;
#endif

	FSceneViewInitOptions()
		: ViewFamily(NULL)
		, SceneViewStateInterface(NULL)
		, ViewActor(NULL)
		, ViewElementDrawer(NULL)
		, BackgroundColor(FLinearColor::Transparent)
		, OverlayColor(FLinearColor::Transparent)
		, ColorScale(FLinearColor::White)
		, StereoPass(eSSP_FULL)
		, WorldToMetersScale(100.f)
		, CursorPos(-1, -1)
		, LODDistanceFactor(1.0f)
		, OverrideFarClippingPlaneDistance(-1.0f)
		, bInCameraCut(false)
		, bOriginOffsetThisFrame(false)
		, bUseFieldOfViewForLOD(true)
#if WITH_EDITOR
		, EditorViewBitflag(1)
		, OverrideLODViewOrigin(ForceInitToZero)
		, bUseFauxOrthoViewPos(false)
		//@TODO: , const TBitArray<>& InSpriteCategoryVisibility=TBitArray<>()
#endif
	{
	}
};


//////////////////////////////////////////////////////////////////////////

struct FViewMatrices
{
	FViewMatrices()
	{
		ProjMatrix.SetIdentity();
		ViewMatrix.SetIdentity();
		TranslatedViewProjectionMatrix.SetIdentity();
		InvTranslatedViewProjectionMatrix.SetIdentity();
		GetDynamicMeshElementsShadowCullFrustum = nullptr;
		PreShadowTranslation = FVector::ZeroVector;
		PreViewTranslation = FVector::ZeroVector;
		ViewOrigin = FVector::ZeroVector;
		TemporalAASample = FVector2D::ZeroVector;
	}

	/** ViewToClip : UE4 projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix		ProjMatrix;
	// WorldToView..
	FMatrix		ViewMatrix;
	/** WorldToView with PreViewTranslation. */
	FMatrix		TranslatedViewMatrix;
	/** The view-projection transform, starting from world-space points translated by -ViewOrigin. */
	FMatrix		TranslatedViewProjectionMatrix;
	/** The inverse view-projection transform, ending with world-space points translated by -ViewOrigin. */
	FMatrix		InvTranslatedViewProjectionMatrix;
	/** During GetDynamicMeshElements this will be the correct cull volume for shadow stuff */
	const FConvexVolume* GetDynamicMeshElementsShadowCullFrustum;
	/** If the above is non-null, a translation that is applied to world-space before transforming by one of the shadow matrices. */
	FVector		PreShadowTranslation;
	/** The translation to apply to the world before TranslatedViewProjectionMatrix. Usually it is -ViewOrigin but with rereflections this can differ */
	FVector		PreViewTranslation;
	/** To support ortho and other modes this is redundant, in world space */
	FVector		ViewOrigin;
	/** Scale applied by the projection matrix in X and Y. */
	FVector2D	ProjectionScale;
	FVector2D	TemporalAASample;
	/**
	 * Scale factor to use when computing the size of a sphere in pixels.
	 * 
	 * A common calculation is to determine the size of a sphere in pixels when projected on the screen:
	 *		ScreenRadius = max(0.5 * ViewSizeX * ProjMatrix[0][0], 0.5 * ViewSizeY * ProjMatrix[1][1]) * SphereRadius / ProjectedSpherePosition.W
	 * Instead you can now simply use:
	 *		ScreenRadius = ScreenScale * SphereRadius / ProjectedSpherePosition.W
	 */
	float ScreenScale;

	//
	// World = TranslatedWorld - PreViewTranslation
	// TranslatedWorld = World + PreViewTranslation
	// 

	// ----------------

	/** @return true:perspective, false:orthographic */
	inline bool IsPerspectiveProjection() const
	{
		return ProjMatrix.M[3][3] < 1.0f;
	}

	FMatrix GetViewProjMatrix() const
	{
		return ViewMatrix * ProjMatrix;
	}

	FMatrix GetViewRotationProjMatrix() const
	{
		return ViewMatrix.RemoveTranslation() * ProjMatrix;
	}

	FMatrix GetInvProjMatrix() const
	{
		return ProjMatrix.Inverse();
	}

	FMatrix GetInvViewMatrix() const
	{
		// can be optimized: it's not a perspective matrix so transpose would be enough
		return ViewMatrix.Inverse();
	}

	FMatrix GetInvViewProjMatrix() const
	{
		return GetInvProjMatrix() * GetInvViewMatrix();
	}
};

#include "ShaderParameters.h"

/** 
 * Current limit of the dynamic branching forward lighting code path (see r.ForwardLighting)
 */
static const int32 GMaxNumForwardLights = 32;

/** 
 * Data used to pass light properties and some other parameters down to the dynamic forward lighting code path.
 * todo: Fix static size (to be more efficient and to not limit the feature)
 */
BEGIN_UNIFORM_BUFFER_STRUCT(FForwardLightData,ENGINE_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,LightCount)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,TileSize)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,TileCountX)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,InvTileSize)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,LightPositionAndInvRadius,[GMaxNumForwardLights])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,LightColorAndFalloffExponent,[GMaxNumForwardLights])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,LightDirectionAndSpotlightMaskAndMinRoughness,[GMaxNumForwardLights])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,SpotAnglesAndSourceRadiusAndDir,[GMaxNumForwardLights])
END_UNIFORM_BUFFER_STRUCT(FForwardLightData)

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////

static const int MAX_FORWARD_SHADOWCASCADES = 2;

/** 
 * Enumeration for currently used translucent lighting volume cascades 
 */
enum ETranslucencyVolumeCascade
{
	TVC_Inner,
	TVC_Outer,

	TVC_MAX,
};

/** The uniform shader parameters associated with a view. */
BEGIN_UNIFORM_BUFFER_STRUCT_WITH_CONSTRUCTOR(FViewUniformShaderParameters,ENGINE_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,TranslatedWorldToClip)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,WorldToClip)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,TranslatedWorldToView)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,ViewToTranslatedWorld)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,ViewToClip)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,ClipToView)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,ClipToTranslatedWorld)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,ScreenToWorld)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,ScreenToTranslatedWorld)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector,ViewForward, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector,ViewUp, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector,ViewRight, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,InvDeviceZToWorldZTransform)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4,ScreenPositionScaleBias, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4,ViewRectMin, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,ViewSizeAndSceneTexelSize)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,ViewOrigin)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,TranslatedViewOrigin)
	// The exposure scale is just a scalar but needs to be a float4 to workaround a driver bug on IOS.
	// After 4.2 we can put the workaround in the cross compiler.
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4,ExposureScale, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4,DiffuseOverrideParameter, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4,SpecularOverrideParameter, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4,NormalOverrideParameter, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector2D,RoughnessOverrideParameter, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, PrevFrameGameTime)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, PrevFrameRealTime)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,PreViewTranslation)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,OutOfBoundsMask, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,ViewOriginDelta)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,CullingSign)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,NearPlane, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,AdaptiveTessellationFactor)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,GameTime)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,RealTime)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,Random)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,FrameNumber)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,CameraCut, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,UseLightmaps, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,UnlitViewmodeMask, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FLinearColor,DirectionalLightColor, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector,DirectionalLightDirection, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float, DirectionalLightShadowTransition, EShaderPrecisionModifier::Half)			
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4, DirectionalLightShadowSize, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FMatrix, DirectionalLightScreenToShadow, [MAX_FORWARD_SHADOWCASCADES])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4, DirectionalLightShadowDistances, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FLinearColor,UpperSkyColor, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FLinearColor,LowerSkyColor, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,TranslucencyLightingVolumeMin,[TVC_MAX])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,TranslucencyLightingVolumeInvSize,[TVC_MAX])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,TemporalAAParams)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,DepthOfFieldFocalDistance)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,DepthOfFieldScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,DepthOfFieldFocalLength)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,DepthOfFieldFocalRegion)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,DepthOfFieldNearTransitionRegion)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,DepthOfFieldFarTransitionRegion)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,MotionBlurNormalizedToPixel)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,GeneralPurposeTweak)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,DemosaicVposOffset, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,PrevProjection)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,PrevViewProj)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,PrevViewRotationProj)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,PrevTranslatedWorldToClip)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,PrevViewOrigin)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,PrevPreViewTranslation)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,PrevInvViewProj)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FMatrix,PrevScreenToTranslatedWorld)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,IndirectLightingColorScale)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,HdrMosaic, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector,AtmosphericFogSunDirection)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogSunPower, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogPower, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogDensityScale, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogDensityOffset, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogGroundOffset, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogDistanceScale, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogAltitudeScale, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogHeightScaleRayleigh, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogStartDistance, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogDistanceOffset, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float,AtmosphericFogSunDiscScale, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,AtmosphericFogRenderMask)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32,AtmosphericFogInscatterAltitudeSampleNum)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FLinearColor,AtmosphericFogSunColor)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FLinearColor,AmbientCubemapTint)//Used via a custom material node. DO NOT REMOVE.
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,AmbientCubemapIntensity)//Used via a custom material node. DO NOT REMOVE.
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector2D,RenderTargetSize)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float,SkyLightParameters)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,SceneTextureMinMax)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FLinearColor,SkyLightColor)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,SkyIrradianceEnvironmentMap,[7])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(float, ES2PreviewMode)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, DirectionalLightShadowTexture)	
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, DirectionalLightShadowSampler)
END_UNIFORM_BUFFER_STRUCT(FViewUniformShaderParameters)

/**
 * A projection from scene space into a 2D screen region.
 */
class ENGINE_API FSceneView
{
public:
	const FSceneViewFamily* Family;
	/** can be 0 (thumbnail rendering) */
	FSceneViewStateInterface* State;

	/** The uniform buffer for the view's parameters.  This is only initialized in the rendering thread's copies of the FSceneView. */
	TUniformBufferRef<FViewUniformShaderParameters> UniformBuffer;

	/** uniform buffer with the lights for forward lighting/shading */
	TUniformBufferRef<FForwardLightData> ForwardLightData;

	/** The actor which is being viewed from. */
	const AActor* ViewActor;

	/** An interaction which draws the view's interaction elements. */
	FViewElementDrawer* Drawer;

	/* Final position of the view in the final render target (in pixels), potentially scaled by ScreenPercentage */
	FIntRect ViewRect;

	/* Final position of the view in the final render target (in pixels), potentially constrained by an aspect ratio requirement (black bars) */
	const FIntRect UnscaledViewRect;

	/* Raw view size (in pixels), used for screen space calculations */
	const FIntRect UnconstrainedViewRect;

	/** Maximum number of shadow cascades to render with. */
	int32 MaxShadowCascades;

	FViewMatrices ViewMatrices;

	/** Variables used to determine the view matrix */
	FVector		ViewLocation;
	FRotator	ViewRotation;
	FQuat		BaseHmdOrientation;
	FVector		BaseHmdLocation;
	float		WorldToMetersScale;

	// normally the same as ViewMatrices unless "r.Shadow.FreezeCamera" is activated
	FViewMatrices ShadowViewMatrices;

	FMatrix ProjectionMatrixUnadjustedForRHI;

	FLinearColor BackgroundColor;
	FLinearColor OverlayColor;

	/** Color scale multiplier used during post processing */
	FLinearColor ColorScale;

	/** For stereoscopic rendering, whether or not this is a full pass, or a left / right eye pass */
	EStereoscopicPass StereoPass;

	/** Whether this view should render the first instance only of any meshes using instancing. */
	bool bRenderFirstInstanceOnly;

	// Whether to use FOV when computing mesh LOD.
	bool bUseFieldOfViewForLOD;

	/** Current buffer visualization mode */
	FName CurrentBufferVisualizationMode;

	/**
	* These can be used to override material parameters across the scene without recompiling shaders.
	* The last component is how much to include of the material's value for that parameter, so 0 will completely remove the material's value.
	*/
	FVector4 DiffuseOverrideParameter;
	FVector4 SpecularOverrideParameter;
	FVector4 NormalOverrideParameter;
	FVector2D RoughnessOverrideParameter;

	/** The primitives which are hidden for this view. */
	TSet<FPrimitiveComponentId> HiddenPrimitives;

	// Derived members.

	/** redundant, ViewMatrices.GetViewProjMatrix() */
	/* UE4 projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix ViewProjectionMatrix;
	/** redundant, ViewMatrices.GetInvViewMatrix() */
	FMatrix InvViewMatrix;				
	/** redundant, ViewMatrices.GetInvViewProjMatrix() */
	FMatrix InvViewProjectionMatrix;	

	float TemporalJitterPixelsX;
	float TemporalJitterPixelsY;

	FConvexVolume ViewFrustum;

	bool bHasNearClippingPlane;

	FPlane NearClippingPlane;

	float NearClippingDistance;

	/** true if ViewMatrix.Determinant() is negative. */
	bool bReverseCulling;

	/* Vector used by shaders to convert depth buffer samples into z coordinates in world space */
	FVector4 InvDeviceZToWorldZTransform;

	/** FOV based multiplier for cull distance on objects */
	float LODDistanceFactor;
	/** Square of the FOV based multiplier for cull distance on objects */
	float LODDistanceFactorSquared;

	/** Whether we did a camera cut for this view this frame. */
	bool bCameraCut;
	
	/** Whether world origin was rebased this frame. */
	bool bOriginOffsetThisFrame;

	// -1,-1 if not setup
	FIntPoint CursorPos;

	/** True if this scene was created from a game world. */
	bool bIsGameView;

	/** Whether this view should override the view family's EngineShowFlags.Materials setting and force it on. */
	bool bForceShowMaterials;

	/** For sanity checking casts that are assumed to be safe. */
	bool bIsViewInfo;

	/** Whether this view is being used to render a scene capture. */
	bool bIsSceneCapture;

	/** Whether this view is being used to render a reflection capture. */
	bool bIsReflectionCapture;
	
	/** Whether this view was created from a locked viewpoint. */
	bool bIsLocked;

	/** 
	 * Whether to only render static lights and objects.  
	 * This is used when capturing the scene for reflection captures, which aren't updated at runtime. 
	 */
	bool bStaticSceneOnly;

	/** Aspect ratio constrained view rect. In the editor, when attached to a camera actor and the camera black bar showflag is enabled, the normal viewrect 
	  * remains as the full viewport, and the black bars are just simulated by drawing black bars. This member stores the effective constrained area within the
	  * bars.
	 **/
	FIntRect CameraConstrainedViewRect;

	/** Sort axis for when TranslucentSortPolicy is SortAlongAxis */
	FVector TranslucentSortAxis;

	/** Translucent sort mode */
	TEnumAsByte<ETranslucentSortPolicy::Type> TranslucentSortPolicy;
	
#if WITH_EDITOR
	/** The set of (the first 64) groups' visibility info for this view */
	uint64 EditorViewBitflag;

	/** For ortho views, this can control how to determine LOD parenting (ortho has no "distance-to-camera") */
	FVector OverrideLODViewOrigin;

	/** True if we should draw translucent objects when rendering hit proxies */
	bool bAllowTranslucentPrimitivesInHitProxy;

	/** BitArray representing the visibility state of the various sprite categories in the editor for this view */
	TBitArray<> SpriteCategoryVisibility;
	/** Selection color for the editor (used by post processing) */
	FLinearColor SelectionOutlineColor;
	/** Selection color for use in the editor with inactive primitives */
	FLinearColor SubduedSelectionOutlineColor;
	/** True if any components are selected in isolation (independent of actor selection) */
	bool bHasSelectedComponents;
#endif

	/**
	 * The final settings for the current viewer position (blended together from many volumes).
	 * Setup by the main thread, passed to the render thread and never touched again by the main thread.
	 */
	FFinalPostProcessSettings FinalPostProcessSettings;

	/** Parameters for atmospheric fog. */
	FTextureRHIRef AtmosphereTransmittanceTexture;
	FTextureRHIRef AtmosphereIrradianceTexture;
	FTextureRHIRef AtmosphereInscatterTexture;

	/** Feature level for this scene */
	ERHIFeatureLevel::Type FeatureLevel;

	/** Initialization constructor. */
	FSceneView(const FSceneViewInitOptions& InitOptions);

	/** used by ScreenPercentage */
	void SetScaledViewRect(FIntRect InScaledViewRect);

	/** Transforms a point from world-space to the view's screen-space. */
	FVector4 WorldToScreen(const FVector& WorldPoint) const;

	/** Transforms a point from the view's screen-space to world-space. */
	FVector ScreenToWorld(const FVector4& ScreenPoint) const;

	/** Transforms a point from the view's screen-space into pixel coordinates relative to the view's X,Y. */
	bool ScreenToPixel(const FVector4& ScreenPoint,FVector2D& OutPixelLocation) const;

	/** Transforms a point from pixel coordinates relative to the view's X,Y (left, top) into the view's screen-space. */
	FVector4 PixelToScreen(float X,float Y,float Z) const;

	/** Transforms a point from the view's world-space into pixel coordinates relative to the view's X,Y (left, top). */
	bool WorldToPixel(const FVector& WorldPoint,FVector2D& OutPixelLocation) const;

	/** Transforms a point from pixel coordinates relative to the view's X,Y (left, top) into the view's world-space. */
	FVector4 PixelToWorld(float X,float Y,float Z) const;

	/** 
	 * Transforms a point from the view's world-space into the view's screen-space. 
	 * Divides the resulting X, Y, Z by W before returning. 
	 */
	FPlane Project(const FVector& WorldPoint) const;

	/** 
	 * Transforms a point from the view's screen-space into world coordinates
	 * multiplies X, Y, Z by W before transforming. 
	 */
	FVector Deproject(const FPlane& ScreenPoint) const;

	/** transforms 2D screen coordinates into a 3D world-space origin and direction 
	 * @param ScreenPos - screen coordinates in pixels
	 * @param out_WorldOrigin (out) - world-space origin vector
	 * @param out_WorldDirection (out) - world-space direction vector
	 */
	void DeprojectFVector2D(const FVector2D& ScreenPos, FVector& out_WorldOrigin, FVector& out_WorldDirection) const;

	/** transforms 2D screen coordinates into a 3D world-space origin and direction 
	 * @param ScreenPos - screen coordinates in pixels
	 * @param ViewRect - view rectangle
	 * @param InvViewMatrix - inverse view matrix
	 * @param InvProjMatrix - inverse projection matrix
	 * @param out_WorldOrigin (out) - world-space origin vector
	 * @param out_WorldDirection (out) - world-space direction vector
	 */
	static void DeprojectScreenToWorld(const FVector2D& ScreenPos, const FIntRect& ViewRect, const FMatrix& InvViewMatrix, const FMatrix& InvProjMatrix, FVector& out_WorldOrigin, FVector& out_WorldDirection);

	inline FVector GetViewRight() const { return ViewMatrices.ViewMatrix.GetColumn(0); }
	inline FVector GetViewUp() const { return ViewMatrices.ViewMatrix.GetColumn(1); }
	inline FVector GetViewDirection() const { return ViewMatrices.ViewMatrix.GetColumn(2); }

	/** @return true:perspective, false:orthographic */
	inline bool IsPerspectiveProjection() const { return ViewMatrices.IsPerspectiveProjection(); }

	/** Returns the location used as the origin for LOD computations
	 * @param Index, 0 or 1, which LOD origin to return
	 * @return LOD origin
	 */
	FVector GetTemporalLODOrigin(int32 Index, bool bUseLaggedLODTransition = true) const;

	/** Get LOD distance factor: Sqrt(GetLODDistanceFactor()*SphereRadius*SphereRadius / ScreenPercentage) = distance to this LOD transition
	 * @return distance factor
	 */
	float GetLODDistanceFactor() const;

	/** Get LOD distance factor for temporal LOD: Sqrt(GetTemporalLODDistanceFactor(?)*SphereRadius*SphereRadius / ScreenPercentage) = distance to this LOD transition
	 * @param Index, 0 or 1, which temporal sample to return
	 * @return distance factor
	 */
	float GetTemporalLODDistanceFactor(int32 Index, bool bUseLaggedLODTransition = true) const;

	/** 
	 * Returns the blend factor between the last two LOD samples
	 */
	float GetTemporalLODTransition() const;

	/** 
	 * returns a unique key for the view state if one exists, otherwise returns zero
	 */
	uint32 GetViewKey() const;

	/** Allow things like HMD displays to update the view matrix at the last minute, to minimize perceived latency */
	void UpdateViewMatrix();

	/** Setup defaults and depending on view position (postprocess volumes) */
	void StartFinalPostprocessSettings(FVector ViewLocation);

	/**
	 * custom layers can be combined with the existing settings
	 * @param Weight usually 0..1 but outside range is clamped
	 */
	void OverridePostProcessSettings(const FPostProcessSettings& Src, float Weight);

	/** applied global restrictions from show flags */
	void EndFinalPostprocessSettings(const FSceneViewInitOptions& ViewInitOptions);

	/** Configure post process settings for the buffer visualization system */
	void ConfigureBufferVisualizationSettings();

	/** Get the feature level for this view **/
	ERHIFeatureLevel::Type GetFeatureLevel() const { return FeatureLevel; }

	/** Get the feature level for this view **/
	EShaderPlatform GetShaderPlatform() const;
};

//////////////////////////////////////////////////////////////////////////

/**
 * A set of views into a scene which only have different view transforms and owner actors.
 */
class FSceneViewFamily
{
public:
	/**
	* Helper struct for creating FSceneViewFamily instances
	* If created with specifying a time it will retrieve them from the world in the given scene.
	* 
	* @param InRenderTarget		The render target which the views are being rendered to.
	* @param InScene			The scene being viewed.
	* @param InShowFlags		The show flags for the views.
	*
	*/
	struct ConstructionValues
	{
		ConstructionValues(
			const FRenderTarget* InRenderTarget,
			FSceneInterface* InScene,
			const FEngineShowFlags& InEngineShowFlags
			)
		:	RenderTarget(InRenderTarget)
		,	Scene(InScene)
		,	EngineShowFlags(InEngineShowFlags)
		,	CurrentWorldTime(0.0f)
		,	DeltaWorldTime(0.0f)
		,	CurrentRealTime(0.0f)
		,	GammaCorrection(1.0f)
		,	bRealtimeUpdate(false)
		,	bDeferClear(false)
		,	bResolveScene(true)			
		,	bTimesSet(false)
		{
			if( InScene != NULL )			
			{
				UWorld* World = InScene->GetWorld();
				// Ensure the world is valid and that we are being called from a game thread (GetRealTimeSeconds requires this)
				if( World && IsInGameThread() )
				{					
					CurrentWorldTime = World->GetTimeSeconds();
					DeltaWorldTime = World->GetDeltaSeconds();
					CurrentRealTime = World->GetRealTimeSeconds();
					bTimesSet = true;
				}
			}
		}
		/** The views which make up the family. */
		const FRenderTarget* RenderTarget;

		/** The render target which the views are being rendered to. */
		FSceneInterface* Scene;

		/** The engine show flags for the views. */
		FEngineShowFlags EngineShowFlags;

		/** The current world time. */
		float CurrentWorldTime;

		/** The difference between the last world time and CurrentWorldTime. */
		float DeltaWorldTime;
		
		/** The current real time. */
		float CurrentRealTime;

		/** Gamma correction used when rendering this family. Default is 1.0 */
		float GammaCorrection;

		/** Indicates whether the view family is updated in real-time. */
		uint32 bRealtimeUpdate:1;
		
		/** Used to defer the back buffer clearing to just before the back buffer is drawn to */
		uint32 bDeferClear:1;
		
		/** If true then results of scene rendering are copied/resolved to the RenderTarget. */
		uint32 bResolveScene:1;		
		
		/** Safety check to ensure valid times are set either from a valid world/scene pointer or via the SetWorldTimes function */
		uint32 bTimesSet:1;

		/** Set the world time ,difference between the last world time and CurrentWorldTime and current real time. */
		ConstructionValues& SetWorldTimes(const float InCurrentWorldTime,const float InDeltaWorldTime,const float InCurrentRealTime) { CurrentWorldTime = InCurrentWorldTime; DeltaWorldTime = InDeltaWorldTime; CurrentRealTime = InCurrentRealTime;bTimesSet = true;return *this; }
		
		/** Set  whether the view family is updated in real-time. */
		ConstructionValues& SetRealtimeUpdate(const bool Value) { bRealtimeUpdate = Value; return *this; }
		
		/** Set whether to defer the back buffer clearing to just before the back buffer is drawn to */
		ConstructionValues& SetDeferClear(const bool Value) { bDeferClear = Value; return *this; }
		
		/** Setting to if true then results of scene rendering are copied/resolved to the RenderTarget. */
		ConstructionValues& SetResolveScene(const bool Value) { bResolveScene = Value; return *this; }
		
		/** Set Gamma correction used when rendering this family. */
		ConstructionValues& SetGammaCorrection(const float Value) { GammaCorrection = Value; return *this; }		
	};
	
	/** The views which make up the family. */
	TArray<const FSceneView*> Views;

	/** The width in screen pixels of the view family being rendered (maximum x of all viewports). */
	uint32 FamilySizeX;

	/** The height in screen pixels of the view family being rendered (maximum y of all viewports). */
	uint32 FamilySizeY;

	/** The render target which the views are being rendered to. */
	const FRenderTarget* RenderTarget;

	/** Indicates that a separate render target is in use (not a backbuffer RT) */
	bool bUseSeparateRenderTarget;

	/** The scene being viewed. */
	FSceneInterface* Scene;

	/** The new show flags for the views (meant to replace the old system). */
	FEngineShowFlags EngineShowFlags;

	/** The current world time. */
	float CurrentWorldTime;

	/** The difference between the last world time and CurrentWorldTime. */
	float DeltaWorldTime;

	/** The current real time. */
	float CurrentRealTime;

	/** Copy from main thread GFrameNumber to be accessible on render thread side. UINT_MAX before CreateSceneRenderer() or BeginRenderingViewFamily() was called */
	uint32 FrameNumber;

	/** Indicates whether the view family is updated in realtime. */
	bool bRealtimeUpdate;

	/** Used to defer the back buffer clearing to just before the back buffer is drawn to */
	bool bDeferClear;

	/** if true then results of scene rendering are copied/resolved to the RenderTarget. */
	bool bResolveScene;

	/** from GetWorld->IsPaused() */
	bool bWorldIsPaused;

	/** Gamma correction used when rendering this family. Default is 1.0 */
	float GammaCorrection;
	
	/** Editor setting to allow designers to override the automatic expose. 0:Automatic, following indices: -4 .. +4 */
	FExposureSettings ExposureSettings;

    /** Extensions that can modify view parameters on the render thread. */
    TArray<TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> > ViewExtensions;

#if WITH_EDITOR
	// Override the LOD of landscape in this viewport
	int8 LandscapeLODOverride;

	// Override the LOD of landscape in this viewport
	int8 HierarchicalLODOverride;

	/** Indicates whether, of not, the base attachment volume should be drawn. */
	bool bDrawBaseInfo;
#endif

	/** Initialization constructor. */
	ENGINE_API FSceneViewFamily( const ConstructionValues& CVS );

	/** Computes FamilySizeX and FamilySizeY from the Views array. */
	ENGINE_API void ComputeFamilySize();

	ENGINE_API ERHIFeatureLevel::Type GetFeatureLevel() const;

	EShaderPlatform GetShaderPlatform() const { return GShaderPlatformForFeatureLevel[GetFeatureLevel()]; }
};

/**
 * A view family which deletes its views when it goes out of scope.
 */
class FSceneViewFamilyContext : public FSceneViewFamily
{
public:
	/** Initialization constructor. */
	FSceneViewFamilyContext( const ConstructionValues& CVS)
		:	FSceneViewFamily(CVS)
	{}

	/** Destructor. */
	ENGINE_API ~FSceneViewFamilyContext();
};
