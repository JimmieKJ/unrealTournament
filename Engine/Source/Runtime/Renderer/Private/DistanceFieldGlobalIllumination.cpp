// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldGlobalIllumination.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "DistanceFieldGlobalIllumination.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "DistanceFieldAtlas.h"

int32 GDistanceFieldGI = 0;
FAutoConsoleVariableRef CVarDistanceFieldGI(
	TEXT("r.DistanceFieldGI"),
	GDistanceFieldGI,
	TEXT(""),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

int32 GVPLMeshGlobalIllumination = 1;
FAutoConsoleVariableRef CVarVPLMeshGlobalIllumination(
	TEXT("r.VPLMeshGlobalIllumination"),
	GVPLMeshGlobalIllumination,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GVPLSurfelRepresentation = 1;
FAutoConsoleVariableRef CVarVPLSurfelRepresentation(
	TEXT("r.VPLSurfelRepresentation"),
	GVPLSurfelRepresentation,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GVPLGridDimension = 128;
FAutoConsoleVariableRef CVarVPLGridDimension(
	TEXT("r.VPLGridDimension"),
	GVPLGridDimension,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GVPLDirectionalLightTraceDistance = 100000;
FAutoConsoleVariableRef CVarVPLDirectionalLightTraceDistance(
	TEXT("r.VPLDirectionalLightTraceDistance"),
	GVPLDirectionalLightTraceDistance,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GVPLPlacementCameraRadius = 4000;
FAutoConsoleVariableRef CVarVPLPlacementCameraRadius(
	TEXT("r.VPLPlacementCameraRadius"),
	GVPLPlacementCameraRadius,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GVPLViewCulling = 1;
FAutoConsoleVariableRef CVarVPLViewCulling(
	TEXT("r.VPLViewCulling"),
	GVPLViewCulling,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GAOUseConesForGI = 1;
FAutoConsoleVariableRef CVarAOUseConesForGI(
	TEXT("r.AOUseConesForGI"),
	GAOUseConesForGI,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

int32 GVPLSpreadUpdateOver = 5;
FAutoConsoleVariableRef CVarVPLSpreadUpdateOver(
	TEXT("r.VPLSpreadUpdateOver"),
	GVPLSpreadUpdateOver,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

float GVPLSelfOcclusionReplacement = .3f;
FAutoConsoleVariableRef CVarVPLSelfOcclusionReplacement(
	TEXT("r.VPLSelfOcclusionReplacement"),
	GVPLSelfOcclusionReplacement,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

TGlobalResource<FVPLResources> GVPLResources;
TGlobalResource<FVPLResources> GCulledVPLResources;

extern TGlobalResource<FDistanceFieldObjectBufferResource> GShadowCulledObjectBuffers;

class FVPLPlacementCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVPLPlacementCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	/** Default constructor. */
	FVPLPlacementCS() {}

	/** Initialization constructor. */
	FVPLPlacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("VPLParameterBuffer"));
		VPLData.Bind(Initializer.ParameterMap, TEXT("VPLData"));
		InvPlacementGridSize.Bind(Initializer.ParameterMap, TEXT("InvPlacementGridSize"));
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		ShadowToWorld.Bind(Initializer.ParameterMap, TEXT("ShadowToWorld"));
		LightDirectionAndTraceDistance.Bind(Initializer.ParameterMap, TEXT("LightDirectionAndTraceDistance"));
		LightColor.Bind(Initializer.ParameterMap, TEXT("LightColor"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		ShadowTileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("ShadowTileHeadDataUnpacked"));
		ShadowTileArrayData.Bind(Initializer.ParameterMap, TEXT("ShadowTileArrayData"));
		ShadowTileListGroupSize.Bind(Initializer.ParameterMap, TEXT("ShadowTileListGroupSize"));
		VPLPlacementCameraRadius.Bind(Initializer.ParameterMap, TEXT("VPLPlacementCameraRadius"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FSceneView& View, 
		const FLightSceneProxy* LightSceneProxy,
		FVector2D InvPlacementGridSizeValue,
		const FMatrix& WorldToShadowValue,
		const FMatrix& ShadowToWorldValue,
		FLightTileIntersectionResources* TileIntersectionResources)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);

		SetShaderValue(RHICmdList, ShaderRHI, InvPlacementGridSize, InvPlacementGridSizeValue);
		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowValue);
		SetShaderValue(RHICmdList, ShaderRHI, ShadowToWorld, ShadowToWorldValue);
		SetShaderValue(RHICmdList, ShaderRHI, LightDirectionAndTraceDistance, FVector4(LightSceneProxy->GetDirection(), GVPLDirectionalLightTraceDistance));
		SetShaderValue(RHICmdList, ShaderRHI, LightColor, LightSceneProxy->GetColor() * LightSceneProxy->GetIndirectLightingScale());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = GVPLResources.VPLParameterBuffer.UAV;
		OutUAVs[1] = GVPLResources.VPLData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		VPLParameterBuffer.SetBuffer(RHICmdList, ShaderRHI, GVPLResources.VPLParameterBuffer);
		VPLData.SetBuffer(RHICmdList, ShaderRHI, GVPLResources.VPLData);

		check(TileIntersectionResources || !ShadowTileHeadDataUnpacked.IsBound());

		if (TileIntersectionResources)
		{
			SetSRVParameter(RHICmdList, ShaderRHI, ShadowTileHeadDataUnpacked, TileIntersectionResources->TileHeadDataUnpacked.SRV);
			SetSRVParameter(RHICmdList, ShaderRHI, ShadowTileArrayData, TileIntersectionResources->TileArrayData.SRV);
			SetShaderValue(RHICmdList, ShaderRHI, ShadowTileListGroupSize, TileIntersectionResources->TileDimensions);
		}

		SetShaderValue(RHICmdList, ShaderRHI, VPLPlacementCameraRadius, GVPLPlacementCameraRadius);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		VPLParameterBuffer.UnsetUAV(RHICmdList, GetComputeShader());
		VPLData.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = GVPLResources.VPLParameterBuffer.UAV;
		OutUAVs[1] = GVPLResources.VPLData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VPLParameterBuffer;
		Ar << VPLData;
		Ar << InvPlacementGridSize;
		Ar << WorldToShadow;
		Ar << ShadowToWorld;
		Ar << LightDirectionAndTraceDistance;
		Ar << LightColor;
		Ar << ObjectParameters;
		Ar << ShadowTileHeadDataUnpacked;
		Ar << ShadowTileArrayData;
		Ar << ShadowTileListGroupSize;
		Ar << VPLPlacementCameraRadius;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter VPLParameterBuffer;
	FRWShaderParameter VPLData;
	FShaderParameter InvPlacementGridSize;
	FShaderParameter WorldToShadow;
	FShaderParameter ShadowToWorld;
	FShaderParameter LightDirectionAndTraceDistance;
	FShaderParameter LightColor;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FShaderResourceParameter ShadowTileHeadDataUnpacked;
	FShaderResourceParameter ShadowTileArrayData;
	FShaderParameter ShadowTileListGroupSize;
	FShaderParameter VPLPlacementCameraRadius;
};

IMPLEMENT_SHADER_TYPE(,FVPLPlacementCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("VPLPlacementCS"),SF_Compute);



class FSetupVPLCullndirectArgumentsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSetupVPLCullndirectArgumentsCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	FSetupVPLCullndirectArgumentsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DispatchParameters.Bind(Initializer.ParameterMap, TEXT("DispatchParameters"));
		VPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("VPLParameterBuffer"));
	}

	FSetupVPLCullndirectArgumentsCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, GVPLResources.VPLDispatchIndirectBuffer.UAV);
		DispatchParameters.SetBuffer(RHICmdList, ShaderRHI, GVPLResources.VPLDispatchIndirectBuffer);
		SetSRVParameter(RHICmdList, ShaderRHI, VPLParameterBuffer, GVPLResources.VPLParameterBuffer.SRV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		DispatchParameters.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, GVPLResources.VPLDispatchIndirectBuffer.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DispatchParameters;
		Ar << VPLParameterBuffer;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter DispatchParameters;
	FShaderResourceParameter VPLParameterBuffer;
};

IMPLEMENT_SHADER_TYPE(,FSetupVPLCullndirectArgumentsCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("SetupVPLCullndirectArgumentsCS"),SF_Compute);

class FCullVPLsForViewCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullVPLsForViewCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	FCullVPLsForViewCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("VPLParameterBuffer"));
		VPLData.Bind(Initializer.ParameterMap, TEXT("VPLData"));
		CulledVPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("CulledVPLParameterBuffer"));
		CulledVPLData.Bind(Initializer.ParameterMap, TEXT("CulledVPLData"));
		AOParameters.Bind(Initializer.ParameterMap);
		NumConvexHullPlanes.Bind(Initializer.ParameterMap, TEXT("NumConvexHullPlanes"));
		ViewFrustumConvexHull.Bind(Initializer.ParameterMap, TEXT("ViewFrustumConvexHull"));
	}

	FCullVPLsForViewCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, const FSceneView& View, const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = GCulledVPLResources.VPLParameterBuffer.UAV;
		OutUAVs[1] = GCulledVPLResources.VPLData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		CulledVPLParameterBuffer.SetBuffer(RHICmdList, ShaderRHI, GCulledVPLResources.VPLParameterBuffer);
		CulledVPLData.SetBuffer(RHICmdList, ShaderRHI, GCulledVPLResources.VPLData);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		SetSRVParameter(RHICmdList, ShaderRHI, VPLParameterBuffer, GVPLResources.VPLParameterBuffer.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, VPLData, GVPLResources.VPLData.SRV);

		// Shader assumes max 6
		check(View.ViewFrustum.Planes.Num() <= 6);
		SetShaderValue(RHICmdList, ShaderRHI, NumConvexHullPlanes, View.ViewFrustum.Planes.Num());
		SetShaderValueArray(RHICmdList, ShaderRHI, ViewFrustumConvexHull, View.ViewFrustum.Planes.GetData(), View.ViewFrustum.Planes.Num());
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		CulledVPLParameterBuffer.UnsetUAV(RHICmdList, GetComputeShader());
		CulledVPLData.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VPLParameterBuffer;
		Ar << VPLData;
		Ar << CulledVPLParameterBuffer;
		Ar << CulledVPLData;
		Ar << AOParameters;
		Ar << NumConvexHullPlanes;
		Ar << ViewFrustumConvexHull;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter VPLParameterBuffer;
	FShaderResourceParameter VPLData;
	FRWShaderParameter CulledVPLParameterBuffer;
	FRWShaderParameter CulledVPLData;
	FAOParameters AOParameters;
	FShaderParameter NumConvexHullPlanes;
	FShaderParameter ViewFrustumConvexHull;
};

IMPLEMENT_SHADER_TYPE(,FCullVPLsForViewCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("CullVPLsForViewCS"),SF_Compute);

TScopedPointer<FLightTileIntersectionResources> GVPLPlacementTileIntersectionResources;

void PlaceVPLs(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FScene* Scene,
	const FDistanceFieldAOParameters& Parameters)
{
	GVPLResources.AllocateFor(GVPLGridDimension * GVPLGridDimension);

	{
		uint32 ClearValues[4] = { 0 };
		RHICmdList.ClearUAV(GVPLResources.VPLParameterBuffer.UAV, ClearValues);
	}

	const FLightSceneProxy* DirectionalLightProxy = NULL;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent() 
			&& LightSceneInfo->Proxy->GetLightType() == LightType_Directional
			&& LightSceneInfo->Proxy->CastsDynamicShadow())
		{
			DirectionalLightProxy = LightSceneInfo->Proxy;
			break;
		}
	}

	if (DirectionalLightProxy)
	{
		SCOPED_DRAW_EVENT(RHICmdList, VPLPlacement);
		FMatrix DirectionalLightShadowToWorld;

		{
			int32 NumPlanes = 0;
			const FPlane* PlaneData = NULL;
			FVector4 ShadowBoundingSphereValue(0, 0, 0, 0);
			FShadowCascadeSettings CascadeSettings;
			FSphere ShadowBounds;
			FConvexVolume FrustumVolume;

			static bool bUseShadowmapBounds = true;

			if (bUseShadowmapBounds)
			{
				ShadowBounds = DirectionalLightProxy->GetShadowSplitBoundsDepthRange(
					View, 
					View.ViewMatrices.ViewOrigin,
					View.NearClippingDistance, 
					GVPLPlacementCameraRadius, 
					&CascadeSettings);

				FSphere SubjectBounds(FVector::ZeroVector, ShadowBounds.W);

				const FMatrix& WorldToLight = DirectionalLightProxy->GetWorldToLight();
				const FMatrix InitializerWorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
				const FVector InitializerFaceDirection = FVector(1,0,0);

				FVector	XAxis, YAxis;
				InitializerFaceDirection.FindBestAxisVectors(XAxis, YAxis);
				const FMatrix WorldToLightScaled = InitializerWorldToLight * FScaleMatrix(FVector(1.0f,1.0f / SubjectBounds.W,1.0f / SubjectBounds.W));
				const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,InitializerFaceDirection.GetSafeNormal(),FVector::ZeroVector);

				static bool bSnapPosition = true;

				if (bSnapPosition)
				{
					// Transform the shadow's position into shadowmap space
					const FVector TransformedPosition = WorldToFace.TransformPosition(ShadowBounds.Center);

					// Determine the distance necessary to snap the shadow's position to the nearest texel
					const float SnapX = FMath::Fmod(TransformedPosition.X, 2.0f / GVPLGridDimension);
					const float SnapY = FMath::Fmod(TransformedPosition.Y, 2.0f / GVPLGridDimension);
					// Snap the shadow's position and transform it back into world space
					// This snapping prevents sub-texel camera movements which removes view dependent aliasing from the final shadow result
					// This only maintains stable shadows under camera translation and rotation
					const FVector SnappedWorldPosition = WorldToFace.InverseFast().TransformPosition(TransformedPosition - FVector(SnapX, SnapY, 0.0f));
					ShadowBounds.Center = SnappedWorldPosition;
				}

				NumPlanes = CascadeSettings.ShadowBoundsAccurate.Planes.Num();
				PlaneData = CascadeSettings.ShadowBoundsAccurate.Planes.GetData();

				DirectionalLightShadowToWorld = FTranslationMatrix(-ShadowBounds.Center) * WorldToFace * FShadowProjectionMatrix(-GVPLDirectionalLightTraceDistance / 2, GVPLDirectionalLightTraceDistance / 2, FVector4(0,0,0,1));
			}
			else
			{
				ShadowBounds = FSphere(View.ViewMatrices.ViewOrigin, GVPLPlacementCameraRadius);

				FSphere SubjectBounds(FVector::ZeroVector, ShadowBounds.W);

				const FMatrix& WorldToLight = DirectionalLightProxy->GetWorldToLight();
				const FMatrix InitializerWorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
				const FVector InitializerFaceDirection = FVector(1,0,0);

				FVector	XAxis, YAxis;
				InitializerFaceDirection.FindBestAxisVectors(XAxis, YAxis);
				const FMatrix WorldToLightScaled = InitializerWorldToLight * FScaleMatrix(FVector(1.0f,1.0f / GVPLPlacementCameraRadius,1.0f / GVPLPlacementCameraRadius));
				const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,InitializerFaceDirection.GetSafeNormal(),FVector::ZeroVector);

				static bool bSnapPosition = true;

				if (bSnapPosition)
				{
					// Transform the shadow's position into shadowmap space
					const FVector TransformedPosition = WorldToFace.TransformPosition(ShadowBounds.Center);

					// Determine the distance necessary to snap the shadow's position to the nearest texel
					const float SnapX = FMath::Fmod(TransformedPosition.X, 2.0f / GVPLGridDimension);
					const float SnapY = FMath::Fmod(TransformedPosition.Y, 2.0f / GVPLGridDimension);
					// Snap the shadow's position and transform it back into world space
					// This snapping prevents sub-texel camera movements which removes view dependent aliasing from the final shadow result
					// This only maintains stable shadows under camera translation and rotation
					const FVector SnappedWorldPosition = WorldToFace.InverseFast().TransformPosition(TransformedPosition - FVector(SnapX, SnapY, 0.0f));
					ShadowBounds.Center = SnappedWorldPosition;
				}

				const float MaxSubjectZ = WorldToFace.TransformPosition(SubjectBounds.Center).Z + SubjectBounds.W;
				const float MinSubjectZ = FMath::Max(MaxSubjectZ - SubjectBounds.W * 2, (float)-HALF_WORLD_MAX);

				//@todo - naming is wrong and maybe derived matrices
				DirectionalLightShadowToWorld = FTranslationMatrix(-ShadowBounds.Center) * WorldToFace * FShadowProjectionMatrix(MinSubjectZ, MaxSubjectZ, FVector4(0,0,0,1));

				GetViewFrustumBounds(FrustumVolume, DirectionalLightShadowToWorld, true);

				NumPlanes = FrustumVolume.Planes.Num();
				PlaneData = FrustumVolume.Planes.GetData();
			}

			CullDistanceFieldObjectsForLight(
				RHICmdList,
				View,
				DirectionalLightProxy, 
				DirectionalLightShadowToWorld,
				NumPlanes,
				PlaneData,
				ShadowBoundingSphereValue,
				ShadowBounds.W,
				GVPLPlacementTileIntersectionResources);
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, PlaceVPLs);

			TShaderMapRef<FVPLPlacementCS> ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DirectionalLightProxy, FVector2D(1.0f / GVPLGridDimension, 1.0f / GVPLGridDimension), DirectionalLightShadowToWorld, DirectionalLightShadowToWorld.InverseFast(), GVPLPlacementTileIntersectionResources);
			DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<int32>(GVPLGridDimension, GDistanceFieldAOTileSizeX), FMath::DivideAndRoundUp<int32>(GVPLGridDimension, GDistanceFieldAOTileSizeY), 1);

			ComputeShader->UnsetParameters(RHICmdList);
		}
		
		if (GVPLViewCulling)
		{
			{
				TShaderMapRef<FSetupVPLCullndirectArgumentsCS> ComputeShader(View.ShaderMap);
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View);

				DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
				ComputeShader->UnsetParameters(RHICmdList);
			}

			{
				GCulledVPLResources.AllocateFor(GVPLGridDimension * GVPLGridDimension);

				uint32 ClearValues[4] = { 0 };
				RHICmdList.ClearUAV(GCulledVPLResources.VPLParameterBuffer.UAV, ClearValues);

				TShaderMapRef<FCullVPLsForViewCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, Scene, View, Parameters);

				DispatchIndirectComputeShader(RHICmdList, *ComputeShader, GVPLResources.VPLDispatchIndirectBuffer.Buffer, 0);
				ComputeShader->UnsetParameters(RHICmdList);
			}
		}
	}
}

const int32 LightVPLsThreadGroupSize = 64;

class FSetupLightVPLsIndirectArgumentsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSetupLightVPLsIndirectArgumentsCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("LIGHT_VPLS_THREADGROUP_SIZE"), LightVPLsThreadGroupSize);
	}

	FSetupLightVPLsIndirectArgumentsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DispatchParameters.Bind(Initializer.ParameterMap, TEXT("DispatchParameters"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		ObjectProcessStride.Bind(Initializer.ParameterMap, TEXT("ObjectProcessStride"));
	}

	FSetupLightVPLsIndirectArgumentsCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, GAOCulledObjectBuffers.Buffers.ObjectIndirectDispatch.UAV);
		DispatchParameters.SetBuffer(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers.ObjectIndirectDispatch);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);

		SetShaderValue(RHICmdList, ShaderRHI, ObjectProcessStride, GVPLSpreadUpdateOver);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		DispatchParameters.UnsetUAV(RHICmdList, ShaderRHI);

		ObjectParameters.UnsetParameters(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, GAOCulledObjectBuffers.Buffers.ObjectIndirectDispatch.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DispatchParameters;
		Ar << ObjectParameters;
		Ar << ObjectProcessStride;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter DispatchParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FShaderParameter ObjectProcessStride;
};

IMPLEMENT_SHADER_TYPE(,FSetupLightVPLsIndirectArgumentsCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("SetupLightVPLsIndirectArgumentsCS"),SF_Compute);

class FLightVPLsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLightVPLsCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("LIGHT_VPLS_THREADGROUP_SIZE"), LightVPLsThreadGroupSize);
	}

	/** Default constructor. */
	FLightVPLsCS() {}

	/** Initialization constructor. */
	FLightVPLsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		LightDirection.Bind(Initializer.ParameterMap, TEXT("LightDirection"));
		LightSourceRadius.Bind(Initializer.ParameterMap, TEXT("LightSourceRadius"));
		LightPositionAndInvRadius.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		TanLightAngleAndNormalThreshold.Bind(Initializer.ParameterMap, TEXT("TanLightAngleAndNormalThreshold"));
		LightColor.Bind(Initializer.ParameterMap, TEXT("LightColor"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		SurfelParameters.Bind(Initializer.ParameterMap);
		ShadowTileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("ShadowTileHeadDataUnpacked"));
		ShadowTileArrayData.Bind(Initializer.ParameterMap, TEXT("ShadowTileArrayData"));
		ShadowTileListGroupSize.Bind(Initializer.ParameterMap, TEXT("ShadowTileListGroupSize"));
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		ShadowObjectIndirectArguments.Bind(Initializer.ParameterMap, TEXT("ShadowObjectIndirectArguments"));
		ShadowCulledObjectBounds.Bind(Initializer.ParameterMap, TEXT("ShadowCulledObjectBounds"));
		ShadowCulledObjectData.Bind(Initializer.ParameterMap, TEXT("ShadowCulledObjectData"));
		ObjectProcessStride.Bind(Initializer.ParameterMap, TEXT("ObjectProcessStride"));
		ObjectProcessStartIndex.Bind(Initializer.ParameterMap, TEXT("ObjectProcessStartIndex"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FLightSceneProxy* LightSceneProxy,
		const FMatrix& WorldToShadowMatrixValue,
		const FDistanceFieldAOParameters& Parameters,
		FLightTileIntersectionResources* TileIntersectionResources)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		const FScene* Scene = (const FScene*)View.Family->Scene;

		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		SurfelParameters.Set(RHICmdList, ShaderRHI, *Scene->DistanceFieldSceneData.SurfelBuffers, *Scene->DistanceFieldSceneData.InstancedSurfelBuffers);

		FVector4 LightPositionAndInvRadiusValue;
		FVector4 LightColorAndFalloffExponent;
		FVector NormalizedLightDirection;
		FVector2D SpotAngles;
		float LightSourceRadiusValue;
		float LightSourceLength;
		float LightMinRoughness;

		LightSceneProxy->GetParameters(LightPositionAndInvRadiusValue, LightColorAndFalloffExponent, NormalizedLightDirection, SpotAngles, LightSourceRadiusValue, LightSourceLength, LightMinRoughness);

		SetShaderValue(RHICmdList, ShaderRHI, LightDirection, NormalizedLightDirection);
		SetShaderValue(RHICmdList, ShaderRHI, LightPositionAndInvRadius, LightPositionAndInvRadiusValue);
		// Default light source radius of 0 gives poor results
		SetShaderValue(RHICmdList, ShaderRHI, LightSourceRadius, LightSourceRadiusValue == 0 ? 20 : FMath::Clamp(LightSourceRadiusValue, .001f, 1.0f / (4 * LightPositionAndInvRadiusValue.W)));

		const float LightSourceAngle = FMath::Clamp(LightSceneProxy->GetLightSourceAngle(), 0.001f, 5.0f) * PI / 180.0f;
		const FVector2D TanLightAngleAndNormalThresholdValue(FMath::Tan(LightSourceAngle), FMath::Cos(PI / 2 + LightSourceAngle));
		SetShaderValue(RHICmdList, ShaderRHI, TanLightAngleAndNormalThreshold, TanLightAngleAndNormalThresholdValue);
		SetShaderValue(RHICmdList, ShaderRHI, LightColor, LightSceneProxy->GetColor() * LightSceneProxy->GetIndirectLightingScale());

		check(TileIntersectionResources || !ShadowTileHeadDataUnpacked.IsBound());

		if (TileIntersectionResources)
		{
			SetSRVParameter(RHICmdList, ShaderRHI, ShadowTileHeadDataUnpacked, TileIntersectionResources->TileHeadDataUnpacked.SRV);
			SetSRVParameter(RHICmdList, ShaderRHI, ShadowTileArrayData, TileIntersectionResources->TileArrayData.SRV);
			SetShaderValue(RHICmdList, ShaderRHI, ShadowTileListGroupSize, TileIntersectionResources->TileDimensions);
		}

		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowMatrixValue);

		SetSRVParameter(RHICmdList, ShaderRHI, ShadowObjectIndirectArguments, GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ShadowCulledObjectBounds, GShadowCulledObjectBuffers.Buffers.Bounds.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ShadowCulledObjectData, GShadowCulledObjectBuffers.Buffers.Data.SRV);

		SetShaderValue(RHICmdList, ShaderRHI, ObjectProcessStride, GVPLSpreadUpdateOver);
		SetShaderValue(RHICmdList, ShaderRHI, ObjectProcessStartIndex, GFrameNumberRenderThread % GVPLSpreadUpdateOver);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		SurfelParameters.UnsetParameters(RHICmdList, GetComputeShader());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << LightDirection;
		Ar << LightPositionAndInvRadius;
		Ar << LightSourceRadius;
		Ar << TanLightAngleAndNormalThreshold;
		Ar << LightColor;
		Ar << ObjectParameters;
		Ar << SurfelParameters;
		Ar << ShadowTileHeadDataUnpacked;
		Ar << ShadowTileArrayData;
		Ar << ShadowTileListGroupSize;
		Ar << WorldToShadow;
		Ar << ShadowObjectIndirectArguments;
		Ar << ShadowCulledObjectBounds;
		Ar << ShadowCulledObjectData;
		Ar << ObjectProcessStride;
		Ar << ObjectProcessStartIndex;
		return bShaderHasOutdatedParameters;
	}

private:

	FAOParameters AOParameters;
	FShaderParameter LightDirection;
	FShaderParameter LightPositionAndInvRadius;
	FShaderParameter LightSourceRadius;
	FShaderParameter TanLightAngleAndNormalThreshold;
	FShaderParameter LightColor;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FSurfelBufferParameters SurfelParameters;
	FShaderResourceParameter ShadowTileHeadDataUnpacked;
	FShaderResourceParameter ShadowTileArrayData;
	FShaderParameter ShadowTileListGroupSize;
	FShaderParameter WorldToShadow;
	FShaderResourceParameter ShadowObjectIndirectArguments;
	FShaderResourceParameter ShadowCulledObjectBounds;
	FShaderResourceParameter ShadowCulledObjectData;
	FShaderParameter ObjectProcessStride;
	FShaderParameter ObjectProcessStartIndex;
};

IMPLEMENT_SHADER_TYPE(,FLightVPLsCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("LightVPLsCS"),SF_Compute);

void UpdateVPLs(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FScene* Scene,
	const FDistanceFieldAOParameters& Parameters)
{
	if (GVPLMeshGlobalIllumination)
	{
		if (GVPLSurfelRepresentation)
		{
			SCOPED_DRAW_EVENT(RHICmdList, UpdateVPLs);

			const FLightSceneProxy* DirectionalLightProxy = NULL;

			for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
			{
				const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
				const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

				if (LightSceneInfo->ShouldRenderLightViewIndependent() 
					&& LightSceneInfo->Proxy->GetLightType() == LightType_Directional
					&& LightSceneInfo->Proxy->CastsDynamicShadow())
				{
					DirectionalLightProxy = LightSceneInfo->Proxy;
					break;
				}
			}

			FMatrix DirectionalLightWorldToShadow = FMatrix::Identity;

			if (DirectionalLightProxy)
			{
				{
					int32 NumPlanes = 0;
					const FPlane* PlaneData = NULL;
					FVector4 ShadowBoundingSphereValue(0, 0, 0, 0);
					FShadowCascadeSettings CascadeSettings;
					FSphere ShadowBounds;
					FConvexVolume FrustumVolume;

					{
						const float ConeExpandDistance = Parameters.ObjectMaxOcclusionDistance;
						const float TanHalfFOV = 1.0f / View.ViewMatrices.ProjMatrix.M[0][0];
						const float VertexPullbackLength = ConeExpandDistance / TanHalfFOV;

						// Pull back cone vertex to contain VPLs outside of the view
						const FVector ViewConeVertex = View.ViewMatrices.ViewOrigin - View.GetViewDirection() * VertexPullbackLength;

						//@todo - expand by AOObjectMaxDistance
						ShadowBounds = DirectionalLightProxy->GetShadowSplitBoundsDepthRange(
							View, 
							ViewConeVertex,
							View.NearClippingDistance, 
							GetMaxAOViewDistance() + VertexPullbackLength + Parameters.ObjectMaxOcclusionDistance, 
							&CascadeSettings); 

						FSphere SubjectBounds(FVector::ZeroVector, ShadowBounds.W);

						const FMatrix& WorldToLight = DirectionalLightProxy->GetWorldToLight();
						const FMatrix InitializerWorldToLight = FInverseRotationMatrix(FVector(WorldToLight.M[0][0],WorldToLight.M[1][0],WorldToLight.M[2][0]).GetSafeNormal().Rotation());
						const FVector InitializerFaceDirection = FVector(1,0,0);

						FVector	XAxis, YAxis;
						InitializerFaceDirection.FindBestAxisVectors(XAxis, YAxis);
						const FMatrix WorldToLightScaled = InitializerWorldToLight * FScaleMatrix(FVector(1.0f,1.0f / SubjectBounds.W,1.0f / SubjectBounds.W));
						const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,InitializerFaceDirection.GetSafeNormal(),FVector::ZeroVector);

						NumPlanes = CascadeSettings.ShadowBoundsAccurate.Planes.Num();
						PlaneData = CascadeSettings.ShadowBoundsAccurate.Planes.GetData();

						DirectionalLightWorldToShadow = FTranslationMatrix(-ShadowBounds.Center) 
							* WorldToFace 
							* FShadowProjectionMatrix(-GVPLDirectionalLightTraceDistance / 2, GVPLDirectionalLightTraceDistance / 2, FVector4(0,0,0,1));
					}

					CullDistanceFieldObjectsForLight(
						RHICmdList,
						View,
						DirectionalLightProxy, 
						DirectionalLightWorldToShadow,
						NumPlanes,
						PlaneData,
						ShadowBoundingSphereValue,
						ShadowBounds.W,
						GVPLPlacementTileIntersectionResources);
				}

				SCOPED_DRAW_EVENT(RHICmdList, LightVPLs);

				{
					TShaderMapRef<FSetupLightVPLsIndirectArgumentsCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View);

					DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
					ComputeShader->UnsetParameters(RHICmdList);
				}

				{
					TShaderMapRef<FLightVPLsCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, DirectionalLightProxy, DirectionalLightWorldToShadow, Parameters, GVPLPlacementTileIntersectionResources);
					DispatchIndirectComputeShader(RHICmdList, *ComputeShader, GAOCulledObjectBuffers.Buffers.ObjectIndirectDispatch.Buffer, 0);
					ComputeShader->UnsetParameters(RHICmdList);
				}
			}
			else
			{
				uint32 ClearValues[4] = { 0 };
				RHICmdList.ClearUAV(Scene->DistanceFieldSceneData.InstancedSurfelBuffers->VPLFlux.UAV, ClearValues);
			}
		}
		else
		{
			PlaceVPLs(RHICmdList, View, Scene, Parameters);
		}
	}
}


class FClearIrradianceSamplesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FClearIrradianceSamplesCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FClearIrradianceSamplesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SurfelIrradiance.Bind(Initializer.ParameterMap, TEXT("SurfelIrradiance"));
		HeightfieldIrradiance.Bind(Initializer.ParameterMap, TEXT("HeightfieldIrradiance"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
	}

	FClearIrradianceSamplesCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		int32 DepthLevel, 
		FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, ScatterDrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, SavedStartIndex, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.SRV);

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = TemporaryIrradianceCacheResources->SurfelIrradiance.UAV;
		OutUAVs[1] = TemporaryIrradianceCacheResources->HeightfieldIrradiance.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		SurfelIrradiance.SetBuffer(RHICmdList, ShaderRHI, TemporaryIrradianceCacheResources->SurfelIrradiance);
		HeightfieldIrradiance.SetBuffer(RHICmdList, ShaderRHI, TemporaryIrradianceCacheResources->HeightfieldIrradiance);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources)
	{
		SurfelIrradiance.UnsetUAV(RHICmdList, GetComputeShader());
		HeightfieldIrradiance.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = TemporaryIrradianceCacheResources->SurfelIrradiance.UAV;
		OutUAVs[1] = TemporaryIrradianceCacheResources->HeightfieldIrradiance.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SurfelIrradiance;
		Ar << HeightfieldIrradiance;
		Ar << ScatterDrawParameters;
		Ar << SavedStartIndex;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter SurfelIrradiance;
	FRWShaderParameter HeightfieldIrradiance;
	FShaderResourceParameter ScatterDrawParameters;
	FShaderResourceParameter SavedStartIndex;
};

IMPLEMENT_SHADER_TYPE(,FClearIrradianceSamplesCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("ClearIrradianceSamplesCS"),SF_Compute);


class FComputeStepBentNormalCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeStepBentNormalCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	FComputeStepBentNormalCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		StepBentNormal.Bind(Initializer.ParameterMap, TEXT("StepBentNormal"));
		IrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheNormal"));
		RecordConeData.Bind(Initializer.ParameterMap, TEXT("RecordConeData"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
	}

	FComputeStepBentNormalCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		int32 DepthLevel, 
		FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheNormal, SurfaceCacheResources.Level[DepthLevel]->Normal.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ScatterDrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, SavedStartIndex, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, RecordConeData, TemporaryIrradianceCacheResources->ConeData.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TemporaryIrradianceCacheResources->StepBentNormal.UAV);
		StepBentNormal.SetBuffer(RHICmdList, ShaderRHI, TemporaryIrradianceCacheResources->StepBentNormal);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		FVector UnoccludedVector(0);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			UnoccludedVector += SampleDirections[SampleIndex];
		}

		float BentNormalNormalizeFactorValue = 1.0f / (UnoccludedVector / NumConeSampleDirections).Size();
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalNormalizeFactor, BentNormalNormalizeFactorValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources)
	{
		StepBentNormal.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, TemporaryIrradianceCacheResources->StepBentNormal.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StepBentNormal;
		Ar << IrradianceCacheNormal;
		Ar << RecordConeData;
		Ar << ScatterDrawParameters;
		Ar << SavedStartIndex;
		Ar << BentNormalNormalizeFactor;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter StepBentNormal;
	FShaderResourceParameter IrradianceCacheNormal;
	FShaderResourceParameter RecordConeData;
	FShaderResourceParameter ScatterDrawParameters;
	FShaderResourceParameter SavedStartIndex;
	FShaderParameter BentNormalNormalizeFactor;
};

IMPLEMENT_SHADER_TYPE(,FComputeStepBentNormalCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("ComputeStepBentNormalCS"),SF_Compute);


enum EVPLMode
{
	VPLMode_PlaceFromLight,
	VPLMode_PlaceFromSurfels
};

template<EVPLMode VPLMode>
class TComputeIrradianceCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TComputeIrradianceCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("IRRADIANCE_FROM_SURFELS"), VPLMode == VPLMode_PlaceFromSurfels);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	TComputeIrradianceCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		SurfelIrradiance.Bind(Initializer.ParameterMap, TEXT("SurfelIrradiance"));
		IrradianceCachePositionRadius.Bind(Initializer.ParameterMap, TEXT("IrradianceCachePositionRadius"));
		IrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheNormal"));
		IrradianceCacheTileCoordinate.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheTileCoordinate"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
		GlobalObjectParameters.Bind(Initializer.ParameterMap);
		ObjectParameters.Bind(Initializer.ParameterMap);
		SurfelParameters.Bind(Initializer.ParameterMap);
		VPLParameterBuffer.Bind(Initializer.ParameterMap, TEXT("VPLParameterBuffer"));
		VPLData.Bind(Initializer.ParameterMap, TEXT("VPLData"));
		VPLGatherRadius.Bind(Initializer.ParameterMap, TEXT("VPLGatherRadius"));
		TileListGroupSize.Bind(Initializer.ParameterMap, TEXT("TileListGroupSize"));
		TileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("TileHeadDataUnpacked"));
		TileArrayData.Bind(Initializer.ParameterMap, TEXT("TileArrayData"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		StepBentNormalBuffer.Bind(Initializer.ParameterMap, TEXT("StepBentNormalBuffer"));
	}

	TComputeIrradianceCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		int32 DepthLevel, 
		const FDistanceFieldAOParameters& Parameters,
		FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCachePositionRadius, SurfaceCacheResources.Level[DepthLevel]->PositionAndRadius.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheNormal, SurfaceCacheResources.Level[DepthLevel]->Normal.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheTileCoordinate, SurfaceCacheResources.Level[DepthLevel]->TileCoordinate.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ScatterDrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, SavedStartIndex, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TemporaryIrradianceCacheResources->SurfelIrradiance.UAV);
		SurfelIrradiance.SetBuffer(RHICmdList, ShaderRHI, TemporaryIrradianceCacheResources->SurfelIrradiance);

		GlobalObjectParameters.Set(RHICmdList, ShaderRHI, *Scene->DistanceFieldSceneData.ObjectBuffers, Scene->DistanceFieldSceneData.NumObjectsInBuffer);
		extern TGlobalResource<FDistanceFieldObjectBufferResource> GAOCulledObjectBuffers;
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		SurfelParameters.Set(RHICmdList, ShaderRHI, *Scene->DistanceFieldSceneData.SurfelBuffers, *Scene->DistanceFieldSceneData.InstancedSurfelBuffers);

		FVPLResources& SourceVPLResources = GVPLViewCulling ? GCulledVPLResources : GVPLResources;
		SetSRVParameter(RHICmdList, ShaderRHI, VPLParameterBuffer, SourceVPLResources.VPLParameterBuffer.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, VPLData, SourceVPLResources.VPLData.SRV);

		SetShaderValue(RHICmdList, ShaderRHI, VPLGatherRadius, Parameters.ObjectMaxOcclusionDistance);

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		SetSRVParameter(RHICmdList, ShaderRHI, TileHeadDataUnpacked, TileIntersectionResources->TileHeadDataUnpacked.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileArrayData, TileIntersectionResources->TileArrayData.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);
		SetShaderValue(RHICmdList, ShaderRHI, TileListGroupSize, TileIntersectionResources->TileDimensions);

		SetSRVParameter(RHICmdList, ShaderRHI, StepBentNormalBuffer, TemporaryIrradianceCacheResources->StepBentNormal.SRV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources)
	{
		SurfelIrradiance.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, TemporaryIrradianceCacheResources->SurfelIrradiance.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << SurfelIrradiance;
		Ar << IrradianceCachePositionRadius;
		Ar << IrradianceCacheNormal;
		Ar << IrradianceCacheTileCoordinate;
		Ar << ScatterDrawParameters;
		Ar << SavedStartIndex;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		Ar << GlobalObjectParameters;
		Ar << ObjectParameters;
		Ar << SurfelParameters;
		Ar << VPLParameterBuffer;
		Ar << VPLData;
		Ar << VPLGatherRadius;
		Ar << TileListGroupSize;
		Ar << TileHeadDataUnpacked;
		Ar << TileArrayData;
		Ar << TileConeDepthRanges;
		Ar << StepBentNormalBuffer;
		return bShaderHasOutdatedParameters;
	}

private:

	FAOParameters AOParameters;
	FRWShaderParameter SurfelIrradiance;
	FShaderResourceParameter IrradianceCachePositionRadius;
	FShaderResourceParameter IrradianceCacheNormal;
	FShaderResourceParameter IrradianceCacheTileCoordinate;
	FShaderResourceParameter ScatterDrawParameters;
	FShaderResourceParameter SavedStartIndex;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
	FDistanceFieldObjectBufferParameters GlobalObjectParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FSurfelBufferParameters SurfelParameters;
	FShaderResourceParameter VPLParameterBuffer;
	FShaderResourceParameter VPLData;
	FShaderParameter VPLGatherRadius;
	FShaderParameter TileListGroupSize;
	FShaderResourceParameter TileHeadDataUnpacked;
	FShaderResourceParameter TileArrayData;
	FShaderResourceParameter TileConeDepthRanges;
	FShaderResourceParameter StepBentNormalBuffer;
};

IMPLEMENT_SHADER_TYPE(template<>,TComputeIrradianceCS<VPLMode_PlaceFromLight>,TEXT("DistanceFieldGlobalIllumination"),TEXT("ComputeIrradianceCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TComputeIrradianceCS<VPLMode_PlaceFromSurfels>,TEXT("DistanceFieldGlobalIllumination"),TEXT("ComputeIrradianceCS"),SF_Compute);


class FCombineIrradianceSamplesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCombineIrradianceSamplesCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FCombineIrradianceSamplesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IrradianceCacheIrradiance.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheIrradiance"));
		SurfelIrradiance.Bind(Initializer.ParameterMap, TEXT("SurfelIrradiance"));
		HeightfieldIrradiance.Bind(Initializer.ParameterMap, TEXT("HeightfieldIrradiance"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
	}

	FCombineIrradianceSamplesCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		int32 DepthLevel, 
		FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, SurfelIrradiance, TemporaryIrradianceCacheResources->SurfelIrradiance.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, HeightfieldIrradiance,TemporaryIrradianceCacheResources->HeightfieldIrradiance.SRV);

		SetSRVParameter(RHICmdList, ShaderRHI, ScatterDrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, SavedStartIndex, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, SurfaceCacheResources.Level[DepthLevel]->Irradiance.UAV);
		IrradianceCacheIrradiance.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->Irradiance);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		IrradianceCacheIrradiance.UnsetUAV(RHICmdList, GetComputeShader());

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, SurfaceCacheResources.Level[DepthLevel]->Irradiance.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IrradianceCacheIrradiance;
		Ar << SurfelIrradiance;
		Ar << HeightfieldIrradiance;
		Ar << ScatterDrawParameters;
		Ar << SavedStartIndex;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter IrradianceCacheIrradiance;
	FShaderResourceParameter SurfelIrradiance;
	FShaderResourceParameter HeightfieldIrradiance;
	FShaderResourceParameter ScatterDrawParameters;
	FShaderResourceParameter SavedStartIndex;
};

IMPLEMENT_SHADER_TYPE(,FCombineIrradianceSamplesCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("CombineIrradianceSamplesCS"),SF_Compute);


void ComputeIrradianceForSamples(
	int32 DepthLevel,
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FScene* Scene,
	const FDistanceFieldAOParameters& Parameters,
	FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources)
{
	FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

	{
		SCOPED_DRAW_EVENT(RHICmdList, ComputeStepBentNormal);

		{	
			TShaderMapRef<TSetupFinalGatherIndirectArgumentsCS<false> > ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
			DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
			ComputeShader->UnsetParameters(RHICmdList, View);
		}

		{
			TShaderMapRef<FComputeStepBentNormalCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DepthLevel, TemporaryIrradianceCacheResources);
			DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);
			ComputeShader->UnsetParameters(RHICmdList, TemporaryIrradianceCacheResources);
		}
		
		{
			TShaderMapRef<FClearIrradianceSamplesCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DepthLevel, TemporaryIrradianceCacheResources);
			DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);
			ComputeShader->UnsetParameters(RHICmdList, TemporaryIrradianceCacheResources);
		}
	}

	View.HeightfieldLightingViewInfo.ComputeIrradianceForSamples(View, RHICmdList, *TemporaryIrradianceCacheResources, DepthLevel, Parameters);

	if (GVPLMeshGlobalIllumination)
	{
		SCOPED_DRAW_EVENT(RHICmdList, MeshIrradiance);

		{	
			TShaderMapRef<TSetupFinalGatherIndirectArgumentsCS<true> > ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
			DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
			ComputeShader->UnsetParameters(RHICmdList, View);
		}
	
		if (GVPLSurfelRepresentation)
		{
			TShaderMapRef<TComputeIrradianceCS<VPLMode_PlaceFromSurfels> > ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DepthLevel, Parameters, TemporaryIrradianceCacheResources);
			DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

			ComputeShader->UnsetParameters(RHICmdList, TemporaryIrradianceCacheResources);
		}
		else
		{
			TShaderMapRef<TComputeIrradianceCS<VPLMode_PlaceFromLight> > ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DepthLevel, Parameters, TemporaryIrradianceCacheResources);
			DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

			ComputeShader->UnsetParameters(RHICmdList, TemporaryIrradianceCacheResources);
		}
	}

	{	
		TShaderMapRef<TSetupFinalGatherIndirectArgumentsCS<false> > ComputeShader(View.ShaderMap);
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
		ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
		DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
		ComputeShader->UnsetParameters(RHICmdList, View);
	}
		
	{
		TShaderMapRef<FCombineIrradianceSamplesCS> ComputeShader(View.ShaderMap);
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
		ComputeShader->SetParameters(RHICmdList, View, DepthLevel, TemporaryIrradianceCacheResources);
		DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);
		ComputeShader->UnsetParameters(RHICmdList, View, DepthLevel);
	}
}


const int32 GScreenGridIrradianceThreadGroupSizeX = 8;

class FComputeStepBentNormalScreenGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeStepBentNormalScreenGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SCREEN_GRID_IRRADIANCE_THREADGROUP_SIZE_X"), GScreenGridIrradianceThreadGroupSizeX);
		extern int32 GConeTraceDownsampleFactor;
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FComputeStepBentNormalScreenGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		ConeDepthVisibilityFunction.Bind(Initializer.ParameterMap, TEXT("ConeDepthVisibilityFunction"));
		StepBentNormal.Bind(Initializer.ParameterMap, TEXT("StepBentNormal"));
	}

	FComputeStepBentNormalScreenGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FAOScreenGridResources& ScreenGridResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		FVector UnoccludedVector(0);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			UnoccludedVector += SampleDirections[SampleIndex];
		}

		float BentNormalNormalizeFactorValue = 1.0f / (UnoccludedVector / NumConeSampleDirections).Size();
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalNormalizeFactor, BentNormalNormalizeFactorValue);

		SetSRVParameter(RHICmdList, ShaderRHI, ConeDepthVisibilityFunction, ScreenGridResources.ConeDepthVisibilityFunction.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources.StepBentNormal.UAV);
		StepBentNormal.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources.StepBentNormal);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FAOScreenGridResources& ScreenGridResources)
	{
		StepBentNormal.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources.StepBentNormal.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ScreenGridParameters;
		Ar << BentNormalNormalizeFactor;
		Ar << ConeDepthVisibilityFunction;
		Ar << StepBentNormal;
		return bShaderHasOutdatedParameters;
	}

private:

	FScreenGridParameters ScreenGridParameters;
	FShaderParameter BentNormalNormalizeFactor;
	FShaderResourceParameter ConeDepthVisibilityFunction;
	FRWShaderParameter StepBentNormal;
};

IMPLEMENT_SHADER_TYPE(,FComputeStepBentNormalScreenGridCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("ComputeStepBentNormalScreenGridCS"),SF_Compute);


class FComputeIrradianceScreenGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeIrradianceScreenGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CULLED_TILE_SIZEX"), GDistanceFieldAOTileSizeX);
		extern int32 GConeTraceDownsampleFactor;
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GConeTraceDownsampleFactor);
		OutEnvironment.SetDefine(TEXT("IRRADIANCE_FROM_SURFELS"), TEXT("1"));

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FComputeIrradianceScreenGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		SurfelParameters.Bind(Initializer.ParameterMap);
		TileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("TileHeadDataUnpacked"));
		TileArrayData.Bind(Initializer.ParameterMap, TEXT("TileArrayData"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		TileListGroupSize.Bind(Initializer.ParameterMap, TEXT("TileListGroupSize"));
		VPLGatherRadius.Bind(Initializer.ParameterMap, TEXT("VPLGatherRadius"));
		StepBentNormalBuffer.Bind(Initializer.ParameterMap, TEXT("StepBentNormalBuffer"));
		SurfelIrradiance.Bind(Initializer.ParameterMap, TEXT("SurfelIrradiance"));
	}

	FComputeIrradianceScreenGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		extern TGlobalResource<FDistanceFieldObjectBufferResource> GAOCulledObjectBuffers;
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		SurfelParameters.Set(RHICmdList, ShaderRHI, *Scene->DistanceFieldSceneData.SurfelBuffers, *Scene->DistanceFieldSceneData.InstancedSurfelBuffers);

		FTileIntersectionResources* TileIntersectionResources = View.ViewState->AOTileIntersectionResources;

		SetSRVParameter(RHICmdList, ShaderRHI, TileHeadDataUnpacked, TileIntersectionResources->TileHeadDataUnpacked.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileArrayData, TileIntersectionResources->TileArrayData.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);
		SetShaderValue(RHICmdList, ShaderRHI, TileListGroupSize, TileIntersectionResources->TileDimensions);

		SetShaderValue(RHICmdList, ShaderRHI, VPLGatherRadius, Parameters.ObjectMaxOcclusionDistance);

		FAOScreenGridResources* ScreenGridResources = View.ViewState->AOScreenGridResources;

		SetSRVParameter(RHICmdList, ShaderRHI, StepBentNormalBuffer, ScreenGridResources->StepBentNormal.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources->SurfelIrradiance.UAV);
		SurfelIrradiance.SetBuffer(RHICmdList, ShaderRHI, ScreenGridResources->SurfelIrradiance);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		SurfelIrradiance.UnsetUAV(RHICmdList, GetComputeShader());

		FAOScreenGridResources* ScreenGridResources = View.ViewState->AOScreenGridResources;
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ScreenGridResources->SurfelIrradiance.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << ScreenGridParameters;
		Ar << SurfelParameters;
		Ar << TileHeadDataUnpacked;
		Ar << TileArrayData;
		Ar << TileConeDepthRanges;
		Ar << TileListGroupSize;
		Ar << VPLGatherRadius;
		Ar << StepBentNormalBuffer;
		Ar << SurfelIrradiance;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FScreenGridParameters ScreenGridParameters;
	FSurfelBufferParameters SurfelParameters;
	FShaderResourceParameter TileHeadDataUnpacked;
	FShaderResourceParameter TileArrayData;
	FShaderResourceParameter TileConeDepthRanges;
	FShaderParameter TileListGroupSize;
	FShaderParameter VPLGatherRadius;
	FShaderResourceParameter StepBentNormalBuffer;
	FRWShaderParameter SurfelIrradiance;
};

IMPLEMENT_SHADER_TYPE(,FComputeIrradianceScreenGridCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("ComputeIrradianceScreenGridCS"),SF_Compute);


class FCombineIrradianceScreenGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCombineIrradianceScreenGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SCREEN_GRID_IRRADIANCE_THREADGROUP_SIZE_X"), GScreenGridIrradianceThreadGroupSizeX);
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FCombineIrradianceScreenGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		SurfelIrradiance.Bind(Initializer.ParameterMap, TEXT("SurfelIrradiance"));
		HeightfieldIrradiance.Bind(Initializer.ParameterMap, TEXT("HeightfieldIrradiance"));
		ScreenGridConeVisibilitySize.Bind(Initializer.ParameterMap, TEXT("ScreenGridConeVisibilitySize"));
	}

	FCombineIrradianceScreenGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FAOScreenGridResources& ScreenGridResources,
		FSceneRenderTargetItem& IrradianceTextureValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, SurfelIrradiance, ScreenGridResources.SurfelIrradiance.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, HeightfieldIrradiance, ScreenGridResources.HeightfieldIrradiance.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, IrradianceTextureValue.UAV);
		IrradianceTexture.SetTexture(RHICmdList, ShaderRHI, IrradianceTextureValue.ShaderResourceTexture, IrradianceTextureValue.UAV);

		SetShaderValue(RHICmdList, ShaderRHI, ScreenGridConeVisibilitySize, ScreenGridResources.ScreenGridDimensions);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& IrradianceTextureValue)
	{
		IrradianceTexture.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, IrradianceTextureValue.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IrradianceTexture;
		Ar << SurfelIrradiance;
		Ar << HeightfieldIrradiance;
		Ar << ScreenGridConeVisibilitySize;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter IrradianceTexture;
	FShaderResourceParameter SurfelIrradiance;
	FShaderResourceParameter HeightfieldIrradiance;
	FShaderParameter ScreenGridConeVisibilitySize;
};

IMPLEMENT_SHADER_TYPE(,FCombineIrradianceScreenGridCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("CombineIrradianceScreenGridCS"),SF_Compute);


void ComputeIrradianceForScreenGrid(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FScene* Scene,
	const FDistanceFieldAOParameters& Parameters,
	FSceneRenderTargetItem& DistanceFieldNormal, 
	const FAOScreenGridResources& ScreenGridResources,
	FSceneRenderTargetItem& IrradianceTexture)
{
	const uint32 GroupSizeX = FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor, GScreenGridIrradianceThreadGroupSizeX);
	const uint32 GroupSizeY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor, GScreenGridIrradianceThreadGroupSizeX);

	uint32 ClearValues[4] = { 0 };
	RHICmdList.ClearUAV(ScreenGridResources.HeightfieldIrradiance.UAV, ClearValues);
	RHICmdList.ClearUAV(ScreenGridResources.SurfelIrradiance.UAV, ClearValues);

	View.HeightfieldLightingViewInfo.
		ComputeIrradianceForScreenGrid(View, RHICmdList, DistanceFieldNormal, ScreenGridResources, Parameters);
	
	if (GVPLMeshGlobalIllumination)
	{
		{
			SCOPED_DRAW_EVENT(RHICmdList, ComputeStepBentNormal);

			TShaderMapRef<FComputeStepBentNormalScreenGridCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal, ScreenGridResources);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
			ComputeShader->UnsetParameters(RHICmdList, ScreenGridResources);
		}

		if (GVPLSurfelRepresentation)
		{
			SCOPED_DRAW_EVENT(RHICmdList, MeshIrradiance);

			TShaderMapRef<FComputeIrradianceScreenGridCS> ComputeShader(View.ShaderMap);
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal, Parameters);

			const uint32 ComputeIrradianceGroupSizeX = View.ViewRect.Size().X / GAODownsampleFactor;
			const uint32 ComputeIrradianceGroupSizeY = View.ViewRect.Size().Y / GAODownsampleFactor;
			DispatchComputeShader(RHICmdList, *ComputeShader, ComputeIrradianceGroupSizeX, ComputeIrradianceGroupSizeY, 1);
			ComputeShader->UnsetParameters(RHICmdList, View);
		}
	}
		
	{
		TShaderMapRef<FCombineIrradianceScreenGridCS> ComputeShader(View.ShaderMap);
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
		ComputeShader->SetParameters(RHICmdList, View, ScreenGridResources, IrradianceTexture);
		DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
		ComputeShader->UnsetParameters(RHICmdList, IrradianceTexture);
	}
}


void ListDistanceFieldGIMemory(const FViewInfo& View)
{
	const FScene* Scene = (const FScene*)View.Family->Scene;

	if (GVPLPlacementTileIntersectionResources)
	{
		UE_LOG(LogTemp, Log, TEXT("   Shadow tile culled objects %.3fMb"), GVPLPlacementTileIntersectionResources->GetSizeBytes() / 1024.0f / 1024.0f);
	}
}
