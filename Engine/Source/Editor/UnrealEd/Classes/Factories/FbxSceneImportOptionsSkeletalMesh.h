// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxSceneImportOptionsSkeletalMesh.generated.h"

UCLASS(config=EditorPerProjectUserSettings, HideCategories=Object, MinimalAPI)
class UFbxSceneImportOptionsSkeletalMesh : public UObject
{
	GENERATED_UCLASS_BODY()
	

	//////////////////////////////////////////////////////////////////////////
	// Skeletal Mesh section

	/** Enable this option to update Skeleton (of the mesh)'s reference pose. Mesh's reference pose is always updated.  */
	UPROPERTY(EditAnywhere, Category = SkeletalMesh, meta = (ToolTip = "If enabled, update the Skeleton (of the mesh being imported)'s reference pose."))
	uint32 bUpdateSkeletonReferencePose : 1;

	/** If checked, create new PhysicsAsset if it doesn't have it */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = SkeletalMesh)
	uint32 bCreatePhysicsAsset : 1;

	/** Enable this option to use frame 0 as reference pose */
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh)
	uint32 bUseT0AsRefPose : 1;

	/** If checked, triangles with non-matching smoothing groups will be physically split. */
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh)
	uint32 bPreserveSmoothingGroups : 1;

	/** If checked, meshes nested in bone hierarchies will be imported instead of being converted to bones. */
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh)
	uint32 bImportMeshesInBoneHierarchy : 1;

	/** True to import morph target meshes from the FBX file */
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh, meta = (ToolTip = "If enabled, creates Unreal morph objects for the imported meshes"))
	uint32 bImportMorphTargets : 1;

	/** If checked, do not filter same vertices. Keep all vertices even if they have exact same properties*/
	UPROPERTY(EditAnywhere, config, Category = SkeletalMesh)
	uint32 bKeepOverlappingVertices : 1;


	/** Enables experimental Mikk tangent generation for skeletal meshes */
	/*UPROPERTY(EditAnywhere, Category = SkeletalMesh)
	uint32 bUseExperimentalTangentGeneration : 1;*/
};



