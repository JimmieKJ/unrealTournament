// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CapsuleShadowRendering.cpp: Functionality for rendering shadows from capsules
=============================================================================*/

#include "CapsuleShadowRendering.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "ShadowRendering.h"
#include "DeferredShadingRenderer.h"
#include "MaterialShaderType.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "DistanceFieldLightingPost.h"
#include "DistanceFieldLightingShared.h"

DECLARE_FLOAT_COUNTER_STAT(TEXT("Capsule Shadows"), Stat_GPU_CapsuleShadows, STATGROUP_GPU);

int32 GCapsuleShadows = 1;
FAutoConsoleVariableRef CVarCapsuleShadows(
	TEXT("r.CapsuleShadows"),
	GCapsuleShadows,
	TEXT("Whether to allow capsule shadowing on skinned components with bCastCapsuleDirectShadow or bCastCapsuleIndirectShadow enabled."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GCapsuleShadowsFullResolution = 0;
FAutoConsoleVariableRef CVarCapsuleShadowsFullResolution(
	TEXT("r.CapsuleShadowsFullResolution"),
	GCapsuleShadowsFullResolution,
	TEXT("Whether to compute capsule shadows at full resolution."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleMaxDirectOcclusionDistance = 400;
FAutoConsoleVariableRef CVarCapsuleMaxDirectOcclusionDistance(
	TEXT("r.CapsuleMaxDirectOcclusionDistance"),
	GCapsuleMaxDirectOcclusionDistance,
	TEXT("Maximum cast distance for direct shadows from capsules.  This has a big impact on performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleMaxIndirectOcclusionDistance = 200;
FAutoConsoleVariableRef CVarCapsuleMaxIndirectOcclusionDistance(
	TEXT("r.CapsuleMaxIndirectOcclusionDistance"),
	GCapsuleMaxIndirectOcclusionDistance,
	TEXT("Maximum cast distance for indirect shadows from capsules.  This has a big impact on performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleShadowFadeAngleFromVertical = PI / 3;
FAutoConsoleVariableRef CVarCapsuleShadowFadeAngleFromVertical(
	TEXT("r.CapsuleShadowFadeAngleFromVertical"),
	GCapsuleShadowFadeAngleFromVertical,
	TEXT("Angle from vertical up to start fading out the indirect shadow, to avoid self shadowing artifacts."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleIndirectConeAngle = PI / 8;
FAutoConsoleVariableRef CVarCapsuleIndirectConeAngle(
	TEXT("r.CapsuleIndirectConeAngle"),
	GCapsuleIndirectConeAngle,
	TEXT("Light source angle used when the indirect shadow direction is derived from precomputed indirect lighting (no stationary skylight present)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleSkyAngleScale = .6f;
FAutoConsoleVariableRef CVarCapsuleSkyAngleScale(
	TEXT("r.CapsuleSkyAngleScale"),
	GCapsuleSkyAngleScale,
	TEXT("Scales the light source angle derived from the precomputed unoccluded sky vector (stationary skylight present)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleMinSkyAngle = 15;
FAutoConsoleVariableRef CVarCapsuleMinSkyAngle(
	TEXT("r.CapsuleMinSkyAngle"),
	GCapsuleMinSkyAngle,
	TEXT("Minimum light source angle derived from the precomputed unoccluded sky vector (stationary skylight present)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GShadowShapeTileSize = 8;

int32 GetCapsuleShadowDownsampleFactor()
{
	return GCapsuleShadowsFullResolution ? 1 : 2;
}

FIntPoint GetBufferSizeForCapsuleShadows()
{
	return FIntPoint::DivideAndRoundDown(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), GetCapsuleShadowDownsampleFactor());
}

enum ECapsuleShadowingType
{
	ShapeShadow_DirectionalLightTiledCulling,
	ShapeShadow_PointLightTiledCulling,
	ShapeShadow_IndirectTiledCulling,
	ShapeShadow_MovableSkylightTiledCulling,
	ShapeShadow_MovableSkylightTiledCullingGatherFromReceiverBentNormal
};

enum EIndirectShadowingPrimitiveTypes
{
	IPT_CapsuleShapes = 1,
	IPT_MeshDistanceFields = 2,
	IPT_CapsuleShapesAndMeshDistanceFields = IPT_CapsuleShapes | IPT_MeshDistanceFields
};

template<ECapsuleShadowingType ShadowingType>
class TCapsuleShadowingBaseCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TCapsuleShadowingBaseCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportCapsuleShadows(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GShadowShapeTileSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GShadowShapeTileSize);
		OutEnvironment.SetDefine(TEXT("POINT_LIGHT"), ShadowingType == ShapeShadow_PointLightTiledCulling);
		uint32 LightSourceMode = 0;

		if (ShadowingType == ShapeShadow_DirectionalLightTiledCulling || ShadowingType == ShapeShadow_PointLightTiledCulling)
		{
			LightSourceMode = 0;
		}
		else if (ShadowingType == ShapeShadow_IndirectTiledCulling || ShadowingType == ShapeShadow_MovableSkylightTiledCulling)
		{
			LightSourceMode = 1;
		}
		else if (ShadowingType == ShapeShadow_MovableSkylightTiledCullingGatherFromReceiverBentNormal)
		{
			LightSourceMode = 2;
		}
		else
		{
			check(0);
		}

		OutEnvironment.SetDefine(TEXT("LIGHT_SOURCE_MODE"), LightSourceMode);
		const bool bApplyToBentNormal = ShadowingType == ShapeShadow_MovableSkylightTiledCulling || ShadowingType == ShapeShadow_MovableSkylightTiledCullingGatherFromReceiverBentNormal;
		OutEnvironment.SetDefine(TEXT("APPLY_TO_BENT_NORMAL"), bApplyToBentNormal);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	/** Default constructor. */
	TCapsuleShadowingBaseCS() {}

	/** Initialization constructor. */
	TCapsuleShadowingBaseCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ShadowFactors.Bind(Initializer.ParameterMap, TEXT("ShadowFactors"));
		TileIntersectionCounts.Bind(Initializer.ParameterMap, TEXT("TileIntersectionCounts"));
		TileDimensions.Bind(Initializer.ParameterMap, TEXT("TileDimensions"));
		BentNormalTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalTexture"));
		ReceiverBentNormalTexture.Bind(Initializer.ParameterMap, TEXT("ReceiverBentNormalTexture"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		LightDirection.Bind(Initializer.ParameterMap, TEXT("LightDirection"));
		LightSourceRadius.Bind(Initializer.ParameterMap, TEXT("LightSourceRadius"));
		RayStartOffsetDepthScale.Bind(Initializer.ParameterMap, TEXT("RayStartOffsetDepthScale"));
		LightPositionAndInvRadius.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		LightAngleAndNormalThreshold.Bind(Initializer.ParameterMap, TEXT("LightAngleAndNormalThreshold"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap, TEXT("ScissorRectMinAndSize"));
		DeferredParameters.Bind(Initializer.ParameterMap);
		DownsampleFactor.Bind(Initializer.ParameterMap, TEXT("DownsampleFactor"));
		NumShadowCapsules.Bind(Initializer.ParameterMap, TEXT("NumShadowCapsules"));
		ShadowCapsuleShapes.Bind(Initializer.ParameterMap, TEXT("ShadowCapsuleShapes"));
		NumMeshDistanceFieldCasters.Bind(Initializer.ParameterMap, TEXT("NumMeshDistanceFieldCasters"));
		MeshDistanceFieldCasterIndices.Bind(Initializer.ParameterMap, TEXT("MeshDistanceFieldCasterIndices"));
		MaxOcclusionDistance.Bind(Initializer.ParameterMap, TEXT("MaxOcclusionDistance"));
		LightDirectionData.Bind(Initializer.ParameterMap, TEXT("LightDirectionData"));
		IndirectCapsuleSelfShadowingIntensity.Bind(Initializer.ParameterMap, TEXT("IndirectCapsuleSelfShadowingIntensity"));
		DistanceFieldObjectParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		FScene* Scene,
		const FSceneView& View, 
		const FLightSceneInfo* LightSceneInfo,
		FSceneRenderTargetItem& OutputTexture, 
		FIntPoint TileDimensionsValue,
		const FRWBuffer* TileIntersectionCountsBuffer,
		FVector2D NumGroupsValue,
		float MaxOcclusionDistanceValue,
		const FIntRect& ScissorRect,
		int32 DownsampleFactorValue,
		int32 NumShadowCapsulesValue,
		FShaderResourceViewRHIParamRef ShadowCapsuleShapesSRV,
		int32 NumMeshDistanceFieldCastersValue,
		FShaderResourceViewRHIParamRef MeshDistanceFieldCasterIndicesSRV,
		FShaderResourceViewRHIParamRef LightDirectionDataSRV,
		FTextureRHIParamRef ReceiverBentNormalTextureValue)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = OutputTexture.UAV;

		if (TileIntersectionCountsBuffer)
		{
			OutUAVs[1] = TileIntersectionCountsBuffer->UAV;
		}
		
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, TileIntersectionCountsBuffer ? ARRAY_COUNT(OutUAVs) : 1);

		if (ShadowingType == ShapeShadow_MovableSkylightTiledCulling)
		{
			check(!ShadowFactors.IsBound());
			BentNormalTexture.SetTexture(RHICmdList, ShaderRHI, OutputTexture.ShaderResourceTexture, OutputTexture.UAV);
		}
		else
		{
			check(!BentNormalTexture.IsBound());
			ShadowFactors.SetTexture(RHICmdList, ShaderRHI, OutputTexture.ShaderResourceTexture, OutputTexture.UAV);
		}
		
		if (TileIntersectionCountsBuffer)
		{
			TileIntersectionCounts.SetBuffer(RHICmdList, ShaderRHI, *TileIntersectionCountsBuffer);
		}
		else
		{
			check(!TileIntersectionCounts.IsBound());
		}

		SetShaderValue(RHICmdList, ShaderRHI, TileDimensions, TileDimensionsValue);

		if (ShadowingType == ShapeShadow_MovableSkylightTiledCulling)
		{
			check(ReceiverBentNormalTextureValue);
			SetTextureParameter(RHICmdList, ShaderRHI, ReceiverBentNormalTexture, ReceiverBentNormalTextureValue);
		}
		else
		{
			check(!ReceiverBentNormalTexture.IsBound());
		}

		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);

		if (LightSceneInfo)
		{
			check(ShadowingType == ShapeShadow_DirectionalLightTiledCulling || ShadowingType == ShapeShadow_PointLightTiledCulling);
			FVector4 LightPositionAndInvRadiusValue;
			FVector4 LightColorAndFalloffExponent;
			FVector NormalizedLightDirection;
			FVector2D SpotAngles;
			float LightSourceRadiusValue;
			float LightSourceLength;
			float LightMinRoughness;

			const FLightSceneProxy& LightProxy = *LightSceneInfo->Proxy;

			LightProxy.GetParameters(LightPositionAndInvRadiusValue, LightColorAndFalloffExponent, NormalizedLightDirection, SpotAngles, LightSourceRadiusValue, LightSourceLength, LightMinRoughness);

			SetShaderValue(RHICmdList, ShaderRHI, LightDirection, NormalizedLightDirection);
			SetShaderValue(RHICmdList, ShaderRHI, LightPositionAndInvRadius, LightPositionAndInvRadiusValue);
			// Default light source radius of 0 gives poor results
			SetShaderValue(RHICmdList, ShaderRHI, LightSourceRadius, LightSourceRadiusValue == 0 ? 20 : FMath::Clamp(LightSourceRadiusValue, .001f, 1.0f / (4 * LightPositionAndInvRadiusValue.W)));

			SetShaderValue(RHICmdList, ShaderRHI, RayStartOffsetDepthScale, LightProxy.GetRayStartOffsetDepthScale());

			const float LightSourceAngle = FMath::Clamp(LightProxy.GetLightSourceAngle() * 5, 1.0f, 30.0f) * PI / 180.0f;
			const FVector LightAngleAndNormalThresholdValue(LightSourceAngle, FMath::Cos(PI / 2 + LightSourceAngle), LightProxy.GetTraceDistance());
			SetShaderValue(RHICmdList, ShaderRHI, LightAngleAndNormalThreshold, LightAngleAndNormalThresholdValue);
		}
		else
		{
			check(ShadowingType == ShapeShadow_IndirectTiledCulling || ShadowingType == ShapeShadow_MovableSkylightTiledCulling || ShadowingType == ShapeShadow_MovableSkylightTiledCullingGatherFromReceiverBentNormal);
			check(!LightDirection.IsBound() && !LightPositionAndInvRadius.IsBound());
		}

		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));
		SetShaderValue(RHICmdList, ShaderRHI, DownsampleFactor, DownsampleFactorValue);

		SetShaderValue(RHICmdList, ShaderRHI, NumShadowCapsules, NumShadowCapsulesValue);
		SetSRVParameter(RHICmdList, ShaderRHI, ShadowCapsuleShapes, ShadowCapsuleShapesSRV);

		SetShaderValue(RHICmdList, ShaderRHI, NumMeshDistanceFieldCasters, NumMeshDistanceFieldCastersValue);
		SetSRVParameter(RHICmdList, ShaderRHI, MeshDistanceFieldCasterIndices, MeshDistanceFieldCasterIndicesSRV);

		SetShaderValue(RHICmdList, ShaderRHI, MaxOcclusionDistance, MaxOcclusionDistanceValue);
		SetSRVParameter(RHICmdList, ShaderRHI, LightDirectionData, LightDirectionDataSRV);

		float IndirectCapsuleSelfShadowingIntensityValue = Scene->DynamicIndirectShadowsSelfShadowingIntensity;
		SetShaderValue(RHICmdList, ShaderRHI, IndirectCapsuleSelfShadowingIntensity, IndirectCapsuleSelfShadowingIntensityValue);

		if (Scene->DistanceFieldSceneData.ObjectBuffers)
		{
			DistanceFieldObjectParameters.Set(RHICmdList, ShaderRHI, *Scene->DistanceFieldSceneData.ObjectBuffers, Scene->DistanceFieldSceneData.NumObjectsInBuffer);
		}
		else
		{
			check(!DistanceFieldObjectParameters.AnyBound());
		}
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& OutputTexture, const FRWBuffer* TileIntersectionCountsBuffer)
	{
		ShadowFactors.UnsetUAV(RHICmdList, GetComputeShader());
		BentNormalTexture.UnsetUAV(RHICmdList, GetComputeShader());
		TileIntersectionCounts.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = OutputTexture.UAV;

		if (TileIntersectionCountsBuffer)
		{
			OutUAVs[1] = TileIntersectionCountsBuffer->UAV;
		}
		
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, TileIntersectionCountsBuffer ? ARRAY_COUNT(OutUAVs) : 1);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ShadowFactors;
		Ar << TileIntersectionCounts;
		Ar << TileDimensions;
		Ar << BentNormalTexture;
		Ar << ReceiverBentNormalTexture;
		Ar << NumGroups;
		Ar << LightDirection;
		Ar << LightPositionAndInvRadius;
		Ar << LightSourceRadius;
		Ar << RayStartOffsetDepthScale;
		Ar << LightAngleAndNormalThreshold;
		Ar << ScissorRectMinAndSize;
		Ar << DeferredParameters;
		Ar << DownsampleFactor;
		Ar << NumShadowCapsules;
		Ar << ShadowCapsuleShapes;
		Ar << NumMeshDistanceFieldCasters;
		Ar << MeshDistanceFieldCasterIndices;
		Ar << MaxOcclusionDistance;
		Ar << LightDirectionData;
		Ar << IndirectCapsuleSelfShadowingIntensity;
		Ar << DistanceFieldObjectParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter ShadowFactors;
	FRWShaderParameter TileIntersectionCounts;
	FShaderParameter TileDimensions;
	FRWShaderParameter BentNormalTexture;
	FShaderResourceParameter ReceiverBentNormalTexture;
	FShaderParameter NumGroups;
	FShaderParameter LightDirection;
	FShaderParameter LightPositionAndInvRadius;
	FShaderParameter LightSourceRadius;
	FShaderParameter RayStartOffsetDepthScale;
	FShaderParameter LightAngleAndNormalThreshold;
	FShaderParameter ScissorRectMinAndSize;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DownsampleFactor;
	FShaderParameter NumShadowCapsules;
	FShaderResourceParameter ShadowCapsuleShapes;
	FShaderParameter NumMeshDistanceFieldCasters;
	FShaderResourceParameter MeshDistanceFieldCasterIndices;
	FShaderParameter MaxOcclusionDistance;
	FShaderResourceParameter LightDirectionData;
	FShaderParameter IndirectCapsuleSelfShadowingIntensity;
	FDistanceFieldObjectBufferParameters DistanceFieldObjectParameters;
};

template<ECapsuleShadowingType ShadowingType, EIndirectShadowingPrimitiveTypes PrimitiveTypes>
class TCapsuleShadowingCS : public TCapsuleShadowingBaseCS<ShadowingType>
{
	DECLARE_SHADER_TYPE(TCapsuleShadowingCS, Global);

	TCapsuleShadowingCS() {}

	/** Initialization constructor. */
	TCapsuleShadowingCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: TCapsuleShadowingBaseCS<ShadowingType>(Initializer)
	{}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TCapsuleShadowingBaseCS<ShadowingType>::ModifyCompilationEnvironment(Platform, OutEnvironment);

		if (PrimitiveTypes & IPT_CapsuleShapes)
		{
			OutEnvironment.SetDefine(TEXT("SUPPORT_CAPSULE_SHAPES"), 1);
		}
		
		if (PrimitiveTypes & IPT_MeshDistanceFields)
		{
			OutEnvironment.SetDefine(TEXT("SUPPORT_MESH_DISTANCE_FIELDS"), 1);
		}
	}
};

#define IMPLEMENT_CAPSULE_SHADOW_TYPE(ShadowType,PrimitiveType) \
	typedef TCapsuleShadowingCS<ShadowType,PrimitiveType> TCapsuleShadowingCS##ShadowType##PrimitiveType; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TCapsuleShadowingCS##ShadowType##PrimitiveType,TEXT("CapsuleShadowShaders"),TEXT("CapsuleShadowingCS"),SF_Compute);

IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_DirectionalLightTiledCulling, IPT_CapsuleShapes);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_PointLightTiledCulling, IPT_CapsuleShapes);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_IndirectTiledCulling, IPT_CapsuleShapes);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_MovableSkylightTiledCulling, IPT_CapsuleShapes);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_MovableSkylightTiledCullingGatherFromReceiverBentNormal, IPT_CapsuleShapes);

IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_DirectionalLightTiledCulling, IPT_MeshDistanceFields);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_PointLightTiledCulling, IPT_MeshDistanceFields);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_IndirectTiledCulling, IPT_MeshDistanceFields);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_MovableSkylightTiledCulling, IPT_MeshDistanceFields);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_MovableSkylightTiledCullingGatherFromReceiverBentNormal, IPT_MeshDistanceFields);

IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_DirectionalLightTiledCulling, IPT_CapsuleShapesAndMeshDistanceFields);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_PointLightTiledCulling, IPT_CapsuleShapesAndMeshDistanceFields);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_IndirectTiledCulling, IPT_CapsuleShapesAndMeshDistanceFields);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_MovableSkylightTiledCulling, IPT_CapsuleShapesAndMeshDistanceFields);
IMPLEMENT_CAPSULE_SHADOW_TYPE(ShapeShadow_MovableSkylightTiledCullingGatherFromReceiverBentNormal, IPT_CapsuleShapesAndMeshDistanceFields);

// Nvidia has lower vertex throughput when only processing a few verts per instance
// Disabled as it hasn't been tested
const int32 NumTileQuadsInBuffer = 1;

class FCapsuleShadowingUpsampleVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCapsuleShadowingUpsampleVS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportCapsuleShadows(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("TILES_PER_INSTANCE"), NumTileQuadsInBuffer);
	}

	/** Default constructor. */
	FCapsuleShadowingUpsampleVS() {}

	/** Initialization constructor. */
	FCapsuleShadowingUpsampleVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TileDimensions.Bind(Initializer.ParameterMap,TEXT("TileDimensions"));
		TileSize.Bind(Initializer.ParameterMap,TEXT("TileSize"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap,TEXT("ScissorRectMinAndSize"));
		TileIntersectionCounts.Bind(Initializer.ParameterMap,TEXT("TileIntersectionCounts"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FIntPoint TileDimensionsValue, const FIntRect& ScissorRect, const FRWBuffer& TileIntersectionCountsBuffer)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, TileDimensions, TileDimensionsValue);
		SetShaderValue(RHICmdList, ShaderRHI, TileSize, FVector2D(
			GShadowShapeTileSize * GetCapsuleShadowDownsampleFactor(), 
			GShadowShapeTileSize * GetCapsuleShadowDownsampleFactor()));
		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));
		SetSRVParameter(RHICmdList, ShaderRHI, TileIntersectionCounts, TileIntersectionCountsBuffer.SRV);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TileDimensions;
		Ar << TileSize;
		Ar << ScissorRectMinAndSize;
		Ar << TileIntersectionCounts;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter TileDimensions;
	FShaderParameter TileSize;
	FShaderParameter ScissorRectMinAndSize;
	FShaderResourceParameter TileIntersectionCounts;
};

IMPLEMENT_SHADER_TYPE(,FCapsuleShadowingUpsampleVS,TEXT("CapsuleShadowShaders"),TEXT("CapsuleShadowingUpsampleVS"),SF_Vertex);


template<bool bUpsampleRequired, bool bApplyToSSAO>
class TCapsuleShadowingUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TCapsuleShadowingUpsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportCapsuleShadows(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), 2);
		OutEnvironment.SetDefine(TEXT("UPSAMPLE_REQUIRED"), bUpsampleRequired);
		OutEnvironment.SetDefine(TEXT("APPLY_TO_SSAO"), bApplyToSSAO);
	}

	/** Default constructor. */
	TCapsuleShadowingUpsamplePS() {}

	/** Initialization constructor. */
	TCapsuleShadowingUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ShadowFactorsTexture.Bind(Initializer.ParameterMap,TEXT("ShadowFactorsTexture"));
		ShadowFactorsSampler.Bind(Initializer.ParameterMap,TEXT("ShadowFactorsSampler"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap,TEXT("ScissorRectMinAndSize"));
		OutputtingToLightAttenuation.Bind(Initializer.ParameterMap,TEXT("OutputtingToLightAttenuation"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FIntRect& ScissorRect, TRefCountPtr<IPooledRenderTarget>& ShadowFactorsTextureValue, bool bOutputtingToLightAttenuation)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, ShadowFactorsTexture, ShadowFactorsSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), ShadowFactorsTextureValue->GetRenderTargetItem().ShaderResourceTexture);
	
		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));
		SetShaderValue(RHICmdList, ShaderRHI, OutputtingToLightAttenuation, bOutputtingToLightAttenuation ? 1.0f : 0.0f);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ShadowFactorsTexture;
		Ar << ShadowFactorsSampler;
		Ar << ScissorRectMinAndSize;
		Ar << OutputtingToLightAttenuation;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter ShadowFactorsTexture;
	FShaderResourceParameter ShadowFactorsSampler;
	FShaderParameter ScissorRectMinAndSize;
	FShaderParameter OutputtingToLightAttenuation;
};

#define IMPLEMENT_CAPSULE_APPLY_SHADER_TYPE(bUpsampleRequired,bApplyToSSAO) \
	typedef TCapsuleShadowingUpsamplePS<bUpsampleRequired,bApplyToSSAO> TCapsuleShadowingUpsamplePS##bUpsampleRequired##bApplyToSSAO; \
	IMPLEMENT_SHADER_TYPE(template<>,TCapsuleShadowingUpsamplePS##bUpsampleRequired##bApplyToSSAO,TEXT("CapsuleShadowShaders"),TEXT("CapsuleShadowingUpsamplePS"),SF_Pixel)

IMPLEMENT_CAPSULE_APPLY_SHADER_TYPE(true, true);
IMPLEMENT_CAPSULE_APPLY_SHADER_TYPE(true, false);
IMPLEMENT_CAPSULE_APPLY_SHADER_TYPE(false, true);
IMPLEMENT_CAPSULE_APPLY_SHADER_TYPE(false, false);

class FTileTexCoordVertexBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		const uint32 Size = sizeof(FVector2D) * 4 * NumTileQuadsInBuffer;
		FRHIResourceCreateInfo CreateInfo;
		void* BufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, BufferData);
		FVector2D* Vertices = (FVector2D*)BufferData;
		for (uint32 SpriteIndex = 0; SpriteIndex < NumTileQuadsInBuffer; ++SpriteIndex)
		{
			Vertices[SpriteIndex*4 + 0] = FVector2D(0.0f, 0.0f);
			Vertices[SpriteIndex*4 + 1] = FVector2D(0.0f, 1.0f);
			Vertices[SpriteIndex*4 + 2] = FVector2D(1.0f, 1.0f);
			Vertices[SpriteIndex*4 + 3] = FVector2D(1.0f, 0.0f);
		}
		RHIUnlockVertexBuffer( VertexBufferRHI );
	}
};

TGlobalResource<FTileTexCoordVertexBuffer> GTileTexCoordVertexBuffer;

class FTileIndexBuffer : public FIndexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() override
	{
		const uint32 Size = sizeof(uint16) * 6 * NumTileQuadsInBuffer;
		const uint32 Stride = sizeof(uint16);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(Stride, Size, BUF_Static, CreateInfo, Buffer);
		uint16* Indices = (uint16*)Buffer;
		for (uint32 SpriteIndex = 0; SpriteIndex < NumTileQuadsInBuffer; ++SpriteIndex)
		{
			Indices[SpriteIndex*6 + 0] = SpriteIndex*4 + 0;
			Indices[SpriteIndex*6 + 1] = SpriteIndex*4 + 1;
			Indices[SpriteIndex*6 + 2] = SpriteIndex*4 + 2;
			Indices[SpriteIndex*6 + 3] = SpriteIndex*4 + 0;
			Indices[SpriteIndex*6 + 4] = SpriteIndex*4 + 2;
			Indices[SpriteIndex*6 + 5] = SpriteIndex*4 + 3;
		}
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};

TGlobalResource<FTileIndexBuffer> GTileIndexBuffer;

class FTileVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FTileVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FVector2D);
		Elements.Add(FVertexElement(0,0,VET_Float2,0,Stride,false));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FTileVertexDeclaration> GTileVertexDeclaration;

void AllocateCapsuleTileIntersectionCountsBuffer(FIntPoint GroupSize, FSceneViewState* ViewState)
{
	EPixelFormat CapsuleTileIntersectionCountsBufferFormat = PF_R32_UINT;

	if (!IsValidRef(ViewState->CapsuleTileIntersectionCountsBuffer.Buffer) 
		|| (int32)ViewState->CapsuleTileIntersectionCountsBuffer.NumBytes < GroupSize.X * GroupSize.Y * GPixelFormats[CapsuleTileIntersectionCountsBufferFormat].BlockBytes)
	{
		ViewState->CapsuleTileIntersectionCountsBuffer.Release();
		ViewState->CapsuleTileIntersectionCountsBuffer.Initialize(GPixelFormats[CapsuleTileIntersectionCountsBufferFormat].BlockBytes, GroupSize.X * GroupSize.Y, CapsuleTileIntersectionCountsBufferFormat);
	}
}

bool FDeferredShadingSceneRenderer::RenderCapsuleDirectShadows(
	const FLightSceneInfo& LightSceneInfo,
	FRHICommandListImmediate& RHICmdList, 
	const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& CapsuleShadows, 
	bool bProjectingForForwardShading) const
{
	bool bAllViewsHaveViewState = true;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		if (!View.ViewState)
		{
			bAllViewsHaveViewState = false;
		}
	}

	if (SupportsCapsuleShadows(FeatureLevel, GShaderPlatformForFeatureLevel[FeatureLevel])
		&& CapsuleShadows.Num() > 0
		&& bAllViewsHaveViewState)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderCapsuleShadows);

		FSceneRenderTargets::Get(RHICmdList).FinishRenderingLightAttenuation(RHICmdList);
		TRefCountPtr<IPooledRenderTarget> RayTracedShadowsRT;

		{
			const FIntPoint BufferSize = GetBufferSizeForCapsuleShadows();
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_G16R16F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RayTracedShadowsRT, TEXT("RayTracedShadows"));
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			SCOPED_DRAW_EVENT(RHICmdList, CapsuleShadows);
			SCOPED_GPU_STAT(RHICmdList, Stat_GPU_CapsuleShadows);

			static TArray<FCapsuleShape> CapsuleShapeData;
			CapsuleShapeData.Reset();

			for (int32 ShadowIndex = 0; ShadowIndex < CapsuleShadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* Shadow = CapsuleShadows[ShadowIndex];

				int32 OriginalCapsuleIndex = CapsuleShapeData.Num();

				TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowGroupPrimitives;
				Shadow->GetParentSceneInfo()->GatherLightingAttachmentGroupPrimitives(ShadowGroupPrimitives);

				for (int32 ChildIndex = 0; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
				{
					const FPrimitiveSceneInfo* PrimitiveSceneInfo = ShadowGroupPrimitives[ChildIndex];

					if (PrimitiveSceneInfo->Proxy->CastsDynamicShadow())
					{
						PrimitiveSceneInfo->Proxy->GetShadowShapes(CapsuleShapeData);
					}
				}

				const float FadeRadiusScale = Shadow->FadeAlphas[ViewIndex];

				for (int32 ShapeIndex = OriginalCapsuleIndex; ShapeIndex < CapsuleShapeData.Num(); ShapeIndex++)
				{
					CapsuleShapeData[ShapeIndex].Radius *= FadeRadiusScale;
				}
			}

			if (CapsuleShapeData.Num() > 0)
			{
				static_assert(sizeof(FCapsuleShape) == sizeof(FVector4) * 2, "FCapsuleShape has padding");
				const int32 DataSize = CapsuleShapeData.Num() * CapsuleShapeData.GetTypeSize();

				if (!IsValidRef(LightSceneInfo.ShadowCapsuleShapesVertexBuffer) || (int32)LightSceneInfo.ShadowCapsuleShapesVertexBuffer->GetSize() < DataSize)
				{
					LightSceneInfo.ShadowCapsuleShapesVertexBuffer.SafeRelease();
					LightSceneInfo.ShadowCapsuleShapesSRV.SafeRelease();
					FRHIResourceCreateInfo CreateInfo;
					LightSceneInfo.ShadowCapsuleShapesVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
					LightSceneInfo.ShadowCapsuleShapesSRV = RHICreateShaderResourceView(LightSceneInfo.ShadowCapsuleShapesVertexBuffer, sizeof(FVector4), PF_A32B32G32R32F);
				}

				void* CapsuleShapeLockedData = RHILockVertexBuffer(LightSceneInfo.ShadowCapsuleShapesVertexBuffer, 0, DataSize, RLM_WriteOnly);
				FPlatformMemory::Memcpy(CapsuleShapeLockedData, CapsuleShapeData.GetData(), DataSize);
				RHIUnlockVertexBuffer(LightSceneInfo.ShadowCapsuleShapesVertexBuffer);

				SetRenderTarget(RHICmdList, NULL, NULL);

				const bool bDirectionalLight = LightSceneInfo.Proxy->GetLightType() == LightType_Directional;
				FIntRect ScissorRect;

				if (!LightSceneInfo.Proxy->GetScissorRect(ScissorRect, View))
				{
					ScissorRect = View.ViewRect;
				}

				const FIntPoint GroupSize(
					FMath::DivideAndRoundUp(ScissorRect.Size().X / GetCapsuleShadowDownsampleFactor(), GShadowShapeTileSize),
					FMath::DivideAndRoundUp(ScissorRect.Size().Y / GetCapsuleShadowDownsampleFactor(), GShadowShapeTileSize));
				
				AllocateCapsuleTileIntersectionCountsBuffer(GroupSize, View.ViewState);

				uint32 ClearValues[4] = { 0 };
				RHICmdList.ClearUAV(View.ViewState->CapsuleTileIntersectionCountsBuffer.UAV, ClearValues);

				{
					SCOPED_DRAW_EVENT(RHICmdList, TiledCapsuleShadowing);

					FSceneRenderTargetItem& RayTracedShadowsRTI = RayTracedShadowsRT->GetRenderTargetItem();
					if (bDirectionalLight)
					{
						TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_DirectionalLightTiledCulling, IPT_CapsuleShapes> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

						ComputeShader->SetParameters(
							RHICmdList, 
							Scene,
							View, 
							&LightSceneInfo, 
							RayTracedShadowsRTI, 
							GroupSize,
							&View.ViewState->CapsuleTileIntersectionCountsBuffer,
							FVector2D(GroupSize.X, GroupSize.Y), 
							GCapsuleMaxDirectOcclusionDistance, 
							ScissorRect, 
							GetCapsuleShadowDownsampleFactor(),
							CapsuleShapeData.Num(), 
							LightSceneInfo.ShadowCapsuleShapesSRV.GetReference(),
							0,
							NULL,
							NULL,
							NULL);

						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSize.X, GroupSize.Y, 1);
						ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI, &View.ViewState->CapsuleTileIntersectionCountsBuffer);
					}
					else
					{
						TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_PointLightTiledCulling, IPT_CapsuleShapes> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

						ComputeShader->SetParameters(
							RHICmdList, 
							Scene,
							View, 
							&LightSceneInfo, 
							RayTracedShadowsRTI, 
							GroupSize,
							&View.ViewState->CapsuleTileIntersectionCountsBuffer,
							FVector2D(GroupSize.X, GroupSize.Y), 
							GCapsuleMaxDirectOcclusionDistance, 
							ScissorRect, 
							GetCapsuleShadowDownsampleFactor(),
							CapsuleShapeData.Num(), 
							LightSceneInfo.ShadowCapsuleShapesSRV.GetReference(),
							0,
							NULL,
							NULL,
							NULL);

						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSize.X, GroupSize.Y, 1);
						ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI, &View.ViewState->CapsuleTileIntersectionCountsBuffer);
					}
				}

				{
					SCOPED_DRAW_EVENTF(RHICmdList, Upsample, TEXT("Upsample %dx%d"),
						ScissorRect.Width(), ScissorRect.Height());
						
					FSceneRenderTargets::Get(RHICmdList).BeginRenderingLightAttenuation(RHICmdList);

					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
					RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				
					FProjectedShadowInfo::SetBlendStateForProjection(
						RHICmdList,
						LightSceneInfo.Proxy->GetPreviewShadowMapChannel(),
						false,
						false,
						bProjectingForForwardShading,
						false);

					TShaderMapRef<FCapsuleShadowingUpsampleVS> VertexShader(View.ShaderMap);

					if (GCapsuleShadowsFullResolution)
					{
						TShaderMapRef<TCapsuleShadowingUpsamplePS<false, false> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GTileVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

						VertexShader->SetParameters(RHICmdList, View, GroupSize, ScissorRect, View.ViewState->CapsuleTileIntersectionCountsBuffer);
						PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, true);
					}
					else
					{
						TShaderMapRef<TCapsuleShadowingUpsamplePS<true, false> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GTileVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

						VertexShader->SetParameters(RHICmdList, View, GroupSize, ScissorRect, View.ViewState->CapsuleTileIntersectionCountsBuffer);
						PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, true);
					}

					RHICmdList.SetStreamSource(0, GTileTexCoordVertexBuffer.VertexBufferRHI, sizeof(FVector2D), 0);
					RHICmdList.DrawIndexedPrimitive(GTileIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 4, 0, 2 * NumTileQuadsInBuffer, FMath::DivideAndRoundUp(GroupSize.X * GroupSize.Y, NumTileQuadsInBuffer));
				}
			}
		}

		return true;
	}

	return false;
}

void FDeferredShadingSceneRenderer::CreateIndirectCapsuleShadows()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_CreateIndirectCapsuleShadows);

	for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->DynamicIndirectCasterPrimitives.Num(); PrimitiveIndex++)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->DynamicIndirectCasterPrimitives[PrimitiveIndex];
		FPrimitiveSceneProxy* PrimitiveProxy = PrimitiveSceneInfo->Proxy;

		if (PrimitiveProxy->CastsDynamicShadow() && PrimitiveProxy->CastsDynamicIndirectShadow())
		{
			TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowGroupPrimitives;
			PrimitiveSceneInfo->GatherLightingAttachmentGroupPrimitives(ShadowGroupPrimitives);

			// Compute the composite bounds of this group of shadow primitives.
			FBoxSphereBounds LightingGroupBounds = ShadowGroupPrimitives[0]->Proxy->GetBounds();

			for (int32 ChildIndex = 1; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
			{
				const FPrimitiveSceneInfo* ShadowChild = ShadowGroupPrimitives[ChildIndex];

				if (ShadowChild->Proxy->CastsDynamicShadow())
				{
					LightingGroupBounds = LightingGroupBounds + ShadowChild->Proxy->GetBounds();
				}
			}

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				FViewInfo& View = Views[ViewIndex];

				float EffectiveMaxIndirectOcclusionDistance = GCapsuleMaxIndirectOcclusionDistance;

				if (PrimitiveProxy->HasDistanceFieldRepresentation())
				{
					// Increase max occlusion distance based on object size for distance field casters
					// This improves the solidness of the shadows, since the fadeout distance causes internal structure of objects to become visible
					EffectiveMaxIndirectOcclusionDistance += .5f * LightingGroupBounds.SphereRadius;
				}

				if (View.ViewFrustum.IntersectBox(LightingGroupBounds.Origin, LightingGroupBounds.BoxExtent + FVector(EffectiveMaxIndirectOcclusionDistance)))
				{
					View.IndirectShadowPrimitives.Add(PrimitiveSceneInfo);
				}
			}
		}
	}
}

void FDeferredShadingSceneRenderer::SetupIndirectCapsuleShadows(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bPrepareLightData, int32& NumCapsuleShapes, int32& NumMeshDistanceFieldCasters) const
{
	const float CosFadeStartAngle = FMath::Cos(GCapsuleShadowFadeAngleFromVertical);
	const FSkyLightSceneProxy* SkyLight = Scene ? Scene->SkyLight : NULL;

	static TArray<FCapsuleShape> CapsuleShapeData;
	static TArray<FVector4> CapsuleLightSourceData;
	static TArray<int32, TInlineAllocator<1>> MeshDistanceFieldCasterIndices;
	static TArray<FVector4> DistanceFieldCasterLightSourceData;

	CapsuleShapeData.Reset();
	MeshDistanceFieldCasterIndices.Reset();
	CapsuleLightSourceData.Reset();
	DistanceFieldCasterLightSourceData.Reset();

	for (int32 PrimitiveIndex = 0; PrimitiveIndex < View.IndirectShadowPrimitives.Num(); PrimitiveIndex++)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = View.IndirectShadowPrimitives[PrimitiveIndex];
		const FIndirectLightingCacheAllocation* Allocation = PrimitiveSceneInfo->IndirectLightingCacheAllocation;

		FVector4 PackedLightDirection(0, 0, 1, PI / 16);
		float ShapeFadeAlpha = 1;

		if (bPrepareLightData)
		{
			if (SkyLight 
				&& !SkyLight->bHasStaticLighting
				&& SkyLight->bWantsStaticShadowing
				&& View.Family->EngineShowFlags.SkyLighting
				&& Allocation)
			{
				// Stationary sky light case
				// Get the indirect shadow direction from the unoccluded sky direction
				const float ConeAngle = FMath::Max(Allocation->CurrentSkyBentNormal.W * GCapsuleSkyAngleScale * .5f * PI, GCapsuleMinSkyAngle * PI / 180.0f);
				PackedLightDirection = FVector4(Allocation->CurrentSkyBentNormal, ConeAngle);
			}
			else if (SkyLight 
				&& !SkyLight->bHasStaticLighting 
				&& !SkyLight->bWantsStaticShadowing
				&& View.Family->EngineShowFlags.SkyLighting)
			{
				// Movable sky light case
				const FSHVector2 SkyLightingIntensity = FSHVectorRGB2(SkyLight->IrradianceEnvironmentMap).GetLuminance();
				const FVector ExtractedMaxDirection = SkyLightingIntensity.GetMaximumDirection();

				// Get the indirect shadow direction from the primary sky lighting direction
				PackedLightDirection = FVector4(ExtractedMaxDirection, GCapsuleIndirectConeAngle);
			}
			else if (Allocation)
			{
				// Static sky light or no sky light case
				FSHVectorRGB2 IndirectLighting;
				IndirectLighting.R = FSHVector2(Allocation->TargetSamplePacked0[0]);
				IndirectLighting.G = FSHVector2(Allocation->TargetSamplePacked0[1]);
				IndirectLighting.B = FSHVector2(Allocation->TargetSamplePacked0[2]);
				const FSHVector2 IndirectLightingIntensity = IndirectLighting.GetLuminance();
				const FVector ExtractedMaxDirection = IndirectLightingIntensity.GetMaximumDirection();

				// Get the indirect shadow direction from the primary indirect lighting direction
				PackedLightDirection = FVector4(ExtractedMaxDirection, GCapsuleIndirectConeAngle);
			}

			if (CosFadeStartAngle < 1)
			{
				// Fade out when nearly vertical up due to self shadowing artifacts
				ShapeFadeAlpha = 1 - FMath::Clamp(2 * (-PackedLightDirection.Z - CosFadeStartAngle) / (1 - CosFadeStartAngle), 0.0f, 1.0f);
			}
		}
			
		if (ShapeFadeAlpha > 0)
		{
			const int32 OriginalNumCapsuleShapes = CapsuleShapeData.Num();
			const int32 OriginalNumMeshDistanceFieldCasters = MeshDistanceFieldCasterIndices.Num();

			TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowGroupPrimitives;
			PrimitiveSceneInfo->GatherLightingAttachmentGroupPrimitives(ShadowGroupPrimitives);

			for (int32 ChildIndex = 0; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
			{
				const FPrimitiveSceneInfo* GroupPrimitiveSceneInfo = ShadowGroupPrimitives[ChildIndex];

				if (GroupPrimitiveSceneInfo->Proxy->CastsDynamicShadow())
				{
					GroupPrimitiveSceneInfo->Proxy->GetShadowShapes(CapsuleShapeData);
					
					if (GroupPrimitiveSceneInfo->Proxy->HasDistanceFieldRepresentation())
					{
						MeshDistanceFieldCasterIndices.Append(GroupPrimitiveSceneInfo->DistanceFieldInstanceIndices);
					}
				}
			}

			// Pack both values into a single float to keep float4 alignment
			const FFloat16 LightAngle16f = FFloat16(PackedLightDirection.W);
			const FFloat16 MinVisibility16f = FFloat16(PrimitiveSceneInfo->Proxy->GetDynamicIndirectShadowMinVisibility());
			const uint32 PackedWInt = ((uint32)LightAngle16f.Encoded) | ((uint32)MinVisibility16f.Encoded << 16);
			PackedLightDirection.W = *(float*)&PackedWInt;

			//@todo - remove entries with 0 fade alpha
			for (int32 ShapeIndex = OriginalNumCapsuleShapes; ShapeIndex < CapsuleShapeData.Num(); ShapeIndex++)
			{
				CapsuleShapeData[ShapeIndex].Radius *= ShapeFadeAlpha;
				CapsuleLightSourceData.Add(PackedLightDirection);
			}

			for (int32 CasterIndex = OriginalNumMeshDistanceFieldCasters; CasterIndex < MeshDistanceFieldCasterIndices.Num(); CasterIndex++)
			{
				DistanceFieldCasterLightSourceData.Add(PackedLightDirection);
			}
		}
	}

	if (CapsuleShapeData.Num() > 0 || MeshDistanceFieldCasterIndices.Num() > 0)
	{
		static_assert(sizeof(FCapsuleShape) == sizeof(FVector4)* 2, "FCapsuleShape has padding");
		if (CapsuleShapeData.Num() > 0)
		{
			const int32 DataSize = CapsuleShapeData.Num() * CapsuleShapeData.GetTypeSize();

			if (!IsValidRef(View.ViewState->IndirectShadowCapsuleShapesVertexBuffer) || (int32)View.ViewState->IndirectShadowCapsuleShapesVertexBuffer->GetSize() < DataSize)
			{
				View.ViewState->IndirectShadowCapsuleShapesVertexBuffer.SafeRelease();
				View.ViewState->IndirectShadowCapsuleShapesSRV.SafeRelease();
				FRHIResourceCreateInfo CreateInfo;
				View.ViewState->IndirectShadowCapsuleShapesVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
				View.ViewState->IndirectShadowCapsuleShapesSRV = RHICreateShaderResourceView(View.ViewState->IndirectShadowCapsuleShapesVertexBuffer, sizeof(FVector4), PF_A32B32G32R32F);
			}

			void* CapsuleShapeLockedData = RHILockVertexBuffer(View.ViewState->IndirectShadowCapsuleShapesVertexBuffer, 0, DataSize, RLM_WriteOnly);
			FPlatformMemory::Memcpy(CapsuleShapeLockedData, CapsuleShapeData.GetData(), DataSize);
			RHIUnlockVertexBuffer(View.ViewState->IndirectShadowCapsuleShapesVertexBuffer);
		}

		if (MeshDistanceFieldCasterIndices.Num() > 0)
		{
			const int32 DataSize = MeshDistanceFieldCasterIndices.Num() * MeshDistanceFieldCasterIndices.GetTypeSize();

			if (!IsValidRef(View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer) || (int32)View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer->GetSize() < DataSize)
			{
				View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer.SafeRelease();
				View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesSRV.SafeRelease();
				FRHIResourceCreateInfo CreateInfo;
				View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
				View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesSRV = RHICreateShaderResourceView(View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer, sizeof(int32), PF_R32_SINT);
			}

			void* LockedData = RHILockVertexBuffer(View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer, 0, DataSize, RLM_WriteOnly);
			FPlatformMemory::Memcpy(LockedData, MeshDistanceFieldCasterIndices.GetData(), DataSize);
			RHIUnlockVertexBuffer(View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesVertexBuffer);
		}

		if (bPrepareLightData)
		{
			size_t CapsuleLightSourceDataSize = CapsuleLightSourceData.Num() * CapsuleLightSourceData.GetTypeSize();
			const int32 DataSize = CapsuleLightSourceDataSize + DistanceFieldCasterLightSourceData.Num() * DistanceFieldCasterLightSourceData.GetTypeSize();

			if (!IsValidRef(View.ViewState->IndirectShadowLightDirectionVertexBuffer) || (int32)View.ViewState->IndirectShadowLightDirectionVertexBuffer->GetSize() < DataSize)
			{
				View.ViewState->IndirectShadowLightDirectionVertexBuffer.SafeRelease();
				View.ViewState->IndirectShadowLightDirectionSRV.SafeRelease();
				FRHIResourceCreateInfo CreateInfo;
				View.ViewState->IndirectShadowLightDirectionVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
				View.ViewState->IndirectShadowLightDirectionSRV = RHICreateShaderResourceView(View.ViewState->IndirectShadowLightDirectionVertexBuffer, sizeof(FVector4), PF_A32B32G32R32F);
			}

			FVector4* LightDirectionLockedData = (FVector4*)RHILockVertexBuffer(View.ViewState->IndirectShadowLightDirectionVertexBuffer, 0, DataSize, RLM_WriteOnly);
			FPlatformMemory::Memcpy(LightDirectionLockedData, CapsuleLightSourceData.GetData(), CapsuleLightSourceDataSize);
			// Light data for distance fields is placed after capsule light data
			// This packing behavior must match GetLightDirectionData
			FPlatformMemory::Memcpy((char*)LightDirectionLockedData + CapsuleLightSourceDataSize, DistanceFieldCasterLightSourceData.GetData(), DistanceFieldCasterLightSourceData.Num() * DistanceFieldCasterLightSourceData.GetTypeSize());
			RHIUnlockVertexBuffer(View.ViewState->IndirectShadowLightDirectionVertexBuffer);
		}
	}

	NumCapsuleShapes = CapsuleShapeData.Num();
	NumMeshDistanceFieldCasters = MeshDistanceFieldCasterIndices.Num();
}

void FDeferredShadingSceneRenderer::RenderIndirectCapsuleShadows(
	FRHICommandListImmediate& RHICmdList, 
	FTextureRHIParamRef IndirectLightingTexture, 
	FTextureRHIParamRef ExistingIndirectOcclusionTexture) const
{
	if (SupportsCapsuleShadows(FeatureLevel, GShaderPlatformForFeatureLevel[FeatureLevel])
		&& FSceneRenderTargets::Get(RHICmdList).IsStaticLightingAllowed())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderIndirectCapsuleShadows);

		bool bAnyViewsUseCapsuleShadows = false;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			if (View.IndirectShadowPrimitives.Num() > 0 && View.ViewState)
			{
				bAnyViewsUseCapsuleShadows = true;
			}
		}

		if (bAnyViewsUseCapsuleShadows)
		{
			SCOPED_DRAW_EVENT(RHICmdList, IndirectCapsuleShadows);

			TRefCountPtr<IPooledRenderTarget> RayTracedShadowsRT;

			{
				const FIntPoint BufferSize = GetBufferSizeForCapsuleShadows();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_G16R16F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				// Reuse temporary target from RTDF shadows
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RayTracedShadowsRT, TEXT("RayTracedShadows"));
			}

			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
			TArray<FTextureRHIParamRef, TInlineAllocator<2>> RenderTargets;

			if (IndirectLightingTexture)
			{
				RenderTargets.Add(IndirectLightingTexture);
			}

			if (ExistingIndirectOcclusionTexture)
			{
				RenderTargets.Add(ExistingIndirectOcclusionTexture);
			}

			if (RenderTargets.Num() == 0)
			{
				SceneContext.bScreenSpaceAOIsValid = true;
				RenderTargets.Add(SceneContext.ScreenSpaceAO->GetRenderTargetItem().TargetableTexture);

				SCOPED_DRAW_EVENT(RHICmdList, ClearIndirectOcclusion);
				// We are the first users of the indirect occlusion texture so we must clear to unoccluded
				SetRenderTargets(RHICmdList, RenderTargets.Num(), RenderTargets.GetData(), FTextureRHIParamRef(), 0, NULL, true);
				RHICmdList.ClearColorTexture(SceneContext.ScreenSpaceAO->GetRenderTargetItem().TargetableTexture, FLinearColor::White, FIntRect());
			}
							
			check(RenderTargets.Num() > 0);

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];

				if (View.IndirectShadowPrimitives.Num() > 0 && View.ViewState)
				{
					SCOPED_GPU_STAT(RHICmdList, Stat_GPU_CapsuleShadows);
		
					int32 NumCapsuleShapes = 0;
					int32 NumMeshDistanceFieldCasters = 0;
					SetupIndirectCapsuleShadows(RHICmdList, View, true, NumCapsuleShapes, NumMeshDistanceFieldCasters);

					if (NumCapsuleShapes > 0 || NumMeshDistanceFieldCasters > 0)
					{
						SetRenderTarget(RHICmdList, NULL, NULL);

						FIntRect ScissorRect = View.ViewRect;

						const FIntPoint GroupSize(
							FMath::DivideAndRoundUp(ScissorRect.Size().X / GetCapsuleShadowDownsampleFactor(), GShadowShapeTileSize),
							FMath::DivideAndRoundUp(ScissorRect.Size().Y / GetCapsuleShadowDownsampleFactor(), GShadowShapeTileSize));
				
						AllocateCapsuleTileIntersectionCountsBuffer(GroupSize, View.ViewState);

						uint32 ClearValues[4] = { 0 };
						RHICmdList.ClearUAV(View.ViewState->CapsuleTileIntersectionCountsBuffer.UAV, ClearValues);

						{
							SCOPED_DRAW_EVENT(RHICmdList, TiledCapsuleShadowing);

							FSceneRenderTargetItem& RayTracedShadowsRTI = RayTracedShadowsRT->GetRenderTargetItem();
							{
								TCapsuleShadowingBaseCS<ShapeShadow_IndirectTiledCulling>* ComputeShaderBase = NULL;

								if (NumCapsuleShapes > 0 && NumMeshDistanceFieldCasters > 0)
								{
									TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_IndirectTiledCulling, IPT_CapsuleShapesAndMeshDistanceFields> > ComputeShader(View.ShaderMap);
									ComputeShaderBase = *ComputeShader;
								}
								else if (NumCapsuleShapes > 0)
								{
									TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_IndirectTiledCulling, IPT_CapsuleShapes> > ComputeShader(View.ShaderMap);
									ComputeShaderBase = *ComputeShader;
								}
								else
								{
									check(NumMeshDistanceFieldCasters > 0);
									TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_IndirectTiledCulling, IPT_MeshDistanceFields> > ComputeShader(View.ShaderMap);
									ComputeShaderBase = *ComputeShader;
								}

								RHICmdList.SetComputeShader(ComputeShaderBase->GetComputeShader());

								ComputeShaderBase->SetParameters(
									RHICmdList, 
									Scene,
									View, 
									NULL, 
									RayTracedShadowsRTI, 
									GroupSize,
									&View.ViewState->CapsuleTileIntersectionCountsBuffer,
									FVector2D(GroupSize.X, GroupSize.Y), 
									GCapsuleMaxIndirectOcclusionDistance, 
									ScissorRect, 
									GetCapsuleShadowDownsampleFactor(),
									NumCapsuleShapes, 
									View.ViewState->IndirectShadowCapsuleShapesSRV ? View.ViewState->IndirectShadowCapsuleShapesSRV.GetReference() : NULL,
									NumMeshDistanceFieldCasters,
									View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesSRV ? View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesSRV.GetReference() : NULL,
									View.ViewState->IndirectShadowLightDirectionSRV,
									NULL);

								DispatchComputeShader(RHICmdList, ComputeShaderBase, GroupSize.X, GroupSize.Y, 1);
								ComputeShaderBase->UnsetParameters(RHICmdList, RayTracedShadowsRTI, &View.ViewState->CapsuleTileIntersectionCountsBuffer);
							}
						}
			
						{
							SCOPED_DRAW_EVENTF(RHICmdList, Upsample, TEXT("Upsample %dx%d"), ScissorRect.Width(), ScissorRect.Height());

							SetRenderTargets(RHICmdList, RenderTargets.Num(), RenderTargets.GetData(), FTextureRHIParamRef(), 0, NULL, true);

							RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
							RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
							RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				
							// Modulative blending against scene color for application to indirect diffuse
							// Modulative blending against SSAO occlusion value for application to indirect specular, since Reflection Environment pass masks by AO
							if (RenderTargets.Num() > 1)
							{
								RHICmdList.SetBlendState(TStaticBlendState<
									CW_RGB, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_Zero, BF_One,
									CW_RED, BO_Add, BF_DestColor, BF_Zero, BO_Add, BF_Zero, BF_One>::GetRHI());
							}
							else
							{
								RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
							}

							TShaderMapRef<FCapsuleShadowingUpsampleVS> VertexShader(View.ShaderMap);

							if (RenderTargets.Num() > 1)
							{
								if (GCapsuleShadowsFullResolution)
								{
									TShaderMapRef<TCapsuleShadowingUpsamplePS<false, true> > PixelShader(View.ShaderMap);

									static FGlobalBoundShaderState BoundShaderState;
									SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GTileVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					
									VertexShader->SetParameters(RHICmdList, View, GroupSize, ScissorRect, View.ViewState->CapsuleTileIntersectionCountsBuffer);
									PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, false);
								}
								else
								{
									TShaderMapRef<TCapsuleShadowingUpsamplePS<true, true> > PixelShader(View.ShaderMap);

									static FGlobalBoundShaderState BoundShaderState;
									SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GTileVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

									VertexShader->SetParameters(RHICmdList, View, GroupSize, ScissorRect, View.ViewState->CapsuleTileIntersectionCountsBuffer);
									PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, false);
								}
							}
							else
							{
								if (GCapsuleShadowsFullResolution)
								{
									TShaderMapRef<TCapsuleShadowingUpsamplePS<false, false> > PixelShader(View.ShaderMap);

									static FGlobalBoundShaderState BoundShaderState;
									SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GTileVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					
									VertexShader->SetParameters(RHICmdList, View, GroupSize, ScissorRect, View.ViewState->CapsuleTileIntersectionCountsBuffer);
									PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, false);
								}
								else
								{
									TShaderMapRef<TCapsuleShadowingUpsamplePS<true, false> > PixelShader(View.ShaderMap);

									static FGlobalBoundShaderState BoundShaderState;
									SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GTileVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

									VertexShader->SetParameters(RHICmdList, View, GroupSize, ScissorRect, View.ViewState->CapsuleTileIntersectionCountsBuffer);
									PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, false);
								}
							}

							RHICmdList.SetStreamSource(0, GTileTexCoordVertexBuffer.VertexBufferRHI, sizeof(FVector2D), 0);
							RHICmdList.DrawIndexedPrimitive(GTileIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 4, 0, 2 * NumTileQuadsInBuffer, FMath::DivideAndRoundUp(GroupSize.X * GroupSize.Y, NumTileQuadsInBuffer));
						}
					}
				}
			}
		}
	}
}

bool FDeferredShadingSceneRenderer::ShouldPrepareForDFInsetIndirectShadow() const
{
	bool bSceneHasInsetDFPrimitives = false;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < View.IndirectShadowPrimitives.Num(); PrimitiveIndex++)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = View.IndirectShadowPrimitives[PrimitiveIndex];
			TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowGroupPrimitives;
			PrimitiveSceneInfo->GatherLightingAttachmentGroupPrimitives(ShadowGroupPrimitives);

			for (int32 ChildIndex = 0; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
			{
				const FPrimitiveSceneInfo* GroupPrimitiveSceneInfo = ShadowGroupPrimitives[ChildIndex];

				if (GroupPrimitiveSceneInfo->Proxy->CastsDynamicShadow() && GroupPrimitiveSceneInfo->Proxy->HasDistanceFieldRepresentation())
				{
					bSceneHasInsetDFPrimitives = true;
				}
			}
		}
	}

	return bSceneHasInsetDFPrimitives && SupportsCapsuleShadows(FeatureLevel, GShaderPlatformForFeatureLevel[FeatureLevel]);
}

void FDeferredShadingSceneRenderer::RenderCapsuleShadowsForMovableSkylight(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& BentNormalOutput) const
{
	if (SupportsCapsuleShadows(FeatureLevel, GShaderPlatformForFeatureLevel[FeatureLevel]))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderCapsuleShadowsSkylight);

		bool bAnyViewsUseCapsuleShadows = false;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			if (View.IndirectShadowPrimitives.Num() > 0 && View.ViewState)
			{
				bAnyViewsUseCapsuleShadows = true;
			}
		}

		if (bAnyViewsUseCapsuleShadows)
		{
			TRefCountPtr<IPooledRenderTarget> NewBentNormal;
			AllocateOrReuseAORenderTarget(RHICmdList, NewBentNormal, TEXT("CapsuleBentNormal"), true);

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];

				if (View.IndirectShadowPrimitives.Num() > 0 && View.ViewState)
				{
					SCOPED_DRAW_EVENT(RHICmdList, IndirectCapsuleShadows);
					SCOPED_GPU_STAT(RHICmdList, Stat_GPU_CapsuleShadows);
		
					int32 NumCapsuleShapes = 0;
					int32 NumMeshDistanceFieldCasters = 0;
					SetupIndirectCapsuleShadows(RHICmdList, View, true, NumCapsuleShapes, NumMeshDistanceFieldCasters);

					if (NumCapsuleShapes > 0 || NumMeshDistanceFieldCasters > 0)
					{
						SetRenderTarget(RHICmdList, NULL, NULL);

						FIntRect ScissorRect = View.ViewRect;

						{
							uint32 GroupSizeX = FMath::DivideAndRoundUp(ScissorRect.Size().X / GAODownsampleFactor, GShadowShapeTileSize);
							uint32 GroupSizeY = FMath::DivideAndRoundUp(ScissorRect.Size().Y / GAODownsampleFactor, GShadowShapeTileSize);

							{
								SCOPED_DRAW_EVENT(RHICmdList, TiledCapsuleShadowing);

								FSceneRenderTargetItem& RayTracedShadowsRTI = NewBentNormal->GetRenderTargetItem();
								{
									TCapsuleShadowingBaseCS<ShapeShadow_MovableSkylightTiledCulling>* ComputeShaderBase = NULL;

									if (NumCapsuleShapes > 0 && NumMeshDistanceFieldCasters > 0)
									{
										TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_MovableSkylightTiledCulling, IPT_CapsuleShapesAndMeshDistanceFields> > ComputeShader(View.ShaderMap);
										ComputeShaderBase = *ComputeShader;
									}
									else if (NumCapsuleShapes > 0)
									{
										TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_MovableSkylightTiledCulling, IPT_CapsuleShapes> > ComputeShader(View.ShaderMap);
										ComputeShaderBase = *ComputeShader;
									}
									else
									{
										check(NumMeshDistanceFieldCasters > 0);
										TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_MovableSkylightTiledCulling, IPT_MeshDistanceFields> > ComputeShader(View.ShaderMap);
										ComputeShaderBase = *ComputeShader;
									}
									
									RHICmdList.SetComputeShader(ComputeShaderBase->GetComputeShader());

									ComputeShaderBase->SetParameters(
										RHICmdList, 
										Scene,
										View, 
										NULL, 
										RayTracedShadowsRTI, 
										FIntPoint(GroupSizeX, GroupSizeY),
										NULL,
										FVector2D(GroupSizeX, GroupSizeY), 
										GCapsuleMaxIndirectOcclusionDistance, 
										ScissorRect, 
										GAODownsampleFactor,
										NumCapsuleShapes, 
										View.ViewState->IndirectShadowCapsuleShapesSRV ? View.ViewState->IndirectShadowCapsuleShapesSRV.GetReference() : NULL,
										NumMeshDistanceFieldCasters,
										View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesSRV ? View.ViewState->IndirectShadowMeshDistanceFieldCasterIndicesSRV.GetReference() : NULL,
										View.ViewState->IndirectShadowLightDirectionSRV,
										BentNormalOutput->GetRenderTargetItem().ShaderResourceTexture);

									DispatchComputeShader(RHICmdList, ComputeShaderBase, GroupSizeX, GroupSizeY, 1);
									ComputeShaderBase->UnsetParameters(RHICmdList, RayTracedShadowsRTI, NULL);
								}
							}
						}

						// Replace the pipeline output with our output that has capsule shadows applied
						BentNormalOutput = NewBentNormal;
					}
				}
			}
		}
	}
}
