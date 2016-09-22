// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldSurfaceCacheLighting.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "DistanceFieldLightingPost.h"
#include "DistanceFieldGlobalIllumination.h"
#include "PostProcessAmbientOcclusion.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "OneColorShader.h"
#include "BasePassRendering.h"
#include "HeightfieldLighting.h"
#include "GlobalDistanceField.h"
#include "FXSystem.h"
#include "PostProcessSubsurface.h"

int32 GDistanceFieldAO = 1;
FAutoConsoleVariableRef CVarDistanceFieldAO(
	TEXT("r.DistanceFieldAO"),
	GDistanceFieldAO,
	TEXT("Whether the distance field AO feature is allowed, which is used to implement shadows of Movable sky lights from static meshes."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GDistanceFieldAOApplyToStaticIndirect = 0;
FAutoConsoleVariableRef CVarDistanceFieldAOApplyToStaticIndirect(
	TEXT("r.AOApplyToStaticIndirect"),
	GDistanceFieldAOApplyToStaticIndirect,
	TEXT("Whether to apply DFAO as indirect shadowing even to static indirect sources (lightmaps + stationary skylight + reflection captures)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GDistanceFieldAOSpecularOcclusionMode = 1;
FAutoConsoleVariableRef CVarDistanceFieldAOSpecularOcclusionMode(
	TEXT("r.AOSpecularOcclusionMode"),
	GDistanceFieldAOSpecularOcclusionMode,
	TEXT("Determines how specular should be occluded by DFAO\n")
	TEXT("0: Apply non-directional AO to specular.\n")
	TEXT("1: (default) Intersect the reflection cone with the unoccluded cone produced by DFAO.  This gives more accurate occlusion than 0, but can bring out DFAO sampling artifacts.\n")
	TEXT("2: (experimental) Cone trace through distance fields along the reflection vector.  Costs about the same as DFAO again because more cone tracing is done, but produces more accurate occlusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

bool IsDistanceFieldGIAllowed(const FViewInfo& View)
{
	return DoesPlatformSupportDistanceFieldGI(View.GetShaderPlatform())
		&& (View.Family->EngineShowFlags.VisualizeDistanceFieldGI || (View.Family->EngineShowFlags.DistanceFieldGI && GDistanceFieldGI && View.Family->EngineShowFlags.GlobalIllumination));
}

int32 GAOUseSurfaceCache = 0;
FAutoConsoleVariableRef CVarAOUseSurfaceCache(
	TEXT("r.AOUseSurfaceCache"),
	GAOUseSurfaceCache,
	TEXT("Whether to use the surface cache or screen grid for cone tracing.  The screen grid has more constant performance characteristics."),
	ECVF_RenderThreadSafe
	);

int32 GAOPowerOfTwoBetweenLevels = 2;
FAutoConsoleVariableRef CVarAOPowerOfTwoBetweenLevels(
	TEXT("r.AOPowerOfTwoBetweenLevels"),
	GAOPowerOfTwoBetweenLevels,
	TEXT("Power of two in resolution between refinement levels of the surface cache"),
	ECVF_RenderThreadSafe
	);

int32 GAOMinLevel = 1;
FAutoConsoleVariableRef CVarAOMinLevel(
	TEXT("r.AOMinLevel"),
	GAOMinLevel,
	TEXT("Smallest downsample power of 4 to use for surface cache population.\n")
	TEXT("The default is 1, which means every 8 full resolution pixels (BaseDownsampleFactor(2) * 4^1) will be checked for a valid interpolation from the cache or shaded.\n")
	TEXT("Going down to 0 gives less aliasing, and removes the need for gap filling, but costs a lot."),
	ECVF_RenderThreadSafe
	);

int32 GAOMaxLevel = FMath::Min(2, GAOMaxSupportedLevel);
FAutoConsoleVariableRef CVarAOMaxLevel(
	TEXT("r.AOMaxLevel"),
	GAOMaxLevel,
	TEXT("Largest downsample power of 4 to use for surface cache population."),
	ECVF_RenderThreadSafe
	);

int32 GAOReuseAcrossFrames = 1;
FAutoConsoleVariableRef CVarAOReuseAcrossFrames(
	TEXT("r.AOReuseAcrossFrames"),
	GAOReuseAcrossFrames,
	TEXT("Whether to allow reusing surface cache results across frames."),
	ECVF_RenderThreadSafe
	);

float GAOTrimOldRecordsFraction = .2f;
FAutoConsoleVariableRef CVarAOTrimOldRecordsFraction(
	TEXT("r.AOTrimOldRecordsFraction"),
	GAOTrimOldRecordsFraction,
	TEXT("When r.AOReuseAcrossFrames is enabled, this is the fraction of the last frame's surface cache records that will not be reused.\n")
	TEXT("Low settings provide better performance, while values closer to 1 give faster lighting updates when dynamic scene changes are happening."),
	ECVF_RenderThreadSafe
	);

float GAORecordRadiusScale = .3f;
FAutoConsoleVariableRef CVarAORecordRadiusScale(
	TEXT("r.AORecordRadiusScale"),
	GAORecordRadiusScale,
	TEXT("Scale applied to the minimum occluder distance to produce the record radius.  This effectively controls how dense shading samples are."),
	ECVF_RenderThreadSafe
	);

float GAOInterpolationRadiusScale = 1.3f;
FAutoConsoleVariableRef CVarAOInterpolationRadiusScale(
	TEXT("r.AOInterpolationRadiusScale"),
	GAOInterpolationRadiusScale,
	TEXT("Scale applied to record radii during the final interpolation pass.  Values larger than 1 result in world space smoothing."),
	ECVF_RenderThreadSafe
	);

float GAOMinPointBehindPlaneAngle = 4;
FAutoConsoleVariableRef CVarAOMinPointBehindPlaneAngle(
	TEXT("r.AOMinPointBehindPlaneAngle"),
	GAOMinPointBehindPlaneAngle,
	TEXT("Minimum angle that a point can lie behind a record and still be considered valid.\n")
	TEXT("This threshold helps reduce leaking that happens when interpolating records in front of the shading point, ignoring occluders in between."),
	ECVF_RenderThreadSafe
	);

float GAOInterpolationMaxAngle = 15;
FAutoConsoleVariableRef CVarAOInterpolationMaxAngle(
	TEXT("r.AOInterpolationMaxAngle"),
	GAOInterpolationMaxAngle,
	TEXT("Largest angle allowed between the shading point's normal and a nearby record's normal."),
	ECVF_RenderThreadSafe
	);

float GAOInterpolationAngleScale = 1.5f;
FAutoConsoleVariableRef CVarAOInterpolationAngleScale(
	TEXT("r.AOInterpolationAngleScale"),
	GAOInterpolationAngleScale,
	TEXT("Scale applied to angle error during the final interpolation pass.  Values larger than 1 result in smoothing."),
	ECVF_RenderThreadSafe
	);

float GAOStepExponentScale = .5f;
FAutoConsoleVariableRef CVarAOStepExponentScale(
	TEXT("r.AOStepExponentScale"),
	GAOStepExponentScale,
	TEXT("Exponent used to distribute AO samples along a cone direction."),
	ECVF_RenderThreadSafe
	);

float GAOMaxViewDistance = 20000;
FAutoConsoleVariableRef CVarAOMaxViewDistance(
	TEXT("r.AOMaxViewDistance"),
	GAOMaxViewDistance,
	TEXT("The maximum distance that AO will be computed at."),
	ECVF_RenderThreadSafe
	);

int32 GAOScatterTileCulling = 1;
FAutoConsoleVariableRef CVarAOScatterTileCulling(
	TEXT("r.AOScatterTileCulling"),
	GAOScatterTileCulling,
	TEXT("Whether to use the rasterizer for binning occluder objects into screenspace tiles."),
	ECVF_RenderThreadSafe
	);

int32 GAOComputeShaderNormalCalculation = 0;
FAutoConsoleVariableRef CVarAOComputeShaderNormalCalculation(
	TEXT("r.AOComputeShaderNormalCalculation"),
	GAOComputeShaderNormalCalculation,
	TEXT("Whether to use the compute shader version of the distance field normal computation."),
	ECVF_RenderThreadSafe
	);

int32 GAOSampleSet = 1;
FAutoConsoleVariableRef CVarAOSampleSet(
	TEXT("r.AOSampleSet"),
	GAOSampleSet,
	TEXT("0 = Original set, 1 = Relaxed set"),
	ECVF_RenderThreadSafe
	);

int32 GAOInterpolationStencilTesting = 1;
FAutoConsoleVariableRef CVarAOInterpolationStencilTesting(
	TEXT("r.AOInterpolationStencilTesting"),
	GAOInterpolationStencilTesting,
	TEXT("Whether to stencil out distant pixels from the interpolation splat pass, useful for debugging"),
	ECVF_RenderThreadSafe
	);

int32 GAOInterpolationDepthTesting = 1;
FAutoConsoleVariableRef CVarAOInterpolationDepthTesting(
	TEXT("r.AOInterpolationDepthTesting"),
	GAOInterpolationDepthTesting,
	TEXT("Whether to use depth testing during the interpolation splat pass, useful for debugging"),
	ECVF_RenderThreadSafe
	);

int32 GAOOverwriteSceneColor = 0;
FAutoConsoleVariableRef CVarAOOverwriteSceneColor(
	TEXT("r.AOOverwriteSceneColor"),
	GAOOverwriteSceneColor,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

DEFINE_LOG_CATEGORY(LogDistanceField);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FAOSampleData2,TEXT("AOSamples2"));

FDistanceFieldAOParameters::FDistanceFieldAOParameters(float InOcclusionMaxDistance, float InContrast)
{
	Contrast = FMath::Clamp(InContrast, .01f, 2.0f);
	InOcclusionMaxDistance = FMath::Clamp(InOcclusionMaxDistance, 2.0f, 3000.0f);

	if (GAOGlobalDistanceField != 0 && GAOUseSurfaceCache == 0)
	{
		extern float GAOGlobalDFStartDistance;
		ObjectMaxOcclusionDistance = FMath::Min(InOcclusionMaxDistance, GAOGlobalDFStartDistance);
		GlobalMaxOcclusionDistance = InOcclusionMaxDistance >= GAOGlobalDFStartDistance ? InOcclusionMaxDistance : 0;
	}
	else
	{
		ObjectMaxOcclusionDistance = InOcclusionMaxDistance;
		GlobalMaxOcclusionDistance = 0;
	}
}

FIntPoint GetBufferSizeForAO()
{
	return FIntPoint::DivideAndRoundDown(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), GAODownsampleFactor);
}

// Sample set restricted to not self-intersect a surface based on cone angle .475882232
// Coverage of hemisphere = 0.755312979
const FVector SpacedVectors9[] = 
{
	FVector(-0.573257625, 0.625250816, 0.529563010),
	FVector(0.253354192, -0.840093017, 0.479640961),
	FVector(-0.421664953, -0.718063235, 0.553700149),
	FVector(0.249163717, 0.796005428, 0.551627457),
	FVector(0.375082791, 0.295851320, 0.878512800),
	FVector(-0.217619032, 0.00193520682, 0.976031899),
	FVector(-0.852834642, 0.0111727007, 0.522061586),
	FVector(0.745701790, 0.239393353, 0.621787369),
	FVector(-0.151036426, -0.465937436, 0.871831656)
};

// Generated from SpacedVectors9 by applying repulsion forces until convergence
const FVector RelaxedSpacedVectors9[] = 
{
	FVector(-0.467612, 0.739424, 0.484347),
	FVector(0.517459, -0.705440, 0.484346),
	FVector(-0.419848, -0.767551, 0.484347),
	FVector(0.343077, 0.804802, 0.484347),
	FVector(0.364239, 0.244290, 0.898695),
	FVector(-0.381547, 0.185815, 0.905481),
	FVector(-0.870176, -0.090559, 0.484347),
	FVector(0.874448, 0.027390, 0.484346),
	FVector(0.032967, -0.435625, 0.899524)
};

void GetSpacedVectors(TArray<FVector, TInlineAllocator<9> >& OutVectors)
{
	OutVectors.Empty(ARRAY_COUNT(SpacedVectors9));

	if (GAOSampleSet == 0)
	{
		for (int32 i = 0; i < ARRAY_COUNT(SpacedVectors9); i++)
		{
			OutVectors.Add(SpacedVectors9[i]);
		}
	}
	else
	{
		for (int32 i = 0; i < ARRAY_COUNT(RelaxedSpacedVectors9); i++)
		{
			OutVectors.Add(RelaxedSpacedVectors9[i]);
		}
	}
}

// Cone half angle derived from each cone covering an equal solid angle
float GAOConeHalfAngle = FMath::Acos(1 - 1.0f / (float)ARRAY_COUNT(SpacedVectors9));

// Number of AO sample positions along each cone
// Must match shader code
uint32 GAONumConeSteps = 10;

class FCircleVertexBuffer : public FVertexBuffer
{
public:

	int32 NumSections;

	FCircleVertexBuffer()
	{
		NumSections = 8;
	}

	virtual void InitRHI() override
	{
		// Used as a non-indexed triangle list, so 3 vertices per triangle
		const uint32 Size = 3 * NumSections * sizeof(FScreenVertex);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, Buffer);
		FScreenVertex* DestVertex = (FScreenVertex*)Buffer;

		const float RadiansPerRingSegment = PI / (float)NumSections;

		// Boost the effective radius so that the edges of the circle approximation lie on the circle, instead of the vertices
		const float Radius = 1.0f / FMath::Cos(RadiansPerRingSegment);

		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			float Fraction = SectionIndex / (float)NumSections;
			float CurrentAngle = Fraction * 2 * PI;
			float NextAngle = ((SectionIndex + 1) / (float)NumSections) * 2 * PI;
			FVector2D CurrentPosition(Radius * FMath::Cos(CurrentAngle), Radius * FMath::Sin(CurrentAngle));
			FVector2D NextPosition(Radius * FMath::Cos(NextAngle), Radius * FMath::Sin(NextAngle));

			DestVertex[SectionIndex * 3 + 0].Position = FVector2D(0, 0);
			DestVertex[SectionIndex * 3 + 0].UV = CurrentPosition;
			DestVertex[SectionIndex * 3 + 1].Position = FVector2D(0, 0);
			DestVertex[SectionIndex * 3 + 1].UV = NextPosition;
			DestVertex[SectionIndex * 3 + 2].Position = FVector2D(0, 0);
			DestVertex[SectionIndex * 3 + 2].UV = FVector2D(.5f, .5f);
		}

		RHIUnlockVertexBuffer(VertexBufferRHI);      
	}
};

TGlobalResource<FCircleVertexBuffer> GCircleVertexBuffer;
TGlobalResource<FDistanceFieldObjectBufferResource> GAOCulledObjectBuffers;
TGlobalResource<FTemporaryIrradianceCacheResources> GTemporaryIrradianceCacheResources;

void FTileIntersectionResources::InitDynamicRHI()
{
	TileConeAxisAndCos.Initialize(sizeof(float)* 4, TileDimensions.X * TileDimensions.Y, PF_A32B32G32R32F, BUF_Static);
	TileConeDepthRanges.Initialize(sizeof(float)* 4, TileDimensions.X * TileDimensions.Y, PF_A32B32G32R32F, BUF_Static);

	TileHeadDataUnpacked.Initialize(sizeof(uint32), TileDimensions.X * TileDimensions.Y * 4, PF_R32_UINT, BUF_Static);

	//@todo - handle max exceeded
	TileArrayData.Initialize(sizeof(uint32), GMaxNumObjectsPerTile * TileDimensions.X * TileDimensions.Y * 3, PF_R32_UINT, BUF_Static);
	TileArrayNextAllocation.Initialize(sizeof(uint32), 1, PF_R32_UINT, BUF_Static);
}

void OnClearSurfaceCache(UWorld* InWorld)
{
	FlushRenderingCommands();

	FScene* Scene = (FScene*)InWorld->Scene;

	if (Scene && Scene->SurfaceCacheResources)
	{
		Scene->SurfaceCacheResources->bClearedResources = false;
	}
}

FAutoConsoleCommandWithWorld ClearCacheConsoleCommand(
	TEXT("r.AOClearCache"),
	TEXT(""),
	FConsoleCommandWithWorldDelegate::CreateStatic(OnClearSurfaceCache)
	);

bool bListMemoryNextFrame = false;

void OnListMemory(UWorld* InWorld)
{
	bListMemoryNextFrame = true;
}

FAutoConsoleCommandWithWorld ListMemoryConsoleCommand(
	TEXT("r.AOListMemory"),
	TEXT(""),
	FConsoleCommandWithWorldDelegate::CreateStatic(OnListMemory)
	);

class FCullObjectsForViewCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullObjectsForViewCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
	}

	FCullObjectsForViewCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
		ObjectIndirectArguments.Bind(Initializer.ParameterMap, TEXT("ObjectIndirectArguments"));
		CulledObjectBounds.Bind(Initializer.ParameterMap, TEXT("CulledObjectBounds"));
		CulledObjectData.Bind(Initializer.ParameterMap, TEXT("CulledObjectData"));
		CulledObjectBoxBounds.Bind(Initializer.ParameterMap, TEXT("CulledObjectBoxBounds"));
		AOParameters.Bind(Initializer.ParameterMap);
		NumConvexHullPlanes.Bind(Initializer.ParameterMap, TEXT("NumConvexHullPlanes"));
		ViewFrustumConvexHull.Bind(Initializer.ParameterMap, TEXT("ViewFrustumConvexHull"));
		ObjectBoundingGeometryIndexCount.Bind(Initializer.ParameterMap, TEXT("ObjectBoundingGeometryIndexCount"));
	}

	FCullObjectsForViewCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, const FSceneView& View, const FDistanceFieldAOParameters& Parameters)
	{
		FUnorderedAccessViewRHIParamRef OutUAVs[6];
		OutUAVs[0] = GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GAOCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GAOCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GAOCulledObjectBuffers.Buffers.BoxBounds.UAV;
		OutUAVs[4] = Scene->DistanceFieldSceneData.ObjectBuffers->Data.UAV;
		OutUAVs[5] = Scene->DistanceFieldSceneData.ObjectBuffers->Bounds.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, *(Scene->DistanceFieldSceneData.ObjectBuffers), Scene->DistanceFieldSceneData.NumObjectsInBuffer);

		

		ObjectIndirectArguments.SetBuffer(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments);
		CulledObjectBounds.SetBuffer(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers.Bounds);
		CulledObjectData.SetBuffer(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers.Data);
		CulledObjectBoxBounds.SetBuffer(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers.BoxBounds);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		// Shader assumes max 6
		check(View.ViewFrustum.Planes.Num() <= 6);
		SetShaderValue(RHICmdList, ShaderRHI, NumConvexHullPlanes, View.ViewFrustum.Planes.Num());
		SetShaderValueArray(RHICmdList, ShaderRHI, ViewFrustumConvexHull, View.ViewFrustum.Planes.GetData(), View.ViewFrustum.Planes.Num());
		SetShaderValue(RHICmdList, ShaderRHI, ObjectBoundingGeometryIndexCount, StencilingGeometry::GLowPolyStencilSphereIndexBuffer.GetIndexCount());
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FScene* Scene)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), *(Scene->DistanceFieldSceneData.ObjectBuffers));
		ObjectIndirectArguments.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBounds.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectData.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBoxBounds.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[6];
		OutUAVs[0] = GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GAOCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GAOCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GAOCulledObjectBuffers.Buffers.BoxBounds.UAV;
		OutUAVs[4] = Scene->DistanceFieldSceneData.ObjectBuffers->Data.UAV;
		OutUAVs[5] = Scene->DistanceFieldSceneData.ObjectBuffers->Bounds.UAV;		
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectBufferParameters;
		Ar << ObjectIndirectArguments;
		Ar << CulledObjectBounds;
		Ar << CulledObjectData;
		Ar << CulledObjectBoxBounds;
		Ar << AOParameters;
		Ar << NumConvexHullPlanes;
		Ar << ViewFrustumConvexHull;
		Ar << ObjectBoundingGeometryIndexCount;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
	FRWShaderParameter ObjectIndirectArguments;
	FRWShaderParameter CulledObjectBounds;
	FRWShaderParameter CulledObjectData;
	FRWShaderParameter CulledObjectBoxBounds;
	FAOParameters AOParameters;
	FShaderParameter NumConvexHullPlanes;
	FShaderParameter ViewFrustumConvexHull;
	FShaderParameter ObjectBoundingGeometryIndexCount;
};

IMPLEMENT_SHADER_TYPE(,FCullObjectsForViewCS,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("CullObjectsForViewCS"),SF_Compute);

/**  */
class FBuildTileConesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FBuildTileConesCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		
		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FBuildTileConesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		TileConeAxisAndCos.Bind(Initializer.ParameterMap, TEXT("TileConeAxisAndCos"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		TileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("TileHeadDataUnpacked"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		ViewDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
	}

	FBuildTileConesCS()
	{
	}
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FSceneRenderTargetItem& DistanceFieldNormal, FScene* Scene, FVector2D NumGroupsValue, const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[3];
		OutUAVs[0] = TileIntersectionResources->TileConeAxisAndCos.UAV;
		OutUAVs[1] = TileIntersectionResources->TileConeDepthRanges.UAV;
		OutUAVs[2] = TileIntersectionResources->TileHeadDataUnpacked.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		TileConeAxisAndCos.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileConeAxisAndCos);
		TileConeDepthRanges.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileConeDepthRanges);
		TileHeadDataUnpacked.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileHeadDataUnpacked);

		SetShaderValue(RHICmdList, ShaderRHI, ViewDimensionsParameter, View.ViewRect);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		TileConeAxisAndCos.UnsetUAV(RHICmdList, GetComputeShader());
		TileConeDepthRanges.UnsetUAV(RHICmdList, GetComputeShader());
		TileHeadDataUnpacked.UnsetUAV(RHICmdList, GetComputeShader());

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[3];
		OutUAVs[0] = TileIntersectionResources->TileConeAxisAndCos.UAV;
		OutUAVs[1] = TileIntersectionResources->TileConeDepthRanges.UAV;
		OutUAVs[2] = TileIntersectionResources->TileHeadDataUnpacked.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << AOParameters;
		Ar << TileConeAxisAndCos;
		Ar << TileConeDepthRanges;
		Ar << TileHeadDataUnpacked;
		Ar << NumGroups;
		Ar << ViewDimensionsParameter;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
	FRWShaderParameter TileConeAxisAndCos;
	FRWShaderParameter TileConeDepthRanges;
	FRWShaderParameter TileHeadDataUnpacked;
	FShaderParameter ViewDimensionsParameter;
	FShaderParameter NumGroups;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
};

IMPLEMENT_SHADER_TYPE(,FBuildTileConesCS,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("BuildTileConesMain"),SF_Compute);


/**  */
class FObjectCullVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FObjectCullVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform); 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FObjectCullVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		ConservativeRadiusScale.Bind(Initializer.ParameterMap, TEXT("ConservativeRadiusScale"));
	}

	FObjectCullVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FDistanceFieldAOParameters& Parameters)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		const int32 NumRings = StencilingGeometry::GLowPolyStencilSphereVertexBuffer.GetNumRings();
		const float RadiansPerRingSegment = PI / (float)NumRings;

		// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
		const float ConservativeRadiusScaleValue = 1.0f / FMath::Cos(RadiansPerRingSegment);

		SetShaderValue(RHICmdList, ShaderRHI, ConservativeRadiusScale, ConservativeRadiusScaleValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << ConservativeRadiusScale;
		return bShaderHasOutdatedParameters;
	}

private:
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FShaderParameter ConservativeRadiusScale;
};

IMPLEMENT_SHADER_TYPE(,FObjectCullVS,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("ObjectCullVS"),SF_Vertex);

class FObjectCullPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FObjectCullPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform) && RHISupportsPixelShaderUAVs(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
	}

	/** Default constructor. */
	FObjectCullPS() {}

	/** Initialization constructor. */
	FObjectCullPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		TileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("TileHeadDataUnpacked"));
		TileArrayData.Bind(Initializer.ParameterMap, TEXT("TileArrayData"));
		TileConeAxisAndCos.Bind(Initializer.ParameterMap, TEXT("TileConeAxisAndCos"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FVector2D NumGroupsValue, const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		SetSRVParameter(RHICmdList, ShaderRHI, TileConeAxisAndCos, TileIntersectionResources->TileConeAxisAndCos.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void GetUAVs(const FSceneView& View, TArray<FUnorderedAccessViewRHIParamRef>& UAVs)
	{
		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		int32 MaxIndex = FMath::Max(TileHeadDataUnpacked.GetUAVIndex(), TileArrayData.GetUAVIndex());
		UAVs.AddZeroed(MaxIndex + 1);

		if (TileHeadDataUnpacked.IsBound())
		{
			UAVs[TileHeadDataUnpacked.GetUAVIndex()] = TileIntersectionResources->TileHeadDataUnpacked.UAV;
		}

		if (TileArrayData.IsBound())
		{
			UAVs[TileArrayData.GetUAVIndex()] = TileIntersectionResources->TileArrayData.UAV;
		}

		check(UAVs.Num() > 0);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << TileHeadDataUnpacked;
		Ar << TileArrayData;
		Ar << TileConeAxisAndCos;
		Ar << TileConeDepthRanges;
		Ar << NumGroups;
		return bShaderHasOutdatedParameters;
	}

private:
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FRWShaderParameter TileHeadDataUnpacked;
	FRWShaderParameter TileArrayData;
	FShaderResourceParameter TileConeAxisAndCos;
	FShaderResourceParameter TileConeDepthRanges;
	FShaderParameter NumGroups;
};

IMPLEMENT_SHADER_TYPE(,FObjectCullPS,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("ObjectCullPS"),SF_Pixel);


/**  */
class FDistanceFieldBuildTileListCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDistanceFieldBuildTileListCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FDistanceFieldBuildTileListCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		TileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("TileHeadDataUnpacked"));
		TileArrayData.Bind(Initializer.ParameterMap, TEXT("TileArrayData"));
		TileArrayNextAllocation.Bind(Initializer.ParameterMap, TEXT("TileArrayNextAllocation"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		ViewDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
	}

	FDistanceFieldBuildTileListCS()
	{
	}
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FScene* Scene, FVector2D NumGroupsValue, const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);


		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[3];
		OutUAVs[0] = TileIntersectionResources->TileHeadDataUnpacked.UAV;
		OutUAVs[1] = TileIntersectionResources->TileArrayData.UAV;
		OutUAVs[2] = TileIntersectionResources->TileArrayNextAllocation.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		TileHeadDataUnpacked.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileHeadDataUnpacked);
		TileArrayData.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileArrayData);
		TileArrayNextAllocation.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileArrayNextAllocation);

		SetShaderValue(RHICmdList, ShaderRHI, ViewDimensionsParameter, View.ViewRect);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		TileHeadDataUnpacked.UnsetUAV(RHICmdList, GetComputeShader());
		TileArrayData.UnsetUAV(RHICmdList, GetComputeShader());
		TileArrayNextAllocation.UnsetUAV(RHICmdList, GetComputeShader());

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[3];
		OutUAVs[0] = TileIntersectionResources->TileHeadDataUnpacked.UAV;
		OutUAVs[1] = TileIntersectionResources->TileArrayData.UAV;
		OutUAVs[2] = TileIntersectionResources->TileArrayNextAllocation.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << TileHeadDataUnpacked;
		Ar << TileArrayData;
		Ar << TileArrayNextAllocation;
		Ar << NumGroups;
		Ar << ViewDimensionsParameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FRWShaderParameter TileHeadDataUnpacked;
	FRWShaderParameter TileArrayData;
	FRWShaderParameter TileArrayNextAllocation;
	FShaderParameter ViewDimensionsParameter;
	FShaderParameter NumGroups;
};

IMPLEMENT_SHADER_TYPE(,FDistanceFieldBuildTileListCS,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("DistanceFieldAOBuildTileListMain"),SF_Compute);

class FComputeDistanceFieldNormalPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeDistanceFieldNormalPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	/** Default constructor. */
	FComputeDistanceFieldNormalPS() {}

	/** Initialization constructor. */
	FComputeDistanceFieldNormalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << AOParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
};

IMPLEMENT_SHADER_TYPE(,FComputeDistanceFieldNormalPS,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("ComputeDistanceFieldNormalPS"),SF_Pixel);


class FComputeDistanceFieldNormalCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FComputeDistanceFieldNormalCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
	}

	/** Default constructor. */
	FComputeDistanceFieldNormalCS() {}

	/** Initialization constructor. */
	FComputeDistanceFieldNormalCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DistanceFieldNormal.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormal"));
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FSceneRenderTargetItem& DistanceFieldNormalValue, const FDistanceFieldAOParameters& Parameters)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, DistanceFieldNormalValue.UAV);
		DistanceFieldNormal.SetTexture(RHICmdList, ShaderRHI, DistanceFieldNormalValue.ShaderResourceTexture, DistanceFieldNormalValue.UAV);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& DistanceFieldNormalValue)
	{
		DistanceFieldNormal.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, DistanceFieldNormalValue.UAV);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DistanceFieldNormal;
		Ar << DeferredParameters;
		Ar << AOParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter DistanceFieldNormal;
	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
};

IMPLEMENT_SHADER_TYPE(,FComputeDistanceFieldNormalCS,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("ComputeDistanceFieldNormalCS"),SF_Compute);

void ComputeDistanceFieldNormal(FRHICommandListImmediate& RHICmdList, const TArray<FViewInfo>& Views, FSceneRenderTargetItem& DistanceFieldNormal, const FDistanceFieldAOParameters& Parameters)
{
	if (GAOComputeShaderNormalCalculation)
	{
		SetRenderTarget(RHICmdList, NULL, NULL);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			uint32 GroupSizeX = FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor, GDistanceFieldAOTileSizeX);
			uint32 GroupSizeY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor, GDistanceFieldAOTileSizeY);

			{
				SCOPED_DRAW_EVENT(RHICmdList, ComputeNormalCS);
				TShaderMapRef<FComputeDistanceFieldNormalCS> ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal, Parameters);
				DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

				ComputeShader->UnsetParameters(RHICmdList, DistanceFieldNormal);
			}
		}
	}
	else
	{
		SetRenderTarget(RHICmdList, DistanceFieldNormal.TargetableTexture, NULL, true);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			SCOPED_DRAW_EVENT(RHICmdList, ComputeNormal);

			RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FComputeDistanceFieldNormalPS> PixelShader(View.ShaderMap);

			static FGlobalBoundShaderState BoundShaderState;

			SetGlobalBoundShaderState(RHICmdList, View.FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

			PixelShader->SetParameters(RHICmdList, View, Parameters);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
				FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY(),
				*VertexShader);
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, DistanceFieldNormal.TargetableTexture);
	}
}

class FSetupCopyIndirectArgumentsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSetupCopyIndirectArgumentsCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FSetupCopyIndirectArgumentsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DrawParameters.Bind(Initializer.ParameterMap, TEXT("DrawParameters"));
		DispatchParameters.Bind(Initializer.ParameterMap, TEXT("DispatchParameters"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters")); 
		TrimFraction.Bind(Initializer.ParameterMap, TEXT("TrimFraction"));
	}

	FSetupCopyIndirectArgumentsCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, DrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = SurfaceCacheResources.DispatchParameters.UAV;
		OutUAVs[1] = SurfaceCacheResources.TempResources->ScatterDrawParameters.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		DispatchParameters.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.DispatchParameters);
		ScatterDrawParameters.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->ScatterDrawParameters);

		SetShaderValue(RHICmdList, ShaderRHI, TrimFraction, GAOTrimOldRecordsFraction);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		DispatchParameters.UnsetUAV(RHICmdList, GetComputeShader());
		ScatterDrawParameters.UnsetUAV(RHICmdList, GetComputeShader());

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = SurfaceCacheResources.DispatchParameters.UAV;
		OutUAVs[1] = SurfaceCacheResources.TempResources->ScatterDrawParameters.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DrawParameters;
		Ar << DispatchParameters;
		Ar << ScatterDrawParameters;
		Ar << TrimFraction;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter DrawParameters;
	FRWShaderParameter DispatchParameters;
	FRWShaderParameter ScatterDrawParameters;
	FShaderParameter TrimFraction;
};

IMPLEMENT_SHADER_TYPE(,FSetupCopyIndirectArgumentsCS,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("SetupCopyIndirectArgumentsCS"),SF_Compute);

template<bool bSupportIrradiance>
class TCopyIrradianceCacheSamplesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TCopyIrradianceCacheSamplesCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);
	}

	TCopyIrradianceCacheSamplesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CacheParameters.Bind(Initializer.ParameterMap);
		DrawParameters.Bind(Initializer.ParameterMap, TEXT("DrawParameters"));
		CopyIrradianceCachePositionRadius.Bind(Initializer.ParameterMap, TEXT("CopyIrradianceCachePositionRadius"));
		CopyIrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("CopyIrradianceCacheNormal"));
		CopyOccluderRadius.Bind(Initializer.ParameterMap, TEXT("CopyOccluderRadius"));
		CopyIrradianceCacheBentNormal.Bind(Initializer.ParameterMap, TEXT("CopyIrradianceCacheBentNormal"));
		CopyIrradianceCacheIrradiance.Bind(Initializer.ParameterMap, TEXT("CopyIrradianceCacheIrradiance"));
		CopyIrradianceCacheTileCoordinate.Bind(Initializer.ParameterMap, TEXT("CopyIrradianceCacheTileCoordinate"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		TrimFraction.Bind(Initializer.ParameterMap, TEXT("TrimFraction"));
	}

	TCopyIrradianceCacheSamplesCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		CacheParameters.Set(RHICmdList, ShaderRHI, *SurfaceCacheResources.Level[DepthLevel]);
		SetSRVParameter(RHICmdList, ShaderRHI, DrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);

		FUnorderedAccessViewRHIParamRef OutUAVs[7];
		OutUAVs[0] = SurfaceCacheResources.TempResources->PositionAndRadius.UAV;
		OutUAVs[1] = SurfaceCacheResources.TempResources->Normal.UAV;
		OutUAVs[2] = SurfaceCacheResources.TempResources->OccluderRadius.UAV;
		OutUAVs[3] = SurfaceCacheResources.TempResources->BentNormal.UAV;
		OutUAVs[4] = SurfaceCacheResources.TempResources->Irradiance.UAV;
		OutUAVs[5] = SurfaceCacheResources.TempResources->TileCoordinate.UAV;
		OutUAVs[6] = SurfaceCacheResources.TempResources->ScatterDrawParameters.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		CopyIrradianceCachePositionRadius.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->PositionAndRadius);
		CopyIrradianceCacheNormal.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->Normal);
		CopyOccluderRadius.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->OccluderRadius);
		CopyIrradianceCacheBentNormal.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->BentNormal);
		CopyIrradianceCacheIrradiance.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->Irradiance);
		CopyIrradianceCacheTileCoordinate.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->TileCoordinate);
		ScatterDrawParameters.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->ScatterDrawParameters);

		SetShaderValue(RHICmdList, ShaderRHI, TrimFraction, GAOTrimOldRecordsFraction);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		CopyIrradianceCachePositionRadius.UnsetUAV(RHICmdList, GetComputeShader());
		CopyIrradianceCacheNormal.UnsetUAV(RHICmdList, GetComputeShader());
		CopyOccluderRadius.UnsetUAV(RHICmdList, GetComputeShader());
		CopyIrradianceCacheBentNormal.UnsetUAV(RHICmdList, GetComputeShader());
		CopyIrradianceCacheIrradiance.UnsetUAV(RHICmdList, GetComputeShader());
		CopyIrradianceCacheTileCoordinate.UnsetUAV(RHICmdList, GetComputeShader());
		ScatterDrawParameters.UnsetUAV(RHICmdList, GetComputeShader());

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[7];
		OutUAVs[0] = SurfaceCacheResources.TempResources->PositionAndRadius.UAV;
		OutUAVs[1] = SurfaceCacheResources.TempResources->Normal.UAV;
		OutUAVs[2] = SurfaceCacheResources.TempResources->OccluderRadius.UAV;
		OutUAVs[3] = SurfaceCacheResources.TempResources->BentNormal.UAV;
		OutUAVs[4] = SurfaceCacheResources.TempResources->Irradiance.UAV;
		OutUAVs[5] = SurfaceCacheResources.TempResources->TileCoordinate.UAV;
		OutUAVs[6] = SurfaceCacheResources.TempResources->ScatterDrawParameters.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CacheParameters;
		Ar << DrawParameters;
		Ar << CopyIrradianceCachePositionRadius;
		Ar << CopyIrradianceCacheNormal;
		Ar << CopyOccluderRadius;
		Ar << CopyIrradianceCacheBentNormal;
		Ar << CopyIrradianceCacheIrradiance;
		Ar << CopyIrradianceCacheTileCoordinate;
		Ar << ScatterDrawParameters;
		Ar << TrimFraction;
		return bShaderHasOutdatedParameters;
	}

private:

	FIrradianceCacheParameters CacheParameters;
	FShaderResourceParameter DrawParameters;
	FRWShaderParameter CopyIrradianceCachePositionRadius;
	FRWShaderParameter CopyIrradianceCacheNormal;
	FRWShaderParameter CopyOccluderRadius;
	FRWShaderParameter CopyIrradianceCacheBentNormal;
	FRWShaderParameter CopyIrradianceCacheIrradiance;
	FRWShaderParameter CopyIrradianceCacheTileCoordinate;
	FRWShaderParameter ScatterDrawParameters;
	FShaderParameter TrimFraction;
};

IMPLEMENT_SHADER_TYPE(template<>,TCopyIrradianceCacheSamplesCS<true>,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("CopyIrradianceCacheSamplesCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TCopyIrradianceCacheSamplesCS<false>,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("CopyIrradianceCacheSamplesCS"),SF_Compute);

class FSaveStartIndexCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FSaveStartIndexCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	FSaveStartIndexCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DrawParameters.Bind(Initializer.ParameterMap, TEXT("DrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
	}

	FSaveStartIndexCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, DrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.UAV);
		SavedStartIndex.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		SavedStartIndex.UnsetUAV(RHICmdList, GetComputeShader());

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.UAV);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DrawParameters;
		Ar << SavedStartIndex;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter DrawParameters;
	FRWShaderParameter SavedStartIndex;
};

IMPLEMENT_SHADER_TYPE(,FSaveStartIndexCS,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("SaveStartIndexCS"),SF_Compute);

class FPopulateCacheCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPopulateCacheCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FPopulateCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		IrradianceCachePositionRadius.Bind(Initializer.ParameterMap, TEXT("IrradianceCachePositionRadius"));
		IrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheNormal"));
		IrradianceCacheTileCoordinate.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheTileCoordinate"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		ViewDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
		ThreadToCulledTile.Bind(Initializer.ParameterMap, TEXT("ThreadToCulledTile"));
		TileListGroupSize.Bind(Initializer.ParameterMap, TEXT("TileListGroupSize"));
		NumCircleSections.Bind(Initializer.ParameterMap, TEXT("NumCircleSections"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
		IrradianceCacheSplatTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheSplatTexture"));
		IrradianceCacheSplatSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheSplatSampler"));
		AOLevelParameters.Bind(Initializer.ParameterMap);
	}

	FPopulateCacheCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& DistanceFieldAOIrradianceCacheSplat, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		int32 DownsampleFactorValue, 
		int32 DepthLevel, 
		FIntPoint TileListGroupSizeValue, 
		const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		AOLevelParameters.Set(RHICmdList, ShaderRHI, View, DownsampleFactorValue);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[0] = SurfaceCacheResources.Level[DepthLevel]->PositionAndRadius.UAV;
		OutUAVs[1] = SurfaceCacheResources.Level[DepthLevel]->Normal.UAV;
		OutUAVs[2] = SurfaceCacheResources.Level[DepthLevel]->TileCoordinate.UAV;
		OutUAVs[3] = SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		IrradianceCachePositionRadius.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->PositionAndRadius);
		IrradianceCacheNormal.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->Normal);
		IrradianceCacheTileCoordinate.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->TileCoordinate);
		ScatterDrawParameters.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters);

		SetShaderValue(RHICmdList, ShaderRHI, ViewDimensionsParameter, View.ViewRect);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		FVector2D ThreadToCulledTileValue(DownsampleFactorValue / (float)(GAODownsampleFactor * GDistanceFieldAOTileSizeX), DownsampleFactorValue / (float)(GAODownsampleFactor * GDistanceFieldAOTileSizeY));
		SetShaderValue(RHICmdList, ShaderRHI, ThreadToCulledTile, ThreadToCulledTileValue);

		SetShaderValue(RHICmdList, ShaderRHI, TileListGroupSize, TileListGroupSizeValue);

		SetShaderValue(RHICmdList, ShaderRHI, NumCircleSections, GCircleVertexBuffer.NumSections);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			IrradianceCacheSplatTexture,
			IrradianceCacheSplatSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldAOIrradianceCacheSplat.ShaderResourceTexture
			);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		IrradianceCachePositionRadius.UnsetUAV(RHICmdList, GetComputeShader());
		IrradianceCacheNormal.UnsetUAV(RHICmdList, GetComputeShader());
		IrradianceCacheTileCoordinate.UnsetUAV(RHICmdList, GetComputeShader());
		ScatterDrawParameters.UnsetUAV(RHICmdList, GetComputeShader());

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[0] = SurfaceCacheResources.Level[DepthLevel]->PositionAndRadius.UAV;
		OutUAVs[1] = SurfaceCacheResources.Level[DepthLevel]->Normal.UAV;
		OutUAVs[2] = SurfaceCacheResources.Level[DepthLevel]->TileCoordinate.UAV;
		OutUAVs[3] = SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << AOParameters;
		Ar << AOLevelParameters;
		Ar << IrradianceCachePositionRadius;
		Ar << IrradianceCacheNormal;
		Ar << IrradianceCacheTileCoordinate;
		Ar << ScatterDrawParameters;
		Ar << ViewDimensionsParameter;
		Ar << ThreadToCulledTile;
		Ar << TileListGroupSize;
		Ar << NumCircleSections;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		Ar << IrradianceCacheSplatTexture;
		Ar << IrradianceCacheSplatSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
	FAOLevelParameters AOLevelParameters;
	FRWShaderParameter IrradianceCachePositionRadius;
	FRWShaderParameter IrradianceCacheNormal;
	FRWShaderParameter IrradianceCacheTileCoordinate;
	FRWShaderParameter ScatterDrawParameters;
	FShaderParameter ViewDimensionsParameter;
	FShaderParameter ThreadToCulledTile;
	FShaderParameter TileListGroupSize;
	FShaderParameter NumCircleSections;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
	FShaderResourceParameter IrradianceCacheSplatTexture;
	FShaderResourceParameter IrradianceCacheSplatSampler;
};

IMPLEMENT_SHADER_TYPE(,FPopulateCacheCS,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("PopulateCacheCS"),SF_Compute);

IMPLEMENT_SHADER_TYPE(template<>,TSetupFinalGatherIndirectArgumentsCS<true>,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("SetupFinalGatherIndirectArgumentsCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TSetupFinalGatherIndirectArgumentsCS<false>,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("SetupFinalGatherIndirectArgumentsCS"),SF_Compute);

template<bool bSupportIrradiance>
class TConeTraceSurfaceCacheOcclusionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TConeTraceSurfaceCacheOcclusionCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	TConeTraceSurfaceCacheOcclusionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		IrradianceCachePositionRadius.Bind(Initializer.ParameterMap, TEXT("IrradianceCachePositionRadius"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		IrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheNormal"));
		OccluderRadius.Bind(Initializer.ParameterMap, TEXT("OccluderRadius"));
		RecordConeVisibility.Bind(Initializer.ParameterMap, TEXT("RecordConeVisibility"));
		RecordConeData.Bind(Initializer.ParameterMap, TEXT("RecordConeData"));
		IrradianceCacheTileCoordinate.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheTileCoordinate"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
		ViewDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
		ThreadToCulledTile.Bind(Initializer.ParameterMap, TEXT("ThreadToCulledTile"));
		TileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("TileHeadDataUnpacked"));
		TileArrayData.Bind(Initializer.ParameterMap, TEXT("TileArrayData"));
		TileListGroupSize.Bind(Initializer.ParameterMap, TEXT("TileListGroupSize"));
		TanConeHalfAngle.Bind(Initializer.ParameterMap, TEXT("TanConeHalfAngle"));
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		DistanceFieldAtlasTexelSize.Bind(Initializer.ParameterMap, TEXT("DistanceFieldAtlasTexelSize"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
		IrradianceCacheSplatTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheSplatTexture"));
		IrradianceCacheSplatSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheSplatSampler"));
		AOLevelParameters.Bind(Initializer.ParameterMap);
		RecordRadiusScale.Bind(Initializer.ParameterMap, TEXT("RecordRadiusScale"));
	}

	TConeTraceSurfaceCacheOcclusionCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& DistanceFieldAOIrradianceCacheSplat, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		int32 DownsampleFactorValue, 
		int32 DepthLevel, 
		FIntPoint TileListGroupSizeValue, 
		const FDistanceFieldAOParameters& Parameters)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		AOLevelParameters.Set(RHICmdList, ShaderRHI, View, DownsampleFactorValue);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheNormal, SurfaceCacheResources.Level[DepthLevel]->Normal.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ScatterDrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, SavedStartIndex, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCachePositionRadius, SurfaceCacheResources.Level[DepthLevel]->PositionAndRadius.SRV);

		FUnorderedAccessViewRHIParamRef OutUAVs[3];
		OutUAVs[0] = SurfaceCacheResources.Level[DepthLevel]->OccluderRadius.UAV;
		OutUAVs[1] = GTemporaryIrradianceCacheResources.ConeVisibility.UAV;
		OutUAVs[2] = GTemporaryIrradianceCacheResources.ConeData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		OccluderRadius.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->OccluderRadius);
		RecordConeVisibility.SetBuffer(RHICmdList, ShaderRHI, GTemporaryIrradianceCacheResources.ConeVisibility);
		RecordConeData.SetBuffer(RHICmdList, ShaderRHI, GTemporaryIrradianceCacheResources.ConeData);

		SetShaderValue(RHICmdList, ShaderRHI, ViewDimensionsParameter, View.ViewRect);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		FVector2D ThreadToCulledTileValue(DownsampleFactorValue / (float)(GAODownsampleFactor * GDistanceFieldAOTileSizeX), DownsampleFactorValue / (float)(GAODownsampleFactor * GDistanceFieldAOTileSizeY));
		SetShaderValue(RHICmdList, ShaderRHI, ThreadToCulledTile, ThreadToCulledTileValue);

		FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

		SetSRVParameter(RHICmdList, ShaderRHI, TileHeadDataUnpacked, TileIntersectionResources->TileHeadDataUnpacked.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileArrayData, TileIntersectionResources->TileArrayData.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheTileCoordinate, SurfaceCacheResources.Level[DepthLevel]->TileCoordinate.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);

		SetShaderValue(RHICmdList, ShaderRHI, TileListGroupSize, TileListGroupSizeValue);

		SetShaderValue(RHICmdList, ShaderRHI, TanConeHalfAngle, FMath::Tan(GAOConeHalfAngle));

		FVector UnoccludedVector(0);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			UnoccludedVector += SampleDirections[SampleIndex];
		}

		float BentNormalNormalizeFactorValue = 1.0f / (UnoccludedVector / NumConeSampleDirections).Size();
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalNormalizeFactor, BentNormalNormalizeFactorValue);

		const int32 NumTexelsOneDimX = GDistanceFieldVolumeTextureAtlas.GetSizeX();
		const int32 NumTexelsOneDimY = GDistanceFieldVolumeTextureAtlas.GetSizeY();
		const int32 NumTexelsOneDimZ = GDistanceFieldVolumeTextureAtlas.GetSizeZ();
		const FVector InvTextureDim(1.0f / NumTexelsOneDimX, 1.0f / NumTexelsOneDimY, 1.0f / NumTexelsOneDimZ);
		SetShaderValue(RHICmdList, ShaderRHI, DistanceFieldAtlasTexelSize, InvTextureDim);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			IrradianceCacheSplatTexture,
			IrradianceCacheSplatSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldAOIrradianceCacheSplat.ShaderResourceTexture
			);

		SetShaderValue(RHICmdList, ShaderRHI, RecordRadiusScale, GAORecordRadiusScale);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		OccluderRadius.UnsetUAV(RHICmdList, GetComputeShader());
		RecordConeVisibility.UnsetUAV(RHICmdList, GetComputeShader());
		RecordConeData.UnsetUAV(RHICmdList, GetComputeShader());

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		FUnorderedAccessViewRHIParamRef OutUAVs[3];
		OutUAVs[0] = SurfaceCacheResources.Level[DepthLevel]->OccluderRadius.UAV;
		OutUAVs[1] = GTemporaryIrradianceCacheResources.ConeVisibility.UAV;
		OutUAVs[2] = GTemporaryIrradianceCacheResources.ConeData.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << AOLevelParameters;
		Ar << IrradianceCachePositionRadius;
		Ar << TileConeDepthRanges;
		Ar << IrradianceCacheNormal;
		Ar << OccluderRadius;
		Ar << RecordConeVisibility;
		Ar << RecordConeData;
		Ar << IrradianceCacheTileCoordinate;
		Ar << ScatterDrawParameters;
		Ar << SavedStartIndex;
		Ar << ViewDimensionsParameter;
		Ar << ThreadToCulledTile;
		Ar << TileHeadDataUnpacked;
		Ar << TileArrayData;
		Ar << TileListGroupSize;
		Ar << TanConeHalfAngle;
		Ar << BentNormalNormalizeFactor;
		Ar << DistanceFieldAtlasTexelSize;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		Ar << IrradianceCacheSplatTexture;
		Ar << IrradianceCacheSplatSampler;
		Ar << RecordRadiusScale;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FAOLevelParameters AOLevelParameters;
	FShaderResourceParameter IrradianceCacheNormal;
	FShaderResourceParameter IrradianceCachePositionRadius;
	FShaderResourceParameter TileConeDepthRanges;
	FRWShaderParameter OccluderRadius;
	FRWShaderParameter RecordConeVisibility;
	FRWShaderParameter RecordConeData;
	FShaderResourceParameter ScatterDrawParameters;
	FShaderResourceParameter SavedStartIndex;
	FShaderResourceParameter IrradianceCacheTileCoordinate;
	FShaderParameter ViewDimensionsParameter;
	FShaderParameter ThreadToCulledTile;
	FShaderResourceParameter TileHeadDataUnpacked;
	FShaderResourceParameter TileArrayData;
	FShaderParameter TileListGroupSize;
	FShaderParameter TanConeHalfAngle;
	FShaderParameter BentNormalNormalizeFactor;
	FShaderParameter DistanceFieldAtlasTexelSize;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
	FShaderResourceParameter IrradianceCacheSplatTexture;
	FShaderResourceParameter IrradianceCacheSplatSampler;
	FShaderParameter RecordRadiusScale;
};

IMPLEMENT_SHADER_TYPE(template<>,TConeTraceSurfaceCacheOcclusionCS<true>,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("ConeTraceOcclusionCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TConeTraceSurfaceCacheOcclusionCS<false>,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("ConeTraceOcclusionCS"),SF_Compute);

class FCombineConesCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCombineConesCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FCombineConesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		IrradianceCacheBentNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheBentNormal"));
		CacheParameters.Bind(Initializer.ParameterMap);
		RecordConeVisibility.Bind(Initializer.ParameterMap, TEXT("RecordConeVisibility"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		TanConeHalfAngle.Bind(Initializer.ParameterMap, TEXT("TanConeHalfAngle"));
	}

	FCombineConesCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		int32 DepthLevel, 
		const FDistanceFieldAOParameters& Parameters,
		const FLightSceneProxy* DirectionalLight,
		const FMatrix& WorldToShadowMatrixValue,
		FLightTileIntersectionResources* TileIntersectionResources,
		FVPLResources* VPLResources)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		CacheParameters.Set(RHICmdList, ShaderRHI, *SurfaceCacheResources.Level[DepthLevel]);
		SetSRVParameter(RHICmdList, ShaderRHI, ScatterDrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, SavedStartIndex, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, RecordConeVisibility, GTemporaryIrradianceCacheResources.ConeVisibility.SRV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, SurfaceCacheResources.Level[DepthLevel]->BentNormal.UAV);
		IrradianceCacheBentNormal.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->BentNormal);

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

		SetShaderValue(RHICmdList, ShaderRHI, TanConeHalfAngle, FMath::Tan(GAOConeHalfAngle));
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FSceneView& View, int32 DepthLevel)
	{
		IrradianceCacheBentNormal.UnsetUAV(RHICmdList, GetComputeShader());

		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, SurfaceCacheResources.Level[DepthLevel]->BentNormal.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << IrradianceCacheBentNormal;
		Ar << CacheParameters;
		Ar << RecordConeVisibility;
		Ar << ScatterDrawParameters;
		Ar << SavedStartIndex;
		Ar << BentNormalNormalizeFactor;
		Ar << TanConeHalfAngle;
		return bShaderHasOutdatedParameters;
	}

private:

	FAOParameters AOParameters;
	FRWShaderParameter IrradianceCacheBentNormal;
	FIrradianceCacheParameters CacheParameters;
	FShaderResourceParameter RecordConeVisibility;
	FShaderResourceParameter ScatterDrawParameters;
	FShaderResourceParameter SavedStartIndex;
	FShaderParameter BentNormalNormalizeFactor;
	FShaderParameter TanConeHalfAngle;
};

IMPLEMENT_SHADER_TYPE(,FCombineConesCS,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("CombineConesCS"),SF_Compute);

class FWriteDownsampledDepthPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWriteDownsampledDepthPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
	}

	/** Default constructor. */
	FWriteDownsampledDepthPS() {}

	/** Initialization constructor. */
	FWriteDownsampledDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap,TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap,TEXT("DistanceFieldNormalSampler"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FSceneRenderTargetItem& DistanceFieldNormal)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, DistanceFieldNormalTexture, DistanceFieldNormalSampler, TStaticSamplerState<SF_Point>::GetRHI(), DistanceFieldNormal.ShaderResourceTexture);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
};

IMPLEMENT_SHADER_TYPE(,FWriteDownsampledDepthPS,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("WriteDownsampledDepthPS"),SF_Pixel);


/**  */
template<bool bFinalInterpolationPass, bool bSupportIrradiance>
class TIrradianceCacheSplatVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TIrradianceCacheSplatVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);

		OutEnvironment.SetDefine(TEXT("FINAL_INTERPOLATION_PASS"), (uint32)bFinalInterpolationPass);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);
	}

	TIrradianceCacheSplatVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		CacheParameters.Bind(Initializer.ParameterMap);
		InterpolationRadiusScale.Bind(Initializer.ParameterMap, TEXT("InterpolationRadiusScale"));
		NormalizedOffsetToPixelCenter.Bind(Initializer.ParameterMap, TEXT("NormalizedOffsetToPixelCenter"));
		HackExpand.Bind(Initializer.ParameterMap, TEXT("HackExpand"));
		InterpolationBoundingDirection.Bind(Initializer.ParameterMap, TEXT("InterpolationBoundingDirection"));
		AOParameters.Bind(Initializer.ParameterMap);
	}

	TIrradianceCacheSplatVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FDistanceFieldAOParameters& Parameters, int32 DepthLevel, int32 CurrentLevelDownsampleFactorValue, FVector2D NormalizedOffsetToPixelCenterValue)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		
		const FScene* Scene = (const FScene*)View.Family->Scene;
		FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

		CacheParameters.Set(RHICmdList, ShaderRHI, *SurfaceCacheResources.Level[DepthLevel]);

		SetShaderValue(RHICmdList, ShaderRHI, InterpolationRadiusScale, (bFinalInterpolationPass ? GAOInterpolationRadiusScale : 1.0f));
		SetShaderValue(RHICmdList, ShaderRHI, NormalizedOffsetToPixelCenter, NormalizedOffsetToPixelCenterValue);

		const FIntPoint AOViewRectSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), CurrentLevelDownsampleFactorValue);
		const FVector2D HackExpandValue(.5f / AOViewRectSize.X, .5f / AOViewRectSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, HackExpand, HackExpandValue);

		// Must push the bounding geometry toward the camera with depth testing, to be in front of any potential receivers
		SetShaderValue(RHICmdList, ShaderRHI, InterpolationBoundingDirection, GAOInterpolationDepthTesting ? -1.0f : 1.0f);

		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CacheParameters;
		Ar << InterpolationRadiusScale;
		Ar << NormalizedOffsetToPixelCenter;
		Ar << HackExpand;
		Ar << InterpolationBoundingDirection;
		Ar << AOParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FIrradianceCacheParameters CacheParameters;
	FShaderParameter InterpolationRadiusScale;
	FShaderParameter NormalizedOffsetToPixelCenter;
	FShaderParameter HackExpand;
	FShaderParameter InterpolationBoundingDirection;
	FAOParameters AOParameters;
};

// typedef required to get around macro expansion failure due to commas in template argument list 
#define IMPLEMENT_SPLAT_VS_TYPE(bFinalInterpolationPass, bSupportIrradiance) \
	typedef TIrradianceCacheSplatVS<bFinalInterpolationPass, bSupportIrradiance> TIrradianceCacheSplatVS##bFinalInterpolationPass##bSupportIrradiance; \
	IMPLEMENT_SHADER_TYPE(template<>,TIrradianceCacheSplatVS##bFinalInterpolationPass##bSupportIrradiance,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("IrradianceCacheSplatVS"),SF_Vertex);

IMPLEMENT_SPLAT_VS_TYPE(true, true)
IMPLEMENT_SPLAT_VS_TYPE(false, true)
IMPLEMENT_SPLAT_VS_TYPE(true, false)
IMPLEMENT_SPLAT_VS_TYPE(false, false)

template<bool bFinalInterpolationPass, bool bSupportIrradiance>
class TIrradianceCacheSplatPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TIrradianceCacheSplatPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("FINAL_INTERPOLATION_PASS"), (uint32)bFinalInterpolationPass);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);
	}

	/** Default constructor. */
	TIrradianceCacheSplatPS() {}

	/** Initialization constructor. */
	TIrradianceCacheSplatPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOLevelParameters.Bind(Initializer.ParameterMap);
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
		InterpolationAngleNormalization.Bind(Initializer.ParameterMap, TEXT("InterpolationAngleNormalization"));
		InvMinCosPointBehindPlane.Bind(Initializer.ParameterMap, TEXT("InvMinCosPointBehindPlane"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FSceneRenderTargetItem& DistanceFieldNormal, int32 DestLevelDownsampleFactor, const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		AOLevelParameters.Set(RHICmdList, ShaderRHI, View, DestLevelDownsampleFactor);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);

		const float EffectiveMaxAngle = bFinalInterpolationPass ? GAOInterpolationMaxAngle * GAOInterpolationAngleScale : GAOInterpolationMaxAngle;
		const float InterpolationAngleNormalizationValue = 1.0f / FMath::Sqrt(1.0f - FMath::Cos(EffectiveMaxAngle * PI / 180.0f));
		SetShaderValue(RHICmdList, ShaderRHI, InterpolationAngleNormalization, InterpolationAngleNormalizationValue);

		const float MinCosPointBehindPlaneValue = FMath::Cos((GAOMinPointBehindPlaneAngle + 90.0f) * PI / 180.0f);
		SetShaderValue(RHICmdList, ShaderRHI, InvMinCosPointBehindPlane, 1.0f / MinCosPointBehindPlaneValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << AOLevelParameters;
		Ar << DeferredParameters;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		Ar << InterpolationAngleNormalization;
		Ar << InvMinCosPointBehindPlane;
		return bShaderHasOutdatedParameters;
	}

private:
	FAOParameters AOParameters;
	FAOLevelParameters AOLevelParameters;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
	FShaderParameter InterpolationAngleNormalization;
	FShaderParameter InvMinCosPointBehindPlane;
};

// typedef required to get around macro expansion failure due to commas in template argument list 
#define IMPLEMENT_SPLAT_PS_TYPE(bFinalInterpolationPass, bSupportIrradiance) \
	typedef TIrradianceCacheSplatPS<bFinalInterpolationPass, bSupportIrradiance> TIrradianceCacheSplatPS##bFinalInterpolationPass##bSupportIrradiance; \
	IMPLEMENT_SHADER_TYPE(template<>,TIrradianceCacheSplatPS##bFinalInterpolationPass##bSupportIrradiance,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("IrradianceCacheSplatPS"),SF_Pixel);

IMPLEMENT_SPLAT_PS_TYPE(true, true)
IMPLEMENT_SPLAT_PS_TYPE(false, true)
IMPLEMENT_SPLAT_PS_TYPE(true, false)
IMPLEMENT_SPLAT_PS_TYPE(false, false)

/** Generates a pseudo-random position inside the unit sphere, uniformly distributed over the volume of the sphere. */
FVector GetUnitPosition2(FRandomStream& RandomStream)
{
	FVector Result;
	// Use rejection sampling to generate a valid sample
	do
	{
		Result.X = RandomStream.GetFraction() * 2 - 1;
		Result.Y = RandomStream.GetFraction() * 2 - 1;
		Result.Z = RandomStream.GetFraction() * 2 - 1;
	} while( Result.SizeSquared() > 1.f );
	return Result;
}

/** Generates a pseudo-random unit vector, uniformly distributed over all directions. */
FVector GetUnitVector2(FRandomStream& RandomStream)
{
	return GetUnitPosition2(RandomStream).GetUnsafeNormal();
}

void GenerateBestSpacedVectors()
{
	static bool bGenerated = false;
	bool bApplyRepulsion = false;

	if (bApplyRepulsion && !bGenerated)
	{
		bGenerated = true;

		FVector OriginalSpacedVectors9[ARRAY_COUNT(SpacedVectors9)];

		for (int32 i = 0; i < ARRAY_COUNT(OriginalSpacedVectors9); i++)
		{
			OriginalSpacedVectors9[i] = SpacedVectors9[i];
		}

		float CosHalfAngle = 1 - 1.0f / (float)ARRAY_COUNT(OriginalSpacedVectors9);
		// Used to prevent self-shadowing on a plane
		float AngleBias = .03f;
		float MinAngle = FMath::Acos(CosHalfAngle) + AngleBias;
		float MinZ = FMath::Sin(MinAngle);

		// Relaxation iterations by repulsion
		for (int32 Iteration = 0; Iteration < 10000; Iteration++)
		{
			for (int32 i = 0; i < ARRAY_COUNT(OriginalSpacedVectors9); i++)
			{
				FVector Force(0.0f, 0.0f, 0.0f);

				for (int32 j = 0; j < ARRAY_COUNT(OriginalSpacedVectors9); j++)
				{
					if (i != j)
					{
						FVector Distance = OriginalSpacedVectors9[i] - OriginalSpacedVectors9[j];
						float Dot = OriginalSpacedVectors9[i] | OriginalSpacedVectors9[j];

						if (Dot > 0)
						{
							// Repulsion force
							Force += .001f * Distance.GetSafeNormal() * Dot * Dot * Dot * Dot;
						}
					}
				}

				FVector NewPosition = OriginalSpacedVectors9[i] + Force;
				NewPosition.Z = FMath::Max(NewPosition.Z, MinZ);
				NewPosition = NewPosition.GetSafeNormal();
				OriginalSpacedVectors9[i] = NewPosition;
			}
		}

		for (int32 i = 0; i < ARRAY_COUNT(OriginalSpacedVectors9); i++)
		{
			UE_LOG(LogDistanceField, Log, TEXT("FVector(%f, %f, %f),"), OriginalSpacedVectors9[i].X, OriginalSpacedVectors9[i].Y, OriginalSpacedVectors9[i].Z);
		}

		int32 temp = 0;
	}

	bool bBruteForceGenerateConeDirections = false;

	if (bBruteForceGenerateConeDirections)
	{
		FVector BestSpacedVectors9[9];
		float BestCoverage = 0;
		// Each cone covers an area of ConeSolidAngle = HemisphereSolidAngle / NumCones
		// HemisphereSolidAngle = 2 * PI
		// ConeSolidAngle = 2 * PI * (1 - cos(ConeHalfAngle))
		// cos(ConeHalfAngle) = 1 - 1 / NumCones
		float CosHalfAngle = 1 - 1.0f / (float)ARRAY_COUNT(BestSpacedVectors9);
		// Prevent self-intersection in sample set
		float MinAngle = FMath::Acos(CosHalfAngle);
		float MinZ = FMath::Sin(MinAngle);
		FRandomStream RandomStream(123567);

		// Super slow random brute force search
		for (int i = 0; i < 1000000; i++)
		{
			FVector CandidateSpacedVectors[ARRAY_COUNT(BestSpacedVectors9)];

			for (int j = 0; j < ARRAY_COUNT(CandidateSpacedVectors); j++)
			{
				FVector NewSample;

				// Reject invalid directions until we get a valid one
				do 
				{
					NewSample = GetUnitVector2(RandomStream);
				} 
				while (NewSample.Z <= MinZ);

				CandidateSpacedVectors[j] = NewSample;
			}

			float Coverage = 0;
			int NumSamples = 10000;

			// Determine total cone coverage with monte carlo estimation
			for (int sample = 0; sample < NumSamples; sample++)
			{
				FVector NewSample;

				do 
				{
					NewSample = GetUnitVector2(RandomStream);
				} 
				while (NewSample.Z <= 0);

				bool bIntersects = false;

				for (int j = 0; j < ARRAY_COUNT(CandidateSpacedVectors); j++)
				{
					if (FVector::DotProduct(CandidateSpacedVectors[j], NewSample) > CosHalfAngle)
					{
						bIntersects = true;
						break;
					}
				}

				Coverage += bIntersects ? 1 / (float)NumSamples : 0;
			}

			if (Coverage > BestCoverage)
			{
				BestCoverage = Coverage;

				for (int j = 0; j < ARRAY_COUNT(CandidateSpacedVectors); j++)
				{
					BestSpacedVectors9[j] = CandidateSpacedVectors[j];
				}
			}
		}

		int32 temp = 0;
	}
}

FIntPoint BuildTileObjectLists(FRHICommandListImmediate& RHICmdList, FScene* Scene, TArray<FViewInfo>& Views, FSceneRenderTargetItem& DistanceFieldNormal, const FDistanceFieldAOParameters& Parameters)
{
	SCOPED_DRAW_EVENT(RHICmdList, BuildTileList);
	SetRenderTarget(RHICmdList, NULL, NULL);

	FIntPoint TileListGroupSize;

	if (GAOScatterTileCulling)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			uint32 GroupSizeX = FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor, GDistanceFieldAOTileSizeX);
			uint32 GroupSizeY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor, GDistanceFieldAOTileSizeY);
			TileListGroupSize = FIntPoint(GroupSizeX, GroupSizeY);

			FTileIntersectionResources*& TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

			if (!TileIntersectionResources || TileIntersectionResources->TileDimensions != TileListGroupSize)
			{
				if (TileIntersectionResources)
				{
					TileIntersectionResources->ReleaseResource();
				}
				else
				{
					TileIntersectionResources = new FTileIntersectionResources();
				}

				TileIntersectionResources->TileDimensions = TileListGroupSize;

				TileIntersectionResources->InitResource();
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, BuildTileCones);
				TShaderMapRef<FBuildTileConesCS> ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldNormal, Scene, FVector2D(GroupSizeX, GroupSizeY), Parameters);
				DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

				ComputeShader->UnsetParameters(RHICmdList, View);
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, CullObjectsToTiles);

				TShaderMapRef<FObjectCullVS> VertexShader(View.ShaderMap);
				TShaderMapRef<FObjectCullPS> PixelShader(View.ShaderMap);

				TArray<FUnorderedAccessViewRHIParamRef> UAVs;
				PixelShader->GetUAVs(Views[0], UAVs);
				RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, UAVs.GetData(), UAVs.Num());
				RHICmdList.SetRenderTargets(0, (const FRHIRenderTargetView*)NULL, NULL, UAVs.Num(), UAVs.GetData());

				RHICmdList.SetViewport(0, 0, 0.0f, GroupSizeX, GroupSizeY, 1.0f);

				// Render backfaces since camera may intersect
				RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

				static FGlobalBoundShaderState BoundShaderState;
				
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);

				VertexShader->SetParameters(RHICmdList, View, Parameters);
				PixelShader->SetParameters(RHICmdList, View, FVector2D(GroupSizeX, GroupSizeY), Parameters);

				RHICmdList.SetStreamSource(0, StencilingGeometry::GLowPolyStencilSphereVertexBuffer.VertexBufferRHI, sizeof(FVector4), 0);

				RHICmdList.DrawIndexedPrimitiveIndirect(
					PT_TriangleList,
					StencilingGeometry::GLowPolyStencilSphereIndexBuffer.IndexBufferRHI, 
					GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.Buffer,
					0);
				RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, UAVs.GetData(), UAVs.Num());
			}
		}
	}
	else
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			uint32 GroupSizeX = (View.ViewRect.Size().X / GAODownsampleFactor + GDistanceFieldAOTileSizeX - 1) / GDistanceFieldAOTileSizeX;
			uint32 GroupSizeY = (View.ViewRect.Size().Y / GAODownsampleFactor + GDistanceFieldAOTileSizeY - 1) / GDistanceFieldAOTileSizeY;
			TileListGroupSize = FIntPoint(GroupSizeX, GroupSizeY);

			FTileIntersectionResources*& TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

			if (!TileIntersectionResources || TileIntersectionResources->TileDimensions != TileListGroupSize)
			{
				if (TileIntersectionResources)
				{
					TileIntersectionResources->ReleaseResource();
				}
				else
				{
					TileIntersectionResources = new FTileIntersectionResources();
				}

				TileIntersectionResources->TileDimensions = TileListGroupSize;

				TileIntersectionResources->InitResource();
			}

			// Indicates the clear value for each channel of the UAV format
			uint32 ClearValues[4] = { 0 };
			RHICmdList.ClearUAV(TileIntersectionResources->TileArrayNextAllocation.UAV, ClearValues);
			RHICmdList.ClearUAV(TileIntersectionResources->TileHeadDataUnpacked.UAV, ClearValues);

			TShaderMapRef<FDistanceFieldBuildTileListCS > ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, Scene, FVector2D(GroupSizeX, GroupSizeY), Parameters);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

			ComputeShader->UnsetParameters(RHICmdList, View);
		}
	}

	return TileListGroupSize;
}

void SetupDepthStencil(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View, 
	FSceneRenderTargetItem& SplatDepthStencilBuffer,
	FSceneRenderTargetItem& DistanceFieldNormal, 
	int32 DepthLevel, 
	int32 DestLevelDownsampleFactor)
{
	SCOPED_DRAW_EVENT(RHICmdList, SetupDepthStencil);

	SetRenderTarget(RHICmdList, NULL, SplatDepthStencilBuffer.TargetableTexture);
	RHICmdList.Clear(false, FLinearColor(0, 0, 0, 0), true, 0, true, 0, FIntRect());

	{		
		RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
		// Depth writes on
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
		// Color writes off
		RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());

		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FWriteDownsampledDepthPS> PixelShader(View.ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
		PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal);

		// Draw a quad writing depth out to the depth stencil view
		DrawRectangle( 
			RHICmdList,
			0, 0, 
			View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
			0, 0, 
			View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
			FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
			GetBufferSizeForAO(),
			*VertexShader);
	}

	if (GAOInterpolationStencilTesting)
	{
		RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
		RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());

		// Depth tests on, write 1 to stencil if depth test passed
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false,CF_DepthNearOrEqual,
			true,CF_Always,SO_Keep,SO_Keep,SO_Replace,
			false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff
			>::GetRHI(), 1);

		TShaderMapRef<TOneColorVS<true> > VertexShader(View.ShaderMap);
		TShaderMapRef<FOneColorPS> PixelShader(View.ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);
		PixelShader->SetColors(RHICmdList, &FLinearColor::Black, 1);

		FVector ViewSpaceMaxDistance(0.0f, 0.0f, GetMaxAOViewDistance());
		FVector4 ClipSpaceMaxDistance = View.ViewMatrices.ProjMatrix.TransformPosition(ViewSpaceMaxDistance);
		float ClipSpaceZ = ClipSpaceMaxDistance.Z / ClipSpaceMaxDistance.W;

		// Place the quad's depth at the max AO view distance
		// Any pixels that pass the depth test should not receive AO and therefore write a 1 to stencil
		FVector4 ClearQuadVertices[4] = 
		{
			FVector4( -1.0f,  1.0f, ClipSpaceZ, 1.0f ),
			FVector4(  1.0f,  1.0f, ClipSpaceZ, 1.0f ),
			FVector4( -1.0f, -1.0f, ClipSpaceZ, 1.0f ),
			FVector4(  1.0f, -1.0f, ClipSpaceZ, 1.0f )
		};

		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, ClearQuadVertices, sizeof(ClearQuadVertices[0]));
	}

	RHICmdList.CopyToResolveTarget(SplatDepthStencilBuffer.TargetableTexture, SplatDepthStencilBuffer.ShaderResourceTexture, false, FResolveParams());
}

void RenderIrradianceCacheInterpolation(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View, 
	IPooledRenderTarget* BentNormalInterpolationTarget, 
	IPooledRenderTarget* IrradianceInterpolationTarget, 
	FSceneRenderTargetItem& DistanceFieldNormal, 
	int32 DepthLevel, 
	int32 DestLevelDownsampleFactor, 
	const FDistanceFieldAOParameters& Parameters,
	bool bFinalInterpolation)
{
	check(!(bFinalInterpolation && DepthLevel != 0));
	const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

	{
		TRefCountPtr<IPooledRenderTarget> SplatDepthStencilBuffer;

		if (bFinalInterpolation && (GAOInterpolationDepthTesting || GAOInterpolationStencilTesting))
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BentNormalInterpolationTarget->GetDesc().Extent, PF_DepthStencil, FClearValueBinding::DepthZero, TexCreate_None, TexCreate_DepthStencilTargetable, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SplatDepthStencilBuffer, TEXT("DistanceFieldAOSplatDepthBuffer"));

			SetupDepthStencil(RHICmdList, View, SplatDepthStencilBuffer->GetRenderTargetItem(), DistanceFieldNormal, DepthLevel, DestLevelDownsampleFactor);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SplatDepthStencilBuffer);
		}

		SCOPED_DRAW_EVENT(RHICmdList, IrradianceCacheSplat);

		const bool bBindIrradiance = bUseDistanceFieldGI && bFinalInterpolation;

		FTextureRHIParamRef DepthBuffer = SplatDepthStencilBuffer ? SplatDepthStencilBuffer->GetRenderTargetItem().TargetableTexture : NULL;

		{
			FRHIRenderTargetView RenderTargets[MaxSimultaneousRenderTargets];
			RenderTargets[0] = FRHIRenderTargetView(BentNormalInterpolationTarget->GetRenderTargetItem().TargetableTexture);
			RenderTargets[1] = FRHIRenderTargetView(bBindIrradiance ? IrradianceInterpolationTarget->GetRenderTargetItem().TargetableTexture : NULL);

			const int32 MRTCount = bBindIrradiance ? 2 : 1;

			FRHIDepthRenderTargetView DepthView(DepthBuffer);
			FRHISetRenderTargetsInfo Info(MRTCount, RenderTargets, DepthView);

			check(RenderTargets[0].Texture->GetClearColor() == FLinearColor::Transparent);
			check(!RenderTargets[1].Texture || RenderTargets[1].Texture->GetClearColor() == FLinearColor::Transparent);
			Info.bClearColor = true;
						

			// set the render target
			RHICmdList.SetRenderTargetsAndClear(Info);
		}

		static int32 NumInterpolationSections = 8;
		if (GCircleVertexBuffer.NumSections != NumInterpolationSections)
		{
			GCircleVertexBuffer.NumSections = NumInterpolationSections;
			GCircleVertexBuffer.UpdateRHI();
		}

		{
			const FScene* Scene = (const FScene*)View.Family->Scene;
			FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

			const uint32 DownsampledViewSizeX = FMath::DivideAndRoundUp(View.ViewRect.Width(), DestLevelDownsampleFactor);
			const uint32 DownsampledViewSizeY = FMath::DivideAndRoundUp(View.ViewRect.Height(), DestLevelDownsampleFactor);

			RHICmdList.SetViewport(0, 0, 0.0f, DownsampledViewSizeX, DownsampledViewSizeY, 1.0f);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

			if (bFinalInterpolation && (GAOInterpolationDepthTesting || GAOInterpolationStencilTesting))
			{
				if (GAOInterpolationDepthTesting)
				{
					if (GAOInterpolationStencilTesting)
					{
						// Depth tests enabled, pass stencil test if stencil is zero
						RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
							false,CF_DepthNearOrEqual,
							true,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
							false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0xff
							>::GetRHI());
					}
					else
					{
						// Depth tests enabled
						RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
					}
				}
				else if (GAOInterpolationStencilTesting)
				{
					// Pass stencil test if stencil is zero
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
						false,CF_Always,
						true,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
						false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
						0xff,0xff
						>::GetRHI());
				}
			}
			else
			{
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			}

			RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One, CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());

			const FIntPoint BufferSize = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
			const float InvBufferSizeX = 1.0f / BufferSize.X;
			const float InvBufferSizeY = 1.0f / BufferSize.Y;

			const FVector2D ScreenPositionScale(
				View.ViewRect.Width() * InvBufferSizeX / +2.0f,
				View.ViewRect.Height() * InvBufferSizeY / (-2.0f * GProjectionSignY));

			const FVector2D ScreenPositionBias(
				(View.ViewRect.Width() / 2.0f) * InvBufferSizeX,
				(View.ViewRect.Height() / 2.0f) * InvBufferSizeY);

			FVector2D OffsetToLowResCorner = (FVector2D(.5f, .5f) / FVector2D(DownsampledViewSizeX, DownsampledViewSizeY) - FVector2D(.5f, .5f)) * FVector2D(2, -2);
			FVector2D OffsetToTopResPixel = (FVector2D(.5f, .5f) / BufferSize - ScreenPositionBias) / ScreenPositionScale;
			FVector2D NormalizedOffsetToPixelCenter = OffsetToLowResCorner - OffsetToTopResPixel;

			RHICmdList.SetStreamSource(0, GCircleVertexBuffer.VertexBufferRHI, sizeof(FScreenVertex), 0);

			if (bFinalInterpolation)
			{
				for (int32 SplatSourceDepthLevel = GAOMaxLevel; SplatSourceDepthLevel >= FMath::Max(DepthLevel, GAOMinLevel); SplatSourceDepthLevel--)
				{
					if (bUseDistanceFieldGI)
					{
						TShaderMapRef<TIrradianceCacheSplatVS<true, true> > VertexShader(View.ShaderMap);
						TShaderMapRef<TIrradianceCacheSplatPS<true, true> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

						VertexShader->SetParameters(RHICmdList, View, Parameters, SplatSourceDepthLevel, DestLevelDownsampleFactor, NormalizedOffsetToPixelCenter);
						PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, DestLevelDownsampleFactor, Parameters);
					}
					else
					{
						TShaderMapRef<TIrradianceCacheSplatVS<true, false> > VertexShader(View.ShaderMap);
						TShaderMapRef<TIrradianceCacheSplatPS<true, false> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

						VertexShader->SetParameters(RHICmdList, View, Parameters, SplatSourceDepthLevel, DestLevelDownsampleFactor, NormalizedOffsetToPixelCenter);
						PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, DestLevelDownsampleFactor, Parameters);
					}

					RHICmdList.DrawPrimitiveIndirect(PT_TriangleList, SurfaceCacheResources.Level[SplatSourceDepthLevel]->ScatterDrawParameters.Buffer, 0);
				}
			}
			else
			{
				for (int32 SplatSourceDepthLevel = GAOMaxLevel; SplatSourceDepthLevel >= FMath::Max(DepthLevel, GAOMinLevel); SplatSourceDepthLevel--)
				{
					if (bUseDistanceFieldGI)
					{
						TShaderMapRef<TIrradianceCacheSplatVS<false, true> > VertexShader(View.ShaderMap);
						TShaderMapRef<TIrradianceCacheSplatPS<false, true> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

						VertexShader->SetParameters(RHICmdList, View, Parameters, SplatSourceDepthLevel, DestLevelDownsampleFactor, NormalizedOffsetToPixelCenter);
						PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, DestLevelDownsampleFactor, Parameters);
					}
					else
					{
						TShaderMapRef<TIrradianceCacheSplatVS<false, false> > VertexShader(View.ShaderMap);
						TShaderMapRef<TIrradianceCacheSplatPS<false, false> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

						VertexShader->SetParameters(RHICmdList, View, Parameters, SplatSourceDepthLevel, DestLevelDownsampleFactor, NormalizedOffsetToPixelCenter);
						PixelShader->SetParameters(RHICmdList, View, DistanceFieldNormal, DestLevelDownsampleFactor, Parameters);
					}

					RHICmdList.DrawPrimitiveIndirect(PT_TriangleList, SurfaceCacheResources.Level[SplatSourceDepthLevel]->ScatterDrawParameters.Buffer, 0);
				}
			}
		}

		RHICmdList.CopyToResolveTarget(BentNormalInterpolationTarget->GetRenderTargetItem().TargetableTexture, BentNormalInterpolationTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());

		if (bFinalInterpolation && bUseDistanceFieldGI && (GAOInterpolationDepthTesting || GAOInterpolationStencilTesting))
		{
			RHICmdList.CopyToResolveTarget(IrradianceInterpolationTarget->GetRenderTargetItem().TargetableTexture, IrradianceInterpolationTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
		}
	}
}

void ListDistanceFieldLightingMemory(const FViewInfo& View)
{
	const FScene* Scene = (const FScene*)View.Family->Scene;
	UE_LOG(LogTemp, Log, TEXT("Shared GPU memory (excluding render targets)"));

	if (Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("   Scene Object data %.3fMb"), Scene->DistanceFieldSceneData.ObjectBuffers->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	UE_LOG(LogTemp, Log, TEXT("   %s"), *GDistanceFieldVolumeTextureAtlas.GetSizeString());
	extern FString GetObjectBufferMemoryString();
	UE_LOG(LogTemp, Log, TEXT("   %s"), *GetObjectBufferMemoryString());
	UE_LOG(LogTemp, Log, TEXT(""));
	UE_LOG(LogTemp, Log, TEXT("Distance Field AO"));

	if (Scene->SurfaceCacheResources)
	{
		UE_LOG(LogTemp, Log, TEXT("   Surface cache %.3fMb"), Scene->SurfaceCacheResources->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	UE_LOG(LogTemp, Log, TEXT("   Temporary cache %.3fMb"), GTemporaryIrradianceCacheResources.GetSizeBytes() / 1024.0f / 1024.0f);
	UE_LOG(LogTemp, Log, TEXT("   Culled objects %.3fMb"), GAOCulledObjectBuffers.Buffers.GetSizeBytes() / 1024.0f / 1024.0f);

	FTileIntersectionResources* TileIntersectionResources = ((FSceneViewState*)View.State)->AOTileIntersectionResources;

	if (TileIntersectionResources)
	{
		UE_LOG(LogTemp, Log, TEXT("   Tile Culled objects %.3fMb"), TileIntersectionResources->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	FAOScreenGridResources* ScreenGridResources = ((FSceneViewState*)View.State)->AOScreenGridResources;

	if (ScreenGridResources)
	{
		UE_LOG(LogTemp, Log, TEXT("   Screen grid temporaries %.3fMb"), ScreenGridResources->GetSizeBytesForAO() / 1024.0f / 1024.0f);
	}
	
	extern void ListGlobalDistanceFieldMemory();
	ListGlobalDistanceFieldMemory();

	UE_LOG(LogTemp, Log, TEXT(""));
	UE_LOG(LogTemp, Log, TEXT("Distance Field GI"));

	if (Scene->DistanceFieldSceneData.SurfelBuffers)
	{
		UE_LOG(LogTemp, Log, TEXT("   Scene surfel data %.3fMb"), Scene->DistanceFieldSceneData.SurfelBuffers->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	if (Scene->DistanceFieldSceneData.InstancedSurfelBuffers)
	{
		UE_LOG(LogTemp, Log, TEXT("   Instanced scene surfel data %.3fMb"), Scene->DistanceFieldSceneData.InstancedSurfelBuffers->GetSizeBytes() / 1024.0f / 1024.0f);
	}
	
	if (ScreenGridResources)
	{
		UE_LOG(LogTemp, Log, TEXT("   Screen grid temporaries %.3fMb"), ScreenGridResources->GetSizeBytesForGI() / 1024.0f / 1024.0f);
	}

	extern void ListDistanceFieldGIMemory(const FViewInfo& View);
	ListDistanceFieldGIMemory(View);
}

bool SupportsDistanceFieldAO(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform)
{
	return GDistanceFieldAO 
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldAO(ShaderPlatform);
}

bool ShouldRenderDeferredDynamicSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return Scene->SkyLight
		&& Scene->SkyLight->ProcessedTexture
		&& !Scene->SkyLight->bWantsStaticShadowing
		&& !Scene->SkyLight->bHasStaticLighting
		&& ViewFamily.EngineShowFlags.SkyLighting
		&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4
		&& !IsAnyForwardShadingEnabled(Scene->GetShaderPlatform()) 
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling;
}

bool FDeferredShadingSceneRenderer::ShouldPrepareForDistanceFieldAO() const
{
	return SupportsDistanceFieldAO(Scene->GetFeatureLevel(), Scene->GetShaderPlatform())
		&& ((ShouldRenderDeferredDynamicSkyLight(Scene, ViewFamily) && Scene->SkyLight->bCastShadows && ViewFamily.EngineShowFlags.DistanceFieldAO)
			|| ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields
			|| ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO
			|| ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI
			|| (GDistanceFieldAOApplyToStaticIndirect && ViewFamily.EngineShowFlags.DistanceFieldAO));
}

bool FDeferredShadingSceneRenderer::ShouldPrepareDistanceFields() const
{
	if (!ensure(Scene != nullptr))
	{
		return false;
	}

	bool bShouldPrepareForAO = SupportsDistanceFieldAO(Scene->GetFeatureLevel(), Scene->GetShaderPlatform())
		&& (ShouldPrepareForDistanceFieldAO()
			|| ((Views.Num() > 0) && Views[0].bUsesGlobalDistanceField)
			|| ((Scene->FXSystem != nullptr) && Scene->FXSystem->UsesGlobalDistanceField()));

	return bShouldPrepareForAO || ShouldPrepareForDistanceFieldShadows();
}

void RenderDistanceFieldAOSurfaceCache(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View,
	FScene* Scene,
	FIntPoint TileListGroupSize,
	const FDistanceFieldAOParameters& Parameters, 
	const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
	const TRefCountPtr<IPooledRenderTarget>& DistanceFieldNormal, 
	TRefCountPtr<IPooledRenderTarget>& OutDynamicBentNormalAO, 
	TRefCountPtr<IPooledRenderTarget>& OutDynamicIrradiance)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

	// Create surface cache state that will persist across frames if needed
	if (!Scene->SurfaceCacheResources)
	{
		Scene->SurfaceCacheResources = new FSurfaceCacheResources();
	}

	FSurfaceCacheResources& SurfaceCacheResources = *Scene->SurfaceCacheResources;

	FIntPoint DownsampledViewRect = FIntPoint::DivideAndRoundDown(View.ViewRect.Size(), GAODownsampleFactor);
	// Needed because samples are kept across frames
	int32 SlackAmount = 4;
	int32 MaxSurfaceCacheSamples = DownsampledViewRect.X * DownsampledViewRect.Y / (1 << (2 * GAOMinLevel * GAOPowerOfTwoBetweenLevels));
	SurfaceCacheResources.AllocateFor(GAOMinLevel, GAOMaxLevel, SlackAmount * MaxSurfaceCacheSamples);

	GTemporaryIrradianceCacheResources.AllocateFor(MaxSurfaceCacheSamples);

	uint32 ClearValues[4] = {0};

	if (!SurfaceCacheResources.bClearedResources 
		|| !GAOReuseAcrossFrames 
		// Drop records that will have uninitialized Irradiance if switching from AO only to AO + GI
		|| (bUseDistanceFieldGI && !SurfaceCacheResources.bHasIrradiance))
	{
		// Reset the number of active cache records to 0
		for (int32 DepthLevel = GAOMaxLevel; DepthLevel >= GAOMinLevel; DepthLevel--)
		{
			RHICmdList.ClearUAV(SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.UAV, ClearValues);
		}

		RHICmdList.ClearUAV(SurfaceCacheResources.TempResources->ScatterDrawParameters.UAV, ClearValues);

		SurfaceCacheResources.bClearedResources = true;
	}

	SurfaceCacheResources.bHasIrradiance = bUseDistanceFieldGI;

	if (GAOReuseAcrossFrames)
	{
		SCOPED_DRAW_EVENT(RHICmdList, TrimRecords);

		// Copy and trim last frame's surface cache samples
		for (int32 DepthLevel = GAOMaxLevel; DepthLevel >= GAOMinLevel; DepthLevel--)
		{
			if (GAOTrimOldRecordsFraction > 0)
			{
				{	
					TShaderMapRef<FSetupCopyIndirectArgumentsCS> ComputeShader(View.ShaderMap);

					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
					DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);

					ComputeShader->UnsetParameters(RHICmdList, View);
				}

				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TCopyIrradianceCacheSamplesCS<true> > ComputeShader(View.ShaderMap);

					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
					DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

					ComputeShader->UnsetParameters(RHICmdList, View);
				}
				else
				{
					TShaderMapRef<TCopyIrradianceCacheSamplesCS<false> > ComputeShader(View.ShaderMap);

					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
					DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

					ComputeShader->UnsetParameters(RHICmdList, View);
				}

				Swap(SurfaceCacheResources.Level[DepthLevel], SurfaceCacheResources.TempResources);
			}
		}
	}

	// Start from the highest depth, which will consider the fewest potential shading points
	// Each level potentially prevents the next higher resolution level from doing expensive shading work
	// This is the core of the surface cache algorithm
	for (int32 DepthLevel = GAOMaxLevel; DepthLevel >= GAOMinLevel; DepthLevel--)
	{
		int32 DestLevelDownsampleFactor = GAODownsampleFactor * (1 << (DepthLevel * GAOPowerOfTwoBetweenLevels));

		SCOPED_DRAW_EVENTF(RHICmdList, Level, TEXT("Level_%d"), DepthLevel);

		TRefCountPtr<IPooledRenderTarget> DistanceFieldAOBentNormalSplat;

		{
			FIntPoint AOBufferSize = FIntPoint::DivideAndRoundUp(SceneContext.GetBufferSizeXY(), DestLevelDownsampleFactor);
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(AOBufferSize, PF_FloatRGBA, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DistanceFieldAOBentNormalSplat, TEXT("DistanceFieldAOBentNormalSplat"));
		}

		// Splat / interpolate the surface cache records onto the buffer sized for the current depth level
		RenderIrradianceCacheInterpolation(RHICmdList, View, DistanceFieldAOBentNormalSplat, NULL, DistanceFieldNormal->GetRenderTargetItem(), DepthLevel, DestLevelDownsampleFactor, Parameters, false);

		{
			SCOPED_DRAW_EVENT(RHICmdList, PopulateAndConeTrace);
			SetRenderTarget(RHICmdList, NULL, NULL);

			// Save off the current record count before adding any more
			{	
				TShaderMapRef<FSaveStartIndexCS> ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
				DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);

				ComputeShader->UnsetParameters(RHICmdList, View, DepthLevel);
			}

			// Create new records which haven't been shaded yet for shading points which don't have a valid interpolation from existing records
			{
		        FIntPoint DownsampledViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), DestLevelDownsampleFactor);
				uint32 GroupSizeX = FMath::DivideAndRoundUp(DownsampledViewSize.X, GDistanceFieldAOTileSizeX);
				uint32 GroupSizeY = FMath::DivideAndRoundUp(DownsampledViewSize.Y, GDistanceFieldAOTileSizeY);
        
		        TShaderMapRef<FPopulateCacheCS> ComputeShader(View.ShaderMap);
        
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
		        ComputeShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormalSplat->GetRenderTargetItem(), DistanceFieldNormal->GetRenderTargetItem(), DestLevelDownsampleFactor, DepthLevel, TileListGroupSize, Parameters);
				DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

				ComputeShader->UnsetParameters(RHICmdList, View, DepthLevel);
			}

			{	
				TShaderMapRef<TSetupFinalGatherIndirectArgumentsCS<true> > ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
				DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);

				ComputeShader->UnsetParameters(RHICmdList, View);
			}

			// Compute lighting for the new surface cache records by cone-stepping through the object distance fields
			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TConeTraceSurfaceCacheOcclusionCS<true> > ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormalSplat->GetRenderTargetItem(), DistanceFieldNormal->GetRenderTargetItem(), DestLevelDownsampleFactor, DepthLevel, TileListGroupSize, Parameters);
				DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

				ComputeShader->UnsetParameters(RHICmdList, View, DepthLevel);
			}
			else
			{
				TShaderMapRef<TConeTraceSurfaceCacheOcclusionCS<false> > ComputeShader(View.ShaderMap);

				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormalSplat->GetRenderTargetItem(), DistanceFieldNormal->GetRenderTargetItem(), DestLevelDownsampleFactor, DepthLevel, TileListGroupSize, Parameters);
				DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

				ComputeShader->UnsetParameters(RHICmdList, View, DepthLevel);
			}
		}

		if (bUseDistanceFieldGI)
		{
			extern void ComputeIrradianceForSamples(
				int32 DepthLevel,
				FRHICommandListImmediate& RHICmdList,
				const FViewInfo& View,
				const FScene* Scene,
				const FDistanceFieldAOParameters& Parameters,
				FTemporaryIrradianceCacheResources* TemporaryIrradianceCacheResources);

			ComputeIrradianceForSamples(DepthLevel, RHICmdList, View, Scene, Parameters, &GTemporaryIrradianceCacheResources);
		}

		// Compute heightfield occlusion after heightfield GI, otherwise it self-shadows incorrectly
		View.HeightfieldLightingViewInfo.ComputeOcclusionForSamples(View, RHICmdList, GTemporaryIrradianceCacheResources, DepthLevel, Parameters);

		{	
			TShaderMapRef<TSetupFinalGatherIndirectArgumentsCS<false> > ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
			DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);

			ComputeShader->UnsetParameters(RHICmdList, View);
		}

		// Compute and store the final bent normal now that all occlusion sources have been computed (distance fields, heightfields)
		{
			TShaderMapRef<FCombineConesCS> ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, DepthLevel, Parameters, NULL, FMatrix::Identity, NULL, NULL);
			DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

			ComputeShader->UnsetParameters(RHICmdList, View, DepthLevel);
		}
	}

	TRefCountPtr<IPooledRenderTarget> BentNormalAccumulation;
	TRefCountPtr<IPooledRenderTarget> IrradianceAccumulation;

	{
		FIntPoint BufferSize = GetBufferSizeForAO();
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, BentNormalAccumulation, TEXT("BentNormalAccumulation"));

		if (bUseDistanceFieldGI)
		{
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, IrradianceAccumulation, TEXT("IrradianceAccumulation"));
		}
	}

	// Splat the surface cache records onto the opaque pixels, using less strict weighting so the lighting is smoothed in world space
	{
		SCOPED_DRAW_EVENT(RHICmdList, FinalIrradianceCacheSplat);
		RenderIrradianceCacheInterpolation(RHICmdList, View, BentNormalAccumulation, IrradianceAccumulation, DistanceFieldNormal->GetRenderTargetItem(), 0, GAODownsampleFactor, Parameters, true);
	}

	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, BentNormalAccumulation);

	// Post process the AO to cover over artifacts
	PostProcessBentNormalAOSurfaceCache(
		RHICmdList, 
		Parameters, 
		View, 
		VelocityTexture, 
		BentNormalAccumulation->GetRenderTargetItem(), 
		IrradianceAccumulation, 
		DistanceFieldNormal->GetRenderTargetItem(), 
		OutDynamicBentNormalAO, 
		OutDynamicIrradiance);
}

void FDeferredShadingSceneRenderer::RenderDFAOAsIndirectShadowing(
	FRHICommandListImmediate& RHICmdList,
	const TRefCountPtr<IPooledRenderTarget>& VelocityTexture, 
	TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
{
	if (GDistanceFieldAOApplyToStaticIndirect && ShouldRenderDistanceFieldAO())
	{
		// Use the skylight's max distance if there is one, to be consistent with DFAO shadowing on the skylight
		const float OcclusionMaxDistance = Scene->SkyLight && !Scene->SkyLight->bWantsStaticShadowing ? Scene->SkyLight->OcclusionMaxDistance : Scene->DefaultMaxDistanceFieldOcclusionDistance;
		TRefCountPtr<IPooledRenderTarget> DummyOutput;
		RenderDistanceFieldLighting(RHICmdList, FDistanceFieldAOParameters(OcclusionMaxDistance), VelocityTexture, DynamicBentNormalAO, DummyOutput, true, false, false);
	}
}

bool FDeferredShadingSceneRenderer::RenderDistanceFieldLighting(
	FRHICommandListImmediate& RHICmdList, 
	const FDistanceFieldAOParameters& Parameters, 
	const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
	TRefCountPtr<IPooledRenderTarget>& OutDynamicBentNormalAO, 
	TRefCountPtr<IPooledRenderTarget>& OutDynamicIrradiance,
	bool bModulateToSceneColor,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination)
{
	//@todo - support multiple views
	const FViewInfo& View = Views[0];
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	if (SupportsDistanceFieldAO(View.GetFeatureLevel(), View.GetShaderPlatform())
		&& Views.Num() == 1
		// ViewState is used to cache tile intersection resources which have to be sized based on the view
		&& View.State
		&& View.IsPerspectiveProjection())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderDistanceFieldLighting);

		if (GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI && Scene->DistanceFieldSceneData.NumObjectsInBuffer)
		{
			check(!Scene->DistanceFieldSceneData.HasPendingOperations());
			const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

			SCOPED_DRAW_EVENT(RHICmdList, DistanceFieldLighting);

			GenerateBestSpacedVectors();

			if (bListMemoryNextFrame)
			{
				bListMemoryNextFrame = false;
				ListDistanceFieldLightingMemory(View);
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, ObjectFrustumCulling);

				if (GAOCulledObjectBuffers.Buffers.MaxObjects < Scene->DistanceFieldSceneData.NumObjectsInBuffer
					|| GAOCulledObjectBuffers.Buffers.MaxObjects > 3 * Scene->DistanceFieldSceneData.NumObjectsInBuffer)
				{
					GAOCulledObjectBuffers.Buffers.MaxObjects = Scene->DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
					GAOCulledObjectBuffers.Buffers.Release();
					GAOCulledObjectBuffers.Buffers.Initialize();
				}

				{
					uint32 ClearValues[4] = { 0 };
					RHICmdList.ClearUAV(GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV, ClearValues);

					TShaderMapRef<FCullObjectsForViewCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, Scene, View, Parameters);

					DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, UpdateObjectsGroupSize), 1, 1);
					ComputeShader->UnsetParameters(RHICmdList, Scene);
				}
			}
			
			TRefCountPtr<IPooledRenderTarget> DistanceFieldNormal;

			{
				const FIntPoint BufferSize = GetBufferSizeForAO();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DistanceFieldNormal, TEXT("DistanceFieldNormal"));
			}

			ComputeDistanceFieldNormal(RHICmdList, Views, DistanceFieldNormal->GetRenderTargetItem(), Parameters);

			// Intersect objects with screen tiles, build lists
			FIntPoint TileListGroupSize = BuildTileObjectLists(RHICmdList, Scene, Views, DistanceFieldNormal->GetRenderTargetItem(), Parameters);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, DistanceFieldNormal);

			if (bUseDistanceFieldGI)
			{
				extern void UpdateVPLs(
					FRHICommandListImmediate& RHICmdList,
					const FViewInfo& View,
					const FScene* Scene,
					const FDistanceFieldAOParameters& Parameters);

				UpdateVPLs(RHICmdList, View, Scene, Parameters);
			}

			TRefCountPtr<IPooledRenderTarget> BentNormalOutput;
			TRefCountPtr<IPooledRenderTarget> IrradianceOutput;

			if (GAOUseSurfaceCache)
			{
				RenderDistanceFieldAOSurfaceCache(
					RHICmdList, 
					View,
					Scene,
					TileListGroupSize,
					Parameters, 
					VelocityTexture,
					DistanceFieldNormal, 
					BentNormalOutput, 
					IrradianceOutput);
			}
			else
			{
				RenderDistanceFieldAOScreenGrid(
					RHICmdList, 
					View,
					TileListGroupSize,
					Parameters, 
					VelocityTexture,
					DistanceFieldNormal, 
					BentNormalOutput, 
					IrradianceOutput);
			}

			RenderCapsuleShadowsForMovableSkylight(RHICmdList, BentNormalOutput);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, BentNormalOutput);

			TRefCountPtr<IPooledRenderTarget> SpecularOcclusion;
			RenderDistanceFieldSpecularOcclusion(RHICmdList, View, TileListGroupSize, Parameters, DistanceFieldNormal, SpecularOcclusion);

			if (bVisualizeAmbientOcclusion || bVisualizeGlobalIllumination)
			{
				SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilNop);
			}
			else
			{
				FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
				// Make sure we get a signed format
				Desc.Format = PF_FloatRGBA;
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutDynamicBentNormalAO, TEXT("DynamicBentNormalAO"));

				if (bUseDistanceFieldGI)
				{
					Desc.Format = PF_FloatRGB;
					GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutDynamicIrradiance, TEXT("DynamicIrradiance"));
				}

				FTextureRHIParamRef RenderTargets[3] =
				{
					OutDynamicBentNormalAO->GetRenderTargetItem().TargetableTexture,
					NULL,
					NULL
				};

				int32 NumRenderTargets = 1;

				if (bModulateToSceneColor)
				{
					RenderTargets[NumRenderTargets] = SceneContext.GetSceneColorSurface();
					NumRenderTargets++;
				}

				if (bUseDistanceFieldGI)
				{
					RenderTargets[NumRenderTargets] = OutDynamicIrradiance->GetRenderTargetItem().TargetableTexture;
					NumRenderTargets++;
				}

				SetRenderTargets(RHICmdList, NumRenderTargets, RenderTargets, FTextureRHIParamRef(), 0, NULL);
			}

			// Upsample to full resolution, write to output
			UpsampleBentNormalAO(RHICmdList, Views, BentNormalOutput, IrradianceOutput, SpecularOcclusion, bModulateToSceneColor, bVisualizeAmbientOcclusion, bVisualizeGlobalIllumination);

			if (!bVisualizeAmbientOcclusion && !bVisualizeGlobalIllumination)
			{
				RHICmdList.CopyToResolveTarget(OutDynamicBentNormalAO->GetRenderTargetItem().TargetableTexture, OutDynamicBentNormalAO->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
				if (bUseDistanceFieldGI)
				{
					RHICmdList.CopyToResolveTarget(OutDynamicIrradiance->GetRenderTargetItem().TargetableTexture, OutDynamicIrradiance->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
				}
			}

			return true;
		}
	}

	return false;
}

template<bool bApplyShadowing, bool bSupportIrradiance>
class TDynamicSkyLightDiffusePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDynamicSkyLightDiffusePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("APPLY_SHADOWING"), bApplyShadowing);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance);
	}

	/** Default constructor. */
	TDynamicSkyLightDiffusePS() {}

	/** Initialization constructor. */
	TDynamicSkyLightDiffusePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		DynamicBentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		DynamicBentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		DynamicIrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		DynamicIrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		ContrastAndNormalizeMulAdd.Bind(Initializer.ParameterMap, TEXT("ContrastAndNormalizeMulAdd"));
		OcclusionTintAndMinOcclusion.Bind(Initializer.ParameterMap, TEXT("OcclusionTintAndMinOcclusion"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FTextureRHIParamRef DynamicBentNormalAO, IPooledRenderTarget* DynamicIrradiance, const FDistanceFieldAOParameters& Parameters, const FSkyLightSceneProxy* SkyLight)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, DynamicBentNormalAOTexture, DynamicBentNormalAOSampler, TStaticSamplerState<SF_Point>::GetRHI(), DynamicBentNormalAO);

		if (DynamicIrradianceTexture.IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, DynamicIrradianceTexture, DynamicIrradianceSampler, TStaticSamplerState<SF_Point>::GetRHI(), DynamicIrradiance->GetRenderTargetItem().ShaderResourceTexture);
		}

		// Scale and bias to remap the contrast curve to [0,1]
		const float Min = 1 / (1 + FMath::Exp(-Parameters.Contrast * (0 * 10 - 5)));
		const float Max = 1 / (1 + FMath::Exp(-Parameters.Contrast * (1 * 10 - 5)));
		const float Mul = 1.0f / (Max - Min);
		const float Add = -Min / (Max - Min);

		SetShaderValue(RHICmdList, ShaderRHI, ContrastAndNormalizeMulAdd, FVector(Parameters.Contrast, Mul, Add));

		FVector4 OcclusionTintAndMinOcclusionValue = FVector4(SkyLight->OcclusionTint);
		OcclusionTintAndMinOcclusionValue.W = SkyLight->MinOcclusion;
		SetShaderValue(RHICmdList, ShaderRHI, OcclusionTintAndMinOcclusion, OcclusionTintAndMinOcclusionValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << DynamicBentNormalAOTexture;
		Ar << DynamicBentNormalAOSampler;
		Ar << DynamicIrradianceTexture;
		Ar << DynamicIrradianceSampler;
		Ar << ContrastAndNormalizeMulAdd;
		Ar << OcclusionTintAndMinOcclusion;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter DynamicBentNormalAOTexture;
	FShaderResourceParameter DynamicBentNormalAOSampler;
	FShaderResourceParameter DynamicIrradianceTexture;
	FShaderResourceParameter DynamicIrradianceSampler;
	FShaderParameter ContrastAndNormalizeMulAdd;
	FShaderParameter OcclusionTintAndMinOcclusion;
};

#define IMPLEMENT_SKYLIGHT_PS_TYPE(bApplyShadowing, bSupportIrradiance) \
	typedef TDynamicSkyLightDiffusePS<bApplyShadowing, bSupportIrradiance> TDynamicSkyLightDiffusePS##bApplyShadowing##bSupportIrradiance; \
	IMPLEMENT_SHADER_TYPE(template<>,TDynamicSkyLightDiffusePS##bApplyShadowing##bSupportIrradiance,TEXT("SkyLighting"),TEXT("SkyLightDiffusePS"),SF_Pixel);

IMPLEMENT_SKYLIGHT_PS_TYPE(true, true)
IMPLEMENT_SKYLIGHT_PS_TYPE(false, true)
IMPLEMENT_SKYLIGHT_PS_TYPE(true, false)
IMPLEMENT_SKYLIGHT_PS_TYPE(false, false)

bool FDeferredShadingSceneRenderer::ShouldRenderDistanceFieldAO() const
{
	return ViewFamily.EngineShowFlags.DistanceFieldAO 
		&& !ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO
		&& !ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI
		&& !ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields;
}

void FDeferredShadingSceneRenderer::RenderDynamicSkyLighting(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& VelocityTexture, TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
{
	if (ShouldRenderDeferredDynamicSkyLight(Scene, ViewFamily))
	{
		SCOPED_DRAW_EVENT(RHICmdList, SkyLightDiffuse);

		bool bApplyShadowing = false;

		FDistanceFieldAOParameters Parameters(Scene->SkyLight->OcclusionMaxDistance, Scene->SkyLight->Contrast);
		TRefCountPtr<IPooledRenderTarget> DynamicIrradiance;

		if (Scene->SkyLight->bCastShadows 
			&& !GDistanceFieldAOApplyToStaticIndirect
			&& ShouldRenderDistanceFieldAO() 
			&& ViewFamily.EngineShowFlags.AmbientOcclusion)
		{
			bApplyShadowing = RenderDistanceFieldLighting(RHICmdList, Parameters, VelocityTexture, DynamicBentNormalAO, DynamicIrradiance, false, false, false);
		}

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilRead);

		for( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
		{
			const FViewInfo& View = Views[ViewIndex];

			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

			if (GAOOverwriteSceneColor)
			{
				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			}
			else
			{
				const bool bCheckerboardSubsurfaceRendering = FRCPassPostProcessSubsurface::RequiresCheckerboardSubsurfaceRendering(SceneContext.GetSceneColorFormat());
				if (bCheckerboardSubsurfaceRendering)
				{
					RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
				}
				else
				{
					RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());
				}
			}

			const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View) && IsValidRef(DynamicIrradiance);
			TShaderMapRef< FPostProcessVS > VertexShader(View.ShaderMap);

			if (bApplyShadowing)
			{
				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TDynamicSkyLightDiffusePS<true, true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DynamicBentNormalAO->GetRenderTargetItem().ShaderResourceTexture, DynamicIrradiance, Parameters, Scene->SkyLight);
				}
				else
				{
					TShaderMapRef<TDynamicSkyLightDiffusePS<true, false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DynamicBentNormalAO->GetRenderTargetItem().ShaderResourceTexture, DynamicIrradiance, Parameters, Scene->SkyLight);
				}
			}
			else
			{
				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TDynamicSkyLightDiffusePS<false, true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, GWhiteTexture->TextureRHI, NULL, Parameters, Scene->SkyLight);
				}
				else
				{
					TShaderMapRef<TDynamicSkyLightDiffusePS<false, false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, GWhiteTexture->TextureRHI, NULL, Parameters, Scene->SkyLight);
				}
			}

			DrawRectangle( 
				RHICmdList,
				0, 0, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
				SceneContext.GetBufferSizeXY(),
				*VertexShader);
		}
	}
}

template<bool bUseGlobalDistanceField>
class TVisualizeMeshDistanceFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TVisualizeMeshDistanceFieldCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDistanceFieldAOTileSizeY);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_DISTANCE_FIELD"), bUseGlobalDistanceField);
	}

	/** Default constructor. */
	TVisualizeMeshDistanceFieldCS() {}

	/** Initialization constructor. */
	TVisualizeMeshDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VisualizeMeshDistanceFields.Bind(Initializer.ParameterMap, TEXT("VisualizeMeshDistanceFields"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& VisualizeMeshDistanceFieldsValue, 
		FVector2D NumGroupsValue,
		const FDistanceFieldAOParameters& Parameters,
		const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, VisualizeMeshDistanceFieldsValue.UAV);
		VisualizeMeshDistanceFields.SetTexture(RHICmdList, ShaderRHI, VisualizeMeshDistanceFieldsValue.ShaderResourceTexture, VisualizeMeshDistanceFieldsValue.UAV);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		if (bUseGlobalDistanceField)
		{
			GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, GlobalDistanceFieldInfo.ParameterData);
		}

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& VisualizeMeshDistanceFieldsValue)
	{
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, VisualizeMeshDistanceFieldsValue.UAV);
		VisualizeMeshDistanceFields.UnsetUAV(RHICmdList, GetComputeShader());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << VisualizeMeshDistanceFields;
		Ar << NumGroups;
		Ar << ObjectParameters;
		Ar << DeferredParameters;
		Ar << AOParameters;
		Ar << GlobalDistanceFieldParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter VisualizeMeshDistanceFields;
	FShaderParameter NumGroups;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,TVisualizeMeshDistanceFieldCS<true>,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("VisualizeMeshDistanceFieldCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TVisualizeMeshDistanceFieldCS<false>,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("VisualizeMeshDistanceFieldCS"),SF_Compute);

class FVisualizeDistanceFieldUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeDistanceFieldUpsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
	}

	/** Default constructor. */
	FVisualizeDistanceFieldUpsamplePS() {}

	/** Initialization constructor. */
	FVisualizeDistanceFieldUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		VisualizeDistanceFieldTexture.Bind(Initializer.ParameterMap,TEXT("VisualizeDistanceFieldTexture"));
		VisualizeDistanceFieldSampler.Bind(Initializer.ParameterMap,TEXT("VisualizeDistanceFieldSampler"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget>& VisualizeDistanceField)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, VisualizeDistanceFieldTexture, VisualizeDistanceFieldSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), VisualizeDistanceField->GetRenderTargetItem().ShaderResourceTexture);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << VisualizeDistanceFieldTexture;
		Ar << VisualizeDistanceFieldSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter VisualizeDistanceFieldTexture;
	FShaderResourceParameter VisualizeDistanceFieldSampler;
};

IMPLEMENT_SHADER_TYPE(,FVisualizeDistanceFieldUpsamplePS,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("VisualizeDistanceFieldUpsamplePS"),SF_Pixel);


void FDeferredShadingSceneRenderer::RenderMeshDistanceFieldVisualization(FRHICommandListImmediate& RHICmdList, const FDistanceFieldAOParameters& Parameters)
{
	//@todo - support multiple views
	const FViewInfo& View = Views[0];

	if (GDistanceFieldAO 
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldAO(View.GetShaderPlatform())
		&& Views.Num() == 1)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderMeshDistanceFieldVis);
		SCOPED_DRAW_EVENT(RHICmdList, VisualizeMeshDistanceFields);

		if (GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI && Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0)
		{
			check(!Scene->DistanceFieldSceneData.HasPendingOperations());

			QUICK_SCOPE_CYCLE_COUNTER(STAT_AOIssueGPUWork);

			const bool bUseGlobalDistanceField = UseGlobalDistanceField(Parameters) && GAOVisualizeGlobalDistanceField != 0;

			{
				if (GAOCulledObjectBuffers.Buffers.MaxObjects < Scene->DistanceFieldSceneData.NumObjectsInBuffer)
				{
					GAOCulledObjectBuffers.Buffers.MaxObjects = Scene->DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
					GAOCulledObjectBuffers.Buffers.Release();
					GAOCulledObjectBuffers.Buffers.Initialize();
				}

				{
					uint32 ClearValues[4] = { 0 };
					RHICmdList.ClearUAV(GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV, ClearValues);

					TShaderMapRef<FCullObjectsForViewCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, Scene, View, Parameters);

					DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, UpdateObjectsGroupSize), 1, 1);
					ComputeShader->UnsetParameters(RHICmdList, Scene);
				}
			}

			TRefCountPtr<IPooledRenderTarget> VisualizeResultRT;

			{
				const FIntPoint BufferSize = GetBufferSizeForAO();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VisualizeResultRT, TEXT("VisualizeDistanceField"));
			}

			{
				SetRenderTarget(RHICmdList, NULL, NULL);

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& ViewInfo = Views[ViewIndex];

					uint32 GroupSizeX = FMath::DivideAndRoundUp(ViewInfo.ViewRect.Size().X / GAODownsampleFactor, GDistanceFieldAOTileSizeX);
					uint32 GroupSizeY = FMath::DivideAndRoundUp(ViewInfo.ViewRect.Size().Y / GAODownsampleFactor, GDistanceFieldAOTileSizeY);

					SCOPED_DRAW_EVENT(RHICmdList, VisualizeMeshDistanceFieldCS);

					FSceneRenderTargetItem& VisualizeResultRTI = VisualizeResultRT->GetRenderTargetItem();
					if (bUseGlobalDistanceField)
					{
						check(View.GlobalDistanceFieldInfo.Clipmaps.Num() > 0);

						TShaderMapRef<TVisualizeMeshDistanceFieldCS<true> > ComputeShader(ViewInfo.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, ViewInfo, VisualizeResultRTI, FVector2D(GroupSizeX, GroupSizeY), Parameters, View.GlobalDistanceFieldInfo);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

						ComputeShader->UnsetParameters(RHICmdList, VisualizeResultRTI);
					}
					else
					{
						TShaderMapRef<TVisualizeMeshDistanceFieldCS<false> > ComputeShader(ViewInfo.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, ViewInfo, VisualizeResultRTI, FVector2D(GroupSizeX, GroupSizeY), Parameters, View.GlobalDistanceFieldInfo);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

						ComputeShader->UnsetParameters(RHICmdList, VisualizeResultRTI);
					}
				}
			}

			{
				FSceneRenderTargets::Get(RHICmdList).BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilRead);

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& ViewInfo = Views[ViewIndex];

					SCOPED_DRAW_EVENT(RHICmdList, UpsampleAO);

					RHICmdList.SetViewport(ViewInfo.ViewRect.Min.X, ViewInfo.ViewRect.Min.Y, 0.0f, ViewInfo.ViewRect.Max.X, ViewInfo.ViewRect.Max.Y, 1.0f);
					RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
					RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

					TShaderMapRef<FPostProcessVS> VertexShader( ViewInfo.ShaderMap );
					TShaderMapRef<FVisualizeDistanceFieldUpsamplePS> PixelShader( ViewInfo.ShaderMap );

					static FGlobalBoundShaderState BoundShaderState;

					SetGlobalBoundShaderState(RHICmdList, ViewInfo.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

					PixelShader->SetParameters(RHICmdList, ViewInfo, VisualizeResultRT);

					DrawRectangle( 
						RHICmdList,
						0, 0, 
						ViewInfo.ViewRect.Width(), ViewInfo.ViewRect.Height(),
						ViewInfo.ViewRect.Min.X / GAODownsampleFactor, ViewInfo.ViewRect.Min.Y / GAODownsampleFactor, 
						ViewInfo.ViewRect.Width() / GAODownsampleFactor, ViewInfo.ViewRect.Height() / GAODownsampleFactor,
						FIntPoint(ViewInfo.ViewRect.Width(), ViewInfo.ViewRect.Height()),
						GetBufferSizeForAO(),
						*VertexShader);
				}
			}
		}
	}
}
