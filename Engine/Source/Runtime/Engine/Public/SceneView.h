// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
#include "RHIStaticStates.h"
#include "GlobalDistanceFieldParameters.h"
#include "DebugViewModeHelpers.h"
#include "ShaderParameters.h"

class FSceneViewStateInterface;
class FViewUniformShaderParameters;
class FInstancedViewUniformShaderParameters;
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

	bool IsPerspectiveProjection() const
	{
		return ProjectionMatrix.M[3][3] < 1.0f;
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

	/** The primitives which are visible for this view. If the array is not empty, all other primitives will be hidden. */
	TSet<FPrimitiveComponentId> ShowOnlyPrimitives;

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
		TranslatedViewMatrix.SetIdentity();
		TranslatedViewProjectionMatrix.SetIdentity();
		InvTranslatedViewProjectionMatrix.SetIdentity();
		GetDynamicMeshElementsShadowCullFrustum = nullptr;
		PreShadowTranslation = FVector::ZeroVector;
		PreViewTranslation = FVector::ZeroVector;
		ViewOrigin = FVector::ZeroVector;
		ProjectionScale = FVector2D::ZeroVector;
		TemporalAAProjJitter = FVector2D::ZeroVector;
		ScreenScale = 1.f;
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
	/** TemporalAA jitter offset currently stored in the projection matrix */
	FVector2D	TemporalAAProjJitter;

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

	FMatrix GetProjNoAAMatrix() const
	{
		FMatrix ProjNoAAMatrix = ProjMatrix;

		ProjNoAAMatrix.M[2][0] -= TemporalAAProjJitter.X;
		ProjNoAAMatrix.M[2][1] -= TemporalAAProjJitter.Y;

		return ProjNoAAMatrix;
	}

	void RemoveTemporalJitter()
	{
		ProjMatrix = GetProjNoAAMatrix();
		TemporalAAProjJitter = FVector2D::ZeroVector;
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
		return InvertProjMatrix( ProjMatrix );
	}
	
	FMatrix GetInvProjNoAAMatrix() const
	{
		return InvertProjMatrix( GetProjNoAAMatrix() );
	}

	FMatrix GetInvViewMatrix() const
	{
		return FTranslationMatrix( -ViewMatrix.GetOrigin() ) * ViewMatrix.RemoveTranslation().GetTransposed();
	}

	FMatrix GetInvViewProjMatrix() const
	{
		return GetInvProjMatrix() * GetInvViewMatrix();
	}

	// @return in radians (horizontal,vertical)
	FVector2D GetHalfFieldOfViewPerAxis() const
	{
		const FMatrix ClipToView = GetInvProjNoAAMatrix();

		FVector VCenter = FVector(ClipToView.TransformPosition(FVector(0.0, 0.0, 0.0)));
		FVector VUp = FVector(ClipToView.TransformPosition(FVector(0.0, 1.0, 0.0)));
		FVector VRight = FVector(ClipToView.TransformPosition(FVector(1.0, 0.0, 0.0)));

		VCenter.Normalize();
		VUp.Normalize();
		VRight.Normalize();

		return FVector2D(FMath::Acos(VCenter | VRight), FMath::Acos(VCenter | VUp));
	}

private:
	static FMatrix InvertProjMatrix( const FMatrix& M )
	{
		if( M.M[1][0] == 0.0f &&
			M.M[3][0] == 0.0f &&
			M.M[0][1] == 0.0f &&
			M.M[3][1] == 0.0f &&
			M.M[0][2] == 0.0f &&
			M.M[1][2] == 0.0f &&
			M.M[0][3] == 0.0f &&
			M.M[1][3] == 0.0f &&
			M.M[2][3] == 1.0f &&
			M.M[3][3] == 0.0f )
		{
			// Solve the common case directly with very high precision.
			/*
			M = 
			| a | 0 | 0 | 0 |
			| 0 | b | 0 | 0 |
			| s | t | c | 1 |
			| 0 | 0 | d | 0 |
			*/

			double a = M.M[0][0];
			double b = M.M[1][1];
			double c = M.M[2][2];
			double d = M.M[3][2];
			double s = M.M[2][0];
			double t = M.M[2][1];

			return FMatrix(
				FPlane( 1.0 / a, 0.0f, 0.0f, 0.0f ),
				FPlane( 0.0f, 1.0 / b, 0.0f, 0.0f ),
				FPlane( 0.0f, 0.0f, 0.0f, 1.0 / d ),
				FPlane( -s/a, -t/b, 1.0f, -c/d )
			);
		}
		else
		{
			return M.Inverse();
		}
	}
};

//////////////////////////////////////////////////////////////////////////

static const int MAX_MOBILE_SHADOWCASCADES = 2;

/** The uniform shader parameters for a mobile directional light and its shadow.
  * One uniform buffer will be created for the first directional light in each lighting channel.
  */
BEGIN_UNIFORM_BUFFER_STRUCT_WITH_CONSTRUCTOR(FMobileDirectionalLightShaderParameters, ENGINE_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FLinearColor, DirectionalLightColor, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector, DirectionalLightDirection, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(float, DirectionalLightShadowTransition, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4, DirectionalLightShadowSize, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FMatrix, DirectionalLightScreenToShadow, [MAX_MOBILE_SHADOWCASCADES])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(FVector4, DirectionalLightShadowDistances, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, DirectionalLightShadowTexture)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, DirectionalLightShadowSampler)
END_UNIFORM_BUFFER_STRUCT(FMobileDirectionalLightShaderParameters)

//////////////////////////////////////////////////////////////////////////

/** 
 * Enumeration for currently used translucent lighting volume cascades 
 */
enum ETranslucencyVolumeCascade
{
	TVC_Inner,
	TVC_Outer,

	TVC_MAX,
};

// View uniform buffer member declarations
#define VIEW_UNIFORM_BUFFER_MEMBER_TABLE \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, TranslatedWorldToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, WorldToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, TranslatedWorldToView) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, ViewToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, TranslatedWorldToCameraView) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, CameraViewToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, ViewToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, ClipToView) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, ClipToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, SVPositionToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, ScreenToWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, ScreenToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector, ViewForward, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector, ViewUp, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector, ViewRight, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4, InvDeviceZToWorldZTransform) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4, ScreenPositionScaleBias, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, WorldCameraOrigin) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, TranslatedWorldCameraOrigin) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, WorldViewOrigin) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, PreViewTranslation) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevProjection) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevViewProj) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevViewRotationProj) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevViewToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevClipToView) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevTranslatedWorldToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevTranslatedWorldToView) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevViewToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevTranslatedWorldToCameraView) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevCameraViewToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, PrevWorldCameraOrigin) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, PrevWorldViewOrigin) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, PrevPreViewTranslation) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevInvViewProj) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, PrevScreenToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix, ClipToPrevClip) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4, GlobalClippingPlane) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2D, FieldOfViewWideAngles) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2D, PrevFieldOfViewWideAngles) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4, ViewRectMin, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4, ViewSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4, BufferSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4, ExposureScale, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4, DiffuseOverrideParameter, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4, SpecularOverrideParameter, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4, NormalOverrideParameter, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector2D, RoughnessOverrideParameter, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, PrevFrameGameTime) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, PrevFrameRealTime) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, OutOfBoundsMask, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, WorldCameraMovementSinceLastFrame) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, CullingSign) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, NearPlane, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, AdaptiveTessellationFactor) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GameTime) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, RealTime) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, Random) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, FrameNumber) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, StateFrameIndexMod8) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, CameraCut, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, UnlitViewmodeMask, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FLinearColor, DirectionalLightColor, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector, DirectionalLightDirection, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4, TranslucencyLightingVolumeMin, [TVC_MAX]) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4, TranslucencyLightingVolumeInvSize, [TVC_MAX]) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4, TemporalAAParams) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4, CircleDOFParams) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldSensorWidth) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldFocalDistance) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldFocalLength) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldFocalRegion) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldNearTransitionRegion) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldFarTransitionRegion) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MotionBlurNormalizedToPixel) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, bSubsurfacePostprocessEnabled) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GeneralPurposeTweak) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, DemosaicVposOffset, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, IndirectLightingColorScale) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, HDR32bppEncodingMode, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector, AtmosphericFogSunDirection) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogSunPower, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogPower, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogDensityScale, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogDensityOffset, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogGroundOffset, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogDistanceScale, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogAltitudeScale, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogHeightScaleRayleigh, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogStartDistance, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogDistanceOffset, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, AtmosphericFogSunDiscScale, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, AtmosphericFogRenderMask) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, AtmosphericFogInscatterAltitudeSampleNum) \
	VIEW_UNIFORM_BUFFER_MEMBER(FLinearColor, AtmosphericFogSunColor) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2D, NormalCurvatureToRoughnessScaleBias) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, RenderingReflectionCaptureMask) \
	VIEW_UNIFORM_BUFFER_MEMBER(FLinearColor, AmbientCubemapTint) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, AmbientCubemapIntensity) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyLightParameters) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4, SceneTextureMinMax) \
	VIEW_UNIFORM_BUFFER_MEMBER(FLinearColor, SkyLightColor) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4, SkyIrradianceEnvironmentMap, [7]) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MobilePreviewMode) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, HMDEyePaddingOffset) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, ReflectionCubemapMaxMip, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, ShowDecalsMask) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, DistanceFieldAOSpecularOcclusionMode) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2D, ReflectionEnvironmentRoughnessMixingScaleBias) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, StereoPassIndex) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4, GlobalVolumeCenterAndExtent_UB, [GMaxGlobalDistanceFieldClipmaps]) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4, GlobalVolumeWorldToUVAddAndMul_UB, [GMaxGlobalDistanceFieldClipmaps]) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GlobalVolumeDimension_UB) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GlobalVolumeTexelSize_UB) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MaxGlobalDistance_UB) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, bCheckerboardSubsurfaceProfileRendering)

#define VIEW_UNIFORM_BUFFER_MEMBER(type, identifier) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(type, identifier)

#define VIEW_UNIFORM_BUFFER_MEMBER_EX(type, identifier, precision) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX(type, identifier, precision)

#define VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(type, identifier, dimension) \
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(type, identifier, dimension)

/** The uniform shader parameters associated with a view. */
BEGIN_UNIFORM_BUFFER_STRUCT_WITH_CONSTRUCTOR(FViewUniformShaderParameters, ENGINE_API)

	VIEW_UNIFORM_BUFFER_MEMBER_TABLE

	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture3D, GlobalDistanceFieldTexture0_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, GlobalDistanceFieldSampler0_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture3D, GlobalDistanceFieldTexture1_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, GlobalDistanceFieldSampler1_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture3D, GlobalDistanceFieldTexture2_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, GlobalDistanceFieldSampler2_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture3D, GlobalDistanceFieldTexture3_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, GlobalDistanceFieldSampler3_UB)

	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, AtmosphereTransmittanceTexture_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, AtmosphereTransmittanceTextureSampler_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, AtmosphereIrradianceTexture_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, AtmosphereIrradianceTextureSampler_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture3D, AtmosphereInscatterTexture_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, AtmosphereInscatterTextureSampler_UB)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, PerlinNoiseGradientTexture)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, PerlinNoiseGradientTextureSampler)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture3D, PerlinNoise3DTexture)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, PerlinNoise3DTextureSampler)

END_UNIFORM_BUFFER_STRUCT(FViewUniformShaderParameters)

/** Copy of the view uniform shader parameters associated with a view for instanced stereo. */
BEGIN_UNIFORM_BUFFER_STRUCT_WITH_CONSTRUCTOR(FInstancedViewUniformShaderParameters, ENGINE_API)
	VIEW_UNIFORM_BUFFER_MEMBER_TABLE
END_UNIFORM_BUFFER_STRUCT(FInstancedViewUniformShaderParameters)

#undef VIEW_UNIFORM_BUFFER_MEMBER_TABLE
#undef VIEW_UNIFORM_BUFFER_MEMBER
#undef VIEW_UNIFORM_BUFFER_MEMBER_EX
#undef VIEW_UNIFORM_BUFFER_MEMBER_ARRAY

BEGIN_UNIFORM_BUFFER_STRUCT(FBuiltinSamplersParameters, ENGINE_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, Bilinear)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, BilinearClamped)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, Point)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, PointClamped)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, Trilinear)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, TrilinearClamped)
END_UNIFORM_BUFFER_STRUCT(FBuiltinSamplersParameters)

class ENGINE_API FBuiltinSamplersUniformBuffer : public TUniformBuffer<FBuiltinSamplersParameters>
{
public:
	FBuiltinSamplersUniformBuffer();

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;
};

#define USE_GBuiltinSamplersUniformBuffer (0)
extern ENGINE_API TGlobalResource<FBuiltinSamplersUniformBuffer> GBuiltinSamplersUniformBuffer;	

namespace EDrawDynamicFlags
{
	enum Type
	{
		None = 0,
		ForceLowestLOD = 0x1
	};
}

/**
 * A projection from scene space into a 2D screen region.
 */
class ENGINE_API FSceneView
{
public:
	const FSceneViewFamily* Family;
	/** can be 0 (thumbnail rendering) */
	FSceneViewStateInterface* State;

	/** The uniform buffer for the view's parameters. This is only initialized in the rendering thread's copies of the FSceneView. */
	TUniformBufferRef<FViewUniformShaderParameters> ViewUniformBuffer;
	TUniformBufferRef<FViewUniformShaderParameters> DownsampledTranslucencyViewUniformBuffer;

	/** Mobile Directional Lighting uniform buffers, one for each lighting channel 
	  * The first is used for primitives with no lighting channels set.
	  * Only initialized in the rendering thread's copies of the FSceneView.
	  */
	TUniformBufferRef<FMobileDirectionalLightShaderParameters> MobileDirectionalLightUniformBuffers[NUM_LIGHTING_CHANNELS+1];

	/** The actor which is being viewed from. */
	const AActor* ViewActor;

	/** An interaction which draws the view's interaction elements. */
	FViewElementDrawer* Drawer;

	/* Final position of the view in the final render target (in pixels), potentially scaled by ScreenPercentage */
	FIntRect ViewRect;

	/* Final position of the view in the final render target (in pixels), potentially constrained by an aspect ratio requirement (black bars) */
	const FIntRect UnscaledViewRect;

	/* Raw view size (in pixels), used for screen space calculations */
	FIntRect UnconstrainedViewRect;

	/* If set, derive the family view size explicitly using this. */
	FIntRect ResolutionOverrideRect;

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

	EDrawDynamicFlags::Type DrawDynamicFlags;

	/** Current buffer visualization mode */
	FName CurrentBufferVisualizationMode;

#if WITH_EDITOR
	/* Whether to use the pixel inspector */
	bool bUsePixelInspector;
#endif //WITH_EDITOR

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

	/** The primitives which are visible for this view. If the array is not empty, all other primitives will be hidden. */
	TSet<FPrimitiveComponentId> ShowOnlyPrimitives;

	// Derived members.

	/** redundant, ViewMatrices.GetViewProjMatrix() */
	/* UE4 projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix ViewProjectionMatrix;
	/** redundant, ViewMatrices.GetInvViewMatrix() */
	FMatrix InvViewMatrix;				
	/** redundant, ViewMatrices.GetInvViewProjMatrix() */
	FMatrix InvViewProjectionMatrix;	

	bool bAllowTemporalJitter;

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

	/** For sanity checking casts that are assumed to be safe. */
	bool bIsViewInfo;

	/** Whether this view is being used to render a scene capture. */
	bool bIsSceneCapture;

	/** Whether this view is being used to render a reflection capture. */
	bool bIsReflectionCapture;

	/** Whether this view is being used to render a planar reflection. */
	bool bIsPlanarReflection;

	/** Whether to force two sided rendering for this view. */
	bool bRenderSceneTwoSided;

	/** Whether this view was created from a locked viewpoint. */
	bool bIsLocked;

	/** 
	 * Whether to only render static lights and objects.  
	 * This is used when capturing the scene for reflection captures, which aren't updated at runtime. 
	 */
	bool bStaticSceneOnly;

	/** True if instanced stereo is enabled. */
	bool bIsInstancedStereoEnabled;

	/** True if multi-view is enabled. */
	bool bIsMultiViewEnabled;

	/** Global clipping plane being applied to the scene, or all 0's if disabled.  This is used when rendering the planar reflection pass. */
	FPlane GlobalClippingPlane;

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
	EAntiAliasingMethod AntiAliasingMethod;

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

	/** 
	 * Transforms 2D screen coordinates into a 3D world-space origin and direction 
	 * @param ScreenPos - screen coordinates in pixels
	 * @param out_WorldOrigin (out) - world-space origin vector
	 * @param out_WorldDirection (out) - world-space direction vector
	 */
	void DeprojectFVector2D(const FVector2D& ScreenPos, FVector& out_WorldOrigin, FVector& out_WorldDirection) const;

	/** 
	 * Transforms 2D screen coordinates into a 3D world-space origin and direction 
	 * @param ScreenPos - screen coordinates in pixels
	 * @param ViewRect - view rectangle
	 * @param InvViewMatrix - inverse view matrix
	 * @param InvProjMatrix - inverse projection matrix
	 * @param out_WorldOrigin (out) - world-space origin vector
	 * @param out_WorldDirection (out) - world-space direction vector
	 */
	static void DeprojectScreenToWorld(const FVector2D& ScreenPos, const FIntRect& ViewRect, const FMatrix& InvViewMatrix, const FMatrix& InvProjMatrix, FVector& out_WorldOrigin, FVector& out_WorldDirection);

	/** Overload to take a single combined view projection matrix. */
	static void DeprojectScreenToWorld(const FVector2D& ScreenPos, const FIntRect& ViewRect, const FMatrix& InvViewProjMatrix, FVector& out_WorldOrigin, FVector& out_WorldDirection);

	/** 
	 * Transforms 3D world-space origin into 2D screen coordinates
	 * @param WorldPosition - the 3d world point to transform
	 * @param ViewRect - view rectangle
	 * @param ViewProjectionMatrix - combined view projection matrix
	 * @param out_ScreenPos (out) - screen coordinates in pixels
	 */
	static bool ProjectWorldToScreen(const FVector& WorldPosition, const FIntRect& ViewRect, const FMatrix& ViewProjectionMatrix, FVector2D& out_ScreenPos);

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

	/** 
	 * returns a the occlusion frame counter or MAX_uint32 if there is no view state
	 */
	uint32 GetOcclusionFrameCounter() const;

	/** Allow things like HMD displays to update the view matrix at the last minute, to minimize perceived latency */
	void UpdateViewMatrix();

	/** Setup defaults and depending on view position (postprocess volumes) */
	void StartFinalPostprocessSettings(FVector InViewLocation);

	/**
	 * custom layers can be combined with the existing settings
	 * @param Weight usually 0..1 but outside range is clamped
	 */
	void OverridePostProcessSettings(const FPostProcessSettings& Src, float Weight);

	/** applied global restrictions from show flags */
	void EndFinalPostprocessSettings(const FSceneViewInitOptions& ViewInitOptions);

	void SetupAntiAliasingMethod();

	/** Configure post process settings for the buffer visualization system */
	void ConfigureBufferVisualizationSettings();

	/** Get the feature level for this view (cached from the scene so this is not different per view) **/
	ERHIFeatureLevel::Type GetFeatureLevel() const { return FeatureLevel; }

	/** Get the feature level for this view **/
	EShaderPlatform GetShaderPlatform() const;

	/** True if the view should render as an instanced stereo pass */
	bool IsInstancedStereoPass() const { return bIsInstancedStereoEnabled && StereoPass == eSSP_LEFT_EYE; }
};

//////////////////////////////////////////////////////////////////////////

// for r.DisplayInternals (allows for easy passing down data from main to render thread)
struct FDisplayInternalsData
{
	//
	int32 DisplayInternalsCVarValue;
	// current time Matinee location (in seconds) of the single playing playing actor, -1 if none is playing, -2 if multiple are playing
	float MatineeTime;
	// -1 if not set, from IStreamingManager::Get().StreamAllResources(Duration) in FStreamAllResourcesLatentCommand
	uint32 NumPendingStreamingRequests;

	FDisplayInternalsData()
		: DisplayInternalsCVarValue(0)
		, MatineeTime(-1.0f)
		, NumPendingStreamingRequests(-1)
	{
		check(!IsValid());
	}

	// called on main thread
	// @param World may be 0
	void Setup(UWorld *World);

	bool IsValid() const { return DisplayInternalsCVarValue != 0; }
};

//////////////////////////////////////////////////////////////////////////

/**
 * A set of views into a scene which only have different view transforms and owner actors.
 */
class ENGINE_API FSceneViewFamily
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

	/** 
		The width in pixels of the stereo view family being rendered. This may be different than FamilySizeX if
		we're using adaptive resolution stereo rendering. In that case, FamilySizeX represents the maximum size of 
		the family to ensure the backing render targets don't change between frames as the view size varies.
	*/
	uint32 InstancedStereoWidth;

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

	/** 
	 * Which component of the scene rendering should be output to the final render target.
	 * If SCS_FinalColorLDR this indicates do nothing.
	 */
	ESceneCaptureSource SceneCaptureSource;
	

	/** When enabled, the scene capture will composite into the render target instead of overwriting its contents. */
	ESceneCaptureCompositeMode SceneCaptureCompositeMode;

	/**
	 * GetWorld->IsPaused() && !Simulate
	 * Simulate is excluded as the camera can move which invalidates motionblur
	 */
	bool bWorldIsPaused;

	/** Gamma correction used when rendering this family. Default is 1.0 */
	float GammaCorrection;
	
	/** Editor setting to allow designers to override the automatic expose. 0:Automatic, following indices: -4 .. +4 */
	FExposureSettings ExposureSettings;

    /** Extensions that can modify view parameters on the render thread. */
    TArray<TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> > ViewExtensions;

	// for r.DisplayInternals (allows for easy passing down data from main to render thread)
	FDisplayInternalsData DisplayInternalsData;

#if WITH_EDITOR
	// Override the LOD of landscape in this viewport
	int8 LandscapeLODOverride;

	/** Indicates whether, or not, the base attachment volume should be drawn. */
	bool bDrawBaseInfo;

	/**
	 * Indicates whether the shader world space position should be forced to 0. Also sets the view vector to (0,0,1) for all pixels.
	 * This is used in the texture streaming build when computing material tex coords scale.
	 * Because the material are rendered in tiles, there is no actual valid mapping for world space position.
	 * World space mapping would require to render mesh with the level transforms to be valid.
	 */
	bool bNullifyWorldSpacePosition;
#endif

	/** Initialization constructor. */
	FSceneViewFamily( const ConstructionValues& CVS );

	/** Computes FamilySizeX and FamilySizeY from the Views array. */
	void ComputeFamilySize();

	ERHIFeatureLevel::Type GetFeatureLevel() const;

	EShaderPlatform GetShaderPlatform() const { return GShaderPlatformForFeatureLevel[GetFeatureLevel()]; }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	EDebugViewShaderMode DebugViewShaderMode;
	bool bUsedDebugViewPSVSHS;
	FORCEINLINE EDebugViewShaderMode GetDebugViewShaderMode() const { return DebugViewShaderMode; }
	EDebugViewShaderMode ChooseDebugViewShaderMode() const;
	FORCEINLINE bool UseDebugViewVSDSHS() const { return bUsedDebugViewPSVSHS; }
	FORCEINLINE bool UseDebugViewPS() const { return DebugViewShaderMode != DVSM_None; }
#else
	FORCEINLINE EDebugViewShaderMode GetDebugViewShaderMode() const { return DVSM_None; }
	FORCEINLINE bool UseDebugViewVSDSHS() const { return false; }
	FORCEINLINE bool UseDebugViewPS() const { return false; }
#endif

	/** Returns the appropriate view for a given eye in a stereo pair. */
	const FSceneView& GetStereoEyeView(const EStereoscopicPass Eye) const;
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
