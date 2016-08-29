// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=======================================================================================
	PhysXCollision.h: Collision related data structures/types specific to PhysX
=========================================================================================*/

#pragma once

#if WITH_PHYSX

#include "Union.h"
#include "../PhysicsEngine/PhysXSupport.h"
#include "CollisionQueryParams.h"

/** Temporary result buffer size */
#define HIT_BUFFER_SIZE							512		// Hit buffer size for traces and sweeps. This is the total size allowed for sync + async tests.
#define HIT_BUFFER_MAX_SYNC_QUERIES				496		// Maximum number of queries to fill in the hit buffer for synchronous traces and sweeps. Async sweeps get the difference (HIT_BUFFER_SIZE - NumberOfHitsInSyncTest).
#define OVERLAP_BUFFER_SIZE						1024
#define OVERLAP_BUFFER_SIZE_MAX_SYNC_QUERIES	992

static_assert(HIT_BUFFER_SIZE > 0, "Invalid PhysX hit buffer size.");
static_assert(HIT_BUFFER_MAX_SYNC_QUERIES < HIT_BUFFER_SIZE, "Invalid PhysX sync buffer size.");



/** 
 *	Macro for obtaining a specific geometry from the shape passed in
 *	NB. this macro is bad in terms of hiding the variable names, so made variable name to use Macro, so it does not overlap with the ones used outside 
 **/
#define GET_GEOMETRY_FROM_SHAPE( OutGeom, Shape ) \
	PxGeometry*				OutGeom = NULL; \
	PxSphereGeometry		PMacroSphereGeom;	\
	PxBoxGeometry			PMacroBoxGeom;		\
	PxCapsuleGeometry		PMacroCapsuleGeom;	\
	PxConvexMeshGeometry	PMacroConvexGeom;	\
	\
	if(Shape->getGeometryType() == PxGeometryType::eSPHERE)\
	{\
		Shape->getSphereGeometry(PMacroSphereGeom);\
		OutGeom = &PMacroSphereGeom;\
	}\
	else if(Shape->getGeometryType() == PxGeometryType::eBOX)\
	{\
		Shape->getBoxGeometry(PMacroBoxGeom);\
		OutGeom = &PMacroBoxGeom;\
	}\
	else if(Shape->getGeometryType() == PxGeometryType::eCAPSULE)\
	{\
		Shape->getCapsuleGeometry(PMacroCapsuleGeom);\
		OutGeom = &PMacroCapsuleGeom;\
	}\
	else if(Shape->getGeometryType() == PxGeometryType::eCONVEXMESH)\
	{\
		Shape->getConvexMeshGeometry(PMacroConvexGeom);\
		OutGeom = &PMacroConvexGeom;\
	}\
	else \
	{\
		OutGeom = NULL; \
	}


//TODO: update places that use macro with this function
struct GeometryFromShapeStorage
{
	PxSphereGeometry SphereGeom;
	PxBoxGeometry BoxGeom;
	PxCapsuleGeometry CapsuleGeom;
	PxConvexMeshGeometry ConvexGeom;
	PxTriangleMeshGeometry TriangleGeom;
	PxHeightFieldGeometry HeightFieldGeom;
};

PxGeometry * GetGeometryFromShape(GeometryFromShapeStorage & LocalStorage, const PxShape * PShape, bool bTriangleMeshAllowed = false);

// FILTER

/** TArray typedef of components to ignore. */
typedef FCollisionQueryParams::IgnoreComponentsArrayType FilterIgnoreComponentsArrayType;

/** TArray typedef of actors to ignore. */
typedef FCollisionQueryParams::IgnoreActorsArrayType FilterIgnoreActorsArrayType;


/** Unreal PhysX scene query filter callback object */
class FPxQueryFilterCallback : public PxSceneQueryFilterCallback
{
public:

	/** List of ComponentIds for this query to ignore */
	const FilterIgnoreComponentsArrayType& IgnoreComponents;

	/** List of ActorIds for this query to ignore */
	const FilterIgnoreActorsArrayType& IgnoreActors;
	
	/** Result of PreFilter callback. */
	PxSceneQueryHitType::Enum PrefilterReturnValue;

	/** Whether to ignore touches (convert an eTOUCH result to eNONE). */
	bool bIgnoreTouches;

	/** Whether to ignore blocks (convert an eBLOCK result to eNONE). */
	bool bIgnoreBlocks;

	FPxQueryFilterCallback(const FCollisionQueryParams& InQueryParams)
		: IgnoreComponents(InQueryParams.GetIgnoredComponents())
		, IgnoreActors(InQueryParams.GetIgnoredActors())
	{
		PrefilterReturnValue = PxSceneQueryHitType::eNONE;		
		bIgnoreTouches = false;
		bIgnoreBlocks = InQueryParams.bIgnoreBlocks;
	}

	/** 
	 * Calculate Result Query HitType from Query Filter and Shape Filter
	 * 
	 * @param PQueryFilter	: Querier FilterData
	 * @param PShapeFilter	: The Shape FilterData querier is testing against
	 *
	 * @return PxSceneQueryHitType from both FilterData
	 */
	static PxSceneQueryHitType::Enum CalcQueryHitType(const PxFilterData &PQueryFilter, const PxFilterData &PShapeFilter, bool bPreFilter = false);
	
	virtual PxSceneQueryHitType::Enum preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxSceneQueryFlags& queryFlags) override;


	virtual PxSceneQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxSceneQueryHit& hit) override
	{
		// Currently not used
		return PxSceneQueryHitType::eBLOCK;
	}
};


class FPxQueryFilterCallbackSweep : public FPxQueryFilterCallback
{
public:
	bool DiscardInitialOverlaps;

	FPxQueryFilterCallbackSweep(const FCollisionQueryParams& QueryParams)
		: FPxQueryFilterCallback(QueryParams)
	{
		DiscardInitialOverlaps = !QueryParams.bFindInitialOverlaps;
	}

	virtual PxSceneQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxSceneQueryHit& hit) override;
};

// MISC

/** Convert from unreal to physx capsule rotation */
PxQuat ConvertToPhysXCapsuleRot(const FQuat& GeomRot);
/** Convert from physx to unreal capsule rotation */
FQuat ConvertToUECapsuleRot(const PxQuat & PGeomRot);
/** Convert from unreal to physx capsule pose */
PxTransform ConvertToPhysXCapsulePose(const FTransform& GeomPose);

// FILTER DATA

/** Utility for creating a PhysX PxFilterData for performing a query (trace) against the scene */
PxFilterData CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer& InCollisionResponseContainer, const struct FCollisionQueryParams& QueryParam, const struct FCollisionObjectQueryParams & ObjectParam, const bool bMultitrace);

#endif // WITH_PHYX


#if UE_WITH_PHYSICS

// RAYCAST

/** Trace a ray against the world and return if a blocking hit is found */
bool RaycastTest(const UWorld* World, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

/** Trace a ray against the world and return the first blocking hit */
bool RaycastSingle(const UWorld* World, struct FHitResult& OutHit, const FVector Start, const FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

/** 
 *  Trace a ray against the world and return touching hits and then first blocking hit
 *  Results are sorted, so a blocking hit (if found) will be the last element of the array
 *  Only the single closest blocking result will be generated, no tests will be done after that
 */
bool RaycastMulti(const UWorld* World, TArray<struct FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

// GEOM OVERLAP

/** Function for testing overlaps between a supplied PxGeometry and the world. Returns true if at least one overlapping shape is blocking*/
bool GeomOverlapBlockingTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

/** Function for testing overlaps between a supplied PxGeometry and the world. Returns true if anything is overlapping (blocking or touching)*/
bool GeomOverlapAnyTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

/**
* Function for overlapping a supplied PxGeometry against the world.
*/
bool GeomOverlapMulti(const UWorld* World, const struct FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

// GEOM SWEEP

/** Function used for sweeping a supplied PxGeometry against the world as a test */
bool GeomSweepTest(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

/** Function for sweeping a supplied PxGeometry against the world */
bool GeomSweepSingle(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, FHitResult& OutHit, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

/** Function for sweeping a supplied PxGeometry against the world */
bool GeomSweepMulti(const UWorld* World, const struct FCollisionShape& CollisionShape, const FQuat& Rot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);

#endif

// Note: Do not use these methods for new code, they are being phased out!
// These functions do not empty the OutOverlaps/OutHits array; they add items to them.
#if WITH_PHYSX
DEPRECATED_FORGAME(4.9, "Do not access this function directly, use the generic non-PhysX functions.")
bool GeomOverlapMulti_PhysX(const UWorld* World, const PxGeometry& PGeom, const PxTransform& PGeomPose, TArray<FOverlapResult>& OutOverlaps, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams);

DEPRECATED_FORGAME(4.9, "Do not access this function directly, use the generic non-PhysX functions.")
bool GeomSweepMulti_PhysX(const UWorld* World, const PxGeometry& PGeom, const PxQuat& PGeomRot, TArray<FHitResult>& OutHits, FVector Start, FVector End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam);
#endif



#if WITH_PHYSX

// Adapts a FCollisionShape to a PxGeometry type, used for various queries
struct FPhysXShapeAdaptor
{
public:
	FPhysXShapeAdaptor(const FQuat& Rot, const FCollisionShape& CollisionShape)
		: Rotation(PxIdentity)
	{
		// Perform other kinds of zero-extent queries as zero-extent sphere queries
		if ((CollisionShape.ShapeType != ECollisionShape::Sphere) && CollisionShape.IsNearlyZero())
		{
			PtrToUnionData = UnionData.SetSubtype<PxSphereGeometry>(PxSphereGeometry(0.0f));
		}
		else
		{
			switch (CollisionShape.ShapeType)
			{
			case ECollisionShape::Box:
				PtrToUnionData = UnionData.SetSubtype<PxBoxGeometry>(PxBoxGeometry(U2PVector(CollisionShape.GetBox())));
				Rotation = U2PQuat(Rot);
				break;
			case ECollisionShape::Sphere:
				PtrToUnionData = UnionData.SetSubtype<PxSphereGeometry>(PxSphereGeometry(CollisionShape.GetSphereRadius()));
				break;
			case ECollisionShape::Capsule:
				PtrToUnionData = UnionData.SetSubtype<PxCapsuleGeometry>(PxCapsuleGeometry(CollisionShape.GetCapsuleRadius(), CollisionShape.GetCapsuleAxisHalfLength()));
				Rotation = ConvertToPhysXCapsuleRot(Rot);
				break;
			default:
				// invalid point
				ensure(false);
			}
		}
	}

	PxGeometry& GetGeometry() const
	{
		return *PtrToUnionData;
	}

public:
	PxTransform GetGeomPose(const FVector& Pos) const
	{
		return PxTransform(U2PVector(Pos), Rotation);
	}

	PxQuat GetGeomOrientation() const
	{
		return Rotation;
	}

private:
	TUnion<PxSphereGeometry, PxBoxGeometry, PxCapsuleGeometry> UnionData;
	
	PxGeometry* PtrToUnionData;
	PxQuat Rotation;
};

#endif


#if WITH_BOX2D


#endif