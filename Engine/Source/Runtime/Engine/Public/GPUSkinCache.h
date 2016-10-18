// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Copyright (C) Microsoft. All rights reserved.

/*=============================================================================
	GPUSkinCache.h: Performs skinning on a compute shader into a buffer to avoid vertex buffer skinning.
=============================================================================*/

// Requirements
// * Compute shader support (with Atomics)
// * Project settings needs to be enabled (r.SkinCache.CompileShaders)
// * feature need to be enabled (r.SkinCache.Mode)

// Features
// * Skeletal mesh, 4 / 8 weights per vertex, 16/32 index buffer
// * Supports Morph target animation (morph target blending is not done by this code)
// * Saves vertex shader computations when we render an object multiple times (EarlyZ, velocity, shadow, BasePass, CustomDepth, Shadow masking)
// * Fixes velocity rendering (needed for MotionBlur and TemporalAA) for WorldPosOffset animation and morph target animation
// * RecomputeTangents results in improved tangent space for WorldPosOffset animation and morph target animation
// * fixed amount of memory (r.MaxGPUSkinCacheElementsPerFrame, r.SkinCache.BufferSize)
// * Velocity Rendering for MotionBlur and TemporalAA (test Velocity in BasePass)
// * r.SkinCache.Mode and r.SkinCache.RecomputeTangents can be toggled at runtime

// TODO:
// * Test: Tessellation
// * Quality/Optimization: increase TANGENT_RANGE for better quality or accumulate two components in one 32bit value
// * Feature: RecomputeTangents should be a per section setting, the cvar (r.SkinCache.RecomputeTangents) can stay but should be on by default
// * Bug: We iterate through all chunks and try to find the Section. This is costly and might fail (Seems the section has a StartChunkIndex but it's called ChunkIndex)
// * Bug: UpdateMorphVertexBuffer needs to handle SkinCacheObjects that have been rejected by the SkinCache (e.g. because it was running out of memory)
// * Refactor: Unify the 3 compute shaders to use the same C++ setup code for the variables
// * Optimization: Dispatch calls can be merged for better performance, stalls between Dispatch calls can be avoided (DX11 back door, DX12, console API)
// * Feature: Needs show flag to see what objects are using the SkinCache
// * Investigate: There might be issues with the FrameNumber (SceneCaptures, Split screen, HitProxy rendering, pause, scrubbing in Sequencer) causing TemporalAA or MotionBlur artifacts
// * Feature: Cloth is not supported yet (Morph targets is a similar code)
// * Feature: Support Static Meshes ?

#pragma once

#include "Array.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "UniformBuffer.h"

typedef FRHIShaderResourceView* FShaderResourceViewRHIParamRef;

extern ENGINE_API int32 GEnableGPUSkinCacheShaders;
extern ENGINE_API int32 GEnableGPUSkinCache;

class FSkeletalMeshObjectGPUSkin;

class FGPUSkinCache : public FRenderResource
{
public:
	enum ESkinCacheInitSettings
	{
		// max 256 bones as we use a byte to index
		MaxUniformBufferBones = 256,
		// we allow up to this amount of objects being tracked
		MaxCachedElements = 1024,
		MaxCachedVertexBufferSRVs = 128,

		// Controls the output format on GpuSkinCacheComputeShader.usf
		RWPositionOffsetInFloats = 0,	// float3
		RWTangentXOffsetInFloats = 3,	// Packed U8x4N
		RWTangentZOffsetInFloats = 4,	// Packed U8x4N

		// stride in float(4 bytes) in the SkinCache buffer
		RWStrideInFloats = 5,
		// 3 ints for normal, 3 ints for tangent, 1 for orientation = 7, rounded up to 8 as it should result in faster math and caching
		IntermediateAccumBufferNumInts = 8,
		// max vertex count we support once the r.SkinCache.RecomputeTangents feature is used (GPU memory in bytes = n * sizeof(int) * IntermediateAccumBufferNumInts)
		// 4*1024 is the conservative size needed for Senua in CharDemo with NinjaTheory (this should be a cvar and project specific or dynamic, with only using the feature on RecomputeTangents it can be much less)
		IntermediateAccumBufferSizeInKB = 4 * 1024,
	};

	FGPUSkinCache();
	~FGPUSkinCache();

	// FRenderResource overrides
	virtual void ReleaseRHI() override;

	void Initialize(FRHICommandListImmediate& RHICmdList);
	void Cleanup();

	// @param FrameNumber from GFrameNumber or better ViewFamily.FrameNumber
	inline bool IsElementProcessed(uint32 FrameNumber, int32 Key) const
	{
		if (!GEnableGPUSkinCache)
		{
			return false;
		}

		return InternalIsElementProcessed(FrameNumber, Key);
	}

	// For each SkeletalMeshObject:
	//	Call Begin*
	//	For each Chunk:
	//		Call Add*
	//	Call End*
	// @param FrameNumber from GFrameNumber or better ViewFamily.FrameNumber
	// @param Key index in CachedElements[] (redundant, could be computed)
	// @param MorphVertexBuffer if no morph targets are used
	// @returns -1 if failed, otherwise index into CachedElements[]
	int32 StartCacheMesh(FRHICommandListImmediate& RHICmdList, uint32 FrameNumber, int32 Key, class FGPUBaseSkinVertexFactory* VertexFactory,
		class FGPUSkinPassthroughVertexFactory* TargetVertexFactory, const struct FSkelMeshSection& BatchElement, FSkeletalMeshObjectGPUSkin* Skin,
		const class FMorphVertexBuffer* MorphVertexBuffer);

	// @param FrameNumber from GFrameNumber or better ViewFamily.FrameNumber
	inline void SetVertexStreamFromCache(FRHICommandList& RHICmdList, uint32 FrameNumber, int32 Key, FShader* Shader, const FGPUSkinPassthroughVertexFactory* VertexFactory,
		uint32 BaseVertexIndex, FShaderParameter PreviousStreamFloatOffset, FShaderResourceParameter PreviousStreamBuffer)
	{
		if (Key >= 0 && Key < CachedElements.Num())
		{
			InternalSetVertexStreamFromCache(RHICmdList, FrameNumber, Key, Shader, VertexFactory, BaseVertexIndex, PreviousStreamFloatOffset, PreviousStreamBuffer);
			return;
		}

		ensure(0);
		// should never happen but better we render garbage than crash
	}

	struct FElementCacheStatusInfo
	{
		// key section -------------

		// TODO: using a pointer as key is dangerous
		const FSkelMeshSection* BatchElement;
		// TODO: using a pointer as key is dangerous
		const FSkeletalMeshObjectGPUSkin* Skin;

		// value section -------------

		// index in CachedElements[] (redundant, could be computed)
		int32	Key;
		// from GFrameNumber
		uint32	FrameUpdated;
		// start index in SkinCacheBuffer[UAVIndex]
		uint32 StreamOffset;
		// for velocity rendering (previous StreamOffset), start index in SkinCacheBuffer[UAVIndex]
		uint32 PreviousFrameStreamOffset;
		// from GFrameNumber
		uint32 PreviousFrameUpdated;

		// debugging section -------------

		//
		uint32 NumVertices;
		//
		uint32 BaseVertexIndex;
		// input stride in bytes
		uint32	InputVBStride;
		//
		bool bExtraBoneInfluences;
		//
		const FVertexFactory* VertexFactory;
		//
		const FVertexFactory* TargetVertexFactory;

		// -----------------------------

		struct FSearchInfo
		{
			const FSkeletalMeshObjectGPUSkin* Skin;
			const FSkelMeshSection* BatchElement;

			FSearchInfo()  { BatchElement = nullptr; Skin = nullptr; }
			FSearchInfo(const FSkeletalMeshObjectGPUSkin* InSkin, const FSkelMeshSection* InBatchElement) { BatchElement = InBatchElement; Skin = InSkin; }
		};

		// to support FindByKey()
		bool operator == (const FSearchInfo& OtherSearchInfo) const 
		{
			return (BatchElement == OtherSearchInfo.BatchElement && Skin == OtherSearchInfo.Skin);
		}

		// to support == (confusing usage, better: IsSameBatchElement)
		bool operator == (const FSkelMeshSection& OtherBatchElement) const
		{
			return (BatchElement == &OtherBatchElement);
		}
	};
	
	// used temporary when passing data to the Dispatch call chain
	struct FDispatchData 
	{
		FRHICommandListImmediate& RHICmdList;
		const FSkelMeshSection& Section;
		// for debugging / draw events, -1 if not set
		uint32 SectionIdx;
		ERHIFeatureLevel::Type FeatureLevel;
		
		// must not be 0
		FSkeletalMeshObjectGPUSkin* GPUSkin;

		// 0:normal, 1:with morph target, 2:with APEX cloth (not yet implemented)
		uint32 SkinType;
		//
		bool bExtraBoneInfluences;

		// SkinCache output input for RecomputeSkinTagents
		// 0 if no morph
		FRWBuffer* SkinCacheBuffer;
		// in floats (4 bytes)
		uint32 SkinCacheStart;
		uint32 NumVertices;

		// if floats (4 bytes)
		uint32 InputStreamStart;
		// in bytes
		uint32 InputStreamStride;
		FShaderResourceViewRHIRef InputVertexBufferSRV;

		// morph input
		FShaderResourceViewRHIParamRef MorphBuffer;
		uint32 MorphBufferOffset;

		// triangle index buffer (input for the RecomputeSkinTangents, might need special index buffer unique to position and normal, not considering UV/vertex color)
		FShaderResourceViewRHIParamRef IndexBuffer;
		uint32 IndexBufferOffsetValue;
		uint32 NumTriangles;
		
		FDispatchData(FRHICommandListImmediate& InRHICmdList, const FSkelMeshSection& InSection, ERHIFeatureLevel::Type InFeatureLevel, FSkeletalMeshObjectGPUSkin* InGPUSkin)
			: RHICmdList(InRHICmdList)
			, Section(InSection)
			, SectionIdx(-1)
			, FeatureLevel(InFeatureLevel)
			, GPUSkin(InGPUSkin)
			, SkinType(0)
			, bExtraBoneInfluences(false)
			, SkinCacheBuffer(0)
			, SkinCacheStart(0)
			, NumVertices(0)
			, InputStreamStart(0)
			, InputStreamStride(0)
			, MorphBuffer(0)
			, MorphBufferOffset(0)
			, IndexBuffer(0)
			, IndexBufferOffsetValue(0)
			, NumTriangles(0)
		{
			check(GPUSkin);
		}
	};

	ENGINE_API void TransitionToReadable(FRHICommandList& RHICmdList);
	ENGINE_API void TransitionToWriteable(FRHICommandList& RHICmdList);


private:
	// @param FrameNumber from GFrameNumber or better ViewFamily.FrameNumber
	FElementCacheStatusInfo* FindEvictableCacheStatusInfo(uint32 FrameNumber);

	void DispatchSkinCacheProcess(
		const struct FVertexBufferAndSRV& BoneBuffer, FUniformBufferRHIRef UniformBuffer,
		const FVector& MeshOrigin, const FVector& MeshExtension,
		const FDispatchData& DispatchData);
	
	void DispatchUpdateSkinTangents(const FDispatchData& DispatchData);

	// @param FrameNumber from GFrameNumber or better ViewFamily.FrameNumber
	void InternalSetVertexStreamFromCache(FRHICommandList& RHICmdList, uint32 FrameNumber, int32 Key, FShader* Shader, const class FGPUSkinPassthroughVertexFactory* VertexFactory,
		uint32 BaseVertexIndex, FShaderParameter PreviousStreamFloatOffset, FShaderResourceParameter PreviousStreamBuffer);

	void TransitionAllToWriteable(FRHICommandList& RHICmdList);

	// @param FrameNumber from GFrameNumber or better ViewFamily.FrameNumber
	bool InternalIsElementProcessed(uint32 FrameNumber, int32 Key) const;


	uint32 ComputeRecomputeTangentMaxVertexCount() const
	{
		uint32 IntermediateAccumBufferSizeInB = FGPUSkinCache::IntermediateAccumBufferSizeInKB * 1024;
		return IntermediateAccumBufferSizeInB / (sizeof(int32) * FGPUSkinCache::IntermediateAccumBufferNumInts);
	}

	// from GFrameNumber or better ViewFamily.FrameNumber
	bool bInitialized : 1;

	uint32	SkinCacheFrameNumber;

	int		CacheMaxVectorCount;
	int		CacheCurrentFloatOffset;
	int		CachedChunksThisFrameCount;

	TArray<FElementCacheStatusInfo>	CachedElements;

	FRWBuffer	SkinCacheBuffer[GPUSKINCACHE_FRAMES];

	// to accumulate tangents on the GPU with atomics, is shared and reused, if not large enough the method fails
	FRWBuffer SkinTangentIntermediate;

	static void CVarSinkFunction();

	static FAutoConsoleVariableSink CVarSink;
};

BEGIN_UNIFORM_BUFFER_STRUCT(GPUSkinCacheBonesUniformShaderParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,BoneVectors,[FGPUSkinCache::MaxUniformBufferBones*3])
END_UNIFORM_BUFFER_STRUCT(GPUSkinCacheBonesUniformShaderParameters)

extern ENGINE_API TGlobalResource<FGPUSkinCache> GGPUSkinCache;

DECLARE_STATS_GROUP(TEXT("GPU Skin Cache"), STATGROUP_GPUSkinCache, STATCAT_Advanced);

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Total Num Chunks"), STAT_GPUSkinCache_TotalNumChunks, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Total Num Vertices"), STAT_GPUSkinCache_TotalNumVertices, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Total Memory Used"), STAT_GPUSkinCache_TotalMemUsed, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Total Memory Wasted"), STAT_GPUSkinCache_TotalMemWasted, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Skipped for Max Chunks Per LOD"), STAT_GPUSkinCache_SkippedForMaxChunksPerLOD, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Skipped for Num Chunks Per Frame"), STAT_GPUSkinCache_SkippedForChunksPerFrame, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Skipped for zero influences"), STAT_GPUSkinCache_SkippedForZeroInfluences, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Skipped for Cloth"), STAT_GPUSkinCache_SkippedForCloth, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Skipped for Memory"), STAT_GPUSkinCache_SkippedForMemory, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Skipped for Already Cached"), STAT_GPUSkinCache_SkippedForAlreadyCached, STATGROUP_GPUSkinCache,);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Skipped for out of ECS"), STAT_GPUSkinCache_SkippedForOutOfECS, STATGROUP_GPUSkinCache,);
