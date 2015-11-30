// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderGPUSkin.h: GPU skinned mesh object and resource definitions
=============================================================================*/

#pragma once

#include "SkeletalRender.h"
#include "SkeletalRenderPublic.h"
#include "GPUSkinVertexFactory.h"
#include "ClothSimData.h"

/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicSkelMeshObjectDataGPUSkin
{
	/**
	* Constructor, these are recycled, so you never use a constructor
	*/
	FDynamicSkelMeshObjectDataGPUSkin()
	{
		Clear();
	}

	virtual ~FDynamicSkelMeshObjectDataGPUSkin()
	{
		// we leak these
		check(0);
	}

	void Clear();

public:

	static FDynamicSkelMeshObjectDataGPUSkin* AllocDynamicSkelMeshObjectDataGPUSkin();
	static void FreeDynamicSkelMeshObjectDataGPUSkin(FDynamicSkelMeshObjectDataGPUSkin* Who);

	/**
	* Constructor
	* Updates the ReferenceToLocal matrices using the new dynamic data.
	* @param	InSkelMeshComponent - parent skel mesh component
	* @param	InLODIndex - each lod has its own bone map 
	* @param	InActiveVertexAnims - vertex anims active for the mesh
	*/
	void InitDynamicSkelMeshObjectDataGPUSkin(
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

/**
 * Render data for a GPU skinned mesh
 */
class FSkeletalMeshObjectGPUSkin : public FSkeletalMeshObject
{
public:
	/** @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render */
	FSkeletalMeshObjectGPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FSkeletalMeshObjectGPUSkin();

	//~ Begin FSkeletalMeshObject Interface
	virtual void InitResources() override;
	virtual void ReleaseResources() override;
	virtual void Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveVertexAnim>& ActiveVertexAnims) override;
	void UpdateDynamicData_RenderThread(FRHICommandListImmediate& RHICmdList, FDynamicSkelMeshObjectDataGPUSkin* InDynamicData, uint32 FrameNumber);
	virtual void PreGDMECallback(uint32 FrameNumber) override;
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
	//~ End FSkeletalMeshObject Interface

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

	// @param FrameNumber from GFrameNumber
	void ProcessUpdatedDynamicData(FRHICommandListImmediate& RHICmdList, uint32 FrameNumber, bool bMorphNeedsUpdate);

	void WaitForRHIThreadFenceForDynamicData();

	/** Render data for each LOD */
	TArray<struct FSkeletalMeshObjectLOD> LODs;

	/** Data that is updated dynamically and is needed for rendering */
	FDynamicSkelMeshObjectDataGPUSkin* DynamicData;

	/** Fence for dynamic Data */
	FGraphEventRef RHIThreadFenceForDynamicData;

	/** True if we are doing a deferred update later in GDME. */
	bool bNeedsUpdateDeferred;

	/** If true and we are doing a deferred update, then also update the morphs */
	bool bMorphNeedsUpdateDeferred;

	/** true if the morph resources have been initialized */
	bool bMorphResourcesInitialized;

};
