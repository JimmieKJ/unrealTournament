// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderGPUSkin.cpp: GPU skinned skeletal mesh rendering code.

	This code contains embedded portions of source code from dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0, Copyright ?2006-2007 University of Dublin, Trinity College, All Rights Reserved, which have been altered from their original version.

	The following terms apply to dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0:

	This software is provided 'as-is', without any express or implied warranty.  In no event will the author(s) be held liable for any damages arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
=============================================================================*/

#include "EnginePrivate.h"
#include "GPUSkinVertexFactory.h"
#include "SkeletalRenderGPUSkin.h"
#include "SkeletalRenderCPUSkin.h"
#include "GPUSkinCache.h"
#include "ShaderCompiler.h"
#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "LocalVertexFactory.h"

#include "GlobalShader.h"
#include "ShaderParameterUtils.h"


DEFINE_LOG_CATEGORY_STATIC(LogSkeletalGPUSkinMesh, Warning, All);

// 0/1
#define UPDATE_PER_BONE_DATA_ONLY_FOR_OBJECT_BEEN_VISIBLE 1
#define MORPH_TARGETS_USE_BYTE_BUFFER_UAV	0

DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Update"),STAT_MorphVertexBuffer_Update,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Init"),STAT_MorphVertexBuffer_Init,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Apply Delta"),STAT_MorphVertexBuffer_ApplyDelta,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Alloc"), STAT_MorphVertexBuffer_Alloc, STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer RHI Lock and copy"), STAT_MorphVertexBuffer_RhiLockAndCopy, STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer RHI Unlock"), STAT_MorphVertexBuffer_RhiUnlock, STATGROUP_MorphTarget);

static TAutoConsoleVariable<int32> CVarMotionBlurDebug(
	TEXT("r.MotionBlurDebug"),
	0,
	TEXT("Defines if we log debugging output for motion blur rendering.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_Cheat | ECVF_RenderThreadSafe);

static int32 GUseGPUMorphTargets = 0;
static FAutoConsoleVariableRef CVarUseGPUMorphTargets(
	TEXT("r.MorphTarget.Mode"),
	GUseGPUMorphTargets,
	TEXT("Use GPU for computing morph targets.\n")
	TEXT(" 0: Use original CPU method (loop per morph then by vertex) (default)\n")
	TEXT(" 1: Enable GPU method\n"),
	ECVF_Default
	);

/*-----------------------------------------------------------------------------
FMorphVertexBuffer
-----------------------------------------------------------------------------*/

void FMorphVertexBuffer::InitDynamicRHI()
{
	// LOD of the skel mesh is used to find number of vertices in buffer
	FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIdx];

	// Create the buffer rendering resource
	uint32 Size = LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);
	FRHIResourceCreateInfo CreateInfo;
	void* BufferData = nullptr;

	bool bSupportsComputeShaders = RHISupportsComputeShaders(GMaxRHIShaderPlatform);
	bUsesComputeShader = GUseGPUMorphTargets != 0 && bSupportsComputeShaders;

#if PLATFORM_PS4
	// PS4 requires non-static buffers in order to be updated on the GPU while the CPU is writing into it
	EBufferUsageFlags Flags = BUF_Dynamic;
#else
	EBufferUsageFlags Flags = bUsesComputeShader ? (EBufferUsageFlags)(BUF_Static | BUF_UnorderedAccess) : BUF_Dynamic;
#if MORPH_TARGETS_USE_BYTE_BUFFER_UAV
	Flags = (EBufferUsageFlags)(Flags | (bUsesComputeShader ? BUF_ByteAddressBuffer : 0));
#endif
#endif

	// BUF_ShaderResource is needed for Morph support of the SkinCache
	Flags = (EBufferUsageFlags)(Flags | BUF_ShaderResource);

	VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, Flags, CreateInfo, BufferData);
	bool bUsesSkinCache = bSupportsComputeShaders && GEnableGPUSkinCacheShaders && GEnableGPUSkinCache;
	if (bUsesSkinCache)
	{
		SRVValue = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);
	}

	// Lock the buffer.
	FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*)BufferData;
	FMemory::Memzero(&Buffer[0], sizeof(FMorphGPUSkinVertex)*LodModel.NumVertices);
	// Unlock the buffer.
	RHIUnlockVertexBuffer(VertexBufferRHI);
	
	if (bUsesComputeShader)
	{
		UAVValue = RHICreateUnorderedAccessView(VertexBufferRHI, PF_R32_UINT);
	}

	// hasn't been updated yet
	bHasBeenUpdated = false;
}

void FMorphVertexBuffer::ReleaseDynamicRHI()
{
	UAVValue.SafeRelease();
	VertexBufferRHI.SafeRelease();
	SRVValue.SafeRelease();
}

/*-----------------------------------------------------------------------------
FSkeletalMeshObjectGPUSkin
-----------------------------------------------------------------------------*/

FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectGPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type InFeatureLevel)
	: FSkeletalMeshObject(InMeshComponent, InSkeletalMeshResource, InFeatureLevel)
	,	DynamicData(NULL)
	,	bNeedsUpdateDeferred(false)
	,	bMorphNeedsUpdateDeferred(false)
	,	bMorphResourcesInitialized(false)
{
	// create LODs to match the base mesh
	LODs.Empty(SkeletalMeshResource->LODModels.Num());
	for( int32 LODIndex=0;LODIndex < SkeletalMeshResource->LODModels.Num();LODIndex++ )
	{
		new(LODs) FSkeletalMeshObjectLOD(SkeletalMeshResource,LODIndex);
	}

	InitResources();
}


FSkeletalMeshObjectGPUSkin::~FSkeletalMeshObjectGPUSkin()
{
	check(!RHIThreadFenceForDynamicData.GetReference());
	if (DynamicData)
	{
		FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(DynamicData);
	}
	DynamicData = nullptr;
}


void FSkeletalMeshObjectGPUSkin::InitResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
		SkelLOD.InitResources(MeshLODInfo, FeatureLevel);
	}
}

void FSkeletalMeshObjectGPUSkin::ReleaseResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		SkelLOD.ReleaseResources();
	}
	// also release morph resources
	ReleaseMorphResources();
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		WaitRHIThreadFenceForDynamicData,
		FSkeletalMeshObjectGPUSkin*, MeshObject, this,
	{
		FScopeCycleCounter Context(MeshObject->GetStatId());
		MeshObject->WaitForRHIThreadFenceForDynamicData();
	}
	);
}

void FSkeletalMeshObjectGPUSkin::InitMorphResources(bool bInUsePerBoneMotionBlur, const TArray<float>& MorphTargetWeights)
{
	if( bMorphResourcesInitialized )
	{
		// release first if already initialized
		ReleaseMorphResources();
	}

	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		// init any morph vertex buffers for each LOD
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
		SkelLOD.InitMorphResources(MeshLODInfo,bInUsePerBoneMotionBlur, FeatureLevel);
	}
	bMorphResourcesInitialized = true;

	if (MorphTargetWeights.Num() > 0)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(InitMorphWeightsVertexBuffer,
			int32, NumWeights, MorphTargetWeights.Num(),
			FVertexBufferAndSRV&, MorphWeightsVertexBuffer, MorphWeightsVertexBuffer,
			{
				const uint32 Size = (uint32)NumWeights * sizeof(float);
				FRHIResourceCreateInfo CreateInfo;
				MorphWeightsVertexBuffer.VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Dynamic | BUF_ShaderResource, CreateInfo);
				MorphWeightsVertexBuffer.VertexBufferSRV = RHICreateShaderResourceView(MorphWeightsVertexBuffer.VertexBufferRHI, sizeof(float), PF_R32_UINT);
			});
	}
}

void FSkeletalMeshObjectGPUSkin::ReleaseMorphResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		// release morph vertex buffers and factories if they were created
		SkelLOD.ReleaseMorphResources();
	}

	if (MorphWeightsVertexBuffer.VertexBufferRHI)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(InitMorphWeightsVertexBuffer,
			FVertexBufferAndSRV&, MorphWeightsVertexBuffer, MorphWeightsVertexBuffer,
			{
				MorphWeightsVertexBuffer.VertexBufferRHI.SafeRelease();
				MorphWeightsVertexBuffer.VertexBufferSRV.SafeRelease();
			});
	}

	bMorphResourcesInitialized = false;
}


void FSkeletalMeshObjectGPUSkin::Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetWeights)
{
	// make sure morph data has been initialized for each LOD
	if( !bMorphResourcesInitialized && ActiveMorphTargets.Num() > 0 )
	{
		// initialized on-the-fly in order to avoid creating extra vertex streams for each skel mesh instance
		InitMorphResources(InMeshComponent->bPerBoneMotionBlur, MorphTargetWeights);		
	}

	// create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicSkelMeshObjectDataGPUSkin* NewDynamicData = FDynamicSkelMeshObjectDataGPUSkin::AllocDynamicSkelMeshObjectDataGPUSkin();		
	NewDynamicData->InitDynamicSkelMeshObjectDataGPUSkin(InMeshComponent,SkeletalMeshResource,LODIndex,ActiveMorphTargets, MorphTargetWeights);

	{
		// Handle the case of skin caching shaders not done compiling before updates are finished/editor is loading
		static bool bNeedToWait = GEnableGPUSkinCache != 0;
		if (bNeedToWait && GShaderCompilingManager)
		{
			GShaderCompilingManager->ProcessAsyncResults(false, true);
			bNeedToWait = false;
		}
	}

	// We prepare the next frame but still have the value from the last one
	uint32 FrameNumberToPrepare = GFrameNumber + 1;

	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		SkelMeshObjectUpdateDataCommand,
		FSkeletalMeshObjectGPUSkin*, MeshObject, this,
		uint32, FrameNumberToPrepare, FrameNumberToPrepare, 
		FDynamicSkelMeshObjectDataGPUSkin*, NewDynamicData, NewDynamicData,
	{
		FScopeCycleCounter Context(MeshObject->GetStatId());
		MeshObject->UpdateDynamicData_RenderThread(RHICmdList, NewDynamicData, FrameNumberToPrepare);
	}
	);

	if( GIsEditor )
	{
		// this does not need thread-safe update
#if WITH_EDITORONLY_DATA
		ProgressiveDrawingFraction = InMeshComponent->ProgressiveDrawingFraction;
#endif // #if WITH_EDITORONLY_DATA
		CustomSortAlternateIndexMode = (ECustomSortAlternateIndexMode)InMeshComponent->CustomSortAlternateIndexMode;
	}
}

void FSkeletalMeshObjectGPUSkin::UpdateRecomputeTangent(int32 MaterialIndex, bool bRecomputeTangent)
{
	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		SkelMeshObjectUpdateMaterialDataCommand,
		FSkeletalMeshObjectGPUSkin*, MeshObject, this,
		int32, MaterialIndex, MaterialIndex, 
		bool, bRecomputeTangent, bRecomputeTangent,
		{
			// iterate through section and find the section that matches MaterialIndex, if so, set that flag
			for (auto& LODModel : MeshObject->SkeletalMeshResource->LODModels)
			{
				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
				{
					// @todo there can be more than one section that can use same material? If not, please break. 
					if (LODModel.Sections[SectionIndex].MaterialIndex == MaterialIndex)
					{
						LODModel.Sections[SectionIndex].bRecomputeTangent = bRecomputeTangent;
					}
				}
			}
		}
	);
}

static TAutoConsoleVariable<int32> CVarDeferSkeletalDynamicDataUpdateUntilGDME(
	TEXT("r.DeferSkeletalDynamicDataUpdateUntilGDME"),
	0,
	TEXT("If > 0, then do skeletal mesh dynamic data updates will be deferred until GDME. Experimental option."));

void FSkeletalMeshObjectGPUSkin::UpdateDynamicData_RenderThread(FRHICommandListImmediate& RHICmdList, FDynamicSkelMeshObjectDataGPUSkin* InDynamicData, uint32 FrameNumberToPrepare)
{
	SCOPE_CYCLE_COUNTER(STAT_GPUSkinUpdateRTTime);
	check(InDynamicData);
	bool bMorphNeedsUpdate=false;
	// figure out if the morphing vertex buffer needs to be updated. compare old vs new active morphs
	bMorphNeedsUpdate = 
		(bMorphNeedsUpdateDeferred && bNeedsUpdateDeferred) || // the need for an update sticks
		(DynamicData ? (DynamicData->LODIndex != InDynamicData->LODIndex ||
		!DynamicData->ActiveMorphTargetsEqual(InDynamicData->ActiveMorphTargets, InDynamicData->MorphTargetWeights))
		: true);

	WaitForRHIThreadFenceForDynamicData();
	if (DynamicData)
	{
		FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(DynamicData);
	}
	// update with new data
	DynamicData = InDynamicData;

	if (CVarDeferSkeletalDynamicDataUpdateUntilGDME.GetValueOnRenderThread())
	{
		bMorphNeedsUpdateDeferred = bMorphNeedsUpdate;
		bNeedsUpdateDeferred = true;
	}
	else
	{
		ProcessUpdatedDynamicData(RHICmdList, FrameNumberToPrepare, bMorphNeedsUpdate);
	}
}

void FSkeletalMeshObjectGPUSkin::PreGDMECallback(uint32 FrameNumber)
{
	if (bNeedsUpdateDeferred)
	{
		ProcessUpdatedDynamicData(FRHICommandListExecutor::GetImmediateCommandList(), FrameNumber, bMorphNeedsUpdateDeferred);
	}
}

void FSkeletalMeshObjectGPUSkin::WaitForRHIThreadFenceForDynamicData()
{
	// we should be done with the old data at this point
	if (RHIThreadFenceForDynamicData.GetReference())
	{
		FRHICommandListExecutor::WaitOnRHIThreadFence(RHIThreadFenceForDynamicData);
		RHIThreadFenceForDynamicData = nullptr;
	}
}

void FSkeletalMeshObjectGPUSkin::ProcessUpdatedDynamicData(FRHICommandListImmediate& RHICmdList, uint32 FrameNumberToPrepare, bool bMorphNeedsUpdate)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshObjectGPUSkin_ProcessUpdatedDynamicData);
	bNeedsUpdateDeferred = false;
	bMorphNeedsUpdateDeferred = false;

	FSkeletalMeshObjectLOD& LOD = LODs[DynamicData->LODIndex];

	// if hasn't been updated, force update again
	bMorphNeedsUpdate = LOD.MorphVertexBuffer.bHasBeenUpdated ? bMorphNeedsUpdate : true;

	bool bMorph = DynamicData->NumWeightedActiveMorphTargets > 0;

	const FStaticLODModel& LODModel = SkeletalMeshResource->LODModels[DynamicData->LODIndex];
	const TArray<FSkelMeshSection>& Sections = GetRenderSections(DynamicData->LODIndex);

	// use correct vertex factories based on alternate weights usage
	FVertexFactoryData& VertexFactoryData = LOD.GPUSkinVertexFactories;

	bool bDataPresent = false;

	bool bGPUSkinCacheEnabled = GEnableGPUSkinCache && (FeatureLevel >= ERHIFeatureLevel::SM5);

	if(bMorph) 
	{
		bDataPresent = true;
		checkSlow((VertexFactoryData.MorphVertexFactories.Num() == Sections.Num()));
		
		// only update if the morph data changed and there are weighted morph targets
		if(bMorphNeedsUpdate)
		{
			if (GUseGPUMorphTargets)
			{
				SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_RhiLockAndCopy);
				const uint32 MorphWeightsSize = DynamicData->MorphTargetWeights.GetAllocatedSize();
				void* NewMorphWeights = RHICmdList.LockVertexBuffer(MorphWeightsVertexBuffer.VertexBufferRHI, 0, MorphWeightsSize, RLM_WriteOnly);
				FMemory::Memcpy(NewMorphWeights, DynamicData->MorphTargetWeights.GetData(), MorphWeightsSize);
				RHICmdList.UnlockVertexBuffer(MorphWeightsVertexBuffer.VertexBufferRHI);
			}

			// update the morph data for the lod (before SkinCache)
			LOD.UpdateMorphVertexBuffer(RHICmdList, DynamicData->ActiveMorphTargets, DynamicData->MorphTargetWeights, MorphWeightsVertexBuffer.VertexBufferSRV, LODModel.MorphTargetVertexInfoBuffers.GetNumInfluencedVerticesByMorphs(), LODModel.MorphTargetVertexInfoBuffers.PerVertexInfoSRV, LODModel.MorphTargetVertexInfoBuffers.FlattenedDeltasSRV);
		}
	}
	else
	{
//		checkSlow(VertexFactoryData.MorphVertexFactories.Num() == 0);
		bDataPresent = VertexFactoryData.VertexFactories.Num() > 0;
	}

	if (bDataPresent)
	{
		for (int32 SectionIdx = 0; SectionIdx < Sections.Num(); SectionIdx++)
		{
			const FSkelMeshSection& Section = Sections[SectionIdx];

			bool bClothFactory = (FeatureLevel >= ERHIFeatureLevel::SM4) && (DynamicData->ClothSimulUpdateData.Num() > 0) && Section.HasApexClothData();

			FGPUBaseSkinVertexFactory* VertexFactory;
			{
				if(bClothFactory)
				{
					VertexFactory = VertexFactoryData.ClothVertexFactories[SectionIdx]->GetVertexFactory();
				}
				else
				{
					if(DynamicData->NumWeightedActiveMorphTargets > 0)
					{
						VertexFactory = VertexFactoryData.MorphVertexFactories[SectionIdx].Get();
					}
					else
					{
						VertexFactory = VertexFactoryData.VertexFactories[SectionIdx].Get();
					}
				}
			}

			FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = VertexFactory->GetShaderData();

			bool bUseSkinCache = bGPUSkinCacheEnabled;
			if (bUseSkinCache)
			{
				if (SectionIdx >= MAX_GPUSKINCACHE_CHUNKS_PER_LOD)
				{
					INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForMaxChunksPerLOD);
					bUseSkinCache = false;
				}
				else if (bClothFactory)
				{
					INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForCloth);
					bUseSkinCache = false;
				}
				else if (Section.MaxBoneInfluences == 0)
				{
					INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForZeroInfluences);
					bUseSkinCache = false;
				}

				{
					extern TAutoConsoleVariable<int32> CVarGPUSkinCacheRecomputeTangents;
					if(CVarGPUSkinCacheRecomputeTangents.GetValueOnRenderThread() == 2)
					{
						if(!Section.bRecomputeTangent)
						{
							// In this mode the SkinCache should only be used for RecomputeTangent meshes
							bUseSkinCache = false;
						}
					}
				}
			}

			// Create a uniform buffer from the bone transforms.
			TArray<FMatrix>& ReferenceToLocalMatrices = DynamicData->ReferenceToLocal;
			bool bNeedFence = ShaderData.UpdateBoneData(RHICmdList, ReferenceToLocalMatrices, Section.BoneMap, FrameNumberToPrepare, FeatureLevel, bUseSkinCache);

			// Try to use the GPU skinning cache if possible
			if (bUseSkinCache)
			{
				if (GUseGPUMorphTargets)
				{
					// When rewriting the skin cache, first do all morph compute jobs, then all skin cache compute jobs, and then all recompute tangent jobs instead of sequentially
				}
				int32 Key = GGPUSkinCache.StartCacheMesh(RHICmdList, FrameNumberToPrepare, GPUSkinCacheKeys[SectionIdx], VertexFactory,
					VertexFactoryData.PassthroughVertexFactories[SectionIdx].Get(), Section, this, bMorph ? &LOD.MorphVertexBuffer : 0);

				// TODO: If failed we could remove the key (on the other hand it might a bu in cache for longer)

				// -1 if failed
				if(Key >= 0)
				{
					ensure(Key <= 0xffff);
					GPUSkinCacheKeys[SectionIdx] = (int16)Key;
				}
			}

#if WITH_APEX_CLOTHING
			// Update uniform buffer for APEX cloth simulation mesh positions and normals
			if( bClothFactory )
			{				
				FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType& ClothShaderData = VertexFactoryData.ClothVertexFactories[SectionIdx]->GetClothShaderData();
				ClothShaderData.ClothBlendWeight = DynamicData->ClothBlendWeight;
				int16 ActorIdx = Section.CorrespondClothAssetIndex;
				if( FClothSimulData* SimData = DynamicData->ClothSimulUpdateData.Find(ActorIdx) )
				{
					bNeedFence = ClothShaderData.UpdateClothSimulData(RHICmdList, SimData->ClothSimulPositions, SimData->ClothSimulNormals, FrameNumberToPrepare, FeatureLevel) || bNeedFence;
				}
			}
#endif // WITH_APEX_CLOTHING
			if (bNeedFence)
			{
				RHIThreadFenceForDynamicData = RHICmdList.RHIThreadFence(true);
			}
		}
	}
}

void FMorphTargetBaseShader::SetParameters(FRHICommandList& RHICmdList, int32 NumLODVertices, int32 NumMorphVertices, FShaderResourceViewRHIRef MorphWeightsSRV, FShaderResourceViewRHIRef PerVertexInfoListSRV, FShaderResourceViewRHIRef FlattenedDeltaListSRV, FUnorderedAccessViewRHIParamRef UAV)
{
	FComputeShaderRHIRef CS = GetComputeShader();
	RHICmdList.SetComputeShader(CS);
	FVector4 MorphTargetCounts(NumLODVertices, NumMorphVertices, 0, 0);
	SetShaderValue(RHICmdList, CS, MorphTargetCountsParameter, MorphTargetCounts);

	SetSRVParameter(RHICmdList, CS, PerVertexInfoListParameter, PerVertexInfoListSRV);

	SetSRVParameter(RHICmdList, CS, FlattenedDeltaListParameter, FlattenedDeltaListSRV);

	SetSRVParameter(RHICmdList, CS, AllWeightsPerMorphsParameter, MorphWeightsSRV);

	SetUAVParameter(RHICmdList, CS, MorphVertexBufferParameter, UAV);
}


class FGPUMorphUpdateCS : public FMorphTargetBaseShader
{
public:
	DECLARE_SHADER_TYPE(FGPUMorphUpdateCS, Global);

	FGPUMorphUpdateCS() {}

	FGPUMorphUpdateCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMorphTargetBaseShader(Initializer)
	{
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("USE_BYTE_BUFFER"), MORPH_TARGETS_USE_BYTE_BUFFER_UAV);
	}

	void Dispatch(FRHICommandList& RHICmdList, int32 NumLODVertices, int32 NumMorphVertices, FShaderResourceViewRHIRef MorphWeightsSRV, FShaderResourceViewRHIRef PerVertexInfoListSRV, FShaderResourceViewRHIRef FlattenedDeltaListSRV, FUnorderedAccessViewRHIParamRef UAV)
	{
		FComputeShaderRHIRef CS = GetComputeShader();
		FMorphTargetBaseShader::SetParameters(RHICmdList, NumLODVertices, NumMorphVertices, MorphWeightsSRV, PerVertexInfoListSRV, FlattenedDeltaListSRV, UAV);
		RHICmdList.DispatchComputeShader((NumMorphVertices + 63) / 64, 1, 1);
		SetUAVParameter(RHICmdList, CS, MorphVertexBufferParameter, nullptr);
	}
};

IMPLEMENT_SHADER_TYPE(, FGPUMorphUpdateCS, TEXT("MorphTargets"), TEXT("GPUMorphUpdateCS"), SF_Compute);


TArray<FVector> FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::MorphDeltaTangentZAccumulationArray;
TArray<float> FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::MorphAccumulatedWeightArray;

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::UpdateMorphVertexBuffer(FRHICommandListImmediate& RHICmdList, const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetWeights, FShaderResourceViewRHIRef MorphWeightsSRV, int32 NumInfluencedVerticesByMorph, FShaderResourceViewRHIRef MorphPerVertexInfoSRV, FShaderResourceViewRHIRef MorphFlattenedSRV)
{
	SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Update);

	if( IsValidRef(MorphVertexBuffer.VertexBufferRHI) )
	{
		extern ENGINE_API bool DoRecomputeSkinTangentsOnGPU_RT();
		bool bBlendTangentsOnCPU = !DoRecomputeSkinTangentsOnGPU_RT();

/*		// TODO: Need to finish this to avoid artifacts if the SkinCache is not handing all objects
		if(!bBlendTangentsOnCPU)
		{
			// It's possible that we reject the object from the SkinCache (e.g. all memory is used up), the we want to render the normal way.
			// Unfortunately we don't know that at this point and code needs to be changed to get the info here.
			if(!GGPUSkinCache.IsElementProcessed())
			{
				bBlendTangentsOnCPU = true;
			}
		}
*/
		// LOD of the skel mesh is used to find number of vertices in buffer
		FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIndex];

		MorphVertexBuffer.RecreateResourcesIfRequired(GUseGPUMorphTargets != 0);

		if (GUseGPUMorphTargets && RHISupportsComputeShaders(GMaxRHIShaderPlatform))
		{
			SCOPED_DRAW_EVENTF(RHICmdList, MorphUpdate,
				TEXT("MorphUpdate LodVertices=%d InflVerts=%d"),
				LodModel.NumVertices,
				NumInfluencedVerticesByMorph);
			RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, MorphVertexBuffer.GetUAV());

			RHICmdList.ClearUAV(MorphVertexBuffer.GetUAV(), {0, 0, 0, 0});

			{
				SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_ApplyDelta);
				TShaderMapRef<FGPUMorphUpdateCS> GPUMorphUpdateCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
				GPUMorphUpdateCS->Dispatch(RHICmdList, LodModel.NumVertices, NumInfluencedVerticesByMorph, MorphWeightsSRV, MorphPerVertexInfoSRV, MorphFlattenedSRV, MorphVertexBuffer.GetUAV());
			}

			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, MorphVertexBuffer.GetUAV());

			// set update flag
			MorphVertexBuffer.bHasBeenUpdated = true;
		}
		else
		{
			uint32 Size = LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);

			FMorphGPUSkinVertex* Buffer = nullptr;
			{
				SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Alloc);
				Buffer = (FMorphGPUSkinVertex*)FMemory::Malloc(Size);
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Init);

				if (bBlendTangentsOnCPU)
				{
					// zero everything
					int32 vertsToAdd = static_cast<int32>(LodModel.NumVertices) - MorphAccumulatedWeightArray.Num();
					if (vertsToAdd > 0)
					{
						MorphAccumulatedWeightArray.AddUninitialized(vertsToAdd);
					}

					FMemory::Memzero(MorphAccumulatedWeightArray.GetData(), sizeof(float)*LodModel.NumVertices);
				}

				// PackedNormals will be wrong init with 0, but they'll be overwritten later
				FMemory::Memzero(&Buffer[0], sizeof(FMorphGPUSkinVertex)*LodModel.NumVertices);
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_ApplyDelta);

				// iterate over all active morph targets and accumulate their vertex deltas
				for (int32 AnimIdx=0; AnimIdx < ActiveMorphTargets.Num(); AnimIdx++)
				{
					const FActiveMorphTarget& MorphTarget = ActiveMorphTargets[AnimIdx];
					checkSlow(MorphTarget.MorphTarget != NULL);
					checkSlow(MorphTarget.MorphTarget->HasDataForLOD(LODIndex));
					const float MorphTargetWeight = MorphTargetWeights[MorphTarget.WeightIndex];
					const float MorphAbsWeight = FMath::Abs(MorphTargetWeight);
					checkSlow(MorphAbsWeight >= MinMorphTargetBlendWeight && MorphAbsWeight <= MaxMorphTargetBlendWeight);


					// Get deltas
					int32 NumDeltas;
					FMorphTargetDelta* Deltas = MorphTarget.MorphTarget->GetMorphTargetDelta(LODIndex, NumDeltas);

					// iterate over the vertices that this lod model has changed
					for (int32 MorphVertIdx=0; MorphVertIdx < NumDeltas; MorphVertIdx++)
					{
						const FMorphTargetDelta& MorphVertex = Deltas[MorphVertIdx];

						// @TODO FIXMELH : temp hack until we fix importing issue
						if ((MorphVertex.SourceIdx < LodModel.NumVertices))
						{
							FMorphGPUSkinVertex& DestVertex = Buffer[MorphVertex.SourceIdx];

							DestVertex.DeltaPosition += MorphVertex.PositionDelta * MorphTargetWeight;

							// todo: could be moved out of the inner loop to be more efficient
							if (bBlendTangentsOnCPU)
							{
								DestVertex.DeltaTangentZ += MorphVertex.TangentZDelta * MorphTargetWeight;
								// accumulate the weight so we can normalized it later
								MorphAccumulatedWeightArray[MorphVertex.SourceIdx] += MorphAbsWeight;
							}
						}
					} // for all vertices
				} // for all morph targets

				if (bBlendTangentsOnCPU)
				{
					// copy back all the tangent values (can't use Memcpy, since we have to pack the normals)
					for (uint32 iVertex = 0; iVertex < LodModel.NumVertices; ++iVertex)
					{
						FMorphGPUSkinVertex& DestVertex = Buffer[iVertex];
						float AccumulatedWeight = MorphAccumulatedWeightArray[iVertex];

						// if accumulated weight is >1.f
						// previous code was applying the weight again in GPU if less than 1, but it doesn't make sense to do so
						// so instead, we just divide by AccumulatedWeight if it's more than 1.
						// now DeltaTangentZ isn't FPackedNormal, so you can apply any value to it. 
						if (AccumulatedWeight > 1.f)
						{
							DestVertex.DeltaTangentZ /= AccumulatedWeight;
						}
					}
				}
			} // ApplyDelta

			// Lock the real buffer.
			{
				SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_RhiLockAndCopy);
				FMorphGPUSkinVertex* ActualBuffer = (FMorphGPUSkinVertex*)RHILockVertexBuffer(MorphVertexBuffer.VertexBufferRHI, 0, Size, RLM_WriteOnly);
				FMemory::Memcpy(ActualBuffer, Buffer, Size);
				FMemory::Free(Buffer);
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_RhiUnlock);
				// Unlock the buffer.
				RHIUnlockVertexBuffer(MorphVertexBuffer.VertexBufferRHI);
				// set update flag
				MorphVertexBuffer.bHasBeenUpdated = true;
			}
		}
	}
}

const FVertexFactory* FSkeletalMeshObjectGPUSkin::GetSkinVertexFactory(const FSceneView* View, int32 LODIndex,int32 ChunkIdx) const
{
	checkSlow( LODs.IsValidIndex(LODIndex) );
	checkSlow( DynamicData );

	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
	const FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

	// cloth simulation is updated & if this ChunkIdx is for ClothVertexFactory
	if ( DynamicData->ClothSimulUpdateData.Num() > 0 
		&& LOD.GPUSkinVertexFactories.ClothVertexFactories.IsValidIndex(ChunkIdx)  
		&& LOD.GPUSkinVertexFactories.ClothVertexFactories[ChunkIdx].IsValid() )
	{
		return LOD.GPUSkinVertexFactories.ClothVertexFactories[ChunkIdx]->GetVertexFactory();
	}

	// If the GPU skinning cache was used, return the passthrough vertex factory
	if (GGPUSkinCache.IsElementProcessed(View->Family->FrameNumber, GPUSkinCacheKeys[ChunkIdx]))
	{
		if(View->Family->EngineShowFlags.SkinCache)
		{
			return LOD.GPUSkinVertexFactories.PassthroughVertexFactories[ChunkIdx].Get();
		}
		else
		{
			// hide this mesh
			return 0;
		}
	}

	// use the morph enabled vertex factory if any active morphs are set
	if( DynamicData->NumWeightedActiveMorphTargets > 0 )
	{
		return LOD.GPUSkinVertexFactories.MorphVertexFactories[ChunkIdx].Get();
	}

	// use the default gpu skin vertex factory
	return LOD.GPUSkinVertexFactories.VertexFactories[ChunkIdx].Get();
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitGPUSkinVertexFactoryComponents(typename VertexFactoryType::FDataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	typedef TGPUSkinVertexBase<VertexFactoryType::HasExtraBoneInfluences> BaseVertexType;

	// tangents
	VertexFactoryData->TangentBasisComponents[0] = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,TangentX),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_PackedNormal);
	VertexFactoryData->TangentBasisComponents[1] = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,TangentZ),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_PackedNormal);
	
	// bone indices
	VertexFactoryData->BoneIndices = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,InfluenceBones),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4);
	// bone weights
	VertexFactoryData->BoneWeights = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,InfluenceWeights),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4N);
	if (VertexFactoryType::HasExtraBoneInfluences)
	{
		// Extra streams for bone indices & weights
		VertexFactoryData->ExtraBoneIndices = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,InfluenceBones) + 4,VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4);
		VertexFactoryData->ExtraBoneWeights = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,InfluenceWeights) + 4,VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_UByte4N);
	}

	typedef TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS, VertexFactoryType::HasExtraBoneInfluences> VertexType;
	VertexFactoryData->PositionComponent = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float3);

	// Add a texture coordinate for each texture coordinate set we have
	if( !VertexBuffers.VertexBufferGPUSkin->GetUseFullPrecisionUVs() )
	{
		for( uint32 UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
		{
			VertexFactoryData->TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,UVs) + sizeof(FVector2DHalf) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Half2));
		}
	}
	else
	{	
		for( uint32 UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
		{
			VertexFactoryData->TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,UVs) + sizeof(FVector2D) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float2));
		}
	}

	// Color data may be NULL
	if( VertexBuffers.ColorVertexBuffer != NULL && 
		VertexBuffers.ColorVertexBuffer->IsInitialized() )
	{
		// Color
		VertexFactoryData->ColorComponent = FVertexStreamComponent(
			VertexBuffers.ColorVertexBuffer,STRUCT_OFFSET(FGPUSkinVertexColor,VertexColor),VertexBuffers.ColorVertexBuffer->GetStride(),VET_Color);
	}
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitMorphVertexFactoryComponents(typename VertexFactoryType::FDataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	// delta positions
	VertexFactoryData->DeltaPositionComponent = FVertexStreamComponent(
		VertexBuffers.MorphVertexBuffer,STRUCT_OFFSET(FMorphGPUSkinVertex,DeltaPosition),sizeof(FMorphGPUSkinVertex),VET_Float3);
	// delta normals
	VertexFactoryData->DeltaTangentZComponent = FVertexStreamComponent(
		VertexBuffers.MorphVertexBuffer, STRUCT_OFFSET(FMorphGPUSkinVertex, DeltaTangentZ), sizeof(FMorphGPUSkinVertex), VET_Float3);
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitAPEXClothVertexFactoryComponents(typename VertexFactoryType::FDataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	// barycentric coord for positions
	VertexFactoryData->CoordPositionComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FApexClothPhysToRenderVertData,PositionBaryCoordsAndDist),sizeof(FApexClothPhysToRenderVertData),VET_Float4);
	// barycentric coord for normals
	VertexFactoryData->CoordNormalComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FApexClothPhysToRenderVertData,NormalBaryCoordsAndDist),sizeof(FApexClothPhysToRenderVertData),VET_Float4);
	// barycentric coord for tangents
	VertexFactoryData->CoordTangentComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FApexClothPhysToRenderVertData,TangentBaryCoordsAndDist),sizeof(FApexClothPhysToRenderVertData),VET_Float4);
	// indices for reference physics mesh vertices
	VertexFactoryData->SimulIndicesComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FApexClothPhysToRenderVertData,SimulMeshVertIndices),sizeof(FApexClothPhysToRenderVertData),VET_UShort4);
}

/** 
 * Handles transferring data between game/render threads when initializing vertex factory components 
 */
template <class VertexFactoryType>
class TDynamicUpdateVertexFactoryData
{
public:
	TDynamicUpdateVertexFactoryData(
		VertexFactoryType* InVertexFactory,
		const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers)
		:	VertexFactory(InVertexFactory)
		,	VertexBuffers(InVertexBuffers)
	{}
	VertexFactoryType* VertexFactory;
	const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers VertexBuffers;
	
};

ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_TEMPLATE(
InitGPUSkinVertexFactory,VertexFactoryType,
TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData,VertexUpdateData,
{
	typename VertexFactoryType::FDataType Data;
	InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	VertexUpdateData.VertexFactory->SetData(Data);
	VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
	VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
});

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static VertexFactoryType* CreateVertexFactory(TArray<TUniquePtr<VertexFactoryTypeBase>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel
						 )
{
	auto* VertexFactory = new VertexFactoryType(FeatureLevel);
	VertexFactories.Add(TUniquePtr<VertexFactoryTypeBase>(VertexFactory));

	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory,InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE_TEMPLATE(
	InitGPUSkinVertexFactory,VertexFactoryType,
	TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData);

	// init rendering resource	
	BeginInitResource(VertexFactory);

	return VertexFactory;
}

ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_DECLARE_TEMPLATE(
InitPassthroughGPUSkinVertexFactory, VertexFactoryType,
FGPUSkinPassthroughVertexFactory*, PassthroughVertexFactory, PassthroughVertexFactory,
VertexFactoryType*, SourceVertexFactory, SourceVertexFactory,
{
	SourceVertexFactory->CopyDataTypeForPassthroughFactory(PassthroughVertexFactory);
});

template<typename VertexFactoryType>
static void CreatePassthroughVertexFactory(TArray<TUniquePtr<FGPUSkinPassthroughVertexFactory>>& PassthroughVertexFactories,
	VertexFactoryType* SourceVertexFactory)
{
	auto* VertexFactory = new FGPUSkinPassthroughVertexFactory();
	PassthroughVertexFactories.Add(TUniquePtr<FGPUSkinPassthroughVertexFactory>(VertexFactory));

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER_CREATE_TEMPLATE(
		InitPassthroughGPUSkinVertexFactory, VertexFactoryType,
		FGPUSkinPassthroughVertexFactory, VertexFactory,
		VertexFactoryType, SourceVertexFactory);

	// init rendering resource	
	BeginInitResource(VertexFactory);
}

ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_TEMPLATE(
InitGPUSkinVertexFactoryMorph,VertexFactoryType,
TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData,VertexUpdateData,
{
	typename VertexFactoryType::FDataType Data;
	InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	InitMorphVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	VertexUpdateData.VertexFactory->SetData(Data);
	VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
	VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
});

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static VertexFactoryType* CreateVertexFactoryMorph(TArray<TUniquePtr<VertexFactoryTypeBase>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel
						 )

{
	auto* VertexFactory = new VertexFactoryType(FeatureLevel);
	VertexFactories.Add(TUniquePtr<VertexFactoryTypeBase>(VertexFactory));
						
	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE_TEMPLATE(
	InitGPUSkinVertexFactoryMorph,VertexFactoryType,
	TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData);

	// init rendering resource	
	BeginInitResource(VertexFactory);

	return VertexFactory;
}

// APEX cloth
ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_DECLARE_TEMPLATE(
InitGPUSkinAPEXClothVertexFactory,VertexFactoryType,
TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData,VertexUpdateData,
{
	typename VertexFactoryType::FDataType Data;
	InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	InitAPEXClothVertexFactoryComponents<VertexFactoryType>(&Data,VertexUpdateData.VertexBuffers);
	VertexUpdateData.VertexFactory->SetData(Data);
	VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
	VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
});

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static void CreateVertexFactoryCloth(TArray<TUniquePtr<VertexFactoryTypeBase>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel
						 )

{
	VertexFactoryType* VertexFactory = new VertexFactoryType(FeatureLevel);
	VertexFactories.Add(TUniquePtr<VertexFactoryTypeBase>(VertexFactory));
						
	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER_CREATE_TEMPLATE(
	InitGPUSkinAPEXClothVertexFactory,VertexFactoryType,
	TDynamicUpdateVertexFactoryData<VertexFactoryType>,VertexUpdateData);

	// init rendering resource	
	BeginInitResource(VertexFactory);
}

/**
 * Determine the current vertex buffers valid for the current LOD
 *
 * @param OutVertexBuffers output vertex buffers
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::GetVertexBuffers(
	FVertexFactoryBuffers& OutVertexBuffers,
	FStaticLODModel& LODModel,
	const FSkelMeshObjectLODInfo& MeshLODInfo)
{
	OutVertexBuffers.VertexBufferGPUSkin = &LODModel.VertexBufferGPUSkin;
	OutVertexBuffers.ColorVertexBuffer = &LODModel.ColorVertexBuffer;
	OutVertexBuffers.MorphVertexBuffer = &MorphVertexBuffer;
	OutVertexBuffers.APEXClothVertexBuffer = &LODModel.APEXClothVertexBuffer;
}

/** 
 * Init vertex factory resources for this LOD 
 *
 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshSection>& Sections, 
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// first clear existing factories (resources assumed to have been released already)
	// then [re]create the factories

	VertexFactories.Empty(Sections.Num());
	{
		for(int32 FactoryIdx = 0; FactoryIdx < Sections.Num(); ++FactoryIdx)
		{
			if (VertexBuffers.VertexBufferGPUSkin->HasExtraBoneInfluences())
			{
				auto* VertexFactory = CreateVertexFactory< FGPUBaseSkinVertexFactory, TGPUSkinVertexFactory<true> >(VertexFactories, VertexBuffers, InFeatureLevel);
				CreatePassthroughVertexFactory<TGPUSkinVertexFactory<true>>(PassthroughVertexFactories, VertexFactory);
			}
			else
			{
				auto* VertexFactory = CreateVertexFactory< FGPUBaseSkinVertexFactory, TGPUSkinVertexFactory<false> >(VertexFactories, VertexBuffers, InFeatureLevel);
				CreatePassthroughVertexFactory<TGPUSkinVertexFactory<false>>(PassthroughVertexFactories, VertexFactory);
			}
		}
	}
}

/** 
 * Release vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseVertexFactories()
{
	// Default factories
	for( int32 FactoryIdx=0; FactoryIdx < VertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(VertexFactories[FactoryIdx].Get());
	}

	for (int32 FactoryIdx = 0; FactoryIdx < PassthroughVertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(PassthroughVertexFactories[FactoryIdx].Get());
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitMorphVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshSection>& Sections,
	bool bInUsePerBoneMotionBlur,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// clear existing factories (resources assumed to have been released already)
	MorphVertexFactories.Empty(Sections.Num());
	for( int32 FactoryIdx=0; FactoryIdx < Sections.Num(); FactoryIdx++ )
	{
		if (VertexBuffers.VertexBufferGPUSkin->HasExtraBoneInfluences())
		{
			CreateVertexFactoryMorph<FGPUBaseSkinVertexFactory, TGPUSkinMorphVertexFactory<true> >(MorphVertexFactories,VertexBuffers,InFeatureLevel);
		}
		else
		{
			CreateVertexFactoryMorph<FGPUBaseSkinVertexFactory, TGPUSkinMorphVertexFactory<false> >(MorphVertexFactories, VertexBuffers,InFeatureLevel);
		}
	}
}

/** 
 * Release morph vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseMorphVertexFactories()
{
	// Default morph factories
	for( int32 FactoryIdx=0; FactoryIdx < MorphVertexFactories.Num(); FactoryIdx++ )
	{
		BeginReleaseResource(MorphVertexFactories[FactoryIdx].Get());
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitAPEXClothVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshSection>& Sections,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// clear existing factories (resources assumed to have been released already)
	ClothVertexFactories.Empty(Sections.Num());
	for( int32 FactoryIdx=0; FactoryIdx < Sections.Num(); FactoryIdx++ )
	{
		if (Sections[FactoryIdx].HasApexClothData() && InFeatureLevel >= ERHIFeatureLevel::SM4)
		{
			if (VertexBuffers.VertexBufferGPUSkin->HasExtraBoneInfluences())
			{
				CreateVertexFactoryCloth<FGPUBaseSkinAPEXClothVertexFactory, TGPUSkinAPEXClothVertexFactory<true> >(ClothVertexFactories, VertexBuffers, InFeatureLevel);
			}
			else
			{
				CreateVertexFactoryCloth<FGPUBaseSkinAPEXClothVertexFactory, TGPUSkinAPEXClothVertexFactory<false> >(ClothVertexFactories, VertexBuffers, InFeatureLevel);
			}
		}
		else
		{
			ClothVertexFactories.Add(TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>(nullptr));
		}
	}
}

/** 
 * Release APEX cloth vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseAPEXClothVertexFactories()
{
	// Default APEX cloth factories
	for( int32 FactoryIdx=0; FactoryIdx < ClothVertexFactories.Num(); FactoryIdx++ )
	{
		TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>& ClothVertexFactory = ClothVertexFactories[FactoryIdx];
		if (ClothVertexFactory)
		{
			BeginReleaseResource(ClothVertexFactory->GetVertexFactory());
		}
	}
}
/** 
 * Init rendering resources for this LOD 
 * @param bUseLocalVertexFactory - use non-gpu skinned factory when rendering in ref pose
 * @param MeshLODInfo - information about the state of the bone influence swapping
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitResources(const FSkelMeshObjectLODInfo& MeshLODInfo, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(SkelMeshResource);
	check(SkelMeshResource->LODModels.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];
	
	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers,LODModel,MeshLODInfo);

	// init gpu skin factories
	GPUSkinVertexFactories.InitVertexFactories(VertexBuffers,LODModel.Sections, InFeatureLevel);
	if ( LODModel.HasApexClothData() )
	{
		GPUSkinVertexFactories.InitAPEXClothVertexFactories(VertexBuffers,LODModel.Sections, InFeatureLevel);
	}
}

/** 
 * Release rendering resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseResources()
{	
	// Release gpu skin vertex factories
	GPUSkinVertexFactories.ReleaseVertexFactories();

	// Release APEX cloth vertex factory
	GPUSkinVertexFactories.ReleaseAPEXClothVertexFactories();
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(SkelMeshResource);
	check(SkelMeshResource->LODModels.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

	// init the delta vertex buffer for this LOD
	BeginInitResource(&MorphVertexBuffer);

	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers,LODModel,MeshLODInfo);
	// init morph skin factories
	GPUSkinVertexFactories.InitMorphVertexFactories(VertexBuffers, LODModel.Sections, bInUsePerBoneMotionBlur, InFeatureLevel);
}

/** 
* Release rendering resources for the morph stream of this LOD
*/
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseMorphResources()
{
	// Release morph vertex factories
	GPUSkinVertexFactories.ReleaseMorphVertexFactories();
	// release the delta vertex buffer
	BeginReleaseResource(&MorphVertexBuffer);
}


TArray<FTransform>* FSkeletalMeshObjectGPUSkin::GetComponentSpaceTransforms() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(DynamicData)
	{
		return &(DynamicData->MeshComponentSpaceTransforms);
	}
	else
#endif
	{
		return NULL;
	}
}

const TArray<FMatrix>& FSkeletalMeshObjectGPUSkin::GetReferenceToLocalMatrices() const
{
	return DynamicData->ReferenceToLocal;
}

const FTwoVectors& FSkeletalMeshObjectGPUSkin::GetCustomLeftRightVectors(int32 SectionIndex) const
{
	if( DynamicData && DynamicData->CustomLeftRightVectors.IsValidIndex(SectionIndex) )
	{
		return DynamicData->CustomLeftRightVectors[SectionIndex];
	}
	else
	{
		static FTwoVectors Bad( FVector::ZeroVector, FVector(1.f,0.f,0.f) );
		return Bad;
	}
}

/*-----------------------------------------------------------------------------
FDynamicSkelMeshObjectDataGPUSkin
-----------------------------------------------------------------------------*/

void FDynamicSkelMeshObjectDataGPUSkin::Clear()
{
	ReferenceToLocal.Reset();
	CustomLeftRightVectors.Reset();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	MeshComponentSpaceTransforms.Reset();
#endif
	LODIndex = 0;
	ActiveMorphTargets.Reset();
	NumWeightedActiveMorphTargets = 0;
	ClothSimulUpdateData.Reset();
	ClothBlendWeight = 0.0f;
}


FDynamicSkelMeshObjectDataGPUSkin* FDynamicSkelMeshObjectDataGPUSkin::AllocDynamicSkelMeshObjectDataGPUSkin()
{
	return new FDynamicSkelMeshObjectDataGPUSkin;
}

void FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(FDynamicSkelMeshObjectDataGPUSkin* Who)
{
	delete Who;
}

void FDynamicSkelMeshObjectDataGPUSkin::InitDynamicSkelMeshObjectDataGPUSkin(
	USkinnedMeshComponent* InMeshComponent,
	FSkeletalMeshResource* InSkeletalMeshResource,
	int32 InLODIndex,
	const TArray<FActiveMorphTarget>& InActiveMorphTargets,
	const TArray<float>& InMorphTargetWeights
	)
{
	LODIndex = InLODIndex;
	check(!ActiveMorphTargets.Num() && !ReferenceToLocal.Num() && !CustomLeftRightVectors.Num() && !ClothSimulUpdateData.Num() && !MorphTargetWeights.Num());

	// append instead of equals to avoid alloc
	ActiveMorphTargets.Append(InActiveMorphTargets);
	MorphTargetWeights.Append(InMorphTargetWeights);
	NumWeightedActiveMorphTargets = 0;

	// Gather any bones referenced by shadow shapes
	FSkeletalMeshSceneProxy* SkeletalMeshProxy = (FSkeletalMeshSceneProxy*)InMeshComponent->SceneProxy;
	const TArray<FBoneIndexType>* ExtraRequiredBoneIndices = SkeletalMeshProxy ? &SkeletalMeshProxy->GetSortedShadowBoneIndices() : nullptr;

	// update ReferenceToLocal
	UpdateRefToLocalMatrices( ReferenceToLocal, InMeshComponent, InSkeletalMeshResource, LODIndex, ExtraRequiredBoneIndices );
	UpdateCustomLeftRightVectors( CustomLeftRightVectors, InMeshComponent, InSkeletalMeshResource, LODIndex );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	check(!MeshComponentSpaceTransforms.Num());
	// append instead of equals to avoid alloc
	MeshComponentSpaceTransforms.Append(InMeshComponent->GetComponentSpaceTransforms());
#endif

	// find number of morphs that are currently weighted and will affect the mesh
	for( int32 MorphIdx=ActiveMorphTargets.Num()-1; MorphIdx >= 0; MorphIdx-- )
	{
		const FActiveMorphTarget& Morph = ActiveMorphTargets[MorphIdx];
		const float MorphTargetWeight = MorphTargetWeights[Morph.WeightIndex];
		const float MorphAbsWeight = FMath::Abs(MorphTargetWeight);

		if( Morph.MorphTarget != NULL && 
			MorphAbsWeight >= MinMorphTargetBlendWeight &&
			MorphAbsWeight <= MaxMorphTargetBlendWeight &&
			Morph.MorphTarget->HasDataForLOD(LODIndex) ) 
		{
			NumWeightedActiveMorphTargets++;
		}
		else
		{
			ActiveMorphTargets.RemoveAt(MorphIdx, 1, false);
		}
	}

	// Update the clothing simulation mesh positions and normals
	UpdateClothSimulationData(InMeshComponent);
}

bool FDynamicSkelMeshObjectDataGPUSkin::ActiveMorphTargetsEqual( const TArray<FActiveMorphTarget>& CompareActiveMorphTargets, const TArray<float>& CompareMorphTargetWeights)
{
	bool Result=true;
	if( CompareActiveMorphTargets.Num() == ActiveMorphTargets.Num() )
	{
		const float WeightThreshold = 0.001f;
		const float TimeThreshold = 0.001f;
		for( int32 MorphIdx=0; MorphIdx < ActiveMorphTargets.Num(); MorphIdx++ )
		{
			const FActiveMorphTarget& Morph = ActiveMorphTargets[MorphIdx];
			const FActiveMorphTarget& CompMorph = CompareActiveMorphTargets[MorphIdx];

			if( Morph.MorphTarget != CompMorph.MorphTarget ||
				FMath::Abs(MorphTargetWeights[Morph.WeightIndex] - CompareMorphTargetWeights[CompMorph.WeightIndex]) >= WeightThreshold)
			{
				Result=false;
				break;
			}
		}
	}
	else
	{
		Result = false;
	}
	return Result;
}

bool FDynamicSkelMeshObjectDataGPUSkin::UpdateClothSimulationData(USkinnedMeshComponent* InMeshComponent)
{
	USkeletalMeshComponent* SimMeshComponent = Cast<USkeletalMeshComponent>(InMeshComponent);

#if WITH_APEX_CLOTHING
	if (InMeshComponent->MasterPoseComponent.IsValid() && (SimMeshComponent && SimMeshComponent->IsClothBoundToMasterComponent()))
	{
		USkeletalMeshComponent* SrcComponent = SimMeshComponent;

		// if I have master, override sim component
		 SimMeshComponent = Cast<USkeletalMeshComponent>(InMeshComponent->MasterPoseComponent.Get());

		// IF we don't have sim component that is skeletalmeshcomponent, just ignore
		 if (!SimMeshComponent)
		 {
			 return false;
		 }

		 ClothBlendWeight = SrcComponent->ClothBlendWeight;
		 SimMeshComponent->GetUpdateClothSimulationData(ClothSimulUpdateData, SrcComponent);

		return true;
	}
#endif

	if (SimMeshComponent)
	{
		ClothBlendWeight = SimMeshComponent->ClothBlendWeight;
		SimMeshComponent->GetUpdateClothSimulationData(ClothSimulUpdateData);
		return true;
	}
	return false;
}
