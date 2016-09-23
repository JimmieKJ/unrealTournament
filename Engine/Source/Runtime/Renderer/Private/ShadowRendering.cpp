// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShadowRendering.cpp: Shadow rendering implementation
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "TextureLayout.h"
#include "LightPropagationVolume.h"
#include "SceneUtils.h"
#include "SceneFilterRendering.h"
#include "ScreenRendering.h"

static TAutoConsoleVariable<float> CVarCSMShadowDepthBias(
	TEXT("r.Shadow.CSMDepthBias"),
	20.0f,
	TEXT("Constant depth bias used by CSM"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarPerObjectDirectionalShadowDepthBias(
	TEXT("r.Shadow.PerObjectDirectionalDepthBias"),
	4.0f,
	TEXT("Constant depth bias used by per-object shadows from directional lights"),
	ECVF_RenderThreadSafe);


static TAutoConsoleVariable<float> CVarCSMSplitPenumbraScale(
	TEXT("r.Shadow.CSMSplitPenumbraScale"),
	0.5f,
	TEXT("Scale applied to the penumbra size of Cascaded Shadow Map splits, useful for minimizing the transition between splits"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarCSMDepthBoundsTest(
	TEXT("r.Shadow.CSMDepthBoundsTest"),
	1,
	TEXT("Whether to use depth bounds tests rather than stencil tests for the CSM bounds"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSpotLightShadowTransitionScale(
	TEXT("r.Shadow.SpotLightTransitionScale"),
	60.0f,
	TEXT("Transition scale for spotlights"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarShadowTransitionScale(
	TEXT("r.Shadow.TransitionScale"),
	60.0f,
	TEXT("This controls the 'fade in' region between a caster and where his shadow shows up.  Larger values make a smaller region which will have more self shadowing artifacts"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarPointLightShadowDepthBias(
	TEXT("r.Shadow.PointLightDepthBias"),
	0.05f,
	TEXT("Depth bias that is applied in the depth pass for shadows from point lights. (0.03 avoids peter paning but has some shadow acne)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSpotLightShadowDepthBias(
	TEXT("r.Shadow.SpotLightDepthBias"),
	5.0f,
	TEXT("Depth bias that is applied in the depth pass for per object projected shadows from spot lights"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarEnableModulatedSelfShadow(
	TEXT("r.Shadow.EnableModulatedSelfShadow"),
	0,
	TEXT("Allows modulated shadows to affect the shadow caster. (mobile only)"),
	ECVF_RenderThreadSafe);

DECLARE_FLOAT_COUNTER_STAT(TEXT("ShadowProjection"), Stat_GPU_ShadowProjection, STATGROUP_GPU);

// 0:off, 1:low, 2:med, 3:high, 4:very high, 5:max
uint32 GetShadowQuality()
{
	static const auto ICVarQuality = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShadowQuality"));

	int Ret = ICVarQuality->GetValueOnRenderThread();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const auto ICVarLimit = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.LimitRenderingFeatures"));
	if(ICVarLimit)
	{
		int32 Limit = ICVarLimit->GetValueOnRenderThread();

		if(Limit > 2)
		{
			Ret = 0;
		}
	}
#endif

	return FMath::Clamp(Ret, 0, 5);
}

static TAutoConsoleVariable<int32> CVarSupportPointLightWholeSceneShadows(
	TEXT("r.SupportPointLightWholeSceneShadows"),
	1,
	TEXT("Enables shadowcasting point lights."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

float GetLightFadeFactor(const FSceneView& View, const FLightSceneProxy* Proxy)
{
	// Distance fade
	FSphere Bounds = Proxy->GetBoundingSphere();

	const float DistanceSquared = (Bounds.Center - View.ViewMatrices.ViewOrigin).SizeSquared();
	extern float GMinScreenRadiusForLights;
	float SizeFade = FMath::Square(FMath::Min(0.0002f, GMinScreenRadiusForLights / Bounds.W) * View.LODDistanceFactor) * DistanceSquared;
	SizeFade = FMath::Clamp(6.0f - 6.0f * SizeFade, 0.0f, 1.0f);

	float MaxDist = Proxy->GetMaxDrawDistance();
	float Range = Proxy->GetFadeRange();
	float DistanceFade = MaxDist ? (MaxDist - FMath::Sqrt(DistanceSquared)) / Range : 1.0f;
	DistanceFade = FMath::Clamp(DistanceFade, 0.0f, 1.0f);
	return SizeFade * DistanceFade;
}

/** The stencil sphere vertex buffer. */
TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<18, 12, FVector4> > StencilingGeometry::GStencilSphereVertexBuffer;
TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<18, 12, FVector> > StencilingGeometry::GStencilSphereVectorBuffer;

/** The stencil sphere index buffer. */
TGlobalResource<StencilingGeometry::TStencilSphereIndexBuffer<18, 12> > StencilingGeometry::GStencilSphereIndexBuffer;

TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<4, 4, FVector4> > StencilingGeometry::GLowPolyStencilSphereVertexBuffer;
TGlobalResource<StencilingGeometry::TStencilSphereIndexBuffer<4, 4> > StencilingGeometry::GLowPolyStencilSphereIndexBuffer;

/** The (dummy) stencil cone vertex buffer. */
TGlobalResource<StencilingGeometry::FStencilConeVertexBuffer> StencilingGeometry::GStencilConeVertexBuffer;

/** The stencil cone index buffer. */
TGlobalResource<StencilingGeometry::FStencilConeIndexBuffer> StencilingGeometry::GStencilConeIndexBuffer;

/*-----------------------------------------------------------------------------
	FShadowProjectionVS
-----------------------------------------------------------------------------*/

bool FShadowProjectionVS::ShouldCache(EShaderPlatform Platform)
{
	return true;
}

void FShadowProjectionVS::SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo)
{
	FGlobalShader::SetParameters(RHICmdList, GetVertexShader(),View);
	
	if(ShadowInfo->IsWholeSceneDirectionalShadow())
	{
		// Calculate bounding geometry transform for whole scene directional shadow.
		// Use a pair of pre-transformed planes for stenciling.
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4(0,0,0,1));
	}
	else if(ShadowInfo->IsWholeScenePointLightShadow())
	{
		// Handle stenciling sphere for point light.
		StencilingGeometryParameters.Set(RHICmdList, this, View, ShadowInfo->LightSceneInfo);
	}
	else
	{
		// Other bounding geometry types are pre-transformed.
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4(0,0,0,1));
	}
}

IMPLEMENT_SHADER_TYPE(,FShadowProjectionNoTransformVS,TEXT("ShadowProjectionVertexShader"),TEXT("Main"),SF_Vertex);

IMPLEMENT_SHADER_TYPE(,FShadowProjectionVS,TEXT("ShadowProjectionVertexShader"),TEXT("Main"),SF_Vertex);

/**
 * Implementations for TShadowProjectionPS.  
 */
#if !UE_BUILD_DOCS
#define IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(Quality,UseFadePlane) \
	typedef TShadowProjectionPS<Quality, UseFadePlane> FShadowProjectionPS##Quality##UseFadePlane; \
	IMPLEMENT_SHADER_TYPE(template<>,FShadowProjectionPS##Quality##UseFadePlane,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);

// Projection shaders without the distance fade, with different quality levels.
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(1,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(2,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(3,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(4,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5,false);

// Projection shaders with the distance fade, with different quality levels.
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(1,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(2,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(3,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(4,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5,true);
#endif

// Implement a pixel shader for rendering modulated shadow projections.
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<1>, TEXT("ShadowProjectionPixelShader"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<2>, TEXT("ShadowProjectionPixelShader"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<3>, TEXT("ShadowProjectionPixelShader"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<4>, TEXT("ShadowProjectionPixelShader"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TModulatedShadowProjection<5>, TEXT("ShadowProjectionPixelShader"), TEXT("Main"), SF_Pixel);

// with different quality levels
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<1>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<2>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<3>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<4>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<5>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);

// Implement a pixel shader for rendering one pass point light shadows with different quality levels
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<1>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<2>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<3>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<4>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<5>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);

void StencilingGeometry::DrawSphere(FRHICommandList& RHICmdList)
{
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilSphereVertexBuffer.VertexBufferRHI, sizeof(FVector4), 0);
	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilSphereIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0,
		StencilingGeometry::GStencilSphereVertexBuffer.GetVertexCount(), 0, 
		StencilingGeometry::GStencilSphereIndexBuffer.GetIndexCount() / 3, 1);
}
		
void StencilingGeometry::DrawVectorSphere(FRHICommandList& RHICmdList)
{
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilSphereVectorBuffer.VertexBufferRHI, sizeof(FVector), 0);
	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilSphereIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0,
									StencilingGeometry::GStencilSphereVectorBuffer.GetVertexCount(), 0,
									StencilingGeometry::GStencilSphereIndexBuffer.GetIndexCount() / 3, 1);
}

void StencilingGeometry::DrawCone(FRHICommandList& RHICmdList)
{
	// No Stream Source needed since it will generate vertices on the fly
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilConeVertexBuffer.VertexBufferRHI, sizeof(FVector4), 0);

	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilConeIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0,
		FStencilConeIndexBuffer::NumVerts, 0, StencilingGeometry::GStencilConeIndexBuffer.GetIndexCount() / 3, 1);
}

/** bound shader state for stencil masking the shadow projection [0]:FShadowProjectionNoTransformVS [1]:FShadowProjectionVS */
static FGlobalBoundShaderState MaskBoundShaderState[2];

template <uint32 Quality>
static void SetShadowProjectionShaderTemplNew(FRHICommandList& RHICmdList, int32 ViewIndex, const FViewInfo& View, const FProjectedShadowInfo* ShadowInfo, bool bMobileModulatedProjections)
{
	if (ShadowInfo->bTranslucentShadow)
	{
		// Get the Shadow Projection Vertex Shader (with transforms)
		FShadowProjectionVS* ShadowProjVS = View.ShaderMap->GetShader<FShadowProjectionVS>();

		// Get the translucency pixel shader
		FShadowProjectionPixelShaderInterface* ShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionFromTranslucencyPS<Quality> >();

		// Bind shader
		static FGlobalBoundShaderState BoundShaderState;
		
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), ShadowProjVS, ShadowProjPS);

		// Set shader parameters
		ShadowProjVS->SetParameters(RHICmdList, View, ShadowInfo);
		ShadowProjPS->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
	}
	else if (ShadowInfo->IsWholeSceneDirectionalShadow())
	{
		// Get the Shadow Projection Vertex Shader which does not use a transform
		FShadowProjectionNoTransformVS* ShadowProjVS = View.ShaderMap->GetShader<FShadowProjectionNoTransformVS>();

		// Get the Shadow Projection Pixel Shader for PSSM
		if (ShadowInfo->CascadeSettings.FadePlaneLength > 0)
		{
			// This shader fades the shadow towards the end of the split subfrustum.
			FShadowProjectionPixelShaderInterface* ShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<Quality, true> >();

			static FGlobalBoundShaderState BoundShaderState;
			
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), ShadowProjVS, ShadowProjPS);

			ShadowProjPS->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
		}
		else
		{
			// Do not use the fade plane shader if the fade plane region length is 0 (avoids divide by 0).
			FShadowProjectionPixelShaderInterface* ShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<Quality, false> >();

			static FGlobalBoundShaderState BoundShaderState;
			
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), ShadowProjVS, ShadowProjPS);

			ShadowProjPS->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
		}

		ShadowProjVS->SetParameters(RHICmdList, View);
	}
	else
	{
		// Get the Shadow Projection Vertex Shader
		FShadowProjectionVS* ShadowProjVS = View.ShaderMap->GetShader<FShadowProjectionVS>();

		// Get the Shadow Projection Pixel Shader
		// This shader is the ordinary projection shader used by point/spot lights.		
		FShadowProjectionPixelShaderInterface* ShadowProjPS;
		if(bMobileModulatedProjections)
		{
			ShadowProjPS = View.ShaderMap->GetShader<TModulatedShadowProjection<Quality> >();
		}
		else
		{
			ShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<Quality, false> >();
		}

		static FGlobalBoundShaderState BoundShaderState;
		
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), ShadowProjVS, ShadowProjPS);

		ShadowProjVS->SetParameters(RHICmdList, View, ShadowInfo);
		ShadowProjPS->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
	}
}

void FProjectedShadowInfo::SetBlendStateForProjection(
	FRHICommandListImmediate& RHICmdList, 
	int32 ShadowMapChannel, 
	bool bIsWholeSceneDirectionalShadow,
	bool bUseFadePlane,
	bool bProjectingForForwardShading, 
	bool bMobileModulatedProjections)
{
	// With forward shading we are packing shadowing for all 4 possible stationary lights affecting each pixel into channels of the same texture, based on assigned shadowmap channels.
	// With deferred shading we have 4 channels for each light.  
	//	* CSM and per-object shadows are kept in separate channels to allow fading CSM out to precomputed shadowing while keeping per-object shadows past the fade distance.
	//	* Subsurface shadowing requires an extra channel for each

	if (bProjectingForForwardShading)
	{
		if (bUseFadePlane)
		{
			// alpha is used to fade between cascades
			check(ShadowMapChannel == 0);
			RHICmdList.SetBlendState(TStaticBlendState<CW_RED, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI());
		}
		else
		{
			FBlendStateRHIParamRef BlendState = NULL;

			if (ShadowMapChannel == 0)
			{
				BlendState = TStaticBlendState<CW_RED, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}
			else if (ShadowMapChannel == 1)
			{
				BlendState = TStaticBlendState<CW_GREEN, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}
			else if (ShadowMapChannel == 2)
			{
				BlendState = TStaticBlendState<CW_BLUE, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}
			else if (ShadowMapChannel == 3)
			{
				BlendState = TStaticBlendState<CW_ALPHA, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI();
			}

			checkf(BlendState, TEXT("Only shadows whose stationary lights have a valid ShadowMapChannel can be projected with forward shading"));
			RHICmdList.SetBlendState(BlendState);
		}
	}
	else
	{
		// Light Attenuation channel assignment:
		//  R:     WholeSceneShadows, non SSS
		//  G:     WholeSceneShadows,     SSS
		//  B: non WholeSceneShadows, non SSS
		//  A: non WholeSceneShadows,     SSS
		//
		// SSS: SubsurfaceScattering materials
		// non SSS: shadow for opaque materials
		// WholeSceneShadows: directional light CSM
		// non WholeSceneShadows: spotlight, per object shadows, translucency lighting, omni-directional lights

		if (bIsWholeSceneDirectionalShadow)
		{
			// use R and G in Light Attenuation
			if (bUseFadePlane)
			{
				// alpha is used to fade between cascades, we don't don't need to do BO_Min as we leave B and A untouched which has translucency shadow
				RHICmdList.SetBlendState(TStaticBlendState<CW_RG, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI());
			}
			else
			{
				// first cascade rendered doesn't require fading (CO_Min is needed to combine multiple shadow passes)
				// RTDF shadows: CO_Min is needed to combine with far shadows which overlap the same depth range
				RHICmdList.SetBlendState(TStaticBlendState<CW_RG, BO_Min, BF_One, BF_One>::GetRHI());
			}
		}
		else
		{
			if (bMobileModulatedProjections)
			{
				bool bEncodedHDR = IsMobileHDR32bpp() && !IsMobileHDRMosaic();
				if (bEncodedHDR)
				{
					RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
				}
				else
				{
					// Color modulate shadows, ignore alpha.
					RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_Zero, BF_SourceColor, BO_Add, BF_Zero, BF_One>::GetRHI());
				}
			}
			else
			{
				// use B and A in Light Attenuation
				// CO_Min is needed to combine multiple shadow passes
				RHICmdList.SetBlendState(TStaticBlendState<CW_BA, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI());
			}
		}
	}
}

void FProjectedShadowInfo::SetBlendStateForProjection(FRHICommandListImmediate& RHICmdList, bool bProjectingForForwardShading, bool bMobileModulatedProjections) const
{
	SetBlendStateForProjection(
		RHICmdList, 
		GetLightSceneInfo().Proxy->GetPreviewShadowMapChannel(), 
		IsWholeSceneDirectionalShadow(),
		CascadeSettings.FadePlaneLength > 0 && !bRayTracedDistanceField,
		bProjectingForForwardShading, 
		bMobileModulatedProjections);
}

void FProjectedShadowInfo::RenderProjection(FRHICommandListImmediate& RHICmdList, int32 ViewIndex, const FViewInfo* View, bool bProjectingForForwardShading, bool bMobileModulatedProjections) const
{
#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	GetShadowTypeNameForDrawEvent(EventName);
	SCOPED_DRAW_EVENTF(RHICmdList, EventShadowProjectionActor, *EventName);
#endif

	FScopeCycleCounter Scope(bWholeSceneShadow ? GET_STATID(STAT_RenderWholeSceneShadowProjectionsTime) : GET_STATID(STAT_RenderPerObjectShadowProjectionsTime));

	// Find the shadow's view relevance.
	const FVisibleLightViewInfo& VisibleLightViewInfo = View->VisibleLightInfos[LightSceneInfo->Id];
	{
		FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowId];

		// Don't render shadows for subjects which aren't view relevant.
		if (ViewRelevance.bShadowRelevance == false)
		{
			return;
		}
	}

	FVector4 FrustumVertices[8];

	// Calculate whether the camera is inside the shadow frustum, or the near plane is potentially intersecting the frustum.
	bool bCameraInsideShadowFrustum = true;

	if (!IsWholeSceneDirectionalShadow())
	{
		// The shadow transforms and view transforms are relative to different origins, so the world coordinates need to be translated.
		const FVector PreShadowToPreViewTranslation(View->ViewMatrices.PreViewTranslation - PreShadowTranslation);

		// fill out the frustum vertices (this is only needed in the non-whole scene case)
		for(uint32 vZ = 0;vZ < 2;vZ++)
		{
			for(uint32 vY = 0;vY < 2;vY++)
			{
				for(uint32 vX = 0;vX < 2;vX++)
				{
					const FVector4 UnprojectedVertex = InvReceiverMatrix.TransformFVector4(
						FVector4(
							(vX ? -1.0f : 1.0f),
							(vY ? -1.0f : 1.0f),
							(vZ ?  0.0f : 1.0f),
							1.0f
							)
						);
					const FVector ProjectedVertex = UnprojectedVertex / UnprojectedVertex.W + PreShadowToPreViewTranslation;
					FrustumVertices[GetCubeVertexIndex(vX,vY,vZ)] = FVector4(ProjectedVertex, 0);
				}
			}
		}

		const FVector FrontTopRight = FrustumVertices[GetCubeVertexIndex(0,0,1)] - View->ViewMatrices.PreViewTranslation;
		const FVector FrontTopLeft = FrustumVertices[GetCubeVertexIndex(1,0,1)] - View->ViewMatrices.PreViewTranslation;
		const FVector FrontBottomLeft = FrustumVertices[GetCubeVertexIndex(1,1,1)] - View->ViewMatrices.PreViewTranslation;
		const FVector FrontBottomRight = FrustumVertices[GetCubeVertexIndex(0,1,1)] - View->ViewMatrices.PreViewTranslation;
		const FVector BackTopRight = FrustumVertices[GetCubeVertexIndex(0,0,0)] - View->ViewMatrices.PreViewTranslation;
		const FVector BackTopLeft = FrustumVertices[GetCubeVertexIndex(1,0,0)] - View->ViewMatrices.PreViewTranslation;
		const FVector BackBottomLeft = FrustumVertices[GetCubeVertexIndex(1,1,0)] - View->ViewMatrices.PreViewTranslation;
		const FVector BackBottomRight = FrustumVertices[GetCubeVertexIndex(0,1,0)] - View->ViewMatrices.PreViewTranslation;

		const FPlane Front(FrontTopRight, FrontTopLeft, FrontBottomLeft);
		const float FrontDistance = Front.PlaneDot(View->ViewMatrices.ViewOrigin);

        const FPlane Right(BackBottomRight, BackTopRight, FrontTopRight);
		const float RightDistance = Right.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Back(BackTopLeft, BackTopRight, BackBottomRight);
		const float BackDistance = Back.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Left(FrontTopLeft, BackTopLeft, BackBottomLeft);
		const float LeftDistance = Left.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Top(BackTopRight, BackTopLeft, FrontTopLeft);
		const float TopDistance = Top.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Bottom(FrontBottomRight, FrontBottomLeft, BackBottomLeft);
		const float BottomDistance = Bottom.PlaneDot(View->ViewMatrices.ViewOrigin);

		// Use a distance threshold to treat the case where the near plane is intersecting the frustum as the camera being inside
		// The near plane handling is not exact since it just needs to be conservative about saying the camera is outside the frustum
		const float DistanceThreshold = -View->NearClippingDistance * 3.0f;

		bCameraInsideShadowFrustum = 
			FrontDistance > DistanceThreshold && 
			RightDistance > DistanceThreshold && 
			BackDistance > DistanceThreshold && 
			LeftDistance > DistanceThreshold && 
			TopDistance > DistanceThreshold && 
			BottomDistance > DistanceThreshold;
	}

	// Depth test wo/ writes, no color writing.
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	
	bool bDepthBoundsTestEnabled = false;

	// If this is a preshadow, mask the projection by the receiver primitives.
	if (bPreShadow || bSelfShadowOnly)
	{
		SCOPED_DRAW_EVENTF(RHICmdList, EventMaskSubjects, TEXT("Stencil Mask Subjects"));

		// If instanced stereo is enabled, we need to render each view of the stereo pair using the instanced stereo transform to avoid bias issues.
		const bool bIsInstancedStereoEmulated = View->bIsInstancedStereoEnabled && !View->bIsMultiViewEnabled && View->StereoPass != eSSP_FULL;
		if (bIsInstancedStereoEmulated)
		{
			RHICmdList.SetViewport(0, 0, 0, View->Family->InstancedStereoWidth, View->ViewRect.Max.Y, 1);
		}

		// Set stencil to one.
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false, CF_DepthNearOrEqual,
			true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
			false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
			0xff, 0xff
			>::GetRHI(), 1);

		// Pre-shadows mask by receiver elements, self-shadow mask by subject elements.
		// Note that self-shadow pre-shadows still mask by receiver elements.
		const TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>& DynamicMeshElements = bPreShadow ? DynamicReceiverMeshElements : DynamicSubjectMeshElements;

		FDepthDrawingPolicyFactory::ContextType Context(DDM_AllOccluders, false);

		for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicMeshElements.Num(); MeshBatchIndex++)
		{
			const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicMeshElements[MeshBatchIndex];
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, false, bIsInstancedStereoEmulated);
		}

		// Pre-shadows mask by receiver elements, self-shadow mask by subject elements.
		// Note that self-shadow pre-shadows still mask by receiver elements.
		const PrimitiveArrayType& MaskPrimitives = bPreShadow ? ReceiverPrimitives : DynamicSubjectPrimitives;

		for (int32 PrimitiveIndex = 0, PrimitiveCount = MaskPrimitives.Num(); PrimitiveIndex < PrimitiveCount; PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* ReceiverPrimitiveSceneInfo = MaskPrimitives[PrimitiveIndex];

			if (View->PrimitiveVisibilityMap[ReceiverPrimitiveSceneInfo->GetIndex()])
			{
				const FPrimitiveViewRelevance& ViewRelevance = View->PrimitiveViewRelevanceMap[ReceiverPrimitiveSceneInfo->GetIndex()];

				if (ViewRelevance.bRenderInMainPass && ViewRelevance.bStaticRelevance)
				{
					for (int32 StaticMeshIdx = 0; StaticMeshIdx < ReceiverPrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
					{
						const FStaticMesh& StaticMesh = ReceiverPrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

						if (View->StaticMeshVisibilityMap[StaticMesh.Id])
						{
							const FMeshDrawingRenderState DrawRenderState(View->GetDitheredLODTransitionState(StaticMesh));
							FDepthDrawingPolicyFactory::DrawStaticMesh(
								RHICmdList, 
								*View,
								FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders, false),
								StaticMesh,
								StaticMesh.bRequiresPerElementVisibility ? View->StaticMeshBatchVisibility[StaticMesh.Id] : ((1ull << StaticMesh.Elements.Num() )- 1),
								true,
								DrawRenderState,
								ReceiverPrimitiveSceneInfo->Proxy,
								StaticMesh.BatchHitProxyId, 
								false, 
								bIsInstancedStereoEmulated
								);
						}
					}
				}
			}
		}

		if (bSelfShadowOnly && !bPreShadow)
		{
			for (int32 ElementIndex = 0; ElementIndex < StaticSubjectMeshElements.Num(); ++ElementIndex)
			{
				const FStaticMesh& StaticMesh = *StaticSubjectMeshElements[ElementIndex].Mesh;
				const FMeshDrawingRenderState DrawRenderState(View->GetDitheredLODTransitionState(StaticMesh));
				FDepthDrawingPolicyFactory::DrawStaticMesh(
					RHICmdList, 
					*View,
					FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders, false),
					StaticMesh,
					StaticMesh.bRequiresPerElementVisibility ? View->StaticMeshBatchVisibility[StaticMesh.Id] : ((1ull << StaticMesh.Elements.Num() )- 1),
					true,
					DrawRenderState,
					StaticMesh.PrimitiveSceneInfo->Proxy,
					StaticMesh.BatchHitProxyId, 
					false, 
					bIsInstancedStereoEmulated
					);
			}
		}

		// Restore viewport
		if (bIsInstancedStereoEmulated)
		{
			RHICmdList.SetViewport(View->ViewRect.Min.X, View->ViewRect.Min.Y, 0.0f, View->ViewRect.Max.X, View->ViewRect.Max.Y, 1.0f);
		}
		
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		if( GSupportsDepthBoundsTest && CVarCSMDepthBoundsTest.GetValueOnRenderThread() != 0 )
		{
			FVector4 Near = View->ViewMatrices.ProjMatrix.TransformFVector4(FVector4(0, 0, CascadeSettings.SplitNear));
			FVector4 Far = View->ViewMatrices.ProjMatrix.TransformFVector4(FVector4(0, 0, CascadeSettings.SplitFar));
			float DepthNear = Near.Z / Near.W;
			float DepthFar = Far.Z / Far.W;

			DepthFar = FMath::Clamp( DepthFar, 0.0f, 1.0f );
			DepthNear = FMath::Clamp( DepthNear, 0.0f, 1.0f );

			if( DepthNear <= DepthFar )
			{
				DepthNear = 1.0f;
				DepthFar = 0.0f;
			}

			// Note, using a reversed z depth surface
			RHICmdList.EnableDepthBoundsTest(true, DepthFar, DepthNear);
			bDepthBoundsTestEnabled = true;
		}
		else
		{
			// Increment stencil on front-facing zfail, decrement on back-facing zfail.
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false,CF_DepthNearOrEqual,
			true,CF_Always,SO_Keep,SO_Increment,SO_Keep,
			true,CF_Always,SO_Keep,SO_Decrement,SO_Keep,
			0xff,0xff
			>::GetRHI());

			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

			checkSlow(CascadeSettings.ShadowSplitIndex >= 0);
			checkSlow(bDirectionalLight);

			// Draw 2 fullscreen planes, front facing one at the near subfrustum plane, and back facing one at the far.

			// Find the projection shaders.
			TShaderMapRef<FShadowProjectionNoTransformVS> VertexShaderNoTransform(View->ShaderMap);
			VertexShaderNoTransform->SetParameters(RHICmdList, *View);
			SetGlobalBoundShaderState(RHICmdList, View->GetFeatureLevel(), MaskBoundShaderState[0], GetVertexDeclarationFVector4(), *VertexShaderNoTransform, nullptr);

			FVector4 Near = View->ViewMatrices.ProjMatrix.TransformFVector4(FVector4(0, 0, CascadeSettings.SplitNear));
			FVector4 Far = View->ViewMatrices.ProjMatrix.TransformFVector4(FVector4(0, 0, CascadeSettings.SplitFar));
			float StencilNear = Near.Z / Near.W;
			float StencilFar = Far.Z / Far.W;

			FVector4 Verts[] =
			{
				// Far Plane
				FVector4( 1,  1,  StencilFar),
				FVector4(-1,  1,  StencilFar),
				FVector4( 1, -1,  StencilFar),
				FVector4( 1, -1,  StencilFar),
				FVector4(-1,  1,  StencilFar),
				FVector4(-1, -1,  StencilFar),

				// Near Plane
				FVector4(-1,  1, StencilNear),
				FVector4( 1,  1, StencilNear),
				FVector4(-1, -1, StencilNear),
				FVector4(-1, -1, StencilNear),
				FVector4( 1,  1, StencilNear),
				FVector4( 1, -1, StencilNear),
			};

			// Only draw the near plane if this is not the nearest split
			DrawPrimitiveUP(RHICmdList, PT_TriangleList, (CascadeSettings.ShadowSplitIndex > 0) ? 4 : 2, Verts, sizeof(FVector4));
		}
	}
	// Not a preshadow, mask the projection to any pixels inside the frustum.
	else
	{
		// Solid rasterization wo/ backface culling.
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

		if (bCameraInsideShadowFrustum)
		{
			// Use zfail stenciling when the camera is inside the frustum or the near plane is potentially clipping, 
			// Because zfail handles these cases while zpass does not.
			// zfail stenciling is somewhat slower than zpass because on modern GPUs HiZ will be disabled when setting up stencil.
			// Increment stencil on front-facing zfail, decrement on back-facing zfail.
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_DepthNearOrEqual,
				true,CF_Always,SO_Keep,SO_Increment,SO_Keep,
				true,CF_Always,SO_Keep,SO_Decrement,SO_Keep,
				0xff,0xff
				>::GetRHI());
		}
		else
		{
			// Increment stencil on front-facing zpass, decrement on back-facing zpass.
			// HiZ will be enabled on modern GPUs which will save a little GPU time.
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_DepthNearOrEqual,
				true,CF_Always,SO_Keep,SO_Keep,SO_Increment,
				true,CF_Always,SO_Keep,SO_Keep,SO_Decrement,
				0xff,0xff
				>::GetRHI());
		}
		
		// Find the projection shaders.
		TShaderMapRef<FShadowProjectionVS> VertexShader(View->ShaderMap);

		// Cache the bound shader state
		SetGlobalBoundShaderState(RHICmdList, View->GetFeatureLevel(), MaskBoundShaderState[1], GetVertexDeclarationFVector4(), *VertexShader, NULL);

		// Set the projection vertex shader parameters
		VertexShader->SetParameters(RHICmdList, *View, this);

		// Draw the frustum using the stencil buffer to mask just the pixels which are inside the shadow frustum.
		DrawIndexedPrimitiveUP(RHICmdList, PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(uint16), FrustumVertices, sizeof(FVector4));

		// if rendering modulated shadows mask out subject mesh elements to prevent self shadowing.
		if (bMobileModulatedProjections && !CVarEnableModulatedSelfShadow.GetValueOnRenderThread())
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false, CF_DepthNearOrEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				0xff, 0xff
			>::GetRHI(), 0);

			FDepthDrawingPolicyFactory::ContextType Context(DDM_AllOccluders, false);
			for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicSubjectMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicSubjectMeshElements[MeshBatchIndex];
				const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
				FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
			}
		}
	}

	// solid rasterization w/ back-face culling.
	RHICmdList.SetRasterizerState(
		XOR(View->bReverseCulling, IsWholeSceneDirectionalShadow()) ? TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI());

	if( bDepthBoundsTestEnabled )
	{
		// no depth test or writes
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	}
	else
	{
		// no depth test or writes, Test stencil for non-zero.
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false,CF_Always,
			true,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep,
			false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff
			>::GetRHI());
	}

	SetBlendStateForProjection(RHICmdList, bProjectingForForwardShading, bMobileModulatedProjections);

	{
		uint32 LocalQuality = GetShadowQuality();

		if (LocalQuality > 1)
		{
			if (IsWholeSceneDirectionalShadow() && CascadeSettings.ShadowSplitIndex > 0)
			{
				// adjust kernel size so that the penumbra size of distant splits will better match up with the closer ones
				const float SizeScale = CascadeSettings.ShadowSplitIndex / FMath::Max(0.001f, CVarCSMSplitPenumbraScale.GetValueOnRenderThread());
			}
			else if (LocalQuality > 2 && !bWholeSceneShadow)
			{
				static auto CVarPreShadowResolutionFactor = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.PreShadowResolutionFactor"));
				const int32 TargetResolution = bPreShadow ? FMath::TruncToInt(512 * CVarPreShadowResolutionFactor->GetValueOnRenderThread()) : 512;

				int32 Reduce = 0;

				{
					int32 Res = ResolutionX;

					while (Res < TargetResolution)
					{
						Res *= 2;
						++Reduce;
					}
				}

				// Never drop to quality 1 due to low resolution, aliasing is too bad
				LocalQuality = FMath::Clamp((int32)LocalQuality - Reduce, 3, 5);
			}
		}

		switch(LocalQuality)
		{
			case 1: SetShadowProjectionShaderTemplNew<1>(RHICmdList, ViewIndex, *View, this, bMobileModulatedProjections); break;
			case 2: SetShadowProjectionShaderTemplNew<2>(RHICmdList, ViewIndex, *View, this, bMobileModulatedProjections); break;
			case 3: SetShadowProjectionShaderTemplNew<3>(RHICmdList, ViewIndex, *View, this, bMobileModulatedProjections); break;
			case 4: SetShadowProjectionShaderTemplNew<4>(RHICmdList, ViewIndex, *View, this, bMobileModulatedProjections); break;
			case 5: SetShadowProjectionShaderTemplNew<5>(RHICmdList, ViewIndex, *View, this, bMobileModulatedProjections); break;
			default:
				check(0);
		}
	}

	if (IsWholeSceneDirectionalShadow())
	{
		// Render a full screen quad.
		FVector4 Verts[4] =
		{
			FVector4(-1.0f, 1.0f, 0.0f),
			FVector4(1.0f, 1.0f, 0.0f),
			FVector4(-1.0f, -1.0f, 0.0f),
			FVector4(1.0f, -1.0f, 0.0f),
		};
		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Verts, sizeof(FVector4));
	}
	else
	{
		// Draw the frustum using the projection shader..
		DrawIndexedPrimitiveUP(RHICmdList, PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(uint16), FrustumVertices, sizeof(FVector4));
	}

	if( bDepthBoundsTestEnabled )
	{
		// Disable depth bounds testing
		RHICmdList.EnableDepthBoundsTest(false, 0, 1);
	}
	else
	{
		// Clear the stencil buffer to 0.
		RHICmdList.Clear(false, FColor(0, 0, 0), false, 0, true, 0, FIntRect());
	}
}


template <uint32 Quality>
static void SetPointLightShaderTempl(FRHICommandList& RHICmdList, int32 ViewIndex, const FViewInfo& View, const FProjectedShadowInfo* ShadowInfo)
{
	TShaderMapRef<FShadowProjectionVS> VertexShader(View.ShaderMap);
	TShaderMapRef<TOnePassPointShadowProjectionPS<Quality> > PixelShader(View.ShaderMap);

	static FGlobalBoundShaderState BoundShaderState;
	
	SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);

	VertexShader->SetParameters(RHICmdList, View, ShadowInfo);
	PixelShader->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
}

/** Render one pass point light shadow projections. */
void FProjectedShadowInfo::RenderOnePassPointLightProjection(FRHICommandListImmediate& RHICmdList, int32 ViewIndex, const FViewInfo& View, bool bProjectingForForwardShading) const
{
	SCOPE_CYCLE_COUNTER(STAT_RenderWholeSceneShadowProjectionsTime);

	checkSlow(bOnePassPointLightShadow);
	
	const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

	SetBlendStateForProjection(RHICmdList, bProjectingForForwardShading, false);

	const bool bCameraInsideLightGeometry = ((FVector)View.ViewMatrices.ViewOrigin - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f);

	if (bCameraInsideLightGeometry)
	{
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light geometry
		RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
	}
	else
	{
		// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light geometry
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI());
	}

	{
		uint32 LocalQuality = GetShadowQuality();

		if(LocalQuality > 1)
		{
			// adjust kernel size so that the penumbra size of distant splits will better match up with the closer ones
			//const float SizeScale = ShadowInfo->ResolutionX;
			int32 Reduce = 0;

			{
				int32 Res = ResolutionX;

				while(Res < 512)
				{
					Res *= 2;
					++Reduce;
				}
			}
		}

		switch(LocalQuality)
		{
			case 1: SetPointLightShaderTempl<1>(RHICmdList, ViewIndex, View, this); break;
			case 2: SetPointLightShaderTempl<2>(RHICmdList, ViewIndex, View, this); break;
			case 3: SetPointLightShaderTempl<3>(RHICmdList, ViewIndex, View, this); break;
			case 4: SetPointLightShaderTempl<4>(RHICmdList, ViewIndex, View, this); break;
			case 5: SetPointLightShaderTempl<5>(RHICmdList, ViewIndex, View, this); break;
			default:
				check(0);
		}
	}

	// Project the point light shadow with some approximately bounding geometry, 
	// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
	StencilingGeometry::DrawSphere(RHICmdList);
}

void FProjectedShadowInfo::RenderFrustumWireframe(FPrimitiveDrawInterface* PDI) const
{
	// Find the ID of an arbitrary subject primitive to use to color the shadow frustum.
	int32 SubjectPrimitiveId = 0;
	if(DynamicSubjectPrimitives.Num())
	{
		SubjectPrimitiveId = DynamicSubjectPrimitives[0]->GetIndex();
	}

	const FMatrix InvShadowTransform = (bWholeSceneShadow || bPreShadow) ? SubjectAndReceiverMatrix.InverseFast() : InvReceiverMatrix;

	FColor Color;

	if(IsWholeSceneDirectionalShadow())
	{
		Color = FColor::White;
		switch(CascadeSettings.ShadowSplitIndex)
		{
			case 0: Color = FColor::Red; break;
			case 1: Color = FColor::Yellow; break;
			case 2: Color = FColor::Green; break;
			case 3: Color = FColor::Blue; break;
		}
	}
	else
	{
		Color = FLinearColor::FGetHSV(( ( SubjectPrimitiveId + LightSceneInfo->Id ) * 31 ) & 255, 0, 255).ToFColor(true);
	}

	// Render the wireframe for the frustum derived from ReceiverMatrix.
	DrawFrustumWireframe(
		PDI,
		InvShadowTransform * FTranslationMatrix(-PreShadowTranslation),
		Color,
		SDPG_World
		);
}

FMatrix FProjectedShadowInfo::GetScreenToShadowMatrix(const FSceneView& View, uint32 TileOffsetX, uint32 TileOffsetY, uint32 TileResolutionX, uint32 TileResolutionY) const
{
	const FIntPoint ShadowBufferResolution = GetShadowBufferResolution();
	const float InvBufferResolutionX = 1.0f / (float)ShadowBufferResolution.X;
	const float ShadowResolutionFractionX = 0.5f * (float)TileResolutionX * InvBufferResolutionX;
	const float InvBufferResolutionY = 1.0f / (float)ShadowBufferResolution.Y;
	const float ShadowResolutionFractionY = 0.5f * (float)TileResolutionY * InvBufferResolutionY;
	// Calculate the matrix to transform a screenspace position into shadow map space
	FMatrix ScreenToShadow = 
		// Z of the position being transformed is actually view space Z, 
		// Transform it into post projection space by applying the projection matrix,
		// Which is the required space before applying View.InvTranslatedViewProjectionMatrix
		FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,View.ViewMatrices.ProjMatrix.M[2][2],1),
			FPlane(0,0,View.ViewMatrices.ProjMatrix.M[3][2],0)) * 
		// Transform the post projection space position into translated world space
		// Translated world space is normal world space translated to the view's origin, 
		// Which prevents floating point imprecision far from the world origin.
		View.ViewMatrices.InvTranslatedViewProjectionMatrix *
		// Translate to the origin of the shadow's translated world space
		FTranslationMatrix(PreShadowTranslation - View.ViewMatrices.PreViewTranslation) *
		// Transform into the shadow's post projection space
		// This has to be the same transform used to render the shadow depths
		SubjectAndReceiverMatrix *
		// Scale and translate x and y to be texture coordinates into the ShadowInfo's rectangle in the shadow depth buffer
		// Normalize z by MaxSubjectDepth, as was done when writing shadow depths
		FMatrix(
			FPlane(ShadowResolutionFractionX,0,							0,									0),
			FPlane(0,						 -ShadowResolutionFractionY,0,									0),
			FPlane(0,						0,							InvMaxSubjectDepth,	0),
			FPlane(
				(TileOffsetX + BorderSize) * InvBufferResolutionX + ShadowResolutionFractionX,
				(TileOffsetY + BorderSize) * InvBufferResolutionY + ShadowResolutionFractionY,
				0,
				1
			)
		);
	return ScreenToShadow;
}

FMatrix FProjectedShadowInfo::GetWorldToShadowMatrix(FVector4& ShadowmapMinMax, const FIntPoint* ShadowBufferResolutionOverride) const
{
	FIntPoint ShadowBufferResolution = ( ShadowBufferResolutionOverride ) ? *ShadowBufferResolutionOverride : GetShadowBufferResolution();

	const float InvBufferResolutionX = 1.0f / (float)ShadowBufferResolution.X;
	const float ShadowResolutionFractionX = 0.5f * (float)ResolutionX * InvBufferResolutionX;
	const float InvBufferResolutionY = 1.0f / (float)ShadowBufferResolution.Y;
	const float ShadowResolutionFractionY = 0.5f * (float)ResolutionY * InvBufferResolutionY;

	const FMatrix WorldToShadowMatrix =
		// Translate to the origin of the shadow's translated world space
		FTranslationMatrix(PreShadowTranslation) *
		// Transform into the shadow's post projection space
		// This has to be the same transform used to render the shadow depths
		SubjectAndReceiverMatrix *
		// Scale and translate x and y to be texture coordinates into the ShadowInfo's rectangle in the shadow depth buffer
		// Normalize z by MaxSubjectDepth, as was done when writing shadow depths
		FMatrix(
			FPlane(ShadowResolutionFractionX,0,							0,									0),
			FPlane(0,						 -ShadowResolutionFractionY,0,									0),
			FPlane(0,						0,							InvMaxSubjectDepth,	0),
			FPlane(
				(X + BorderSize) * InvBufferResolutionX + ShadowResolutionFractionX,
				(Y + BorderSize) * InvBufferResolutionY + ShadowResolutionFractionY,
				0,
				1
			)
		);

	ShadowmapMinMax = FVector4(
		(X + BorderSize) * InvBufferResolutionX, 
		(Y + BorderSize) * InvBufferResolutionY,
		(X + BorderSize * 2 + ResolutionX) * InvBufferResolutionX, 
		(Y + BorderSize * 2 + ResolutionY) * InvBufferResolutionY);

	return WorldToShadowMatrix;
}

void FProjectedShadowInfo::UpdateShaderDepthBias()
{
	float DepthBias = 0;

	if (IsWholeScenePointLightShadow())
	{
		DepthBias = CVarPointLightShadowDepthBias.GetValueOnRenderThread() * 512.0f / FMath::Max(ResolutionX, ResolutionY);
		// * 2.0f to be compatible with the system we had before ShadowBias
		DepthBias *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		check(CascadeSettings.ShadowSplitIndex >= 0);

		// the z range is adjusted to we need to adjust here as well
		DepthBias = CVarCSMShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

		float WorldSpaceTexelScale = ShadowBounds.W / ResolutionX;
		
		DepthBias *= WorldSpaceTexelScale;
		DepthBias *= LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (bPreShadow)
	{
		// Preshadows don't need a depth bias since there is no self shadowing
		DepthBias = 0;
	}
	else
	{
		// per object shadows
		if(bDirectionalLight)
		{
			// we use CSMShadowDepthBias cvar but this is per object shadows, maybe we want to use different settings

			// the z range is adjusted to we need to adjust here as well
			DepthBias = CVarPerObjectDirectionalShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

			float WorldSpaceTexelScale = ShadowBounds.W / FMath::Max(ResolutionX, ResolutionY);
		
			DepthBias *= WorldSpaceTexelScale;
			DepthBias *= 0.5f;	// avg GetUserShadowBias, in that case we don't want this adjustable
		}
		else
		{
			// spot lights (old code, might need to be improved)
			const float LightTypeDepthBias = CVarSpotLightShadowDepthBias.GetValueOnRenderThread();
			DepthBias = LightTypeDepthBias * 512.0f / ((MaxSubjectZ - MinSubjectZ) * FMath::Max(ResolutionX, ResolutionY));
			// * 2.0f to be compatible with the system we had before ShadowBias
			DepthBias *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
		}
	}

	ShaderDepthBias = FMath::Max(DepthBias, 0.0f);
}

float FProjectedShadowInfo::ComputeTransitionSize() const
{
	float TransitionSize = 1;

	if (IsWholeScenePointLightShadow())
	{
		// todo: optimize
		TransitionSize = bDirectionalLight ? (1.0f / CVarShadowTransitionScale.GetValueOnRenderThread()) : (1.0f / CVarSpotLightShadowTransitionScale.GetValueOnRenderThread());
		// * 2.0f to be compatible with the system we had before ShadowBias
		TransitionSize *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		check(CascadeSettings.ShadowSplitIndex >= 0);

		// todo: remove GetShadowTransitionScale()
		// make 1/ ShadowTransitionScale, SpotLightShadowTransitionScale

		// the z range is adjusted to we need to adjust here as well
		TransitionSize = CVarCSMShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

		float WorldSpaceTexelScale = ShadowBounds.W / ResolutionX;

		TransitionSize *= WorldSpaceTexelScale;
		TransitionSize *= LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (bPreShadow)
	{
		// Preshadows don't have self shadowing, so make sure the shadow starts as close to the caster as possible
		TransitionSize = 0.00001f;
	}
	else
	{
		// todo: optimize
		TransitionSize = bDirectionalLight ? (1.0f / CVarShadowTransitionScale.GetValueOnRenderThread()) : (1.0f / CVarSpotLightShadowTransitionScale.GetValueOnRenderThread());
		// * 2.0f to be compatible with the system we had before ShadowBias
		TransitionSize *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}

	return TransitionSize;
}

/*-----------------------------------------------------------------------------
FDeferredShadingSceneRenderer
-----------------------------------------------------------------------------*/

/**
 * Used by RenderLights to figure out if projected shadows need to be rendered to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @return true if anything needs to be rendered
 */
bool FSceneRenderer::CheckForProjectedShadows( const FLightSceneInfo* LightSceneInfo ) const
{
	// Find the projected shadows cast by this light.
	const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	for( int32 ShadowIndex=0; ShadowIndex<VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++ )
	{
		const FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			bShadowIsVisible |= VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex];
		}

		if(bShadowIsVisible)
		{
			return true;
		}
	}
	return false;
}

bool FDeferredShadingSceneRenderer::InjectReflectiveShadowMaps(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo)
{
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

	// Inject the RSM into the LPVs
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.RSMsToProject.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.RSMsToProject[ShadowIndex];

		check(ProjectedShadowInfo->bReflectiveShadowmap);

		if (ProjectedShadowInfo->bAllocated && ProjectedShadowInfo->DependentView)
		{
			FSceneViewState* ViewState = (FSceneViewState*)ProjectedShadowInfo->DependentView->State;

			FLightPropagationVolume* LightPropagationVolume = ViewState ? ViewState->GetLightPropagationVolume(FeatureLevel) : NULL;

			if (LightPropagationVolume)
			{
				if (ProjectedShadowInfo->bWholeSceneShadow)
				{
					LightPropagationVolume->InjectDirectionalLightRSM( 
						RHICmdList, 
						*ProjectedShadowInfo->DependentView,
						(const FTexture2DRHIRef&)ProjectedShadowInfo->RenderTargets.ColorTargets[0]->GetRenderTargetItem().ShaderResourceTexture,
						(const FTexture2DRHIRef&)ProjectedShadowInfo->RenderTargets.ColorTargets[1]->GetRenderTargetItem().ShaderResourceTexture, 
						(const FTexture2DRHIRef&)ProjectedShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture,
						*ProjectedShadowInfo, 
						LightSceneInfo->Proxy->GetColor() );
				}
			}
		}
	}

	return true;
}
	
extern int32 GCapsuleShadows;

bool FSceneRenderer::RenderShadowProjections(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bProjectingForForwardShading, bool bMobileModulatedProjections)
{
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	if (bMobileModulatedProjections)
	{
		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);
	}
	else
	{
		// Normal deferred shadows render to light attenuation
		SceneContext.BeginRenderingLightAttenuation(RHICmdList);
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

		const FViewInfo& View = Views[ViewIndex];

		// Set the device viewport for the view.
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		// Set the light's scissor rectangle.
		LightSceneInfo->Proxy->SetScissorRect(RHICmdList, View);

		// Project the shadow depth buffers onto the scene.
		for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

			if (ProjectedShadowInfo->bRayTracedDistanceField)
			{
				ProjectedShadowInfo->RenderRayTracedDistanceFieldProjection(RHICmdList, View, bProjectingForForwardShading);
			}
			else if (ProjectedShadowInfo->bAllocated)
			{
				// Only project the shadow if it's large enough in this particular view (split screen, etc... may have shadows that are large in one view but irrelevantly small in others)
				if (ProjectedShadowInfo->FadeAlphas[ViewIndex] > 1.0f / 256.0f)
				{
					if (ProjectedShadowInfo->bOnePassPointLightShadow)
					{
						ProjectedShadowInfo->RenderOnePassPointLightProjection(RHICmdList, ViewIndex, View, bProjectingForForwardShading);
					}
					else 
					{
						ProjectedShadowInfo->RenderProjection(RHICmdList, ViewIndex, &View, bProjectingForForwardShading, bMobileModulatedProjections);
					}

					if (!bMobileModulatedProjections)
					{
						GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.GetLightAttenuation());
					}
				}
			}
		}

		// Reset the scissor rectangle.
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	}

	return true;
}
	
bool FDeferredShadingSceneRenderer::RenderShadowProjections(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool& bInjectedTranslucentVolume)
{
	SCOPE_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime);
	SCOPED_DRAW_EVENT(RHICmdList, ShadowProjectionOnOpaque);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ShadowProjection);

	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

	FSceneRenderer::RenderShadowProjections(RHICmdList, LightSceneInfo, false, false);

	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

		if (ProjectedShadowInfo->bAllocated
			&& ProjectedShadowInfo->bWholeSceneShadow
			// Not supported on translucency yet
			&& !ProjectedShadowInfo->bRayTracedDistanceField
			// Don't inject shadowed lighting with whole scene shadows used for previewing a light with static shadows,
			// Since that would cause a mismatch with the built lighting
			// However, stationary directional lights allow whole scene shadows that blend with precomputed shadowing
			&& (!LightSceneInfo->Proxy->HasStaticShadowing() || ProjectedShadowInfo->IsWholeSceneDirectionalShadow()))
		{
			bInjectedTranslucentVolume = true;
			SCOPED_DRAW_EVENT(RHICmdList, InjectTranslucentVolume);
			// Inject the shadowed light into the translucency lighting volumes
			InjectTranslucentVolumeLighting(RHICmdList, *LightSceneInfo, ProjectedShadowInfo);
		}
	}

	RenderCapsuleDirectShadows(*LightSceneInfo, RHICmdList, VisibleLightInfo.CapsuleShadowsToProject, false);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];

			if (ProjectedShadowInfo->bAllocated
				&& ProjectedShadowInfo->bWholeSceneShadow)
			{
				View.HeightfieldLightingViewInfo.ComputeShadowMapShadowing(View, RHICmdList, ProjectedShadowInfo);
			}
		}
	}

	return true;
}

void FMobileSceneRenderer::RenderModulatedShadowProjections(FRHICommandListImmediate& RHICmdList)
{
	if (IsSimpleForwardShadingEnabled(GetFeatureLevelShaderPlatform(FeatureLevel)) || !ViewFamily.EngineShowFlags.DynamicShadows)
	{
		return;
	}
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// render shadowmaps for relevant lights.
	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
		if(LightSceneInfo->ShouldRenderLightViewIndependent() && LightSceneInfo->Proxy && LightSceneInfo->Proxy->CastsModulatedShadows())
		{
			TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
			SCOPE_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime);
			FSceneRenderer::RenderShadowProjections(RHICmdList, LightSceneInfo, false, true);
		}
	}
}