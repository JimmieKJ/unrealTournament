// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// Copyright (C) Microsoft. All rights reserved.

/*=============================================================================
	GPUSkinCache.cpp: Performs skinning on a compute shader into a buffer to avoid vertex buffer skinning.
=============================================================================*/

#include "EnginePrivate.h"
#include "GlobalShader.h"
#include "SceneManagement.h"
#include "GPUSkinVertexFactory.h"
#include "SkeletalRenderGPUSkin.h"
#include "GPUSkinCache.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"

DEFINE_STAT(STAT_GPUSkinCache_TotalNumChunks);
DEFINE_STAT(STAT_GPUSkinCache_TotalNumVertices);
DEFINE_STAT(STAT_GPUSkinCache_TotalMemUsed);
DEFINE_STAT(STAT_GPUSkinCache_SkippedForMaxChunksPerLOD);
DEFINE_STAT(STAT_GPUSkinCache_SkippedForChunksPerFrame);
DEFINE_STAT(STAT_GPUSkinCache_SkippedForZeroInfluences);
DEFINE_STAT(STAT_GPUSkinCache_SkippedForCloth);
DEFINE_STAT(STAT_GPUSkinCache_SkippedForMemory);
DEFINE_STAT(STAT_GPUSkinCache_SkippedForAlreadyCached);
DEFINE_STAT(STAT_GPUSkinCache_SkippedForOutOfECS);

DEFINE_LOG_CATEGORY_STATIC(LogSkinCache, Log, All);

int32 GEnableGPUSkinCacheShaders = 0;
static FAutoConsoleVariableRef CVarEnableGPUSkinCacheShaders(
	TEXT("r.SkinCacheShaders"),
	GEnableGPUSkinCacheShaders,
	TEXT("Whether or not to compile the GPU compute skinning cache shaders.\n")
	TEXT("This will compile the shaders for skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("GPUSkinVertexFactory.usf needs to be touched to cause a recompile if this changes.\n")
	TEXT("0 is off(default), 1 is on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

// 0/1
int32 GEnableGPUSkinCache = 0;
static TAutoConsoleVariable<int32> CVarEnableGPUSkinCache(
	TEXT("r.SkinCaching"),
	0,
	TEXT("Whether or not to use the GPU compute skinning cache.\n")
	TEXT("This will perform skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("Requires r.SkinCacheShaders=1.\n")
	TEXT(" 0: off(default)\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarGPUSkinCacheRecomputeTangents(
	TEXT("r.SkinCache.RecomputeTangents"),
	0,
	TEXT("If r.SkinCaching is enabled this option recomputes the vertex tangents on the GPU\n")
	TEXT("Can be changed at runtime, Requires r.SkinCacheShaders=1 and r.SkinCache=1\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe
	);

static int32 GMaxGPUSkinCacheElementsPerFrame = 1000;
static FAutoConsoleVariableRef CVarMaxGPUSkinCacheElementsPerFrame(
	TEXT("r.MaxGPUSkinCacheElementsPerFrame"),
	GMaxGPUSkinCacheElementsPerFrame,
	TEXT("The maximum compute processed skin cache elements per frame. Only used with r.SkinCaching=1\n")
	TEXT(" (default is 1000)")
	);

static int32 GGPUSkinCacheBufferSize = 99 * 1024 * 1024;
static FAutoConsoleVariableRef CVarMaxGPUSkinCacheBufferSize(
	TEXT("r.SkinCache.BufferSize"),
	GMaxGPUSkinCacheElementsPerFrame,
	TEXT("The maximum memory used for writing out the vertices in bytes\n")
	TEXT("Split into GPUSKINCACHE_FRAMES chunks (eg 300 MB means 100 MB for 3 frames)\n")
	TEXT("Default is 99MB.  Only used with r.SkinCaching=1\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarGPUSkinCacheDebug(
	TEXT("r.SkinCache.Debug"),
	1.0f,
	TEXT("A constant passed to the SkinCache shader, useful for debugging"),
	ECVF_RenderThreadSafe
	);

// We don't have it always enabled as it's not clear if this has a performance cost
// Call on render thread only!
// Should only be called if SM5 (compute shaders, atomics) are supported.
ENGINE_API bool DoSkeletalMeshIndexBuffersNeedSRV()
{
	// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
	return GMaxRHIShaderPlatform == SP_PCD3D_SM5 && GEnableGPUSkinCacheShaders != 0;
}

ENGINE_API bool DoRecomputeSkinTangentsOnGPU()
{
	// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
	return GMaxRHIShaderPlatform == SP_PCD3D_SM5 && GEnableGPUSkinCacheShaders && GEnableGPUSkinCache && CVarGPUSkinCacheRecomputeTangents.GetValueOnRenderThread() != 0;
}

TGlobalResource<FGPUSkinCache> GGPUSkinCache;

IMPLEMENT_UNIFORM_BUFFER_STRUCT(GPUSkinCacheBonesUniformShaderParameters,TEXT("GPUSkinCacheBones"));


class FBaseGPUSkinCacheCS : public FGlobalShader
{
public:
	FBaseGPUSkinCacheCS() {} 

	FBaseGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SkinMeshOriginParameter.Bind(Initializer.ParameterMap, TEXT("MeshOrigin"));
		SkinMeshExtensionParameter.Bind(Initializer.ParameterMap, TEXT("MeshExtension"));

		//DebugParameter.Bind(Initializer.ParameterMap, TEXT("DebugParameter"));

		SkinInputStreamStride.Bind(Initializer.ParameterMap, TEXT("SkinCacheVertexStride"));
		SkinInputStreamVertexCount.Bind(Initializer.ParameterMap, TEXT("SkinCacheVertexCount"));
		SkinOutputBufferFloatOffset.Bind(Initializer.ParameterMap, TEXT("SkinCacheOutputBufferFloatOffset"));
		BoneMatrices.Bind(Initializer.ParameterMap, TEXT("BoneMatrices"));
		SkinInputStream.Bind(Initializer.ParameterMap, TEXT("SkinStreamInputBuffer"));
		SkinInputStreamFloatOffset.Bind(Initializer.ParameterMap, TEXT("SkinCacheInputStreamFloatOffset"));
		
		SkinCacheBuffer.Bind(Initializer.ParameterMap, TEXT("SkinCacheBuffer"));

		MorphBuffer.Bind(Initializer.ParameterMap, TEXT("MorphBuffer"));
		MorphBufferOffset.Bind(Initializer.ParameterMap, TEXT("MorphBufferOffset"));
		SkinCacheDebug.Bind(Initializer.ParameterMap, TEXT("SkinCacheDebug"));
	}

	void SetParameters(const FVertexBufferAndSRV& BoneBuffer, FUniformBufferRHIRef UniformBuffer,
		FRWBuffer& SkinBuffer, const FVector& MeshOrigin, const FVector& MeshExtension,
		const FGPUSkinCache::FDispatchData& DispatchData)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FRHICommandListImmediate& RHICmdList = DispatchData.RHICmdList;

		SetShaderValue(RHICmdList, ShaderRHI, SkinMeshOriginParameter, MeshOrigin);
		SetShaderValue(RHICmdList, ShaderRHI, SkinMeshExtensionParameter, MeshExtension);

		SetShaderValue(RHICmdList, ShaderRHI, SkinInputStreamStride, DispatchData.InputStreamStride);
		SetShaderValue(RHICmdList, ShaderRHI, SkinInputStreamVertexCount, DispatchData.NumVertices);
		SetShaderValue(RHICmdList, ShaderRHI, SkinInputStreamFloatOffset, DispatchData.InputStreamFloatOffset);
		//SetShaderValue(RHICmdList, ShaderRHI, DebugParameter, FUintVector4((uint32)(uint64)VertexBufferSRV.GetReference(), 0, 0, 1));

		if (UniformBuffer)
		{
			SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<GPUSkinCacheBonesUniformShaderParameters>(), UniformBuffer);
		}
		else
		{
			SetSRVParameter(RHICmdList, ShaderRHI, BoneMatrices, BoneBuffer.VertexBufferSRV);
		}

		SetSRVParameter(RHICmdList, ShaderRHI, SkinInputStream, DispatchData.InputVertexBufferSRV);

		// output UAV
		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBuffer, SkinBuffer.UAV);
		SetShaderValue(RHICmdList, ShaderRHI, SkinOutputBufferFloatOffset, DispatchData.OutputBufferFloatOffset);
	
		const bool bMorph = DispatchData.SkinType == 1;
		if(bMorph)
		{
			SetSRVParameter(RHICmdList, ShaderRHI, MorphBuffer, DispatchData.MorphBuffer);
			SetShaderValue(RHICmdList, ShaderRHI, MorphBufferOffset, DispatchData.MorphBufferOffset);
		}

		SetShaderValue(RHICmdList, ShaderRHI, SkinCacheDebug, CVarGPUSkinCacheDebug.GetValueOnRenderThread());
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBuffer, 0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << SkinMeshOriginParameter << SkinMeshExtensionParameter << SkinInputStreamStride
			<< SkinInputStreamVertexCount << SkinInputStreamFloatOffset << SkinOutputBufferFloatOffset
			<< SkinInputStream << SkinCacheBuffer << BoneMatrices << MorphBuffer << MorphBufferOffset
			<< SkinCacheDebug;
		//Ar << DebugParameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter SkinMeshOriginParameter;
	FShaderParameter SkinMeshExtensionParameter;

	FShaderParameter SkinInputStreamStride;
	FShaderParameter SkinInputStreamVertexCount;
	FShaderParameter SkinCacheDebug;
	FShaderParameter SkinInputStreamFloatOffset;
	FShaderParameter SkinOutputBufferFloatOffset;

	//FShaderParameter DebugParameter;

	FShaderUniformBufferParameter SkinUniformBuffer;

	FShaderResourceParameter BoneMatrices;
	FShaderResourceParameter SkinInputStream;
	FShaderResourceParameter SkinCacheBuffer;

	FShaderResourceParameter MorphBuffer;
	FShaderParameter MorphBufferOffset;
};

/** Compute shader that skins a batch of vertices. */
// @param SkinType 0:normal, 1:with morph target, 2:with APEX cloth (not yet implemented)
template <bool bUseExtraBoneInfluencesT, uint32 SkinType>
class TGPUSkinCacheCS : public FBaseGPUSkinCacheCS
{
	DECLARE_SHADER_TYPE(TGPUSkinCacheCS, Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return GEnableGPUSkinCacheShaders && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		const uint32 UseExtraBoneInfluences = bUseExtraBoneInfluencesT;
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), UseExtraBoneInfluences);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_MORPH_BLEND"), (uint32)(SkinType == 1));

		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_POSITION"), FGPUSkinCache::RWPositionOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_X"), FGPUSkinCache::RWTangentXOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_Z"), FGPUSkinCache::RWTangentZOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_NUM_FLOATS"), FGPUSkinCache::RWStrideInFloats);
	}

	TGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseGPUSkinCacheCS(Initializer)
	{
	}

	TGPUSkinCacheCS()
	{
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("GpuSkinCacheComputeShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("SkinCacheUpdateBatchCS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)
#define VARIATION2(A, B) typedef TGPUSkinCacheCS<A, B> TGPUSkinCacheCS##A##B; \
	IMPLEMENT_SHADER_TYPE2(TGPUSkinCacheCS##A##B, SF_Compute);
	VARIATION1(0) VARIATION1(1)
#undef VARIATION1
#undef VARIATION2

FGPUSkinCache::FGPUSkinCache()
	: bInitialized(false)
	, FrameCounter(0)
	, CacheMaxVectorCount(0)
	, CacheCurrentFloatOffset(0)
	, CachedChunksThisFrameCount(0)
{
}

FGPUSkinCache::~FGPUSkinCache()
{
	Cleanup();
}

void FGPUSkinCache::ReleaseRHI()
{
	Cleanup();
}

void FGPUSkinCache::Initialize(FRHICommandListImmediate& RHICmdList)
{
	check(!bInitialized);

	UE_LOG(LogSkinCache, Log, TEXT("GPUSkinCache:"));

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BasePassOutputsVelocity"));
	checkf(!CVar || CVar->GetValueOnRenderThread() == 0, TEXT("GPU Skin caching is not allowed with outputting velocity on base pass (r.BasePassOutputsVelocity=1)"));

	checkf(GEnableGPUSkinCacheShaders, TEXT("GPU Skin caching requires the shaders enabled (r.SkinCacheShaders=1)"));

	int32 NumFloatsPerBuffer = GGPUSkinCacheBufferSize / GPUSKINCACHE_FRAMES / sizeof(float);
	for (int32 Index = 0; Index < GPUSKINCACHE_FRAMES; ++Index)
	{
		SkinCacheBuffer[Index].Initialize(sizeof(float), NumFloatsPerBuffer, PF_R32_FLOAT, BUF_Static);

		FString Name = FString::Printf(TEXT("SkinCacheRWBuffer%d"), Index);
		RHICmdList.BindDebugLabelName(SkinCacheBuffer[Index].UAV, *Name);
	}

	UE_LOG(LogSkinCache, Log, TEXT("  Frames:            %d"), GPUSKINCACHE_FRAMES);
	UE_LOG(LogSkinCache, Log, TEXT("  VertexBuffers:     %.1f MB"), FMath::DivideAndRoundUp(GGPUSkinCacheBufferSize * GPUSKINCACHE_FRAMES, 1024) / 1024.0f);

	// we always allocate so we can change r.SkinCache.RecomputeTangents at runtime
	{
		uint32 MaxVertexCount = ComputeRecomputeTangentMaxVertexCount();
		uint32 NumIntsPerBuffer = MaxVertexCount * FGPUSkinCache::IntermediateAccumBufferNumInts;
		SkinTangentIntermediate.Initialize(sizeof(int32), NumIntsPerBuffer, PF_R32_SINT, BUF_UnorderedAccess);
		RHICmdList.BindDebugLabelName(SkinTangentIntermediate.UAV, TEXT("SkinTangentIntermediate"));

		uint32 MemInB = MaxVertexCount * FGPUSkinCache::IntermediateAccumBufferNumInts * sizeof(int32);
		UE_LOG(LogSkinCache, Log, TEXT("  RecomputeTangents: %.1f MB"), FMath::DivideAndRoundUp(MemInB, (uint32)1024) / 1024.0f);

		// We clear once in the beginning and then each time in the PerVertexPass (this saves the clear or another Dispatch)
		uint32 Value[4] = { 0, 0, 0, 0 };
		RHICmdList.ClearUAV(SkinTangentIntermediate.UAV, Value);
	}

	TransitionAllToWriteable(RHICmdList);

	CachedElements.Reserve(MaxCachedElements);

	bInitialized = true;
}

void FGPUSkinCache::Cleanup()
{
	if (bInitialized)
	{
		for (int32 Index = 0; Index < GPUSKINCACHE_FRAMES; ++Index)
		{
			SkinCacheBuffer[Index].Release();
		}

		SkinTangentIntermediate.Release();

		bInitialized = false;
	}
}

// Kick off compute shader skinning for incoming chunk, return key for fast lookup in draw passes
int32 FGPUSkinCache::StartCacheMesh(FRHICommandListImmediate& RHICmdList, int32 Key, FGPUBaseSkinVertexFactory* VertexFactory,
	FGPUSkinPassthroughVertexFactory* TargetVertexFactory, const FSkelMeshChunk& BatchElement, FSkeletalMeshObjectGPUSkin* Skin,
	const FMorphVertexBuffer* MorphVertexBuffer)
{
	if (CachedChunksThisFrameCount >= GMaxGPUSkinCacheElementsPerFrame && GFrameNumberRenderThread <= FrameCounter)
	{
		INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForChunksPerFrame);
		return -1;
	}

	if (!bInitialized)
	{
		Initialize(RHICmdList);
	}

	if (GFrameNumberRenderThread > FrameCounter)	// Reset cache output if on a new frame
	{
		CacheCurrentFloatOffset = 0;
		CachedChunksThisFrameCount = 0;
		FrameCounter = GFrameNumberRenderThread;
	}

	FElementCacheStatusInfo::FSearchInfo SInfo;
	SInfo.BatchElement = &BatchElement;
	SInfo.Skin = Skin;

	FElementCacheStatusInfo* ECSInfo = nullptr;

	if (Key >= 0 && Key < CachedElements.Num())
	{
		ECSInfo = &(CachedElements.GetData()[Key]);

		if (ECSInfo->Skin != Skin || &BatchElement != ECSInfo->BatchElement)
		{
			ECSInfo = CachedElements.FindByKey(SInfo);
		}
	}
	else
	{
		ECSInfo = CachedElements.FindByKey(SInfo);
	}

	bool AlignStreamOffsetsFrames = false;

	const bool bExtraBoneInfluences = VertexFactory->UsesExtraBoneInfluences();
	bool bHasPreviousOffset = false;
	if (ECSInfo != nullptr)
	{
		if (ECSInfo->FrameUpdated == GFrameNumberRenderThread)	// Already cached this element this frame, return 0 key
		{
			INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForAlreadyCached);
			return -1;
		}

		AlignStreamOffsetsFrames = (GFrameNumberRenderThread - ECSInfo->FrameUpdated) > 2;

		ECSInfo->FrameUpdated = GFrameNumberRenderThread;
		ECSInfo->PreviousFrameStreamOffset = ECSInfo->StreamOffset;
		bHasPreviousOffset = true;
	}
	else	// New element, init and add
	{
		if (CachedElements.Num() < MaxCachedElements)
		{
			FElementCacheStatusInfo	Info;
			Info.BatchElement = &BatchElement;
			Info.VertexFactory = VertexFactory;
			Info.TargetVertexFactory = TargetVertexFactory;
			Info.Skin = Skin;
			Info.Key = CachedElements.Num();
			Info.FrameUpdated = GFrameNumberRenderThread;
			Info.bExtraBoneInfluences = bExtraBoneInfluences;
			CachedElements.Add(Info);

			ECSInfo = &(CachedElements.GetData()[Info.Key]);
		}
		else
		{
			FElementCacheStatusInfo*	Info = FindEvictableCacheStatusInfo();

			if (Info == nullptr)
			{
				INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForOutOfECS);
				return -1;
			}

			Info->BatchElement = &BatchElement;
			Info->VertexFactory = VertexFactory;
			Info->TargetVertexFactory = TargetVertexFactory;
			Info->Skin = Skin;
			Info->FrameUpdated = GFrameNumberRenderThread;
			Info->bExtraBoneInfluences = bExtraBoneInfluences;

			ECSInfo = Info;
		}

		AlignStreamOffsetsFrames = true;
	}

	uint32	StreamStrides[MaxVertexElementCount];
	uint32	StreamStrideCount = VertexFactory->GetStreamStrides(StreamStrides);

	uint32	NumVertices = BatchElement.GetNumVertices();

	uint32	InputStreamFloatOffset = (StreamStrides[0] * BatchElement.BaseVertexIndex) / sizeof(float);

	uint32	NumRWFloats = (RWStrideInFloats * NumVertices);

	int32 CompensatedOffset = ((int32)CacheCurrentFloatOffset * sizeof(float)) - (BatchElement.BaseVertexIndex * StreamStrides[0]);
	if (CompensatedOffset < 0)
	{
		CacheCurrentFloatOffset -= CompensatedOffset / sizeof(float);
	}

	if (CacheCurrentFloatOffset + NumRWFloats > (uint32)GGPUSkinCacheBufferSize)
	{
		// Can't fit this
		INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForMemory);
		return -1;
	}

	INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalMemUsed, NumRWFloats * sizeof(float));

	ECSInfo->InputVBStride = StreamStrides[0];
	ECSInfo->StreamOffset = CacheCurrentFloatOffset;

	if (AlignStreamOffsetsFrames || !bHasPreviousOffset)
	{
		ECSInfo->PreviousFrameStreamOffset = ECSInfo->StreamOffset;
	}

	const FSkeletalMeshVertexBuffer* SkeletalMeshVertexBuffer = VertexFactory->GetSkinVertexBuffer();

	FDispatchData DispatchData(RHICmdList, BatchElement, Skin->GetFeatureLevel(), Skin);
	DispatchData.NumVertices = NumVertices;

	if(MorphVertexBuffer)
	{
		DispatchData.MorphBuffer = MorphVertexBuffer->GetSRV();
		check(DispatchData.MorphBuffer);

		// in bytes
		const uint32 MorphStride = sizeof(FMorphGPUSkinVertex);

		// see GPU code "check(MorphStride == sizeof(float) * 6);"
		check(MorphStride == sizeof(float) * 6);

		DispatchData.MorphBufferOffset = (MorphStride * BatchElement.BaseVertexIndex) / sizeof(float);
	}

	INC_DWORD_STAT(STAT_GPUSkinCache_TotalNumChunks);
	INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, NumVertices);

	auto& ShaderData = VertexFactory->GetShaderData();
	
	// SkinType 0:normal, 1:with morph target, 2:with APEX cloth (not yet implemented)
	DispatchData.SkinType = MorphVertexBuffer ? 1 : 0;
	DispatchData.InputStreamFloatOffset = InputStreamFloatOffset;
	DispatchData.OutputBufferFloatOffset = ECSInfo->StreamOffset;

	int32 UAVIndex = FrameCounter % GPUSKINCACHE_FRAMES;
	DispatchData.SkinCacheBuffer = &SkinCacheBuffer[UAVIndex];
	DispatchData.InputStreamStride = StreamStrides[0];
	DispatchData.InputVertexBufferSRV = SkeletalMeshVertexBuffer->GetSRV();
	check(DispatchData.InputVertexBufferSRV);

	DispatchSkinCacheProcess(
		ShaderData.GetBoneBufferForWriting(false, GFrameNumberRenderThread), ShaderData.GetUniformBuffer(),
		ShaderData.MeshOrigin, ShaderData.MeshExtension, bExtraBoneInfluences,
		DispatchData);

	TargetVertexFactory->UpdateVertexDeclaration(VertexFactory, &SkinCacheBuffer[FrameCounter % GPUSKINCACHE_FRAMES]);

	const bool bRecomputeTangents = MorphVertexBuffer && CVarGPUSkinCacheRecomputeTangents.GetValueOnRenderThread() != 0;

	if(bRecomputeTangents)
	{
		FStaticLODModel* StaticLODModel = MorphVertexBuffer->GetStaticLODModel();
		FRawStaticIndexBuffer16or32Interface* IndexBuffer = StaticLODModel->MultiSizeIndexContainer.GetIndexBuffer();

		DispatchData.IndexBuffer = IndexBuffer->GetSRV();
		check(DispatchData.IndexBuffer);

		// todo: this can be optimized, worse: with multiple chunks per section this is likely not to work
		const FSkelMeshSection* Section = StaticLODModel->FindSectionForChunk(StaticLODModel->FindChunkIndex(BatchElement));
		DispatchData.NumTriangles = Section->NumTriangles;
		DispatchData.IndexBufferOffsetValue = Section->BaseIndex;
		DispatchUpdateSkinTangents(DispatchData);
	}

	CacheCurrentFloatOffset += NumRWFloats;
	CachedChunksThisFrameCount++;

	return ECSInfo->Key;
}

void FGPUSkinCache::TransitionToReadable(FRHICommandList& RHICmdList)
{
	int32 BufferIndex = FrameCounter % GPUSKINCACHE_FRAMES;
	FUnorderedAccessViewRHIParamRef OutUAVs[] ={SkinCacheBuffer[BufferIndex].UAV};
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutUAVs, ARRAY_COUNT(OutUAVs));
}

void FGPUSkinCache::TransitionToWriteable(FRHICommandList& RHICmdList)
{
	int32 BufferIndex = (FrameCounter + GPUSKINCACHE_FRAMES - 1) % GPUSKINCACHE_FRAMES;
	FUnorderedAccessViewRHIParamRef OutUAVs[] = {SkinCacheBuffer[BufferIndex].UAV};
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
}

void FGPUSkinCache::TransitionAllToWriteable(FRHICommandList& RHICmdList)
{
	if (bInitialized)
	{
		FUnorderedAccessViewRHIParamRef OutUAVs[GPUSKINCACHE_FRAMES];

		for (int32 Index = 0; Index < GPUSKINCACHE_FRAMES; ++Index)
		{
			OutUAVs[Index] = SkinCacheBuffer[Index].UAV;
		}

		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}
}

bool FGPUSkinCache::InternalIsElementProcessed(int32 Key) const
{
	if (Key >= 0 && Key < CachedElements.Num())
	{
		const FElementCacheStatusInfo& CacheInfo = CachedElements.GetData()[Key];

		if (CacheInfo.FrameUpdated == GFrameNumberRenderThread)
		{
			return true;
		}
	}

	return false;
}

bool FGPUSkinCache::InternalSetVertexStreamFromCache(FRHICommandList& RHICmdList, int32 Key, FShader* Shader, const FGPUSkinPassthroughVertexFactory* VertexFactory, uint32 BaseVertexIndex, FShaderParameter PreviousStreamFloatOffset, FShaderResourceParameter PreviousStreamBuffer)
{
	FElementCacheStatusInfo& CacheInfo = CachedElements.GetData()[Key];

	// Verify that we found the correct chunk and that is was processed this frame
	if (CacheInfo.BatchElement->BaseVertexIndex == BaseVertexIndex && CacheInfo.TargetVertexFactory == VertexFactory && CacheInfo.FrameUpdated == GFrameNumberRenderThread)
	{
		// Compensated offset since the StreamOffset is relative to the RWBuffer start, while the SetStreamSource one is relative to the BaseVertexIndex
		int32 CompensatedOffset = ((int32)CacheInfo.StreamOffset * sizeof(float)) - (CacheInfo.BatchElement->BaseVertexIndex * RWStrideInFloats * sizeof(float));
		if (CompensatedOffset < 0)
		{
			return false;
		}

		int32 UAVIndex = FrameCounter % GPUSKINCACHE_FRAMES;
		RHICmdList.SetStreamSource(VertexFactory->GetStreamIndex(UAVIndex), SkinCacheBuffer[UAVIndex].Buffer, RWStrideInFloats * sizeof(float), CompensatedOffset);

		FVertexShaderRHIParamRef ShaderRHI = Shader->GetVertexShader();
		if (ShaderRHI)
		{
			int32 PreviousCompensatedOffset = ((int32)CacheInfo.PreviousFrameStreamOffset * sizeof(float)) - (CacheInfo.BatchElement->BaseVertexIndex * RWStrideInFloats * sizeof(float));
			if (PreviousCompensatedOffset < 0)
			{
				return false;
			}

			if (PreviousStreamBuffer.IsBound())
			{
				int32 PrevUAVIndex = (FrameCounter + GPUSKINCACHE_FRAMES - 1) % GPUSKINCACHE_FRAMES;
				SetShaderValue(RHICmdList, ShaderRHI, PreviousStreamFloatOffset, PreviousCompensatedOffset / sizeof(float));
				RHICmdList.SetShaderResourceViewParameter(ShaderRHI, PreviousStreamBuffer.GetBaseIndex(), SkinCacheBuffer[PrevUAVIndex].SRV);
			}
		}

		return true;
	}

	return false;
}

FGPUSkinCache::FElementCacheStatusInfo* FGPUSkinCache::FindEvictableCacheStatusInfo()
{
	if (!CachedElements.Num())
	{
		return nullptr;
	}

	uint32	BestFrameCounter = 0x7fffffff;
	FElementCacheStatusInfo* BestEntry = nullptr;

	uint32	TargetFrameCounter = GFrameNumberRenderThread >= 10 ? GFrameNumberRenderThread - 10 : 0;

	FElementCacheStatusInfo* CSInfo = CachedElements.GetData();

	for (int32 i = 0; i < CachedElements.Num(); i++)
	{
		if (CSInfo->FrameUpdated < BestFrameCounter)
		{
			BestEntry = CSInfo;
			BestFrameCounter = CSInfo->FrameUpdated;

			if (BestFrameCounter < TargetFrameCounter)
			{
				break;
			}
		}

		CSInfo++;
	}

	if (BestEntry && BestFrameCounter < GFrameNumberRenderThread)
	{
		return BestEntry;
	}
	else
	{
		return nullptr;
	}
}

void FGPUSkinCache::DispatchSkinCacheProcess(
	const FVertexBufferAndSRV& BoneBuffer, FUniformBufferRHIRef UniformBuffer,
	const FVector& MeshOrigin, const FVector& MeshExtension, bool bUseExtraBoneInfluences,
	const FDispatchData& DispatchData)
{
	check(DispatchData.FeatureLevel >= ERHIFeatureLevel::SM5);

	SCOPED_DRAW_EVENT(DispatchData.RHICmdList, SkinCacheDispatch);

	int32 UAVIndex = FrameCounter % GPUSKINCACHE_FRAMES;

	TShaderMapRef<TGPUSkinCacheCS<true,0> > SkinCacheCS10(GetGlobalShaderMap(DispatchData.FeatureLevel));
	TShaderMapRef<TGPUSkinCacheCS<false,0> > SkinCacheCS00(GetGlobalShaderMap(DispatchData.FeatureLevel));
	TShaderMapRef<TGPUSkinCacheCS<true,1> > SkinCacheCS11(GetGlobalShaderMap(DispatchData.FeatureLevel));
	TShaderMapRef<TGPUSkinCacheCS<false,1> > SkinCacheCS01(GetGlobalShaderMap(DispatchData.FeatureLevel));

	FBaseGPUSkinCacheCS* Shader = 0;
	
	switch(DispatchData.SkinType)
	{
		case 0: Shader = bUseExtraBoneInfluences ? (FBaseGPUSkinCacheCS*)*SkinCacheCS10 : (FBaseGPUSkinCacheCS*)*SkinCacheCS00; break;
		case 1: Shader = bUseExtraBoneInfluences ? (FBaseGPUSkinCacheCS*)*SkinCacheCS11 : (FBaseGPUSkinCacheCS*)*SkinCacheCS01; break;
		default:
			check(0);
	}
	
	check(Shader);
	DispatchData.RHICmdList.SetComputeShader(Shader->GetComputeShader());
	Shader->SetParameters(BoneBuffer, UniformBuffer,
		SkinCacheBuffer[UAVIndex], MeshOrigin, MeshExtension,
		DispatchData);

	uint32 VertexCountAlign64 = FMath::DivideAndRoundUp(DispatchData.NumVertices, (uint32)64);
	DispatchData.RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);
	Shader->UnsetParameters(DispatchData.RHICmdList);
}

/** Encapsulates the RecomputeSkinTangents compute shader. */
template <uint32 FullPrecisionUV>
class FRecomputeTangentsPerTrianglePassCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRecomputeTangentsPerTrianglePassCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
		OutEnvironment.SetDefine(TEXT("FULL_PRECISION_UV"), FullPrecisionUV);
	}

	static const uint32 ThreadGroupSizeX = 64;

	FRecomputeTangentsPerTrianglePassCS() {}

public:
	FShaderResourceParameter IntermediateAccumBufferUAV;
	FShaderParameter UnitCount;
	FShaderResourceParameter GPUSkinCacheBuffer;
	FShaderParameter GPUSkinCacheFloatOffset;
	FShaderResourceParameter IndexBuffer;
	FShaderParameter IndexBufferOffset;
	FShaderParameter SkinInputStreamFloatOffset;
	FShaderParameter SkinInputStreamStride;
	FShaderResourceParameter SkinInputStream;

	FRecomputeTangentsPerTrianglePassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		UnitCount.Bind(Initializer.ParameterMap, TEXT("UnitCount"));
		GPUSkinCacheBuffer.Bind(Initializer.ParameterMap,TEXT("GPUSkinCacheBuffer"));
		GPUSkinCacheFloatOffset.Bind(Initializer.ParameterMap,TEXT("GPUSkinCacheFloatOffset"));
		IndexBuffer.Bind(Initializer.ParameterMap,TEXT("IndexBuffer"));
		IndexBufferOffset.Bind(Initializer.ParameterMap,TEXT("IndexBufferOffset"));

		SkinInputStreamFloatOffset.Bind(Initializer.ParameterMap, TEXT("SkinCacheInputStreamFloatOffset"));
		SkinInputStreamStride.Bind(Initializer.ParameterMap, TEXT("SkinCacheVertexStride"));
		SkinInputStream.Bind(Initializer.ParameterMap, TEXT("SkinStreamInputBuffer"));
	}

	void SetParameters(const FGPUSkinCache::FDispatchData& DispatchData, FRWBuffer& IntermediateAccumBuffer)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FRHICommandListImmediate& RHICmdList = DispatchData.RHICmdList;

		check(DispatchData.SkinCacheBuffer);

//later		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, UnitCount, DispatchData.NumTriangles);

		SetSRVParameter(RHICmdList, ShaderRHI, GPUSkinCacheBuffer, DispatchData.SkinCacheBuffer->SRV);
		SetShaderValue(RHICmdList, ShaderRHI, GPUSkinCacheFloatOffset, DispatchData.OutputBufferFloatOffset);

		SetSRVParameter(RHICmdList, ShaderRHI, IndexBuffer, DispatchData.IndexBuffer);
		SetShaderValue(RHICmdList, ShaderRHI, IndexBufferOffset, DispatchData.IndexBufferOffsetValue);
		
		SetShaderValue(RHICmdList, ShaderRHI, SkinInputStreamFloatOffset, DispatchData.InputStreamFloatOffset);
		SetShaderValue(RHICmdList, ShaderRHI, SkinInputStreamStride, DispatchData.InputStreamStride);
 		SetSRVParameter(RHICmdList, ShaderRHI, SkinInputStream, DispatchData.InputVertexBufferSRV);

		// UAV
		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, IntermediateAccumBuffer.UAV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, 0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IntermediateAccumBufferUAV << UnitCount << GPUSkinCacheBuffer << GPUSkinCacheFloatOffset << IndexBuffer << IndexBufferOffset
			<< SkinInputStreamFloatOffset << SkinInputStreamStride << SkinInputStream;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("RecomputeTangentsPerTrianglePass");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainCS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FRecomputeTangentsPerTrianglePassCS<A> FRecomputeTangentsPerTrianglePassCS##A; \
	IMPLEMENT_SHADER_TYPE2(FRecomputeTangentsPerTrianglePassCS##A, SF_Compute);
	VARIATION1(0) VARIATION1(1)
#undef VARIATION1

/** Encapsulates the RecomputeSkinTangentsResolve compute shader. */
class FRecomputeTangentsPerVertexPassCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRecomputeTangentsPerVertexPassCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_X"), FGPUSkinCache::RWTangentXOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_Z"), FGPUSkinCache::RWTangentZOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_NUM_FLOATS"), FGPUSkinCache::RWStrideInFloats);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
	}

	static const uint32 ThreadGroupSizeX = 64;

	FRecomputeTangentsPerVertexPassCS() {}

public:
	FShaderResourceParameter IntermediateAccumBufferUAV;
	FShaderResourceParameter GPUSkinCacheBufferUAV;
	FShaderParameter GPUSkinCacheFloatOffset;
	FShaderParameter UnitCount;
	FShaderParameter SkinInputStreamFloatOffset;

	FRecomputeTangentsPerVertexPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		GPUSkinCacheBufferUAV.Bind(Initializer.ParameterMap,TEXT("GPUSkinCacheBufferUAV"));
		GPUSkinCacheFloatOffset.Bind(Initializer.ParameterMap,TEXT("GPUSkinCacheFloatOffset"));
		UnitCount.Bind(Initializer.ParameterMap, TEXT("UnitCount"));
		SkinInputStreamFloatOffset.Bind(Initializer.ParameterMap, TEXT("SkinCacheInputStreamFloatOffset"));
	}

	void SetParameters(const FGPUSkinCache::FDispatchData& DispatchData, FRWBuffer& IntermediateAccumBuffer)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FRHICommandListImmediate& RHICmdList = DispatchData.RHICmdList;
		
		check(DispatchData.SkinCacheBuffer);

//later		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, GPUSkinCacheFloatOffset, DispatchData.OutputBufferFloatOffset);
		SetShaderValue(RHICmdList, ShaderRHI, UnitCount, DispatchData.NumVertices);
		SetShaderValue(RHICmdList, ShaderRHI, SkinInputStreamFloatOffset, DispatchData.InputStreamFloatOffset);

		// UAVs
		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, IntermediateAccumBuffer.UAV);
		SetUAVParameter(RHICmdList, ShaderRHI, GPUSkinCacheBufferUAV, DispatchData.SkinCacheBuffer->UAV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetUAVParameter(RHICmdList, ShaderRHI, GPUSkinCacheBufferUAV, 0);
		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, 0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IntermediateAccumBufferUAV << GPUSkinCacheBufferUAV << GPUSkinCacheFloatOffset << UnitCount << SkinInputStreamFloatOffset;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FRecomputeTangentsPerVertexPassCS,TEXT("RecomputeTangentsPerVertexPass"),TEXT("MainCS"),SF_Compute);

void FGPUSkinCache::DispatchUpdateSkinTangents(const FDispatchData& DispatchData)
{
	FRHICommandListImmediate& RHICmdList = DispatchData.RHICmdList;

	// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
	check(DispatchData.FeatureLevel == ERHIFeatureLevel::SM5);

	//	if( IsValidRef(MorphVertexBuffer.VertexBufferRHI) )
	{
		// no need to clear the intermediate buffer becasue we create it cleared and clear it after each usage in the per vertex pass

		FSkeletalMeshResource& SkeletalMeshResource = DispatchData.GPUSkin->GetSkeletalMeshResource();
		int32 LODIndex = DispatchData.GPUSkin->GetLOD();
		FStaticLODModel& LodModel = SkeletalMeshResource.LODModels[LODIndex];

		SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());

		FRawStaticIndexBuffer16or32Interface* IndexBuffer = LodModel.MultiSizeIndexContainer.GetIndexBuffer();
		FIndexBufferRHIParamRef IndexBufferRHI = IndexBuffer->IndexBufferRHI;

		const uint32 RequiredVertexCount = LodModel.NumVertices;

		uint32 MaxVertexCount = ComputeRecomputeTangentMaxVertexCount();

		// We could do automatically reallocate but that is not yet supported
		ensure(RequiredVertexCount < MaxVertexCount);

		// todo
//		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToCompute, IntermediateAccumBuffer.UAV);

		// This code can be optimized by batched up and doing it with less Dispatch calls (costs more memory)
		{
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<0> > ComputeShader0(GetGlobalShaderMap(DispatchData.FeatureLevel));
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<1> > ComputeShader1(GetGlobalShaderMap(DispatchData.FeatureLevel));
			
			bool bFullPrecisionUV = LodModel.VertexBufferGPUSkin.GetUseFullPrecisionUVs();
			uint32 NumTriangles = DispatchData.NumTriangles;
			uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(NumTriangles, FRecomputeTangentsPerTrianglePassCS<0>::ThreadGroupSizeX);

			SCOPED_DRAW_EVENTF(RHICmdList, SkinTangents_PerTrianglePass, TEXT("SkinTangents_PerTrianglePass IndexStart=%d Tri=%d UVPrecision=%d"),
				DispatchData.IndexBufferOffsetValue, DispatchData.NumTriangles, bFullPrecisionUV);

			if(bFullPrecisionUV)
			{
				const FComputeShaderRHIParamRef ShaderRHI = ComputeShader1->GetComputeShader();
				RHICmdList.SetComputeShader(ShaderRHI);

				ComputeShader1->SetParameters(DispatchData, SkinTangentIntermediate);
				DispatchComputeShader(RHICmdList, *ComputeShader1, ThreadGroupCountValue, 1, 1);
				ComputeShader1->UnsetParameters(RHICmdList);
			}
			else
			{
				FGlobalShader* ComputeShader = bFullPrecisionUV ? (FGlobalShader*)*ComputeShader1 : (FGlobalShader*)*ComputeShader0;
				const FComputeShaderRHIParamRef ShaderRHI = ComputeShader0->GetComputeShader();
				RHICmdList.SetComputeShader(ShaderRHI);

				ComputeShader0->SetParameters(DispatchData, SkinTangentIntermediate);
				DispatchComputeShader(RHICmdList, *ComputeShader0, ThreadGroupCountValue, 1, 1);
				ComputeShader0->UnsetParameters(RHICmdList);
			}
		}

		{
			SCOPED_DRAW_EVENTF(RHICmdList, SkinTangents_PerVertexPass, TEXT("SkinTangents_PerVertexPass InputVertStart=%d, SkinCacheStart=%d, Vert=%d"),
				DispatchData.InputStreamFloatOffset, DispatchData.OutputBufferFloatOffset, DispatchData.NumVertices);

			TShaderMapRef<FRecomputeTangentsPerVertexPassCS> ComputeShader(GetGlobalShaderMap(DispatchData.FeatureLevel));
			const FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

			uint32 VertexCount = DispatchData.NumVertices;
			uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(VertexCount, FRecomputeTangentsPerVertexPassCS::ThreadGroupSizeX);

			ComputeShader->SetParameters(DispatchData, SkinTangentIntermediate);
	
			DispatchComputeShader(RHICmdList, *ComputeShader, ThreadGroupCountValue, 1, 1);

			ComputeShader->UnsetParameters(RHICmdList);
		}
// todo				RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, TangentsBlendBuffer.VertexBufferSRV);
//			ensureMsgf(DestRenderTarget.TargetableTexture == DestRenderTarget.ShaderResourceTexture, TEXT("%s should be resolved to a separate SRV"), *DestRenderTarget.TargetableTexture->GetName().ToString());	
	}
}

void FGPUSkinCache::CVarSinkFunction()
{
	// 0/1
	int32 NewGPUSkinCacheValue = CVarEnableGPUSkinCache.GetValueOnAnyThread() != 0;

	if (!GEnableGPUSkinCacheShaders)
	{
		NewGPUSkinCacheValue = 0;
	}

	if (NewGPUSkinCacheValue != GEnableGPUSkinCache)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(DoEnableSkinCaching, 
			int32, SkinValue, NewGPUSkinCacheValue,
		{
			GEnableGPUSkinCache = SkinValue;
			if (SkinValue)
			{
				GGPUSkinCache.TransitionAllToWriteable(RHICmdList);
			}
		});
	}
}

FAutoConsoleVariableSink FGPUSkinCache::CVarSink(FConsoleCommandDelegate::CreateStatic(&CVarSinkFunction));
