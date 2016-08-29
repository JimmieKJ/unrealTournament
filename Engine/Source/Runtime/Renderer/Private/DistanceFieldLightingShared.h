// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldLightingShared.h
=============================================================================*/

#pragma once

#include "DistanceFieldAtlas.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDistanceField, Warning, All);

/** Tile sized used for most AO compute shaders. */
const int32 GDistanceFieldAOTileSizeX = 16;
const int32 GDistanceFieldAOTileSizeY = 16;
/** Must match usf */
static const int32 GMaxNumObjectsPerTile = 512;

extern int32 GDistanceFieldGI;

inline bool DoesPlatformSupportDistanceFieldGI(EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5;
}

inline bool SupportsDistanceFieldGI(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform)
{
	return GDistanceFieldGI 
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportDistanceFieldGI(ShaderPlatform);
}

extern bool IsDistanceFieldGIAllowed(const FViewInfo& View);

class FDistanceFieldObjectBuffers
{
public:

	// In float4's
	static int32 ObjectDataStride;

	int32 MaxObjects;

	FRWBuffer Bounds;
	FRWBuffer Data;

	FDistanceFieldObjectBuffers()
	{
		MaxObjects = 0;
	}

	void Initialize()
	{
		if (MaxObjects > 0)
		{
			const uint32 BufferFlags = BUF_ShaderResource;

			Bounds.Initialize(sizeof(float), 4 * MaxObjects, PF_R32_FLOAT);
			Data.Initialize(sizeof(float), 4 * MaxObjects * ObjectDataStride, PF_R32_FLOAT);
		}
	}

	void Release()
	{
		Bounds.Release();
		Data.Release();
	}

	size_t GetSizeBytes() const
	{
		return Bounds.NumBytes + Data.NumBytes;
	}
};

class FSurfelBuffers
{
public:

	// In float4's
	static int32 SurfelDataStride;
	static int32 InterpolatedVertexDataStride;

	int32 MaxSurfels;

	void Initialize()
	{
		if (MaxSurfels > 0)
		{
			InterpolatedVertexData.Initialize(sizeof(FVector4), MaxSurfels * InterpolatedVertexDataStride, PF_A32B32G32R32F, BUF_Static);
			Surfels.Initialize(sizeof(FVector4), MaxSurfels * SurfelDataStride, PF_A32B32G32R32F, BUF_Static);
		}
	}

	void Release()
	{
		InterpolatedVertexData.Release();
		Surfels.Release();
	}

	size_t GetSizeBytes() const
	{
		return InterpolatedVertexData.NumBytes + Surfels.NumBytes;
	}

	FRWBuffer InterpolatedVertexData;
	FRWBuffer Surfels;
};

class FInstancedSurfelBuffers
{
public:

	int32 MaxSurfels;

	void Initialize()
	{
		if (MaxSurfels > 0)
		{
			VPLFlux.Initialize(sizeof(FVector4), MaxSurfels, PF_A32B32G32R32F, BUF_Static);
		}
	}

	void Release()
	{
		VPLFlux.Release();
	}

	size_t GetSizeBytes() const
	{
		return VPLFlux.NumBytes;
	}

	FRWBuffer VPLFlux;
};

class FDistanceFieldObjectBufferParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ObjectBounds.Bind(ParameterMap, TEXT("ObjectBounds"));
		ObjectData.Bind(ParameterMap, TEXT("ObjectData"));
		NumSceneObjects.Bind(ParameterMap, TEXT("NumSceneObjects"));
		DistanceFieldTexture.Bind(ParameterMap, TEXT("DistanceFieldTexture"));
		DistanceFieldSampler.Bind(ParameterMap, TEXT("DistanceFieldSampler"));
		DistanceFieldAtlasTexelSize.Bind(ParameterMap, TEXT("DistanceFieldAtlasTexelSize"));
	}

	template<typename TParamRef>
	void Set(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FDistanceFieldObjectBuffers& ObjectBuffers, int32 NumObjectsValue, bool bBarrier = false)
	{
		if (bBarrier)
		{
			FUnorderedAccessViewRHIParamRef OutUAVs[2];
			OutUAVs[0] = ObjectBuffers.Bounds.UAV;
			OutUAVs[1] = ObjectBuffers.Data.UAV;
			RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
		}

		ObjectBounds.SetBuffer(RHICmdList, ShaderRHI, ObjectBuffers.Bounds);
		ObjectData.SetBuffer(RHICmdList, ShaderRHI, ObjectBuffers.Data);
		SetShaderValue(RHICmdList, ShaderRHI, NumSceneObjects, NumObjectsValue);

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldTexture,
			DistanceFieldSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI
			);

		const int32 NumTexelsOneDimX = GDistanceFieldVolumeTextureAtlas.GetSizeX();
		const int32 NumTexelsOneDimY = GDistanceFieldVolumeTextureAtlas.GetSizeY();
		const int32 NumTexelsOneDimZ = GDistanceFieldVolumeTextureAtlas.GetSizeZ();
		const FVector InvTextureDim(1.0f / NumTexelsOneDimX, 1.0f / NumTexelsOneDimY, 1.0f / NumTexelsOneDimZ);
		SetShaderValue(RHICmdList, ShaderRHI, DistanceFieldAtlasTexelSize, InvTextureDim);
	}

	template<typename TParamRef>
	void UnsetParameters(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FDistanceFieldObjectBuffers& ObjectBuffers, bool bBarrier = false)
	{
		ObjectBounds.UnsetUAV(RHICmdList, ShaderRHI);
		ObjectData.UnsetUAV(RHICmdList, ShaderRHI);

		if (bBarrier)
		{
			FUnorderedAccessViewRHIParamRef OutUAVs[2];
			OutUAVs[0] = ObjectBuffers.Bounds.UAV;
			OutUAVs[1] = ObjectBuffers.Data.UAV;
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
		}
	}

	friend FArchive& operator<<(FArchive& Ar, FDistanceFieldObjectBufferParameters& P)
	{
		Ar << P.ObjectBounds << P.ObjectData << P.NumSceneObjects << P.DistanceFieldTexture << P.DistanceFieldSampler << P.DistanceFieldAtlasTexelSize;
		return Ar;
	}

private:
	FRWShaderParameter ObjectBounds;
	FRWShaderParameter ObjectData;
	FShaderParameter NumSceneObjects;
	FShaderResourceParameter DistanceFieldTexture;
	FShaderResourceParameter DistanceFieldSampler;
	FShaderParameter DistanceFieldAtlasTexelSize;
};

class FSurfelBufferParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		InterpolatedVertexData.Bind(ParameterMap, TEXT("InterpolatedVertexData"));
		SurfelData.Bind(ParameterMap, TEXT("SurfelData"));
		VPLFlux.Bind(ParameterMap, TEXT("VPLFlux"));
	}

	template<typename TParamRef>
	void Set(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FSurfelBuffers& SurfelBuffers, const FInstancedSurfelBuffers& InstancedSurfelBuffers)
	{
		InterpolatedVertexData.SetBuffer(RHICmdList, ShaderRHI, SurfelBuffers.InterpolatedVertexData);
		SurfelData.SetBuffer(RHICmdList, ShaderRHI, SurfelBuffers.Surfels);
		VPLFlux.SetBuffer(RHICmdList, ShaderRHI, InstancedSurfelBuffers.VPLFlux);
	}

	template<typename TParamRef>
	void UnsetParameters(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI)
	{
		InterpolatedVertexData.UnsetUAV(RHICmdList, ShaderRHI);
		SurfelData.UnsetUAV(RHICmdList, ShaderRHI);
		VPLFlux.UnsetUAV(RHICmdList, ShaderRHI);
	}

	friend FArchive& operator<<(FArchive& Ar, FSurfelBufferParameters& P)
	{
		Ar << P.InterpolatedVertexData << P.SurfelData << P.VPLFlux;
		return Ar;
	}

private:
	FRWShaderParameter InterpolatedVertexData;
	FRWShaderParameter SurfelData;
	FRWShaderParameter VPLFlux;
};

class FDistanceFieldCulledObjectBuffers
{
public:

	static int32 ObjectDataStride;
	static int32 ObjectBoxBoundsStride;

	bool bWantBoxBounds;
	int32 MaxObjects;

	FRWBuffer ObjectIndirectArguments;
	FRWBuffer ObjectIndirectDispatch;
	FRWBuffer Bounds;
	FRWBuffer Data;
	FRWBuffer BoxBounds;

	FDistanceFieldCulledObjectBuffers()
	{
		MaxObjects = 0;
		bWantBoxBounds = false;
	}

	void Initialize()
	{
		if (MaxObjects > 0)
		{
			ObjectIndirectArguments.Initialize(sizeof(uint32), 5, PF_R32_UINT, BUF_Static | BUF_DrawIndirect);
			ObjectIndirectDispatch.Initialize(sizeof(uint32), 3, PF_R32_UINT, BUF_Static | BUF_DrawIndirect);
			Bounds.Initialize(sizeof(FVector4), MaxObjects, PF_A32B32G32R32F);
			Data.Initialize(sizeof(FVector4), MaxObjects * ObjectDataStride, PF_A32B32G32R32F);

			if (bWantBoxBounds)
			{
				BoxBounds.Initialize(sizeof(FVector4), MaxObjects * ObjectBoxBoundsStride, PF_A32B32G32R32F);
			}
		}
	}

	void Release()
	{
		ObjectIndirectArguments.Release();
		ObjectIndirectDispatch.Release();
		Bounds.Release();
		Data.Release();
		BoxBounds.Release();
	}

	size_t GetSizeBytes() const
	{
		return ObjectIndirectArguments.NumBytes + ObjectIndirectDispatch.NumBytes + Bounds.NumBytes + Data.NumBytes + BoxBounds.NumBytes;
	}
};

class FDistanceFieldObjectBufferResource : public FRenderResource
{
public:
	FDistanceFieldCulledObjectBuffers Buffers;

	virtual void InitDynamicRHI()  override
	{
		Buffers.Initialize();
	}

	virtual void ReleaseDynamicRHI() override
	{
		Buffers.Release();
	}
};

class FDistanceFieldCulledObjectBufferParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ObjectIndirectArguments.Bind(ParameterMap, TEXT("ObjectIndirectArguments"));
		CulledObjectBounds.Bind(ParameterMap, TEXT("CulledObjectBounds"));
		CulledObjectData.Bind(ParameterMap, TEXT("CulledObjectData"));
		CulledObjectBoxBounds.Bind(ParameterMap, TEXT("CulledObjectBoxBounds"));
		DistanceFieldTexture.Bind(ParameterMap, TEXT("DistanceFieldTexture"));
		DistanceFieldSampler.Bind(ParameterMap, TEXT("DistanceFieldSampler"));
		DistanceFieldAtlasTexelSize.Bind(ParameterMap, TEXT("DistanceFieldAtlasTexelSize"));
	}

	template<typename TParamRef>
	void Set(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FDistanceFieldCulledObjectBuffers& ObjectBuffers)
	{
		int32 NumOutUAVs = 0;
		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[NumOutUAVs++] = ObjectBuffers.ObjectIndirectArguments.UAV;
		OutUAVs[NumOutUAVs++] = ObjectBuffers.Bounds.UAV;
		OutUAVs[NumOutUAVs++] = ObjectBuffers.Data.UAV;

		if (CulledObjectBoxBounds.IsBound()) 
		{
			OutUAVs[NumOutUAVs++] = ObjectBuffers.BoxBounds.UAV;
		}
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, NumOutUAVs);

		ObjectIndirectArguments.SetBuffer(RHICmdList, ShaderRHI, ObjectBuffers.ObjectIndirectArguments);
		CulledObjectBounds.SetBuffer(RHICmdList, ShaderRHI, ObjectBuffers.Bounds);
		CulledObjectData.SetBuffer(RHICmdList, ShaderRHI, ObjectBuffers.Data);

		if (CulledObjectBoxBounds.IsBound())
		{
			check(ObjectBuffers.bWantBoxBounds);
			CulledObjectBoxBounds.SetBuffer(RHICmdList, ShaderRHI, ObjectBuffers.BoxBounds);
		}

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			DistanceFieldTexture,
			DistanceFieldSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI
			);

		const int32 NumTexelsOneDimX = GDistanceFieldVolumeTextureAtlas.GetSizeX();
		const int32 NumTexelsOneDimY = GDistanceFieldVolumeTextureAtlas.GetSizeY();
		const int32 NumTexelsOneDimZ = GDistanceFieldVolumeTextureAtlas.GetSizeZ();
		const FVector InvTextureDim(1.0f / NumTexelsOneDimX, 1.0f / NumTexelsOneDimY, 1.0f / NumTexelsOneDimZ);
		SetShaderValue(RHICmdList, ShaderRHI, DistanceFieldAtlasTexelSize, InvTextureDim);
	}

	template<typename TParamRef>
	void UnsetParameters(FRHICommandList& RHICmdList, const TParamRef& ShaderRHI, const FDistanceFieldCulledObjectBuffers& ObjectBuffers)
	{
		ObjectIndirectArguments.UnsetUAV(RHICmdList, ShaderRHI);
		CulledObjectBounds.UnsetUAV(RHICmdList, ShaderRHI);
		CulledObjectData.UnsetUAV(RHICmdList, ShaderRHI);
		CulledObjectBoxBounds.UnsetUAV(RHICmdList, ShaderRHI);

		int32 NumOutUAVs = 0;
		FUnorderedAccessViewRHIParamRef OutUAVs[4];
		OutUAVs[NumOutUAVs++] = ObjectBuffers.ObjectIndirectArguments.UAV;
		OutUAVs[NumOutUAVs++] = ObjectBuffers.Bounds.UAV;
		OutUAVs[NumOutUAVs++] = ObjectBuffers.Data.UAV;

		if (CulledObjectBoxBounds.IsBound())
		{
			OutUAVs[NumOutUAVs++] = ObjectBuffers.BoxBounds.UAV;
		}
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, NumOutUAVs);
	}

	friend FArchive& operator<<(FArchive& Ar, FDistanceFieldCulledObjectBufferParameters& P)
	{
		Ar << P.ObjectIndirectArguments << P.CulledObjectBounds << P.CulledObjectData << P.CulledObjectBoxBounds << P.DistanceFieldTexture << P.DistanceFieldSampler << P.DistanceFieldAtlasTexelSize;
		return Ar;
	}

private:
	FRWShaderParameter ObjectIndirectArguments;
	FRWShaderParameter CulledObjectBounds;
	FRWShaderParameter CulledObjectData;
	FRWShaderParameter CulledObjectBoxBounds;
	FShaderResourceParameter DistanceFieldTexture;
	FShaderResourceParameter DistanceFieldSampler;
	FShaderParameter DistanceFieldAtlasTexelSize;
};

class FCPUUpdatedBuffer
{
public:

	EPixelFormat Format;
	int32 Stride;
	int32 MaxElements;

	FVertexBufferRHIRef Buffer;
	FShaderResourceViewRHIRef BufferSRV;

	FCPUUpdatedBuffer()
	{
		Format = PF_A32B32G32R32F;
		Stride = 1;
		MaxElements = 0;
	}

	void Initialize()
	{
		if (MaxElements > 0 && Stride > 0)
		{
			FRHIResourceCreateInfo CreateInfo;
			Buffer = RHICreateVertexBuffer(MaxElements * Stride * GPixelFormats[Format].BlockBytes, BUF_Dynamic | BUF_ShaderResource, CreateInfo);
			BufferSRV = RHICreateShaderResourceView(Buffer, GPixelFormats[Format].BlockBytes, Format);
		}
	}

	void Release()
	{
		Buffer.SafeRelease();
		BufferSRV.SafeRelease(); 
	}

	size_t GetSizeBytes() const
	{
		return MaxElements * Stride * GPixelFormats[Format].BlockBytes;
	}
};

/**  */
class FLightTileIntersectionResources
{
public:
	void Initialize()
	{
		TileHeadDataUnpacked.Initialize(sizeof(uint32), TileDimensions.X * TileDimensions.Y * 2, PF_R32_UINT, BUF_Static);

		//@todo - handle max exceeded
		TileArrayData.Initialize(sizeof(uint32), GMaxNumObjectsPerTile * TileDimensions.X * TileDimensions.Y, PF_R32_UINT, BUF_Static);
	}

	void Release()
	{
		TileHeadDataUnpacked.Release();
		TileArrayData.Release();
	}

	size_t GetSizeBytes() const
	{
		return TileHeadDataUnpacked.NumBytes + TileArrayData.NumBytes;
	}

	FIntPoint TileDimensions;

	FRWBuffer TileHeadDataUnpacked;
	FRWBuffer TileArrayData;
};

extern void CullDistanceFieldObjectsForLight(
	FRHICommandListImmediate& RHICmdList,
	const FViewInfo& View,
	const FLightSceneProxy* LightSceneProxy, 
	const FMatrix& WorldToShadowValue, 
	int32 NumPlanes, 
	const FPlane* PlaneData, 
	const FVector4& ShadowBoundingSphereValue,
	float ShadowBoundingRadius,
	TScopedPointer<class FLightTileIntersectionResources>& TileIntersectionResources);

class FUniformMeshBuffers
{
public:

	int32 MaxElements;

	FVertexBufferRHIRef TriangleData;
	FShaderResourceViewRHIRef TriangleDataSRV;

	FRWBuffer TriangleAreas;
	FRWBuffer TriangleCDFs;

	FUniformMeshBuffers()
	{
		MaxElements = 0;
	}

	void Initialize();

	void Release()
	{
		TriangleData.SafeRelease();
		TriangleDataSRV.SafeRelease(); 
		TriangleAreas.Release();
		TriangleCDFs.Release();
	}
};

class FUniformMeshConverter
{
public:
	static int32 Convert(
		FRHICommandListImmediate& RHICmdList, 
		FSceneRenderer& Renderer,
		FViewInfo& View, 
		const FPrimitiveSceneInfo* PrimitiveSceneInfo, 
		int32 LODIndex,
		FUniformMeshBuffers*& OutUniformMeshBuffers,
		const FMaterialRenderProxy*& OutMaterialRenderProxy,
		FUniformBufferRHIParamRef& OutPrimitiveUniformBuffer);

	static void GenerateSurfels(
		FRHICommandListImmediate& RHICmdList, 
		FViewInfo& View, 
		const FPrimitiveSceneInfo* PrimitiveSceneInfo, 
		const FMaterialRenderProxy* MaterialProxy,
		FUniformBufferRHIParamRef PrimitiveUniformBuffer,
		const FMatrix& Instance0Transform,
		int32 SurfelOffset,
		int32 NumSurfels);
};

class FPreCulledTriangleBuffers
{
public:

	int32 MaxIndices;

	FRWBuffer TriangleVisibleMask;

	FPreCulledTriangleBuffers()
	{
		MaxIndices = 0;
	}

	void Initialize()
	{
		if (MaxIndices > 0)
		{
			TriangleVisibleMask.Initialize(sizeof(uint32), MaxIndices / 3, PF_R32_UINT);
		}
	}

	void Release()
	{
		TriangleVisibleMask.Release();
	}

	size_t GetSizeBytes() const
	{
		return TriangleVisibleMask.NumBytes;
	}
};

extern TGlobalResource<FDistanceFieldObjectBufferResource> GAOCulledObjectBuffers;

extern bool SupportsDistanceFieldAO(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform);