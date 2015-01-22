// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// Copyright (C) Microsoft. All rights reserved.

/*=============================================================================
	GPUSkinCache.h: Performs skinning on a compute shader into a buffer to avoid vertex buffer skinning.
=============================================================================*/

#pragma once

#include "Array.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "UniformBuffer.h"

class FSkeletalMeshObjectGPUSkin;

class FGPUSkinCache : public FRenderResource
{
public:
	enum SkinCacheInitSettings
	{
		MaxBufferSize = 1024 * 1024 * 32,
		MaxUniformBufferBones = 256,
		MaxCachedElements = 1024,
		MaxCachedVertexBufferSRVs = 128,
	};

	FGPUSkinCache();
	~FGPUSkinCache();

	// FRenderResource overrides
	virtual void ReleaseRHI() override;

	void	Initialize();
	void	Cleanup();

	bool	IsElementProcessed(int32 Key);

	// For each SkeletalMeshObject:
	//	Call Begin*
	//	For each Chunk:
	//		Call Add*
	//	Call End*
	int32	StartCacheMesh(FRHICommandListImmediate& RHICmdList, int32 Key, const class FVertexFactory* VertexFactory, const FVertexFactory* TargetVertexFactory, const struct FSkelMeshChunk& BatchElement, const FSkeletalMeshObjectGPUSkin* Skin, bool bExtraBoneInfluences);

	bool	SetVertexStreamFromCache(FRHICommandList& RHICmdList, int32 Key, FShader* Shader, const FVertexFactory* VertexFactory, uint32 BaseVertexIndex, bool VelocityPass, FShaderParameter PreviousStreamFloatOffset, FShaderParameter PreviousStreamFloatStride, FShaderResourceParameter PreviousStreamBuffer);

	struct FElementCacheStatusInfo
	{
		const FSkelMeshChunk* BatchElement;
		const FVertexFactory* VertexFactory;
		const FVertexFactory* TargetVertexFactory;
		const FSkeletalMeshObjectGPUSkin* Skin;

		int32	Key;
		int32	VertexBufferSRVIndex;

		uint32	FrameUpdated;

		uint32	StreamStride;
		uint32	StreamOffset;
		uint32	PreviousFrameStreamOffset;

		bool	bExtraBoneInfluences;

		struct FSearchInfo
		{
			const FSkeletalMeshObjectGPUSkin* Skin;
			const FSkelMeshChunk* BatchElement;

			FSearchInfo()  { BatchElement = nullptr; Skin = nullptr; }
			FSearchInfo(const FSkeletalMeshObjectGPUSkin* InSkin, const FSkelMeshChunk* InBatchElement) { BatchElement = InBatchElement; Skin = InSkin; }
		};

		bool operator == (const FSearchInfo& OtherSearchInfo) const 
		{
			return (BatchElement == OtherSearchInfo.BatchElement && Skin == OtherSearchInfo.Skin);
		}

		bool operator == (const FSkelMeshChunk& OtherBatchElement) const 
		{
			return (BatchElement == &OtherBatchElement);
		}
	};

	struct FVertexBufferInfo
	{
		const FVertexBuffer* VertexBuffer;
		FShaderResourceViewRHIRef	VertexBufferSRV;
		int32	Index;

		bool operator == (const FVertexBuffer& OtherVertexBuffer) const 
		{
			return (VertexBuffer == &OtherVertexBuffer);
		}
	};

private:

	struct FDispatchInfo
	{
		uint32 InputStreamFloatOffset;
		uint32 OutputBufferFloatOffset;
		uint32 VertexStride;
		uint32 VertexCount;

		FUniformBufferRHIRef UniformBuffer;
		FBoneBufferTypeRef BoneBuffer;

		const FVertexBufferInfo* VBInfo;

		const class FGPUBaseSkinVertexFactory* GPUVertexFactory;
	};

	FElementCacheStatusInfo* FindEvictableCacheStatusInfo();

	void	DispatchSkinCacheProcess(FRHICommandListImmediate& RHICmdList, uint32 InputStreamFloatOffset, uint32 OutputBufferFloatOffset, FBoneBufferTypeRef BoneBuffer, FUniformBufferRHIRef UniformBuffer, const FVertexBufferInfo* VBInfo, uint32 VertexStride, uint32 VertexCount, const FVector& MeshOrigin, const FVector& MeshExtension, bool bUseExtraBoneInfluences, ERHIFeatureLevel::Type FeatureLevel);

	bool	Initialized : 1;

	uint32	FrameCounter;

	int		CacheMaxVectorCount;
	int		CacheCurrentFloatOffset;
	int		CacheCurrentFrameOffset;
	int		CachedChunksThisFrameCount;

	TArray<FVertexBufferInfo>		CachedVertexBuffers;

	TArray<FElementCacheStatusInfo>	CachedElements;

	FRWBuffer	SkinCacheBufferRW;
};

BEGIN_UNIFORM_BUFFER_STRUCT(GPUSkinCacheBonesUniformShaderParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,BoneVectors,[FGPUSkinCache::MaxUniformBufferBones*3])
END_UNIFORM_BUFFER_STRUCT(GPUSkinCacheBonesUniformShaderParameters)

extern	int32 GEnableGPUSkinCache;
extern	TGlobalResource<FGPUSkinCache> GGPUSkinCache;
