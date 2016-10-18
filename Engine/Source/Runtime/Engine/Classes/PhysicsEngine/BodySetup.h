// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AggregateGeom.h"
#include "BodySetupEnums.h"
#include "BodySetup.generated.h"

namespace physx
{
	class PxTriangleMesh;
	class PxRigidActor;
}

/** UV information for BodySetup, only created if UPhysicsSettings::bSupportUVFromHitResults */
struct FBodySetupUVInfo
{
	/** Index buffer, required to go from face index to UVs */
	TArray<int32> IndexBuffer;
	/** Vertex positions, used to determine barycentric co-ords */
	TArray<FVector> VertPositions;
	/** UV channels for each vertex */
	TArray< TArray<FVector2D> > VertUVs;

	friend FArchive& operator<<(FArchive& Ar, FBodySetupUVInfo& UVInfo)
	{
		Ar << UVInfo.IndexBuffer;
		Ar << UVInfo.VertPositions;
		Ar << UVInfo.VertUVs;

		return Ar;
	}

	/** Get resource size of UV info */
	SIZE_T GetResourceSize();
};

/**
 * BodySetup contains all collision information that is associated with a single asset.
 * A single BodySetup instance is shared among many BodyInstances so that geometry data is not duplicated.
 * Assets typically implement a GetBodySetup function that is used during physics state creation.
 * 
 * @see GetBodySetup
 * @see FBodyInstance
 */

UCLASS(hidecategories=Object, MinimalAPI)
class UBodySetup : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Simplified collision representation of this  */
	UPROPERTY(EditAnywhere, Category = BodySetup, meta=(DisplayName = "Primitives"))
	struct FKAggregateGeom AggGeom;

	/** Used in the PhysicsAsset case. Associates this Body with Bone in a skeletal mesh. */
	UPROPERTY(Category=BodySetup, VisibleAnywhere)
	FName BoneName;

	/** 
	 *	If Unfixed it will use physics. If fixed, it will use kinematic. Default will inherit from OwnerComponent's behavior.
	 */
	UPROPERTY(EditAnywhere, Category=Physics)
	TEnumAsByte<EPhysicsType> PhysicsType;

	/** 
	 *	If true (and bEnableFullAnimWeightBodies in SkelMeshComp is true), the physics of this bone will always be blended into the skeletal mesh, regardless of what PhysicsWeight of the SkelMeshComp is. 
	 *	This is useful for bones that should always be physics, even when blending physics in and out for hit reactions (eg cloth or pony-tails).
	 */
	UPROPERTY()
	uint32 bAlwaysFullAnimWeight_DEPRECATED:1;

	/** 
	 *	Should this BodySetup be considered for the bounding box of the PhysicsAsset (and hence SkeletalMeshComponent).
	 *	There is a speed improvement from having less BodySetups processed each frame when updating the bounds.
	 */
	UPROPERTY(EditAnywhere, Category=BodySetup)
	uint32 bConsiderForBounds:1;

	/** 
	 *	If true, the physics of this mesh (only affects static meshes) will always contain ALL elements from the mesh - not just the ones enabled for collision. 
	 *	This is useful for forcing high detail collisions using the entire render mesh.
	 */
	UPROPERTY(Transient)
	uint32 bMeshCollideAll:1;

	/**
	*	If true, the physics triangle mesh will use double sided faces when doing scene queries.
	*	This is useful for planes and single sided meshes that need traces to work on both sides.
	*/
	UPROPERTY(EditAnywhere, Category=Physics)
	uint32 bDoubleSidedGeometry : 1;

	/**	Should we generate data necessary to support collision on normal (non-mirrored) versions of this body. */
	UPROPERTY()
	uint32 bGenerateNonMirroredCollision:1;

	/** Whether the cooked data is shared by multiple body setups. This is needed for per poly collision case where we don't want to duplicate cooked data, but still need multiple body setups for in place geometry changes */
	UPROPERTY()
	uint32 bSharedCookedData : 1;

	/** 
	 *	Should we generate data necessary to support collision on mirrored versions of this mesh. 
	 *	This halves the collision data size for this mesh, but disables collision on mirrored instances of the body.
	 */
	UPROPERTY()
	uint32 bGenerateMirroredCollision:1;

	/** Physical material to use for simple collision on this body. Encodes information about density, friction etc. */
	UPROPERTY(EditAnywhere, Category=Physics, meta=(DisplayName="Simple Collision Physical Material"))
	class UPhysicalMaterial* PhysMaterial;

	/** Collision Type for this body. This eventually changes response to collision to others **/
	UPROPERTY(EditAnywhere, Category=Collision)
	TEnumAsByte<enum EBodyCollisionResponse::Type> CollisionReponse;

	/** Collision Trace behavior - by default, it will keep simple(convex)/complex(per-poly) separate **/
	UPROPERTY(EditAnywhere, Category=Collision, meta=(DisplayName = "Collision Complexity"))
	TEnumAsByte<enum ECollisionTraceFlag> CollisionTraceFlag;

	ENGINE_API TEnumAsByte<enum ECollisionTraceFlag> GetCollisionTraceFlag() const;

	/** Default properties of the body instance, copied into objects on instantiation, was URB_BodyInstance */
	UPROPERTY(EditAnywhere, Category=Collision, meta=(FullyExpand = "true"))
	struct FBodyInstance DefaultInstance;

	/** Custom walkable slope setting for this body. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Physics)
	struct FWalkableSlopeOverride WalkableSlopeOverride;

	UPROPERTY()
	float BuildScale_DEPRECATED;

	/** Build scale for this body setup (static mesh settings define this value) */
	UPROPERTY()
	FVector BuildScale3D;

	/** GUID used to uniquely identify this setup so it can be found in the DDC */
	FGuid BodySetupGuid;

	/** Cooked physics data for each format */
	FFormatContainer CookedFormatData;

private:
#if WITH_EDITOR
	/** Cooked physics data with runtime only optimizations. This allows us to remove editor only data (like face index remap) assuming the project doesn't use it at runtime. At runtime we load this into CookedFormatData */
	FFormatContainer CookedFormatDataRuntimeOnlyOptimization;
#endif

	int32 GetRuntimeOnlyCookOptimizationFlags() const;
public:

	/** Cooked physics data override. This is needed in cases where some other body setup has the cooked data and you don't want to own it or copy it. See per poly skeletal mesh */
	FFormatContainer* CookedFormatDataOverride;

#if WITH_PHYSX
	/** Physics triangle mesh, created from cooked data in CreatePhysicsMeshes */
	TArray<physx::PxTriangleMesh*> TriMeshes;
#endif

	/** Additional UV info, if available. Used for determining UV for a line trace impact. */
	FBodySetupUVInfo UVInfo;

	/** Flag used to know if we have created the physics convex and tri meshes from the cooked data yet */
	bool bCreatedPhysicsMeshes;

	/** Indicates whether this setup has any cooked collision data. */
	bool bHasCookedCollisionData;

	/** Indicates that we will never use convex or trimesh shapes. This is an optimization to skip checking for binary data. */
	bool bNeverNeedsCookedCollisionData;

public:
	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif // WITH_EDITOR
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	//~ End UObject Interface.

	//
	//~ Begin UBodySetup Interface.
	//
	ENGINE_API void CopyBodyPropertiesFrom(const UBodySetup* FromSetup);

	/** Add collision shapes from another body setup to this one */
	ENGINE_API void AddCollisionFrom(class UBodySetup* FromSetup);
	ENGINE_API void AddCollisionFrom(const FKAggregateGeom& FromAggGeom);
	
	/** Create Physics meshes (ConvexMeshes, TriMesh & TriMeshNegX) from cooked data */
	/** Release Physics meshes (ConvexMeshes, TriMesh & TriMeshNegX). Must be called before the BodySetup is destroyed */
	ENGINE_API virtual void CreatePhysicsMeshes();

	/** Returns the volume of this element */
	ENGINE_API virtual float GetVolume(const FVector& Scale) const;

	/** Release Physics meshes (ConvexMeshes, TriMesh & TriMeshNegX) */
	ENGINE_API void ClearPhysicsMeshes();

	/** Calculates the mass. You can pass in the component where additional information is pulled from ( Scale, PhysMaterialOverride ) */
	ENGINE_API virtual float CalculateMass(const UPrimitiveComponent* Component = nullptr) const;

	/** Returns the physics material used for this body. If none, specified, returns the default engine material. */
	ENGINE_API class UPhysicalMaterial* GetPhysMaterial() const;

#if WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR
	/** Clear all simple collision */
	ENGINE_API void RemoveSimpleCollision();

	/** 
	 * Rescales simple collision geometry.  Note you must recreate physics meshes after this 
	 *
	 * @param BuildScale	The scale to apply to the geometry
	 */
	ENGINE_API void RescaleSimpleCollision( FVector BuildScale );

	/** Invalidate physics data */
	ENGINE_API virtual void	InvalidatePhysicsData();	

	/**
	 * Converts a UModel to a set of convex hulls for simplified collision.  Any convex elements already in
	 * this BodySetup will be destroyed.  WARNING: the input model can have no single polygon or
	 * set of coplanar polygons which merge to more than FPoly::MAX_VERTICES vertices.
	 *
	 * @param		InModel					The input BSP.
	 * @param		bRemoveExisting			If true, clears any pre-existing collision
	 * @return								true on success, false on failure because of vertex count overflow.
	 */
	ENGINE_API void CreateFromModel(class UModel* InModel, bool bRemoveExisting);

#endif // WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR

	/**
	 * Converts the skinned data of a skeletal mesh into a tri mesh collision. This is used for per poly scene queries and is quite expensive.
	 * In 99% of cases you should be fine using a physics asset created for the skeletal mesh
	 * @param	InSkeletalMeshComponent		The skeletal mesh component we'll be grabbing the skinning information from
	 */
	ENGINE_API void UpdateTriMeshVertices(const TArray<FVector> & NewPositions);

	/**	
	 * Finds the shortest distance between the body setup and a world position. Input and output are given in world space
	 * @param	WorldPosition	The point we are trying to get close to
	 * @param	BodyToWorldTM	The transform to convert BodySetup into world space
	 * @return					The distance between WorldPosition and the body setup. 0 indicates WorldPosition is inside one of the shapes.
	 *
	 * NOTE: This function ignores convex and trimesh data
	 */
	ENGINE_API float GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BodyToWorldTM) const;

	/** 
	 * Finds the closest point in the body setup. Input and outputs are given in world space.
	 * @param	WorldPosition			The point we are trying to get close to
	 * @param	BodyToWorldTM			The transform to convert BodySetup into world space
	 * @param	ClosestWorldPosition	The closest point on the body setup to WorldPosition
	 * @param	FeatureNormal			The normal of the feature associated with ClosestWorldPosition
	 * @return							The distance between WorldPosition and the body setup. 0 indicates WorldPosition is inside one of the shapes.
	 *
	 * NOTE: This function ignores convex and trimesh data
	 */
	ENGINE_API float GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BodyToWorldTM, FVector& ClosestWorldPosition, FVector& FeatureNormal) const;

	/**
	 * Given a format name returns its cooked data.
	 *
	 * @param Format Physics format name.
	 * @param bRuntimeOnlyOptimizedVersion whether we want the data that has runtime only optimizations. At runtime this flag is ignored and we use the runtime only optimized data regardless.
	 * @return Cooked data or NULL of the data was not found.
	 */
	FByteBulkData* GetCookedData(FName Format, bool bRuntimeOnlyOptimizedVersion = false);

	/** 
	 *	Given a location in body space, and face index, find the UV of the desired UV channel.
	 *	Note this ONLY works if 'Support UV From Hit Results' is enabled in Physics Settings.
	 */
	bool CalcUVAtLocation(const FVector& BodySpaceLocation, int32 FaceIndex, int32 UVChannel, FVector2D& UV) const;


#if WITH_EDITOR
	ENGINE_API virtual void BeginCacheForCookedPlatformData(  const ITargetPlatform* TargetPlatform ) override;
	ENGINE_API virtual void ClearCachedCookedPlatformData(  const ITargetPlatform* TargetPlatform ) override;

	/*
	* Copy all UPROPERTY settings except the collision geometry.
	* This function is use when we restore the original data after a re-import of a static mesh.
	* All UProperty should be copy here except the collision geometry (i.e. AggGeom)
	*/
	ENGINE_API virtual void CopyBodySetupProperty(const UBodySetup* Other);
#endif // WITH_EDITOR

#if WITH_PHYSX
	/** 
	 *   Add the shapes defined by this body setup to the supplied PxRigidBody. 
	 */
	DEPRECATED(4.8, "Please call AddShapesToRigidActor_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	ENGINE_API void AddShapesToRigidActor(FBodyInstance* OwningInstance, physx::PxRigidActor* PDestActor, EPhysicsSceneType SceneType, FVector& Scale3D, physx::PxMaterial* SimpleMaterial, TArray<UPhysicalMaterial*>& ComplexMaterials, FShapeData& ShapeData, const FTransform& RelativeTM = FTransform::Identity, TArray<physx::PxShape*>* NewShapes = NULL)
	{
		AddShapesToRigidActor_AssumesLocked(OwningInstance, PDestActor, SceneType, Scale3D, SimpleMaterial, ComplexMaterials, ShapeData, RelativeTM, NewShapes);
	}

	/** 
	 *   Add the shapes defined by this body setup to the supplied PxRigidBody. 
	 */
	ENGINE_API void AddShapesToRigidActor_AssumesLocked(FBodyInstance* OwningInstance, physx::PxRigidActor* PDestActor, EPhysicsSceneType SceneType, FVector& Scale3D, physx::PxMaterial* SimpleMaterial, TArray<UPhysicalMaterial*>& ComplexMaterials, FShapeData& ShapeData, const FTransform& RelativeTM = FTransform::Identity, TArray<physx::PxShape*>* NewShapes = NULL, bool bShapeSharing = false);
#endif // WITH_PHYSX

	friend struct FAddShapesHelper;

};
