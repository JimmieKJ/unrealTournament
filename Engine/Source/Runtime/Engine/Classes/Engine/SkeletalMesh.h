// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Contains the shared data that is used by all SkeletalMeshComponents (instances).
 */

#include "SkeletalMeshTypes.h"
#include "Animation/PreviewAssetAttachComponent.h"
#include "BoneContainer.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "BoneIndices.h"
#include "SkeletalMesh.generated.h"

/** The maximum number of skeletal mesh LODs allowed. */
#define MAX_SKELETAL_MESH_LODS 5

class UMorphTarget;
class USkeleton;

#if WITH_APEX_CLOTHING
struct FApexClothCollisionVolumeData;

namespace physx
{
	namespace apex
	{
		class NxClothingAsset;
	}
}
#endif

/** Enum specifying the importance of properties when simplifying skeletal meshes. */
UENUM()
enum SkeletalMeshOptimizationImportance
{	
	SMOI_Off,
	SMOI_Lowest,
	SMOI_Low,
	SMOI_Normal,
	SMOI_High,
	SMOI_Highest,
	SMOI_MAX,
};

/** Enum specifying the reduction type to use when simplifying skeletal meshes. */
UENUM()
enum SkeletalMeshOptimizationType
{
	SMOT_NumOfTriangles,
	SMOT_MaxDeviation,
	SMOT_MAX,
};

USTRUCT()
struct FBoneMirrorInfo
{
	GENERATED_USTRUCT_BODY()

	/** The bone to mirror. */
	UPROPERTY(EditAnywhere, Category=BoneMirrorInfo, meta=(ArrayClamp = "RefSkeleton"))
	int32 SourceIndex;

	/** Axis the bone is mirrored across. */
	UPROPERTY(EditAnywhere, Category=BoneMirrorInfo)
	TEnumAsByte<EAxis::Type> BoneFlipAxis;

	FBoneMirrorInfo()
		: SourceIndex(0)
		, BoneFlipAxis(0)
	{
	}

};

/** Structure to export/import bone mirroring information */
USTRUCT()
struct FBoneMirrorExport
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=BoneMirrorExport)
	FName BoneName;

	UPROPERTY(EditAnywhere, Category=BoneMirrorExport)
	FName SourceBoneName;

	UPROPERTY(EditAnywhere, Category=BoneMirrorExport)
	TEnumAsByte<EAxis::Type> BoneFlipAxis;


	FBoneMirrorExport()
		: BoneFlipAxis(0)
	{
	}

};

/** Struct containing triangle sort settings for a particular section */
USTRUCT()
struct FTriangleSortSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=TriangleSortSettings)
	TEnumAsByte<enum ETriangleSortOption> TriangleSorting;

	UPROPERTY(EditAnywhere, Category=TriangleSortSettings)
	TEnumAsByte<enum ETriangleSortAxis> CustomLeftRightAxis;

	UPROPERTY(EditAnywhere, Category=TriangleSortSettings)
	FName CustomLeftRightBoneName;


	FTriangleSortSettings()
		: TriangleSorting(0)
		, CustomLeftRightAxis(0)
	{
	}

};


USTRUCT()
struct FBoneReference
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, Category = BoneReference)
	FName BoneName;

	/** Cached bone index for run time - right now bone index of skeleton **/
	int32 BoneIndex;

	FBoneReference()
		: BoneIndex(INDEX_NONE)
	{
	}

	bool operator==(const FBoneReference& Other) const
	{
		// faster to compare, and BoneName won't matter
		return BoneIndex == Other.BoneIndex;
	}
	/** Initialize Bone Reference, return TRUE if success, otherwise, return false **/
	ENGINE_API bool Initialize(const FBoneContainer& RequiredBones);

	// @fixme laurent - only used by blendspace 'PerBoneBlend'. Fix this to support SkeletalMesh pose.
	ENGINE_API bool Initialize(const USkeleton* Skeleton);

	/** return true if valid. Otherwise return false **/
	ENGINE_API bool IsValid(const FBoneContainer& RequiredBones) const;

	FMeshPoseBoneIndex GetMeshPoseIndex() const { return FMeshPoseBoneIndex(BoneIndex); }
	FCompactPoseBoneIndex GetCompactPoseIndex(const FBoneContainer& RequiredBones) const { return RequiredBones.MakeCompactPoseIndex(GetMeshPoseIndex()); }
};

/**
* FSkeletalMeshOptimizationSettings - The settings used to optimize a skeletal mesh LOD.
*/
USTRUCT()
struct FSkeletalMeshOptimizationSettings
{
	GENERATED_USTRUCT_BODY()

	/** The method to use when optimizing the skeletal mesh LOD */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationType> ReductionMethod;

	/** If ReductionMethod equals SMOT_NumOfTriangles this value is the ratio of triangles [0-1] to remove from the mesh */
	UPROPERTY()
	float NumOfTrianglesPercentage;

	/**If ReductionMethod equals SMOT_MaxDeviation this value is the maximum deviation from the base mesh as a percentage of the bounding sphere. */
	UPROPERTY()
	float MaxDeviationPercentage;

	/** The welding threshold distance. Vertices under this distance will be welded. */
	UPROPERTY()
	float WeldingThreshold;

	/** Whether Normal smoothing groups should be preserved. If false then NormalsThreshold is used **/
	UPROPERTY()
	bool bRecalcNormals;

	/** If the angle between two triangles are above this value, the normals will not be
	smooth over the edge between those two triangles. Set in degrees. This is only used when PreserveNormals is set to false*/
	UPROPERTY()
	float NormalsThreshold;

	/** How important the shape of the geometry is. */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> SilhouetteImportance;

	/** How important texture density is. */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> TextureImportance;

	/** How important shading quality is. */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> ShadingImportance;

	/** How important skinning quality is. */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> SkinningImportance;

	/** The ratio of bones that will be removed from the mesh */
	UPROPERTY()
	float BoneReductionRatio;

	/** Maximum number of bones that can be assigned to each vertex. */
	UPROPERTY()
	int32 MaxBonesPerVertex;

	UPROPERTY()
	TArray<FBoneReference> BonesToRemove_DEPRECATED;

	/** Maximum number of bones that can be assigned to each vertex. */
	UPROPERTY()
	int32 BaseLOD;

	FSkeletalMeshOptimizationSettings()
		: ReductionMethod(SMOT_MaxDeviation)
		, NumOfTrianglesPercentage(1.0f)
		, MaxDeviationPercentage(0)
		, WeldingThreshold(0.1f)
		, bRecalcNormals(true)
		, NormalsThreshold(60.0f)
		, SilhouetteImportance(SMOI_Normal)
		, TextureImportance(SMOI_Normal)
		, ShadingImportance(SMOI_Normal)
		, SkinningImportance(SMOI_Normal)
		, BoneReductionRatio(100.0f)
		, MaxBonesPerVertex(4)
		, BaseLOD(0)
	{
	}

	/** Equality operator. */
	bool operator==(const FSkeletalMeshOptimizationSettings& Other) const
	{
		return ReductionMethod == Other.ReductionMethod
			&& NumOfTrianglesPercentage == Other.NumOfTrianglesPercentage
			&& MaxDeviationPercentage == Other.MaxDeviationPercentage
			&& WeldingThreshold == Other.WeldingThreshold
			&& NormalsThreshold == Other.NormalsThreshold
			&& SilhouetteImportance == Other.SilhouetteImportance
			&& TextureImportance == Other.TextureImportance
			&& ShadingImportance == Other.ShadingImportance
			&& SkinningImportance == Other.SkinningImportance
			&& bRecalcNormals == Other.bRecalcNormals
			&& BoneReductionRatio == Other.BoneReductionRatio
			&& MaxBonesPerVertex == Other.MaxBonesPerVertex
			&& BaseLOD == Other.BaseLOD;
	}

	/** Inequality. */
	bool operator!=(const FSkeletalMeshOptimizationSettings& Other) const
	{
		return !(*this == Other);
	}
};

/** Struct containing information for a particular LOD level, such as materials and info for when to use it. */
USTRUCT()
struct FSkeletalMeshLODInfo
{
	GENERATED_USTRUCT_BODY()

	/**	Indicates when to use this LOD. A smaller number means use this LOD when further away. */
	UPROPERTY(EditAnywhere, Category=SkeletalMeshLODInfo)
	float ScreenSize;

	/**	Used to avoid 'flickering' when on LOD boundary. Only taken into account when moving from complex->simple. */
	UPROPERTY(EditAnywhere, Category=SkeletalMeshLODInfo)
	float LODHysteresis;

	/** Mapping table from this LOD's materials to the USkeletalMesh materials array. */
	UPROPERTY(EditAnywhere, editfixedsize, Category=SkeletalMeshLODInfo)
	TArray<int32> LODMaterialMap;

	/** Per-section control over whether to enable shadow casting. */
	UPROPERTY()
	TArray<bool> bEnableShadowCasting_DEPRECATED;

	UPROPERTY(EditAnywhere, editfixedsize, Category=SkeletalMeshLODInfo)
	TArray<struct FTriangleSortSettings> TriangleSortSettings;

	/** Whether to disable morph targets for this LOD. */
	UPROPERTY()
	uint32 bHasBeenSimplified:1;

	/** Reduction settings to apply when building render data. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	FSkeletalMeshOptimizationSettings ReductionSettings;

	/** This has been removed in editor. We could re-apply this in import time or by mesh reduction utilities*/
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	TArray<FName> RemovedBones;

	FSkeletalMeshLODInfo()
		: ScreenSize(0)
		, LODHysteresis(0)
		, bHasBeenSimplified(false)
	{
	}

};

/** 
 * constrain Coefficients - max distance, collisionSphere radius, collision sphere distance 
 */
struct FClothConstrainCoeff
{
	float ClothMaxDistance;
	float ClothBackstopRadius;
	float ClothBackstopDistance;
};

/** 
 * bone weights & bone indices for visualization of cloth physical mesh 
 */
struct FClothBoneWeightsInfo
{
	// support up 4 bone influences but used MAX_TOTAL_INFLUENCES for the future
	uint16 Indices[MAX_TOTAL_INFLUENCES];
	float Weights[MAX_TOTAL_INFLUENCES];
};
/** 
 * save temporary data only for debugging on Persona editor 
 */
struct FClothVisualizationInfo
{
	TArray<FVector> ClothPhysicalMeshVertices;
	TArray<FVector> ClothPhysicalMeshNormals;
	TArray<uint32> ClothPhysicalMeshIndices;
	TArray<FClothConstrainCoeff> ClothConstrainCoeffs;
	// bone weights & bone indices
	TArray<FClothBoneWeightsInfo> ClothPhysicalMeshBoneWeightsInfo;
	uint8	NumMaxBoneInfluences;

	// Max value of max distances
	float MaximumMaxDistance;
};

/** 
 * data structure for loading bone planes of collision volumes data
 */
struct FClothBonePlane
{
	int32 BoneIndex;
	FPlane PlaneData;
};

/**
 * now exposed a part of properties based on 3DS Max plug-in
 * property names are also changed into 3DS Max plug-in's one
 */
USTRUCT()
struct FClothPhysicsProperties
{
	GENERATED_USTRUCT_BODY()

	// vertical stiffness of the cloth in the range [0, 1].   usually set to 1.0
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float VerticalResistance;

	// Horizontal stiffness of the cloth in the range [0, 1].  usually set to 1.0
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float HorizontalResistance;

	// Bending stiffness of the cloth in the range [0, 1]. 
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BendResistance;

	// Shearing stiffness of the cloth in the range [0, 1]. 
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ShearResistance;

	//  latest email from nvidia suggested this is not in use, my code search revealed the same.   will wait for nv engineering confirmation before deleting
	// Make cloth simulation less stretchy. A value smaller than 1 will turn it off.  Apex parameter hardStretchLimitation.
	//UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "4.0"))
	//float HardStretchLimitation;

	// Friction coefficient in the range[0, 1]
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Friction;
	// Spring damping of the cloth in the range[0, 1]
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Damping;

	// Tether stiffness of the cloth in the range[0, 1].  Equivalent to 1.0-Relax in autodesk plugin.
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float TetherStiffness;
	// Tether Limit, corresponds to 1.0+StretchLimit parameter on Autodesk plugin.  
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", UIMin = "1.0", UIMax = "2.0"))
	float TetherLimit;



	// Drag coefficient n the range [0, 1] 
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Drag;

	// Frequency for stiffness 
	UPROPERTY(EditAnywhere, Category = Stiffness, meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "1000"))
	float StiffnessFrequency;

	// Gravity multiplier for this cloth.  Also called Density in Autodesk plugin.
	UPROPERTY(EditAnywhere, Category = Scale, meta = ( UIMin = "0.0", UIMax = "100.0"))
	float GravityScale;

	// A mass scaling that is applied to the cloth.   Corresponds to 100X the MotionAdaptation parameter in autodesk plugin.
	UPROPERTY(EditAnywhere, Category = Scale, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "100.0"))
	float MassScale;

	// Amount of inertia that is kept when using local space simulation. Internal name is inertia scale
	UPROPERTY(EditAnywhere, Category = Scale, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float InertiaBlend;

	// Minimal amount of distance particles will keep of each other.
	UPROPERTY(EditAnywhere, Category = SelfCollision, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "10000.0"))
	float SelfCollisionThickness;

	// unclear what this actually does.
	UPROPERTY(EditAnywhere, Category = SelfCollision, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5.0"))
	float SelfCollisionSquashScale;

	// Self collision stiffness.  0 off, 1 for on.
	UPROPERTY(EditAnywhere, Category = SelfCollision, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float SelfCollisionStiffness;

	// A computation parameter for the Solver.   Along with frame rate this probably specifies the number of solver iterations
	UPROPERTY(EditAnywhere, Category = Solver, meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "1000.0"))
	float SolverFrequency;


	// Lower (compression) Limit of SoftZone (relative to rest length).  Applied for all fiber types.  If both compression and expansion are 1.0 then there is no deadzone.
	UPROPERTY(EditAnywhere, Category = FiberSoftZone, meta = (UIMin = "0.0", UIMax = "1.0"))
	float FiberCompression;

	// Upper (expansion) Limit of SoftZone (relative to rest length).  Applied to all fiber types.   Also referred to as "stretch" range by apex internally.
	UPROPERTY(EditAnywhere, Category = FiberSoftZone, meta = (UIMin = "1.0", UIMax = "2.0"))
	float FiberExpansion;

	// Resistance Multiplier that's applied to within SoftZone amount for all fiber types.  0.0 for a complete deadzone (no force).  At 1.0 the spring response within the softzone is as stiff it is elsewhere.  This parameter also known as scale by Apex internally.
	UPROPERTY(EditAnywhere, Category = FiberSoftZone, meta = (UIMin = "0.0", UIMax = "1.0"))
	float FiberResistance;

};

USTRUCT()
struct FClothingAssetData
{
	GENERATED_USTRUCT_BODY()

	/* User-defined asset name */
	UPROPERTY(EditAnywhere, Category=ClothingAssetData)
	FName AssetName;

	UPROPERTY(EditAnywhere, Category=ClothingAssetData)
	FString	ApexFileName;

	/** the flag whether cloth physics properties are changed from UE4 editor or not */
	UPROPERTY(EditAnywhere, Category = ClothingAssetData)
	bool bClothPropertiesChanged;

	UPROPERTY(EditAnywhere, Transient, Category = ClothingAssetData)
	FClothPhysicsProperties PhysicsProperties;

#if WITH_APEX_CLOTHING
	physx::apex::NxClothingAsset* ApexClothingAsset;

	/** Collision volume data for showing to the users whether collision shape is correct or not */
	TArray<FApexClothCollisionVolumeData> ClothCollisionVolumes;
	TArray<uint32> ClothCollisionConvexPlaneIndices;
	TArray<FClothBonePlane> ClothCollisionVolumePlanes;
	TArray<FApexClothBoneSphereData> ClothBoneSpheres;
	TArray<uint16> BoneSphereConnections;

	/**
	 * saved temporarily just for debugging / visualization 
	 * Num of this array means LOD number of clothing physical meshes 
	 */
	TArray<FClothVisualizationInfo> ClothVisualizationInfos;

	/** Apex stores only the bones that cloth needs. We need a mapping from apex bone index to UE bone index. */
	TArray<int32> ApexToUnrealBoneMapping;

	/** currently mapped morph target name */
	FName PreparedMorphTargetName;

	FClothingAssetData()
		:ApexClothingAsset(NULL)
	{
	}
#endif// #if WITH_APEX_CLOTHING

	// serialization
	friend FArchive& operator<<(FArchive& Ar, FClothingAssetData& A);

	// get resource size
	SIZE_T GetResourceSize() const;
};

//~ Begin Material Interface for USkeletalMesh - contains a material and a shadow casting flag
USTRUCT()
struct FSkeletalMaterial
{
	GENERATED_USTRUCT_BODY()

	FSkeletalMaterial()
		: MaterialInterface( NULL )
		, bEnableShadowCasting( true )
		, bRecomputeTangent( false )
	{

	}

	FSkeletalMaterial( class UMaterialInterface* InMaterialInterface, bool bInEnableShadowCasting = true, bool bInRecomputeTangent = false )
		: MaterialInterface( InMaterialInterface )
		, bEnableShadowCasting( bInEnableShadowCasting )
		, bRecomputeTangent( bInRecomputeTangent )
	{

	}

	friend FArchive& operator<<( FArchive& Ar, FSkeletalMaterial& Elem );

	ENGINE_API friend bool operator==( const FSkeletalMaterial& LHS, const FSkeletalMaterial& RHS );
	ENGINE_API friend bool operator==( const FSkeletalMaterial& LHS, const UMaterialInterface& RHS );
	ENGINE_API friend bool operator==( const UMaterialInterface& LHS, const FSkeletalMaterial& RHS );

	UPROPERTY(EditAnywhere, BlueprintReadOnly, transient, Category=SkeletalMesh)
	class UMaterialInterface *	MaterialInterface;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=SkeletalMesh, Category=SkeletalMesh)
	bool						bEnableShadowCasting;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SkeletalMesh, Category = SkeletalMesh)
	bool						bRecomputeTangent;
};

class FSkeletalMeshResource;

/**
 * SkeletalMesh is geometry bound to a hierarchical skeleton of bones which can be animated for the purpose of deforming the mesh.
 * Skeletal Meshes are built up of two parts; a set of polygons composed to make up the surface of the mesh, and a hierarchical skeleton which can be used to animate the polygons.
 * The 3D models, rigging, and animations are created in an external modeling and animation application (3DSMax, Maya, Softimage, etc).
 *
 * @see https://docs.unrealengine.com/latest/INT/Engine/Content/Types/SkeletalMeshes/
 */
UCLASS(hidecategories=Object, MinimalAPI, BlueprintType)
class USkeletalMesh : public UObject, public IInterface_CollisionDataProvider, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()

private:
	/** Rendering resources created at import time. */
	TSharedPtr<FSkeletalMeshResource> ImportedResource;

public:
	/** Get the default resource for this skeletal mesh. */
	FORCEINLINE FSkeletalMeshResource* GetImportedResource() const { return ImportedResource.Get(); }

	/** Get the resource to use for rendering. */
	FORCEINLINE FSkeletalMeshResource* GetResourceForRendering() const { return GetImportedResource(); }

	/** Skeleton of this skeletal mesh **/
	UPROPERTY(Category=Mesh, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	USkeleton* Skeleton;

private:
	/** Original imported mesh bounds */
	UPROPERTY(transient, duplicatetransient)
	FBoxSphereBounds ImportedBounds;

	/** Bounds extended by user values below */
	UPROPERTY(transient, duplicatetransient)
	FBoxSphereBounds ExtendedBounds;

protected:
	// The properties below are protected to force the use of the Set* methods for this data
	// in code so we can keep the extended bounds up to date after changing the data.
	// Property editors will trigger property events to correctly recalculate the extended bounds.

	/** Bound extension values in addition to imported bound in the positive direction of XYZ, 
	 *	positive value increases bound size and negative value decreases bound size. 
	 *	The final bound would be from [Imported Bound - Negative Bound] to [Imported Bound + Positive Bound]. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mesh)
	FVector PositiveBoundsExtension;

	/** Bound extension values in addition to imported bound in the negative direction of XYZ, 
	 *	positive value increases bound size and negative value decreases bound size. 
	 *	The final bound would be from [Imported Bound - Negative Bound] to [Imported Bound + Positive Bound]. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mesh)
	FVector NegativeBoundsExtension;

public:

	/** Get the extended bounds of this mesh (imported bounds plus bounds extension) */
	UFUNCTION(BlueprintCallable, Category = Mesh)
	ENGINE_API FBoxSphereBounds GetBounds();

	/** Get the original imported bounds of the skel mesh */
	UFUNCTION(BlueprintCallable, Category = Mesh)
	ENGINE_API FBoxSphereBounds GetImportedBounds();

	/** Set the original imported bounds of the skel mesh, will recalculate extended bounds */
	ENGINE_API void SetImportedBounds(const FBoxSphereBounds& InBounds);

	/** Set bound extension values in the positive direction of XYZ, positive value increases bound size */
	ENGINE_API void SetPositiveBoundsExtension(const FVector& InExtension);

	/** Set bound extension values in the negative direction of XYZ, positive value increases bound size */
	ENGINE_API void SetNegativeBoundsExtension(const FVector& InExtension);

	/** Calculate the extended bounds based on the imported bounds and the extension values */
	void CalculateExtendedBounds();

	/** Alters the bounds extension values to fit correctly into the current bounds (so negative values never extend the bounds etc.) */
	void ValidateBoundsExtension();

	/** List of materials applied to this mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, transient, duplicatetransient, Category=SkeletalMesh)
	TArray<FSkeletalMaterial> Materials;

	/** List of bones that should be mirrored. */
	UPROPERTY(EditAnywhere, editfixedsize, Category=Mirroring)
	TArray<struct FBoneMirrorInfo> SkelMirrorTable;

	UPROPERTY(EditAnywhere, Category=Mirroring)
	TEnumAsByte<EAxis::Type> SkelMirrorAxis;

	UPROPERTY(EditAnywhere, Category=Mirroring)
	TEnumAsByte<EAxis::Type> SkelMirrorFlipAxis;

	/** Struct containing information for each LOD level, such as materials to use, and when use the LOD. */
	UPROPERTY(EditAnywhere, EditFixedSize, Category=LevelOfDetail)
	TArray<struct FSkeletalMeshLODInfo> LODInfo;

	/** If true, use 32 bit UVs. If false, use 16 bit UVs to save memory */
	UPROPERTY(EditAnywhere, Category=Mesh)
	uint32 bUseFullPrecisionUVs:1;

	/** true if this mesh has ever been simplified with Simplygon. */
	UPROPERTY()
	uint32 bHasBeenSimplified:1;

	/** Whether or not the mesh has vertex colors */
	UPROPERTY()
	uint32 bHasVertexColors:1;


	/** Uses skinned data for collision data. Per poly collision cannot be used for simulation, in most cases you are better off using the physics asset */
	UPROPERTY(EditAnywhere, Category = Physics)
	uint32 bEnablePerPolyCollision : 1;

	// Physics data for the per poly collision case. In 99% of cases you will not need this and are better off using simple ragdoll collision (physics asset)
	UPROPERTY(transient)
	class UBodySetup* BodySetup;

	/**
	 *	Physics and collision information used for this USkeletalMesh, set up in PhAT.
	 *	This is used for per-bone hit detection, accurate bounding box calculation and ragdoll physics for example.
	 */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, BlueprintReadOnly, Category=Physics)
	class UPhysicsAsset* PhysicsAsset;

	/**
	 * Physics asset whose shapes will be used for shadowing when components have bCastCharacterCapsuleDirectShadow or bCastCharacterCapsuleIndirectShadow enabled.
	 * Only spheres and sphyl shapes in the physics asset can be supported.  The more shapes used, the higher the cost of the capsule shadows will be.
	 */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, BlueprintReadOnly, Category=Lighting)
	class UPhysicsAsset* ShadowPhysicsAsset;

#if WITH_EDITORONLY_DATA

	/** Importing data and options used for this mesh */
	UPROPERTY(VisibleAnywhere, Instanced, Category = ImportSettings)
	class UAssetImportData* AssetImportData;

	/** Path to the resource used to construct this skeletal mesh */
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY()
	FString SourceFileTimestamp_DEPRECATED;

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;

	/** Optimization settings used to simplify LODs of this mesh. */
	UPROPERTY()
	TArray<struct FSkeletalMeshOptimizationSettings> OptimizationSettings;

	/* Attached assets component for this mesh */
	UPROPERTY()
	FPreviewAssetAttachContainer PreviewAttachedAssetContainer;
#endif // WITH_EDITORONLY_DATA

	/**
	 * Allows artists to adjust the distance where textures using UV 0 are streamed in/out.
	 * 1.0 is the default, whereas a higher value increases the streamed-in resolution.
	 * Value can be < 0 (from legcay content, or code changes)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureStreaming, meta=(ClampMin = 0))
	float StreamingDistanceMultiplier;

	UPROPERTY(Category=Mesh, BlueprintReadWrite)
	TArray<UMorphTarget*> MorphTargets;

	/** A fence which is used to keep track of the rendering thread releasing the static mesh resources. */
	FRenderCommandFence ReleaseResourcesFence;

	/** New Reference skeleton type **/
	FReferenceSkeleton RefSkeleton;

	/** Map of morph target name to index into USkeletalMesh::MorphTargets**/
	TMap<FName, int32> MorphTargetIndexMap;

	/** Reference skeleton precomputed bases. */
	TArray<FMatrix> RefBasesInvMatrix;    

#if WITH_EDITORONLY_DATA
	/** The section currently selected in the Editor. Used for highlighting */
	UPROPERTY(transient)
	int32 SelectedEditorSection;

	/** The section currently selected for clothing. need to remember this index for reimporting cloth */
	UPROPERTY(transient)
	int32 SelectedClothingSection;

	/** Height offset for the floor mesh in the editor */
	UPROPERTY()
	float FloorOffset;

	/** This is buffer that saves pose that is used by retargeting*/
	UPROPERTY()
	TArray<FTransform> RetargetBasePose;

#endif

	/** Clothing asset data */
	UPROPERTY(EditAnywhere, editfixedsize, BlueprintReadOnly, Category=Clothing)
	TArray<FClothingAssetData>		ClothingAssets;

protected:

	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category=SkeletalMesh)
	TArray<UAssetUserData*> AssetUserData;

private:
	/** Skeletal mesh source data */
	class FSkeletalMeshSourceData* SourceData;

	/** The cached streaming texture factors.  If the array doesn't have MAX_TEXCOORDS entries in it, the cache is outdated. */
	TArray<float> CachedStreamingTextureFactors;

	/** 
	 *	Array of named socket locations, set up in editor and used as a shortcut instead of specifying 
	 *	everything explicitly to AttachComponent in the SkeletalMeshComponent. 
	 */
	UPROPERTY()
	TArray<class USkeletalMeshSocket*> Sockets;

	/** Cached matrices from GetComposedRefPoseMatrix */
	TArray<FMatrix> CachedComposedRefPoseMatrices;

public:
	/**
	* Initialize the mesh's render resources.
	*/
	ENGINE_API void InitResources();

	/**
	* Releases the mesh's render resources.
	*/
	ENGINE_API void ReleaseResources();


	/** Release CPU access version of buffer */
	void ReleaseCPUResources();

	/**
	 * Returns the scale dependent texture factor used by the texture streaming code.	
	 *
	 * @param RequestedUVIndex UVIndex to look at
	 * @return scale dependent texture factor
	 */
	float GetStreamingTextureFactor( int32 RequestedUVIndex );

	/**
	 * Gets the center point from which triangles should be sorted, if any.
	 */
	ENGINE_API bool GetSortCenterPoint(FVector& OutSortCenter) const;

	/**
	 * Computes flags for building vertex buffers.
	 */
	ENGINE_API uint32 GetVertexBufferFlags() const;

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void PostEditUndo() override;
	virtual void GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual FString GetDesc() override;
	virtual FString GetDetailedInfoInternal() const override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface.

	/** Setup-only routines - not concerned with the instance. */

	ENGINE_API void CalculateInvRefMatrices();

	/** Calculate the required bones for a Skeletal Mesh LOD, including possible extra influences */
	ENGINE_API static void CalculateRequiredBones(class FStaticLODModel& LODModel, const struct FReferenceSkeleton& RefSkeleton, const TMap<FBoneIndexType, FBoneIndexType> * BonesToRemove);

	/** 
	 *	Find a socket object in this SkeletalMesh by name. 
	 *	Entering NAME_None will return NULL. If there are multiple sockets with the same name, will return the first one.
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	ENGINE_API USkeletalMeshSocket* FindSocket(FName InSocketName) const;

	/**
	*	Find a socket object in this SkeletalMesh by name.
	*	Entering NAME_None will return NULL. If there are multiple sockets with the same name, will return the first one.
	*   Also returns the index for the socket allowing for future fast access via GetSocketByIndex()
	*/
	UFUNCTION(BlueprintCallable, Category = "Animation")
	ENGINE_API USkeletalMeshSocket* FindSocketAndIndex(FName InSocketName, int32& OutIndex) const;

	/** Returns the number of sockets available. Both on this mesh and it's skeleton. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	ENGINE_API int32 NumSockets() const;

	/** Returns a socket by index. Max index is NumSockets(). The meshes sockets are accessed first, then the skeletons.  */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	ENGINE_API USkeletalMeshSocket* GetSocketByIndex(int32 Index) const;

	// @todo document
	ENGINE_API FMatrix GetRefPoseMatrix( int32 BoneIndex ) const;

	/** 
	 *	Get the component orientation of a bone or socket. Transforms by parent bones.
	 */
	ENGINE_API FMatrix GetComposedRefPoseMatrix( FName InBoneName ) const;
	ENGINE_API FMatrix GetComposedRefPoseMatrix( int32 InBoneIndex ) const;

	/** Allocate and initialise bone mirroring table for this skeletal mesh. Default is source = destination for each bone. */
	void InitBoneMirrorInfo();

	/** Utility for copying and converting a mirroring table from another USkeletalMesh. */
	ENGINE_API void CopyMirrorTableFrom(USkeletalMesh* SrcMesh);
	ENGINE_API void ExportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo);
	ENGINE_API void ImportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo);

	/** 
	 *	Utility for checking that the bone mirroring table of this mesh is good.
	 *	Return true if mirror table is OK, false if there are problems.
	 *	@param	ProblemBones	Output string containing information on bones that are currently bad.
	 */
	ENGINE_API bool MirrorTableIsGood(FString& ProblemBones);

	/**
	 * Returns the mesh only socket list - this ignores any sockets in the skeleton
	 * Return value is a non-const reference so the socket list can be changed
	 */
	ENGINE_API TArray<USkeletalMeshSocket*>& GetMeshOnlySocketList();

	/**
	* Returns the "active" socket list - all sockets from this mesh plus all non-duplicates from the skeleton
	* Const ref return value as this cannot be modified externally
	*/
	ENGINE_API TArray<USkeletalMeshSocket*> GetActiveSocketList() const;

#if WITH_EDITOR
	/** Retrieves the source model for this skeletal mesh. */
	ENGINE_API FStaticLODModel& GetSourceModel();

	/**
	 * Copies off the source model for this skeletal mesh if necessary and returns it. This function should always be called before
	 * making destructive changes to the mesh's geometry, e.g. simplification.
	 */
	ENGINE_API FStaticLODModel& PreModifyMesh();

	/**
	* Makes sure all attached objects are valid and removes any that aren't.
	*
	* @return		NumberOfBrokenAssets
	*/
	ENGINE_API int32 ValidatePreviewAttachedObjects();

#endif // #if WITH_EDITOR

	/**
	* Verify SkeletalMeshLOD is set up correctly	
	*/
	void DebugVerifySkeletalMeshLOD();

	/**
	 * Find a named MorphTarget from the MorphSets array in the SkinnedMeshComponent.
	 * This searches the array in the same way as FindAnimSequence
	 *
	 * @param MorphTargetName Name of MorphTarget to look for.
	 *
	 * @return Pointer to found MorphTarget. Returns NULL if could not find target with that name.
	 */
	ENGINE_API UMorphTarget* FindMorphTarget(FName MorphTargetName) const;
	ENGINE_API UMorphTarget* FindMorphTargetAndIndex(FName MorphTargetName, int32& OutIndex) const;

	/** if name conflicts, it will overwrite the reference */
	ENGINE_API void RegisterMorphTarget(UMorphTarget* MorphTarget);

	ENGINE_API void UnregisterMorphTarget(UMorphTarget* MorphTarget);

	/** Initialize MorphSets look up table : MorphTargetIndexMap */
	ENGINE_API void InitMorphTargets();

#if WITH_APEX_CLOTHING
	ENGINE_API bool  HasClothSectionsInAllLODs(int AssetIndex);
	ENGINE_API bool	 HasClothSections(int32 LODIndex,int AssetIndex);
	ENGINE_API void	 GetOriginSectionIndicesWithCloth(int32 LODIndex, TArray<uint32>& OutSectionIndices);
	ENGINE_API void	 GetOriginSectionIndicesWithCloth(int32 LODIndex, int32 AssetIndex, TArray<uint32>& OutSectionIndices);
	ENGINE_API void	 GetClothSectionIndices(int32 LODIndex, int32 AssetIndex, TArray<uint32>& OutSectionIndices);
	//moved from ApexClothingUtils because of compile issues
	ENGINE_API void  LoadClothCollisionVolumes(int32 AssetIndex, physx::apex::NxClothingAsset* ClothingAsset);
	ENGINE_API bool IsMappedClothingLOD(int32 LODIndex, int32 AssetIndex);
	ENGINE_API int32 GetClothAssetIndex(int32 LODIndex, int32 SectionIndex);
	ENGINE_API void BuildApexToUnrealBoneMapping();
#endif

	/** 
	 * Checks whether the provided section is using APEX cloth. if bCheckCorrespondingSections is true
	 * disabled sections will defer to correspond sections to see if they use cloth (non-cloth sections
	 * are disabled and another section added when cloth is enabled, using this flag allows for a check
	 * on the original section to succeed)
	 * @param InSectionIndex Index to check
	 * @param bCheckCorrespondingSections Whether to check corresponding sections for disabled sections
	 */
	UFUNCTION(BlueprintCallable, Category="Cloth")
	ENGINE_API bool IsSectionUsingCloth(int32 InSectionIndex, bool bCheckCorrespondingSections = true) const;

	ENGINE_API void CreateBodySetup();
	ENGINE_API UBodySetup* GetBodySetup();

#if WITH_EDITOR
	/** Trigger a physics build to ensure per poly collision is created */
	ENGINE_API void BuildPhysicsData();
	ENGINE_API void AddBoneToReductionSetting(int32 LODIndex, const TArray<FName>& BoneNames);
	ENGINE_API void AddBoneToReductionSetting(int32 LODIndex, FName BoneName);
#endif
	

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override
	{
		return true;
	}
	//~ End Interface_CollisionDataProvider Interface

	//~ Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface

private:

	/** Utility function to help with building the combined socket list */
	bool IsSocketOnMesh( const FName& InSocketName ) const;

	/**
	* Flush current render state
	*/
	void FlushRenderState();
	/**
	* Restart render state. 
	*/
	void RestartRenderState();

	/**
	* In older data, the bEnableShadowCasting flag was stored in LODInfo
	* so it needs moving over to materials
	*/
	void MoveDeprecatedShadowFlagToMaterials();

	/**
	* Test whether all the flags in an array are identical (could be moved to Array.h?)
	*/
	bool AreAllFlagsIdentical( const TArray<bool>& BoolArray ) const;

	/*
	* Ask the reference skeleton to rebuild the NameToIndexMap array. This is use to load old package before this array was created.
	*/
	void RebuildRefSkeletonNameToIndexMap();
};


/**
 * Refresh Physics Asset Change
 * 
 * Physics Asset has been changed, so it will need to recreate physics state to reflect it
 * Utilities functions to propagate new Physics Asset for InSkeletalMesh
 *
 * @param	InSkeletalMesh	SkeletalMesh that physics asset has been changed for
 */
ENGINE_API void RefreshSkelMeshOnPhysicsAssetChange(const USkeletalMesh* InSkeletalMesh);
