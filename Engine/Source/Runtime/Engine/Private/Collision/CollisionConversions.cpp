// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#if WITH_PHYSX
#include "PhysicsPublic.h"
#include "PhysXCollision.h"
#include "CollisionDebugDrawing.h"
#include "CollisionConversions.h"
#include "Components/DestructibleComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// Sentinel for invalid query results.
static const PxQueryHit InvalidQueryHit;

// Forward declare, I don't want to move the entire function right now or we lose change history.
static bool ConvertOverlappedShapeToImpactHit(const PxLocationHit& PHit, const FVector& StartLoc, const FVector& EndLoc, FHitResult& OutResult, const PxGeometry& Geom, const PxTransform& QueryTM, const PxFilterData& QueryFilter, bool bReturnPhysMat);


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/* Validate Normal of OutResult. We're on hunt for invalid normal */
static void CheckHitResultNormal(const FHitResult& OutResult, const TCHAR* Message, const FVector& Start=FVector::ZeroVector, const FVector& End = FVector::ZeroVector, const PxGeometry* const Geom=NULL)
{
	if(!OutResult.bStartPenetrating && !OutResult.Normal.IsNormalized())
	{
		UE_LOG(LogPhysics, Warning, TEXT("(%s) Non-normalized OutResult.Normal from capsule conversion: %s (Component- %s)"), Message, *OutResult.Normal.ToString(), *GetNameSafe(OutResult.Component.Get()));
		// now I'm adding Outresult input
		UE_LOG(LogPhysics, Warning, TEXT("Start Loc(%s), End Loc(%s), Hit Loc(%s), ImpactNormal(%s)"), *Start.ToString(), *End.ToString(), *OutResult.Location.ToString(), *OutResult.ImpactNormal.ToString() );
		if (Geom != NULL)
		{
			// I only seen this crash on capsule
			if (Geom->getType() == PxGeometryType::eCAPSULE)
			{
				const PxCapsuleGeometry * Capsule = (PxCapsuleGeometry*)Geom;
				UE_LOG(LogPhysics, Warning, TEXT("Capsule radius (%f), Capsule Halfheight (%f)"), Capsule->radius, Capsule->halfHeight);
			}
		}
		check(OutResult.Normal.IsNormalized());
	}
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)


static FORCEINLINE bool PxQuatIsIdentity(PxQuat const& Q)
{
	return
		Q.x == 0.f &&
		Q.y == 0.f &&
		Q.z == 0.f &&
		Q.w == 1.f;
}


/** Helper to transform a normal when non-uniform scale is present. */
static PxVec3 TransformNormalToShapeSpace(const PxMeshScale& meshScale, const PxVec3& nIn)
{
	// Uniform scale makes this unnecessary
	if (meshScale.scale.x == meshScale.scale.y &&
		meshScale.scale.x == meshScale.scale.z)
	{
		return nIn;
	}
	
	if (PxQuatIsIdentity(meshScale.rotation))
	{
		// Inverse transpose: inverse is 1/scale, transpose = original when rotation is identity.
		const PxVec3 tmp = PxVec3(nIn.x / meshScale.scale.x, nIn.y / meshScale.scale.y, nIn.z / meshScale.scale.z);
		const PxReal denom = 1.0f / tmp.magnitude();
		return tmp * denom;
	}
	else
	{
		const PxMat33 rot(meshScale.rotation);
		const PxMat33 diagonal = PxMat33::createDiagonal(meshScale.scale);
		const PxMat33 vertex2Shape = (rot.getTranspose() * diagonal) * rot;

		const PxMat33 shape2Vertex = vertex2Shape.getInverse();
		const PxVec3 tmp = shape2Vertex.transformTranspose(nIn);
		const PxReal denom = 1.0f / tmp.magnitude();
		return tmp * denom;
	}
}

static bool FindSimpleOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, FVector& OutNormal)
{
	const bool bNormalData = PHit.flags & PxHitFlag::eNORMAL;
	OutNormal = bNormalData ? P2UVector(PHit.normal) : -TraceDirectionDenorm.GetSafeNormal();
	return true;
}

static bool FindHeightFieldOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, FVector& OutNormal)
{

	PxHeightFieldGeometry PHeightFieldGeom;
	bool bSuccess = PHit.shape->getHeightFieldGeometry(PHeightFieldGeom);
	check(bSuccess);	//we should only call this function when we have a heightfield
	if (PHeightFieldGeom.heightField)
	{
		const PxU32 TriIndex = PHit.faceIndex;
		const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);

		PxTriangle Tri;
		PxMeshQuery::getTriangle(PHeightFieldGeom, PShapeWorldPose, TriIndex, Tri);

		PxVec3 TriNormal;
		Tri.normal(TriNormal);
		OutNormal = P2UVector(TriNormal);

		return true;
	}

	return false;
}

static bool FindConvexMeshOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, FVector& OutNormal)
{
	PxConvexMeshGeometry PConvexMeshGeom;
	bool bSuccess = PHit.shape->getConvexMeshGeometry(PConvexMeshGeom);
	check(bSuccess);	//should only call this function when we have a convex mesh

	if (PConvexMeshGeom.convexMesh)
	{
		check(PHit.faceIndex < PConvexMeshGeom.convexMesh->getNbPolygons());

		const PxU32 PolyIndex = PHit.faceIndex;
		PxHullPolygon PPoly;
		bool bSuccessData = PConvexMeshGeom.convexMesh->getPolygonData(PolyIndex, PPoly);
		if (bSuccessData)
		{
			// Account for non-uniform scale in local space normal.
			const PxVec3 PPlaneNormal(PPoly.mPlane[0], PPoly.mPlane[1], PPoly.mPlane[2]);
			const PxVec3 PLocalPolyNormal = TransformNormalToShapeSpace(PConvexMeshGeom.scale, PPlaneNormal.getNormalized());

			// Convert to world space
			const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);
			const PxVec3 PWorldPolyNormal = PShapeWorldPose.rotate(PLocalPolyNormal);
			OutNormal = P2UVector(PWorldPolyNormal);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (!OutNormal.IsNormalized())
			{
				UE_LOG(LogPhysics, Warning, TEXT("Non-normalized Normal (Hit shape is ConvexMesh): %s (LocalPolyNormal:%s)"), *OutNormal.ToString(), *P2UVector(PLocalPolyNormal).ToString());
				UE_LOG(LogPhysics, Warning, TEXT("WorldTransform \n: %s"), *P2UTransform(PShapeWorldPose).ToString());
			}
#endif
			return true;
		}
	}

	return false;
}

static bool FindTriMeshOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, FVector& OutNormal)
{
	PxTriangleMeshGeometry PTriMeshGeom;
	bool bSuccess = PHit.shape->getTriangleMeshGeometry(PTriMeshGeom);
	check(bSuccess);	//this function should only be called when we have a trimesh

	if (PTriMeshGeom.triangleMesh)
	{
		check(PHit.faceIndex < PTriMeshGeom.triangleMesh->getNbTriangles());

		const PxU32 TriIndex = PHit.faceIndex;
		const void* Triangles = PTriMeshGeom.triangleMesh->getTriangles();

		// Grab triangle indices that we hit
		int32 I0, I1, I2;

		if (PTriMeshGeom.triangleMesh->getTriangleMeshFlags() & PxTriangleMeshFlag::eHAS_16BIT_TRIANGLE_INDICES)
		{
			PxU16* P16BitIndices = (PxU16*)Triangles;
			I0 = P16BitIndices[(TriIndex * 3) + 0];
			I1 = P16BitIndices[(TriIndex * 3) + 1];
			I2 = P16BitIndices[(TriIndex * 3) + 2];
		}
		else
		{
			PxU32* P32BitIndices = (PxU32*)Triangles;
			I0 = P32BitIndices[(TriIndex * 3) + 0];
			I1 = P32BitIndices[(TriIndex * 3) + 1];
			I2 = P32BitIndices[(TriIndex * 3) + 2];
		}

		// Get verts we hit (local space)
		const PxVec3* PVerts = PTriMeshGeom.triangleMesh->getVertices();
		const PxVec3 V0 = PVerts[I0];
		const PxVec3 V1 = PVerts[I1];
		const PxVec3 V2 = PVerts[I2];

		// Find normal of triangle (local space), and account for non-uniform scale
		const PxVec3 PTempNormal = ((V1 - V0).cross(V2 - V0)).getNormalized();
		const PxVec3 PLocalTriNormal = TransformNormalToShapeSpace(PTriMeshGeom.scale, PTempNormal);

		// Convert to world space
		const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PHit.shape, *PHit.actor);
		const PxVec3 PWorldTriNormal = PShapeWorldPose.rotate(PLocalTriNormal);
		OutNormal = P2UVector(PWorldTriNormal);

		if (PTriMeshGeom.meshFlags & PxMeshGeometryFlag::eDOUBLE_SIDED)
		{
			//double sided mesh so we need to consider direction of query
			const float sign = FVector::DotProduct(OutNormal, TraceDirectionDenorm) > 0.f ? -1.f : 1.f;
			OutNormal *= sign;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!OutNormal.IsNormalized())
		{
			UE_LOG(LogPhysics, Warning, TEXT("Non-normalized Normal (Hit shape is TriangleMesh): %s (V0:%s, V1:%s, V2:%s)"), *OutNormal.ToString(), *P2UVector(V0).ToString(), *P2UVector(V1).ToString(), *P2UVector(V2).ToString());
			UE_LOG(LogPhysics, Warning, TEXT("WorldTransform \n: %s"), *P2UTransform(PShapeWorldPose).ToString());
		}
#endif
		return true;
	}

	return false;
}

/**
 * Util to find the normal of the face that we hit.
 * @param PHit - incoming hit from PhysX
 * @param TraceDirectionDenorm - direction of sweep test (not normalized)
 * @param OutNormal - normal we may recompute based on the faceIndex of the hit
 * @return true if we compute a new normal for the geometry.
 */
static bool FindGeomOpposingNormal(const PxLocationHit& PHit, const FVector& TraceDirectionDenorm, FVector& OutNormal)
{
	checkf(InvalidQueryHit.faceIndex == 0xFFFFffff, TEXT("Engine code needs fixing: PhysX invalid face index sentinel has changed or is not part of default PxQueryHit!"));
	if (PHit.faceIndex == InvalidQueryHit.faceIndex)
	{
		return false;
	}

	PxGeometryType::Enum GeomType = PHit.shape->getGeometryType();
	switch (GeomType)
	{
		case PxGeometryType::eSPHERE:
		case PxGeometryType::eBOX:
		case PxGeometryType::eCAPSULE:		return FindSimpleOpposingNormal(PHit, TraceDirectionDenorm, OutNormal);
		case PxGeometryType::eCONVEXMESH:	return FindConvexMeshOpposingNormal(PHit, TraceDirectionDenorm, OutNormal);
		case PxGeometryType::eHEIGHTFIELD:	return FindHeightFieldOpposingNormal(PHit, TraceDirectionDenorm, OutNormal);
		case PxGeometryType::eTRIANGLEMESH:	return FindTriMeshOpposingNormal(PHit, TraceDirectionDenorm, OutNormal);
		default: check(false);	//unsupported geom type
	}

	return false;
}


/** Set info in the HitResult (Actor, Component, PhysMaterial, BoneName, Item) based on the supplied shape and face index */
static void SetHitResultFromShapeAndFaceIndex(const PxShape* PShape,  const PxRigidActor* PActor, const uint32 FaceIndex, FHitResult& OutResult, bool bReturnPhysMat)
{
	FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(PShape->userData);
	FDestructibleChunkInfo* ChunkInfo = FPhysxUserData::Get<FDestructibleChunkInfo>(PShape->userData);
	UPrimitiveComponent* PrimComp = FPhysxUserData::Get<UPrimitiveComponent>(PShape->userData);

	if ((BodyInst == NULL && ChunkInfo == NULL))
	{
		BodyInst = FPhysxUserData::Get<FBodyInstance>(PActor->userData);
		ChunkInfo = FPhysxUserData::Get<FDestructibleChunkInfo>(PActor->userData);
	}

	UPrimitiveComponent* OwningComponent = ChunkInfo != NULL ? ChunkInfo->OwningComponent.Get() : NULL;
	if (OwningComponent == NULL && BodyInst != NULL)
	{
		OwningComponent = BodyInst->OwnerComponent.Get();
	}

	// If the shape has a different parent component, we take that one instead of the ChunkInfo. This can happen in some 
	// cases where APEX moves shapes internally to another actor ( ex. FormExtended structures )
	if (PrimComp != NULL && OwningComponent != PrimComp)
	{
		OwningComponent = PrimComp;
	}

	OutResult.PhysMaterial = NULL;

	bool bReturnBody = false;

	// Grab actor/component
	if( OwningComponent )
	{
		OutResult.Actor = OwningComponent->GetOwner();
		OutResult.Component = OwningComponent;

		if (bReturnPhysMat && FaceIndex != InvalidQueryHit.faceIndex)
		{
			// @fixme: only do this for InGameThread, otherwise, this will be done in AsyncTrace
			if ( IsInGameThread() )
			{
				// This function returns the single material in all cases other than trimesh or heightfield
				PxMaterial* PxMat = PShape->getMaterialFromInternalFaceIndex(FaceIndex);
				if(PxMat != NULL)
				{
					OutResult.PhysMaterial = FPhysxUserData::Get<UPhysicalMaterial>(PxMat->userData);
				}
			}
			else
			{
				//@fixme: this will be fixed properly when we can make FBodyInstance to be TWeakPtr - TTP (263842)
			}
		}

		bReturnBody = OwningComponent->bMultiBodyOverlap;
	}

	// For destructibles give the ChunkInfo-Index as Item
	if (bReturnBody && ChunkInfo)
	{
		OutResult.Item = ChunkInfo->Index;

		UDestructibleComponent* DMComp = Cast<UDestructibleComponent>(OwningComponent);
		OutResult.BoneName = DMComp->GetBoneName(UDestructibleComponent::ChunkIdxToBoneIdx(ChunkInfo->ChunkIndex));
	}
	// If BodyInstance and not destructible, give BodyIndex as Item
	else if (BodyInst)
	{
		OutResult.Item = BodyInst->InstanceBodyIndex;

		if (BodyInst->BodySetup.IsValid())
		{
			OutResult.BoneName = BodyInst->BodySetup->BoneName;
		}
	}
	else
	{
		// invalid index
		OutResult.Item = INDEX_NONE;
		OutResult.BoneName = NAME_None;
	}
}

void ConvertQueryImpactHit(const PxLocationHit& PHit, FHitResult& OutResult, float CheckLength, const PxFilterData& QueryFilter, const FVector& StartLoc, const FVector& EndLoc, const PxGeometry* const Geom, const PxTransform& QueryTM, bool bReturnFaceIndex, bool bReturnPhysMat)
{
	checkSlow(PHit.flags & PxHitFlag::eDISTANCE);
	if (Geom != NULL && PHit.hadInitialOverlap())
	{
		ConvertOverlappedShapeToImpactHit(PHit, StartLoc, EndLoc, OutResult, *Geom, QueryTM, QueryFilter, bReturnPhysMat);
		return;
	}

	SetHitResultFromShapeAndFaceIndex(PHit.shape,  PHit.actor, PHit.faceIndex, OutResult, bReturnPhysMat);

	// calculate the hit time
	const float HitTime = PHit.distance/CheckLength;

	// figure out where the the "safe" location for this shape is by moving from the startLoc toward the ImpactPoint
	const FVector TraceStartToEnd = EndLoc - StartLoc;
	const FVector SafeLocationToFitShape = StartLoc + (HitTime * TraceStartToEnd);

	// Other info
	OutResult.Location = SafeLocationToFitShape;
	OutResult.ImpactPoint = (PHit.flags & PxHitFlag::ePOSITION) ? P2UVector(PHit.position) : StartLoc;
	OutResult.Normal = (PHit.flags & PxHitFlag::eNORMAL) ? P2UVector(PHit.normal) : -TraceStartToEnd.GetSafeNormal();
	OutResult.ImpactNormal = OutResult.Normal;

	OutResult.TraceStart = StartLoc;
	OutResult.TraceEnd = EndLoc;
	OutResult.Time = HitTime;

	// See if this is a 'blocking' hit
	PxFilterData PShapeFilter = PHit.shape->getQueryFilterData();
	PxSceneQueryHitType::Enum HitType = FPxQueryFilterCallback::CalcQueryHitType(QueryFilter, PShapeFilter);
	OutResult.bBlockingHit = (HitType == PxSceneQueryHitType::eBLOCK); 
	OutResult.bStartPenetrating = (PxU32)(PHit.hadInitialOverlap());


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST || !WITH_EDITOR)
	CheckHitResultNormal(OutResult, TEXT("Invalid Normal from ConvertQueryImpactHit"), StartLoc, EndLoc, Geom);
#endif

	PxGeometryType::Enum GeometryType = Geom ? Geom->getType() : PxGeometryType::eINVALID;

	// Special handling for swept-capsule results
	if (GeometryType == PxGeometryType::eCAPSULE || GeometryType == PxGeometryType::eSPHERE)
	{
		FindGeomOpposingNormal(PHit, TraceStartToEnd, OutResult.ImpactNormal);
	}
	
	if( PHit.shape->getGeometryType() == PxGeometryType::eHEIGHTFIELD)
	{
		// Lookup physical material for heightfields
		if (PHit.faceIndex != InvalidQueryHit.faceIndex)
		{
			PxMaterial* HitMaterial = PHit.shape->getMaterialFromInternalFaceIndex(PHit.faceIndex);
			if (HitMaterial != NULL)
			{
				OutResult.PhysMaterial = FPhysxUserData::Get<UPhysicalMaterial>(HitMaterial->userData);
			}
		}
	}
	else
	if(bReturnFaceIndex && PHit.shape->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
	{
		PxTriangleMeshGeometry PTriMeshGeom;
		if(	PHit.shape->getTriangleMeshGeometry(PTriMeshGeom) && 
			PTriMeshGeom.triangleMesh != NULL &&
			PHit.faceIndex < PTriMeshGeom.triangleMesh->getNbTriangles() )
		{
			OutResult.FaceIndex	= PTriMeshGeom.triangleMesh->getTrianglesRemap()[PHit.faceIndex];
		}
		else
		{
			OutResult.FaceIndex	= INDEX_NONE;
		}
	}
	else
	{
		OutResult.FaceIndex	= INDEX_NONE;
	}
}

void ConvertRaycastResults(int32 NumHits, PxRaycastHit* Hits, float CheckLength, const PxFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, bool bReturnFaceIndex, bool bReturnPhysMat)
{
	PxTransform PStartTM(U2PVector(StartLoc));
	for(int32 i=0; i<NumHits; i++)
	{
		FHitResult NewResult(ForceInit);
		const PxRaycastHit& PHit = Hits[i];

		ConvertQueryImpactHit(PHit, NewResult, CheckLength, QueryFilter, StartLoc, EndLoc, NULL, PStartTM, bReturnFaceIndex, bReturnPhysMat);

		OutHits.Add(NewResult);
	}

	// Sort results from first to last hit
	OutHits.Sort( FCompareFHitResultTime() );
}

bool AddSweepResults(int32 NumHits, const PxSweepHit* Hits, float CheckLength, const PxFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, const PxGeometry& Geom, const PxTransform& QueryTM, float MaxDistance, bool bReturnPhysMat)
{
	OutHits.Reserve(NumHits);
	bool bHadBlockingHit = false;

	for(int32 i=0; i<NumHits; i++)
	{
		const PxSweepHit& PHit = Hits[i];
		checkSlow(PHit.flags & PxHitFlag::eDISTANCE);
		if(PHit.distance <= MaxDistance)
		{
			FHitResult NewResult(ForceInit);
			ConvertQueryImpactHit(PHit, NewResult, CheckLength, QueryFilter, StartLoc, EndLoc, &Geom, QueryTM, false, bReturnPhysMat);
			bHadBlockingHit |= NewResult.bBlockingHit;
			OutHits.Add(NewResult);
		}
	}

	// Sort results from first to last hit
	OutHits.Sort( FCompareFHitResultTime() );
	return bHadBlockingHit;
}

#define DRAW_OVERLAPPING_TRIS 0

/* Function to find the best normal from the list of triangles that are overlapping our geom. */
template<typename GeomType>
FVector FindBestOverlappingNormal(const PxGeometry& Geom, const PxTransform& QueryTM, const GeomType& ShapeGeom, const PxTransform& PShapeWorldPose, PxU32* HitTris, int32 NumTrisHit)
{
#if DRAW_OVERLAPPING_TRIS
	TArray<FOverlapResult> Overlaps;
	DrawGeomOverlaps(World, Geom, QueryTM, Overlaps);

	TArray<FBatchedLine> Lines;
	const FLinearColor LineColor = FLinearColor(1.f, 0.7f, 0.7f);
	const FLinearColor NormalColor = FLinearColor(1.f, 1.f, 1.f);
	const float Lifetime = 5.f;
#endif // DRAW_OVERLAPPING_TRIS

	// Track the best triangle plane distance
	float BestPlaneDist = -BIG_NUMBER;
	FVector BestPlaneNormal(0, 0, 1);
	// Iterate over triangles
	for (int32 TriIdx = 0; TriIdx < NumTrisHit; TriIdx++)
	{
		PxTriangle Tri;
		PxMeshQuery::getTriangle(ShapeGeom, PShapeWorldPose, HitTris[TriIdx], Tri);

		const FVector A = P2UVector(Tri.verts[0]);
		const FVector B = P2UVector(Tri.verts[1]);
		const FVector C = P2UVector(Tri.verts[2]);

		FVector TriNormal = ((B - A) ^ (C - A));

		// Use a more accurate normalization that avoids InvSqrtEst
		const float TriNormalSize = TriNormal.Size();
		TriNormal = (TriNormalSize >= KINDA_SMALL_NUMBER ? TriNormal / TriNormalSize : FVector::ZeroVector);

		const FPlane TriPlane(A, TriNormal);

		const FVector QueryCenter = P2UVector(QueryTM.p);
		const float DistToPlane = TriPlane.PlaneDot(QueryCenter);

		if (DistToPlane > BestPlaneDist)
		{
			BestPlaneDist = DistToPlane;
			BestPlaneNormal = TriNormal;
		}

#if DRAW_OVERLAPPING_TRIS
		Lines.Add(FBatchedLine(A, B, LineColor, Lifetime, 0.1f, SDPG_Foreground));
		Lines.Add(FBatchedLine(B, C, LineColor, Lifetime, 0.1f, SDPG_Foreground));
		Lines.Add(FBatchedLine(C, A, LineColor, Lifetime, 0.1f, SDPG_Foreground));
		Lines.Add(FBatchedLine(A, A + (50.f*TriNormal), NormalColor, Lifetime, 0.1f, SDPG_Foreground));
#endif // DRAW_OVERLAPPING_TRIS
	}

#if DRAW_OVERLAPPING_TRIS
	if (World->PersistentLineBatcher)
	{
		World->PersistentLineBatcher->DrawLines(Lines);
	}
#endif // DRAW_OVERLAPPING_TRIS

	return BestPlaneNormal;
}



static bool ComputeInflatedMTD_Internal(const float MtdInflation, const PxLocationHit& PHit, FHitResult& OutResult, const PxTransform& QueryTM, const PxGeometry& Geom, const PxTransform& PShapeWorldPose)
{
	PxGeometry* InflatedGeom = NULL;

	PxVec3 PxMtdNormal(0.f);
	PxF32 PxMtdDepth = 0.f;
	const PxGeometry& POtherGeom = PHit.shape->getGeometry().any();
	const bool bMtdResult = PxGeometryQuery::computePenetration(PxMtdNormal, PxMtdDepth, Geom, QueryTM, POtherGeom, PShapeWorldPose);
	if (bMtdResult)
	{
		if (PxMtdNormal.isFinite())
		{
			OutResult.ImpactNormal = P2UVector(PxMtdNormal);
			OutResult.PenetrationDepth = FMath::Max(FMath::Abs(PxMtdDepth) - MtdInflation, 0.f) + KINDA_SMALL_NUMBER;
			return true;
		}
		else
		{
			UE_LOG(LogPhysics, Verbose, TEXT("Warning: ComputeInflatedMTD_Internal: MTD returned NaN :( normal: (X:%f, Y:%f, Z:%f)"), PxMtdNormal.x, PxMtdNormal.y, PxMtdNormal.z);
		}
	}

	return false;
}


// Compute depenetration vector and distance if possible with a slightly larger geometry
static bool ComputeInflatedMTD(const float MtdInflation, const PxLocationHit& PHit, FHitResult& OutResult, const PxTransform& QueryTM, const PxGeometry& Geom, const PxTransform& PShapeWorldPose)
{
	switch(Geom.getType())
	{
		case PxGeometryType::eCAPSULE:
		{
			const PxCapsuleGeometry* InCapsule = static_cast<const PxCapsuleGeometry*>(&Geom);
			PxCapsuleGeometry InflatedCapsule(InCapsule->radius + MtdInflation, InCapsule->halfHeight); // don't inflate halfHeight, radius is added all around.
			return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedCapsule, PShapeWorldPose);
		}

		case PxGeometryType::eBOX:
		{
			const PxBoxGeometry* InBox = static_cast<const PxBoxGeometry*>(&Geom);
			PxBoxGeometry InflatedBox(InBox->halfExtents + PxVec3(MtdInflation));
			return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedBox, PShapeWorldPose);
		}

		case PxGeometryType::eSPHERE:
		{
			const PxSphereGeometry* InSphere = static_cast<const PxSphereGeometry*>(&Geom);
			PxSphereGeometry InflatedSphere(InSphere->radius + MtdInflation);
			return ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, QueryTM, InflatedSphere, PShapeWorldPose);
		}

		case PxGeometryType::eCONVEXMESH:
		{
			// We can't exactly inflate the mesh (not easily), so try jittering it a bit to get an MTD result.
			PxVec3 TraceDir = U2PVector(OutResult.TraceEnd - OutResult.TraceStart);
			TraceDir.normalizeSafe();

			// Try forward (in trace direction)
			PxTransform JitteredTM = PxTransform(QueryTM.p + (TraceDir * MtdInflation), QueryTM.q);
			if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
			{
				return true;
			}

			// Try backward (opposite trace direction)
			JitteredTM = PxTransform(QueryTM.p - (TraceDir * MtdInflation), QueryTM.q);
			if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
			{
				return true;
			}

			// Try axial directions.
			// Start with -Z because this is the most common case (objects on the floor).
			for (int32 i=2; i >= 0; i--)
			{
				PxVec3 Jitter(0.f);
				Jitter[i] = MtdInflation;

				JitteredTM = PxTransform(QueryTM.p - Jitter, QueryTM.q);
				if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
				{
					return true;
				}

				JitteredTM = PxTransform(QueryTM.p + Jitter, QueryTM.q);
				if (ComputeInflatedMTD_Internal(MtdInflation, PHit, OutResult, JitteredTM, Geom, PShapeWorldPose))
				{
					return true;
				}
			}

			return false;
		}

		default:
		{
			return false;
		}
	}
}


/** Util to convert an overlapped shape into a sweep hit result, returns whether it was a blocking hit. */
static bool ConvertOverlappedShapeToImpactHit(const PxLocationHit& PHit, const FVector& StartLoc, const FVector& EndLoc, FHitResult& OutResult, const PxGeometry& Geom, const PxTransform& QueryTM, const PxFilterData& QueryFilter, bool bReturnPhysMat)
{
	OutResult.TraceStart = StartLoc;
	OutResult.TraceEnd = EndLoc;

	const PxShape* PShape = PHit.shape;
	const PxRigidActor* PActor = PHit.actor;
	const uint32 FaceIdx = PHit.faceIndex;

	SetHitResultFromShapeAndFaceIndex(PShape, PActor, FaceIdx, OutResult, bReturnPhysMat);

	// Time of zero because initially overlapping
	OutResult.Time = 0.f;
	OutResult.bStartPenetrating = true;

	// See if this is a 'blocking' hit
	PxFilterData PShapeFilter = PShape->getQueryFilterData();
	PxSceneQueryHitType::Enum HitType = FPxQueryFilterCallback::CalcQueryHitType(QueryFilter, PShapeFilter);
	OutResult.bBlockingHit = (HitType == PxSceneQueryHitType::eBLOCK); 

	// Return start location as 'safe location'
	OutResult.Location = P2UVector(QueryTM.p);
	OutResult.ImpactPoint = OutResult.Location; // @todo not really sure of a better thing to do here...

	const bool bFiniteNormal = PHit.normal.isFinite();
	const bool bValidNormal = (PHit.flags & PxHitFlag::eNORMAL) && bFiniteNormal;

	// Use MTD result if possible. We interpret the MTD vector as both the direction to move and the opposing normal.
	if (bValidNormal)
	{
		OutResult.ImpactNormal = P2UVector(PHit.normal);
		OutResult.PenetrationDepth = FMath::Abs(PHit.distance);
	}
	else
	{
		// Fallback normal if we can't find it with MTD or otherwise.
		OutResult.ImpactNormal = FVector::UpVector;
		OutResult.PenetrationDepth = 0.f;
		if (!bFiniteNormal)
		{
			UE_LOG(LogPhysics, Verbose, TEXT("Warning: ConvertOverlappedShapeToImpactHit: MTD returned NaN :( normal: (X:%f, Y:%f, Z:%f)"), PHit.normal.x, PHit.normal.y, PHit.normal.z);
		}
	}

	// Zero-distance hits are often valid hits and we can extract the hit normal from a larger shape with MTD.
	// For invalid normals we can try other methods as well (get overlapping triangles).

	if (PHit.distance == 0.f || !bValidNormal)
	{
		const PxTransform PShapeWorldPose = PxShapeExt::getGlobalPose(*PShape, *PActor);

		// Try MTD with a small inflation for better accuracy, then a larger one in case the first one fails due to precision issues.
		static const float SmallMtdInflation = 0.250f;
		static const float LargeMtdInflation = 2.500f;

		if (ComputeInflatedMTD(SmallMtdInflation, PHit, OutResult, QueryTM, Geom, PShapeWorldPose) ||
			ComputeInflatedMTD(LargeMtdInflation, PHit, OutResult, QueryTM, Geom, PShapeWorldPose))
		{
			// Success
		}
		else
		{
			PxTriangleMeshGeometry PTriMeshGeom;
			PxHeightFieldGeometry PHeightfieldGeom;

			if (PShape->getTriangleMeshGeometry(PTriMeshGeom) || PShape->getHeightFieldGeometry(PHeightfieldGeom))
			{
				PxGeometryType::Enum GeometryType = PShape->getGeometryType();
				const bool bIsTriMesh = (GeometryType == PxGeometryType::eTRIANGLEMESH);
				PxU32 HitTris[64];
				bool bOverflow = false;

				const int32 NumTrisHit = bIsTriMesh ? PxMeshQuery::findOverlapTriangleMesh(Geom, QueryTM, PTriMeshGeom, PShapeWorldPose, HitTris, ARRAY_COUNT(HitTris), 0, bOverflow) :
													  PxMeshQuery::findOverlapHeightField( Geom, QueryTM, PHeightfieldGeom, PShapeWorldPose, HitTris, ARRAY_COUNT(HitTris), 0, bOverflow);
				if (NumTrisHit > 0)
				{
					if (bIsTriMesh)
					{
						OutResult.ImpactNormal = FindBestOverlappingNormal(Geom, QueryTM, PTriMeshGeom, PShapeWorldPose, HitTris, NumTrisHit);
					}
					else
					{
						OutResult.ImpactNormal = FindBestOverlappingNormal(Geom, QueryTM, PHeightfieldGeom, PShapeWorldPose, HitTris, NumTrisHit);
					}

				}
			}
			else
			{
				// Not a tri-mesh or heightfield

				// MTD failed, use point distance. This is not ideal.
				// Note: faceIndex seems to be unreliable for convex meshes in these cases, so not using FindGeomOpposingNormal() for them here.
				PxGeometry& PGeom = PShape->getGeometry().any();
				PxVec3 PClosestPoint;
				const float Distance = PxGeometryQuery::pointDistance(QueryTM.p, PGeom, PShapeWorldPose, &PClosestPoint);

				if (Distance < KINDA_SMALL_NUMBER)
				{
					UE_LOG(LogCollision, Verbose, TEXT("Warning: ConvertOverlappedShapeToImpactHit: Query origin inside shape, giving poor MTD."));
					PClosestPoint = PxShapeExt::getWorldBounds(*PShape, *PActor).getCenter();
				}

				OutResult.ImpactNormal = (OutResult.Location - P2UVector(PClosestPoint)).GetSafeNormal();
			}
		}
	}

	OutResult.Normal = OutResult.ImpactNormal;
	return OutResult.bBlockingHit;
}


void ConvertQueryOverlap(const PxShape* PShape, const PxRigidActor* PActor, FOverlapResult& OutOverlap, const PxFilterData& QueryFilter)
{
	// See if this is a 'blocking' hit
	PxFilterData PShapeFilter = PShape->getQueryFilterData();
	// word1 is block, word2 is overlap
	PxSceneQueryHitType::Enum HitType = FPxQueryFilterCallback::CalcQueryHitType(QueryFilter, PShapeFilter);
	bool bBlock = (HitType == PxSceneQueryHitType::eBLOCK); 

	FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(PActor->userData);
	FDestructibleChunkInfo* ChunkInfo = FPhysxUserData::Get<FDestructibleChunkInfo>(PActor->userData);

	// Grab actor/component
	if(BodyInst && BodyInst->OwnerComponent.IsValid())
	{
		OutOverlap.Actor = BodyInst->OwnerComponent->GetOwner();
		OutOverlap.Component = BodyInst->OwnerComponent;
		OutOverlap.ItemIndex = BodyInst->OwnerComponent->bMultiBodyOverlap ? BodyInst->InstanceBodyIndex : INDEX_NONE;
	}
	else if (ChunkInfo && ChunkInfo->OwningComponent.IsValid())
	{
		OutOverlap.Actor = ChunkInfo->OwningComponent->GetOwner();
		OutOverlap.Component = ChunkInfo->OwningComponent.Get();
		OutOverlap.ItemIndex = ChunkInfo->OwningComponent->bMultiBodyOverlap ? UDestructibleComponent::ChunkIdxToBoneIdx(ChunkInfo->ChunkIndex) : INDEX_NONE;
	}

	// Other info
	OutOverlap.bBlockingHit = bBlock;
}

/** Util to add NewOverlap to OutOverlaps if it is not already there */
static void AddUniqueOverlap(TArray<FOverlapResult>& OutOverlaps, const FOverlapResult& NewOverlap)
{
	// Look to see if we already have this overlap (based on component)
	for(int32 TestIdx=0; TestIdx<OutOverlaps.Num(); TestIdx++)
	{
		FOverlapResult& Overlap = OutOverlaps[TestIdx];

		if(Overlap.Component == NewOverlap.Component && Overlap.ItemIndex == NewOverlap.ItemIndex)
		{
			// These should be the same if the component matches!
			check(Overlap.Actor == NewOverlap.Actor);

			// If we had a non-blocking overlap with this component, but now we have a blocking one, use that one instead!
			if(!Overlap.bBlockingHit && NewOverlap.bBlockingHit)
			{
				Overlap = NewOverlap;
			}

			return;
		}
	}

	// Not found, so add it 
	OutOverlaps.Add(NewOverlap);
}

bool ConvertOverlapResults(int32 NumOverlaps, PxOverlapHit* POverlapResults, const PxFilterData& QueryFilter, TArray<FOverlapResult>& OutOverlaps)
{
	OutOverlaps.Reserve(NumOverlaps);
	bool bBlockingFound = false;

	for(int32 i=0; i<NumOverlaps; i++)
	{
		FOverlapResult NewOverlap;		
		
		ConvertQueryOverlap( POverlapResults[i].shape, POverlapResults[i].actor, NewOverlap, QueryFilter);


		if(NewOverlap.bBlockingHit)
		{
			bBlockingFound = true;
		}

		AddUniqueOverlap(OutOverlaps, NewOverlap);
	}

	return bBlockingFound;
}


#endif // WITH_PHYSX
