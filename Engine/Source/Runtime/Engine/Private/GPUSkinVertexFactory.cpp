// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUVertexFactory.cpp: GPU skin vertex factory implementation
=============================================================================*/

#include "EnginePrivate.h"
#include "GPUSkinVertexFactory.h"
#include "SkeletalRenderGPUSkin.h"	// FPreviousPerBoneMotionBlur
#include "GPUSkinCache.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FAPEXClothUniformShaderParameters,TEXT("APEXClothParam"));

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FBoneMatricesUniformShaderParameters,TEXT("Bones"));

static FBoneMatricesUniformShaderParameters GBoneUniformStruct;

#define IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE_INTERNAL(FactoryClass, ShaderFilename,bUsedWithMaterials,bSupportsStaticLighting,bSupportsDynamicLighting,bPrecisePrevWorldPos,bSupportsPositionOnly) \
	template <bool bExtraBoneInfluencesT> FVertexFactoryType FactoryClass<bExtraBoneInfluencesT>::StaticType( \
	bExtraBoneInfluencesT ? TEXT(#FactoryClass) TEXT("true") : TEXT(#FactoryClass) TEXT("false"), \
	TEXT(ShaderFilename), \
	bUsedWithMaterials, \
	bSupportsStaticLighting, \
	bSupportsDynamicLighting, \
	bPrecisePrevWorldPos, \
	bSupportsPositionOnly, \
	Construct##FactoryClass##ShaderParameters<bExtraBoneInfluencesT>, \
	FactoryClass<bExtraBoneInfluencesT>::ShouldCache, \
	FactoryClass<bExtraBoneInfluencesT>::ModifyCompilationEnvironment, \
	FactoryClass<bExtraBoneInfluencesT>::SupportsTessellationShaders \
	);

#define IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE(FactoryClass, ShaderFilename,bUsedWithMaterials,bSupportsStaticLighting,bSupportsDynamicLighting,bPrecisePrevWorldPos,bSupportsPositionOnly) \
	template <bool bExtraBoneInfluencesT> FVertexFactoryShaderParameters* Construct##FactoryClass##ShaderParameters(EShaderFrequency ShaderFrequency) { return FactoryClass<bExtraBoneInfluencesT>::ConstructShaderParameters(ShaderFrequency); } \
	IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE_INTERNAL(FactoryClass, ShaderFilename,bUsedWithMaterials,bSupportsStaticLighting,bSupportsDynamicLighting,bPrecisePrevWorldPos,bSupportsPositionOnly) \
	template class FactoryClass<false>;	\
	template class FactoryClass<true>;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarVelocityTest(
	TEXT("r.VelocityTest"),
	0,
	TEXT("Allows to enable some low level testing code for the velocity rendering (Affects object motion blur and TemporalAA).")
	TEXT(" 0: off (default)")
	TEXT(" 1: add random data to the buffer where we store skeletal mesh bone data to test if the code (good to test in PAUSED as well)."),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif // if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/*-----------------------------------------------------------------------------
 FSharedPoolPolicyData
 -----------------------------------------------------------------------------*/
uint32 FSharedPoolPolicyData::GetPoolBucketIndex(uint32 Size)
{
	unsigned long Lower = 0;
	unsigned long Upper = NumPoolBucketSizes;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= BucketSizes[Middle-1] )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= BucketSizes[Lower] );
	check( (Lower == 0 ) || ( Size > BucketSizes[Lower-1] ) );
	
	return Lower;
}

uint32 FSharedPoolPolicyData::GetPoolBucketSize(uint32 Bucket)
{
	check(Bucket < NumPoolBucketSizes);
	return BucketSizes[Bucket];
}

uint32 FSharedPoolPolicyData::BucketSizes[NumPoolBucketSizes] = {
	16, 48, 96, 192, 384, 768, 1536, 
	3072, 4608, 6144, 7680, 9216, 12288, 
	65536, 131072, 262144 // these 3 numbers are added for large cloth simulation vertices, supports up to 16,384 verts
};

/*-----------------------------------------------------------------------------
 FBoneBufferPoolPolicy
 -----------------------------------------------------------------------------*/
FBoneBufferTypeRef FBoneBufferPoolPolicy::CreateResource(CreationArguments Args)
{
	uint32 BufferSize = GetPoolBucketSize(GetPoolBucketIndex(Args));
	FBoneBuffer Buffer;
	FRHIResourceCreateInfo CreateInfo;
	Buffer.VertexBufferRHI = RHICreateVertexBuffer( BufferSize, (BUF_Dynamic | BUF_ShaderResource), CreateInfo );
	Buffer.VertexBufferSRV = RHICreateShaderResourceView( Buffer.VertexBufferRHI, sizeof(FVector4), PF_A32B32G32R32F );
	return Buffer;
}

FSharedPoolPolicyData::CreationArguments FBoneBufferPoolPolicy::GetCreationArguments(FBoneBufferTypeRef Resource)
{
	return (Resource.VertexBufferRHI->GetSize());
}

/*-----------------------------------------------------------------------------
 FBoneBufferPool
 -----------------------------------------------------------------------------*/
FBoneBufferPool::~FBoneBufferPool()
{
}

TStatId FBoneBufferPool::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FBoneBufferPool, STATGROUP_Tickables);
}

TConsoleVariableData<int32>* FGPUBaseSkinVertexFactory::ShaderDataType::MaxBonesVar = NULL;
uint32 FGPUBaseSkinVertexFactory::ShaderDataType::MaxGPUSkinBones = 0;

void FGPUBaseSkinVertexFactory::ShaderDataType::UpdateBoneData(ERHIFeatureLevel::Type FeatureLevel)
{
	uint32 NumBones = BoneMatrices.Num();
	check(NumBones <= MaxGPUSkinBones);
	if (FeatureLevel >= ERHIFeatureLevel::ES3_1)
	{
		static FSharedPoolPolicyData PoolPolicy;
		uint32 NumVectors = NumBones*3;
		check(NumVectors <= (MaxGPUSkinBones*3));
		uint32 VectorArraySize = NumVectors * sizeof(FVector4);
		uint32 PooledArraySize = BoneBufferPool.PooledSizeForCreationArguments(VectorArraySize);
		if(!IsValidRef(BoneBuffer) || PooledArraySize != BoneBuffer.VertexBufferRHI->GetSize())
		{
			if(IsValidRef(BoneBuffer))
			{
				BoneBufferPool.ReleasePooledResource(BoneBuffer);
			}
			BoneBuffer = BoneBufferPool.CreatePooledResource(VectorArraySize);
			check(IsValidRef(BoneBuffer));
		}
		if(NumBones)
		{
			if (GRHIThread)
			{
				check(VectorArraySize == NumBones * sizeof(BoneMatrices[0]));
				GRHICommandList.GetImmediateCommandList().UpdateVertexBuffer(BoneBuffer.VertexBufferRHI, BoneMatrices.GetData(), NumBones * sizeof(BoneMatrices[0]));
			}
			else
			{
				float* Data = (float*)RHILockVertexBuffer(BoneBuffer.VertexBufferRHI, 0, VectorArraySize, RLM_WriteOnly);
				checkSlow(Data);
				FMemory::Memcpy(Data, BoneMatrices.GetData(), NumBones * sizeof(BoneMatrices[0]));
				RHIUnlockVertexBuffer(BoneBuffer.VertexBufferRHI);
			}
		}
	}
	else
	{
		check(NumBones * BoneMatrices.GetTypeSize() <= sizeof(GBoneUniformStruct));
		FMemory::Memcpy(&GBoneUniformStruct, BoneMatrices.GetData(), NumBones * sizeof(BoneMatrices[0]));
		UniformBuffer = RHICreateUniformBuffer(&GBoneUniformStruct, FBoneMatricesUniformShaderParameters::StaticStruct.GetLayout(), UniformBuffer_MultiFrame);
	}
}

/*-----------------------------------------------------------------------------
	FBoneDataTexture
	SizeX(32 * 1024) - Good size for UE3
-----------------------------------------------------------------------------*/

FBoneDataVertexBuffer::FBoneDataVertexBuffer()
	: SizeX(80 * 1024)		// todo: we will replace this fixed size using FGlobalDynamicVertexBuffer
	, AllocSize(0)
	, AllocBlock(nullptr)
{
}

float* FBoneDataVertexBuffer::LockData()
{
	check(IsInRenderingThread() && GetSizeX() && IsValidRef(BoneBuffer));
	check(!AllocSize && !AllocBlock);
	AllocSize = ComputeMemorySize();
	AllocBlock = FMemory::Malloc(AllocSize + 15, 16);
	float* Data = (float*)AllocBlock;
	check(AllocSize && AllocBlock);
	check(Data);
	return Data;
}

void FBoneDataVertexBuffer::UnlockData(uint32 SizeInBytes)
{
	check(IsValidRef(BoneBuffer));
	check(IsInRenderingThread() && AllocBlock && AllocSize);
	FRHICommandListExecutor::GetImmediateCommandList().UpdateVertexBuffer(BoneBuffer.VertexBufferRHI, AllocBlock, SizeInBytes ? SizeInBytes : 4);
	FMemory::Free(AllocBlock);
	AllocBlock = nullptr;
	AllocSize = 0;
}

uint32 FBoneDataVertexBuffer::GetSizeX() const
{
	return SizeX;
}

uint32 FBoneDataVertexBuffer::ComputeMemorySize()
{
	return SizeX * sizeof(FVector4);
}

/*-----------------------------------------------------------------------------
TGPUSkinVertexFactory
-----------------------------------------------------------------------------*/

TGlobalResource<FBoneBufferPool> FGPUBaseSkinVertexFactory::BoneBufferPool;

template <bool bExtraBoneInfluencesT>
bool TGPUSkinVertexFactory<bExtraBoneInfluencesT>::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType)
{
	// Skip trying to use extra bone influences on < SM4
	if (bExtraBoneInfluencesT && GetMaxSupportedFeatureLevel(Platform) < ERHIFeatureLevel::ES3_1)
	{
		return false;
	}

	return (Material->IsUsedWithSkeletalMesh() || Material->IsSpecialEngineMaterial());
}


template <bool bExtraBoneInfluencesT>
void TGPUSkinVertexFactory<bExtraBoneInfluencesT>::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
{
	FVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	const int32 MaxGPUSkinBones = GetFeatureLevelMaxNumberOfBones(GetMaxSupportedFeatureLevel(Platform));
	OutEnvironment.SetDefine(TEXT("MAX_SHADER_BONES"), MaxGPUSkinBones);
	const uint32 UseExtraBoneInfluences = bExtraBoneInfluencesT;
	OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), UseExtraBoneInfluences);
}

/**
* Add the decl elements for the streams
* @param InData - type with stream components
* @param OutElements - vertex decl list to modify
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinVertexFactory<bExtraBoneInfluencesT>::AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements)
{
	// position decls
	OutElements.Add(AccessStreamComponent(InData.PositionComponent,0));

	// tangent basis vector decls
	OutElements.Add(AccessStreamComponent(InData.TangentBasisComponents[0],1));
	OutElements.Add(AccessStreamComponent(InData.TangentBasisComponents[1],2));

	// texture coordinate decls
	if(InData.TextureCoordinates.Num())
	{
		const uint8 BaseTexCoordAttribute = 5;
		for(int32 CoordinateIndex = 0;CoordinateIndex < InData.TextureCoordinates.Num();CoordinateIndex++)
		{
			OutElements.Add(AccessStreamComponent(
				InData.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
				));
		}

		for(int32 CoordinateIndex = InData.TextureCoordinates.Num();CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
		{
			OutElements.Add(AccessStreamComponent(
				InData.TextureCoordinates[InData.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
				));
		}
	}

	// Account for the possibility that the mesh has no vertex colors
	if( InData.ColorComponent.VertexBuffer )
	{
		OutElements.Add(AccessStreamComponent(InData.ColorComponent, 13));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		OutElements.Add(AccessStreamComponent(NullColorComponent,13));
	}

	// bone indices decls
	OutElements.Add(AccessStreamComponent(InData.BoneIndices,3));

	// bone weights decls
	OutElements.Add(AccessStreamComponent(InData.BoneWeights,4));

	if (bExtraBoneInfluencesT)
	{
		// Extra bone indices & weights decls
		OutElements.Add(AccessStreamComponent(InData.ExtraBoneIndices, 14));
		OutElements.Add(AccessStreamComponent(InData.ExtraBoneWeights, 15));
	}
}

/**
* Creates declarations for each of the vertex stream components and
* initializes the device resource
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinVertexFactory<bExtraBoneInfluencesT>::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;
	AddVertexElements(Data,Elements);	

	// create the actual device decls
	InitDeclaration(Elements,FVertexFactory::DataType());
}

template <bool bExtraBoneInfluencesT>
void TGPUSkinVertexFactory<bExtraBoneInfluencesT>::InitDynamicRHI()
{
	FVertexFactory::InitDynamicRHI();
	ShaderData.UpdateBoneData(GetFeatureLevel());
}

template <bool bExtraBoneInfluencesT>
void TGPUSkinVertexFactory<bExtraBoneInfluencesT>::ReleaseDynamicRHI()
{
	FVertexFactory::ReleaseDynamicRHI();
	ShaderData.ReleaseBoneData();
}

/*-----------------------------------------------------------------------------
TGPUSkinAPEXClothVertexFactory
-----------------------------------------------------------------------------*/

template <bool bExtraBoneInfluencesT>
void TGPUSkinAPEXClothVertexFactory<bExtraBoneInfluencesT>::ReleaseDynamicRHI()
{
	Super::ReleaseDynamicRHI();
	ClothShaderData.ReleaseClothSimulData();
}

/*-----------------------------------------------------------------------------
TGPUSkinVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/

/** Shader parameters for use with TGPUSkinVertexFactory */
class FGPUSkinVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		BoneIndexOffset.Bind(ParameterMap,TEXT("BoneIndexOffset"));
		MeshOriginParameter.Bind(ParameterMap,TEXT("MeshOrigin"));
		MeshExtensionParameter.Bind(ParameterMap,TEXT("MeshExtension"));
		PerBoneMotionBlur.Bind(ParameterMap,TEXT("PerBoneMotionBlur"));
		BoneMatrices.Bind(ParameterMap,TEXT("BoneMatrices"));
		PreviousBoneMatrices.Bind(ParameterMap,TEXT("PreviousBoneMatrices"));
	}
	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar) override
	{
		Ar << BoneIndexOffset;
		Ar << MeshOriginParameter;
		Ar << MeshExtensionParameter;
		Ar << PerBoneMotionBlur;
		Ar << BoneMatrices;
		Ar << PreviousBoneMatrices;
	}

	/** Are we are in the velocity rendering pass or render velocity in the base pass? */
	bool IsRenderingVelocity() const
	{
		return BoneIndexOffset.IsBound();
	}

	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		if(Shader->GetVertexShader())
		{
			uint32 FrameNumber = View.Family->FrameNumber;

			const auto FeatureLevel = View.GetFeatureLevel();
			const FGPUBaseSkinVertexFactory::ShaderDataType& ShaderData = ((const FGPUBaseSkinVertexFactory*)VertexFactory)->GetShaderData();

			SetShaderValue(
				RHICmdList, 
				Shader->GetVertexShader(), 
				MeshOriginParameter, 
				ShaderData.MeshOrigin
				);
			SetShaderValue(
				RHICmdList, 
				Shader->GetVertexShader(), 
				MeshExtensionParameter, 
				ShaderData.MeshExtension
				);
			
			if (FeatureLevel >= ERHIFeatureLevel::ES3_1)
			{
				if(BoneMatrices.IsBound())
				{
					RHICmdList.SetShaderResourceViewParameter(Shader->GetVertexShader(), BoneMatrices.GetBaseIndex(), ShaderData.GetBoneBuffer().VertexBufferSRV);
				}
			}
			else
			{
				SetUniformBufferParameter(RHICmdList, Shader->GetVertexShader(), Shader->GetUniformBufferParameter<FBoneMatricesUniformShaderParameters>(), ShaderData.GetUniformBuffer());
			}

			bool bLocalPerBoneMotionBlur = false;
			
			if (FeatureLevel >= ERHIFeatureLevel::SM4 && GPrevPerBoneMotionBlur.IsAppendStarted())
			{
				const bool bWorldIsPaused = View.Family->bWorldIsPaused;

				// we are in the velocity rendering pass

				// 0xffffffff or valid index
				uint32 OldBoneDataIndex = ShaderData.GetOldBoneData(FrameNumber);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				{
					static const auto MotionBlurDebugVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MotionBlurDebug"));

					if (MotionBlurDebugVar && MotionBlurDebugVar->GetValueOnRenderThread())
					{
						UE_LOG(LogEngine, Log, TEXT("%s"), *ShaderData.GetDebugString(VertexFactory, OldBoneDataIndex));
					}
				}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

				// Read old data if it was written last frame (normal data) or this frame (e.g. split screen)
				bLocalPerBoneMotionBlur = (OldBoneDataIndex != 0xffffffff);

				// we tell the shader where to pickup the data (always, even if we don't have bone data, to avoid false binding)
				if(PreviousBoneMatrices.IsBound())
				{
					RHICmdList.SetShaderResourceViewParameter(Shader->GetVertexShader(), PreviousBoneMatrices.GetBaseIndex(), GPrevPerBoneMotionBlur.GetReadData()->BoneBuffer.VertexBufferSRV);
				}

				if(bLocalPerBoneMotionBlur)
				{
					uint32 BoneIndexOffsetValue[4];

					BoneIndexOffsetValue[0] = OldBoneDataIndex;
					BoneIndexOffsetValue[1] = OldBoneDataIndex + 1;
					BoneIndexOffsetValue[2] = OldBoneDataIndex + 2;
					BoneIndexOffsetValue[3] = 0;

					SetShaderValue(
						RHICmdList, 
						Shader->GetVertexShader(), 
						BoneIndexOffset, 
						BoneIndexOffsetValue
						);
				}
				FScopeLock Lock(&ShaderData.OldBoneDataLock);

				const FGPUBaseSkinVertexFactory* GPUVertexFactory = (const FGPUBaseSkinVertexFactory*)VertexFactory;

				// if we haven't copied the data yet we skip the update (e.g. split screen)
				if(ShaderData.IsOldBoneDataUpdateNeeded(FrameNumber))
				{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (CVarVelocityTest.GetValueOnRenderThread())
					{
						FBoneSkinning Tab[10];
						GPrevPerBoneMotionBlur.AppendData(Tab, (FMath::Rand() % 55) + 1);
					}
#endif // if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

					// copy the bone data and tell the instance where it can pick it up next frame
					// append data to a buffer we bind next frame to read old matrix data for motion blur
					uint32 OldBoneDataStartIndex = GPrevPerBoneMotionBlur.AppendData(ShaderData.BoneMatrices.GetData(), ShaderData.BoneMatrices.Num());
					GPUVertexFactory->SetOldBoneDataStartIndex(FrameNumber, OldBoneDataStartIndex);
				}
			}
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), PerBoneMotionBlur, bLocalPerBoneMotionBlur);
		}
	}

	virtual uint32 GetSize() const override { return sizeof(*this); }

private:
	FShaderParameter BoneIndexOffset;
	FShaderParameter MeshOriginParameter;
	FShaderParameter MeshExtensionParameter;
	FShaderParameter PerBoneMotionBlur;
	FShaderResourceParameter BoneMatrices;
	FShaderResourceParameter PreviousBoneMatrices;
};

template <bool bExtraBoneInfluencesT>
FVertexFactoryShaderParameters* TGPUSkinVertexFactory<bExtraBoneInfluencesT>::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return (ShaderFrequency == SF_Vertex) ? new FGPUSkinVertexFactoryShaderParameters() : NULL;
}

/** bind gpu skin vertex factory to its shader file and its shader parameters */
IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE(TGPUSkinVertexFactory, "GpuSkinVertexFactory", true, false, true, false, false);

/*-----------------------------------------------------------------------------
TGPUSkinVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/

/** Shader parameters for use with TGPUSkinVertexFactory */
class FGPUSkinVertexPassthroughFactoryShaderParameters : public FGPUSkinVertexFactoryShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		FGPUSkinVertexFactoryShaderParameters::Bind(ParameterMap);
		GPUSkinCacheStreamFloatOffset.Bind(ParameterMap,TEXT("GPUSkinCacheStreamFloatOffset"));
		GPUSkinCacheStreamStride.Bind(ParameterMap,TEXT("GPUSkinCacheStreamStride"));
		GPUSkinCacheStreamBuffer.Bind(ParameterMap,TEXT("GPUSkinCacheStreamBuffer"));
	}
	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar) override
	{
		FGPUSkinVertexFactoryShaderParameters::Serialize(Ar);
		Ar << GPUSkinCacheStreamFloatOffset;
		Ar << GPUSkinCacheStreamStride;
		Ar << GPUSkinCacheStreamBuffer;
	}
	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FGPUSkinVertexFactoryShaderParameters::SetMesh(RHICmdList, Shader, VertexFactory, View, BatchElement, DataFlags);

		bool bIsGPUCached = false;

		if (GEnableGPUSkinCache && GGPUSkinCache.SetVertexStreamFromCache(RHICmdList, BatchElement.UserIndex, Shader, VertexFactory, BatchElement.MinVertexIndex, GPrevPerBoneMotionBlur.IsAppendStarted(), GPUSkinCacheStreamFloatOffset, GPUSkinCacheStreamStride, GPUSkinCacheStreamBuffer))
		{
			bIsGPUCached = true;
		}
	}

	virtual uint32 GetSize() const override { return sizeof(*this); }

private:
	FShaderParameter GPUSkinCacheStreamFloatOffset;
	FShaderParameter GPUSkinCacheStreamStride;
	FShaderResourceParameter GPUSkinCacheStreamBuffer;
};

/*-----------------------------------------------------------------------------
FGPUSkinPassthroughVertexFactory
-----------------------------------------------------------------------------*/
void FGPUSkinPassthroughVertexFactory::ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
{
	Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("GPUSKIN_PASS_THROUGH"),TEXT("1"));
}

bool FGPUSkinPassthroughVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	// Passhrough is only valid on platforms with Compute Shader support
	return GEnableGPUSkinCache && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && Super::ShouldCache(Platform, Material, ShaderType);
}

FVertexFactoryShaderParameters* FGPUSkinPassthroughVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return (ShaderFrequency == SF_Vertex) ? new FGPUSkinVertexPassthroughFactoryShaderParameters() : nullptr;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FGPUSkinPassthroughVertexFactory, "GpuSkinVertexFactory", true, false, true, false, false);

/*-----------------------------------------------------------------------------
TGPUSkinMorphVertexFactory
-----------------------------------------------------------------------------*/

/**
* Modify compile environment to enable the morph blend codepath
* @param OutEnvironment - shader compile environment to modify
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinMorphVertexFactory<bExtraBoneInfluencesT>::ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
{
	Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("GPUSKIN_MORPH_BLEND"),TEXT("1"));
}

template <bool bExtraBoneInfluencesT>
bool TGPUSkinMorphVertexFactory<bExtraBoneInfluencesT>::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithMorphTargets() || Material->IsSpecialEngineMaterial()) 
		&& Super::ShouldCache(Platform, Material, ShaderType);
}

/**
* Add the decl elements for the streams
* @param InData - type with stream components
* @param OutElements - vertex decl list to modify
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinMorphVertexFactory<bExtraBoneInfluencesT>::AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements)
{
	// add the base gpu skin elements
	TGPUSkinVertexFactory<bExtraBoneInfluencesT>::AddVertexElements(InData,OutElements);
	// add the morph delta elements
	OutElements.Add(FVertexFactory::AccessStreamComponent(InData.DeltaPositionComponent,9));
	OutElements.Add(FVertexFactory::AccessStreamComponent(InData.DeltaTangentZComponent,10));
}

/**
* Creates declarations for each of the vertex stream components and
* initializes the device resource
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinMorphVertexFactory<bExtraBoneInfluencesT>::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;	
	AddVertexElements(MorphData,Elements);

	// create the actual device decls
	FVertexFactory::InitDeclaration(Elements,FVertexFactory::DataType());
}

template <bool bExtraBoneInfluencesT>
FVertexFactoryShaderParameters* TGPUSkinMorphVertexFactory<bExtraBoneInfluencesT>::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FGPUSkinVertexFactoryShaderParameters() : NULL;
}

/** bind morph target gpu skin vertex factory to its shader file and its shader parameters */
IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE(TGPUSkinMorphVertexFactory, "GpuSkinVertexFactory", true, false, true, false, false);


/*-----------------------------------------------------------------------------
	TGPUSkinAPEXClothVertexFactoryShaderParameters
-----------------------------------------------------------------------------*/
/** Shader parameters for use with TGPUSkinAPEXClothVertexFactory */
class TGPUSkinAPEXClothVertexFactoryShaderParameters : public FGPUSkinVertexFactoryShaderParameters
{
public:

	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		FGPUSkinVertexFactoryShaderParameters::Bind(ParameterMap);
		ClothSimulPositionsParameter.Bind(ParameterMap,TEXT("ClothSimulVertsPositions"));
		ClothSimulNormalsParameter.Bind(ParameterMap,TEXT("ClothSimulVertsNormals"));
		ClothBlendWeightParameter.Bind(ParameterMap, TEXT("ClothBlendWeight"));
	}
	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar) override
	{ 
		FGPUSkinVertexFactoryShaderParameters::Serialize(Ar);
		Ar << ClothSimulPositionsParameter;
		Ar << ClothSimulNormalsParameter;
		Ar << ClothBlendWeightParameter;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		if(Shader->GetVertexShader())
		{
			// Call regular GPU skinning shader parameters
			FGPUSkinVertexFactoryShaderParameters::SetMesh(RHICmdList, Shader, VertexFactory, View, BatchElement, DataFlags);
			const auto* GPUSkinVertexFactory = (const FGPUBaseSkinVertexFactory*)VertexFactory;
			// A little hacky; problem is we can't upcast from FGPUBaseSkinVertexFactory to FGPUBaseSkinAPEXClothVertexFactory as they are unrelated; a nice solution would be
			// to use virtual inheritance, but that requires RTTI and complicates things further...
			const FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType& ClothShaderData = GPUSkinVertexFactory->UsesExtraBoneInfluences()
				? ((const TGPUSkinAPEXClothVertexFactory<true>*)GPUSkinVertexFactory)->GetClothShaderData()
				: ((const TGPUSkinAPEXClothVertexFactory<false>*)GPUSkinVertexFactory)->GetClothShaderData();

			SetUniformBufferParameter(RHICmdList, Shader->GetVertexShader(),Shader->GetUniformBufferParameter<FAPEXClothUniformShaderParameters>(),ClothShaderData.GetClothUniformBuffer());

			// we tell the shader where to pickup the data
			if(ClothSimulPositionsParameter.IsBound())
			{
				RHICmdList.SetShaderResourceViewParameter(Shader->GetVertexShader(), ClothSimulPositionsParameter.GetBaseIndex(), ClothShaderData.GetClothSimulPositionBuffer().VertexBufferSRV);
			}
			
			if(ClothSimulNormalsParameter.IsBound())
			{
				RHICmdList.SetShaderResourceViewParameter(Shader->GetVertexShader(), ClothSimulNormalsParameter.GetBaseIndex(), ClothShaderData.GetClothSimulNormalBuffer().VertexBufferSRV);
			}

			SetShaderValue(
				RHICmdList,
				Shader->GetVertexShader(),
				ClothBlendWeightParameter,
				ClothShaderData.ClothBlendWeight
				);
		}
	}

protected:
	FShaderResourceParameter ClothSimulPositionsParameter;
	FShaderResourceParameter ClothSimulNormalsParameter;
	FShaderParameter ClothBlendWeightParameter;
};

/*-----------------------------------------------------------------------------
	TGPUSkinAPEXClothVertexFactory::ClothShaderType
-----------------------------------------------------------------------------*/
void FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType::UpdateClothSimulData(const TArray<FVector4>& InSimulPositions, const TArray<FVector4>& InSimulNormals, ERHIFeatureLevel::Type FeatureLevel)
{
	uint32 NumSimulVerts = InSimulPositions.Num();

	if (FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		FMath::Min(NumSimulVerts, (uint32)MAX_APEXCLOTH_VERTICES_FOR_VB);

		uint32 VectorArraySize = NumSimulVerts * sizeof(FVector4);
		uint32 PooledArraySize = ClothSimulDataBufferPool.PooledSizeForCreationArguments(VectorArraySize);
		if(!IsValidRef(ClothSimulPositionBuffer) || PooledArraySize != ClothSimulPositionBuffer.VertexBufferRHI->GetSize())
		{
			if(IsValidRef(ClothSimulPositionBuffer))
			{
				ClothSimulDataBufferPool.ReleasePooledResource(ClothSimulPositionBuffer);
			}
			ClothSimulPositionBuffer = ClothSimulDataBufferPool.CreatePooledResource(VectorArraySize);
			check(IsValidRef(ClothSimulPositionBuffer));
		}

		if(!IsValidRef(ClothSimulNormalBuffer) || PooledArraySize != ClothSimulNormalBuffer.VertexBufferRHI->GetSize())
		{
			if(IsValidRef(ClothSimulNormalBuffer))
			{
				ClothSimulDataBufferPool.ReleasePooledResource(ClothSimulNormalBuffer);
			}
			ClothSimulNormalBuffer = ClothSimulDataBufferPool.CreatePooledResource(VectorArraySize);
			check(IsValidRef(ClothSimulNormalBuffer));
		}

		if(NumSimulVerts)
		{
			float* Data = (float*)RHILockVertexBuffer(ClothSimulPositionBuffer.VertexBufferRHI, 0, VectorArraySize, RLM_WriteOnly);
			checkSlow(Data);
			FMemory::Memcpy(Data, InSimulPositions.GetData(), NumSimulVerts * sizeof(FVector4));
			RHIUnlockVertexBuffer(ClothSimulPositionBuffer.VertexBufferRHI);

			Data = (float*)RHILockVertexBuffer(ClothSimulNormalBuffer.VertexBufferRHI, 0, VectorArraySize, RLM_WriteOnly);
			checkSlow(Data);
			FMemory::Memcpy(Data, InSimulNormals.GetData(), NumSimulVerts * sizeof(FVector4));
			RHIUnlockVertexBuffer(ClothSimulNormalBuffer.VertexBufferRHI);
		}
	}
	else
	{
		UpdateClothUniformBuffer(InSimulPositions, InSimulNormals);
	}
}

/*-----------------------------------------------------------------------------
	TGPUSkinAPEXClothVertexFactory
-----------------------------------------------------------------------------*/
TGlobalResource<FClothSimulDataBufferPool> FGPUBaseSkinAPEXClothVertexFactory::ClothSimulDataBufferPool;

void FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType::UpdateClothUniformBuffer(const TArray<FVector4>& InSimulPositions, const TArray<FVector4>& InSimulNormals)
{
	FAPEXClothUniformShaderParameters ClothUniformShaderParameters;

	uint32 NumSimulVertices = InSimulPositions.Num();

	if(NumSimulVertices > 0)
	{
		NumSimulVertices = FMath::Min((uint32)MAX_APEXCLOTH_VERTICES_FOR_UB, NumSimulVertices);

		for(uint32 i=0; i<NumSimulVertices; i++)
		{
			ClothUniformShaderParameters.Positions[i] = InSimulPositions[i];
			ClothUniformShaderParameters.Normals[i] = InSimulNormals[i];
		}
	}
		
	APEXClothUniformBuffer = TUniformBufferRef<FAPEXClothUniformShaderParameters>::CreateUniformBufferImmediate(ClothUniformShaderParameters, UniformBuffer_SingleFrame);

}
/**
* Modify compile environment to enable the apex clothing path
* @param OutEnvironment - shader compile environment to modify
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinAPEXClothVertexFactory<bExtraBoneInfluencesT>::ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
{
	Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("GPUSKIN_APEX_CLOTH"),TEXT("1"));
}

template <bool bExtraBoneInfluencesT>
bool TGPUSkinAPEXClothVertexFactory<bExtraBoneInfluencesT>::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return GetMaxSupportedFeatureLevel(Platform) >= ERHIFeatureLevel::SM4
		&& (Material->IsUsedWithAPEXCloth() || Material->IsSpecialEngineMaterial()) 
		&& Super::ShouldCache(Platform, Material, ShaderType);
}

/**
* Add the decl elements for the streams
* @param InData - type with stream components
* @param OutElements - vertex decl list to modify
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinAPEXClothVertexFactory<bExtraBoneInfluencesT>::AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements)
{
	// add the base gpu skin elements
	TGPUSkinVertexFactory<bExtraBoneInfluencesT>::AddVertexElements(InData,OutElements);
	// add the morph delta elements
//	return;
	if(InData.CoordNormalComponent.VertexBuffer)
	{
		OutElements.Add(FVertexFactory::AccessStreamComponent(InData.CoordPositionComponent,9));
		OutElements.Add(FVertexFactory::AccessStreamComponent(InData.CoordNormalComponent,10));
		OutElements.Add(FVertexFactory::AccessStreamComponent(InData.CoordTangentComponent,11));
		OutElements.Add(FVertexFactory::AccessStreamComponent(InData.SimulIndicesComponent,12));
	}
}

/**
* Creates declarations for each of the vertex stream components and
* initializes the device resource
*/
template <bool bExtraBoneInfluencesT>
void TGPUSkinAPEXClothVertexFactory<bExtraBoneInfluencesT>::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;	
	AddVertexElements(MeshMappingData,Elements);

	// create the actual device decls
	FVertexFactory::InitDeclaration(Elements,FVertexFactory::DataType());
}

template <bool bExtraBoneInfluencesT>
FVertexFactoryShaderParameters* TGPUSkinAPEXClothVertexFactory<bExtraBoneInfluencesT>::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new TGPUSkinAPEXClothVertexFactoryShaderParameters() : NULL;
}

/** bind cloth gpu skin vertex factory to its shader file and its shader parameters */
IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE(TGPUSkinAPEXClothVertexFactory, "GpuSkinVertexFactory", true, false, true, false, false);


#undef IMPLEMENT_GPUSKINNING_VERTEX_FACTORY_TYPE
