// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GlobalDistanceField.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "GlobalDistanceField.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "HeightfieldLighting.h"

int32 GAOGlobalDistanceField = 1;
FAutoConsoleVariableRef CVarAOGlobalDistanceField(
	TEXT("r.AOGlobalDistanceField"),
	GAOGlobalDistanceField,
	TEXT("Whether to use a global distance field to optimize occlusion cone traces.\n")
	TEXT("The global distance field is created by compositing object distance fields into clipmaps as the viewer moves through the level."),
	ECVF_RenderThreadSafe
	);

int32 GAOUpdateGlobalDistanceField = 1;
FAutoConsoleVariableRef CVarAOUpdateGlobalDistanceField(
	TEXT("r.AOUpdateGlobalDistanceField"),
	GAOUpdateGlobalDistanceField,
	TEXT("Whether to update the global distance field, useful for debugging."),
	ECVF_RenderThreadSafe
	);

int32 GAOGlobalDistanceFieldPartialUpdates = 1;
FAutoConsoleVariableRef CVarAOGlobalDistanceFieldPartialUpdates(
	TEXT("r.AOGlobalDistanceFieldPartialUpdates"),
	GAOGlobalDistanceFieldPartialUpdates,
	TEXT("Whether to allow partial updates of the global distance field.  When profiling it's useful to disable this and get the worst case composition time that happens on camera cuts."),
	ECVF_RenderThreadSafe
	);

int32 GAOGlobalDistanceFieldStaggeredUpdates = 1;
FAutoConsoleVariableRef CVarAOGlobalDistanceFieldStaggeredUpdatess(
	TEXT("r.AOGlobalDistanceFieldStaggeredUpdates"),
	GAOGlobalDistanceFieldStaggeredUpdates,
	TEXT("Whether to allow the larger clipmaps to be updated less frequently."),
	ECVF_RenderThreadSafe
	);

int32 GAOLogGlobalDistanceFieldModifiedPrimitives = 0;
FAutoConsoleVariableRef CVarAOLogGlobalDistanceFieldModifiedPrimitives(
	TEXT("r.AOGlobalDistanceFieldLogModifiedPrimitives"),
	GAOLogGlobalDistanceFieldModifiedPrimitives,
	TEXT("Whether to log primitive modifications (add, remove, updatetransform) that caused an update of the global distance field.\n")
	TEXT("This can be useful for tracking down why updating the global distance field is always costing a lot, since it should be mostly cached."),
	ECVF_RenderThreadSafe
	);

int32 GAOVisualizeGlobalDistanceField = 0;
FAutoConsoleVariableRef CVarAOVisualizeGlobalDistanceField(
	TEXT("r.AOVisualizeGlobalDistanceField"),
	GAOVisualizeGlobalDistanceField,
	TEXT("Whether to visualize the global distance field instead of the object DFs with the 'Mesh Distance Fields' Visualize mode"),
	ECVF_RenderThreadSafe
	);

float GAOGlobalDFClipmapDistanceExponent = 2;
FAutoConsoleVariableRef CVarAOGlobalDFClipmapDistanceExponent(
	TEXT("r.AOGlobalDFClipmapDistanceExponent"),
	GAOGlobalDFClipmapDistanceExponent,
	TEXT("Exponent used to derive each clipmap's size, together with r.AOInnerGlobalDFClipmapDistance."),
	ECVF_RenderThreadSafe
	);

int32 GAOGlobalDFResolution = 128;
FAutoConsoleVariableRef CVarAOGlobalDFResolution(
	TEXT("r.AOGlobalDFResolution"),
	GAOGlobalDFResolution,
	TEXT("Resolution of the global distance field.  Higher values increase fidelity but also increase memory and composition cost."),
	ECVF_RenderThreadSafe
	);

float GAOGlobalDFStartDistance = 200;
FAutoConsoleVariableRef CVarAOGlobalDFStartDistance(
	TEXT("r.AOGlobalDFStartDistance"),
	GAOGlobalDFStartDistance,
	TEXT("World space distance along a cone trace to switch to using the global distance field instead of the object distance fields.\n")
	TEXT("This has to be large enough to hide the low res nature of the global distance field, but smaller values result in faster cone tracing."),
	ECVF_RenderThreadSafe
	);

void FGlobalDistanceFieldInfo::UpdateParameterData(float MaxOcclusionDistance)
{
	if (Clipmaps.Num() > 0)
	{
		for (int32 ClipmapIndex = 0; ClipmapIndex < GMaxGlobalDistanceFieldClipmaps; ClipmapIndex++)
		{
			FTextureRHIParamRef TextureValue = ClipmapIndex < Clipmaps.Num() 
				? Clipmaps[ClipmapIndex].RenderTarget->GetRenderTargetItem().ShaderResourceTexture 
				: NULL;

			ParameterData.Textures[ClipmapIndex] = TextureValue;

			if (ClipmapIndex < Clipmaps.Num())
			{
				const FGlobalDistanceFieldClipmap& Clipmap = Clipmaps[ClipmapIndex];
				ParameterData.CenterAndExtent[ClipmapIndex] = FVector4(Clipmap.Bounds.GetCenter(), Clipmap.Bounds.GetExtent().X);

				// GlobalUV = (WorldPosition - GlobalVolumeCenterAndExtent[ClipmapIndex].xyz + GlobalVolumeScollOffset[ClipmapIndex].xyz) / (GlobalVolumeCenterAndExtent[ClipmapIndex].w * 2) + .5f;
				// WorldToUVMul = 1.0f / (GlobalVolumeCenterAndExtent[ClipmapIndex].w * 2)
				// WorldToUVAdd = (GlobalVolumeScollOffset[ClipmapIndex].xyz - GlobalVolumeCenterAndExtent[ClipmapIndex].xyz) / (GlobalVolumeCenterAndExtent[ClipmapIndex].w * 2) + .5f
				const FVector WorldToUVAdd = (Clipmap.ScrollOffset - Clipmap.Bounds.GetCenter()) / (Clipmap.Bounds.GetExtent().X * 2) + FVector(.5f);
				ParameterData.WorldToUVAddAndMul[ClipmapIndex] = FVector4(WorldToUVAdd, 1.0f / (Clipmap.Bounds.GetExtent().X * 2));
			}
			else
			{
				ParameterData.CenterAndExtent[ClipmapIndex] = FVector4(0, 0, 0, 0);
				ParameterData.WorldToUVAddAndMul[ClipmapIndex] = FVector4(0, 0, 0, 0);
			}
		}

		ParameterData.GlobalDFResolution = GAOGlobalDFResolution;

		extern float GAOConeHalfAngle;
		const float GlobalMaxSphereQueryRadius = MaxOcclusionDistance / (1 + FMath::Tan(GAOConeHalfAngle));
		ParameterData.MaxDistance = GlobalMaxSphereQueryRadius;
	}
	else
	{
		FPlatformMemory::Memzero(&ParameterData, sizeof(ParameterData));
	}

	bInitialized = true;
}

TGlobalResource<FDistanceFieldObjectBufferResource> GGlobalDistanceFieldCulledObjectBuffers;

uint32 CullObjectsGroupSize = 64;

class FCullObjectsForVolumeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullObjectsForVolumeCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CULLOBJECTS_THREADGROUP_SIZE"), CullObjectsGroupSize);
	}

	FCullObjectsForVolumeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
		ObjectIndirectArguments.Bind(Initializer.ParameterMap, TEXT("ObjectIndirectArguments"));
		CulledObjectBounds.Bind(Initializer.ParameterMap, TEXT("CulledObjectBounds"));
		CulledObjectData.Bind(Initializer.ParameterMap, TEXT("CulledObjectData"));
		CulledObjectBoxBounds.Bind(Initializer.ParameterMap, TEXT("CulledObjectBoxBounds"));
		AOGlobalMaxSphereQueryRadius.Bind(Initializer.ParameterMap, TEXT("AOGlobalMaxSphereQueryRadius"));
		VolumeBounds.Bind(Initializer.ParameterMap, TEXT("VolumeBounds"));
	}

	FCullObjectsForVolumeCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, const FSceneView& View, float MaxOcclusionDistance, const FVector4& VolumeBoundsValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, *(Scene->DistanceFieldSceneData.ObjectBuffers), Scene->DistanceFieldSceneData.NumObjectsInBuffer);

		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[0] = GGlobalDistanceFieldCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GGlobalDistanceFieldCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GGlobalDistanceFieldCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GGlobalDistanceFieldCulledObjectBuffers.Buffers.BoxBounds.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		ObjectIndirectArguments.SetBuffer(RHICmdList, ShaderRHI, GGlobalDistanceFieldCulledObjectBuffers.Buffers.ObjectIndirectArguments);
		CulledObjectBounds.SetBuffer(RHICmdList, ShaderRHI, GGlobalDistanceFieldCulledObjectBuffers.Buffers.Bounds);
		CulledObjectData.SetBuffer(RHICmdList, ShaderRHI, GGlobalDistanceFieldCulledObjectBuffers.Buffers.Data);
		CulledObjectBoxBounds.SetBuffer(RHICmdList, ShaderRHI, GGlobalDistanceFieldCulledObjectBuffers.Buffers.BoxBounds);

		extern float GAOConeHalfAngle;
		const float GlobalMaxSphereQueryRadius = MaxOcclusionDistance / (1 + FMath::Tan(GAOConeHalfAngle));
		SetShaderValue(RHICmdList, ShaderRHI, AOGlobalMaxSphereQueryRadius, GlobalMaxSphereQueryRadius);
		SetShaderValue(RHICmdList, ShaderRHI, VolumeBounds, VolumeBoundsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FScene* Scene)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), *(Scene->DistanceFieldSceneData.ObjectBuffers));
		ObjectIndirectArguments.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBounds.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectData.UnsetUAV(RHICmdList, GetComputeShader());
		CulledObjectBoxBounds.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[0] = GGlobalDistanceFieldCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV;
		OutUAVs[1] = GGlobalDistanceFieldCulledObjectBuffers.Buffers.Bounds.UAV;
		OutUAVs[2] = GGlobalDistanceFieldCulledObjectBuffers.Buffers.Data.UAV;
		OutUAVs[3] = GGlobalDistanceFieldCulledObjectBuffers.Buffers.BoxBounds.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ObjectBufferParameters;
		Ar << ObjectIndirectArguments;
		Ar << CulledObjectBounds;
		Ar << CulledObjectData;
		Ar << CulledObjectBoxBounds;
		Ar << AOGlobalMaxSphereQueryRadius;
		Ar << VolumeBounds;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
	FRWShaderParameter ObjectIndirectArguments;
	FRWShaderParameter CulledObjectBounds;
	FRWShaderParameter CulledObjectData;
	FRWShaderParameter CulledObjectBoxBounds;
	FShaderParameter AOGlobalMaxSphereQueryRadius;
	FShaderParameter VolumeBounds;
};

IMPLEMENT_SHADER_TYPE(,FCullObjectsForVolumeCS,TEXT("GlobalDistanceField"),TEXT("CullObjectsForVolumeCS"),SF_Compute);

const int32 GMaxGridCulledObjects = 2047;

class FObjectGridBuffers : public FRenderResource
{
public:

	int32 GridDimension;
	FRWBuffer CulledObjectGrid;

	FObjectGridBuffers()
	{
		GridDimension = 0;
	}

	virtual void InitDynamicRHI()  override
	{
		if (GridDimension > 0)
		{
			CulledObjectGrid.Initialize(sizeof(uint32), GridDimension * GridDimension * GridDimension * (GMaxGridCulledObjects + 1), PF_R32_UINT);
		}
	}

	virtual void ReleaseDynamicRHI() override
	{
		CulledObjectGrid.Release();
	}

	size_t GetSizeBytes() const
	{
		return CulledObjectGrid.NumBytes;
	}
};

TGlobalResource<FObjectGridBuffers> GObjectGridBuffers;

const int32 GCullGridTileSize = 16;

class FCullObjectsToGridCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCullObjectsToGridCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CULL_GRID_TILE_SIZE"), GCullGridTileSize);
		OutEnvironment.SetDefine(TEXT("MAX_GRID_CULLED_DF_OBJECTS"), GMaxGridCulledObjects);
	}

	FCullObjectsToGridCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CulledObjectBufferParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
		CulledObjectGrid.Bind(Initializer.ParameterMap, TEXT("CulledObjectGrid"));
		CullGridDimension.Bind(Initializer.ParameterMap, TEXT("CullGridDimension"));
		VolumeTexelSize.Bind(Initializer.ParameterMap, TEXT("VolumeTexelSize"));
		UpdateRegionVolumeMin.Bind(Initializer.ParameterMap, TEXT("UpdateRegionVolumeMin"));
		ClipmapIndex.Bind(Initializer.ParameterMap, TEXT("ClipmapIndex"));
		AOGlobalMaxSphereQueryRadius.Bind(Initializer.ParameterMap, TEXT("AOGlobalMaxSphereQueryRadius"));
	}

	FCullObjectsToGridCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FScene* Scene, 
		const FSceneView& View, 
		float MaxOcclusionDistance, 
		const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo,
		int32 ClipmapIndexValue,
		const FVolumeUpdateRegion& UpdateRegion)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		CulledObjectBufferParameters.Set(RHICmdList, ShaderRHI, GGlobalDistanceFieldCulledObjectBuffers.Buffers);
		GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, GlobalDistanceFieldInfo.ParameterData);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, GObjectGridBuffers.CulledObjectGrid.UAV);
		CulledObjectGrid.SetBuffer(RHICmdList, ShaderRHI, GObjectGridBuffers.CulledObjectGrid);

		const FIntVector GridDimensionValue(
			FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.X, GCullGridTileSize),
			FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Y, GCullGridTileSize),
			FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Z, GCullGridTileSize));

		SetShaderValue(RHICmdList, ShaderRHI, CullGridDimension, GridDimensionValue);
		SetShaderValue(RHICmdList, ShaderRHI, VolumeTexelSize, FVector(1.0f / GAOGlobalDFResolution));
		SetShaderValue(RHICmdList, ShaderRHI, UpdateRegionVolumeMin, UpdateRegion.Bounds.Min);
		SetShaderValue(RHICmdList, ShaderRHI, ClipmapIndex, ClipmapIndexValue);

		extern float GAOConeHalfAngle;
		const float GlobalMaxSphereQueryRadius = MaxOcclusionDistance / (1 + FMath::Tan(GAOConeHalfAngle));
		SetShaderValue(RHICmdList, ShaderRHI, AOGlobalMaxSphereQueryRadius, GlobalMaxSphereQueryRadius);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		CulledObjectGrid.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, GObjectGridBuffers.CulledObjectGrid.UAV);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CulledObjectBufferParameters;
		Ar << GlobalDistanceFieldParameters;
		Ar << CulledObjectGrid;
		Ar << CullGridDimension;
		Ar << VolumeTexelSize;
		Ar << UpdateRegionVolumeMin;
		Ar << ClipmapIndex;
		Ar << AOGlobalMaxSphereQueryRadius;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldCulledObjectBufferParameters CulledObjectBufferParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
	FRWShaderParameter CulledObjectGrid;
	FShaderParameter CullGridDimension;
	FShaderParameter VolumeTexelSize;
	FShaderParameter UpdateRegionVolumeMin;
	FShaderParameter ClipmapIndex;
	FShaderParameter AOGlobalMaxSphereQueryRadius;
};

IMPLEMENT_SHADER_TYPE(,FCullObjectsToGridCS,TEXT("GlobalDistanceField"),TEXT("CullObjectsToGridCS"),SF_Compute);

const int32 CompositeTileSize = 4;

class FCompositeObjectDistanceFieldsCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeObjectDistanceFieldsCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPOSITE_THREADGROUP_SIZE"), CompositeTileSize);
		OutEnvironment.SetDefine(TEXT("CULL_GRID_TILE_SIZE"), GCullGridTileSize);
		OutEnvironment.SetDefine(TEXT("MAX_GRID_CULLED_DF_OBJECTS"), GMaxGridCulledObjects);
	}

	FCompositeObjectDistanceFieldsCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CulledObjectBufferParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldTexture.Bind(Initializer.ParameterMap, TEXT("GlobalDistanceFieldTexture"));
		CulledObjectGrid.Bind(Initializer.ParameterMap, TEXT("CulledObjectGrid"));
		UpdateRegionSize.Bind(Initializer.ParameterMap, TEXT("UpdateRegionSize"));
		CullGridDimension.Bind(Initializer.ParameterMap, TEXT("CullGridDimension"));
		VolumeTexelSize.Bind(Initializer.ParameterMap, TEXT("VolumeTexelSize"));
		UpdateRegionVolumeMin.Bind(Initializer.ParameterMap, TEXT("UpdateRegionVolumeMin"));
		ClipmapIndex.Bind(Initializer.ParameterMap, TEXT("ClipmapIndex"));
		AOGlobalMaxSphereQueryRadius.Bind(Initializer.ParameterMap, TEXT("AOGlobalMaxSphereQueryRadius"));
	}

	FCompositeObjectDistanceFieldsCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FScene* Scene, 
		const FSceneView& View, 
		float MaxOcclusionDistance, 
		const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo,
		int32 ClipmapIndexValue,
		const FVolumeUpdateRegion& UpdateRegion)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		CulledObjectBufferParameters.Set(RHICmdList, ShaderRHI, GGlobalDistanceFieldCulledObjectBuffers.Buffers);
		GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, GlobalDistanceFieldInfo.ParameterData);

		const FSceneRenderTargetItem& ClipMapRTI = GlobalDistanceFieldInfo.Clipmaps[ClipmapIndexValue].RenderTarget->GetRenderTargetItem();
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ClipMapRTI.UAV);
		GlobalDistanceFieldTexture.SetTexture(RHICmdList, ShaderRHI, ClipMapRTI.ShaderResourceTexture, ClipMapRTI.UAV);

		SetSRVParameter(RHICmdList, ShaderRHI, CulledObjectGrid, GObjectGridBuffers.CulledObjectGrid.SRV);

		const FIntVector GridDimensionValue(
			FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.X, GCullGridTileSize),
			FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Y, GCullGridTileSize),
			FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Z, GCullGridTileSize));

		SetShaderValue(RHICmdList, ShaderRHI, CullGridDimension, GridDimensionValue);

		SetShaderValue(RHICmdList, ShaderRHI, UpdateRegionSize, UpdateRegion.CellsSize);
		SetShaderValue(RHICmdList, ShaderRHI, VolumeTexelSize, FVector(1.0f / GAOGlobalDFResolution));
		SetShaderValue(RHICmdList, ShaderRHI, UpdateRegionVolumeMin, UpdateRegion.Bounds.Min);
		SetShaderValue(RHICmdList, ShaderRHI, ClipmapIndex, ClipmapIndexValue);

		extern float GAOConeHalfAngle;
		const float GlobalMaxSphereQueryRadius = MaxOcclusionDistance / (1 + FMath::Tan(GAOConeHalfAngle));
		SetShaderValue(RHICmdList, ShaderRHI, AOGlobalMaxSphereQueryRadius, GlobalMaxSphereQueryRadius);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo, int32 ClipmapIndexValue)
	{
		GlobalDistanceFieldTexture.UnsetUAV(RHICmdList, GetComputeShader());

		const FSceneRenderTargetItem& ClipMapRTI = GlobalDistanceFieldInfo.Clipmaps[ClipmapIndexValue].RenderTarget->GetRenderTargetItem();
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ClipMapRTI.UAV);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CulledObjectBufferParameters;
		Ar << GlobalDistanceFieldParameters;
		Ar << GlobalDistanceFieldTexture;
		Ar << CulledObjectGrid;
		Ar << UpdateRegionSize;
		Ar << CullGridDimension;
		Ar << VolumeTexelSize;
		Ar << UpdateRegionVolumeMin;
		Ar << ClipmapIndex;
		Ar << AOGlobalMaxSphereQueryRadius;
		return bShaderHasOutdatedParameters;
	}

private:

	FDistanceFieldCulledObjectBufferParameters CulledObjectBufferParameters;
	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
	FRWShaderParameter GlobalDistanceFieldTexture;
	FShaderResourceParameter CulledObjectGrid;
	FShaderParameter UpdateRegionSize;
	FShaderParameter CullGridDimension;
	FShaderParameter VolumeTexelSize;
	FShaderParameter UpdateRegionVolumeMin;
	FShaderParameter ClipmapIndex;
	FShaderParameter AOGlobalMaxSphereQueryRadius;
};

IMPLEMENT_SHADER_TYPE(,FCompositeObjectDistanceFieldsCS,TEXT("GlobalDistanceField"),TEXT("CompositeObjectDistanceFieldsCS"),SF_Compute);

const int32 HeightfieldCompositeTileSize = 8;

class FCompositeHeightfieldsIntoGlobalDistanceFieldCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCompositeHeightfieldsIntoGlobalDistanceFieldCS, Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform) && !IsMetalPlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("COMPOSITE_HEIGHTFIELDS_THREADGROUP_SIZE"), HeightfieldCompositeTileSize);
	}

	FCompositeHeightfieldsIntoGlobalDistanceFieldCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		GlobalDistanceFieldParameters.Bind(Initializer.ParameterMap);
		GlobalDistanceFieldTexture.Bind(Initializer.ParameterMap, TEXT("GlobalDistanceFieldTexture"));
		UpdateRegionSize.Bind(Initializer.ParameterMap, TEXT("UpdateRegionSize"));
		VolumeTexelSize.Bind(Initializer.ParameterMap, TEXT("VolumeTexelSize"));
		UpdateRegionVolumeMin.Bind(Initializer.ParameterMap, TEXT("UpdateRegionVolumeMin"));
		ClipmapIndex.Bind(Initializer.ParameterMap, TEXT("ClipmapIndex"));
		AOGlobalMaxSphereQueryRadius.Bind(Initializer.ParameterMap, TEXT("AOGlobalMaxSphereQueryRadius"));
		HeightfieldDescriptionParameters.Bind(Initializer.ParameterMap);
		HeightfieldTextureParameters.Bind(Initializer.ParameterMap);
	}

	FCompositeHeightfieldsIntoGlobalDistanceFieldCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FScene* Scene,
		const FSceneView& View,
		float MaxOcclusionDistance,
		const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo,
		int32 ClipmapIndexValue,
		const FVolumeUpdateRegion& UpdateRegion,
		UTexture2D* HeightfieldTextureValue,
		int32 NumHeightfieldsValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		GlobalDistanceFieldParameters.Set(RHICmdList, ShaderRHI, GlobalDistanceFieldInfo.ParameterData);

		const FSceneRenderTargetItem& ClipMapRTI = GlobalDistanceFieldInfo.Clipmaps[ClipmapIndexValue].RenderTarget->GetRenderTargetItem();
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ClipMapRTI.UAV);
		GlobalDistanceFieldTexture.SetTexture(RHICmdList, ShaderRHI, ClipMapRTI.ShaderResourceTexture, ClipMapRTI.UAV);

		SetShaderValue(RHICmdList, ShaderRHI, UpdateRegionSize, UpdateRegion.CellsSize);
		SetShaderValue(RHICmdList, ShaderRHI, VolumeTexelSize, FVector(1.0f / GAOGlobalDFResolution));
		SetShaderValue(RHICmdList, ShaderRHI, UpdateRegionVolumeMin, UpdateRegion.Bounds.Min);
		SetShaderValue(RHICmdList, ShaderRHI, ClipmapIndex, ClipmapIndexValue);

		extern float GAOConeHalfAngle;
		const float GlobalMaxSphereQueryRadius = MaxOcclusionDistance / (1 + FMath::Tan(GAOConeHalfAngle));
		SetShaderValue(RHICmdList, ShaderRHI, AOGlobalMaxSphereQueryRadius, GlobalMaxSphereQueryRadius);

		HeightfieldDescriptionParameters.Set(RHICmdList, ShaderRHI, GetHeightfieldDescriptionsSRV(), NumHeightfieldsValue);
		HeightfieldTextureParameters.Set(RHICmdList, ShaderRHI, HeightfieldTextureValue, NULL);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo, int32 ClipmapIndexValue)
	{
		GlobalDistanceFieldTexture.UnsetUAV(RHICmdList, GetComputeShader());

		const FSceneRenderTargetItem& ClipMapRTI = GlobalDistanceFieldInfo.Clipmaps[ClipmapIndexValue].RenderTarget->GetRenderTargetItem();
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, ClipMapRTI.UAV);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << GlobalDistanceFieldParameters;
		Ar << GlobalDistanceFieldTexture;
		Ar << UpdateRegionSize;
		Ar << VolumeTexelSize;
		Ar << UpdateRegionVolumeMin;
		Ar << ClipmapIndex;
		Ar << AOGlobalMaxSphereQueryRadius;
		Ar << HeightfieldDescriptionParameters;
		Ar << HeightfieldTextureParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
	FRWShaderParameter GlobalDistanceFieldTexture;
	FShaderParameter UpdateRegionSize;
	FShaderParameter VolumeTexelSize;
	FShaderParameter UpdateRegionVolumeMin;
	FShaderParameter ClipmapIndex;
	FShaderParameter AOGlobalMaxSphereQueryRadius;
	FHeightfieldDescriptionParameters HeightfieldDescriptionParameters;
	FHeightfieldTextureParameters HeightfieldTextureParameters;
};

IMPLEMENT_SHADER_TYPE(, FCompositeHeightfieldsIntoGlobalDistanceFieldCS, TEXT("GlobalDistanceField"), TEXT("CompositeHeightfieldsIntoGlobalDistanceFieldCS"), SF_Compute);

extern int32 GAOHeightfieldOcclusion;
extern void UploadHeightfieldDescriptions(const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions, FVector2D InvLightingAtlasSize, float InvDownsampleFactor);

void FHeightfieldLightingViewInfo::CompositeHeightfieldsIntoGlobalDistanceField(
	FRHICommandList& RHICmdList,
	const FScene* Scene,
	const FViewInfo& View,
	float MaxOcclusionDistance,
	const FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo,
	int32 ClipmapIndexValue,
	const FVolumeUpdateRegion& UpdateRegion) const
{
	const int32 NumPrimitives = Scene->DistanceFieldSceneData.HeightfieldPrimitives.Num();

	if (GAOHeightfieldOcclusion
		&& NumPrimitives > 0
		&& SupportsDistanceFieldAO(Scene->GetFeatureLevel(), Scene->GetShaderPlatform())
		&& !IsMetalPlatform(Scene->GetShaderPlatform()))
	{
		FHeightfieldDescription UpdateRegionHeightfield;
		float LocalToWorldScale = 1;

		for (int32 HeightfieldPrimitiveIndex = 0; HeightfieldPrimitiveIndex < NumPrimitives; HeightfieldPrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* HeightfieldPrimitive = Scene->DistanceFieldSceneData.HeightfieldPrimitives[HeightfieldPrimitiveIndex];
			const FBoxSphereBounds& PrimitiveBounds = HeightfieldPrimitive->Proxy->GetBounds();
			const float DistanceToPrimitiveSq = (PrimitiveBounds.Origin - View.ViewMatrices.ViewOrigin).SizeSquared();

			if (UpdateRegion.Bounds.Intersect(PrimitiveBounds.GetBox()))
			{
				UTexture2D* HeightfieldTexture = NULL;
				UTexture2D* DiffuseColorTexture = NULL;
				FHeightfieldComponentDescription NewComponentDescription(HeightfieldPrimitive->Proxy->GetLocalToWorld());
				HeightfieldPrimitive->Proxy->GetHeightfieldRepresentation(HeightfieldTexture, DiffuseColorTexture, NewComponentDescription);

				if (HeightfieldTexture && HeightfieldTexture->Resource->TextureRHI)
				{
					const FIntPoint HeightfieldSize = NewComponentDescription.HeightfieldRect.Size();

					if (UpdateRegionHeightfield.Rect.Area() == 0)
					{
						UpdateRegionHeightfield.Rect = NewComponentDescription.HeightfieldRect;
						LocalToWorldScale = NewComponentDescription.LocalToWorld.GetScaleVector().X;
					}
					else
					{
						UpdateRegionHeightfield.Rect.Union(NewComponentDescription.HeightfieldRect);
					}

					TArray<FHeightfieldComponentDescription>& ComponentDescriptions = UpdateRegionHeightfield.ComponentDescriptions.FindOrAdd(FHeightfieldComponentTextures(HeightfieldTexture, DiffuseColorTexture));
					ComponentDescriptions.Add(NewComponentDescription);
				}
			}
		}

		if (UpdateRegionHeightfield.ComponentDescriptions.Num() > 0)
		{
			SCOPED_DRAW_EVENT(RHICmdList, CompositeHeightfields);

			for (TMap<FHeightfieldComponentTextures, TArray<FHeightfieldComponentDescription>>::TConstIterator It(UpdateRegionHeightfield.ComponentDescriptions); It; ++It)
			{
				const TArray<FHeightfieldComponentDescription>& HeightfieldDescriptions = It.Value();

				if (HeightfieldDescriptions.Num() > 0)
				{
					UploadHeightfieldDescriptions(HeightfieldDescriptions, FVector2D(1, 1), 1.0f / UpdateRegionHeightfield.DownsampleFactor);

					UTexture2D* HeightfieldTexture = It.Key().HeightAndNormal;

					TShaderMapRef<FCompositeHeightfieldsIntoGlobalDistanceFieldCS> ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, Scene, View, MaxOcclusionDistance, GlobalDistanceFieldInfo, ClipmapIndexValue, UpdateRegion, HeightfieldTexture, HeightfieldDescriptions.Num());

					//@todo - match typical update sizes.  Camera movement creates narrow slabs.
					const uint32 NumGroupsX = FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.X, HeightfieldCompositeTileSize);
					const uint32 NumGroupsY = FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Y, HeightfieldCompositeTileSize);

					DispatchComputeShader(RHICmdList, *ComputeShader, NumGroupsX, NumGroupsY, 1);
					ComputeShader->UnsetParameters(RHICmdList, GlobalDistanceFieldInfo, ClipmapIndexValue);
				}
			}
		}
	}
}

/** Constructs and adds an update region based on camera movement for the given axis. */
void AddUpdateRegionForAxis(FIntVector Movement, const FBox& ClipmapBounds, float CellSize, int32 ComponentIndex, TArray<FVolumeUpdateRegion, TInlineAllocator<3> >& UpdateRegions)
{
	FVolumeUpdateRegion UpdateRegion;
	UpdateRegion.Bounds = ClipmapBounds;
	UpdateRegion.CellsSize = FIntVector(GAOGlobalDFResolution);
	UpdateRegion.CellsSize[ComponentIndex] = FMath::Min(FMath::Abs(Movement[ComponentIndex]), GAOGlobalDFResolution);

	if (Movement[ComponentIndex] > 0)
	{
		// Positive axis movement, set the min of that axis to contain the newly exposed area
		UpdateRegion.Bounds.Min[ComponentIndex] = FMath::Max(ClipmapBounds.Max[ComponentIndex] - Movement[ComponentIndex] * CellSize, ClipmapBounds.Min[ComponentIndex]);
	}
	else if (Movement[ComponentIndex] < 0)
	{
		// Negative axis movement, set the max of that axis to contain the newly exposed area
		UpdateRegion.Bounds.Max[ComponentIndex] = FMath::Min(ClipmapBounds.Min[ComponentIndex] - Movement[ComponentIndex] * CellSize, ClipmapBounds.Max[ComponentIndex]);
	}

	if (UpdateRegion.CellsSize[ComponentIndex] > 0)
	{
		UpdateRegions.Add(UpdateRegion);
	}
}

/** Constructs and adds an update region based on the given primitive bounds. */
void AddUpdateRegionForPrimitive(const FVector4& Bounds, float MaxSphereQueryRadius, const FBox& ClipmapBounds, float CellSize, TArray<FVolumeUpdateRegion, TInlineAllocator<3> >& UpdateRegions)
{
	// Object influence bounds
	FBox BoundingBox((FVector)Bounds - Bounds.W - MaxSphereQueryRadius, (FVector)Bounds + Bounds.W + MaxSphereQueryRadius);

	FVolumeUpdateRegion UpdateRegion;
	UpdateRegion.Bounds.Init();
	// Snap the min and clamp to clipmap bounds
	UpdateRegion.Bounds.Min.X = FMath::Max(CellSize * FMath::FloorToFloat(BoundingBox.Min.X / CellSize), ClipmapBounds.Min.X);
	UpdateRegion.Bounds.Min.Y = FMath::Max(CellSize * FMath::FloorToFloat(BoundingBox.Min.Y / CellSize), ClipmapBounds.Min.Y);
	UpdateRegion.Bounds.Min.Z = FMath::Max(CellSize * FMath::FloorToFloat(BoundingBox.Min.Z / CellSize), ClipmapBounds.Min.Z);

	// Derive the max from the snapped min and size, clamp to clipmap bounds
	UpdateRegion.Bounds.Max = UpdateRegion.Bounds.Min + FVector(FMath::CeilToFloat((Bounds.W + MaxSphereQueryRadius) * 2 / CellSize)) * CellSize;
	UpdateRegion.Bounds.Max.X = FMath::Min(UpdateRegion.Bounds.Max.X, ClipmapBounds.Max.X);
	UpdateRegion.Bounds.Max.Y = FMath::Min(UpdateRegion.Bounds.Max.Y, ClipmapBounds.Max.Y);
	UpdateRegion.Bounds.Max.Z = FMath::Min(UpdateRegion.Bounds.Max.Z, ClipmapBounds.Max.Z);

	const FVector UpdateRegionSize = UpdateRegion.Bounds.GetSize();
	UpdateRegion.CellsSize.X = FMath::TruncToInt(UpdateRegionSize.X / CellSize + .5f);
	UpdateRegion.CellsSize.Y = FMath::TruncToInt(UpdateRegionSize.Y / CellSize + .5f);
	UpdateRegion.CellsSize.Z = FMath::TruncToInt(UpdateRegionSize.Z / CellSize + .5f);

	// Only add update regions with positive area
	if (UpdateRegion.CellsSize.X > 0 && UpdateRegion.CellsSize.Y > 0 && UpdateRegion.CellsSize.Z > 0)
	{
		checkSlow(UpdateRegion.CellsSize.X <= GAOGlobalDFResolution && UpdateRegion.CellsSize.Y <= GAOGlobalDFResolution && UpdateRegion.CellsSize.Z <= GAOGlobalDFResolution);
		UpdateRegions.Add(UpdateRegion);
	}
}

void AllocateClipmapTexture(FRHICommandListImmediate& RHICmdList, int32 ClipmapIndex, TRefCountPtr<IPooledRenderTarget>& Texture)
{
	const TCHAR* TextureName = TEXT("GlobalDistanceField0");

	if (ClipmapIndex == 1)
	{
		TextureName = TEXT("GlobalDistanceField1");
	}
	else if (ClipmapIndex == 2)
	{
		TextureName = TEXT("GlobalDistanceField2");
	}
	else if (ClipmapIndex == 3)
	{
		TextureName = TEXT("GlobalDistanceField3");
	}

	FPooledRenderTargetDesc VolumeDesc = FPooledRenderTargetDesc(FPooledRenderTargetDesc::CreateVolumeDesc(
		GAOGlobalDFResolution,
		GAOGlobalDFResolution,
		GAOGlobalDFResolution,
		PF_R16F,
		FClearValueBinding::None,
		0,
		TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV,
		false));
	VolumeDesc.AutoWritable = false;

	GRenderTargetPool.FindFreeElement(
		RHICmdList,
		VolumeDesc,
		Texture,
		TextureName 
	);
}

/** Staggers clipmap updates so there are only 2 per frame */
bool ShouldUpdateClipmapThisFrame(int32 ClipmapIndex, int32 GlobalDistanceFieldUpdateIndex)
{
	if (ClipmapIndex == 0 || !GAOGlobalDistanceFieldStaggeredUpdates)
	{
		return true;
	}
	else if (ClipmapIndex == 1)
	{
		return GlobalDistanceFieldUpdateIndex % 2 == 0;
	}
	else if (ClipmapIndex == 2)
	{
		return GlobalDistanceFieldUpdateIndex % 4 == 1;
	}
	else
	{
		check(ClipmapIndex == 3);
		return GlobalDistanceFieldUpdateIndex % 4 == 3;
	}
}

float ComputeClipmapExtent(int32 ClipmapIndex, const FScene* Scene)
{
	const float InnerClipmapDistance = Scene->GlobalDistanceFieldViewDistance / FMath::Pow(GAOGlobalDFClipmapDistanceExponent, 3);
	return InnerClipmapDistance * FMath::Pow(GAOGlobalDFClipmapDistanceExponent, ClipmapIndex);
}

void ComputeUpdateRegionsAndUpdateViewState(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View, 
	const FScene* Scene, 
	FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo, 
	int32 NumClipmaps, 
	const TArray<FVector4>& PrimitiveModifiedBounds, 
	float MaxOcclusionDistance)
{
	GlobalDistanceFieldInfo.Clipmaps.AddZeroed(NumClipmaps);

	if (View.ViewState)
	{
		View.ViewState->GlobalDistanceFieldUpdateIndex++;

		if (View.ViewState->GlobalDistanceFieldUpdateIndex > 4)
		{
			View.ViewState->GlobalDistanceFieldUpdateIndex = 0;
		}

		for (int32 ClipmapIndex = 0; ClipmapIndex < NumClipmaps; ClipmapIndex++)
		{
			FGlobalDistanceFieldClipmapState& ClipmapViewState = View.ViewState->GlobalDistanceFieldClipmapState[ClipmapIndex];
			TRefCountPtr<IPooledRenderTarget>& RenderTarget = ClipmapViewState.VolumeTexture;

			bool bReallocated = false;

			if (!RenderTarget || RenderTarget->GetDesc().Extent.X != GAOGlobalDFResolution)
			{
				AllocateClipmapTexture(RHICmdList, ClipmapIndex, RenderTarget);
				bReallocated = true;
			}

			FGlobalDistanceFieldClipmap& Clipmap = GlobalDistanceFieldInfo.Clipmaps[ClipmapIndex];

			const float Extent = ComputeClipmapExtent(ClipmapIndex, Scene);
			const float CellSize = (Extent * 2) / GAOGlobalDFResolution;

			// Accumulate primitive modifications in the viewstate in case we don't update the clipmap this frame
			ClipmapViewState.PrimitiveModifiedBounds.Append(PrimitiveModifiedBounds);

			const bool bForceFullUpdate = bReallocated
				|| !View.ViewState->bInitializedGlobalDistanceFieldOrigins
				// Detect when max occlusion distance has changed
				|| ClipmapViewState.CachedMaxOcclusionDistance != MaxOcclusionDistance
				|| ClipmapViewState.CachedGlobalDistanceFieldViewDistance != Scene->GlobalDistanceFieldViewDistance;

			if (ShouldUpdateClipmapThisFrame(ClipmapIndex, View.ViewState->GlobalDistanceFieldUpdateIndex)
				|| bForceFullUpdate)
			{
				const FVector NewCenter = View.ViewMatrices.ViewOrigin;

				FIntVector GridCenter;
				GridCenter.X = FMath::FloorToInt(NewCenter.X / CellSize);
				GridCenter.Y = FMath::FloorToInt(NewCenter.Y / CellSize);
				GridCenter.Z = FMath::FloorToInt(NewCenter.Z / CellSize);

				const FVector SnappedCenter = FVector(GridCenter) * CellSize;
				const FBox ClipmapBounds(SnappedCenter - Extent, SnappedCenter + Extent);

				bool bUsePartialUpdates = GAOGlobalDistanceFieldPartialUpdates
					&& !bForceFullUpdate
					// Only use partial updates with small numbers of primitive modifications
					&& ClipmapViewState.PrimitiveModifiedBounds.Num() < 100;

				if (bUsePartialUpdates)
				{
					FIntVector Movement = GridCenter - ClipmapViewState.LastPartialUpdateOrigin;

					// Add an update region for each potential axis of camera movement
					AddUpdateRegionForAxis(Movement, ClipmapBounds, CellSize, 0, Clipmap.UpdateRegions);
					AddUpdateRegionForAxis(Movement, ClipmapBounds, CellSize, 1, Clipmap.UpdateRegions);
					AddUpdateRegionForAxis(Movement, ClipmapBounds, CellSize, 2, Clipmap.UpdateRegions);
					
					extern float GAOConeHalfAngle;
					const float GlobalMaxSphereQueryRadius = MaxOcclusionDistance / (1 + FMath::Tan(GAOConeHalfAngle));

					for (int32 BoundsIndex = 0; BoundsIndex < ClipmapViewState.PrimitiveModifiedBounds.Num(); BoundsIndex++)
					{
						// Add an update region for each primitive that has been modified
						AddUpdateRegionForPrimitive(ClipmapViewState.PrimitiveModifiedBounds[BoundsIndex], GlobalMaxSphereQueryRadius, ClipmapBounds, CellSize, Clipmap.UpdateRegions);
					}

					int32 TotalTexelsBeingUpdated = 0;

					// Trim fully contained update regions
					//@todo - trim overlapping regions, especially for update transforms which create update regions for previous and current bounds which are typically overlapping
					for (int32 UpdateRegionIndex = 0; UpdateRegionIndex < Clipmap.UpdateRegions.Num(); UpdateRegionIndex++)
					{
						const FVolumeUpdateRegion& UpdateRegion = Clipmap.UpdateRegions[UpdateRegionIndex];
						bool bCompletelyContained = false;

						for (int32 OtherUpdateRegionIndex = 0; OtherUpdateRegionIndex < Clipmap.UpdateRegions.Num(); OtherUpdateRegionIndex++)
						{
							if (UpdateRegionIndex != OtherUpdateRegionIndex)
							{
								const FVolumeUpdateRegion& OtherUpdateRegion = Clipmap.UpdateRegions[OtherUpdateRegionIndex];

								if (OtherUpdateRegion.Bounds.IsInsideOrOn(UpdateRegion.Bounds.Min)
									&& OtherUpdateRegion.Bounds.IsInsideOrOn(UpdateRegion.Bounds.Max))
								{
									bCompletelyContained = true;
									break;
								}
							}
						}

						if (bCompletelyContained)
						{
							Clipmap.UpdateRegions.RemoveAt(UpdateRegionIndex);
							UpdateRegionIndex--;
						}
						else
						{
							TotalTexelsBeingUpdated += UpdateRegion.CellsSize.X * UpdateRegion.CellsSize.Y * UpdateRegion.CellsSize.Z;
						}
					}

					if (TotalTexelsBeingUpdated >= GAOGlobalDFResolution * GAOGlobalDFResolution * GAOGlobalDFResolution)
					{
						// Fall back to a full update if the partial updates were going to do more work
						bUsePartialUpdates = false;
						Clipmap.UpdateRegions.Reset();
					}
				}
		
				if (!bUsePartialUpdates)
				{
					FVolumeUpdateRegion UpdateRegion;
					UpdateRegion.Bounds = ClipmapBounds;
					UpdateRegion.CellsSize = FIntVector(GAOGlobalDFResolution);
					Clipmap.UpdateRegions.Add(UpdateRegion);

					// Store the location of the full update
					ClipmapViewState.FullUpdateOrigin = GridCenter;
					View.ViewState->bInitializedGlobalDistanceFieldOrigins = true;
				}

				ClipmapViewState.PrimitiveModifiedBounds.Reset();
				ClipmapViewState.LastPartialUpdateOrigin = GridCenter;
			}

			const FVector Center = FVector(ClipmapViewState.LastPartialUpdateOrigin) * CellSize;

			// Setup clipmap properties from view state exclusively, so we can skip updating on some frames
			Clipmap.RenderTarget = ClipmapViewState.VolumeTexture;
			Clipmap.Bounds = FBox(Center - Extent, Center + Extent);
			// Scroll offset so the contents of the global distance field don't have to be moved as the camera moves around, only updated in slabs
			Clipmap.ScrollOffset = FVector(ClipmapViewState.LastPartialUpdateOrigin - ClipmapViewState.FullUpdateOrigin) * CellSize;

			ClipmapViewState.CachedMaxOcclusionDistance = MaxOcclusionDistance;
			ClipmapViewState.CachedGlobalDistanceFieldViewDistance = Scene->GlobalDistanceFieldViewDistance;
		}
	}
	else
	{
		for (int32 ClipmapIndex = 0; ClipmapIndex < NumClipmaps; ClipmapIndex++)
		{
			FGlobalDistanceFieldClipmap& Clipmap = GlobalDistanceFieldInfo.Clipmaps[ClipmapIndex];
			AllocateClipmapTexture(RHICmdList, ClipmapIndex, Clipmap.RenderTarget);
			Clipmap.ScrollOffset = FVector::ZeroVector;

			const float Extent = ComputeClipmapExtent(ClipmapIndex, Scene);
			FVector Center = View.ViewMatrices.ViewOrigin;

			const float CellSize = (Extent * 2) / GAOGlobalDFResolution;

			FIntVector GridCenter;
			GridCenter.X = FMath::FloorToInt(Center.X / CellSize);
			GridCenter.Y = FMath::FloorToInt(Center.Y / CellSize);
			GridCenter.Z = FMath::FloorToInt(Center.Z / CellSize);

			Center = FVector(GridCenter) * CellSize;

			FBox ClipmapBounds(Center - Extent, Center + Extent);
			Clipmap.Bounds = ClipmapBounds;

			FVolumeUpdateRegion UpdateRegion;
			UpdateRegion.Bounds = ClipmapBounds;
			UpdateRegion.CellsSize = FIntVector(GAOGlobalDFResolution);
			Clipmap.UpdateRegions.Add(UpdateRegion);
		}
	}

	GlobalDistanceFieldInfo.UpdateParameterData(MaxOcclusionDistance);
}

void FViewInfo::SetupDefaultGlobalDistanceFieldUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	// Initialize global distance field members to defaults, because View.GlobalDistanceFieldInfo is not valid yet
	for (int32 Index = 0; Index < GMaxGlobalDistanceFieldClipmaps; Index++)
	{
		ViewUniformShaderParameters.GlobalVolumeCenterAndExtent_UB[Index] = FVector4(0);
		ViewUniformShaderParameters.GlobalVolumeWorldToUVAddAndMul_UB[Index] = FVector4(0);
	}
	ViewUniformShaderParameters.GlobalVolumeDimension_UB = 0.0f;
	ViewUniformShaderParameters.GlobalVolumeTexelSize_UB = 0.0f;
	ViewUniformShaderParameters.MaxGlobalDistance_UB = 0.0f;

	ViewUniformShaderParameters.GlobalDistanceFieldTexture0_UB = GBlackVolumeTexture->TextureRHI.GetReference();
	ViewUniformShaderParameters.GlobalDistanceFieldSampler0_UB = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	ViewUniformShaderParameters.GlobalDistanceFieldTexture1_UB = GBlackVolumeTexture->TextureRHI.GetReference();
	ViewUniformShaderParameters.GlobalDistanceFieldSampler1_UB = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	ViewUniformShaderParameters.GlobalDistanceFieldTexture2_UB = GBlackVolumeTexture->TextureRHI.GetReference();
	ViewUniformShaderParameters.GlobalDistanceFieldSampler2_UB = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	ViewUniformShaderParameters.GlobalDistanceFieldTexture3_UB = GBlackVolumeTexture->TextureRHI.GetReference();
	ViewUniformShaderParameters.GlobalDistanceFieldSampler3_UB = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
}

void FViewInfo::SetupGlobalDistanceFieldUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	check(GlobalDistanceFieldInfo.bInitialized);

	for (int32 Index = 0; Index < GMaxGlobalDistanceFieldClipmaps; Index++)
	{
		ViewUniformShaderParameters.GlobalVolumeCenterAndExtent_UB[Index] = GlobalDistanceFieldInfo.ParameterData.CenterAndExtent[Index];
		ViewUniformShaderParameters.GlobalVolumeWorldToUVAddAndMul_UB[Index] = GlobalDistanceFieldInfo.ParameterData.WorldToUVAddAndMul[Index];
	}
	ViewUniformShaderParameters.GlobalVolumeDimension_UB = GlobalDistanceFieldInfo.ParameterData.GlobalDFResolution;
	ViewUniformShaderParameters.GlobalVolumeTexelSize_UB = 1.0f / GlobalDistanceFieldInfo.ParameterData.GlobalDFResolution;
	ViewUniformShaderParameters.MaxGlobalDistance_UB = GlobalDistanceFieldInfo.ParameterData.MaxDistance;

	ViewUniformShaderParameters.GlobalDistanceFieldTexture0_UB = OrBlack3DIfNull(GlobalDistanceFieldInfo.ParameterData.Textures[0]);
	ViewUniformShaderParameters.GlobalDistanceFieldSampler0_UB = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	ViewUniformShaderParameters.GlobalDistanceFieldTexture1_UB = OrBlack3DIfNull(GlobalDistanceFieldInfo.ParameterData.Textures[1]);
	ViewUniformShaderParameters.GlobalDistanceFieldSampler1_UB = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	ViewUniformShaderParameters.GlobalDistanceFieldTexture2_UB = OrBlack3DIfNull(GlobalDistanceFieldInfo.ParameterData.Textures[2]);
	ViewUniformShaderParameters.GlobalDistanceFieldSampler2_UB = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	ViewUniformShaderParameters.GlobalDistanceFieldTexture3_UB = OrBlack3DIfNull(GlobalDistanceFieldInfo.ParameterData.Textures[3]);
	ViewUniformShaderParameters.GlobalDistanceFieldSampler3_UB = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
}

/** 
 * Updates the global distance field for a view.  
 * Typically issues updates for just the newly exposed regions of the volume due to camera movement.
 * In the worst case of a camera cut or large distance field scene changes, a full update of the global distance field will be done.
 **/
void UpdateGlobalDistanceFieldVolume(
	FRHICommandListImmediate& RHICmdList, 
	FViewInfo& View, 
	const FScene* Scene, 
	float MaxOcclusionDistance, 
	FGlobalDistanceFieldInfo& GlobalDistanceFieldInfo)
{
	if (Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0)
	{
		ComputeUpdateRegionsAndUpdateViewState(RHICmdList, View, Scene, GlobalDistanceFieldInfo, GMaxGlobalDistanceFieldClipmaps, Scene->DistanceFieldSceneData.PrimitiveModifiedBounds, MaxOcclusionDistance);

		// Recreate the view uniform buffer now that we have updated GlobalDistanceFieldInfo
		View.SetupGlobalDistanceFieldUniformBufferParameters(*View.CachedViewUniformShaderParameters);
		View.ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(*View.CachedViewUniformShaderParameters, UniformBuffer_SingleFrame);

		bool bHasUpdateRegions = false;

		for (int32 ClipmapIndex = 0; ClipmapIndex < GlobalDistanceFieldInfo.Clipmaps.Num(); ClipmapIndex++)
		{
			bHasUpdateRegions = bHasUpdateRegions || GlobalDistanceFieldInfo.Clipmaps[ClipmapIndex].UpdateRegions.Num() > 0;
		}

		if (bHasUpdateRegions && GAOUpdateGlobalDistanceField)
		{
			SCOPED_DRAW_EVENT(RHICmdList, UpdateGlobalDistanceFieldVolume);

			if (GGlobalDistanceFieldCulledObjectBuffers.Buffers.MaxObjects < Scene->DistanceFieldSceneData.NumObjectsInBuffer
				|| GGlobalDistanceFieldCulledObjectBuffers.Buffers.MaxObjects > 3 * Scene->DistanceFieldSceneData.NumObjectsInBuffer)
			{
				GGlobalDistanceFieldCulledObjectBuffers.Buffers.MaxObjects = Scene->DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
				GGlobalDistanceFieldCulledObjectBuffers.Buffers.Release();
				GGlobalDistanceFieldCulledObjectBuffers.Buffers.Initialize();
			}

			const uint32 MaxCullGridDimension = GAOGlobalDFResolution / GCullGridTileSize;

			if (GObjectGridBuffers.GridDimension != MaxCullGridDimension)
			{
				GObjectGridBuffers.GridDimension = MaxCullGridDimension;
				GObjectGridBuffers.UpdateRHI();
			}

			for (int32 ClipmapIndex = 0; ClipmapIndex < GlobalDistanceFieldInfo.Clipmaps.Num(); ClipmapIndex++)
			{
				SCOPED_DRAW_EVENT(RHICmdList, Clipmap);

				FGlobalDistanceFieldClipmap& Clipmap = GlobalDistanceFieldInfo.Clipmaps[ClipmapIndex];

				for (int32 UpdateRegionIndex = 0; UpdateRegionIndex < Clipmap.UpdateRegions.Num(); UpdateRegionIndex++)
				{
					const FVolumeUpdateRegion& UpdateRegion = Clipmap.UpdateRegions[UpdateRegionIndex];

					{
						SCOPED_DRAW_EVENT(RHICmdList, GridCull);

						// Cull the global objects to the volume being updated
						{
							uint32 ClearValues[4] = { 0 };
							RHICmdList.ClearUAV(GGlobalDistanceFieldCulledObjectBuffers.Buffers.ObjectIndirectArguments.UAV, ClearValues);

							TShaderMapRef<FCullObjectsForVolumeCS> ComputeShader(View.ShaderMap);
							RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
							const FVector4 VolumeBounds(UpdateRegion.Bounds.GetCenter(), UpdateRegion.Bounds.GetExtent().Size());
							ComputeShader->SetParameters(RHICmdList, Scene, View, MaxOcclusionDistance, VolumeBounds);

							DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(Scene->DistanceFieldSceneData.NumObjectsInBuffer, CullObjectsGroupSize), 1, 1);
							ComputeShader->UnsetParameters(RHICmdList, Scene);
						}

						// Further cull the objects into a low resolution grid
						{
							TShaderMapRef<FCullObjectsToGridCS> ComputeShader(View.ShaderMap);
							RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
							ComputeShader->SetParameters(RHICmdList, Scene, View, MaxOcclusionDistance, GlobalDistanceFieldInfo, ClipmapIndex, UpdateRegion);

							const uint32 NumGroupsX = FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.X, GCullGridTileSize);
							const uint32 NumGroupsY = FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Y, GCullGridTileSize);
							const uint32 NumGroupsZ = FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Z, GCullGridTileSize); 

							DispatchComputeShader(RHICmdList, *ComputeShader, NumGroupsX, NumGroupsY, NumGroupsZ);
							ComputeShader->UnsetParameters(RHICmdList);
						}
					}

					// Further cull the objects to the dispatch tile and composite the global distance field by computing the min distance from intersecting per-object distance fields
					{
						SCOPED_DRAW_EVENT(RHICmdList, TileCullAndComposite);
						TShaderMapRef<FCompositeObjectDistanceFieldsCS> ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, Scene, View, MaxOcclusionDistance, GlobalDistanceFieldInfo, ClipmapIndex, UpdateRegion);

						//@todo - match typical update sizes.  Camera movement creates narrow slabs.
						const uint32 NumGroupsX = FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.X, CompositeTileSize);
						const uint32 NumGroupsY = FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Y, CompositeTileSize);
						const uint32 NumGroupsZ = FMath::DivideAndRoundUp<int32>(UpdateRegion.CellsSize.Z, CompositeTileSize);

						DispatchComputeShader(RHICmdList, *ComputeShader, NumGroupsX, NumGroupsY, NumGroupsZ);
						ComputeShader->UnsetParameters(RHICmdList, GlobalDistanceFieldInfo, ClipmapIndex);
					}

					View.HeightfieldLightingViewInfo.CompositeHeightfieldsIntoGlobalDistanceField(RHICmdList, Scene, View, MaxOcclusionDistance, GlobalDistanceFieldInfo, ClipmapIndex, UpdateRegion);
				}
			}
		}
	}
}

void ListGlobalDistanceFieldMemory()
{
	UE_LOG(LogTemp, Log, TEXT("   Global DF culled objects %.3fMb"), (GGlobalDistanceFieldCulledObjectBuffers.Buffers.GetSizeBytes() + GObjectGridBuffers.GetSizeBytes()) / 1024.0f / 1024.0f);
}
