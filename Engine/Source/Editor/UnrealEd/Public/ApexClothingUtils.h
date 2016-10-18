// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#ifndef __ApexClothingUtils_h__
#define __ApexClothingUtils_h__

class USkeletalMeshComponent;
class FPhysScene;
struct FClothPhysicsProperties;

#if WITH_APEX_CLOTHING

namespace physx
{
	namespace apex
	{
		class NxClothingAsset;
	}
}

struct FSubmeshInfo
{
	int32  SubmeshIndex;
	uint32 VertexCount;
	uint32 TriangleCount;
	uint32 SimulVertexCount;
	uint32 FixedVertexCount;
	uint32 NumUsedBones;
	int32  NumBoneSpheres;
	bool operator==(const FSubmeshInfo& Other) const
	{
		return (SubmeshIndex		== Other.SubmeshIndex
				&& VertexCount		== Other.VertexCount
				&& TriangleCount	== Other.TriangleCount
				&& SimulVertexCount == Other.SimulVertexCount
				&& FixedVertexCount == Other.FixedVertexCount
				&& NumUsedBones     == Other.NumUsedBones
				&& NumBoneSpheres   == Other.NumBoneSpheres
				);
	}
};

struct FClothingBackup
{
	bool						bBackedUp;
	TArray<FClothingAssetData>	ClothingAssets;
	TArray<FSkelMeshSection>	Sections;
	TArray<uint32>				IndexBuffer;

	void	Clear()
	{
		bBackedUp = false;
		ClothingAssets.Empty();
		Sections.Empty();
	}
};
#endif // #if WITH_APEX_CLOTHING

// Define interface for importing apex clothing asset
namespace ApexClothingUtils
{
	enum EClothUtilRetType
	{
		CURT_Fail = 0,
		CURT_Ok,
		CURT_Cancel
	};

	// Function to restore all clothing section to original mesh section related to specified asset index
	UNREALED_API void RemoveAssetFromSkeletalMesh(USkeletalMesh* SkelMesh, uint32 AssetIndex, bool bRecreateSkelMeshComponent = false);
	// Function to restore clothing section to original mesh section
	UNREALED_API void RestoreOriginalClothingSection(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 SectionIndex, bool bReregisterSkelMeshComponent = true);
	UNREALED_API void RestoreOriginalClothingSectionAllLODs(USkeletalMesh* SkelMesh, uint32 LOD0_SectionIndex);

	// using global variable for backing up clothing data
	UNREALED_API void BackupClothingDataFromSkeletalMesh(USkeletalMesh* SkelMesh);
	UNREALED_API void ReapplyClothingDataToSkeletalMesh(USkeletalMesh* SkelMesh);

	// Find mesh section index in a StaticLODModel by material index
	UNREALED_API int32 FindSectionByMaterialIndex( USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 MaterialIndex );

	UNREALED_API uint32 GetMaxClothSimulVertices(ERHIFeatureLevel::Type InFeatureLevel);

#if WITH_APEX_CLOTHING

	// if AssetIndex is -1, means newly added asset, otherwise re-import
	UNREALED_API EClothUtilRetType ImportApexAssetFromApexFile(FString& ApexFile,USkeletalMesh* SkelMesh, int32 AssetIndex=-1);
	UNREALED_API bool GetSubmeshInfoFromApexAsset(physx::apex::NxClothingAsset *InAsset, uint32 LODIndex, TArray<FSubmeshInfo>& OutSubmeshInfos);

	// import cloth by a specifed section and LOD index
	UNREALED_API bool ImportClothingSectionFromClothingAsset(USkeletalMesh* SkelMesh, uint32 LODIndex, uint32 SectionIndex, int32 AssetIndex, int32 AssetSubmeshIndex);

	UNREALED_API void ReImportClothingSectionsFromClothingAsset(USkeletalMesh* SkelMesh);
	UNREALED_API void ReImportClothingSectionFromClothingAsset(USkeletalMesh* SkelMesh, int32 LODIndex, uint32 SectionIndex);

	UNREALED_API void BackupClothingDataFromSkeletalMesh(USkeletalMesh* SkelMesh, FClothingBackup& ClothingBackup);
	UNREALED_API void ReapplyClothingDataToSkeletalMesh(USkeletalMesh* SkelMesh, FClothingBackup& ClothingBackup);
	UNREALED_API int32 GetNumLODs(physx::apex::NxClothingAsset *InAsset);
	UNREALED_API int32 GetNumRenderSubmeshes(physx::apex::NxClothingAsset *InAsset, int32 LODIndex);

	UNREALED_API physx::apex::NxClothingAsset* CreateApexClothingAssetFromBuffer(const uint8* Buffer, int32 BufferSize);

	// if MaterialIndex is not specified, default material index will be used
	UNREALED_API void GetPhysicsPropertiesFromApexAsset(physx::apex::NxClothingAsset *InAsset, FClothPhysicsProperties& OutPropertyInfo);
	UNREALED_API void SetPhysicsPropertiesToApexAsset(physx::apex::NxClothingAsset *InAsset, FClothPhysicsProperties& InPropertyInfo);

	UNREALED_API void GenerateMeshToMeshSkinningData(TArray<FApexClothPhysToRenderVertData>& OutSkinningData,
													 const TArray<FVector>& Mesh0Verts,
													 const TArray<FVector>& Mesh0Normals,
													 const TArray<FVector>& Mesh0Tangents,
													 const TArray<FVector>& Mesh1Verts,
													 const TArray<FVector>& Mesh1Normals,
													 const TArray<uint32>& Mesh1Indices);

	FVector4 GetPointBaryAndDist(const FVector& A,
								 const FVector& B,
								 const FVector& C,
								 const FVector& NA,
								 const FVector& NB,
								 const FVector& NC,
								 const FVector& Point);

#endif // #if WITH_APEX_CLOTHING
} // namespace ApexClothingUtils

#endif //__ApexClothingUtils_h__
