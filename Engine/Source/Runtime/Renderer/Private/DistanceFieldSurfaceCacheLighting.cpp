// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
#include "DistanceFieldGlobalIllumination.h"
#include "PostProcessAmbientOcclusion.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "OneColorShader.h"
#include "BasePassRendering.h"
#include "HeightfieldLighting.h"

int32 GDistanceFieldAO = 1;
FAutoConsoleVariableRef CVarDistanceFieldAO(
	TEXT("r.DistanceFieldAO"),
	GDistanceFieldAO,
	TEXT("Whether the distance field AO feature is allowed, which is used to implement shadows of Movable sky lights from static meshes."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

bool IsDistanceFieldGIAllowed(const FViewInfo& View)
{
	return DoesPlatformSupportDistanceFieldGI(View.GetShaderPlatform())
		&& (View.Family->EngineShowFlags.VisualizeDistanceFieldGI || (View.Family->EngineShowFlags.DistanceFieldGI && GDistanceFieldGI && View.Family->EngineShowFlags.GlobalIllumination));
}

int32 GAOPowerOfTwoBetweenLevels = 2;
FAutoConsoleVariableRef CVarAOPowerOfTwoBetweenLevels(
	TEXT("r.AOPowerOfTwoBetweenLevels"),
	GAOPowerOfTwoBetweenLevels,
	TEXT("Power of two in resolution between refinement levels of the surface cache"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOMinLevel = 1;
FAutoConsoleVariableRef CVarAOMinLevel(
	TEXT("r.AOMinLevel"),
	GAOMinLevel,
	TEXT("Smallest downsample power of 4 to use for surface cache population.\n")
	TEXT("The default is 1, which means every 8 full resolution pixels (BaseDownsampleFactor(2) * 4^1) will be checked for a valid interpolation from the cache or shaded.\n")
	TEXT("Going down to 0 gives less aliasing, and removes the need for gap filling, but costs a lot."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOMaxLevel = FMath::Min(2, GAOMaxSupportedLevel);
FAutoConsoleVariableRef CVarAOMaxLevel(
	TEXT("r.AOMaxLevel"),
	GAOMaxLevel,
	TEXT("Largest downsample power of 4 to use for surface cache population."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOReuseAcrossFrames = 1;
FAutoConsoleVariableRef CVarAOReuseAcrossFrames(
	TEXT("r.AOReuseAcrossFrames"),
	GAOReuseAcrossFrames,
	TEXT("Whether to allow reusing surface cache results across frames."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOTrimOldRecordsFraction = .2f;
FAutoConsoleVariableRef CVarAOTrimOldRecordsFraction(
	TEXT("r.AOTrimOldRecordsFraction"),
	GAOTrimOldRecordsFraction,
	TEXT("When r.AOReuseAcrossFrames is enabled, this is the fraction of the last frame's surface cache records that will not be reused.\n")
	TEXT("Low settings provide better performance, while values closer to 1 give faster lighting updates when dynamic scene changes are happening."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOFillGaps = 1;
FAutoConsoleVariableRef CVarAOFillGaps(
	TEXT("r.AOFillGaps"),
	GAOFillGaps,
	TEXT("Whether to fill in pixels using a screen space filter that had no valid world space interpolation weight from surface cache samples.\n")
	TEXT("This is needed whenever r.AOMinLevel is not 0."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOFillGapsHighQuality = 1;
FAutoConsoleVariableRef CVarAOFillGapsHighQuality(
	TEXT("r.AOFillGapsHighQuality"),
	GAOFillGapsHighQuality,
	TEXT("Whether to use the higher quality gap filling method that does a 5x5 gather, or the cheaper method that just looks up the 4 grid samples."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOUseHistory = 1;
FAutoConsoleVariableRef CVarAOUseHistory(
	TEXT("r.AOUseHistory"),
	GAOUseHistory,
	TEXT("Whether to apply a temporal filter to the distance field AO, which reduces flickering but also adds trails when occluders are moving."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOHistoryStabilityPass = 1;
FAutoConsoleVariableRef CVarAOHistoryStabilityPass(
	TEXT("r.AOHistoryStabilityPass"),
	GAOHistoryStabilityPass,
	TEXT("Whether to gather stable results to fill in holes in the temporal reprojection.  Adds some GPU cost but improves temporal stability with foliage."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOHistoryWeight = .7f;
FAutoConsoleVariableRef CVarAOHistoryWeight(
	TEXT("r.AOHistoryWeight"),
	GAOHistoryWeight,
	TEXT("Amount of last frame's AO to lerp into the final result.  Higher values increase stability, lower values have less streaking under occluder movement."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOHistoryDistanceThreshold = 20;
FAutoConsoleVariableRef CVarAOHistoryDistanceThreshold(
	TEXT("r.AOHistoryDistanceThreshold"),
	GAOHistoryDistanceThreshold,
	TEXT("World space distance threshold needed to discard last frame's DFAO results.  Lower values reduce ghosting from characters when near a wall but increase flickering artifacts."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAORecordRadiusScale = .3f;
FAutoConsoleVariableRef CVarAORecordRadiusScale(
	TEXT("r.AORecordRadiusScale"),
	GAORecordRadiusScale,
	TEXT("Scale applied to the minimum occluder distance to produce the record radius.  This effectively controls how dense shading samples are."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOInterpolationRadiusScale = 1.3f;
FAutoConsoleVariableRef CVarAOInterpolationRadiusScale(
	TEXT("r.AOInterpolationRadiusScale"),
	GAOInterpolationRadiusScale,
	TEXT("Scale applied to record radii during the final interpolation pass.  Values larger than 1 result in world space smoothing."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOMinPointBehindPlaneAngle = 4;
FAutoConsoleVariableRef CVarAOMinPointBehindPlaneAngle(
	TEXT("r.AOMinPointBehindPlaneAngle"),
	GAOMinPointBehindPlaneAngle,
	TEXT("Minimum angle that a point can lie behind a record and still be considered valid.\n")
	TEXT("This threshold helps reduce leaking that happens when interpolating records in front of the shading point, ignoring occluders in between."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOInterpolationMaxAngle = 15;
FAutoConsoleVariableRef CVarAOInterpolationMaxAngle(
	TEXT("r.AOInterpolationMaxAngle"),
	GAOInterpolationMaxAngle,
	TEXT("Largest angle allowed between the shading point's normal and a nearby record's normal."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOInterpolationAngleScale = 1.5f;
FAutoConsoleVariableRef CVarAOInterpolationAngleScale(
	TEXT("r.AOInterpolationAngleScale"),
	GAOInterpolationAngleScale,
	TEXT("Scale applied to angle error during the final interpolation pass.  Values larger than 1 result in smoothing."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOStepExponentScale = .5f;
FAutoConsoleVariableRef CVarAOStepExponentScale(
	TEXT("r.AOStepExponentScale"),
	GAOStepExponentScale,
	TEXT("Exponent used to distribute AO samples along a cone direction."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOMaxViewDistance = 10000;
FAutoConsoleVariableRef CVarAOMaxViewDistance(
	TEXT("r.AOMaxViewDistance"),
	GAOMaxViewDistance,
	TEXT("The maximum distance that AO will be computed at."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

float GAOViewFadeDistanceScale = .7f;
FAutoConsoleVariableRef CVarAOViewFadeDistanceScale(
	TEXT("r.AOViewFadeDistanceScale"),
	GAOViewFadeDistanceScale,
	TEXT("Distance over which AO will fade out as it approaches r.AOMaxViewDistance, as a fraction of r.AOMaxViewDistance."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOScatterTileCulling = 1;
FAutoConsoleVariableRef CVarAOScatterTileCulling(
	TEXT("r.AOScatterTileCulling"),
	GAOScatterTileCulling,
	TEXT("Whether to use the rasterizer for binning occluder objects into screenspace tiles."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOComputeShaderNormalCalculation = 0;
FAutoConsoleVariableRef CVarAOComputeShaderNormalCalculation(
	TEXT("r.AOComputeShaderNormalCalculation"),
	GAOComputeShaderNormalCalculation,
	TEXT("Whether to use the compute shader version of the distance field normal computation."),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOSampleSet = 1;
FAutoConsoleVariableRef CVarAOSampleSet(
	TEXT("r.AOSampleSet"),
	GAOSampleSet,
	TEXT("0 = Original set, 1 = Relaxed set"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOInterpolationStencilTesting = 1;
FAutoConsoleVariableRef CVarAOInterpolationStencilTesting(
	TEXT("r.AOInterpolationStencilTesting"),
	GAOInterpolationStencilTesting,
	TEXT("Whether to stencil out distant pixels from the interpolation splat pass, useful for debugging"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOInterpolationDepthTesting = 1;
FAutoConsoleVariableRef CVarAOInterpolationDepthTesting(
	TEXT("r.AOInterpolationDepthTesting"),
	GAOInterpolationDepthTesting,
	TEXT("Whether to use depth testing during the interpolation splat pass, useful for debugging"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

int32 GAOOverwriteSceneColor = 0;
FAutoConsoleVariableRef CVarAOOverwriteSceneColor(
	TEXT("r.AOOverwriteSceneColor"),
	GAOOverwriteSceneColor,
	TEXT(""),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

DEFINE_LOG_CATEGORY(LogDistanceField);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FAOSampleData2,TEXT("AOSamples2"));

FIntPoint GetBufferSizeForAO()
{
	return FIntPoint::DivideAndRoundDown(GSceneRenderTargets.GetBufferSizeXY(), GAODownsampleFactor);
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
		VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);
		void* Buffer = RHILockVertexBuffer(VertexBufferRHI, 0, Size, RLM_WriteOnly);
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
	TileHeadData.Initialize(sizeof(uint32)* 4, TileDimensions.X * TileDimensions.Y, PF_R32G32B32A32_UINT, BUF_Static);

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

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader());
		ObjectIndirectArguments.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBounds.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectData.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBoxBounds.UnsetUAV(RHICmdList, GetComputeShader());
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
		OutEnvironment.SetDefine(TEXT("NUM_CONE_DIRECTIONS"), NumConeSampleDirections);
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

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		TileConeAxisAndCos.UnsetUAV(RHICmdList, GetComputeShader());
		TileConeDepthRanges.UnsetUAV(RHICmdList, GetComputeShader());
		TileHeadDataUnpacked.UnsetUAV(RHICmdList, GetComputeShader());
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
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
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
		Ar << AOParameters;
		Ar << TileHeadDataUnpacked;
		Ar << TileArrayData;
		Ar << TileConeAxisAndCos;
		Ar << TileConeDepthRanges;
		Ar << NumGroups;
		return bShaderHasOutdatedParameters;
	}

private:
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
		OutEnvironment.SetDefine(TEXT("NUM_CONE_DIRECTIONS"), NumConeSampleDirections);
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
		TileHeadData.Bind(Initializer.ParameterMap, TEXT("TileHeadData"));
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

		TileHeadData.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileHeadData);
		TileArrayData.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileArrayData);
		TileArrayNextAllocation.SetBuffer(RHICmdList, ShaderRHI, TileIntersectionResources->TileArrayNextAllocation);

		SetShaderValue(RHICmdList, ShaderRHI, ViewDimensionsParameter, View.ViewRect);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		TileHeadData.UnsetUAV(RHICmdList, GetComputeShader());
		TileArrayData.UnsetUAV(RHICmdList, GetComputeShader());
		TileArrayNextAllocation.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << TileHeadData;
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
	FRWShaderParameter TileHeadData;
	FRWShaderParameter TileArrayData;
	FRWShaderParameter TileArrayNextAllocation;
	FShaderParameter ViewDimensionsParameter;
	FShaderParameter NumGroups;
};

IMPLEMENT_SHADER_TYPE(,FDistanceFieldBuildTileListCS,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("DistanceFieldAOBuildTileListMain"),SF_Compute);

class FAOLevelParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		CurrentLevelDownsampleFactor.Bind(ParameterMap, TEXT("CurrentLevelDownsampleFactor"));
		AOBufferSize.Bind(ParameterMap, TEXT("AOBufferSize"));
		DownsampleFactorToBaseLevel.Bind(ParameterMap, TEXT("DownsampleFactorToBaseLevel"));
		BaseLevelTexelSize.Bind(ParameterMap, TEXT("BaseLevelTexelSize"));
	}

	template<typename TParamRef>
	void Set(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FSceneView& View, int32 CurrentLevelDownsampleFactorValue)
	{
		SetShaderValue(RHICmdList, ShaderRHI, CurrentLevelDownsampleFactor, CurrentLevelDownsampleFactorValue);

		// Round up, to match render target allocation
		const FVector2D AOBufferSizeValue = FIntPoint::DivideAndRoundUp(GSceneRenderTargets.GetBufferSizeXY(), CurrentLevelDownsampleFactorValue);
		SetShaderValue(RHICmdList, ShaderRHI, AOBufferSize, AOBufferSizeValue);

		SetShaderValue(RHICmdList, ShaderRHI, DownsampleFactorToBaseLevel, CurrentLevelDownsampleFactorValue / GAODownsampleFactor);

		const FIntPoint DownsampledBufferSize = GetBufferSizeForAO();
		const FVector2D BaseLevelBufferSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BaseLevelTexelSize, BaseLevelBufferSizeValue);
	}

	friend FArchive& operator<<(FArchive& Ar,FAOLevelParameters& P)
	{
		Ar << P.CurrentLevelDownsampleFactor << P.AOBufferSize << P.DownsampleFactorToBaseLevel << P.BaseLevelTexelSize;
		return Ar;
	}

private:
	FShaderParameter CurrentLevelDownsampleFactor;
	FShaderParameter AOBufferSize;
	FShaderParameter DownsampleFactorToBaseLevel;
	FShaderParameter BaseLevelTexelSize;
};

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

		DistanceFieldNormal.SetTexture(RHICmdList, ShaderRHI, DistanceFieldNormalValue.ShaderResourceTexture, DistanceFieldNormalValue.UAV);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		DistanceFieldNormal.UnsetUAV(RHICmdList, GetComputeShader());
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
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FShaderParameter UVToTileHead;
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

				ComputeShader->UnsetParameters(RHICmdList);
			}
		}
	}
	else
	{
		SetRenderTarget(RHICmdList, DistanceFieldNormal.TargetableTexture, NULL);

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
				GSceneRenderTargets.GetBufferSizeXY(),
				*VertexShader);
		}
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

		DispatchParameters.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.DispatchParameters);
		ScatterDrawParameters.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->ScatterDrawParameters);

		SetShaderValue(RHICmdList, ShaderRHI, TrimFraction, GAOTrimOldRecordsFraction);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		DispatchParameters.UnsetUAV(RHICmdList, GetComputeShader());
		ScatterDrawParameters.UnsetUAV(RHICmdList, GetComputeShader());
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
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	TCopyIrradianceCacheSamplesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IrradianceCachePositionRadius.Bind(Initializer.ParameterMap, TEXT("IrradianceCachePositionRadius"));
		IrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheNormal"));
		OccluderRadius.Bind(Initializer.ParameterMap, TEXT("OccluderRadius"));
		IrradianceCacheBentNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheBentNormal"));
		IrradianceCacheIrradiance.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheIrradiance"));
		IrradianceCacheTileCoordinate.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheTileCoordinate"));
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

		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCachePositionRadius, SurfaceCacheResources.Level[DepthLevel]->PositionAndRadius.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheNormal, SurfaceCacheResources.Level[DepthLevel]->Normal.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, OccluderRadius, SurfaceCacheResources.Level[DepthLevel]->OccluderRadius.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheBentNormal, SurfaceCacheResources.Level[DepthLevel]->BentNormal.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheIrradiance, SurfaceCacheResources.Level[DepthLevel]->Irradiance.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheTileCoordinate, SurfaceCacheResources.Level[DepthLevel]->TileCoordinate.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, DrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);

		CopyIrradianceCachePositionRadius.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->PositionAndRadius);
		CopyIrradianceCacheNormal.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->Normal);
		CopyOccluderRadius.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->OccluderRadius);
		CopyIrradianceCacheBentNormal.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->BentNormal);
		CopyIrradianceCacheIrradiance.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->Irradiance);
		CopyIrradianceCacheTileCoordinate.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->TileCoordinate);
		ScatterDrawParameters.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.TempResources->ScatterDrawParameters);

		SetShaderValue(RHICmdList, ShaderRHI, TrimFraction, GAOTrimOldRecordsFraction);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		CopyIrradianceCachePositionRadius.UnsetUAV(RHICmdList, GetComputeShader());
		CopyIrradianceCacheNormal.UnsetUAV(RHICmdList, GetComputeShader());
		CopyOccluderRadius.UnsetUAV(RHICmdList, GetComputeShader());
		CopyIrradianceCacheBentNormal.UnsetUAV(RHICmdList, GetComputeShader());
		CopyIrradianceCacheIrradiance.UnsetUAV(RHICmdList, GetComputeShader());
		CopyIrradianceCacheTileCoordinate.UnsetUAV(RHICmdList, GetComputeShader());
		ScatterDrawParameters.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IrradianceCachePositionRadius;
		Ar << IrradianceCacheNormal;
		Ar << OccluderRadius;
		Ar << IrradianceCacheBentNormal;
		Ar << IrradianceCacheIrradiance;
		Ar << IrradianceCacheTileCoordinate;
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

	FShaderResourceParameter IrradianceCachePositionRadius;
	FShaderResourceParameter IrradianceCacheNormal;
	FShaderResourceParameter OccluderRadius;
	FShaderResourceParameter IrradianceCacheBentNormal;
	FShaderResourceParameter IrradianceCacheIrradiance;
	FShaderResourceParameter IrradianceCacheTileCoordinate;
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
		SavedStartIndex.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		SavedStartIndex.UnsetUAV(RHICmdList, GetComputeShader());
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
		OutEnvironment.SetDefine(TEXT("NUM_CONE_DIRECTIONS"), NumConeSampleDirections);
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

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		IrradianceCachePositionRadius.UnsetUAV(RHICmdList, GetComputeShader());
		IrradianceCacheNormal.UnsetUAV(RHICmdList, GetComputeShader());
		IrradianceCacheTileCoordinate.UnsetUAV(RHICmdList, GetComputeShader());
		ScatterDrawParameters.UnsetUAV(RHICmdList, GetComputeShader());
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

class FDebugBuffer : public FRenderResource
{
public:
	virtual void InitDynamicRHI() override
	{
        if(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
        {
            DebugData.Initialize(sizeof(float)* 4, 50, PF_A32B32G32R32F, BUF_Static);
        }
	}

	virtual void ReleaseDynamicRHI() override
	{
        if(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
        {
            DebugData.Release();
        }
	}

	FRWBuffer DebugData;
};

TGlobalResource<FDebugBuffer> GDebugBuffer;

template<bool bSupportIrradiance>
class TConeTraceOcclusionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TConeTraceOcclusionCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_CONE_DIRECTIONS"), NumConeSampleDirections);
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	TConeTraceOcclusionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		IrradianceCachePositionRadius.Bind(Initializer.ParameterMap, TEXT("IrradianceCachePositionRadius"));
		IrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheNormal"));
		OccluderRadius.Bind(Initializer.ParameterMap, TEXT("OccluderRadius"));
		RecordConeVisibility.Bind(Initializer.ParameterMap, TEXT("RecordConeVisibility"));
		RecordConeData.Bind(Initializer.ParameterMap, TEXT("RecordConeData"));
		DebugBuffer.Bind(Initializer.ParameterMap, TEXT("DebugBuffer"));
		IrradianceCacheTileCoordinate.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheTileCoordinate"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
		ViewDimensionsParameter.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
		ThreadToCulledTile.Bind(Initializer.ParameterMap, TEXT("ThreadToCulledTile"));
		TileHeadData.Bind(Initializer.ParameterMap, TEXT("TileHeadData"));
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

	TConeTraceOcclusionCS()
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

		OccluderRadius.SetBuffer(RHICmdList, ShaderRHI, SurfaceCacheResources.Level[DepthLevel]->OccluderRadius);
		RecordConeVisibility.SetBuffer(RHICmdList, ShaderRHI, GTemporaryIrradianceCacheResources.ConeVisibility);
		RecordConeData.SetBuffer(RHICmdList, ShaderRHI, GTemporaryIrradianceCacheResources.ConeData);
		DebugBuffer.SetBuffer(RHICmdList, ShaderRHI, GDebugBuffer.DebugData);

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
		SetSRVParameter(RHICmdList, ShaderRHI, TileHeadData, TileIntersectionResources->TileHeadData.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileArrayData, TileIntersectionResources->TileArrayData.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheTileCoordinate, SurfaceCacheResources.Level[DepthLevel]->TileCoordinate.SRV);

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

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		OccluderRadius.UnsetUAV(RHICmdList, GetComputeShader());
		RecordConeVisibility.UnsetUAV(RHICmdList, GetComputeShader());
		RecordConeData.UnsetUAV(RHICmdList, GetComputeShader());
		DebugBuffer.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << AOLevelParameters;
		Ar << IrradianceCachePositionRadius;
		Ar << IrradianceCacheNormal;
		Ar << OccluderRadius;
		Ar << RecordConeVisibility;
		Ar << RecordConeData;
		Ar << DebugBuffer;
		Ar << IrradianceCacheTileCoordinate;
		Ar << ScatterDrawParameters;
		Ar << SavedStartIndex;
		Ar << ViewDimensionsParameter;
		Ar << ThreadToCulledTile;
		Ar << TileHeadDataUnpacked;
		Ar << TileHeadData;
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
	FRWShaderParameter OccluderRadius;
	FRWShaderParameter RecordConeVisibility;
	FRWShaderParameter RecordConeData;
	FRWShaderParameter DebugBuffer;
	FShaderResourceParameter ScatterDrawParameters;
	FShaderResourceParameter SavedStartIndex;
	FShaderResourceParameter IrradianceCacheTileCoordinate;
	FShaderParameter ViewDimensionsParameter;
	FShaderParameter ThreadToCulledTile;
	FShaderResourceParameter TileHeadDataUnpacked;
	FShaderResourceParameter TileHeadData;
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

IMPLEMENT_SHADER_TYPE(template<>,TConeTraceOcclusionCS<true>,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("ConeTraceOcclusionCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TConeTraceOcclusionCS<false>,TEXT("DistanceFieldSurfaceCacheLightingCompute"),TEXT("ConeTraceOcclusionCS"),SF_Compute);

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
		OutEnvironment.SetDefine(TEXT("NUM_CONE_DIRECTIONS"), NumConeSampleDirections);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FCombineConesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		AOParameters.Bind(Initializer.ParameterMap);
		IrradianceCacheBentNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheBentNormal"));
		IrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheNormal"));
		RecordConeVisibility.Bind(Initializer.ParameterMap, TEXT("RecordConeVisibility"));
		ScatterDrawParameters.Bind(Initializer.ParameterMap, TEXT("ScatterDrawParameters"));
		SavedStartIndex.Bind(Initializer.ParameterMap, TEXT("SavedStartIndex"));
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
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

		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheNormal, SurfaceCacheResources.Level[DepthLevel]->Normal.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, ScatterDrawParameters, SurfaceCacheResources.Level[DepthLevel]->ScatterDrawParameters.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, SavedStartIndex, SurfaceCacheResources.Level[DepthLevel]->SavedStartIndex.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, RecordConeVisibility, GTemporaryIrradianceCacheResources.ConeVisibility.SRV);

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
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		IrradianceCacheBentNormal.UnsetUAV(RHICmdList, GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << AOParameters;
		Ar << IrradianceCacheBentNormal;
		Ar << IrradianceCacheNormal;
		Ar << RecordConeVisibility;
		Ar << ScatterDrawParameters;
		Ar << SavedStartIndex;
		Ar << BentNormalNormalizeFactor;
		return bShaderHasOutdatedParameters;
	}

private:

	FAOParameters AOParameters;
	FRWShaderParameter IrradianceCacheBentNormal;
	FShaderResourceParameter IrradianceCacheNormal;
	FShaderResourceParameter RecordConeVisibility;
	FShaderResourceParameter ScatterDrawParameters;
	FShaderResourceParameter SavedStartIndex;
	FShaderParameter BentNormalNormalizeFactor;
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
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	TIrradianceCacheSplatVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		IrradianceCachePositionRadius.Bind(Initializer.ParameterMap, TEXT("IrradianceCachePositionRadius"));
		IrradianceCacheOccluderRadius.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheOccluderRadius"));
		IrradianceCacheNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheNormal"));
		IrradianceCacheBentNormal.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheBentNormal"));
		IrradianceCacheIrradiance.Bind(Initializer.ParameterMap, TEXT("IrradianceCacheIrradiance"));
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

		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCachePositionRadius, SurfaceCacheResources.Level[DepthLevel]->PositionAndRadius.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheOccluderRadius, SurfaceCacheResources.Level[DepthLevel]->OccluderRadius.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheNormal, SurfaceCacheResources.Level[DepthLevel]->Normal.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheBentNormal, SurfaceCacheResources.Level[DepthLevel]->BentNormal.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, IrradianceCacheIrradiance, SurfaceCacheResources.Level[DepthLevel]->Irradiance.SRV);

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
		Ar << IrradianceCachePositionRadius;
		Ar << IrradianceCacheOccluderRadius;
		Ar << IrradianceCacheNormal;
		Ar << IrradianceCacheBentNormal;
		Ar << IrradianceCacheIrradiance;
		Ar << InterpolationRadiusScale;
		Ar << NormalizedOffsetToPixelCenter;
		Ar << HackExpand;
		Ar << InterpolationBoundingDirection;
		Ar << AOParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter IrradianceCachePositionRadius;
	FShaderResourceParameter IrradianceCacheOccluderRadius;
	FShaderResourceParameter IrradianceCacheNormal;
	FShaderResourceParameter IrradianceCacheBentNormal;
	FShaderResourceParameter IrradianceCacheIrradiance;
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
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
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

template<bool bSupportIrradiance>
class TDistanceFieldAOCombinePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldAOCombinePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TDistanceFieldAOCombinePS() {}

	/** Initialization constructor. */
	TDistanceFieldAOCombinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		AOLevelParameters.Bind(Initializer.ParameterMap);
		BentNormalAOTexture.Bind(Initializer.ParameterMap,TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap,TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap,TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap,TEXT("IrradianceSampler"));
		DistanceFieldNormalTexture.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalTexture"));
		DistanceFieldNormalSampler.Bind(Initializer.ParameterMap, TEXT("DistanceFieldNormalSampler"));
		DebugBuffer.Bind(Initializer.ParameterMap, TEXT("DebugBuffer"));
		DistanceFadeScale.Bind(Initializer.ParameterMap, TEXT("DistanceFadeScale"));
		SelfOcclusionReplacement.Bind(Initializer.ParameterMap, TEXT("SelfOcclusionReplacement"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& InBentNormalTexture, 
		IPooledRenderTarget* InIrradianceTexture, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FDistanceFieldAOParameters& Parameters)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		AOLevelParameters.Set(RHICmdList, ShaderRHI, View, GAODownsampleFactor);

		SetTextureParameter(RHICmdList, ShaderRHI, BentNormalAOTexture, BentNormalAOSampler, TStaticSamplerState<SF_Point>::GetRHI(), InBentNormalTexture.ShaderResourceTexture);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, IrradianceTexture, IrradianceSampler, TStaticSamplerState<SF_Point>::GetRHI(), InIrradianceTexture->GetRenderTargetItem().ShaderResourceTexture);
		}

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldNormalTexture,
			DistanceFieldNormalSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldNormal.ShaderResourceTexture
			);

		SetSRVParameter(RHICmdList, ShaderRHI, DebugBuffer, GDebugBuffer.DebugData.SRV);

		const float DistanceFadeScaleValue = 1.0f / ((1.0f - GAOViewFadeDistanceScale) * GAOMaxViewDistance);
		SetShaderValue(RHICmdList, ShaderRHI, DistanceFadeScale, DistanceFadeScaleValue);

		extern float GVPLSelfOcclusionReplacement;
		SetShaderValue(RHICmdList, ShaderRHI, SelfOcclusionReplacement, GVPLSelfOcclusionReplacement);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << AOParameters;
		Ar << AOLevelParameters;
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << DistanceFieldNormalTexture;
		Ar << DistanceFieldNormalSampler;
		Ar << DebugBuffer;
		Ar << DistanceFadeScale;
		Ar << SelfOcclusionReplacement;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
	FAOLevelParameters AOLevelParameters;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderResourceParameter DistanceFieldNormalTexture;
	FShaderResourceParameter DistanceFieldNormalSampler;
	FShaderResourceParameter DebugBuffer;
	FShaderParameter DistanceFadeScale;
	FShaderParameter SelfOcclusionReplacement;
};

IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldAOCombinePS<true>,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("AOCombinePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldAOCombinePS<false>,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("AOCombinePS"),SF_Pixel);

template<bool bSupportIrradiance, bool bHighQuality>
class TFillGapsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFillGapsPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("HIGH_QUALITY_FILL_GAPS"), bHighQuality ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TFillGapsPS() {}

	/** Initialization constructor. */
	TFillGapsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		BentNormalAOTexelSize.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexelSize"));
		MinDownsampleFactorToBaseLevel.Bind(Initializer.ParameterMap, TEXT("MinDownsampleFactorToBaseLevel"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, FSceneRenderTargetItem& DistanceFieldAOBentNormal2, IPooledRenderTarget* DistanceFieldIrradiance2)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldAOBentNormal2.ShaderResourceTexture
			);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceTexture,
				IrradianceSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				DistanceFieldIrradiance2->GetRenderTargetItem().ShaderResourceTexture
				);
		}

		const FIntPoint DownsampledBufferSize(GSceneRenderTargets.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor));
		const FVector2D BaseLevelTexelSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalAOTexelSize, BaseLevelTexelSizeValue);

		const float MinDownsampleFactor = 1 << (GAOMinLevel * GAOPowerOfTwoBetweenLevels); 
		SetShaderValue(RHICmdList, ShaderRHI, MinDownsampleFactorToBaseLevel, MinDownsampleFactor);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << BentNormalAOTexelSize;
		Ar << MinDownsampleFactorToBaseLevel;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderParameter BentNormalAOTexelSize;
	FShaderParameter MinDownsampleFactorToBaseLevel;
};

#define IMPLEMENT_FILLGAPS_PS_TYPE(bSupportIrradiance,bHighQuality) \
	typedef TFillGapsPS<bSupportIrradiance, bHighQuality> TFillGapsPS##bSupportIrradiance##bHighQuality; \
	IMPLEMENT_SHADER_TYPE(template<>,TFillGapsPS##bSupportIrradiance##bHighQuality,TEXT("DistanceFieldLightingPost"),TEXT("FillGapsPS"),SF_Pixel); 

IMPLEMENT_FILLGAPS_PS_TYPE(true, true);
IMPLEMENT_FILLGAPS_PS_TYPE(true, false);
IMPLEMENT_FILLGAPS_PS_TYPE(false, true);
IMPLEMENT_FILLGAPS_PS_TYPE(false, false);

template<bool bSupportIrradiance>
class TUpdateHistoryPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TUpdateHistoryPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TUpdateHistoryPS() {}

	/** Initialization constructor. */
	TUpdateHistoryPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		BentNormalHistoryTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalHistoryTexture"));
		BentNormalHistorySampler.Bind(Initializer.ParameterMap, TEXT("BentNormalHistorySampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		IrradianceHistoryTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceHistoryTexture"));
		IrradianceHistorySampler.Bind(Initializer.ParameterMap, TEXT("IrradianceHistorySampler"));
		CameraMotion.Bind(Initializer.ParameterMap);
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		HistoryDistanceThreshold.Bind(Initializer.ParameterMap, TEXT("HistoryDistanceThreshold"));
		UseHistoryFilter.Bind(Initializer.ParameterMap, TEXT("UseHistoryFilter"));
		VelocityTexture.Bind(Initializer.ParameterMap, TEXT("VelocityTexture"));
		VelocityTextureSampler.Bind(Initializer.ParameterMap, TEXT("VelocityTextureSampler"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& BentNormalHistoryTextureValue, 
		TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryRT, 
		FSceneRenderTargetItem& DistanceFieldAOBentNormal, 
		IPooledRenderTarget* DistanceFieldIrradiance,
		IPooledRenderTarget* VelocityTextureValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistanceFieldAOBentNormal.ShaderResourceTexture
			);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalHistoryTexture,
			BentNormalHistorySampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			BentNormalHistoryTextureValue.ShaderResourceTexture
			);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceTexture,
				IrradianceSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				DistanceFieldIrradiance->GetRenderTargetItem().ShaderResourceTexture
				);
		}
		
		if (IrradianceHistoryTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceHistoryTexture,
				IrradianceHistorySampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				(*IrradianceHistoryRT)->GetRenderTargetItem().ShaderResourceTexture
				);
		}

		CameraMotion.Set(RHICmdList, View, ShaderRHI);

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GAOHistoryWeight);
		SetShaderValue(RHICmdList, ShaderRHI, HistoryDistanceThreshold, GAOHistoryDistanceThreshold);
		SetShaderValue(RHICmdList, ShaderRHI, UseHistoryFilter, (GAOHistoryStabilityPass ? 1.0f : 0.0f));

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			VelocityTexture,
			VelocityTextureSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			VelocityTextureValue ? VelocityTextureValue->GetRenderTargetItem().ShaderResourceTexture : GBlackTexture->TextureRHI
			);
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << BentNormalHistoryTexture;
		Ar << BentNormalHistorySampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << IrradianceHistoryTexture;
		Ar << IrradianceHistorySampler;
		Ar << CameraMotion;
		Ar << HistoryWeight;
		Ar << HistoryDistanceThreshold;
		Ar << UseHistoryFilter;
		Ar << VelocityTexture;
		Ar << VelocityTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter BentNormalHistoryTexture;
	FShaderResourceParameter BentNormalHistorySampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderResourceParameter IrradianceHistoryTexture;
	FShaderResourceParameter IrradianceHistorySampler;
	FCameraMotionParameters CameraMotion;
	FShaderParameter HistoryWeight;
	FShaderParameter HistoryDistanceThreshold;
	FShaderParameter UseHistoryFilter;
	FShaderResourceParameter VelocityTexture;
	FShaderResourceParameter VelocityTextureSampler;
};

IMPLEMENT_SHADER_TYPE(template<>,TUpdateHistoryPS<true>,TEXT("DistanceFieldLightingPost"),TEXT("UpdateHistoryPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TUpdateHistoryPS<false>,TEXT("DistanceFieldLightingPost"),TEXT("UpdateHistoryPS"),SF_Pixel);


template<bool bSupportIrradiance>
class TFilterHistoryPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TFilterHistoryPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TFilterHistoryPS() {}

	/** Initialization constructor. */
	TFilterHistoryPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BentNormalAOTexture.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap, TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap, TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap, TEXT("IrradianceSampler"));
		HistoryWeight.Bind(Initializer.ParameterMap, TEXT("HistoryWeight"));
		BentNormalAOTexelSize.Bind(Initializer.ParameterMap, TEXT("BentNormalAOTexelSize"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& BentNormalHistoryTextureValue, 
		IPooledRenderTarget* IrradianceHistoryRT)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			BentNormalAOTexture,
			BentNormalAOSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			BentNormalHistoryTextureValue.ShaderResourceTexture
			);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				IrradianceTexture,
				IrradianceSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				IrradianceHistoryRT->GetRenderTargetItem().ShaderResourceTexture
				);
		}

		SetShaderValue(RHICmdList, ShaderRHI, HistoryWeight, GAOHistoryWeight);
		
		const FIntPoint DownsampledBufferSize(GSceneRenderTargets.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor));
		const FVector2D BaseLevelTexelSizeValue(1.0f / DownsampledBufferSize.X, 1.0f / DownsampledBufferSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalAOTexelSize, BaseLevelTexelSizeValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		Ar << HistoryWeight;
		Ar << BentNormalAOTexelSize;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
	FShaderParameter HistoryWeight;
	FShaderParameter BentNormalAOTexelSize;
};

IMPLEMENT_SHADER_TYPE(template<>,TFilterHistoryPS<true>,TEXT("DistanceFieldLightingPost"),TEXT("FilterHistoryPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TFilterHistoryPS<false>,TEXT("DistanceFieldLightingPost"),TEXT("FilterHistoryPS"),SF_Pixel);


void AllocateOrReuseAORenderTarget(TRefCountPtr<IPooledRenderTarget>& Target, const TCHAR* Name, bool bWithAlpha)
{
	if (!Target)
	{
		FIntPoint BufferSize = GetBufferSizeForAO();

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, bWithAlpha ? PF_FloatRGBA : PF_FloatRGB, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
		GRenderTargetPool.FindFreeElement(Desc, Target, Name);
	}
}

void UpdateHistory(
	FRHICommandList& RHICmdList,
	const FViewInfo& View, 
	const TCHAR* BentNormalHistoryRTName,
	const TCHAR* IrradianceHistoryRTName,
	IPooledRenderTarget* VelocityTexture,
	/** Contains last frame's history, if non-NULL.  This will be updated with the new frame's history. */
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState,
	TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryState,
	/** Source */
	TRefCountPtr<IPooledRenderTarget>& BentNormalSource, 
	TRefCountPtr<IPooledRenderTarget>& IrradianceSource, 
	/** Output of Temporal Reprojection for the next step in the pipeline. */
	TRefCountPtr<IPooledRenderTarget>& BentNormalHistoryOutput,
	TRefCountPtr<IPooledRenderTarget>& IrradianceHistoryOutput)
{
	if (BentNormalHistoryState)
	{
		const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

		if (*BentNormalHistoryState 
			&& !View.bCameraCut 
			&& !View.bPrevTransformsReset 
			&& (!bUseDistanceFieldGI || (IrradianceHistoryState && *IrradianceHistoryState)))
		{
			// Reuse a render target from the pool with a consistent name, for vis purposes
			TRefCountPtr<IPooledRenderTarget> NewBentNormalHistory;
			AllocateOrReuseAORenderTarget(NewBentNormalHistory, BentNormalHistoryRTName, true);

			TRefCountPtr<IPooledRenderTarget> NewIrradianceHistory;

			if (bUseDistanceFieldGI)
			{
				AllocateOrReuseAORenderTarget(NewIrradianceHistory, IrradianceHistoryRTName, false);
			}

			SCOPED_DRAW_EVENT(RHICmdList, UpdateHistory);

			{
				FTextureRHIParamRef RenderTargets[2] =
				{
					NewBentNormalHistory->GetRenderTargetItem().TargetableTexture,
					bUseDistanceFieldGI ? NewIrradianceHistory->GetRenderTargetItem().TargetableTexture : NULL,
				};

				SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);

				RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
				RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

				TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TUpdateHistoryPS<true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, (*BentNormalHistoryState)->GetRenderTargetItem(), IrradianceHistoryState, BentNormalSource->GetRenderTargetItem(), IrradianceSource, VelocityTexture);
				}
				else
				{
					TShaderMapRef<TUpdateHistoryPS<false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, (*BentNormalHistoryState)->GetRenderTargetItem(), IrradianceHistoryState, BentNormalSource->GetRenderTargetItem(), IrradianceSource, VelocityTexture);
				}

				VertexShader->SetParameters(RHICmdList, View);

				DrawRectangle( 
					RHICmdList,
					0, 0, 
					View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
					View.ViewRect.Min.X / GAODownsampleFactor, View.ViewRect.Min.Y / GAODownsampleFactor,
					View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
					FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
					GSceneRenderTargets.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
					*VertexShader);

				RHICmdList.CopyToResolveTarget(NewBentNormalHistory->GetRenderTargetItem().TargetableTexture, NewBentNormalHistory->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
				if (bUseDistanceFieldGI)
				{
					RHICmdList.CopyToResolveTarget(NewIrradianceHistory->GetRenderTargetItem().TargetableTexture, NewIrradianceHistory->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
				}
			}

			if (GAOHistoryStabilityPass)
			{
				const FPooledRenderTargetDesc& HistoryDesc = (*BentNormalHistoryState)->GetDesc();

				// Reallocate history if buffer sizes have changed
				if (HistoryDesc.Extent != GSceneRenderTargets.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor))
				{
					GRenderTargetPool.FreeUnusedResource(*BentNormalHistoryState);
					*BentNormalHistoryState = NULL;
					// Update the view state's render target reference with the new history
					AllocateOrReuseAORenderTarget(*BentNormalHistoryState, BentNormalHistoryRTName, true);

					if (bUseDistanceFieldGI)
					{
						GRenderTargetPool.FreeUnusedResource(*IrradianceHistoryState);
						*IrradianceHistoryState = NULL;
						AllocateOrReuseAORenderTarget(*IrradianceHistoryState, IrradianceHistoryRTName, false);
					}
				}

				{
					FTextureRHIParamRef RenderTargets[2] =
					{
						(*BentNormalHistoryState)->GetRenderTargetItem().TargetableTexture,
						bUseDistanceFieldGI ? (*IrradianceHistoryState)->GetRenderTargetItem().TargetableTexture : NULL,
					};

					SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);

					RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
					RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
					RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

					TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

					if (bUseDistanceFieldGI)
					{
						TShaderMapRef<TFilterHistoryPS<true> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
						PixelShader->SetParameters(RHICmdList, View, NewBentNormalHistory->GetRenderTargetItem(), NewIrradianceHistory);
					}
					else
					{
						TShaderMapRef<TFilterHistoryPS<false> > PixelShader(View.ShaderMap);

						static FGlobalBoundShaderState BoundShaderState;
						SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
						PixelShader->SetParameters(RHICmdList, View, NewBentNormalHistory->GetRenderTargetItem(), NewIrradianceHistory);
					}

					VertexShader->SetParameters(RHICmdList, View);

					DrawRectangle( 
						RHICmdList,
						0, 0, 
						View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
						0, 0,
						View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
						FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
						GSceneRenderTargets.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
						*VertexShader);

					RHICmdList.CopyToResolveTarget((*BentNormalHistoryState)->GetRenderTargetItem().TargetableTexture, (*BentNormalHistoryState)->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
					if (bUseDistanceFieldGI)
					{
						RHICmdList.CopyToResolveTarget((*IrradianceHistoryState)->GetRenderTargetItem().TargetableTexture, (*IrradianceHistoryState)->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
					}
				}

				BentNormalHistoryOutput = *BentNormalHistoryState;
				IrradianceHistoryOutput = *IrradianceHistoryState;
			}
			else
			{
				// Update the view state's render target reference with the new history
				*BentNormalHistoryState = NewBentNormalHistory;
				BentNormalHistoryOutput = NewBentNormalHistory;

				*IrradianceHistoryState = NewIrradianceHistory;
				IrradianceHistoryOutput = NewIrradianceHistory;
			}
		}
		else
		{
			// Use the current frame's mask for next frame's history
			*BentNormalHistoryState = BentNormalSource;
			BentNormalHistoryOutput = BentNormalSource;
			BentNormalSource = NULL;

			*IrradianceHistoryState = IrradianceSource;
			IrradianceHistoryOutput = IrradianceSource;
			IrradianceSource = NULL;
		}
	}
	else
	{
		// Temporal reprojection is disabled or there is no view state - pass through
		BentNormalHistoryOutput = BentNormalSource;
		IrradianceHistoryOutput = IrradianceSource;
	}
}

void PostProcessBentNormalAO(
	FRHICommandList& RHICmdList, 
	const FDistanceFieldAOParameters& Parameters, 
	const FViewInfo& View, 
	IPooledRenderTarget* VelocityTexture,
	FSceneRenderTargetItem& BentNormalInterpolation, 
	IPooledRenderTarget* IrradianceInterpolation,
	FSceneRenderTargetItem& DistanceFieldNormal,
	TRefCountPtr<IPooledRenderTarget>& BentNormalOutput,
	TRefCountPtr<IPooledRenderTarget>& IrradianceOutput)
{
	const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

	TRefCountPtr<IPooledRenderTarget> DistanceFieldAOBentNormal;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldIrradiance;
	AllocateOrReuseAORenderTarget(DistanceFieldAOBentNormal, TEXT("DistanceFieldBentNormalAO"), true);

	if (bUseDistanceFieldGI)
	{
		AllocateOrReuseAORenderTarget(DistanceFieldIrradiance, TEXT("DistanceFieldIrradiance"), false);
	}

	TRefCountPtr<IPooledRenderTarget> DistanceFieldAOBentNormal2;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldIrradiance2;

	if (GAOFillGaps)
	{
		AllocateOrReuseAORenderTarget(DistanceFieldAOBentNormal2, TEXT("DistanceFieldBentNormalAO2"), true);

		if (bUseDistanceFieldGI)
		{
			AllocateOrReuseAORenderTarget(DistanceFieldIrradiance2, TEXT("DistanceFieldIrradiance2"), false);
		}
	}

	{
		SCOPED_DRAW_EVENT(RHICmdList, AOCombine);

		TRefCountPtr<IPooledRenderTarget> EffectiveDiffuseIrradianceTarget;

		if (bUseDistanceFieldGI)
		{
			EffectiveDiffuseIrradianceTarget = GAOFillGaps ? DistanceFieldIrradiance2 : DistanceFieldIrradiance;
		}

		FTextureRHIParamRef RenderTargets[2] =
		{
			GAOFillGaps ? DistanceFieldAOBentNormal2->GetRenderTargetItem().TargetableTexture : DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture,
			EffectiveDiffuseIrradianceTarget ? EffectiveDiffuseIrradianceTarget->GetRenderTargetItem().TargetableTexture : NULL,
		};

		SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);

		{
			RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TDistanceFieldAOCombinePS<true> > PixelShader(View.ShaderMap);
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(RHICmdList, View, BentNormalInterpolation, IrradianceInterpolation, DistanceFieldNormal, Parameters);
			}
			else
			{
				TShaderMapRef<TDistanceFieldAOCombinePS<false> > PixelShader(View.ShaderMap);
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(RHICmdList, View, BentNormalInterpolation, IrradianceInterpolation, DistanceFieldNormal, Parameters);
			}

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

		RHICmdList.CopyToResolveTarget(DistanceFieldAOBentNormal2->GetRenderTargetItem().TargetableTexture, DistanceFieldAOBentNormal2->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
		if (bUseDistanceFieldGI)
		{
			RHICmdList.CopyToResolveTarget(EffectiveDiffuseIrradianceTarget->GetRenderTargetItem().TargetableTexture, EffectiveDiffuseIrradianceTarget->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
		}
	}

	if (GAOFillGaps)
	{
		SCOPED_DRAW_EVENT(RHICmdList, FillGaps);

		FTextureRHIParamRef RenderTargets[2] =
		{
			DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture,
			bUseDistanceFieldGI ? DistanceFieldIrradiance->GetRenderTargetItem().TargetableTexture : NULL,
		};

		SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);

		{
			RHICmdList.SetViewport(0, 0, 0.0f, View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor, 1.0f);
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

			if (GAOFillGapsHighQuality)
			{
				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TFillGapsPS<true, true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal2->GetRenderTargetItem(), DistanceFieldIrradiance2);
				}
				else
				{
					TShaderMapRef<TFillGapsPS<false, true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal2->GetRenderTargetItem(), DistanceFieldIrradiance2);
				}
			}
			else
			{
				if (bUseDistanceFieldGI)
				{
					TShaderMapRef<TFillGapsPS<true, false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal2->GetRenderTargetItem(), DistanceFieldIrradiance2);
				}
				else
				{
					TShaderMapRef<TFillGapsPS<false, false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
					PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal2->GetRenderTargetItem(), DistanceFieldIrradiance2);
				}
			}

			VertexShader->SetParameters(RHICmdList, View);

			DrawRectangle( 
				RHICmdList,
				0, 0, 
				View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
				0, 0, 
				View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
				FIntPoint(View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor),
				GSceneRenderTargets.GetBufferSizeXY() / FIntPoint(GAODownsampleFactor, GAODownsampleFactor),
				*VertexShader);
		}

		RHICmdList.CopyToResolveTarget(DistanceFieldAOBentNormal->GetRenderTargetItem().TargetableTexture, DistanceFieldAOBentNormal->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
			
		if (bUseDistanceFieldGI)
		{
			RHICmdList.CopyToResolveTarget(DistanceFieldIrradiance->GetRenderTargetItem().TargetableTexture, DistanceFieldIrradiance->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
		}
	}

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState = ViewState ? &ViewState->DistanceFieldAOHistoryRT : NULL;
	TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryState = ViewState ? &ViewState->DistanceFieldIrradianceHistoryRT : NULL;
	BentNormalOutput = DistanceFieldAOBentNormal;
	IrradianceOutput = DistanceFieldIrradiance;

	if (GAOUseHistory)
	{
		UpdateHistory(
			RHICmdList,
			View, 
			TEXT("DistanceFieldAOHistory"),
			TEXT("DistanceFieldIrradianceHistory"),
			VelocityTexture,
			BentNormalHistoryState,
			IrradianceHistoryState,
			DistanceFieldAOBentNormal, 
			DistanceFieldIrradiance,
			BentNormalOutput,
			IrradianceOutput);
	}
}

enum EAOUpsampleType
{
	AOUpsample_OutputBentNormal,
	AOUpsample_OutputAO,
	AOUpsample_OutputBentNormalAndIrradiance,
	AOUpsample_OutputIrradiance
};

template<EAOUpsampleType UpsampleType>
class TDistanceFieldAOUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistanceFieldAOUpsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), GAODownsampleFactor);
		OutEnvironment.SetDefine(TEXT("OUTPUT_BENT_NORMAL"), (UpsampleType == AOUpsample_OutputBentNormal || UpsampleType == AOUpsample_OutputBentNormalAndIrradiance) ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), (UpsampleType == AOUpsample_OutputIrradiance || UpsampleType == AOUpsample_OutputBentNormalAndIrradiance) ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("OUTPUT_AO"), (UpsampleType == AOUpsample_OutputAO) ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TDistanceFieldAOUpsamplePS() {}

	/** Initialization constructor. */
	TDistanceFieldAOUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		BentNormalAOTexture.Bind(Initializer.ParameterMap,TEXT("BentNormalAOTexture"));
		BentNormalAOSampler.Bind(Initializer.ParameterMap,TEXT("BentNormalAOSampler"));
		IrradianceTexture.Bind(Initializer.ParameterMap,TEXT("IrradianceTexture"));
		IrradianceSampler.Bind(Initializer.ParameterMap,TEXT("IrradianceSampler"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, IPooledRenderTarget* DistanceFieldIrradiance)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, BentNormalAOTexture, BentNormalAOSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), DistanceFieldAOBentNormal->GetRenderTargetItem().ShaderResourceTexture);

		if (IrradianceTexture.IsBound())
		{
			SetTextureParameter(RHICmdList, ShaderRHI, IrradianceTexture, IrradianceSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), DistanceFieldIrradiance->GetRenderTargetItem().ShaderResourceTexture);
		}
	}
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << BentNormalAOTexture;
		Ar << BentNormalAOSampler;
		Ar << IrradianceTexture;
		Ar << IrradianceSampler;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter BentNormalAOTexture;
	FShaderResourceParameter BentNormalAOSampler;
	FShaderResourceParameter IrradianceTexture;
	FShaderResourceParameter IrradianceSampler;
};

#define IMPLEMENT_UPSAMPLE_PS_TYPE(UpsampleType) \
	typedef TDistanceFieldAOUpsamplePS<UpsampleType> TDistanceFieldAOUpsamplePS##UpsampleType; \
	IMPLEMENT_SHADER_TYPE(template<>,TDistanceFieldAOUpsamplePS##UpsampleType,TEXT("DistanceFieldLightingPost"),TEXT("AOUpsamplePS"),SF_Pixel);

IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormal)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputAO)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputBentNormalAndIrradiance)
IMPLEMENT_UPSAMPLE_PS_TYPE(AOUpsample_OutputIrradiance)

void UpsampleBentNormalAO(
	FRHICommandList& RHICmdList, 
	const TArray<FViewInfo>& Views, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldIrradiance,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

		SCOPED_DRAW_EVENT(RHICmdList, UpsampleAO);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

		TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

		if (bVisualizeAmbientOcclusion)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputAO> > PixelShader(View.ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
		}
		else if (bVisualizeGlobalIllumination && bUseDistanceFieldGI)
		{
			TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputIrradiance> > PixelShader(View.ShaderMap);
			static FGlobalBoundShaderState BoundShaderState;
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
			PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
		}
		else
		{
			if (bUseDistanceFieldGI)
			{
				TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputBentNormalAndIrradiance> > PixelShader(View.ShaderMap);
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
			}
			else
			{
				TShaderMapRef<TDistanceFieldAOUpsamplePS<AOUpsample_OutputBentNormal> > PixelShader(View.ShaderMap);
				static FGlobalBoundShaderState BoundShaderState;
				SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);
				PixelShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormal, DistanceFieldIrradiance);
			}
		}

		DrawRectangle( 
			RHICmdList,
			0, 0, 
			View.ViewRect.Width(), View.ViewRect.Height(),
			View.ViewRect.Min.X / GAODownsampleFactor, View.ViewRect.Min.Y / GAODownsampleFactor, 
			View.ViewRect.Width() / GAODownsampleFactor, View.ViewRect.Height() / GAODownsampleFactor,
			FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
			GetBufferSizeForAO(),
			*VertexShader);
	}
}

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

				ComputeShader->UnsetParameters(RHICmdList);
			}

			{
				SCOPED_DRAW_EVENT(RHICmdList, CullObjectsToTiles);

				TShaderMapRef<FObjectCullVS> VertexShader(View.ShaderMap);
				TShaderMapRef<FObjectCullPS> PixelShader(View.ShaderMap);

				TArray<FUnorderedAccessViewRHIParamRef> UAVs;
				PixelShader->GetUAVs(Views[0], UAVs);
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
			RHICmdList.ClearUAV(TileIntersectionResources->TileHeadData.UAV, ClearValues);

			TShaderMapRef<FDistanceFieldBuildTileListCS > ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, Scene, FVector2D(GroupSizeX, GroupSizeY), Parameters);
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

			ComputeShader->UnsetParameters(RHICmdList);
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

		SetShaderValue(RHICmdList, PixelShader->GetPixelShader(), PixelShader->ColorParameter, FLinearColor::Black);

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
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BentNormalInterpolationTarget->GetDesc().Extent, PF_DepthStencil, TexCreate_None, TexCreate_DepthStencilTargetable, false));
			GRenderTargetPool.FindFreeElement(Desc, SplatDepthStencilBuffer, TEXT("DistanceFieldAOSplatDepthBuffer"));

			SetupDepthStencil(RHICmdList, View, SplatDepthStencilBuffer->GetRenderTargetItem(), DistanceFieldNormal, DepthLevel, DestLevelDownsampleFactor);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SplatDepthStencilBuffer);
		}

		SCOPED_DRAW_EVENT(RHICmdList, IrradianceCacheSplat);

		const bool bBindIrradiance = bUseDistanceFieldGI && bFinalInterpolation;

		FTextureRHIParamRef DepthBuffer = SplatDepthStencilBuffer ? SplatDepthStencilBuffer->GetRenderTargetItem().TargetableTexture : NULL;

		// PS4 fast clear which is only triggered through SetRenderTargetsAndClear adds 3ms of GPU idle time currently
		static bool bUseSetAndClear = false;

		if (bUseSetAndClear)
		{
			FRHIRenderTargetView RenderTargets[MaxSimultaneousRenderTargets];
			RenderTargets[0] = FRHIRenderTargetView(BentNormalInterpolationTarget->GetRenderTargetItem().TargetableTexture);
			RenderTargets[1] = FRHIRenderTargetView(bBindIrradiance ? IrradianceInterpolationTarget->GetRenderTargetItem().TargetableTexture : NULL);

			const int32 MRTCount = bBindIrradiance ? 2 : 1;

			FRHIDepthRenderTargetView DepthView(DepthBuffer);
			FRHISetRenderTargetsInfo Info(MRTCount, RenderTargets, DepthView);

			Info.bClearColor = true;
			Info.ClearColors[0] = FLinearColor(0, 0, 0, 0);
			Info.ClearColors[1] = FLinearColor(0, 0, 0, 0);

			// set the render target
			RHICmdList.SetRenderTargetsAndClear(Info);
		}
		else
		{
			FTextureRHIParamRef RenderTargets[2] =
			{
				BentNormalInterpolationTarget->GetRenderTargetItem().TargetableTexture,
				bBindIrradiance ? IrradianceInterpolationTarget->GetRenderTargetItem().TargetableTexture : NULL
			};

			SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bBindIrradiance ? 0 : 1), RenderTargets, DepthBuffer, 0, NULL);

			FLinearColor ClearColors[2] = {FLinearColor(0, 0, 0, 0), FLinearColor(0, 0, 0, 0)};
			RHICmdList.ClearMRT(true, ARRAY_COUNT(ClearColors) - (bBindIrradiance ? 0 : 1), ClearColors, false, 0, false, 0, FIntRect());
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

			const FIntPoint BufferSize = GSceneRenderTargets.GetBufferSizeXY();
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
	
	extern void ListDistanceFieldGIMemory(const FViewInfo& View);
	ListDistanceFieldGIMemory(View);
}

bool SupportsDistanceFieldAO(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform)
{
	return GDistanceFieldAO 
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldAO(ShaderPlatform);
}

bool ShouldRenderDynamicSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return Scene->SkyLight
		&& Scene->SkyLight->ProcessedTexture
		&& !Scene->SkyLight->bWantsStaticShadowing
		&& !Scene->SkyLight->bHasStaticLighting
		&& ViewFamily.EngineShowFlags.SkyLighting
		&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4
		&& !IsSimpleDynamicLightingEnabled() 
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling;
}

bool FDeferredShadingSceneRenderer::ShouldPrepareForDistanceFieldAO() const
{
	return SupportsDistanceFieldAO(Scene->GetFeatureLevel(), Scene->GetShaderPlatform())
		&& ((ShouldRenderDynamicSkyLight(Scene, ViewFamily) && Scene->SkyLight->bCastShadows && ViewFamily.EngineShowFlags.DistanceFieldAO)
			|| ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields
			|| ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO
			|| ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI);
}

FRWBuffer* GDebugBuffer2 = NULL;

IMPLEMENT_SHADER_TYPE(,FTrackGPUProgressCS,TEXT("DistanceFieldGlobalIllumination"),TEXT("TrackGPUProgressCS"),SF_Compute);

void TrackGPUProgress(FRHICommandListImmediate& RHICmdList, uint32 DebugId)
{
	/*
	TShaderMapRef<FTrackGPUProgressCS> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
	ComputeShader->SetParameters(RHICmdList, DebugId);
	DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);
	ComputeShader->UnsetParameters(RHICmdList);

	{
		FRenderQueryRHIRef Query = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);
		uint64 Temp = 0;
		RHICmdList.EndRenderQuery(Query);
		RHICmdList.GetRenderQueryResult(Query, Temp, true);

		uint32* Ptr = (uint32*)RHICmdList.LockVertexBuffer(GDebugBuffer2->Buffer, 0, 4, RLM_ReadOnly);

		UE_LOG(LogDistanceField, Warning, TEXT("DebugId %u,"), *Ptr);
		RHICmdList.UnlockVertexBuffer(GDebugBuffer2->Buffer);
	}*/
}

bool FDeferredShadingSceneRenderer::RenderDistanceFieldAOSurfaceCache(
	FRHICommandListImmediate& RHICmdList, 
	const FDistanceFieldAOParameters& Parameters, 
	const TRefCountPtr<IPooledRenderTarget>& VelocityTexture,
	TRefCountPtr<IPooledRenderTarget>& OutDynamicBentNormalAO, 
	TRefCountPtr<IPooledRenderTarget>& OutDynamicIrradiance,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination)
{
	//@todo - support multiple views
	const FViewInfo& View = Views[0];

	if (SupportsDistanceFieldAO(View.GetFeatureLevel(), View.GetShaderPlatform())
		&& Views.Num() == 1
		// ViewState is used to cache tile intersection resources which have to be sized based on the view
		&& View.State
		&& View.IsPerspectiveProjection())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderDistanceFieldAOSurfaceCache);
		SCOPED_DRAW_EVENT(RHICmdList, DistanceFieldLighting);

		if (GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI && Scene->DistanceFieldSceneData.NumObjectsInBuffer)
		{
			check(!Scene->DistanceFieldSceneData.HasPendingOperations());
			const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);

			if (!GDebugBuffer2)
			{
				GDebugBuffer2 = new FRWBuffer();
				GDebugBuffer2->Initialize(4, 1, PF_R32_UINT);
			}

			GenerateBestSpacedVectors();

			// Create surface cache state that will persist across frames if needed
			if (!Scene->SurfaceCacheResources)
			{
				Scene->SurfaceCacheResources = new FSurfaceCacheResources();
			}

			if (bListMemoryNextFrame)
			{
				bListMemoryNextFrame = false;
				ListDistanceFieldLightingMemory(View);
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

			QUICK_SCOPE_CYCLE_COUNTER(STAT_AOIssueGPUWork);

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
					RHICmdList.ClearUAV(GAOCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV, ClearValues);

					TShaderMapRef<FCullObjectsForViewCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, Scene, View, Parameters);

					DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, UpdateObjectsGroupSize), 1, 1);
					ComputeShader->UnsetParameters(RHICmdList);
				}
			}
			
			TRefCountPtr<IPooledRenderTarget> DistanceFieldNormal;

			{
				const FIntPoint BufferSize = GetBufferSizeForAO();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(Desc, DistanceFieldNormal, TEXT("DistanceFieldNormal"));
			}

			// Compute the distance field normal, this is used for surface caching instead of the GBuffer normal because it matches the occluding geometry
			ComputeDistanceFieldNormal(RHICmdList, Views, DistanceFieldNormal->GetRenderTargetItem(), Parameters);

			// Intersect objects with screen tiles, build lists
			FIntPoint TileListGroupSize = BuildTileObjectLists(RHICmdList, Scene, Views, DistanceFieldNormal->GetRenderTargetItem(), Parameters);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, DistanceFieldNormal);

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

							ComputeShader->UnsetParameters(RHICmdList);
						}

						if (bUseDistanceFieldGI)
						{
							TShaderMapRef<TCopyIrradianceCacheSamplesCS<true> > ComputeShader(View.ShaderMap);

							RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
							ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
							DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

							ComputeShader->UnsetParameters(RHICmdList);
						}
						else
						{
							TShaderMapRef<TCopyIrradianceCacheSamplesCS<false> > ComputeShader(View.ShaderMap);

							RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
							ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
							DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

							ComputeShader->UnsetParameters(RHICmdList);
						}

						Swap(SurfaceCacheResources.Level[DepthLevel], SurfaceCacheResources.TempResources);
					}
				}
			}

			if (bUseDistanceFieldGI)
			{
				extern void UpdateVPLs(
					FRHICommandListImmediate& RHICmdList,
					const FViewInfo& View,
					const FScene* Scene,
					const FDistanceFieldAOParameters& Parameters);

				UpdateVPLs(RHICmdList, View, Scene, Parameters);
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
					FIntPoint AOBufferSize = FIntPoint::DivideAndRoundUp(GSceneRenderTargets.GetBufferSizeXY(), DestLevelDownsampleFactor);
					FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(AOBufferSize, PF_FloatRGBA, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
					GRenderTargetPool.FindFreeElement(Desc, DistanceFieldAOBentNormalSplat, TEXT("DistanceFieldAOBentNormalSplat"));
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

						ComputeShader->UnsetParameters(RHICmdList);
					}

					// Create new records which haven't been shaded yet for shading points which don't have a valid interpolation from existing records
					for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						const FViewInfo& ViewInfo = Views[ViewIndex];

						FIntPoint DownsampledViewSize = FIntPoint::DivideAndRoundUp(ViewInfo.ViewRect.Size(), DestLevelDownsampleFactor);
						uint32 GroupSizeX = FMath::DivideAndRoundUp(DownsampledViewSize.X, GDistanceFieldAOTileSizeX);
						uint32 GroupSizeY = FMath::DivideAndRoundUp(DownsampledViewSize.Y, GDistanceFieldAOTileSizeY);

						TShaderMapRef<FPopulateCacheCS> ComputeShader(ViewInfo.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, ViewInfo, DistanceFieldAOBentNormalSplat->GetRenderTargetItem(), DistanceFieldNormal->GetRenderTargetItem(), DestLevelDownsampleFactor, DepthLevel, TileListGroupSize, Parameters);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

						ComputeShader->UnsetParameters(RHICmdList);
					}

					{	
						TShaderMapRef<TSetupFinalGatherIndirectArgumentsCS<true> > ComputeShader(View.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, DepthLevel);
						DispatchComputeShader(RHICmdList, *ComputeShader, 1, 1, 1);

						ComputeShader->UnsetParameters(RHICmdList);
					}

					// Compute lighting for the new surface cache records by cone-stepping through the object distance fields
					if (bUseDistanceFieldGI)
					{
						TShaderMapRef<TConeTraceOcclusionCS<true> > ComputeShader(View.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormalSplat->GetRenderTargetItem(), DistanceFieldNormal->GetRenderTargetItem(), DestLevelDownsampleFactor, DepthLevel, TileListGroupSize, Parameters);
						DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

						ComputeShader->UnsetParameters(RHICmdList);
					}
					else
					{
						TShaderMapRef<TConeTraceOcclusionCS<false> > ComputeShader(View.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, View, DistanceFieldAOBentNormalSplat->GetRenderTargetItem(), DistanceFieldNormal->GetRenderTargetItem(), DestLevelDownsampleFactor, DepthLevel, TileListGroupSize, Parameters);
						DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

						ComputeShader->UnsetParameters(RHICmdList);
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

					ComputeShader->UnsetParameters(RHICmdList);
				}

				// Compute and store the final bent normal now that all occlusion sources have been computed (distance fields, heightfields)
				{
					TShaderMapRef<FCombineConesCS> ComputeShader(View.ShaderMap);

					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View, DepthLevel, Parameters, NULL, FMatrix::Identity, NULL, NULL);
					DispatchIndirectComputeShader(RHICmdList, *ComputeShader, SurfaceCacheResources.DispatchParameters.Buffer, 0);

					ComputeShader->UnsetParameters(RHICmdList);
				}
			}

			TRefCountPtr<IPooledRenderTarget> BentNormalAccumulation;
			TRefCountPtr<IPooledRenderTarget> IrradianceAccumulation;

			{
				FIntPoint BufferSize = GetBufferSizeForAO();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(Desc, BentNormalAccumulation, TEXT("BentNormalAccumulation"));

				if (bUseDistanceFieldGI)
				{
					GRenderTargetPool.FindFreeElement(Desc, IrradianceAccumulation, TEXT("IrradianceAccumulation"));
				}
			}

			// Splat the surface cache records onto the opaque pixels, using less strict weighting so the lighting is smoothed in world space
			{
				SCOPED_DRAW_EVENT(RHICmdList, FinalIrradianceCacheSplat);
				RenderIrradianceCacheInterpolation(RHICmdList, View, BentNormalAccumulation, IrradianceAccumulation, DistanceFieldNormal->GetRenderTargetItem(), 0, GAODownsampleFactor, Parameters, true);
			}

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, BentNormalAccumulation);

			// Post process the AO to cover over artifacts
			TRefCountPtr<IPooledRenderTarget> BentNormalOutput;
			TRefCountPtr<IPooledRenderTarget> IrradianceOutput;

			PostProcessBentNormalAO(
				RHICmdList, 
				Parameters, 
				View, 
				VelocityTexture, 
				BentNormalAccumulation->GetRenderTargetItem(), 
				IrradianceAccumulation, 
				DistanceFieldNormal->GetRenderTargetItem(), 
				BentNormalOutput, 
				IrradianceOutput);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, BentNormalOutput);

			if (bVisualizeAmbientOcclusion || bVisualizeGlobalIllumination)
			{
				GSceneRenderTargets.BeginRenderingSceneColor(RHICmdList);
			}
			else
			{
				FPooledRenderTargetDesc Desc = GSceneRenderTargets.GetSceneColor()->GetDesc();
				// Make sure we get a signed format
				Desc.Format = PF_FloatRGBA;
				GRenderTargetPool.FindFreeElement(Desc, OutDynamicBentNormalAO, TEXT("DynamicBentNormalAO"));

				if (bUseDistanceFieldGI)
				{
					Desc.Format = PF_FloatRGB;
					GRenderTargetPool.FindFreeElement(Desc, OutDynamicIrradiance, TEXT("DynamicIrradiance"));
				}

				FTextureRHIParamRef RenderTargets[2] =
				{
					OutDynamicBentNormalAO->GetRenderTargetItem().TargetableTexture,
					bUseDistanceFieldGI ? OutDynamicIrradiance->GetRenderTargetItem().TargetableTexture : NULL
				};
				SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets) - (bUseDistanceFieldGI ? 0 : 1), RenderTargets, FTextureRHIParamRef(), 0, NULL);
			}

			// Upsample to full resolution, write to output
			UpsampleBentNormalAO(RHICmdList, Views, BentNormalOutput, IrradianceOutput, bVisualizeAmbientOcclusion, bVisualizeGlobalIllumination);

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
		OutEnvironment.SetDefine(TEXT("APPLY_SHADOWING"), (uint32)(bApplyShadowing ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("SUPPORT_IRRADIANCE"), bSupportIrradiance ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TDynamicSkyLightDiffusePS() {}

	/** Initialization constructor. */
	TDynamicSkyLightDiffusePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		PreIntegratedGF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGF"));
		PreIntegratedGFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGFSampler"));
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

		SetTextureParameter(RHICmdList, ShaderRHI, PreIntegratedGF, PreIntegratedGFSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture );

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
		Ar << PreIntegratedGF << PreIntegratedGFSampler;
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
	FShaderResourceParameter PreIntegratedGF;
	FShaderResourceParameter PreIntegratedGFSampler;
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
	return Scene->SkyLight->bCastShadows
		&& ViewFamily.EngineShowFlags.DistanceFieldAO 
		&& !ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO
		&& !ViewFamily.EngineShowFlags.VisualizeDistanceFieldGI
		&& !ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields;
}

void FDeferredShadingSceneRenderer::RenderDynamicSkyLighting(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& VelocityTexture, TRefCountPtr<IPooledRenderTarget>& DynamicBentNormalAO)
{
	if (ShouldRenderDynamicSkyLight(Scene, ViewFamily))
	{
		SCOPED_DRAW_EVENT(RHICmdList, SkyLightDiffuse);

		bool bApplyShadowing = false;

		FDistanceFieldAOParameters Parameters(Scene->SkyLight->OcclusionMaxDistance, Scene->SkyLight->Contrast);
		TRefCountPtr<IPooledRenderTarget> DynamicIrradiance;

		if (ShouldRenderDistanceFieldAO())
		{
			bApplyShadowing = RenderDistanceFieldAOSurfaceCache(RHICmdList, Parameters, VelocityTexture, DynamicBentNormalAO, DynamicIrradiance, false, false);
		}

		GSceneRenderTargets.BeginRenderingSceneColor(RHICmdList);

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
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
			}

			const bool bUseDistanceFieldGI = IsDistanceFieldGIAllowed(View);
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
				GSceneRenderTargets.GetBufferSizeXY(),
				*VertexShader);
		}
	}
}

class FVisualizeMeshDistanceFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FVisualizeMeshDistanceFieldCS, Global);
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
	FVisualizeMeshDistanceFieldCS() {}

	/** Initialization constructor. */
	FVisualizeMeshDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		VisualizeMeshDistanceFields.Bind(Initializer.ParameterMap, TEXT("VisualizeMeshDistanceFields"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		ObjectParameters.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		FSceneRenderTargetItem& VisualizeMeshDistanceFieldsValue, 
		FVector2D NumGroupsValue,
		const FDistanceFieldAOParameters& Parameters)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		VisualizeMeshDistanceFields.SetTexture(RHICmdList, ShaderRHI, VisualizeMeshDistanceFieldsValue.ShaderResourceTexture, VisualizeMeshDistanceFieldsValue.UAV);

		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
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
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter VisualizeMeshDistanceFields;
	FShaderParameter NumGroups;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FDeferredPixelShaderParameters DeferredParameters;
	FAOParameters AOParameters;
};

IMPLEMENT_SHADER_TYPE(,FVisualizeMeshDistanceFieldCS,TEXT("DistanceFieldSurfaceCacheLighting"),TEXT("VisualizeMeshDistanceFieldCS"),SF_Compute);


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
					ComputeShader->UnsetParameters(RHICmdList);
				}
			}

			TRefCountPtr<IPooledRenderTarget> VisualizeResultRT;

			{
				const FIntPoint BufferSize = GetBufferSizeForAO();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_FloatRGBA, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(Desc, VisualizeResultRT, TEXT("VisualizeDistanceField"));
			}

			{
				SetRenderTarget(RHICmdList, NULL, NULL);

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& ViewInfo = Views[ViewIndex];

					uint32 GroupSizeX = FMath::DivideAndRoundUp(ViewInfo.ViewRect.Size().X / GAODownsampleFactor, GDistanceFieldAOTileSizeX);
					uint32 GroupSizeY = FMath::DivideAndRoundUp(ViewInfo.ViewRect.Size().Y / GAODownsampleFactor, GDistanceFieldAOTileSizeY);

					{
						SCOPED_DRAW_EVENT(RHICmdList, VisualizeMeshDistanceFieldCS);
						TShaderMapRef<FVisualizeMeshDistanceFieldCS> ComputeShader(ViewInfo.ShaderMap);

						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, ViewInfo, VisualizeResultRT->GetRenderTargetItem(), FVector2D(GroupSizeX, GroupSizeY), Parameters);
						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

						ComputeShader->UnsetParameters(RHICmdList);
					}
				}
			}

			{
				GSceneRenderTargets.BeginRenderingSceneColor(RHICmdList);

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