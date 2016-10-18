// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderCPUSkin.h: CPU skinned mesh object and resource definitions
=============================================================================*/

#pragma once

#include "SkeletalRender.h"
#include "SkeletalRenderPublic.h"
#include "LocalVertexFactory.h"

/** data for a single skinned skeletal mesh vertex */
struct FFinalSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX;
	FPackedNormal	TangentZ;
	float			U;
	float			V;
};

/**
 * Skeletal mesh vertices which have been skinned to their final positions 
 */
class FFinalSkinVertexBuffer : public FVertexBuffer
{
public:

	/** 
	 * Constructor
	 * @param	InSkelMesh - parent mesh containing the static model data for each LOD
	 * @param	InLODIdx - index of LOD model to use from the parent mesh
	 */
	FFinalSkinVertexBuffer(FSkeletalMeshResource* InSkelMeshResource, int32 InLODIdx)
	:	LODIdx(InLODIdx)
	,	SkeletalMeshResource(InSkelMeshResource)
	{
		check(SkeletalMeshResource);
		check(SkeletalMeshResource->LODModels.IsValidIndex(LODIdx));
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
	 * Cpu skinned vertex name 
	 */
	virtual FString GetFriendlyName() const { return TEXT("CPU skinned mesh vertices"); }

	/**
	 * Get Resource Size : mostly copied from InitDynamicRHI - how much they allocate when initialize
	 */
	virtual SIZE_T GetResourceSize()
	{
		// all the vertex data for a single LOD of the skel mesh
		FStaticLODModel& LodModel = SkeletalMeshResource->LODModels[LODIdx];

		return LodModel.NumVertices * sizeof(FFinalSkinVertex);
	}

private:
	/** index to the SkeletalMeshResource.LODModels */
	int32	LODIdx;
	/** parent mesh containing the source data */
	FSkeletalMeshResource* SkeletalMeshResource;

	template <bool bExtraBoneInfluencesT>
	void InitVertexData(FStaticLODModel& LodModel);
};


/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicSkelMeshObjectDataCPUSkin
{
public:

	/**
	* Constructor
	* Updates the ReferenceToLocal matrices using the new dynamic data.
	* @param	InSkelMeshComponent - parent skel mesh component
	* @param	InLODIndex - each lod has its own bone map 
	* @param	InActiveMorphTargets - Active Morph Targets to blend with during skinning
	* @param	InMorphTargetWeights - All Morph Target weights to blend with during skinning
	*/
	FDynamicSkelMeshObjectDataCPUSkin(
		USkinnedMeshComponent* InMeshComponent,
		FSkeletalMeshResource* InSkeletalMeshResource,
		int32 InLODIndex,
		const TArray<FActiveMorphTarget>& InActiveMorphTargets,
		const TArray<float>& InMorphTargetWeights
		);

	virtual ~FDynamicSkelMeshObjectDataCPUSkin()
	{
	}

	/** ref pose to local space transforms */
	TArray<FMatrix> ReferenceToLocal;

	/** origin and direction vectors for TRISORT_CustomLeftRight sections */
	TArray<FTwoVectors> CustomLeftRightVectors;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	/** component space bone transforms*/
	TArray<FTransform> MeshComponentSpaceTransforms;
#endif
	/** currently LOD for bones being updated */
	int32 LODIndex;
	/** Morphs to blend when skinning verts */
	TArray<FActiveMorphTarget> ActiveMorphTargets;
	/** Morph Weights to blend when skinning verts */
	TArray<float> MorphTargetWeights;

	/**
	* Returns the size of memory allocated by render data
	*/
	virtual SIZE_T GetResourceSize()
 	{
		SIZE_T ResourceSize = sizeof(*this);
 		
 		ResourceSize += ReferenceToLocal.GetAllocatedSize();
 		ResourceSize += ActiveMorphTargets.GetAllocatedSize();
 		
		return ResourceSize;
 	}
};

/**
 * Render data for a CPU skinned mesh
 */
class FSkeletalMeshObjectCPUSkin : public FSkeletalMeshObject
{
public:

	/** @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render */
	FSkeletalMeshObjectCPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FSkeletalMeshObjectCPUSkin();

	//~ Begin FSkeletalMeshObject Interface
	virtual void InitResources() override;
	virtual void ReleaseResources() override;
	virtual void Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetsWeights) override;
	void UpdateDynamicData_RenderThread(FRHICommandListImmediate& RHICmdList, FDynamicSkelMeshObjectDataCPUSkin* InDynamicData, uint32 FrameNumberToPrepare);
	virtual void UpdateRecomputeTangent(int32 MaterialIndex, bool bRecomputeTangent) override;
	virtual void EnableBlendWeightRendering(bool bEnabled, const TArray<int32>& InBonesOfInterest) override;
	virtual void CacheVertices(int32 LODIndex, bool bForce) const override;
	virtual bool IsCPUSkinned() const override { return true; }
	virtual const FVertexFactory* GetSkinVertexFactory(const FSceneView* View, int32 LODIndex, int32 ChunkIdx) const override;
	virtual TArray<FTransform>* GetComponentSpaceTransforms() const override;
	virtual const TArray<FMatrix>& GetReferenceToLocalMatrices() const override;
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
		SIZE_T ResourceSize=sizeof(*this);

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

		ResourceSize += CachedFinalVertices.GetAllocatedSize();
		ResourceSize += BonesOfInterest.GetAllocatedSize();

		return ResourceSize;
	}

	virtual void DrawVertexElements(FPrimitiveDrawInterface* PDI, const FTransform& ToWorldSpace, bool bDrawNormals, bool bDrawTangents, bool bDrawBinormals) const override;
	//~ End FSkeletalMeshObject Interface

private:
	/** vertex data for rendering a single LOD */
	struct FSkeletalMeshObjectLOD
	{
		FLocalVertexFactory				VertexFactory;
		mutable FFinalSkinVertexBuffer	VertexBuffer;

		/** true if resources for this LOD have already been initialized. */
		bool						bResourcesInitialized;

		FSkeletalMeshObjectLOD(FSkeletalMeshResource* InSkelMeshResource,int32 InLOD)
		:	VertexBuffer(InSkelMeshResource,InLOD)
		,	bResourcesInitialized( false )
		{
		}
		/** 
		 * Init rendering resources for this LOD 
		 */
		void InitResources();
		/** 
		 * Release rendering resources for this LOD 
		 */
		void ReleaseResources();
		/** 
		 * Update the contents of the vertex buffer with new data
		 * @param	NewVertices - array of new vertex data
		 * @param	Size - size of new vertex data aray 
		 */
		void UpdateFinalSkinVertexBuffer(void* NewVertices, uint32 Size) const;

		/**
	 	 * Get Resource Size : return the size of Resource this allocates
	 	 */
		SIZE_T GetResourceSize()
		{
			return VertexBuffer.GetResourceSize();
		}
	};

	/** Render data for each LOD */
	TArray<struct FSkeletalMeshObjectLOD> LODs;

	/** Data that is updated dynamically and is needed for rendering */
	class FDynamicSkelMeshObjectDataCPUSkin* DynamicData;

 	/** Index of LOD level's vertices that are currently stored in CachedFinalVertices */
 	mutable int32	CachedVertexLOD;

 	/** Cached skinned vertices. Only updated/accessed by the rendering thread */
 	mutable TArray<FFinalSkinVertex> CachedFinalVertices;

	/** Array of bone's to render bone weights for */
	TArray<int32> BonesOfInterest;

	/** Bone weight viewing in editor */
	bool bRenderBoneWeight;
};

