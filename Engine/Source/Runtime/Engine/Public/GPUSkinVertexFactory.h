// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUSkinVertexFactory.h: GPU skinning vertex factory definitions.
	
	This code contains embedded portions of source code from dqconv.c Conversion routines between (regular quaternion, translation) and dual quaternion, Version 1.0.0, Copyright (c) 2006-2007 University of Dublin, Trinity College, All Rights Reserved, which have been altered from their original version.

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

#ifndef __GPUSKINVERTEXFACTORY_H__
#define __GPUSKINVERTEXFACTORY_H__

#include "GPUSkinPublicDefs.h"
#include "ResourcePool.h"
#include "UniformBuffer.h"

/** for final bone matrices - this needs to move out of ifdef due to APEX using it*/
MS_ALIGN(16) struct FSkinMatrix3x4
{
	float M[3][4];
	FORCEINLINE void SetMatrix(const FMatrix& Mat)
	{
		const float* RESTRICT Src = &(Mat.M[0][0]);
		float* RESTRICT Dest = &(M[0][0]);

		Dest[0] = Src[0];   // [0][0]
		Dest[1] = Src[1];   // [0][1]
		Dest[2] = Src[2];   // [0][2]
		Dest[3] = Src[3];   // [0][3]

		Dest[4] = Src[4];   // [1][0]
		Dest[5] = Src[5];   // [1][1]
		Dest[6] = Src[6];   // [1][2]
		Dest[7] = Src[7];   // [1][3]

		Dest[8] = Src[8];   // [2][0]
		Dest[9] = Src[9];   // [2][1]
		Dest[10] = Src[10]; // [2][2]
		Dest[11] = Src[11]; // [2][3]
	}

	FORCEINLINE void SetMatrixTranspose(const FMatrix& Mat)
	{

		const float* RESTRICT Src = &(Mat.M[0][0]);
		float* RESTRICT Dest = &(M[0][0]);

		Dest[0] = Src[0];   // [0][0]
		Dest[1] = Src[4];   // [1][0]
		Dest[2] = Src[8];   // [2][0]
		Dest[3] = Src[12];  // [3][0]

		Dest[4] = Src[1];   // [0][1]
		Dest[5] = Src[5];   // [1][1]
		Dest[6] = Src[9];   // [2][1]
		Dest[7] = Src[13];  // [3][1]

		Dest[8] = Src[2];   // [0][2]
		Dest[9] = Src[6];   // [1][2]
		Dest[10] = Src[10]; // [2][2]
		Dest[11] = Src[14]; // [3][2]
	}
} GCC_ALIGN(16);

template<>
class TUniformBufferTypeInfo<FSkinMatrix3x4>
{
public:
	enum { BaseType = UBMT_FLOAT32 };
	enum { NumRows = 3 };
	enum { NumColumns = 4 };
	enum { NumElements = 0 };
	enum { Alignment = 16 };
	enum { IsResource = 0 };
	static const FUniformBufferStruct* GetStruct() { return NULL; }
};

// Uniform buffer for APEX cloth (for now) buffer limitation is up to 64kb
BEGIN_UNIFORM_BUFFER_STRUCT(FAPEXClothUniformShaderParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector,Positions,[MAX_APEXCLOTH_VERTICES_FOR_UB])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector,Normals,[MAX_APEXCLOTH_VERTICES_FOR_UB])
END_UNIFORM_BUFFER_STRUCT(FAPEXClothUniformShaderParameters)

enum
{
	MAX_GPU_BONE_MATRICES_UNIFORMBUFFER = 75,
};
BEGIN_UNIFORM_BUFFER_STRUCT(FBoneMatricesUniformShaderParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FSkinMatrix3x4, BoneMatrices, [MAX_GPU_BONE_MATRICES_UNIFORMBUFFER])
END_UNIFORM_BUFFER_STRUCT(FBoneMatricesUniformShaderParameters)

typedef FSkinMatrix3x4 FBoneSkinning;

#define SET_BONE_DATA(B, X) B.SetMatrixTranspose(X)

/** Shared data & implementation for the different types of pool */
class FSharedPoolPolicyData
{
public:
	/** Buffers are created with a simple byte size */
	typedef uint32 CreationArguments;
	enum
	{
		NumSafeFrames = 3, /** Number of frames to leaves buffers before reclaiming/reusing */
		NumPoolBucketSizes = 16, /** Number of pool buckets */
		NumToDrainPerFrame = 10, /** Max. number of resources to cull in a single frame */
		CullAfterFramesNum = 30 /** Resources are culled if unused for more frames than this */
	};
	
	/** Get the pool bucket index from the size
	 * @param Size the number of bytes for the resource 
	 * @returns The bucket index.
	 */
	uint32 GetPoolBucketIndex(uint32 Size);
	
	/** Get the pool bucket size from the index
	 * @param Bucket the bucket index
	 * @returns The bucket size.
	 */
	uint32 GetPoolBucketSize(uint32 Bucket);
	
private:
	/** The bucket sizes */
	static uint32 BucketSizes[NumPoolBucketSizes];
};

/** Struct to pool the vertex buffer & SRV together */
struct FBoneBuffer
{
	void SafeRelease()
    {
        VertexBufferRHI.SafeRelease();
        VertexBufferSRV.SafeRelease();
    }

    FVertexBufferRHIRef VertexBufferRHI;
    FShaderResourceViewRHIRef VertexBufferSRV;
};

/** Helper function to test whether the buffer is valid.
 * @param BoneBuffer Buffer to test
 * @returns True if the buffer is valid otherwise false
 */
inline bool IsValidRef(const FBoneBuffer& BoneBuffer)
{
    return IsValidRef(BoneBuffer.VertexBufferRHI) && IsValidRef(BoneBuffer.VertexBufferSRV);
}

/** The type for bone buffers */
typedef FBoneBuffer FBoneBufferTypeRef;

/** The policy for pooling bone vertex buffers */
class FBoneBufferPoolPolicy : public FSharedPoolPolicyData
{
public:
	enum
	{
		NumSafeFrames = FSharedPoolPolicyData::NumSafeFrames,
		NumPoolBuckets = FSharedPoolPolicyData::NumPoolBucketSizes,
		NumToDrainPerFrame = FSharedPoolPolicyData::NumToDrainPerFrame,
		CullAfterFramesNum = FSharedPoolPolicyData::CullAfterFramesNum
	};
	/** Creates the resource 
	 * @param Args The buffer size in bytes.
	 * @returns A suitably sized buffer or NULL on failure.
	 */
	FBoneBufferTypeRef CreateResource(FSharedPoolPolicyData::CreationArguments Args);
	
	/** Gets the arguments used to create resource
	 * @param Resource The buffer to get data for.
	 * @returns The arguments used to create the buffer.
	 */
	FSharedPoolPolicyData::CreationArguments GetCreationArguments(FBoneBufferTypeRef Resource);
};

/** A pool for vertex buffers with consistent usage, bucketed for efficiency. */
class FBoneBufferPool : public TRenderResourcePool<FBoneBufferTypeRef, FBoneBufferPoolPolicy, FSharedPoolPolicyData::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FBoneBufferPool();
	
public: // From FTickableObjectRenderThread
	virtual TStatId GetStatId() const override;
};

/** The type for the buffer pool */
typedef FBoneBufferPool FBoneBufferPool;

/** for motion blur skinning */
class FBoneDataVertexBuffer : public FRenderResource
{
public:
	/** constructor */
	FBoneDataVertexBuffer();

	/** 
	* call UnlockData() after this one
	* @return never 0
	*/
	float* LockData();
	/**
	* Needs to be called after LockData()
	*/
	void UnlockData(uint32 SizeInBytes);

	/** Returns the size of the buffer in pixels. */
	uint32 GetSizeX() const;

	bool IsValid() const
	{
		return IsValidRef(BoneBuffer);
	}

	// interface FRenderResource ------------------------------------------

	virtual void ReleaseRHI() override
	{
		DEC_DWORD_STAT_BY( STAT_SkeletalMeshMotionBlurSkinningMemory, ComputeMemorySize());
		BoneBuffer.SafeRelease();
		FRenderResource::ReleaseRHI();
	}

	virtual void InitDynamicRHI() override
	{
		if(SizeX)
		{
			INC_DWORD_STAT_BY( STAT_SkeletalMeshMotionBlurSkinningMemory, ComputeMemorySize());
			const int32 TileBufferSize = ComputeMemorySize();
			FRHIResourceCreateInfo CreateInfo;
			// BUF_Dynamic as it can be too large (e.g. 100 characters each 100 bones with 48 bytes per bone is about 1/2 MB) to do it as BUF_Volatile
			BoneBuffer.VertexBufferRHI = RHICreateVertexBuffer( TileBufferSize, BUF_Dynamic | BUF_ShaderResource, CreateInfo );
			BoneBuffer.VertexBufferSRV = RHICreateShaderResourceView( BoneBuffer.VertexBufferRHI, sizeof(FVector4), PF_A32B32G32R32F );
		}
	}
	
	/** Bone buffer reference. */
	FBoneBufferTypeRef BoneBuffer;
	
private: // -------------------------------------------------------
	/** Buffer size in texels */
	uint32 SizeX;

	/** Buffer size, as allocated */
	uint32 AllocSize;

	/** Allocated Buffer */
	void *AllocBlock;

	/** @return in bytes */
	uint32 ComputeMemorySize();
};

/** Vertex factory with vertex stream components for GPU skinned vertices */
class FGPUBaseSkinVertexFactory : public FVertexFactory
{
public:
	struct ShaderDataType
	{
		/**
		 * Constructor, presizing bones array.
		 *
		 * @param	InBoneMatrices	Reference to shared bone matrices array.
		 */
		ShaderDataType(
			TArray<FBoneSkinning>& InBoneMatrices
			)
		:	BoneMatrices( InBoneMatrices )
		{
			OldBoneDataStartIndex[0] = 0xffffffff;
			OldBoneDataStartIndex[1] = 0xffffffff;

			// BoneDataOffset and BoneTextureSize are not set as they are only valid if IsValidRef(BoneTexture)
			if(!MaxBonesVar)
			{
				MaxBonesVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("Compat.MAX_GPUSKIN_BONES"));
				MaxGPUSkinBones = MaxBonesVar->GetValueOnGameThread();
				check(MaxGPUSkinBones <= 256);
			}
		}

		/** Reference to shared ref pose to local space matrices */
		TArray<FBoneSkinning>& BoneMatrices;

		/** Mesh origin and Mesh Extension for Mesh compressions **/
		/** This value will be (0, 0, 0), (1, 1, 1) relatively for non compressed meshes **/
		FVector MeshOrigin;
		FVector MeshExtension;

		/** @return 0xffffffff means not valid */
		uint32 GetOldBoneData(uint32 InFrameNumber) const
		{
			// will also work in the wrap around case
			uint32 LastFrameNumber = InFrameNumber - 1;

			// pick the data with the right frame number
			if(OldBoneFrameNumber[0] == LastFrameNumber)
			{
				// [0] has the right data
				return OldBoneDataStartIndex[0];
			}
			else if(OldBoneFrameNumber[1] == LastFrameNumber)
			{
				// [1] has the right data
				return OldBoneDataStartIndex[1];
			}
			else
			{
				// all data stored is too old
				return 0xffffffff;
			}
		}

		/** Set the data with the given frame number, keeps data for the last frame. */
		void SetOldBoneData(uint32 FrameNumber, uint32 Index) const
		{
			// keep the one from last from, someone might read want to read that
			if (OldBoneFrameNumber[0] + 1 == FrameNumber)
			{
				// [1] has the right data
				OldBoneFrameNumber[1] = FrameNumber;
				OldBoneDataStartIndex[1] = Index;
			}
			else 
			{
				// [0] has the right data
				OldBoneFrameNumber[0] = FrameNumber;
				OldBoneDataStartIndex[0] = Index;
			}
		}

		/** Checks if we need to update the data for this frame */
		bool IsOldBoneDataUpdateNeeded(uint32 InFrameNumber) const
		{
			if(OldBoneFrameNumber[0] == InFrameNumber
			|| OldBoneFrameNumber[1] == InFrameNumber)
			{
				return false;
			}

			return true;
		}

		void UpdateBoneData(ERHIFeatureLevel::Type FeatureLevel);

		void ReleaseBoneData()
		{
			UniformBuffer.SafeRelease();
            if(IsValidRef(BoneBuffer))
            {
                BoneBufferPool.ReleasePooledResource(BoneBuffer);
            }
            BoneBuffer.SafeRelease();
		}
		
		FUniformBufferRHIParamRef GetUniformBuffer() const
		{
			return UniformBuffer;
		}
		
		FBoneBufferTypeRef GetBoneBuffer() const
		{
			return BoneBuffer;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FString GetDebugString(const FVertexFactory* VertexFactory, uint32 OldBoneDataIndex) const
		{
			return FString::Printf(TEXT("r.MotionBlurDebug: OldBoneDataIndex[%p] (%d:%d %d:%d), => %d"),
				VertexFactory,
				OldBoneFrameNumber[0], OldBoneDataStartIndex[0],
				OldBoneFrameNumber[1], OldBoneDataStartIndex[1],
				OldBoneDataIndex);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		mutable FCriticalSection OldBoneDataLock;
private:

		// the following members can be stored in less bytes (after some adjustments)

		/** Start bone index in BoneTexture valid means != 0xffffffff */
		mutable uint32 OldBoneDataStartIndex[2];
		/** FrameNumber from the view when the data was set, only valid if OldBoneDataStartIndex != 0xffffffff */
		mutable uint32 OldBoneFrameNumber[2];

		FBoneBufferTypeRef BoneBuffer;
		FUniformBufferRHIRef UniformBuffer;
		
		static TConsoleVariableData<int32>* MaxBonesVar;
		static uint32 MaxGPUSkinBones;
	};

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	FGPUBaseSkinVertexFactory(TArray<FBoneSkinning>& InBoneMatrices, ERHIFeatureLevel::Type InFeatureLevel)
	:	FVertexFactory(InFeatureLevel), ShaderData( InBoneMatrices )
	{}

	/** accessor */
	FORCEINLINE ShaderDataType& GetShaderData()
	{
		return ShaderData;
	}

	FORCEINLINE const ShaderDataType& GetShaderData() const
	{
		return ShaderData;
	}

	virtual bool UsesExtraBoneInfluences() const = 0;

	/**
	 * Set the data with the given frame number, keeps data for the last frame.
	 * @param Index 0xffffffff to disable motion blur skinning
	 */
	void SetOldBoneDataStartIndex(uint32 FrameNumber, uint32 Index) const
	{
		ShaderData.SetOldBoneData(FrameNumber, Index);
	}

	static bool SupportsTessellationShaders() { return true; }

	/** Checks if we need to update the data for this frame */
	bool IsOldBoneDataUpdateNeeded(uint32 InFrameNumber) const
	{
		return ShaderData.IsOldBoneDataUpdateNeeded(InFrameNumber);
	}

	const FVertexBuffer* GetSkinVertexBuffer() const
	{
		return Streams[0].VertexBuffer;
	}

protected:
	/** dynamic data need for setting the shader */ 
	ShaderDataType ShaderData;
	/** Pool of buffers for bone matrices. */
	static TGlobalResource<FBoneBufferPool> BoneBufferPool;
};

/** Vertex factory with vertex stream components for GPU skinned vertices */
template<bool bExtraBoneInfluencesT>
class TGPUSkinVertexFactory : public FGPUBaseSkinVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(TGPUSkinVertexFactory<bExtraBoneInfluencesT>);

public:

	enum
	{
		HasExtraBoneInfluences = bExtraBoneInfluencesT,
	};

	struct DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		TArray<FVertexStreamComponent,TFixedAllocator<MAX_TEXCOORDS> > TextureCoordinates;

		/** The stream to read the vertex color from. */
		FVertexStreamComponent ColorComponent;

		/** The stream to read the bone indices from */
		FVertexStreamComponent BoneIndices;

		/** The stream to read the extra bone indices from */
		FVertexStreamComponent ExtraBoneIndices;

		/** The stream to read the bone weights from */
		FVertexStreamComponent BoneWeights;

		/** The stream to read the extra bone weights from */
		FVertexStreamComponent ExtraBoneWeights;
	};

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	TGPUSkinVertexFactory(TArray<FBoneSkinning>& InBoneMatrices, ERHIFeatureLevel::Type InFeatureLevel)
		: FGPUBaseSkinVertexFactory(InBoneMatrices, InFeatureLevel)
	{}

	virtual bool UsesExtraBoneInfluences() const override
	{
		return bExtraBoneInfluencesT;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType);
	
	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	void SetData(const DataType& InData)
	{
		Data = InData;
		FGPUBaseSkinVertexFactory::UpdateRHI();
	}

	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	void AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements);

private:
	/** stream component data bound to this vertex factory */
	DataType Data;  
};

/** 
 * Vertex factory with vertex stream components for GPU-skinned streams, enabled for passthrough mode when vertices have been pre-skinned 
 */
class FGPUSkinPassthroughVertexFactory : public TGPUSkinVertexFactory<false>
{
	DECLARE_VERTEX_FACTORY_TYPE(FGPUSkinPassthroughVertexFactory);

	typedef TGPUSkinVertexFactory<false> Super;

public:
	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	FGPUSkinPassthroughVertexFactory(TArray<FBoneSkinning>& InBoneMatrices, ERHIFeatureLevel::Type InFeatureLevel)
		: TGPUSkinVertexFactory<false>(InBoneMatrices, InFeatureLevel)
	{}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType);

	// FRenderResource interface.
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
template<bool bExtraBoneInfluencesT>
class TGPUSkinMorphVertexFactory : public TGPUSkinVertexFactory<bExtraBoneInfluencesT>
{
	DECLARE_VERTEX_FACTORY_TYPE(TGPUSkinMorphVertexFactory<bExtraBoneInfluencesT>);

	typedef TGPUSkinVertexFactory<bExtraBoneInfluencesT> Super;
public:

	struct DataType : TGPUSkinVertexFactory<bExtraBoneInfluencesT>::DataType
	{
		/** stream which has the position deltas to add to the vertex position */
		FVertexStreamComponent DeltaPositionComponent;
		/** stream which has the TangentZ deltas to add to the vertex normals */
		FVertexStreamComponent DeltaTangentZComponent;
	};

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	TGPUSkinMorphVertexFactory(TArray<FBoneSkinning>& InBoneMatrices, ERHIFeatureLevel::Type InFeatureLevel)
	: TGPUSkinVertexFactory<bExtraBoneInfluencesT>(InBoneMatrices, InFeatureLevel)
	{}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType);
	
	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	void SetData(const DataType& InData)
	{
		MorphData = InData;
		FGPUBaseSkinVertexFactory::UpdateRHI();
	}

	// FRenderResource interface.

	/**
	* Creates declarations for each of the vertex stream components and
	* initializes the device resource
	*/
	virtual void InitRHI() override;

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	void AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements);

private:
	/** stream component data bound to this vertex factory */
	DataType MorphData; 

};

// to reuse BoneBuffer structure for clothing simulation data
typedef FBoneBufferTypeRef FClothSimulDataBufferTypeRef;
typedef FBoneBufferPool FClothSimulDataBufferPool;

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
class FGPUBaseSkinAPEXClothVertexFactory
{
public:
	struct ClothShaderType
	{
		/**
		 * weight to blend between simulated positions and key-framed poses
		 * if ClothBlendWeight is 1.0, it shows only simulated positions and if it is 0.0, it shows only key-framed animation
		 */
		float ClothBlendWeight;

		void UpdateClothUniformBuffer(const TArray<FVector4>& InSimulPositions, const TArray<FVector4>& InSimulNormals);

		void UpdateClothSimulData(const TArray<FVector4>& InSimulPositions, const TArray<FVector4>& InSimulNormals, ERHIFeatureLevel::Type FeatureLevel);

		void ReleaseClothSimulData()
		{
			APEXClothUniformBuffer.SafeRelease();

			if(IsValidRef(ClothSimulPositionBuffer))
			{
				ClothSimulDataBufferPool.ReleasePooledResource(ClothSimulPositionBuffer);
			}
			ClothSimulPositionBuffer.SafeRelease();

			if(IsValidRef(ClothSimulNormalBuffer))
			{
				ClothSimulDataBufferPool.ReleasePooledResource(ClothSimulNormalBuffer);
			}
			ClothSimulNormalBuffer.SafeRelease();
		}

		TUniformBufferRef<FAPEXClothUniformShaderParameters> GetClothUniformBuffer() const
		{
			return APEXClothUniformBuffer;
		}

		FClothSimulDataBufferTypeRef GetClothSimulPositionBuffer() const
		{
			return ClothSimulPositionBuffer;
		}

		FClothSimulDataBufferTypeRef GetClothSimulNormalBuffer() const
		{
			return ClothSimulNormalBuffer;
		}

	private:
		TUniformBufferRef<FAPEXClothUniformShaderParameters> APEXClothUniformBuffer;
		FClothSimulDataBufferTypeRef ClothSimulPositionBuffer;
		FClothSimulDataBufferTypeRef ClothSimulNormalBuffer;

	};


	/** accessor */
	FORCEINLINE ClothShaderType& GetClothShaderData()
	{
		return ClothShaderData;
	}

	FORCEINLINE const ClothShaderType& GetClothShaderData() const
	{
		return ClothShaderData;
	}

	virtual FGPUBaseSkinVertexFactory* GetVertexFactory() = 0;
	virtual const FGPUBaseSkinVertexFactory* GetVertexFactory() const = 0;

protected:
	ClothShaderType ClothShaderData;

	/** Pool of buffers for clothing simulation data */
	static TGlobalResource<FClothSimulDataBufferPool> ClothSimulDataBufferPool;
};

/** Vertex factory with vertex stream components for GPU-skinned and morph target streams */
template<bool bExtraBoneInfluencesT>
class TGPUSkinAPEXClothVertexFactory : public FGPUBaseSkinAPEXClothVertexFactory, public TGPUSkinVertexFactory<bExtraBoneInfluencesT>
{
	DECLARE_VERTEX_FACTORY_TYPE(TGPUSkinAPEXClothVertexFactory<bExtraBoneInfluencesT>);

	typedef TGPUSkinVertexFactory<bExtraBoneInfluencesT> Super;

public:

	struct DataType : TGPUSkinVertexFactory<bExtraBoneInfluencesT>::DataType
	{
		/** stream which has the physical mesh position + height offset */
		FVertexStreamComponent CoordPositionComponent;
		/** stream which has the physical mesh coordinate for normal + offset */
		FVertexStreamComponent CoordNormalComponent;
		/** stream which has the physical mesh coordinate for tangent + offset */
		FVertexStreamComponent CoordTangentComponent;
		/** stream which has the physical mesh vertex indices */
		FVertexStreamComponent SimulIndicesComponent;
	};

	/**
	 * Constructor presizing bone matrices array to used amount.
	 *
	 * @param	InBoneMatrices	Reference to shared bone matrices array.
	 */
	TGPUSkinAPEXClothVertexFactory(TArray<FBoneSkinning>& InBoneMatrices, ERHIFeatureLevel::Type InFeatureLevel)
		: TGPUSkinVertexFactory<bExtraBoneInfluencesT>(InBoneMatrices, InFeatureLevel)
	{}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const FShaderType* ShaderType);

	/**
	* An implementation of the interface used by TSynchronizedResource to 
	* update the resource with new data from the game thread.
	* @param	InData - new stream component data
	*/
	void SetData(const DataType& InData)
	{
		MeshMappingData = InData;
		FGPUBaseSkinVertexFactory::UpdateRHI();
	}

	virtual FGPUBaseSkinVertexFactory* GetVertexFactory() override
	{
		return this;
	}

	virtual const FGPUBaseSkinVertexFactory* GetVertexFactory() const override
	{
		return this;
	}

	// FRenderResource interface.

	/**
	* Creates declarations for each of the vertex stream components and
	* initializes the device resource
	*/
	virtual void InitRHI() override;
	virtual void ReleaseDynamicRHI() override;

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/**
	* Add the decl elements for the streams
	* @param InData - type with stream components
	* @param OutElements - vertex decl list to modify
	*/
	void AddVertexElements(DataType& InData, FVertexDeclarationElementList& OutElements);

private:
	/** stream component data bound to this vertex factory */
	DataType MeshMappingData; 
};

#endif // __GPUSKINVERTEXFACTORY_H__
