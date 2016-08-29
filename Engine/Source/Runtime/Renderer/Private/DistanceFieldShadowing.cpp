// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldShadowing.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "DistanceFieldAtlas.h"

int32 GDistanceFieldShadowing = 1;
FAutoConsoleVariableRef CVarDistanceFieldShadowing(
	TEXT("r.DistanceFieldShadowing"),
	GDistanceFieldShadowing,
	TEXT("Whether the distance field shadowing feature is allowed."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GFullResolutionDFShadowing = 0;
FAutoConsoleVariableRef CVarFullResolutionDFShadowing(
	TEXT("r.DFFullResolution"),
	GFullResolutionDFShadowing,
	TEXT("1 = full resolution distance field shadowing, 0 = half resolution with bilateral upsample."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GShadowScatterTileCulling = 1;
FAutoConsoleVariableRef CVarShadowScatterTileCulling(
	TEXT("r.DFShadowScatterTileCulling"),
	GShadowScatterTileCulling,
	TEXT("Whether to use the rasterizer to scatter objects onto the tile grid for culling."),
	ECVF_RenderThreadSafe
	);

float GShadowWorldTileSize = 200.0f;
FAutoConsoleVariableRef CVarShadowWorldTileSize(
	TEXT("r.DFShadowWorldTileSize"),
	GShadowWorldTileSize,
	TEXT("World space size of a tile used for culling for directional lights."),
	ECVF_RenderThreadSafe
	);

float GTwoSidedMeshDistanceBias = 4;
FAutoConsoleVariableRef CVarTwoSidedMeshDistanceBias(
	TEXT("r.DFTwoSidedMeshDistanceBias"),
	GTwoSidedMeshDistanceBias,
	TEXT("World space amount to expand distance field representations of two sided meshes.  This is useful to get tree shadows to match up with standard shadow mapping."),
	ECVF_RenderThreadSafe
	);

int32 GetDFShadowDownsampleFactor()
{
	return GFullResolutionDFShadowing ? 1 : GAODownsampleFactor;
}

FIntPoint GetBufferSizeForDFShadows()
{
	return FIntPoint::DivideAndRoundDown(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), GetDFShadowDownsampleFactor());
}

TGlobalResource<FDistanceFieldObjectBufferResource> GShadowCulledObjectBuffers;

/**  */
class FClearObjectIndirectArguments : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FClearObjectIndirectArguments,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Platform);
	}

	FClearObjectIndirectArguments(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectIndirectArguments.Bind(Initializer.ParameterMap, TEXT("ObjectIndirectArguments"));
	}

	FClearObjectIndirectArguments()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		FUnorderedAccessViewRHIParamRef OutUAVs[1];
		OutUAVs[0] = GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;	
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		ObjectIndirectArguments.SetBuffer(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		ObjectIndirectArguments.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[1];
		OutUAVs[0] = GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;	
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectIndirectArguments;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter ObjectIndirectArguments;
};

IMPLEMENT_SHADER_TYPE(,FClearObjectIndirectArguments,TEXT("DistanceFieldShadowing"),TEXT("ClearObjectIndirectArguments"),SF_Compute);

class FCullObjectsForShadowCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullObjectsForShadowCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
	}

	FCullObjectsForShadowCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
		ObjectIndirectArguments.Bind(Initializer.ParameterMap, TEXT("ObjectIndirectArguments"));
		CulledObjectBounds.Bind(Initializer.ParameterMap, TEXT("CulledObjectBounds"));
		CulledObjectData.Bind(Initializer.ParameterMap, TEXT("CulledObjectData"));
		CulledObjectBoxBounds.Bind(Initializer.ParameterMap, TEXT("CulledObjectBoxBounds"));
		ObjectBoundingGeometryIndexCount.Bind(Initializer.ParameterMap, TEXT("ObjectBoundingGeometryIndexCount"));
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		NumShadowHullPlanes.Bind(Initializer.ParameterMap, TEXT("NumShadowHullPlanes"));
		ShadowBoundingSphere.Bind(Initializer.ParameterMap, TEXT("ShadowBoundingSphere"));
		ShadowConvexHull.Bind(Initializer.ParameterMap, TEXT("ShadowConvexHull"));
	}

	FCullObjectsForShadowCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, const FSceneView& View, const FMatrix& WorldToShadowValue, int32 NumPlanes, const FPlane* PlaneData, const FVector4& ShadowBoundingSphereValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, *(Scene->DistanceFieldSceneData.ObjectBuffers), Scene->DistanceFieldSceneData.NumObjectsInBuffer);

		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[0] = GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GShadowCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GShadowCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GShadowCulledObjectBuffers.Buffers.BoxBounds.UAV;		
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		ObjectIndirectArguments.SetBuffer(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments);
		CulledObjectBounds.SetBuffer(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers.Bounds);
		CulledObjectData.SetBuffer(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers.Data);
		CulledObjectBoxBounds.SetBuffer(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers.BoxBounds);

		SetShaderValue(RHICmdList, ShaderRHI, ObjectBoundingGeometryIndexCount, StencilingGeometry::GLowPolyStencilSphereIndexBuffer.GetIndexCount());
		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowValue);
		SetShaderValue(RHICmdList, ShaderRHI, ShadowBoundingSphere, ShadowBoundingSphereValue);

		if (NumPlanes <= 12)
		{
			SetShaderValue(RHICmdList, ShaderRHI, NumShadowHullPlanes, NumPlanes);
			SetShaderValueArray(RHICmdList, ShaderRHI, ShadowConvexHull, PlaneData, NumPlanes);
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, NumShadowHullPlanes, 0);
		}
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FScene* Scene)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), *(Scene->DistanceFieldSceneData.ObjectBuffers));
		ObjectIndirectArguments.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBounds.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectData.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBoxBounds.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[0] = GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GShadowCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GShadowCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GShadowCulledObjectBuffers.Buffers.BoxBounds.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectBufferParameters;
		Ar << ObjectIndirectArguments;
		Ar << CulledObjectBounds;
		Ar << CulledObjectData;
		Ar << CulledObjectBoxBounds;
		Ar << ObjectBoundingGeometryIndexCount;
		Ar << WorldToShadow;
		Ar << NumShadowHullPlanes;
		Ar << ShadowBoundingSphere;
		Ar << ShadowConvexHull;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
	FRWShaderParameter ObjectIndirectArguments;
	FRWShaderParameter CulledObjectBounds;
	FRWShaderParameter CulledObjectData;
	FRWShaderParameter CulledObjectBoxBounds;
	FShaderParameter ObjectBoundingGeometryIndexCount;
	FShaderParameter WorldToShadow;
	FShaderParameter NumShadowHullPlanes;
	FShaderParameter ShadowBoundingSphere;
	FShaderParameter ShadowConvexHull;
};

IMPLEMENT_SHADER_TYPE(,FCullObjectsForShadowCS,TEXT("DistanceFieldShadowing"),TEXT("CullObjectsForShadowCS"),SF_Compute);


/**  */
class FClearTilesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FClearTilesCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	FClearTilesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ShadowTileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("ShadowTileHeadDataUnpacked"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
	}

	FClearTilesCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FVector2D NumGroupsValue, FLightTileIntersectionResources* TileIntersectionResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, TileIntersectionResources->TileHeadDataUnpacked.UAV);
		ShadowTileHeadDataUnpacked.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileHeadDataUnpacked);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FLightTileIntersectionResources* TileIntersectionResources)
	{
		ShadowTileHeadDataUnpacked.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, TileIntersectionResources->TileHeadDataUnpacked.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ShadowTileHeadDataUnpacked;
		Ar << NumGroups;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter ShadowTileHeadDataUnpacked;
	FShaderParameter NumGroups;
};

IMPLEMENT_SHADER_TYPE(,FClearTilesCS,TEXT("DistanceFieldShadowing"),TEXT("ClearTilesCS"),SF_Compute);


/**  */
class FShadowObjectCullVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowObjectCullVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Platform); 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FShadowObjectCullVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		ConservativeRadiusScale.Bind(Initializer.ParameterMap, TEXT("ConservativeRadiusScale"));
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		MinRadius.Bind(Initializer.ParameterMap, TEXT("MinRadius"));
	}

	FShadowObjectCullVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FVector2D NumGroupsValue, const FMatrix& WorldToShadowMatrixValue, float ShadowRadius)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		
		ObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);

		const int32 NumRings = StencilingGeometry::GLowPolyStencilSphereVertexBuffer.GetNumRings();
		const float RadiansPerRingSegment = PI / (float)NumRings;

		// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
		const float ConservativeRadiusScaleValue = 1.0f / FMath::Cos(RadiansPerRingSegment);

		SetShaderValue(RHICmdList, ShaderRHI, ConservativeRadiusScale, ConservativeRadiusScaleValue);

		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowMatrixValue);

		float MinRadiusValue = 2 * ShadowRadius / FMath::Min(NumGroupsValue.X, NumGroupsValue.Y);
		SetShaderValue(RHICmdList, ShaderRHI, MinRadius, MinRadiusValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << ConservativeRadiusScale;
		Ar << WorldToShadow;
		Ar << MinRadius;
		return bShaderHasOutdatedParameters;
	}

private:
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FShaderParameter ConservativeRadiusScale;
	FShaderParameter WorldToShadow;
	FShaderParameter MinRadius;
};

IMPLEMENT_SHADER_TYPE(,FShadowObjectCullVS,TEXT("DistanceFieldShadowing"),TEXT("ShadowObjectCullVS"),SF_Vertex);

class FShadowObjectCullPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShadowObjectCullPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Platform) && RHISupportsPixelShaderUAVs(Platform);
	}

	/** Default constructor. */
	FShadowObjectCullPS() {}

	/** Initialization constructor. */
	FShadowObjectCullPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		ShadowTileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("ShadowTileHeadDataUnpacked"));
		ShadowTileArrayData.Bind(Initializer.ParameterMap, TEXT("ShadowTileArrayData"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FVector2D NumGroupsValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);
		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void GetUAVs(const FSceneView& View, FLightTileIntersectionResources* TileIntersectionResources, TArray<FUnorderedAccessViewRHIParamRef>& UAVs)
	{
		int32 MaxIndex = FMath::Max(ShadowTileHeadDataUnpacked.GetUAVIndex(), ShadowTileArrayData.GetUAVIndex());
		UAVs.AddZeroed(MaxIndex + 1);

		if (ShadowTileHeadDataUnpacked.IsBound())
		{
			UAVs[ShadowTileHeadDataUnpacked.GetUAVIndex()] = TileIntersectionResources->TileHeadDataUnpacked.UAV;
		}

		if (ShadowTileArrayData.IsBound())
		{
			UAVs[ShadowTileArrayData.GetUAVIndex()] = TileIntersectionResources->TileArrayData.UAV;
		}

		check(UAVs.Num() > 0);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << ShadowTileHeadDataUnpacked;
		Ar << ShadowTileArrayData;
		Ar << NumGroups;
		return bShaderHasOutdatedParameters;
	}

private:
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FRWShaderParameter ShadowTileHeadDataUnpacked;
	FRWShaderParameter ShadowTileArrayData;
	FShaderParameter NumGroups;
};

IMPLEMENT_SHADER_TYPE(,FShadowObjectCullPS,TEXT("DistanceFieldShadowing"),TEXT("ShadowObjectCullPS"),SF_Pixel);



/**  */
class FWorkaroundAMDBugCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWorkaroundAMDBugCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Platform);
	}

	FWorkaroundAMDBugCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ShadowTileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("ShadowTileHeadDataUnpacked"));
		ShadowTileArrayData.Bind(Initializer.ParameterMap, TEXT("ShadowTileArrayData"));
	}

	FWorkaroundAMDBugCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FLightTileIntersectionResources* TileIntersectionResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = TileIntersectionResources->TileHeadDataUnpacked.UAV;
		OutUAVs[1] = TileIntersectionResources->TileArrayData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		ShadowTileHeadDataUnpacked.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileHeadDataUnpacked);
		ShadowTileArrayData.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileArrayData);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FLightTileIntersectionResources* TileIntersectionResources)
	{
		ShadowTileHeadDataUnpacked.UnsetUAV(RHICmdList, GetComputeShader());
		ShadowTileArrayData.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = TileIntersectionResources->TileHeadDataUnpacked.UAV;
		OutUAVs[1] = TileIntersectionResources->TileArrayData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ShadowTileHeadDataUnpacked;
		Ar << ShadowTileArrayData;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter ShadowTileHeadDataUnpacked;
	FRWShaderParameter ShadowTileArrayData;
};

IMPLEMENT_SHADER_TYPE(,FWorkaroundAMDBugCS,TEXT("DistanceFieldShadowing"),TEXT("WorkaroundAMDBugCS"),SF_Compute);

enum EDistanceFieldShadowingType
{
	DFS_DirectionalLightScatterTileCulling,
	DFS_DirectionalLightTiledCulling,
	DFS_PointLightTiledCulling
};

template<EDistanceFieldShadowingType ShadowingType>
class TDistanceFieldShadowingCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldShadowingCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
		OutEnvironment.SetDefine(TEXT("SCATTER_TILE_CULLING"), ShadowingType == DFS_DirectionalLightScatterTileCulling);
		OutEnvironment.SetDefine(TEXT("POINT_LIGHT"), ShadowingType == DFS_PointLightTiledCulling);
	}

	/** Default constructor. */
	TDistanceFieldShadowingCS() {}

	/** Initialization constructor. */
	TDistanceFieldShadowingCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ShadowFactors.Bind(Initializer.ParameterMap, TEXT("ShadowFactors"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		LightDirection.Bind(Initializer.ParameterMap, TEXT("LightDirection"));
		LightSourceRadius.Bind(Initializer.ParameterMap, TEXT("LightSourceRadius"));
		RayStartOffsetDepthScale.Bind(Initializer.ParameterMap, TEXT("RayStartOffsetDepthScale"));
		LightPositionAndInvRadius.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		TanLightAngleAndNormalThreshold.Bind(Initializer.ParameterMap, TEXT("TanLightAngleAndNormalThreshold"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap, TEXT("ScissorRectMinAndSize"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		ShadowTileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("ShadowTileHeadDataUnpacked"));
		ShadowTileArrayData.Bind(Initializer.ParameterMap, TEXT("ShadowTileArrayData"));
		ShadowTileListGroupSize.Bind(Initializer.ParameterMap, TEXT("ShadowTileListGroupSize"));
		WorldToShadow.Bind(Initializer.ParameterMap, TEXT("WorldToShadow"));
		TwoSidedMeshDistanceBias.Bind(Initializer.ParameterMap, TEXT("TwoSidedMeshDistanceBias"));
		DownsampleFactor.Bind(Initializer.ParameterMap, TEXT("DownsampleFactor"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FProjectedShadowInfo* ProjectedShadowInfo,
		FSceneRenderTargetItem& ShadowFactorsValue, 
		FVector2D NumGroupsValue,
		const FIntRect& ScissorRect,
		FLightTileIntersectionResources* TileIntersectionResources)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ShadowFactorsValue.UAV);
		ShadowFactors.SetTexture(RHICmdList, ShaderRHI, ShadowFactorsValue.ShaderResourceTexture, ShadowFactorsValue.UAV);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GShadowCulledObjectBuffers.Buffers);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);

		FVector4 LightPositionAndInvRadiusValue;
		FVector4 LightColorAndFalloffExponent;
		FVector NormalizedLightDirection;
		FVector2D SpotAngles;
		float LightSourceRadiusValue;
		float LightSourceLength;
		float LightMinRoughness;

		const FLightSceneProxy& LightProxy = *(ProjectedShadowInfo->GetLightSceneInfo().Proxy);

		LightProxy.GetParameters(LightPositionAndInvRadiusValue, LightColorAndFalloffExponent, NormalizedLightDirection, SpotAngles, LightSourceRadiusValue, LightSourceLength, LightMinRoughness);

		SetShaderValue(RHICmdList, ShaderRHI, LightDirection, NormalizedLightDirection);
		SetShaderValue(RHICmdList, ShaderRHI, LightPositionAndInvRadius, LightPositionAndInvRadiusValue);
		// Default light source radius of 0 gives poor results
		SetShaderValue(RHICmdList, ShaderRHI, LightSourceRadius, LightSourceRadiusValue == 0 ? 20 : FMath::Clamp(LightSourceRadiusValue, .001f, 1.0f / (4 * LightPositionAndInvRadiusValue.W)));

		SetShaderValue(RHICmdList, ShaderRHI, RayStartOffsetDepthScale, LightProxy.GetRayStartOffsetDepthScale());

		const float LightSourceAngle = FMath::Clamp(LightProxy.GetLightSourceAngle(), 0.001f, 5.0f) * PI / 180.0f;
		const FVector TanLightAngleAndNormalThresholdValue(FMath::Tan(LightSourceAngle), FMath::Cos(PI / 2 + LightSourceAngle), LightProxy.GetTraceDistance());
		SetShaderValue(RHICmdList, ShaderRHI, TanLightAngleAndNormalThreshold, TanLightAngleAndNormalThresholdValue);

		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));

		check(TileIntersectionResources || !ShadowTileHeadDataUnpacked.IsBound());

		if (TileIntersectionResources)
		{
			SetSRVParameter(RHICmdList, ShaderRHI, ShadowTileHeadDataUnpacked, TileIntersectionResources->TileHeadDataUnpacked.SRV);
			SetSRVParameter(RHICmdList, ShaderRHI, ShadowTileArrayData, TileIntersectionResources->TileArrayData.SRV);
			SetShaderValue(RHICmdList, ShaderRHI, ShadowTileListGroupSize, TileIntersectionResources->TileDimensions);
		}

		FMatrix WorldToShadowMatrixValue = FTranslationMatrix(ProjectedShadowInfo->PreShadowTranslation) * ProjectedShadowInfo->SubjectAndReceiverMatrix;
		SetShaderValue(RHICmdList, ShaderRHI, WorldToShadow, WorldToShadowMatrixValue);

		SetShaderValue(RHICmdList, ShaderRHI, TwoSidedMeshDistanceBias, GTwoSidedMeshDistanceBias);

		SetShaderValue(RHICmdList, ShaderRHI, DownsampleFactor, GetDFShadowDownsampleFactor());
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& ShadowFactorsValue)
	{
		ShadowFactors.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ShadowFactorsValue.UAV);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ShadowFactors;
		Ar << NumGroups;
		Ar << LightDirection;
		Ar << LightPositionAndInvRadius;
		Ar << LightSourceRadius;
		Ar << RayStartOffsetDepthScale;
		Ar << TanLightAngleAndNormalThreshold;
		Ar << ScissorRectMinAndSize;
		Ar << ObjectParameters;
		Ar << DeferredParameters;
		Ar << ShadowTileHeadDataUnpacked;
		Ar << ShadowTileArrayData;
		Ar << ShadowTileListGroupSize;
		Ar << WorldToShadow;
		Ar << TwoSidedMeshDistanceBias;
		Ar << DownsampleFactor;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter ShadowFactors;
	FShaderParameter NumGroups;
	FShaderParameter LightDirection;
	FShaderParameter LightPositionAndInvRadius;
	FShaderParameter LightSourceRadius;
	FShaderParameter RayStartOffsetDepthScale;
	FShaderParameter TanLightAngleAndNormalThreshold;
	FShaderParameter ScissorRectMinAndSize;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter ShadowTileHeadDataUnpacked;
	FShaderResourceParameter ShadowTileArrayData;
	FShaderParameter ShadowTileListGroupSize;
	FShaderParameter WorldToShadow;
	FShaderParameter TwoSidedMeshDistanceBias;
	FShaderParameter DownsampleFactor;
};

IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldShadowingCS<DFS_DirectionalLightScatterTileCulling>,TEXT("DistanceFieldShadowing"),TEXT("DistanceFieldShadowingCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldShadowingCS<DFS_DirectionalLightTiledCulling>,TEXT("DistanceFieldShadowing"),TEXT("DistanceFieldShadowingCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldShadowingCS<DFS_PointLightTiledCulling>,TEXT("DistanceFieldShadowing"),TEXT("DistanceFieldShadowingCS"),SF_Compute);

template<bool bUpsampleRequired>
class TDistanceFieldShadowingUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldShadowingUpsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldShadowing(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("UPSAMPLE_REQUIRED"), bUpsampleRequired);
	}

	/** Default constructor. */
	TDistanceFieldShadowingUpsamplePS() {}

	/** Initialization constructor. */
	TDistanceFieldShadowingUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ShadowFactorsTexture.Bind(Initializer.ParameterMap,TEXT("ShadowFactorsTexture"));
		ShadowFactorsSampler.Bind(Initializer.ParameterMap,TEXT("ShadowFactorsSampler"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap,TEXT("ScissorRectMinAndSize"));
		FadePlaneOffset.Bind(Initializer.ParameterMap,TEXT("FadePlaneOffset"));
		InvFadePlaneLength.Bind(Initializer.ParameterMap,TEXT("InvFadePlaneLength"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo, const FIntRect& ScissorRect, TRefCountPtr<IPooledRenderTarget>& ShadowFactorsTextureValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, ShadowFactorsTexture, ShadowFactorsSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), ShadowFactorsTextureValue->GetRenderTargetItem().ShaderResourceTexture);
	
		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));

		if (ShadowInfo->bDirectionalLight && ShadowInfo->CascadeSettings.FadePlaneLength > 0)
		{
			SetShaderValue(RHICmdList, ShaderRHI, FadePlaneOffset, ShadowInfo->CascadeSettings.FadePlaneOffset);
			SetShaderValue(RHICmdList, ShaderRHI, InvFadePlaneLength, 1.0f / FMath::Max(ShadowInfo->CascadeSettings.FadePlaneLength, .00001f));
		}
		else
		{
			SetShaderValue(RHICmdList, ShaderRHI, FadePlaneOffset, 0.0f);
			SetShaderValue(RHICmdList, ShaderRHI, InvFadePlaneLength, 0.0f);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ShadowFactorsTexture;
		Ar << ShadowFactorsSampler;
		Ar << ScissorRectMinAndSize;
		Ar << FadePlaneOffset;
		Ar << InvFadePlaneLength;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter ShadowFactorsTexture;
	FShaderResourceParameter ShadowFactorsSampler;
	FShaderParameter ScissorRectMinAndSize;
	FShaderParameter FadePlaneOffset;
	FShaderParameter InvFadePlaneLength;
};

IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldShadowingUpsamplePS<true>,TEXT("DistanceFieldShadowing"),TEXT("DistanceFieldShadowingUpsamplePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldShadowingUpsamplePS<false>,TEXT("DistanceFieldShadowing"),TEXT("DistanceFieldShadowingUpsamplePS"),SF_Pixel);

void CullDistanceFieldObjectsForLight(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FLightSceneProxy* LightSceneProxy, 
	const FMatrix& WorldToShadowValue, 
	int32 NumPlanes, 
	const FPlane* PlaneData, 
	const FVector4& ShadowBoundingSphereValue,
	float ShadowBoundingRadius,
	TScopedPointer<FLightTileIntersectionResources>& TileIntersectionResources)
{
	const FScene* Scene = (const FScene*)(View.Family->Scene);

	SCOPED_DRAW_EVENT(RHICmdList, CullObjectsForLight);

	{
		if (GShadowCulledObjectBuffers.Buffers.MaxObjects < Scene->DistanceFieldSceneData.NumObjectsInBuffer
			|| GShadowCulledObjectBuffers.Buffers.MaxObjects > 3 * Scene->DistanceFieldSceneData.NumObjectsInBuffer)
		{
			GShadowCulledObjectBuffers.Buffers.bWantBoxBounds = true;
			GShadowCulledObjectBuffers.Buffers.MaxObjects = Scene->DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
			GShadowCulledObjectBuffers.Buffers.Release();
			GShadowCulledObjectBuffers.Buffers.Initialize();
		}

		{
			// Manual compute shader clear to work around a bug on PS4 where a ClearUAV does not complete before the DrawIndexedPrimitiveIndirect read in the graphics pipe
			bool bUseComputeShaderClear = true;

			if (bUseComputeShaderClear)
			{
				TShaderMapRef<FClearObjectIndirectArguments> ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View);
				DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
				ComputeShader->UnsetParameters(RHICmdList);
			}
			else
			{
				uint32 ClearValues[4] = { 0 };
				RHICmdList.ClearUAV(GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV, ClearValues);
			}

			TShaderMapRef<FCullObjectsForShadowCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, Scene, View, WorldToShadowValue, NumPlanes, PlaneData, ShadowBoundingSphereValue);

			DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, UpdateObjectsGroupSize), 1, 1);
			ComputeShader->UnsetParameters(RHICmdList, Scene);
		}
	}

	// Allocate tile resolution based on world space size
	//@todo - light space perspective shadow maps would make much better use of the resolution
	const float LightTiles = FMath::Min(ShadowBoundingRadius / GShadowWorldTileSize + 1, 256.0f);
	FIntPoint LightTileDimensions(LightTiles, LightTiles);

	if (LightSceneProxy->GetLightType() == LightType_Directional && GShadowScatterTileCulling)
	{
		if (!TileIntersectionResources || TileIntersectionResources->TileDimensions != LightTileDimensions)
		{
			if (TileIntersectionResources)
			{
				TileIntersectionResources->Release();
			}
			else
			{
				TileIntersectionResources = new FLightTileIntersectionResources();
			}

			TileIntersectionResources->TileDimensions = LightTileDimensions;

			TileIntersectionResources->Initialize();
		}
		
		if (View.GetShaderPlatform() == SP_PCD3D_SM5)
		{
			// AMD PC driver versions before 15.10 have a bug where the UAVs are not updated correctly in FShadowObjectCullPS unless we touch them here (tested on 15.7)
			// This bug is fixed in 15.10, but the workaround makes sure the feature works everywhere
			TShaderMapRef<FWorkaroundAMDBugCS> ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, TileIntersectionResources);
			DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);

			ComputeShader->UnsetParameters(RHICmdList, TileIntersectionResources);
		}
		
		{
			TShaderMapRef<FClearTilesCS> ComputeShader(View.ShaderMap);

			uint32 GroupSizeX = FMath::DivideAndRoundUp(LightTileDimensions.X, GDistanceFieldAOTileSizeX);
			uint32 GroupSizeY = FMath::DivideAndRoundUp(LightTileDimensions.Y, GDistanceFieldAOTileSizeY);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, FVector2D(LightTileDimensions.X, LightTileDimensions.Y), TileIntersectionResources);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

			ComputeShader->UnsetParameters(RHICmdList, TileIntersectionResources);
		}

		{
			TShaderMapRef<FShadowObjectCullVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FShadowObjectCullPS> PixelShader(View.ShaderMap);

			TArray<FUnorderedAccessViewRHIParamRef> UAVs;
			PixelShader->GetUAVs(View, TileIntersectionResources, UAVs);
			RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, UAVs.GetData(), UAVs.Num());
			RHICmdList.SetRenderTargets(0, (const FRHIRenderTargetView *)NULL, NULL, UAVs.Num(), UAVs.GetData());

			RHICmdList.SetViewport(0, 0, 0.0f, LightTileDimensions.X, LightTileDimensions.Y, 1.0f);

			// Render backfaces since camera may intersect
			RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);

			VertexShader->SetParameters(RHICmdList, View, FVector2D(LightTileDimensions.X, LightTileDimensions.Y), WorldToShadowValue, ShadowBoundingRadius);
			PixelShader->SetParameters(RHICmdList, View, FVector2D(LightTileDimensions.X, LightTileDimensions.Y));

			RHICmdList.SetStreamSource(0, StencilingGeometry::GLowPolyStencilSphereVertexBuffer.VertexBufferRHI, sizeof(FVector4), 0);

			RHICmdList.DrawIndexedPrimitiveIndirect(
				PT_TriangleList,
				StencilingGeometry::GLowPolyStencilSphereIndexBuffer.IndexBufferRHI, 
				GShadowCulledObjectBuffers.Buffers.ObjectIndirectArguments.Buffer,
				0);

			SetRenderTarget(RHICmdList, NULL, NULL);
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToCompute, UAVs.GetData(), UAVs.Num());
		}
	}
}

bool SupportsDistanceFieldShadows(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform)
{
	return GDistanceFieldShadowing
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldShadowing(ShaderPlatform);
}

bool FDeferredShadingSceneRenderer::ShouldPrepareForDistanceFieldShadows() const 
{
	bool bSceneHasRayTracedDFShadows = false;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (LightSceneInfo->ShouldRenderLightViewIndependent())
		{
			const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

			for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
			{
				const FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

				if (ProjectedShadowInfo->bRayTracedDistanceField)
				{
					bSceneHasRayTracedDFShadows = true;
					break;
				}
			}
		}
	}

	return ViewFamily.EngineShowFlags.DynamicShadows 
		&& bSceneHasRayTracedDFShadows
		&& SupportsDistanceFieldShadows(Scene->GetFeatureLevel(), Scene->GetShaderPlatform());
}

void FProjectedShadowInfo::RenderRayTracedDistanceFieldProjection(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bProjectingForForwardShading) const
{
	if (SupportsDistanceFieldShadows(View.GetFeatureLevel(), View.GetShaderPlatform()))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderRayTracedDistanceFieldShadows);
		SCOPED_DRAW_EVENT(RHICmdList, RayTracedDistanceFieldShadow);

		const FScene* Scene = (const FScene*)(View.Family->Scene);

		if (GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI && Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0)
		{
			check(!Scene->DistanceFieldSceneData.HasPendingOperations());

			FSceneRenderTargets::Get(RHICmdList).FinishRenderingLightAttenuation(RHICmdList);
			SetRenderTarget(RHICmdList, NULL, NULL);

			int32 NumPlanes = 0;
			const FPlane* PlaneData = NULL;
			FVector4 ShadowBoundingSphereValue(0, 0, 0, 0);

			if (bDirectionalLight)
			{
				NumPlanes = CascadeSettings.ShadowBoundsAccurate.Planes.Num();
				PlaneData = CascadeSettings.ShadowBoundsAccurate.Planes.GetData();
			}
			else if (bOnePassPointLightShadow)
			{
				ShadowBoundingSphereValue = FVector4(ShadowBounds.Center.X, ShadowBounds.Center.Y, ShadowBounds.Center.Z, ShadowBounds.W);
			}
			else
			{
				NumPlanes = CasterFrustum.Planes.Num();
				PlaneData = CasterFrustum.Planes.GetData();
				ShadowBoundingSphereValue = FVector4(PreShadowTranslation, 0);
			}

			const FMatrix WorldToShadowValue = FTranslationMatrix(PreShadowTranslation) * SubjectAndReceiverMatrix;

			CullDistanceFieldObjectsForLight(
				RHICmdList,
				View,
				LightSceneInfo->Proxy, 
				WorldToShadowValue,
				NumPlanes,
				PlaneData,
				ShadowBoundingSphereValue,
				ShadowBounds.W,
				LightSceneInfo->TileIntersectionResources
				);

			FLightTileIntersectionResources* TileIntersectionResources = LightSceneInfo->TileIntersectionResources;

			View.HeightfieldLightingViewInfo.ComputeRayTracedShadowing(View, RHICmdList, this, TileIntersectionResources, GShadowCulledObjectBuffers);

			TRefCountPtr<IPooledRenderTarget> RayTracedShadowsRT;

			{
				const FIntPoint BufferSize = GetBufferSizeForDFShadows();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_G16R16F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RayTracedShadowsRT, TEXT("RayTracedShadows"));
			}

			FIntRect ScissorRect;

			{
				if (!LightSceneInfo->Proxy->GetScissorRect(ScissorRect, View))
				{
					ScissorRect = View.ViewRect;
				}

				uint32 GroupSizeX = FMath::DivideAndRoundUp(ScissorRect.Size().X / GetDFShadowDownsampleFactor(), GDistanceFieldAOTileSizeX);
				uint32 GroupSizeY = FMath::DivideAndRoundUp(ScissorRect.Size().Y / GetDFShadowDownsampleFactor(), GDistanceFieldAOTileSizeY);

				{
					SCOPED_DRAW_EVENT(RHICmdList, RayTraceShadows);

					SetRenderTarget(RHICmdList, NULL, NULL);

					FSceneRenderTargetItem& RayTracedShadowsRTI = RayTracedShadowsRT->GetRenderTargetItem();
					if (bDirectionalLight && GShadowScatterTileCulling)
					{
						TShaderMapRef<TDistanceFieldShadowingCS<DFS_DirectionalLightScatterTileCulling> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, this, RayTracedShadowsRTI, FVector2D(GroupSizeX, GroupSizeY), ScissorRect, TileIntersectionResources);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
						ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI);
					}
					else if (bDirectionalLight)
					{
						TShaderMapRef<TDistanceFieldShadowingCS<DFS_DirectionalLightTiledCulling> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, this, RayTracedShadowsRTI, FVector2D(GroupSizeX, GroupSizeY), ScissorRect, TileIntersectionResources);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
						ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI);
					}
					else
					{
						TShaderMapRef<TDistanceFieldShadowingCS<DFS_PointLightTiledCulling> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, this, RayTracedShadowsRTI, FVector2D(GroupSizeX, GroupSizeY), ScissorRect, TileIntersectionResources);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
						ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI);
					}
				}
			}

			{
				FSceneRenderTargets::Get(RHICmdList).BeginRenderingLightAttenuation(RHICmdList);

				SCOPED_DRAW_EVENT(RHICmdList, Upsample);

				RHICmdList.SetViewport(ScissorRect.Min.X, ScissorRect.Min.Y, 0.0f, ScissorRect.Max.X, ScissorRect.Max.Y, 1.0f);
				RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				
				SetBlendStateForProjection(RHICmdList, bProjectingForForwardShading, false);

				TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

				if (GFullResolutionDFShadowing)
				{
					TShaderMapRef<TDistanceFieldShadowingUpsamplePS<false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

					VertexShader->SetParameters(RHICmdList, View);
					PixelShader->SetParameters(RHICmdList, View, this, ScissorRect, RayTracedShadowsRT);
				}
				else
				{
					TShaderMapRef<TDistanceFieldShadowingUpsamplePS<true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

					VertexShader->SetParameters(RHICmdList, View);
					PixelShader->SetParameters(RHICmdList, View, this, ScissorRect, RayTracedShadowsRT);
				}

				DrawRectangle( 
					RHICmdList,
					0, 0, 
					ScissorRect.Width(), ScissorRect.Height(),
					ScissorRect.Min.X / GetDFShadowDownsampleFactor(), ScissorRect.Min.Y / GetDFShadowDownsampleFactor(), 
					ScissorRect.Width() / GetDFShadowDownsampleFactor(), ScissorRect.Height() / GetDFShadowDownsampleFactor(),
					FIntPoint(ScissorRect.Width(), ScissorRect.Height()),
					GetBufferSizeForDFShadows(),
					*VertexShader);
			}
		}
	}
}
