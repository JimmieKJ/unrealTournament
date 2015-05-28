// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

#define GPUSKINCACHE_FRAMES	3

int32 GEnableGPUSkinCache = 0;
static FAutoConsoleVariableRef CVarEnableGPUSkinCache(
	TEXT("r.SkinCaching"),
	GEnableGPUSkinCache,
	TEXT("Whether or not to use the GPU compute skinning cache.\n")
	TEXT("This will perform skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("GPUSkinVertexFactory.usf needs to be touched to cause a recompile if this changes.\n")
	TEXT("0 is off(default), 1 is on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

int32 GMaxGPUSkinCacheElementsPerFrame = 1000;
static FAutoConsoleVariableRef CVarMaxGPUSkinCacheElementsPerFrame(
	TEXT("r.MaxGPUSkinCacheElementsPerFrame"),
	GMaxGPUSkinCacheElementsPerFrame,
	TEXT("The maximum compute processed skin cache elements per frame.")
	);

TGlobalResource<FGPUSkinCache> GGPUSkinCache;

IMPLEMENT_UNIFORM_BUFFER_STRUCT(GPUSkinCacheBonesUniformShaderParameters,TEXT("GPUSkinCacheBones"));


/** Compute shader that skins a batch of vertices. */
template <bool bUseExtraBoneInfluencesT>
class FGPUSkinCacheCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FGPUSkinCacheCS<bUseExtraBoneInfluencesT>,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		const uint32 UseExtraBoneInfluences = bUseExtraBoneInfluencesT;
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), UseExtraBoneInfluences);
	}

	FGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SkinMeshOriginParameter.Bind(Initializer.ParameterMap,TEXT("MeshOrigin"));
		SkinMeshExtensionParameter.Bind(Initializer.ParameterMap,TEXT("MeshExtension"));

		SkinInputStreamStride.Bind(Initializer.ParameterMap, TEXT("SkinCacheVertexStride"));
		SkinInputStreamVertexCount.Bind(Initializer.ParameterMap, TEXT("SkinCacheVertexCount"));
		SkinInputStreamFloatOffset.Bind(Initializer.ParameterMap, TEXT("SkinCacheInputStreamFloatOffset"));
		SkinOutputBufferFloatOffset.Bind(Initializer.ParameterMap, TEXT("SkinCacheOutputBufferFloatOffset"));
		BoneMatrices.Bind(Initializer.ParameterMap, TEXT("BoneMatrices"));
		SkinInputStream.Bind(Initializer.ParameterMap, TEXT("SkinStreamInputBuffer"));

		SkinCacheBufferRW.Bind(Initializer.ParameterMap, TEXT("SkinCacheBuffer"));
	}

	FGPUSkinCacheCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, uint32 InputStreamStride, uint32 InputStreamFloatOffset, uint32 InputStreamVertexCount, uint32 OutputBufferFloatOffset, FBoneBufferTypeRef BoneBuffer, FUniformBufferRHIRef UniformBuffer, FShaderResourceViewRHIRef VertexBufferSRV, FRWBuffer& SkinBuffer, const FVector& MeshOrigin, const FVector& MeshExtension)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		SetShaderValue(RHICmdList, ComputeShaderRHI, SkinMeshOriginParameter, MeshOrigin);
		SetShaderValue(RHICmdList, ComputeShaderRHI, SkinMeshExtensionParameter, MeshExtension);

		SetShaderValue(RHICmdList, ComputeShaderRHI, SkinInputStreamStride, InputStreamStride);
		SetShaderValue(RHICmdList, ComputeShaderRHI, SkinInputStreamVertexCount, InputStreamVertexCount);
		SetShaderValue(RHICmdList, ComputeShaderRHI, SkinInputStreamFloatOffset, InputStreamFloatOffset);
		SetShaderValue(RHICmdList, ComputeShaderRHI, SkinOutputBufferFloatOffset, OutputBufferFloatOffset);

		if (UniformBuffer)
		{
			SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, GetUniformBufferParameter<GPUSkinCacheBonesUniformShaderParameters>(), UniformBuffer);
		}
		else
		{
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, BoneMatrices.GetBaseIndex(), BoneBuffer.VertexBufferSRV);
		}

		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SkinInputStream.GetBaseIndex(), VertexBufferSRV);

		RHICmdList.SetUAVParameter(
			ComputeShaderRHI,
			SkinCacheBufferRW.GetBaseIndex(),
			SkinBuffer.UAV
			);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
		FShaderResourceViewRHIParamRef NullSRV = FShaderResourceViewRHIParamRef();
		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, BoneMatrices.GetBaseIndex(), NullSRV);

		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, SkinInputStream.GetBaseIndex(), NullSRV);

		RHICmdList.SetUAVParameter( ComputeShaderRHI, SkinCacheBufferRW.GetBaseIndex(), FUnorderedAccessViewRHIParamRef() );
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << SkinMeshOriginParameter << SkinMeshExtensionParameter << SkinInputStreamStride
			<< SkinInputStreamVertexCount << SkinInputStreamFloatOffset << SkinOutputBufferFloatOffset
			<< SkinInputStream << SkinCacheBufferRW << BoneMatrices;

		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter SkinMeshOriginParameter;
	FShaderParameter SkinMeshExtensionParameter;

	FShaderParameter SkinInputStreamStride;
	FShaderParameter SkinInputStreamVertexCount;
	FShaderParameter SkinInputStreamFloatOffset;
	FShaderParameter SkinOutputBufferFloatOffset;

	FShaderUniformBufferParameter SkinUniformBuffer;

	FShaderResourceParameter BoneMatrices;
	FShaderResourceParameter SkinInputStream;
	FShaderResourceParameter SkinCacheBufferRW;
};


FGPUSkinCache::FGPUSkinCache()
	: Initialized(false)
	, FrameCounter(0)
	, CacheMaxVectorCount(0)
	, CacheCurrentFloatOffset(0)
	, CacheCurrentFrameOffset(0)
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

void FGPUSkinCache::Initialize()
{
	check(!Initialized);

	SkinCacheBufferRW.Initialize(sizeof(float), MaxBufferSize * GPUSKINCACHE_FRAMES, PF_R32_FLOAT, BUF_Static);

	CachedElements.Reserve(MaxCachedElements);
	CachedVertexBuffers.Reserve(MaxCachedVertexBufferSRVs);

	Initialized = true;
}

void FGPUSkinCache::Cleanup()
{
	if (Initialized)
	{
		SkinCacheBufferRW.Release();

		Initialized = false;
	}
}

// Kick off compute shader skinning for incoming chunk, return key for fast lookup in draw passes
int32 FGPUSkinCache::StartCacheMesh(FRHICommandListImmediate& RHICmdList, int32 Key, const FVertexFactory* VertexFactory, const FVertexFactory* TargetVertexFactory, const FSkelMeshChunk& BatchElement, const FSkeletalMeshObjectGPUSkin* Skin, bool bExtraBoneInfluences)
{
	if (!GEnableGPUSkinCache)
	{
		return -1;
	}

	if (CachedChunksThisFrameCount >= GMaxGPUSkinCacheElementsPerFrame && GFrameNumberRenderThread <= FrameCounter)
	{
		return -1;
	}

	if (!Initialized)
	{
		Initialize();
	}

	if (GFrameNumberRenderThread > FrameCounter)	// Reset cache output if on a new frame
	{
		CacheCurrentFloatOffset = 0;
		CachedChunksThisFrameCount = 0;
		FrameCounter = GFrameNumberRenderThread;
	}

	CacheCurrentFrameOffset = (FrameCounter % GPUSKINCACHE_FRAMES) * MaxBufferSize;

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

	if (ECSInfo != nullptr)
	{
		if (ECSInfo->FrameUpdated == GFrameNumberRenderThread)	// Already cached this element this frame, return 0 key
		{
			return -1;
		}

		AlignStreamOffsetsFrames = (GFrameNumberRenderThread - ECSInfo->FrameUpdated) > 2 ? true : false;

		ECSInfo->FrameUpdated = GFrameNumberRenderThread;
		ECSInfo->PreviousFrameStreamOffset = ECSInfo->StreamOffset;
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
			Info.VertexBufferSRVIndex = -1;
			Info.FrameUpdated = GFrameNumberRenderThread;
			Info.PreviousFrameStreamOffset = 0;
			Info.bExtraBoneInfluences = bExtraBoneInfluences;
			CachedElements.Add(Info);

			ECSInfo = &(CachedElements.GetData()[Info.Key]);
		}
		else
		{
			FElementCacheStatusInfo*	Info = FindEvictableCacheStatusInfo();

			if (Info == nullptr)
			{
				return -1;
			}

			Info->BatchElement = &BatchElement;
			Info->VertexFactory = VertexFactory;
			Info->TargetVertexFactory = TargetVertexFactory;
			Info->Skin = Skin;
			Info->VertexBufferSRVIndex = -1;
			Info->FrameUpdated = GFrameNumberRenderThread;
			Info->PreviousFrameStreamOffset = 0;
			Info->bExtraBoneInfluences = bExtraBoneInfluences;

			ECSInfo = Info;
		}

		AlignStreamOffsetsFrames = true;
	}

	uint32	StreamStrides[32];
	uint32	StreamStrideCount = VertexFactory->GetStreamStrides(StreamStrides);

	uint32	ElementVertexCount = BatchElement.GetNumVertices();

	uint32	InputStreamFloatOffset = (StreamStrides[0] * BatchElement.BaseVertexIndex) / sizeof(float);

	uint32	ProcessedFloatCount = (StreamStrides[0] * ElementVertexCount) / sizeof(float);

	int32 CompensatedOffset = ((int32)CacheCurrentFloatOffset*4) - (BatchElement.BaseVertexIndex * StreamStrides[0]);
	if (CompensatedOffset < 0)
	{
		CacheCurrentFloatOffset -= CompensatedOffset/4;
	}

	if (CacheCurrentFloatOffset + ProcessedFloatCount > MaxBufferSize)
	{
		return -1;
	}

	ECSInfo->StreamStride = StreamStrides[0];
	ECSInfo->StreamOffset = CacheCurrentFloatOffset + CacheCurrentFrameOffset;

	if (AlignStreamOffsetsFrames)
	{
		ECSInfo->PreviousFrameStreamOffset = ECSInfo->StreamOffset;
	}

	const auto* GPUVertexFactory = (const FGPUBaseSkinVertexFactory*)VertexFactory;
	const FVertexBuffer* SkinVertexBuffer = GPUVertexFactory->GetSkinVertexBuffer();
	FVertexBufferInfo* VBInfo = nullptr;

	if (ECSInfo->VertexBufferSRVIndex >= 0)
	{
		VBInfo = &(CachedVertexBuffers.GetData()[ECSInfo->VertexBufferSRVIndex]);
	}
	else
	{
		VBInfo = CachedVertexBuffers.FindByKey(*SkinVertexBuffer);

		if (VBInfo)
		{
			ECSInfo->VertexBufferSRVIndex = VBInfo->Index;
		}
	}

	if (VBInfo == nullptr)	// Add new cache entry for vertex buffer and SRV
	{
		FVertexBufferInfo	Info;
		Info.VertexBuffer = SkinVertexBuffer;
		Info.VertexBufferSRV = RHICreateShaderResourceView(SkinVertexBuffer->VertexBufferRHI, 4, PF_R32_FLOAT);
		int32 Index = CachedVertexBuffers.Add(Info);
		VBInfo = &(CachedVertexBuffers.GetData()[Index]);
		VBInfo->Index = Index;
		ECSInfo->VertexBufferSRVIndex = Index;
	}

	auto& ShaderData = GPUVertexFactory->GetShaderData();
	
	DispatchSkinCacheProcess(RHICmdList, InputStreamFloatOffset, ECSInfo->StreamOffset, ShaderData.GetBoneBuffer(), ShaderData.GetUniformBuffer(), VBInfo, StreamStrides[0], ElementVertexCount, ShaderData.MeshOrigin, ShaderData.MeshExtension, bExtraBoneInfluences, Skin->GetFeatureLevel());

	CacheCurrentFloatOffset += ProcessedFloatCount;
	CachedChunksThisFrameCount++;

	return ECSInfo->Key;
}

bool FGPUSkinCache::IsElementProcessed(int32 Key)
{
	if (!GEnableGPUSkinCache)
	{
		return false;
	}

	if (Key >= 0 && Key < CachedElements.Num())
	{
		FElementCacheStatusInfo& CacheInfo = CachedElements.GetData()[Key];

		if (CacheInfo.FrameUpdated == GFrameNumberRenderThread)
		{
			return true;
		}
	}

	return false;
}

bool FGPUSkinCache::SetVertexStreamFromCache(FRHICommandList& RHICmdList, int32 Key, FShader* Shader, const FVertexFactory* VertexFactory, uint32 BaseVertexIndex, bool VelocityPass, FShaderParameter PreviousStreamFloatOffset, FShaderParameter PreviousStreamFloatStride, FShaderResourceParameter PreviousStreamBuffer)
{
	if (Key >= 0 && Key < CachedElements.Num())
	{
		FElementCacheStatusInfo& CacheInfo = CachedElements.GetData()[Key];

		// Verify that we found the correct chunk and that is was processed this frame
		if (CacheInfo.BatchElement->BaseVertexIndex == BaseVertexIndex && CacheInfo.TargetVertexFactory == VertexFactory && CacheInfo.FrameUpdated == GFrameNumberRenderThread)
		{
			int32 CompensatedOffset = ((int32)CacheInfo.StreamOffset*4) - (CacheInfo.BatchElement->BaseVertexIndex * CacheInfo.StreamStride);
			if (CompensatedOffset < 0)
			{
				return false;
			}

			RHICmdList.SetStreamSource(0, SkinCacheBufferRW.Buffer, CacheInfo.StreamStride, CompensatedOffset);

			FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
			if (VelocityPass && VertexShaderRHI)
			{
				int32 PreviousCompensatedOffset = ((int32)CacheInfo.PreviousFrameStreamOffset*4) - (CacheInfo.BatchElement->BaseVertexIndex * CacheInfo.StreamStride);
				if (PreviousCompensatedOffset < 0)
				{
					return false;
				}

				if (PreviousStreamFloatOffset.IsBound())
				{
					SetShaderValue(RHICmdList, VertexShaderRHI, PreviousStreamFloatOffset, PreviousCompensatedOffset/4);
				}

				if (PreviousStreamFloatStride.IsBound())
				{
					SetShaderValue(RHICmdList, VertexShaderRHI, PreviousStreamFloatStride, CacheInfo.StreamStride/4);
				}

				if (PreviousStreamBuffer.IsBound())
				{
					RHICmdList.SetShaderResourceViewParameter(VertexShaderRHI, PreviousStreamBuffer.GetBaseIndex(), SkinCacheBufferRW.SRV);
				}
			}

			return true;
		}
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

void FGPUSkinCache::DispatchSkinCacheProcess(FRHICommandListImmediate& RHICmdList, uint32 InputStreamFloatOffset, uint32 OutputBufferFloatOffset, FBoneBufferTypeRef BoneBuffer, FUniformBufferRHIRef UniformBuffer, const FVertexBufferInfo* VBInfo, uint32 VertexStride, uint32 VertexCount, const FVector& MeshOrigin, const FVector& MeshExtension, bool bUseExtraBoneInfluences, ERHIFeatureLevel::Type FeatureLevel)
{
	if (FeatureLevel < ERHIFeatureLevel::SM5)
	{
		return;
	}

	uint32 VertexCountAlign64 = (VertexCount + 63) / 64;

	SCOPED_DRAW_EVENT(RHICmdList, SkinCacheDispatch);

	if (bUseExtraBoneInfluences)
	{
		TShaderMapRef<FGPUSkinCacheCS<true> > SkinCacheCS(GetGlobalShaderMap(FeatureLevel));
		SkinCacheCS->SetParameters(RHICmdList, VertexStride, InputStreamFloatOffset, VertexCount, OutputBufferFloatOffset, BoneBuffer, UniformBuffer, VBInfo->VertexBufferSRV, SkinCacheBufferRW, MeshOrigin, MeshExtension);

		RHICmdList.SetComputeShader(SkinCacheCS->GetComputeShader());
		RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);
		SkinCacheCS->UnsetParameters(RHICmdList);
	}
	else
	{
		TShaderMapRef<FGPUSkinCacheCS<false> > SkinCacheCS(GetGlobalShaderMap(FeatureLevel));
		SkinCacheCS->SetParameters(RHICmdList, VertexStride, InputStreamFloatOffset, VertexCount, OutputBufferFloatOffset, BoneBuffer, UniformBuffer, VBInfo->VertexBufferSRV, SkinCacheBufferRW, MeshOrigin, MeshExtension);

		RHICmdList.SetComputeShader(SkinCacheCS->GetComputeShader());
		RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);
		SkinCacheCS->UnsetParameters(RHICmdList);
	}
}

IMPLEMENT_SHADER_TYPE(template<>,FGPUSkinCacheCS<false>,TEXT("GpuSkinCacheComputeShader"),TEXT("SkinCacheUpdateBatchCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,FGPUSkinCacheCS<true>,TEXT("GpuSkinCacheComputeShader"),TEXT("SkinCacheUpdateBatchCS"),SF_Compute);
