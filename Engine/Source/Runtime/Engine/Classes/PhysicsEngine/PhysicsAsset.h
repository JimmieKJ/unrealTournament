// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigidBodyIndexPair.h"
#include "PhysicsAsset.generated.h"

/**
 * PhysicsAsset contains a set of rigid bodies and constraints that make up a single ragdoll.
 * The asset is not limited to human ragdolls, and can be used for any physical simulation using bodies and constraints.
 * A SkeletalMesh has a single PhysicsAsset, which allows for easily turning ragdoll physics on or off for many SkeletalMeshComponents
 * The asset can be configured inside the Physics Asset Editor (PhAT).
 *
 * @see https://docs.unrealengine.com/latest/INT/Engine/Physics/PhAT/Reference/index.html
 * @see USkeletalMesh
 */

UCLASS(hidecategories=Object, BlueprintType, MinimalAPI)
class UPhysicsAsset : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** 
	 *	Default skeletal mesh to use when previewing this PhysicsAsset etc. 
	 *	Is the one that was used as the basis for creating this Asset.
	 */
	UPROPERTY()
	class USkeletalMesh * DefaultSkelMesh_DEPRECATED;

	UPROPERTY()
	TAssetPtr<class USkeletalMesh> PreviewSkeletalMesh;

#endif // WITH_EDITORONLY_DATA
	/** 
	 *	Array of BodySetup objects. Stores information about collision shape etc. for each body.
	 *	Does not include body position - those are taken from mesh.
	 */
	UPROPERTY(instanced)
	TArray<class UBodySetup*> BodySetup;

	/** Index of bodies that are marked bConsiderForBounds */
	UPROPERTY()
	TArray<int32> BoundsBodies;

	/** 
	 *	Array of RB_ConstraintSetup objects. 
	 *	Stores information about a joint between two bodies, such as position relative to each body, joint limits etc.
	 */
	UPROPERTY(instanced)
	TArray<class UPhysicsConstraintTemplate*> ConstraintSetup;

public:
	/** This caches the BodySetup Index by BodyName to speed up FindBodyIndex */
	TMap<FName, int32>					BodySetupIndexMap;

	/** 
	 *	Table indicating which pairs of bodies have collision disabled between them. Used internally. 
	 *	Note, this is accessed from within physics engine, so is not safe to change while physics is running
	 */
	TMap<FRigidBodyIndexPair,bool>		CollisionDisableTable;

	// Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual FString GetDesc() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif
	// End UObject interface

	// Find the index of the physics bone that is controlling this graphics bone.
	ENGINE_API int32		FindControllingBodyIndex(class USkeletalMesh* skelMesh, int32 BoneIndex);
	ENGINE_API int32		FindParentBodyIndex(class USkeletalMesh * skelMesh, int32 StartBoneIndex) const;
	ENGINE_API int32		FindConstraintIndex(FName ConstraintName);
	FName					FindConstraintBoneName(int32 ConstraintIndex);
	ENGINE_API int32		FindMirroredBone(class USkeletalMesh* skelMesh, int32 BoneIndex);

	/** Utility for getting indices of all bodies below (and including) the one with the supplied name. */
	ENGINE_API void			GetBodyIndicesBelow(TArray<int32>& OutBodyIndices, FName InBoneName, USkeletalMesh* InSkelMesh, bool bIncludeParent = true);

	ENGINE_API void			GetNearestBodyIndicesBelow(TArray<int32> & OutBodyIndices, FName InBoneName, USkeletalMesh * InSkelMesh);

	ENGINE_API FBox			CalcAABB(const class USkinnedMeshComponent* MeshComponent, const FTransform& LocalToWorld) const;

	/** Clears physics meshes from all bodies */
	ENGINE_API void ClearAllPhysicsMeshes();
	
#if WITH_EDITOR
	/** Invalidates physics meshes from all bodies. Data will be rebuilt completely. */
	ENGINE_API void InvalidateAllPhysicsMeshes();
#endif

	// @todo document
	void GetCollisionMesh(int32 ViewIndex, FMeshElementCollector& Collector, const USkeletalMesh* SkelMesh, const TArray<FTransform>& SpaceBases, const FTransform& LocalToWorld, float Scale);

	// @todo document
	void DrawConstraints(class FPrimitiveDrawInterface* PDI, const USkeletalMesh* SkelMesh, const TArray<FTransform>& SpaceBases, const FTransform& LocalToWorld, float Scale);


	// @todo document
	ENGINE_API void DisableCollision(int32 BodyIndexA, int32 BodyIndexB);

	// @todo document
	ENGINE_API void EnableCollision(int32 BodyIndexA, int32 BodyIndexB);

	/** Update the BoundsBodies array and cache the indices of bodies marked with bConsiderForBounds to BoundsBodies array. */
	ENGINE_API void UpdateBoundsBodiesArray();

	/** Update the BodySetup Array Index Map.  */
	ENGINE_API void UpdateBodySetupIndexMap();


	// @todo document
	ENGINE_API int32 FindBodyIndex(FName BodyName) const;

	/** Find all the constraints that are connected to a particular body.
	 * 
	 * @param	BodyIndex		The index of the body to find the constraints for
	 * @param	Constraints		Returns the found constraints
	 **/
	ENGINE_API void BodyFindConstraints(int32 BodyIndex, TArray<int32>& Constraints);
};

#define		RB_MinSizeToLockDOF				(0.1)
#define		RB_MinAngleToLockDOF			(5.0)