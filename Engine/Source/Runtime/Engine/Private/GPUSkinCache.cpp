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
DEFINE_STAT(STAT_GPUSkinCache_TotalMemWasted);
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
	TEXT("r.SkinCache.CompileShaders"),
	GEnableGPUSkinCacheShaders,
	TEXT("Whether or not to compile the GPU compute skinning cache shaders.\n")
	TEXT("This will compile the shaders for skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("GPUSkinVertexFactory.usf needs to be touched to cause a recompile if this changes.\n")
	TEXT("0 is off(default), 1 is on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

int32 GSkinCacheSafety = 1;
static FAutoConsoleVariableRef CVarGSkinCacheSafety(
	TEXT("r.SkinCache.Safety"),
	GSkinCacheSafety,
	TEXT("To supress ensure() and check() when running on stage\n")
	TEXT(" 0: disable ensure() and check()\n")
	TEXT(" 1: enable ensure() and check() (default)"),
	ECVF_RenderThreadSafe
	);

// 0/1
int32 GEnableGPUSkinCache = 0;
static TAutoConsoleVariable<int32> CVarEnableGPUSkinCache(
	TEXT("r.SkinCache.Mode"),
	0,
	TEXT("Whether or not to use the GPU compute skinning cache.\n")
	TEXT("This will perform skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("Requires r.SkinCache.CompileShaders=1\n")
	TEXT(" 0: off(default)\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe
	);

TAutoConsoleVariable<int32> CVarGPUSkinCacheRecomputeTangents(
	TEXT("r.SkinCache.RecomputeTangents"),
	2,
	TEXT("If r.SkinCache.Mode is enabled this option recomputes the vertex tangents on the GPU\n")
	TEXT("Can be changed at runtime, Requires r.SkinCache.CompileShaders=1 and r.SkinCache.Mode=1\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on\n")
	TEXT(" 2: on, SkinCache only objects that would require the RecomputTangents feature (default)"),
	ECVF_RenderThreadSafe
	);

static int32 GMaxGPUSkinCacheElementsPerFrame = 1000;
static FAutoConsoleVariableRef CVarMaxGPUSkinCacheElementsPerFrame(
	TEXT("r.SkinCache.MaxGPUElementsPerFrame"),
	GMaxGPUSkinCacheElementsPerFrame,
	TEXT("The maximum compute processed skin cache elements per frame. Only used with r.SkinCache.Mode=1\n")
	TEXT(" (default is 1000)")
	);

static int32 GGPUSkinCacheBufferSize = 99 * 1024 * 1024;
static FAutoConsoleVariableRef CVarMaxGPUSkinCacheBufferSize(
	TEXT("r.SkinCache.BufferSize"),
	GGPUSkinCacheBufferSize,
	TEXT("The maximum memory used for writing out the vertices in bytes\n")
	TEXT("Split into GPUSKINCACHE_FRAMES chunks (eg 300 MB means 100 MB for 3 frames)\n")
	TEXT("Default is 99MB.  Only used with r.SkinCache.Mode=1\n"),
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

ENGINE_API bool DoRecomputeSkinTangentsOnGPU_RT()
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

		InputStreamStride.Bind(Initializer.ParameterMap, TEXT("InputStreamStride"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		SkinCacheStart.Bind(Initializer.ParameterMap, TEXT("SkinCacheStart"));
		BoneMatrices.Bind(Initializer.ParameterMap, TEXT("BoneMatrices"));
		SkinInputStream.Bind(Initializer.ParameterMap, TEXT("SkinStreamInputBuffer"));
		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		
		SkinCacheBufferUAV.Bind(Initializer.ParameterMap, TEXT("SkinCacheBufferUAV"));

		MorphBuffer.Bind(Initializer.ParameterMap, TEXT("MorphBuffer"));
		MorphBufferOffset.Bind(Initializer.ParameterMap, TEXT("MorphBufferOffset"));
		SkinCacheDebug.Bind(Initializer.ParameterMap, TEXT("SkinCacheDebug"));
	}

	void SetParameters(const FVertexBufferAndSRV& BoneBuffer, FUniformBufferRHIRef UniformBuffer,
		const FVector& MeshOrigin, const FVector& MeshExtension,
		const FGPUSkinCache::FDispatchData& DispatchData)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FRHICommandListImmediate& RHICmdList = DispatchData.RHICmdList;

		SetShaderValue(RHICmdList, ShaderRHI, SkinMeshOriginParameter, MeshOrigin);
		SetShaderValue(RHICmdList, ShaderRHI, SkinMeshExtensionParameter, MeshExtension);

		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStride, DispatchData.InputStreamStride);
		SetShaderValue(RHICmdList, ShaderRHI, NumVertices, DispatchData.NumVertices);
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStart, DispatchData.InputStreamStart);
		//SetShaderValue(RHICmdList, ShaderRHI, DebugParameter, FUintVector4((uint32)(uint64)VertexBufferSRV.GetReference(), 0, 0, 1));

		if (UniformBuffer)
		{
			SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<GPUSkinCacheBonesUniformShaderParameters>(), UniformBuffer);
		}
		else
		{
			check(!GSkinCacheSafety || BoneBuffer.VertexBufferSRV);
			SetSRVParameter(RHICmdList, ShaderRHI, BoneMatrices, BoneBuffer.VertexBufferSRV);
		}

		SetSRVParameter(RHICmdList, ShaderRHI, SkinInputStream, DispatchData.InputVertexBufferSRV);

		// output UAV
		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBufferUAV, DispatchData.SkinCacheBuffer->UAV);
		SetShaderValue(RHICmdList, ShaderRHI, SkinCacheStart, DispatchData.SkinCacheStart);
	
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
		
		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBufferUAV, 0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << SkinMeshOriginParameter << SkinMeshExtensionParameter << InputStreamStride
			<< NumVertices << InputStreamStart << SkinCacheStart
			<< SkinInputStream << SkinCacheBufferUAV << BoneMatrices << MorphBuffer << MorphBufferOffset
			<< SkinCacheDebug;
		//Ar << DebugParameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter SkinMeshOriginParameter;
	FShaderParameter SkinMeshExtensionParameter;
	
	FShaderParameter InputStreamStride;
	FShaderParameter NumVertices;
	FShaderParameter SkinCacheDebug;
	FShaderParameter InputStreamStart;
	FShaderParameter SkinCacheStart;

	//FShaderParameter DebugParameter;

	FShaderUniformBufferParameter SkinUniformBuffer;

	FShaderResourceParameter BoneMatrices;
	FShaderResourceParameter SkinInputStream;
	FShaderResourceParameter SkinCacheBufferUAV;

	FShaderResourceParameter MorphBuffer;
	FShaderParameter MorphBufferOffset;
};

/** Compute shader that skins a batch of vertices. */
// @param SkinType 0:normal, 1:with morph targets calculated outside the cache, 2:with morph target calculated insde the cache (not yet implemented), 3:with APEX cloth (not yet implemented)
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
		OutEnvironment.SetDefine(TEXT("GPUSKIN_MORPH_BLEND"), SkinType);
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
	, SkinCacheFrameNumber(0)
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

	checkf(GEnableGPUSkinCacheShaders, TEXT("GPU Skin caching requires the shaders enabled (r.SkinCache.CompileShaders=1)"));

	int32 NumFloatsPerBuffer = GGPUSkinCacheBufferSize / GPUSKINCACHE_FRAMES / sizeof(float);
	for (int32 Index = 0; Index < GPUSKINCACHE_FRAMES; ++Index)
	{
		SkinCacheBuffer[Index].Initialize(sizeof(float), NumFloatsPerBuffer, PF_R32_FLOAT, BUF_Static);

		FString Name = FString::Printf(TEXT("SkinCacheRWBuffer%d"), Index);
		RHICmdList.BindDebugLabelName(SkinCacheBuffer[Index].UAV, *Name);
	}

	UE_LOG(LogSkinCache, Log, TEXT("  Frames:            %d"), GPUSKINCACHE_FRAMES);
	UE_LOG(LogSkinCache, Log, TEXT("  VertexBuffers:     %.1f MB"), FMath::DivideAndRoundUp((uint32)(NumFloatsPerBuffer * GPUSKINCACHE_FRAMES * sizeof(float)), (uint32)1024) / 1024.0f);

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
int32 FGPUSkinCache::StartCacheMesh(FRHICommandListImmediate& RHICmdList, uint32 FrameNumber, int32 Key, FGPUBaseSkinVertexFactory* VertexFactory,
	FGPUSkinPassthroughVertexFactory* TargetVertexFactory, const FSkelMeshSection& BatchElement, FSkeletalMeshObjectGPUSkin* Skin,
	const FMorphVertexBuffer* MorphVertexBuffer)
{
	if (CachedChunksThisFrameCount >= GMaxGPUSkinCacheElementsPerFrame && FrameNumber <= SkinCacheFrameNumber)
	{
		INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForChunksPerFrame);
		return -1;
	}

	if (!bInitialized)
	{
		Initialize(RHICmdList);
	}

	if (FrameNumber > SkinCacheFrameNumber)	// Reset cache output if on a new frame
	{
		CacheCurrentFloatOffset = 0;
		CachedChunksThisFrameCount = 0;
		SkinCacheFrameNumber = FrameNumber;
	}

	FElementCacheStatusInfo::FSearchInfo SInfo;
	SInfo.BatchElement = &BatchElement;
	SInfo.Skin = Skin;

	FElementCacheStatusInfo* ECSInfo = nullptr;

	if (Key >= 0 && Key < CachedElements.Num())
	{
		ECSInfo = &(CachedElements.GetData()[Key]);

		if (ECSInfo->Skin == Skin && &BatchElement == ECSInfo->BatchElement)
		{
			// that however should not happen:
			ensure(!GSkinCacheSafety || ECSInfo->NumVertices == BatchElement.GetNumVertices());
			// that also should not happen:
			ensureMsgf(!GSkinCacheSafety || ECSInfo->BaseVertexIndex == BatchElement.BaseVertexIndex, TEXT("SkinCache internal error (Debug: %p %d %d %d %d)"),
				&BatchElement,
				ECSInfo->NumVertices, BatchElement.GetNumVertices(),
				ECSInfo->BaseVertexIndex, BatchElement.BaseVertexIndex);
		}
		else
		{
			ECSInfo = CachedElements.FindByKey(SInfo);
		}
	}
	else
	{
		ECSInfo = CachedElements.FindByKey(SInfo);
	}

	int32 UAVIndex = SkinCacheFrameNumber % GPUSKINCACHE_FRAMES;
	bool AlignStreamOffsetsFrames = false;
	const bool bExtraBoneInfluences = VertexFactory->UsesExtraBoneInfluences();
	bool bHasPreviousOffset = false;
	if (ECSInfo)
	{
		if (ECSInfo->FrameUpdated == FrameNumber)
		{
			// Already cached this element this frame
			INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForAlreadyCached);
			// This line is needed when dragging in a specific skeletal mesh which uses APEX cloth (the PassThroughVertexFactory get recreated but not the data
			// we store in it, also FrameNumber seems to not be increased, maybe we should fix that).
			// //UE4/Private-GDC16-Character/Samples/NotForLicensees/GDC2016/CharacterDemo/Content/Developers/matthewstoneham/APEX/
			TargetVertexFactory->UpdateVertexDeclaration(VertexFactory, &SkinCacheBuffer[UAVIndex]);
			return ECSInfo->Key;
		}

		// Seems needed because we bump the frame number in each rendering but then 2 is not always enough
		// TODO: Fix by using per scene/world frame number or store previsou frame number in scene/world
		AlignStreamOffsetsFrames = (FrameNumber - ECSInfo->FrameUpdated) > 2;

		ECSInfo->PreviousFrameStreamOffset = ECSInfo->StreamOffset;
		ECSInfo->PreviousFrameUpdated = ECSInfo->FrameUpdated;
		bHasPreviousOffset = true;
	}
	else	// New element, init and add
	{
		if (CachedElements.Num() < MaxCachedElements)
		{
			FElementCacheStatusInfo	Info;
			Info.Key = CachedElements.Num();
			CachedElements.Add(Info);

			ECSInfo = &(CachedElements.GetData()[Info.Key]);
		}
		else
		{
			ECSInfo = FindEvictableCacheStatusInfo(FrameNumber);

			if (ECSInfo == nullptr)
			{
				INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForOutOfECS);
				return -1;
			}
		}
		ECSInfo->BatchElement = &BatchElement;
		ECSInfo->Skin = Skin;

		AlignStreamOffsetsFrames = true;
	}

	uint32	StreamStrides[MaxVertexElementCount];
	uint32	StreamStrideCount = VertexFactory->GetStreamStrides(StreamStrides);
	uint32	NumVertices = BatchElement.GetNumVertices();
	uint32 InputStreamStart = (StreamStrides[0] * BatchElement.BaseVertexIndex) / sizeof(float);
	uint32	NumRWFloats = (RWStrideInFloats * NumVertices);

	// This is needed because the input index buffer are not 0 based for each chunk but 0 based for the whole mesh (better for combing draw calls).
	// We would have to specify a negative BaseVertexIndex on the draw call and that is undefined (might work on some platforms)
	// Doing this wastes memory and less elements might fit into the cache. By ordering the elements differently we could reduce the waste (content side or when inserting into the cache).
	// in bytes
	int32 CompensatedOffset = ((int32)CacheCurrentFloatOffset * sizeof(float)) - (BatchElement.BaseVertexIndex * RWStrideInFloats * sizeof(float));
	if (CompensatedOffset < 0)
	{
		CacheCurrentFloatOffset -= CompensatedOffset / sizeof(float);
		INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalMemWasted, -CompensatedOffset);
	}

	if (CacheCurrentFloatOffset + NumRWFloats > (uint32)GGPUSkinCacheBufferSize)
	{
		// Can't fit this
		INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForMemory);
		return -1;
	}

	INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalMemUsed, NumRWFloats * sizeof(float));

	// value section -------------
	
	ECSInfo->FrameUpdated = FrameNumber;
	ECSInfo->StreamOffset = CacheCurrentFloatOffset;

	if (AlignStreamOffsetsFrames || !bHasPreviousOffset)
	{
		ECSInfo->PreviousFrameStreamOffset = ECSInfo->StreamOffset;
		ECSInfo->PreviousFrameUpdated = ECSInfo->FrameUpdated;
	}

	// debugging section -------------
	
	ECSInfo->NumVertices = NumVertices;
	ECSInfo->BaseVertexIndex = BatchElement.BaseVertexIndex;
	ECSInfo->InputVBStride = StreamStrides[0];
	ECSInfo->bExtraBoneInfluences = bExtraBoneInfluences;
	ECSInfo->VertexFactory = VertexFactory;
	ECSInfo->TargetVertexFactory = TargetVertexFactory;

	if (AlignStreamOffsetsFrames || !bHasPreviousOffset)
	{
		ECSInfo->PreviousFrameStreamOffset = ECSInfo->StreamOffset;
	}

	const FSkeletalMeshVertexBuffer* SkeletalMeshVertexBuffer = VertexFactory->GetSkinVertexBuffer();

	FDispatchData DispatchData(RHICmdList, BatchElement, Skin->GetFeatureLevel(), Skin);

	// could be removed for SHIPPING
	{
		FSkeletalMeshResource& SkeletalMeshResource = Skin->GetSkeletalMeshResource();
		int32 LODIndex = Skin->GetLOD();
		FStaticLODModel& LodModel = SkeletalMeshResource.LODModels[LODIndex];
		DispatchData.SectionIdx = LodModel.FindSectionIndex(BatchElement);
	}

	DispatchData.NumVertices = NumVertices;

	if(MorphVertexBuffer)
	{
		DispatchData.MorphBuffer = MorphVertexBuffer->GetSRV();
		check(!GSkinCacheSafety || DispatchData.MorphBuffer);

		// in bytes
		const uint32 MorphStride = sizeof(FMorphGPUSkinVertex);

		// see GPU code "check(MorphStride == sizeof(float) * 6);"
		check(!GSkinCacheSafety || MorphStride == sizeof(float) * 6);

		DispatchData.MorphBufferOffset = (MorphStride * BatchElement.BaseVertexIndex) / sizeof(float);
	}

	INC_DWORD_STAT(STAT_GPUSkinCache_TotalNumChunks);
	INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, NumVertices);

	auto& ShaderData = VertexFactory->GetShaderData();
	
	// SkinType 0:normal, 1:with morph target, 2:with APEX cloth (not yet implemented)
	DispatchData.SkinType = MorphVertexBuffer ? 1 : 0;
	DispatchData.InputStreamStart = InputStreamStart;
	DispatchData.SkinCacheStart = ECSInfo->StreamOffset;

	DispatchData.SkinCacheBuffer = &SkinCacheBuffer[UAVIndex];
	DispatchData.InputStreamStride = StreamStrides[0];
	DispatchData.InputVertexBufferSRV = SkeletalMeshVertexBuffer->GetSRV();
	DispatchData.bExtraBoneInfluences = bExtraBoneInfluences;
	check(!GSkinCacheSafety || DispatchData.InputVertexBufferSRV);

	DispatchSkinCacheProcess(
		ShaderData.GetBoneBufferForReading(false, FrameNumber), ShaderData.GetUniformBuffer(),
		ShaderData.MeshOrigin, ShaderData.MeshExtension,
		DispatchData);
	
	TargetVertexFactory->UpdateVertexDeclaration(VertexFactory, DispatchData.SkinCacheBuffer);

	const bool bRecomputeTangents = MorphVertexBuffer && CVarGPUSkinCacheRecomputeTangents.GetValueOnRenderThread() != 0;

	if(bRecomputeTangents)
	{
		if(BatchElement.bRecomputeTangent)
		{
			FStaticLODModel* StaticLODModel = MorphVertexBuffer->GetStaticLODModel();
			FRawStaticIndexBuffer16or32Interface* IndexBuffer = StaticLODModel->MultiSizeIndexContainer.GetIndexBuffer();

			DispatchData.IndexBuffer = IndexBuffer->GetSRV();
			check(!GSkinCacheSafety || DispatchData.IndexBuffer);
			DispatchData.NumTriangles = BatchElement.NumTriangles;
			DispatchData.IndexBufferOffsetValue = BatchElement.BaseIndex;
			DispatchUpdateSkinTangents(DispatchData);
		}
	}

	CacheCurrentFloatOffset += NumRWFloats;
	CachedChunksThisFrameCount++;

	return ECSInfo->Key;
}

void FGPUSkinCache::TransitionToReadable(FRHICommandList& RHICmdList)
{
	int32 BufferIndex = SkinCacheFrameNumber % GPUSKINCACHE_FRAMES;
	FUnorderedAccessViewRHIParamRef OutUAVs[] ={SkinCacheBuffer[BufferIndex].UAV};
	RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutUAVs, ARRAY_COUNT(OutUAVs));
}

void FGPUSkinCache::TransitionToWriteable(FRHICommandList& RHICmdList)
{
	int32 BufferIndex = (SkinCacheFrameNumber + GPUSKINCACHE_FRAMES - 1) % GPUSKINCACHE_FRAMES;
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

bool FGPUSkinCache::InternalIsElementProcessed(uint32 FrameNumber, int32 Key) const
{
	// Happens, is this a problem?
//	ensure(FrameNumber == SkinCacheFrameNumber);

	if (Key >= 0 && Key < CachedElements.Num())
	{
		const FElementCacheStatusInfo& CacheInfo = CachedElements.GetData()[Key];

		if (CacheInfo.FrameUpdated == FrameNumber)
		{
			return true;
		}
	}

	return false;
}

void FGPUSkinCache::InternalSetVertexStreamFromCache(FRHICommandList& RHICmdList, uint32 FrameNumber, int32 Key, FShader* Shader, const FGPUSkinPassthroughVertexFactory* VertexFactory,
	uint32 BaseVertexIndex, FShaderParameter PreviousStreamFloatOffset, FShaderResourceParameter PreviousStreamBuffer)
{
	const FElementCacheStatusInfo& CacheInfo = CachedElements.GetData()[Key];

	// Verify that we found the correct chunk and that it was processed this frame

	// happened, is that ok?: http://crashreporter/Buggs/Show/149552
//	ensure(CacheInfo.BatchElement->BaseVertexIndex == BaseVertexIndex);
	// this assumes during StartCacheMesh() we have the same BaseVertexIndex, see ensure there
	ensureMsgf(!GSkinCacheSafety || CacheInfo.BaseVertexIndex == BaseVertexIndex, TEXT("SkinCache internal error (Debug: %p %d %d %d %d cloth:%d)"),
		CacheInfo.BatchElement,
		CacheInfo.NumVertices, CacheInfo.BatchElement->GetNumVertices(),
		CacheInfo.BaseVertexIndex, BaseVertexIndex,
		CacheInfo.BatchElement->HasApexClothData());

	ensure(!GSkinCacheSafety || FrameNumber == SkinCacheFrameNumber);
	ensure(!GSkinCacheSafety || CacheInfo.BatchElement->GetNumVertices() == CacheInfo.NumVertices);

	{
		// Compensated offset since the StreamOffset is relative to the RWBuffer start, while the SetStreamSource one is relative to the BaseVertexIndex
		int32 CompensatedOffset = ((int32)CacheInfo.StreamOffset * sizeof(float)) - (CacheInfo.BatchElement->BaseVertexIndex * RWStrideInFloats * sizeof(float));
		if (CompensatedOffset < 0)
		{
			ensure(!GSkinCacheSafety);
			// should never happen but better we render garbage than crash
			return;
		}

		int32 UAVIndex = SkinCacheFrameNumber % GPUSKINCACHE_FRAMES;
		RHICmdList.SetStreamSource(VertexFactory->GetStreamIndex(UAVIndex), SkinCacheBuffer[UAVIndex].Buffer, RWStrideInFloats * sizeof(float), CompensatedOffset);
	}

	FVertexShaderRHIParamRef ShaderRHI = Shader->GetVertexShader();
	if (ShaderRHI && PreviousStreamBuffer.IsBound())
	{
		// Compensated offset since in the shader Shader we use VertexId which is offset by BaseVertexIndex
		int32 PreviousCompensatedOffset = ((int32)CacheInfo.PreviousFrameStreamOffset * sizeof(float)) - (CacheInfo.BatchElement->BaseVertexIndex * RWStrideInFloats * sizeof(float));
		if (PreviousCompensatedOffset < 0)
		{
			ensure(!GSkinCacheSafety);
			// should never happen but better we render garbage than crash
			return;
		}

		// Before this was assuming the previous frame is SkinCacheFrameNumber-1 but that is wrong when an object was just added (strong single frame motionblur artifact)
		int32 PrevUAVIndex = (CacheInfo.PreviousFrameUpdated) % GPUSKINCACHE_FRAMES;
		SetShaderValue(RHICmdList, ShaderRHI, PreviousStreamFloatOffset, PreviousCompensatedOffset / sizeof(float));
		RHICmdList.SetShaderResourceViewParameter(ShaderRHI, PreviousStreamBuffer.GetBaseIndex(), SkinCacheBuffer[PrevUAVIndex].SRV);
	}
}

FGPUSkinCache::FElementCacheStatusInfo* FGPUSkinCache::FindEvictableCacheStatusInfo(uint32 FrameNumber)
{
	if (!CachedElements.Num())
	{
		return nullptr;
	}

	uint32	BestFrameCounter = 0x7fffffff;
	FElementCacheStatusInfo* BestEntry = nullptr;

	uint32 TargetFrameCounter = (FrameNumber >= 10) ? (FrameNumber - 10) : 0;

	FElementCacheStatusInfo* CSInfo = CachedElements.GetData();

	// TODO: This could be speed up a lot by using a free list. We should measure CPU cost before and after.

	// O(n)
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

	if (BestEntry && BestFrameCounter < FrameNumber)
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
	const FVector& MeshOrigin, const FVector& MeshExtension,
	const FDispatchData& DispatchData)
{
	check(DispatchData.FeatureLevel >= ERHIFeatureLevel::SM5);

	SCOPED_DRAW_EVENTF(DispatchData.RHICmdList, SkinCacheDispatch,
		TEXT("SkinCacheDispatch<%d,%d> Chunk=%d InputStreamStart=%d SkinCacheStart=%d Vert=%d Morph=%d/%d StrideInFloats(In/Out):%d/%d"),
		(int32)DispatchData.bExtraBoneInfluences, DispatchData.SkinType,
		DispatchData.SectionIdx, DispatchData.InputStreamStart, DispatchData.SkinCacheStart, DispatchData.NumVertices, DispatchData.MorphBuffer != 0, DispatchData.MorphBufferOffset,
		DispatchData.InputStreamStride / sizeof(float), FGPUSkinCache::RWStrideInFloats);

	TShaderMapRef<TGPUSkinCacheCS<true,0> > SkinCacheCS10(GetGlobalShaderMap(DispatchData.FeatureLevel));
	TShaderMapRef<TGPUSkinCacheCS<false,0> > SkinCacheCS00(GetGlobalShaderMap(DispatchData.FeatureLevel));
	TShaderMapRef<TGPUSkinCacheCS<true,1> > SkinCacheCS11(GetGlobalShaderMap(DispatchData.FeatureLevel));
	TShaderMapRef<TGPUSkinCacheCS<false,1> > SkinCacheCS01(GetGlobalShaderMap(DispatchData.FeatureLevel));

	FBaseGPUSkinCacheCS* Shader = 0;
	
	switch(DispatchData.SkinType)
	{
		case 0: Shader = DispatchData.bExtraBoneInfluences ? (FBaseGPUSkinCacheCS*)*SkinCacheCS10 : (FBaseGPUSkinCacheCS*)*SkinCacheCS00;
			break;
		case 1: Shader = DispatchData.bExtraBoneInfluences ? (FBaseGPUSkinCacheCS*)*SkinCacheCS11 : (FBaseGPUSkinCacheCS*)*SkinCacheCS01;
			break;
		default:
			check(0);
	}
	
	check(Shader);
	DispatchData.RHICmdList.SetComputeShader(Shader->GetComputeShader());
	Shader->SetParameters(BoneBuffer, UniformBuffer,MeshOrigin, MeshExtension, DispatchData);

	uint32 VertexCountAlign64 = FMath::DivideAndRoundUp(DispatchData.NumVertices, (uint32)64);
	DispatchData.RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);
	Shader->UnsetParameters(DispatchData.RHICmdList);
}

/** base of the FRecomputeTangentsPerTrianglePassCS class */
class FBaseRecomputeTangents : public FGlobalShader
{
public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return Platform == SP_PCD3D_SM5;
	}

	static const uint32 ThreadGroupSizeX = 64;

	FShaderResourceParameter IntermediateAccumBufferUAV;
	FShaderParameter NumTriangles;
	FShaderResourceParameter GPUSkinCacheBuffer;
	FShaderParameter SkinCacheStart;
	FShaderResourceParameter IndexBuffer;
	FShaderParameter IndexBufferOffset;
	FShaderParameter InputStreamStart;
	FShaderParameter InputStreamStride;
	FShaderResourceParameter SkinInputStream;

	FBaseRecomputeTangents()
	{}

	FBaseRecomputeTangents(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		NumTriangles.Bind(Initializer.ParameterMap, TEXT("NumTriangles"));
		GPUSkinCacheBuffer.Bind(Initializer.ParameterMap,TEXT("GPUSkinCacheBuffer"));
		SkinCacheStart.Bind(Initializer.ParameterMap,TEXT("SkinCacheStart"));
		IndexBuffer.Bind(Initializer.ParameterMap,TEXT("IndexBuffer"));
		IndexBufferOffset.Bind(Initializer.ParameterMap,TEXT("IndexBufferOffset"));

		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		InputStreamStride.Bind(Initializer.ParameterMap, TEXT("InputStreamStride"));
		SkinInputStream.Bind(Initializer.ParameterMap, TEXT("SkinStreamInputBuffer"));
	}

	void SetParameters(const FGPUSkinCache::FDispatchData& DispatchData, FRWBuffer& IntermediateAccumBuffer)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FRHICommandListImmediate& RHICmdList = DispatchData.RHICmdList;

		check(!GSkinCacheSafety || DispatchData.SkinCacheBuffer);

//later		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, NumTriangles, DispatchData.NumTriangles);

		SetSRVParameter(RHICmdList, ShaderRHI, GPUSkinCacheBuffer, DispatchData.SkinCacheBuffer->SRV);
		SetShaderValue(RHICmdList, ShaderRHI, SkinCacheStart, DispatchData.SkinCacheStart);

		SetSRVParameter(RHICmdList, ShaderRHI, IndexBuffer, DispatchData.IndexBuffer);
		SetShaderValue(RHICmdList, ShaderRHI, IndexBufferOffset, DispatchData.IndexBufferOffsetValue);
		
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStart, DispatchData.InputStreamStart);
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStride, DispatchData.InputStreamStride);
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
		Ar << IntermediateAccumBufferUAV << NumTriangles << GPUSkinCacheBuffer << SkinCacheStart << IndexBuffer << IndexBufferOffset
			<< InputStreamStart << InputStreamStride << SkinInputStream;
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

/** Encapsulates the RecomputeSkinTangents compute shader. */
template <bool bUseExtraBoneInfluencesT, bool bFullPrecisionUV>
class FRecomputeTangentsPerTrianglePassCS : public FBaseRecomputeTangents
{
	DECLARE_SHADER_TYPE(FRecomputeTangentsPerTrianglePassCS, Global);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		const uint32 UseExtraBoneInfluences = bUseExtraBoneInfluencesT;
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), UseExtraBoneInfluences);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
		OutEnvironment.SetDefine(TEXT("FULL_PRECISION_UV"), bFullPrecisionUV);
	}

	FRecomputeTangentsPerTrianglePassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseRecomputeTangents(Initializer)
	{
	}

	FRecomputeTangentsPerTrianglePassCS()
	{}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)
#define VARIATION2(A, B) typedef FRecomputeTangentsPerTrianglePassCS<A, B> FRecomputeTangentsPerTrianglePassCS##A##B; \
	IMPLEMENT_SHADER_TYPE2(FRecomputeTangentsPerTrianglePassCS##A##B, SF_Compute);
	VARIATION1(0) VARIATION1(1)
#undef VARIATION1
#undef VARIATION2

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
		// this pass cannot read the input as it doesn't have the permutation
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), (uint32)0);
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
	FShaderResourceParameter SkinCacheBufferUAV;
	FShaderParameter SkinCacheStart;
	FShaderParameter NumVertices;
	FShaderParameter InputStreamStart;
	FShaderParameter InputStreamStride;

	FRecomputeTangentsPerVertexPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		SkinCacheBufferUAV.Bind(Initializer.ParameterMap,TEXT("SkinCacheBufferUAV"));
		SkinCacheStart.Bind(Initializer.ParameterMap,TEXT("SkinCacheStart"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		InputStreamStride.Bind(Initializer.ParameterMap, TEXT("InputStreamStride"));
	}

	void SetParameters(const FGPUSkinCache::FDispatchData& DispatchData, FRWBuffer& IntermediateAccumBuffer)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FRHICommandListImmediate& RHICmdList = DispatchData.RHICmdList;
		
		check(!GSkinCacheSafety || DispatchData.SkinCacheBuffer);

//later		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, SkinCacheStart, DispatchData.SkinCacheStart);
		SetShaderValue(RHICmdList, ShaderRHI, NumVertices, DispatchData.NumVertices);
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStart, DispatchData.InputStreamStart);
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStride, DispatchData.InputStreamStride);

		// UAVs
		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, IntermediateAccumBuffer.UAV);
		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBufferUAV, DispatchData.SkinCacheBuffer->UAV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBufferUAV, 0);
		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, 0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IntermediateAccumBufferUAV << SkinCacheBufferUAV << SkinCacheStart << NumVertices << InputStreamStart << InputStreamStride;
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
		ensure(!GSkinCacheSafety || RequiredVertexCount < MaxVertexCount);

		// todo
//		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToCompute, IntermediateAccumBuffer.UAV);

		// This code can be optimized by batched up and doing it with less Dispatch calls (costs more memory)
		{
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<0, 0> > ComputeShader00(GetGlobalShaderMap(DispatchData.FeatureLevel));
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<0, 1> > ComputeShader01(GetGlobalShaderMap(DispatchData.FeatureLevel));
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<1, 0> > ComputeShader10(GetGlobalShaderMap(DispatchData.FeatureLevel));
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<1, 1> > ComputeShader11(GetGlobalShaderMap(DispatchData.FeatureLevel));
			
			bool bFullPrecisionUV = LodModel.VertexBufferGPUSkin.GetUseFullPrecisionUVs();

			FBaseRecomputeTangents* Shader = 0;

			switch((uint32)bFullPrecisionUV)
			{
				case 0: Shader = DispatchData.bExtraBoneInfluences ? (FBaseRecomputeTangents*)*ComputeShader10 : (FBaseRecomputeTangents*)*ComputeShader00; break;
				case 1: Shader = DispatchData.bExtraBoneInfluences ? (FBaseRecomputeTangents*)*ComputeShader11 : (FBaseRecomputeTangents*)*ComputeShader01; break;
				default:
					check(0);
			}
	
			check(Shader);

			uint32 NumTriangles = DispatchData.NumTriangles;
			uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(NumTriangles, FBaseRecomputeTangents::ThreadGroupSizeX);

			SCOPED_DRAW_EVENTF(RHICmdList, SkinTangents_PerTrianglePass, TEXT("SkinTangents_PerTrianglePass IndexStart=%d Tri=%d ExtraBoneInfluences=%d UVPrecision=%d"),
				DispatchData.IndexBufferOffsetValue, DispatchData.NumTriangles, DispatchData.bExtraBoneInfluences, bFullPrecisionUV);

			const FComputeShaderRHIParamRef ShaderRHI = Shader->GetComputeShader();
				RHICmdList.SetComputeShader(ShaderRHI);

			Shader->SetParameters(DispatchData, SkinTangentIntermediate);
			DispatchComputeShader(RHICmdList, Shader, ThreadGroupCountValue, 1, 1);
			Shader->UnsetParameters(RHICmdList);
		}

		{
			SCOPED_DRAW_EVENTF(RHICmdList, SkinTangents_PerVertexPass, TEXT("SkinTangents_PerVertexPass InputStreamStart=%d, SkinCacheStart=%d, Vert=%d"),
				DispatchData.InputStreamStart, DispatchData.SkinCacheStart, DispatchData.NumVertices);

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
