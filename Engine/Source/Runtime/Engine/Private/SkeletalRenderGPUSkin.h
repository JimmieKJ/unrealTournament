// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderGPUSkin.h: GPU skinned mesh object and resource definitions
=============================================================================*/

#pragma once

#include "SkeletalRender.h"
#include "SkeletalRenderPublic.h"
#include "GPUSkinVertexFactory.h" 

// 1 for a single buffer, this creates and releases textures every frame (the driver has to keep the reference and need to defer the release, low memory as it only occupies rendered buffers (up to 3 copies), best Xbox360 method?)
// 2 for double buffering (works well for PC, caused Xbox360 to stall)
// 3 for triple buffering (works well for PC and Xbox360, wastes a bit of memory)
#define PER_BONE_BUFFER_COUNT 2

/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicSkelMeshObjectDataGPUSkin : public FDynamicSkelMeshObjectData
{
public:

	/**
	* Constructor
	* Updates the ReferenceToLocal matrices using the new dynamic data.
	* @param	InSkelMeshComponent - parent skel mesh component
	* @param	InLODIndex - each lod has its own bone map 
	* @param	InActiveVertexAnims - vertex anims active for the mesh
	*/
	FDynamicSkelMeshObjectDataGPUSkin(
		USkinnedMeshComponent* InMeshComponent,
		FSkeletalMeshResource* InSkeletalMeshResource,
		int32 InLODIndex,
		const TArray<FActiveVertexAnim>& InActiveVertexAnims
		);

	/** ref pose to local space transforms */
	TArray<FMatrix> ReferenceToLocal;

	/** origin and direction vectors for TRISORT_CustomLeftRight sections */
	TArray<FTwoVectors> CustomLeftRightVectors;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	/** component space bone transforms*/
	TArray<FTransform> MeshSpaceBases;
#endif

	/** currently LOD for bones being updated */
	int32 LODIndex;
	/** current morph targets active on this mesh */
	TArray<FActiveVertexAnim> ActiveVertexAnims;
	/** number of active vertex anims with weights > 0 */
	int32 NumWeightedActiveVertexAnims;

	/** data for updating cloth section */
	TArray<FClothSimulData> ClothSimulUpdateData;

	/** a weight factor to blend between simulated positions and skinned positions */	
	float ClothBlendWeight;

	/**
	* Compare the given set of active morph targets with the current list to check if different
	* @param CompareActiveVertexAnims - array of vertex anims to compare
	* @return true if boths sets of active vertex anims are equal
	*/
	bool ActiveVertexAnimsEqual( const TArray<FActiveVertexAnim>& CompareActiveVertexAnims );
	
	/**
	* Returns the size of memory allocated by render data
	*/
	virtual SIZE_T GetResourceSize()
	{
		SIZE_T ResourceSize = sizeof(*this);
		
		ResourceSize += ReferenceToLocal.GetAllocatedSize();
		ResourceSize += ActiveVertexAnims.GetAllocatedSize();

		return ResourceSize;
	}

	/** Update Simulated Positions & Normals from APEX Clothing actor */
	bool UpdateClothSimulationData(USkinnedMeshComponent* InMeshComponent);
};

/** morph target mesh data for a single vertex delta */
struct FMorphGPUSkinVertex
{
	FVector			DeltaPosition;
	FPackedNormal	DeltaTangentZ;

	FMorphGPUSkinVertex() {};
	
	/** Construct for special case **/
	FMorphGPUSkinVertex(const FVector& InDeltaPosition, const FPackedNormal& InDeltaTangentZ)
	{
		DeltaPosition = InDeltaPosition;
		DeltaTangentZ = InDeltaTangentZ;
	}
};

/**
* VertexAnim target vertices which have been combined into single position/tangentZ deltas
*/
class FMorphVertexBuffer : public FVertexBuffer
{
public:

	/** 
	* Constructor
	* @param	InSkelMesh - parent mesh containing the static model data for each LOD
	* @param	InLODIdx - index of LOD model to use from the parent mesh
	*/
	FMorphVertexBuffer(FSkeletalMeshResource* InSkelMeshResource, int32 InLODIdx)
		:	bHasBeenUpdated(false)	
		,	LODIdx(InLODIdx)
		,	SkelMeshResource(InSkelMeshResource)
	{
		check(SkelMeshResource);
		check(SkelMeshResource->LODModels.IsValidIndex(LODIdx));
	}
	/** 
	* Initialize the dynamic RHI for this rendering resource 
	*/
	virtual void InitDynamicRHI();

	/** 
	* Release the dynamic RHI for this rendering resource 
	*/
	virtual void ReleaseDynamicRHI();

	/** 
	* Morph target vertex name 
	*/
	virtual FString GetFriendlyName() const { return TEXT("VertexAnim target mesh vertices"); }

	/**
	 * Get Resource Size : mostly copied from InitDynamicRHI - how much they allocate when initialize
	 */
	SIZE_T GetResourceSize()
	{
		SIZE_T ResourceSize = sizeof(*this);

		if (VertexBufferRHI)
		{
			// LOD of the skel mesh is used to find number of vertices in buffer
			FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIdx];

			// Create the buffer rendering resource
			ResourceSize += LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);
		}

		return ResourceSize;
	}

	/** Has been updated or not by UpdateMorphVertexBuffer**/
	bool bHasBeenUpdated;

private:
	/** index to the SkelMeshResource.LODModels */
	int32	LODIdx;
	/** parent mesh containing the source data */
	FSkeletalMeshResource* SkelMeshResource;
};

// only used on the render thread
class FPreviousPerBoneMotionBlur
{
public:
	/** constructor */
	FPreviousPerBoneMotionBlur();

	void InitResources();

	/** 
	* call from render thread
	*/
	void ReleaseResources();

	/** */
	ENGINE_API void RestoreForPausedMotionBlur();

	void InitIfNeeded();

	/** Returns the width of the texture in pixels. */
	uint32 GetSizeX() const;

	/** so we update only during velocity rendering pass */
	bool IsAppendStarted() const;

	/** needed before AppendData() ccan be called */
	ENGINE_API void StartAppend(bool bWorldIsPaused);

	/**
	 * use between LockData() and UnlockData()
	 * @param	DataStart, must not be 0
	 * @param	BoneCount number of FBoneSkinning elements, must not be 0
	 * @return	StartIndex where the data can be referenced in the texture, 0xffffffff if this failed (not enough space in the buffer, will be fixed next frame)
	 */
	uint32 AppendData(FBoneSkinning *DataStart, uint32 BoneCount);

	/** only call if StartAppend(), if the append wasn't started it silently ignores the call */
	ENGINE_API void EndAppend();

	/** @return 0 if there should be no bone based motion blur (no previous data available or it's not active) */
	FBoneDataVertexBuffer* GetReadData();

	FString GetDebugString() const;

private:

	/** Stores the bone information with one frame delay, required for per bone motion blur. Buffered to avoid stalls on texture Lock() */
	FBoneDataVertexBuffer PerChunkBoneMatricesTexture[PER_BONE_BUFFER_COUNT];
	/** cycles between the buffers to avoid stalls (when CPU would need to wait on GPU) */
	uint32 BufferIndex;
	/* !=0 if data is locked, does not change during the lock */
	float* LockedData;
	/** only valid if LockedData != 0, advances with every Append() */
	FThreadSafeCounter LockedTexelPosition;
	/** only valid if LockedData != 0 */
	uint32 LockedTexelCount;

	bool bWarningBufferSizeExceeded;

	/** @return 0 .. PER_BONE_BUFFER_COUNT-1 */
	uint32 GetReadBufferIndex() const;

	/** @return 0 .. PER_BONE_BUFFER_COUNT-1 */
	uint32 GetWriteBufferIndex() const;

	/** to cycle the internal buffer counter */
	void AdvanceBufferIndex();
};



/**
 * Render data for a GPU skinned mesh
 */
class FSkeletalMeshObjectGPUSkin : public FSkeletalMeshObject
{
public:
	/** @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render */
	FSkeletalMeshObjectGPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FSkeletalMeshObjectGPUSkin();

	// Begin FSkeletalMeshObject interface
	virtual void InitResources() override;
	virtual void ReleaseResources() override;
	virtual void Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveVertexAnim>& ActiveVertexAnims) override;
	virtual void UpdateDynamicData_RenderThread(FRHICommandListImmediate& RHICmdList, FDynamicSkelMeshObjectData* InDynamicData) override;
	virtual const FVertexFactory* GetVertexFactory(int32 LODIndex,int32 ChunkIdx) const override;
	virtual void CacheVertices(int32 LODIndex, bool bForce) const override {}
	virtual bool IsCPUSkinned() const override { return false; }
	virtual TArray<FTransform>* GetSpaceBases() const override;

	virtual int32 GetLOD() const override
	{
		if(DynamicData)
		{
			return DynamicData->LODIndex;
		}
		else
		{
			return 0;
		}
	}

	virtual const FTwoVectors& GetCustomLeftRightVectors(int32 SectionIndex) const override;
	virtual bool HaveValidDynamicData() override
	{ 
		return ( DynamicData!=NULL ); 
	}

	virtual SIZE_T GetResourceSize() override
	{
		SIZE_T ResourceSize = sizeof(*this);
		
		if(DynamicData)
		{
			ResourceSize += DynamicData->GetResourceSize();
		}
		
		ResourceSize += LODs.GetAllocatedSize(); 

		// include extra data from LOD
		for (int32 I=0; I<LODs.Num(); ++I)
		{
			ResourceSize += LODs[I].GetResourceSize();
		}

		return ResourceSize;
	}
	// End FSkeletalMeshObject interface

	/** 
	 * Vertex buffers that can be used for GPU skinning factories 
	 */
	class FVertexFactoryBuffers
	{
	public:
		FVertexFactoryBuffers()
			: VertexBufferGPUSkin(NULL)
			, ColorVertexBuffer(NULL)
			, MorphVertexBuffer(NULL)
			, APEXClothVertexBuffer(NULL)
		{}

		FSkeletalMeshVertexBuffer* VertexBufferGPUSkin;
		FSkeletalMeshVertexColorBuffer*	ColorVertexBuffer;
		FMorphVertexBuffer* MorphVertexBuffer;
		FSkeletalMeshVertexAPEXClothBuffer*	APEXClothVertexBuffer;
	};

private:

	/**
	 * Vertex factories and their matrix arrays
	 */
	class FVertexFactoryData
	{
	public:
		FVertexFactoryData() {}

		/** one vertex factory for each chunk */
		TIndirectArray<FGPUBaseSkinVertexFactory> VertexFactories;

		/** one passthrough vertex factory for each chunk */
		TIndirectArray<FGPUBaseSkinVertexFactory> PassthroughVertexFactories;

		/** Vertex factory defining both the base mesh as well as the morph delta vertex decals */
		TIndirectArray<FGPUBaseSkinVertexFactory> MorphVertexFactories;

		/** Vertex factory defining both the base mesh as well as the APEX cloth vertex data */
		TArray<FGPUBaseSkinAPEXClothVertexFactory*> ClothVertexFactories;

		/** shared ref pose to local space matrices */
		TArray< TArray<FBoneSkinning>, TInlineAllocator<1> > PerChunkBoneMatricesArray;


		/** 
		 * Init default vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshChunk>& Chunks, ERHIFeatureLevel::Type FeatureLevel);
		/** 
		 * Release default vertex factory resources for this LOD 
		 */
		void ReleaseVertexFactories();
		/** 
		 * Init morph vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitMorphVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshChunk>& Chunks, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type InFeatureLevel);
		/** 
		 * Release morph vertex factory resources for this LOD 
		 */
		void ReleaseMorphVertexFactories();
		/** 
		 * Init APEX cloth vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitAPEXClothVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshChunk>& Chunks, ERHIFeatureLevel::Type InFeatureLevel);
		/** 
		 * Release morph vertex factory resources for this LOD 
		 */
		void ReleaseAPEXClothVertexFactories();

		/**
		 * Init one array of matrices for each chunk (shared across vertex factory types)
		 *
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitPerChunkBoneMatrices(const TArray<FSkelMeshChunk>& Chunks);
		/**
		 * Clear factory arrays
		 */
		void ClearFactories()
		{
			VertexFactories.Empty();
			MorphVertexFactories.Empty();
			ClothVertexFactories.Empty();
		}

		/**
		 * @return memory in bytes of size of the vertex factories and their matrices
		 */
		SIZE_T GetResourceSize()
		{
			SIZE_T Size = 0;
			Size += VertexFactories.GetAllocatedSize();

			Size += MorphVertexFactories.GetAllocatedSize();

			Size += ClothVertexFactories.GetAllocatedSize();

			Size += PerChunkBoneMatricesArray.GetAllocatedSize();

			return Size;
		}	
	};

	/** vertex data for rendering a single LOD */
	struct FSkeletalMeshObjectLOD
	{
		FSkeletalMeshObjectLOD(FSkeletalMeshResource* InSkelMeshResource,int32 InLOD)
		:	SkelMeshResource(InSkelMeshResource)
		,	LODIndex(InLOD)
		,	MorphVertexBuffer(InSkelMeshResource,LODIndex)
		{
		}

		/** 
		 * Init rendering resources for this LOD 
		 * @param bUseLocalVertexFactory - use non-gpu skinned factory when rendering in ref pose
		 * @param MeshLODInfo - information about the state of the bone influence swapping
		 */
		void InitResources(const FSkelMeshObjectLODInfo& MeshLODInfo, ERHIFeatureLevel::Type FeatureLevel);		

		/** 
		 * Release rendering resources for this LOD 
		 */
		void ReleaseResources();

		/** 
		 * Init rendering resources for the morph stream of this LOD
		 * @param MeshLODInfo - information about the state of the bone influence swapping
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type FeatureLevel);

		/** 
		 * Release rendering resources for the morph stream of this LOD
		 */
		void ReleaseMorphResources();

		/** 
		 * Update the contents of the vertex buffer with new data. Note that this
		 * function is called from the render thread.
		 * @param	NewVertices - array of new vertex data
		 * @param	NumVertices - Number of vertices
		 */
		void UpdateShadowVertexBuffer( const FVector* NewVertices, uint32 NumVertices ) const;

		/**
		 * @return memory in bytes of size of the resources for this LOD
		 */
		SIZE_T GetResourceSize()
		{
			SIZE_T Size = MorphVertexBuffer.GetResourceSize();

			Size += GPUSkinVertexFactories.GetResourceSize();

			return Size;
		}

		FSkeletalMeshResource* SkelMeshResource;
		int32 LODIndex;

		/** Vertex buffer that stores the morph target vertex deltas. Updated on the CPU */
		FMorphVertexBuffer MorphVertexBuffer;

		/** Default GPU skinning vertex factories and matrices */
		FVertexFactoryData GPUSkinVertexFactories;

		/**
		 * Update the contents of the vertexanim vertex buffer by accumulating all 
		 * delta positions and delta normals from the set of active vertex anims
		 * @param ActiveVertexAnims - vertex anims to accumulate. assumed to be weighted and have valid targets
		 */
		void UpdateMorphVertexBuffer( const TArray<FActiveVertexAnim>& ActiveVertexAnims );

		/**
		 * Determine the current vertex buffers valid for this LOD
		 *
		 * @param OutVertexBuffers output vertex buffers
		 */
		void GetVertexBuffers(FVertexFactoryBuffers& OutVertexBuffers,FStaticLODModel& LODModel,const FSkelMeshObjectLODInfo& MeshLODInfo);
	};

	/** 
	* Initialize morph rendering resources for each LOD 
	*/
	void InitMorphResources(bool bInUsePerBoneMotionBlur);

	/** 
	* Release morph rendering resources for each LOD. 
	*/
	void ReleaseMorphResources();

	/** Render data for each LOD */
	TArray<struct FSkeletalMeshObjectLOD> LODs;

	/** Data that is updated dynamically and is needed for rendering */
	FDynamicSkelMeshObjectDataGPUSkin* DynamicData;

	/** true if the morph resources have been initialized */
	bool bMorphResourcesInitialized;

};

// accessed on the rendering thread[s]
extern ENGINE_API FPreviousPerBoneMotionBlur GPrevPerBoneMotionBlur;
