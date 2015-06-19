// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BodySetup.cpp
=============================================================================*/ 

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "TargetPlatform.h"
#include "AnimTree.h"

#if WITH_PHYSX
	#include "PhysXSupport.h"
#endif // WITH_PHYSX


#include "PhysDerivedData.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#if WITH_PHYSX
	namespace
	{
		// Quaternion that converts Sphyls from UE space to PhysX space (negate Y, swap X & Z)
		// This is equivalent to a 180 degree rotation around the normalized (1, 0, 1) axis
		static const PxQuat U2PSphylBasis( PI, PxVec3( 1.0f / FMath::Sqrt( 2.0f ), 0.0f, 1.0f / FMath::Sqrt( 2.0f ) ) );
	}
#endif // WITH_PHYSX

// CVars
static TAutoConsoleVariable<float> CVarContactOffsetFactor(
	TEXT("p.ContactOffsetFactor"),
	0.01f,
	TEXT("Multiplied by min dimension of object to calculate how close objects get before generating contacts. Default: 0.01"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarMaxContactOffset(
	TEXT("p.MaxContactOffset"),
	1.f,
	TEXT("Max value of contact offset, which controls how close objects get before generating contacts. Default: 1.0"),
	ECVF_Default);

DEFINE_LOG_CATEGORY(LogPhysics);
UBodySetup::UBodySetup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bConsiderForBounds = true;
	bMeshCollideAll = false;
	CollisionTraceFlag = CTF_UseDefault;
	bHasCookedCollisionData = true;
	bGenerateMirroredCollision = true;
	bGenerateNonMirroredCollision = true;
	DefaultInstance.SetObjectType(ECC_PhysicsBody);
	BuildScale_DEPRECATED = 1.0f;
	BuildScale3D = FVector(1.0f, 1.0f, 1.0f);
	SetFlags(RF_Transactional);
	bSharedCookedData = false;
	CookedFormatDataOverride = nullptr;
}

void UBodySetup::CopyBodyPropertiesFrom(const UBodySetup* FromSetup)
{
	AggGeom = FromSetup->AggGeom;

	// clear pointers copied from other BodySetup, as 
	for (int32 i = 0; i < AggGeom.ConvexElems.Num(); i++)
	{
		FKConvexElem& ConvexElem = AggGeom.ConvexElems[i];
		ConvexElem.ConvexMesh = NULL;
		ConvexElem.ConvexMeshNegX = NULL;
	}

	DefaultInstance.CopyBodyInstancePropertiesFrom(&FromSetup->DefaultInstance);
	PhysMaterial = FromSetup->PhysMaterial;
	PhysicsType = FromSetup->PhysicsType;
	bDoubleSidedGeometry = FromSetup->bDoubleSidedGeometry;
}

void UBodySetup::AddCollisionFrom(class UBodySetup* FromSetup)
{
	// Add shapes from static mesh
	AggGeom.SphereElems.Append(FromSetup->AggGeom.SphereElems);
	AggGeom.BoxElems.Append(FromSetup->AggGeom.BoxElems);
	AggGeom.SphylElems.Append(FromSetup->AggGeom.SphylElems);

	// Remember how many convex we already have
	int32 FirstNewConvexIdx = AggGeom.ConvexElems.Num();
	// copy convex
	AggGeom.ConvexElems.Append(FromSetup->AggGeom.ConvexElems);
	// clear pointers on convex elements
	for (int32 i = FirstNewConvexIdx; i < AggGeom.ConvexElems.Num(); i++)
	{
		FKConvexElem& ConvexElem = AggGeom.ConvexElems[i];
		ConvexElem.ConvexMesh = NULL;
		ConvexElem.ConvexMeshNegX = NULL;
	}
}

DECLARE_CYCLE_STAT(TEXT("Create Physics Meshes"), STAT_CreatePhysicsMeshes, STATGROUP_Physics);


void UBodySetup::CreatePhysicsMeshes()
{
	SCOPE_CYCLE_COUNTER(STAT_CreatePhysicsMeshes);

#if WITH_PHYSX
	// Create meshes from cooked data if not already done
	if(bCreatedPhysicsMeshes)
	{
		return;
	}

	// Find or create cooked physics data
	static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());
	FByteBulkData* FormatData = GetCookedData(PhysicsFormatName);
	if( FormatData )
	{
		if (FormatData->IsLocked())
		{
			// seems it's being already processed
			return;
		}

		FPhysXFormatDataReader CookedDataReader(*FormatData);

		if (CollisionTraceFlag != CTF_UseComplexAsSimple)
		{
			bool bNeedsCooking = bGenerateNonMirroredCollision && CookedDataReader.ConvexMeshes.Num() != AggGeom.ConvexElems.Num();
			bNeedsCooking = bNeedsCooking || (bGenerateMirroredCollision && CookedDataReader.ConvexMeshesNegX.Num() != AggGeom.ConvexElems.Num());
			if (bNeedsCooking)	//Because of bugs it's possible to save with out of sync cooked data. In editor we want to fixup this data
			{
#if WITH_EDITOR
				InvalidatePhysicsData();
				CreatePhysicsMeshes();
				return;
#endif
			}
		}
		
		ClearPhysicsMeshes();

		if (CollisionTraceFlag != CTF_UseComplexAsSimple)
		{

			ensure(!bGenerateNonMirroredCollision || CookedDataReader.ConvexMeshes.Num() == 0 || CookedDataReader.ConvexMeshes.Num() == AggGeom.ConvexElems.Num());
			ensure(!bGenerateMirroredCollision || CookedDataReader.ConvexMeshesNegX.Num() == 0 || CookedDataReader.ConvexMeshesNegX.Num() == AggGeom.ConvexElems.Num());

			//If the cooked data no longer has convex meshes, make sure to empty AggGeom.ConvexElems - otherwise we leave NULLS which cause issues, and we also read past the end of CookedDataReader.ConvexMeshes
			if ((bGenerateNonMirroredCollision && CookedDataReader.ConvexMeshes.Num() == 0) || (bGenerateMirroredCollision && CookedDataReader.ConvexMeshesNegX.Num() == 0))
			{
				AggGeom.ConvexElems.Empty();
			}

			for (int32 ElementIndex = 0; ElementIndex < AggGeom.ConvexElems.Num(); ElementIndex++)
			{
				FKConvexElem& ConvexElem = AggGeom.ConvexElems[ElementIndex];

				if (bGenerateNonMirroredCollision)
				{
					ConvexElem.ConvexMesh = CookedDataReader.ConvexMeshes[ElementIndex];
					FPhysxSharedData::Get().Add(ConvexElem.ConvexMesh);
				}

				if (bGenerateMirroredCollision)
				{
					ConvexElem.ConvexMeshNegX = CookedDataReader.ConvexMeshesNegX[ElementIndex];
					FPhysxSharedData::Get().Add(ConvexElem.ConvexMeshNegX);
				}
			}
		}

		if(bGenerateNonMirroredCollision)
		{
			TriMesh = CookedDataReader.TriMesh;
			FPhysxSharedData::Get().Add(TriMesh);
		}

		if(bGenerateMirroredCollision)
		{
			TriMeshNegX = CookedDataReader.TriMeshNegX;
			FPhysxSharedData::Get().Add(TriMeshNegX);
		}
	}
	else
	{
		ClearPhysicsMeshes(); // Make sure all are cleared then
	}

	// Clear the cooked data
	if (!GIsEditor && !bSharedCookedData)
	{
		CookedFormatData.FlushData();
	}

	bCreatedPhysicsMeshes = true;
#endif
}


void UBodySetup::ClearPhysicsMeshes()
{
#if WITH_PHYSX
	for(int32 i=0; i<AggGeom.ConvexElems.Num(); i++)
	{
		FKConvexElem* ConvexElem = &(AggGeom.ConvexElems[i]);

		if(ConvexElem->ConvexMesh != NULL)
		{
			// put in list for deferred release
			GPhysXPendingKillConvex.Add(ConvexElem->ConvexMesh);
			FPhysxSharedData::Get().Remove(ConvexElem->ConvexMesh);
			ConvexElem->ConvexMesh = NULL;
		}

		if(ConvexElem->ConvexMeshNegX != NULL)
		{
			// put in list for deferred release
			GPhysXPendingKillConvex.Add(ConvexElem->ConvexMeshNegX);
			FPhysxSharedData::Get().Remove(ConvexElem->ConvexMeshNegX);
			ConvexElem->ConvexMeshNegX = NULL;
		}
	}

	if(TriMesh != NULL)
	{
		GPhysXPendingKillTriMesh.Add(TriMesh);
		FPhysxSharedData::Get().Remove(TriMesh);
		TriMesh = NULL;
	}

	if(TriMeshNegX != NULL)
	{
		GPhysXPendingKillTriMesh.Add(TriMeshNegX);
		FPhysxSharedData::Get().Remove(TriMeshNegX);
		TriMeshNegX = NULL;
	}

	bCreatedPhysicsMeshes = false;
#endif

	// Also clear render info
	AggGeom.FreeRenderInfo();
}

#if WITH_PHYSX
/** Util to determine whether to use NegX version of mesh, and what transform (rotation) to apply. */
bool CalcMeshNegScaleCompensation(const FVector& InScale3D, PxTransform& POutTransform)
{
	POutTransform = PxTransform::createIdentity();

	if(InScale3D.Y > 0.f)
	{
		if(InScale3D.Z > 0.f)
		{
			// no rotation needed
		}
		else
		{
			// y pos, z neg
			POutTransform.q = PxQuat(PxPi, PxVec3(0,1,0));
		}
	}
	else
	{
		if(InScale3D.Z > 0.f)
		{
			// y neg, z pos
			POutTransform.q = PxQuat(PxPi, PxVec3(0,0,1));
		}
		else
		{
			// y neg, z neg
			POutTransform.q = PxQuat(PxPi, PxVec3(1,0,0));
		}
	}

	// Use inverted mesh if determinant is negative
	return (InScale3D.X * InScale3D.Y * InScale3D.Z) < 0.f;
}

void SetupNonUniformHelper(FVector& Scale3D, float& MinScale, float& MinScaleAbs, FVector& Scale3DAbs)
{
	// if almost zero, set min scale
	// @todo fixme
	if (Scale3D.IsNearlyZero())
	{
		// set min scale
		Scale3D = FVector(0.1f);
	}

	Scale3DAbs = Scale3D.GetAbs();
	MinScaleAbs = Scale3DAbs.GetMin();

	MinScale = FMath::Max3(Scale3D.X, Scale3D.Y, Scale3D.Z) < 0.f ? -MinScaleAbs : MinScaleAbs;	//if all three values are negative make minScale negative
	
	if (FMath::IsNearlyZero(MinScale))
	{
		// only one of them can be 0, we make sure they have mini set up correctly
		MinScale = 0.1f;
		MinScaleAbs = 0.1f;
	}
}

void GetContactOffsetParams(float& ContactOffsetFactor, float& MaxContactOffset)
{
	// Get contact offset params
	ContactOffsetFactor = CVarContactOffsetFactor.GetValueOnGameThread();
	MaxContactOffset = CVarMaxContactOffset.GetValueOnGameThread();
}

PxMaterial* GetDefaultPhysMaterial()
{
	check(GEngine->DefaultPhysMaterial != NULL);
	return GEngine->DefaultPhysMaterial->GetPhysXMaterial();
}

/** Helper struct for adding shapes into an actor.*/
struct FAddShapesHelper
{
	FAddShapesHelper(UBodySetup* InBodySetup, FBodyInstance* InOwningInstance, physx::PxRigidActor* InPDestActor, EPhysicsSceneType InSceneType, FVector& InScale3D, physx::PxMaterial* InSimpleMaterial, TArray<UPhysicalMaterial*>& InComplexMaterials, FShapeData& InShapeData, const FTransform& InRelativeTM, TArray<physx::PxShape*>* InNewShapes, const bool InShapeSharing)
	: BodySetup(InBodySetup)
	, OwningInstance(InOwningInstance)
	, PDestActor(InPDestActor)
	, SceneType(InSceneType)
	, Scale3D(InScale3D)
	, SimpleMaterial(InSimpleMaterial)
	, ComplexMaterials(InComplexMaterials)
	, ShapeData(InShapeData)
	, RelativeTM(InRelativeTM)
	, NewShapes(InNewShapes)
	, bShapeSharing(InShapeSharing)
	{
		SetupNonUniformHelper(Scale3D, MinScale, MinScaleAbs, Scale3DAbs);
		{
			float MinScaleRelative;
			float MinScaleAbsRelative;
			FVector Scale3DAbsRelative;
			FVector Scale3DRelative = RelativeTM.GetScale3D();

			SetupNonUniformHelper(Scale3DRelative, MinScaleRelative, MinScaleAbsRelative, Scale3DAbsRelative);

			MinScaleAbs *= MinScaleAbsRelative;
			Scale3DAbs.X *= Scale3DAbsRelative.X;
			Scale3DAbs.Y *= Scale3DAbsRelative.Y;
			Scale3DAbs.Z *= Scale3DAbsRelative.Z;
		}
	}

	UBodySetup* BodySetup;
	FBodyInstance* OwningInstance;
	physx::PxRigidActor* PDestActor;
	EPhysicsSceneType SceneType;
	FVector& Scale3D;
	physx::PxMaterial* SimpleMaterial;
	const TArray<UPhysicalMaterial*>& ComplexMaterials;
	const FShapeData& ShapeData;
	const FTransform& RelativeTM;
	TArray<physx::PxShape*>* NewShapes;
	const bool bShapeSharing;

	float MinScaleAbs;
	float MinScale;
	FVector Scale3DAbs;

public:
	void AddSpheresToRigidActor_AssumesLocked() const
	{
		float ContactOffsetFactor, MaxContactOffset;
		GetContactOffsetParams(ContactOffsetFactor, MaxContactOffset);

		for (int32 i = 0; i < BodySetup->AggGeom.SphereElems.Num(); i++)
		{
			const FKSphereElem& SphereElem = BodySetup->AggGeom.SphereElems[i];

			PxSphereGeometry PSphereGeom;
			PSphereGeom.radius = (SphereElem.Radius * MinScaleAbs);

			if (ensure(PSphereGeom.isValid()))
			{
				FVector LocalOrigin = RelativeTM.TransformPosition(SphereElem.Center);
				PxTransform PLocalPose(U2PVector(LocalOrigin));
				PLocalPose.p *= MinScale;

				ensure(PLocalPose.isValid());
				{
					const float ContactOffset = FMath::Min(MaxContactOffset, ContactOffsetFactor * PSphereGeom.radius);
					AttachShape_AssumesLocked(PSphereGeom, PLocalPose, ContactOffset);
				}
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("AddSpheresToRigidActor: [%s] SphereElem[%d] invalid"), *GetPathNameSafe(BodySetup->GetOuter()), i);
			}
		}
	}

	void AddBoxesToRigidActor_AssumesLocked() const
	{
		float ContactOffsetFactor, MaxContactOffset;
		GetContactOffsetParams(ContactOffsetFactor, MaxContactOffset);

		for (int32 i = 0; i < BodySetup->AggGeom.BoxElems.Num(); i++)
		{
			const FKBoxElem& BoxElem = BodySetup->AggGeom.BoxElems[i];

			PxBoxGeometry PBoxGeom;
			PBoxGeom.halfExtents.x = (0.5f * BoxElem.X * Scale3DAbs.X);
			PBoxGeom.halfExtents.y = (0.5f * BoxElem.Y * Scale3DAbs.Y);
			PBoxGeom.halfExtents.z = (0.5f * BoxElem.Z * Scale3DAbs.Z);

			FTransform BoxTransform = BoxElem.GetTransform() * RelativeTM;
			if (PBoxGeom.isValid() && BoxTransform.IsValid())
			{
				PxTransform PLocalPose(U2PTransform(BoxTransform));
				PLocalPose.p.x *= Scale3D.X;
				PLocalPose.p.y *= Scale3D.Y;
				PLocalPose.p.z *= Scale3D.Z;

				ensure(PLocalPose.isValid());
				{
					const float ContactOffset = FMath::Min(MaxContactOffset, ContactOffsetFactor * PBoxGeom.halfExtents.minElement());
					AttachShape_AssumesLocked(PBoxGeom, PLocalPose, ContactOffset);
				}
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("AddBoxesToRigidActor: [%s] BoxElems[%d] invalid or has invalid transform"), *GetPathNameSafe(BodySetup->GetOuter()), i);
			}
		}
	}

	void AddSphylsToRigidActor_AssumesLocked() const
	{
		float ContactOffsetFactor, MaxContactOffset;
		GetContactOffsetParams(ContactOffsetFactor, MaxContactOffset);

		float ScaleRadius = FMath::Max(Scale3DAbs.X, Scale3DAbs.Y);
		float ScaleLength = Scale3DAbs.Z;

		for (int32 i = 0; i < BodySetup->AggGeom.SphylElems.Num(); i++)
		{
			const FKSphylElem& SphylElem = BodySetup->AggGeom.SphylElems[i];

			// this is a bit confusing since radius and height is scaled
			// first apply the scale first 
			float Radius = FMath::Max(SphylElem.Radius * ScaleRadius, 0.1f);
			float Length = SphylElem.Length + SphylElem.Radius * 2.f;
			float HalfLength = Length * ScaleLength * 0.5f;
			Radius = FMath::Clamp(Radius, 0.1f, HalfLength);	//radius is capped by half length
			float HalfHeight = HalfLength - Radius;
			HalfHeight = FMath::Max(0.1f, HalfHeight);

			PxCapsuleGeometry PCapsuleGeom;
			PCapsuleGeom.halfHeight = HalfHeight;
			PCapsuleGeom.radius = Radius;

			if (PCapsuleGeom.isValid())
			{
				// The stored sphyl transform assumes the sphyl axis is down Z. In PhysX, it points down X, so we twiddle the matrix a bit here (swap X and Z and negate Y).
				PxTransform PLocalPose(U2PVector(RelativeTM.TransformPosition(SphylElem.Center)), U2PQuat(SphylElem.Orientation) * U2PSphylBasis);
				PLocalPose.p.x *= Scale3D.X;
				PLocalPose.p.y *= Scale3D.Y;
				PLocalPose.p.z *= Scale3D.Z;

				ensure(PLocalPose.isValid());
				{
					const float ContactOffset = FMath::Min(MaxContactOffset, ContactOffsetFactor * PCapsuleGeom.radius);
					AttachShape_AssumesLocked(PCapsuleGeom, PLocalPose, ContactOffset);
				}
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("AddSphylsToRigidActor: [%s] SphylElems[%d] invalid"), *GetPathNameSafe(BodySetup->GetOuter()), i);
			}
		}
	}

	void AddConvexElemsToRigidActor_AssumesLocked() const
	{
		float ContactOffsetFactor, MaxContactOffset;
		GetContactOffsetParams(ContactOffsetFactor, MaxContactOffset);

		for (int32 i = 0; i < BodySetup->AggGeom.ConvexElems.Num(); i++)
		{
			const FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems[i];

			if (ConvexElem.ConvexMesh)
			{
				PxTransform PLocalPose;
				bool bUseNegX = CalcMeshNegScaleCompensation(Scale3D, PLocalPose);

				PxConvexMeshGeometry PConvexGeom;
				PConvexGeom.convexMesh = bUseNegX ? ConvexElem.ConvexMeshNegX : ConvexElem.ConvexMesh;
				PConvexGeom.scale.scale = U2PVector(Scale3DAbs * ConvexElem.GetTransform().GetScale3D().GetAbs());
				FTransform ConvexTransform = ConvexElem.GetTransform();
				if (ConvexTransform.GetScale3D().X < 0 || ConvexTransform.GetScale3D().Y < 0 || ConvexTransform.GetScale3D().Z < 0)
				{
					UE_LOG(LogPhysics, Warning, TEXT("AddConvexElemsToRigidActor: [%s] ConvexElem[%d] has negative scale. Not currently supported"), *GetPathNameSafe(BodySetup->GetOuter()), i);
				}
				if (ConvexTransform.IsValid())
				{
					PxTransform PElementTransform = U2PTransform(ConvexTransform * RelativeTM);
					PLocalPose.q *= PElementTransform.q;
					PLocalPose.p = PElementTransform.p;
					PLocalPose.p.x *= Scale3D.X;
					PLocalPose.p.y *= Scale3D.Y;
					PLocalPose.p.z *= Scale3D.Z;

					if (PConvexGeom.isValid())
					{
						PxVec3 PBoundsExtents = PConvexGeom.convexMesh->getLocalBounds().getExtents();

						ensure(PLocalPose.isValid());
						{
							const float ContactOffset = FMath::Min(MaxContactOffset, ContactOffsetFactor * PBoundsExtents.minElement());
							AttachShape_AssumesLocked(PConvexGeom, PLocalPose, ContactOffset);
						}
					}
					else
					{
						UE_LOG(LogPhysics, Warning, TEXT("AddConvexElemsToRigidActor: [%s] ConvexElem[%d] invalid"), *GetPathNameSafe(BodySetup->GetOuter()), i);
					}
				}
				else
				{
					UE_LOG(LogPhysics, Warning, TEXT("AddConvexElemsToRigidActor: [%s] ConvexElem[%d] has invalid transform"), *GetPathNameSafe(BodySetup->GetOuter()), i);
				}
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("AddConvexElemsToRigidActor: ConvexElem is missing ConvexMesh (%d: %s)"), i, *BodySetup->GetPathName());
			}
		}
	}

	void AddTriMeshToRigidActor_AssumesLocked() const
	{
		float ContactOffsetFactor, MaxContactOffset;
		GetContactOffsetParams(ContactOffsetFactor, MaxContactOffset);

		if (BodySetup->TriMesh || BodySetup->TriMeshNegX)
		{
			PxTransform PLocalPose;
			bool bUseNegX = CalcMeshNegScaleCompensation(Scale3D, PLocalPose);

			// Only case where TriMeshNegX should be null is BSP, which should not require negX version
			if (bUseNegX && BodySetup->TriMeshNegX == NULL)
			{
				UE_LOG(LogPhysics, Log, TEXT("AddTriMeshToRigidActor: Want to use NegX but it doesn't exist! %s"), *BodySetup->GetPathName());
			}

			PxTriangleMesh* UseTriMesh = bUseNegX ? BodySetup->TriMeshNegX : BodySetup->TriMesh;
			if (UseTriMesh != NULL)
			{
				PxTriangleMeshGeometry PTriMeshGeom;
				PTriMeshGeom.triangleMesh = bUseNegX ? BodySetup->TriMeshNegX : BodySetup->TriMesh;
				PTriMeshGeom.scale.scale = U2PVector(Scale3DAbs);
				if (BodySetup->bDoubleSidedGeometry)
				{
					PTriMeshGeom.meshFlags |= PxMeshGeometryFlag::eDOUBLE_SIDED;
				}


				if (PTriMeshGeom.isValid())
				{
					ensure(PLocalPose.isValid());

					// Create without 'sim shape' flag, problematic if it's kinematic, and it gets set later anyway.
					{
						if (!AttachShape_AssumesLocked(PTriMeshGeom, PLocalPose, MaxContactOffset, PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eVISUALIZATION))
						{
							UE_LOG(LogPhysics, Log, TEXT("Can't create new mesh shape in AddShapesToRigidActor"));
						}
					}
				}
				else
				{
					UE_LOG(LogPhysics, Log, TEXT("AddTriMeshToRigidActor: TriMesh invalid"));
				}
			}
		}
	}

private:

	PxShape* AttachShape_AssumesLocked(const PxGeometry& PGeom, const PxTransform& PLocalPose, const float ContactOffset, PxShapeFlags PShapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE) const
	{
		const PxMaterial* PMaterial = GetDefaultPhysMaterial(); 
		PxShape* PNewShape = bShapeSharing ? GPhysXSDK->createShape(PGeom, *PMaterial, /*isExclusive =*/ false, PShapeFlags) : PDestActor->createShape(PGeom, *PMaterial, PShapeFlags);

		if (PNewShape)
		{
			PNewShape->setLocalPose(PLocalPose);

			if (NewShapes)
			{
				NewShapes->Add(PNewShape);
			}

			PNewShape->setContactOffset(ContactOffset);

			const bool bSyncFlags = bShapeSharing || SceneType == PST_Sync;
			const FShapeFilterData& Filters = ShapeData.FilterData;
			const bool bComplexShape = PNewShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH;

			PNewShape->setQueryFilterData(bComplexShape ? Filters.QueryComplexFilter : Filters.QuerySimpleFilter);
			PNewShape->setFlags( (bSyncFlags ? ShapeData.SyncShapeFlags : ShapeData.AsyncShapeFlags) | (bComplexShape ? ShapeData.ComplexShapeFlags : ShapeData.SimpleShapeFlags));
			PNewShape->setSimulationFilterData(Filters.SimFilter);
			FBodyInstance::ApplyMaterialToShape_AssumesLocked(PNewShape, SimpleMaterial, ComplexMaterials, bShapeSharing);

			if(bShapeSharing)
			{
				PDestActor->attachShape(*PNewShape);
				PNewShape->release();
			}
		}

		return PNewShape;
	}


};



void UBodySetup::AddShapesToRigidActor_AssumesLocked(FBodyInstance* OwningInstance, physx::PxRigidActor* PDestActor, EPhysicsSceneType SceneType, FVector& Scale3D, physx::PxMaterial* SimpleMaterial, TArray<UPhysicalMaterial*>& ComplexMaterials, FShapeData& ShapeData, const FTransform& RelativeTM, TArray<physx::PxShape*>* NewShapes, bool bShapeSharing)
{
#if WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR
	// in editor, there are a lot of things relying on body setup to create physics meshes
	CreatePhysicsMeshes();
#endif

	FAddShapesHelper AddShapesHelper(this, OwningInstance, PDestActor, SceneType, Scale3D, SimpleMaterial, ComplexMaterials, ShapeData, RelativeTM, NewShapes, bShapeSharing);

	// Create shapes for simple collision if we do not want to use the complex collision mesh 
	// for simple queries as well
	if (CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple)
	{
		AddShapesHelper.AddSpheresToRigidActor_AssumesLocked();
		AddShapesHelper.AddBoxesToRigidActor_AssumesLocked();
		AddShapesHelper.AddSphylsToRigidActor_AssumesLocked();
		AddShapesHelper.AddConvexElemsToRigidActor_AssumesLocked();
	}

	// Create tri-mesh shape, when we are not using simple collision shapes for 
	// complex queries as well
	if (CollisionTraceFlag != ECollisionTraceFlag::CTF_UseSimpleAsComplex)
	{
		AddShapesHelper.AddTriMeshToRigidActor_AssumesLocked();
	}

	if (OwningInstance)
	{
		if (PxRigidBody* RigidBody = OwningInstance->GetPxRigidBody_AssumesLocked())
		{
			RigidBody->setRigidBodyFlags(ShapeData.SyncBodyFlags);
		}
	}
}

#endif // WITH_PHYSX


#if WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR

void UBodySetup::RemoveSimpleCollision()
{
	AggGeom.EmptyElements();
	InvalidatePhysicsData();
}

void UBodySetup::RescaleSimpleCollision( FVector BuildScale )
{
	if( BuildScale3D != BuildScale )
	{					
		// Back out the old scale when applying the new scale
		const FVector ScaleMultiplier3D = (BuildScale / BuildScale3D);

		for (int32 i = 0; i < AggGeom.ConvexElems.Num(); i++)
		{
			FKConvexElem* ConvexElem = &(AggGeom.ConvexElems[i]);

			FTransform ConvexTrans = ConvexElem->GetTransform();
			FVector ConvexLoc = ConvexTrans.GetLocation();
			ConvexLoc *= ScaleMultiplier3D;
			ConvexTrans.SetLocation(ConvexLoc);
			ConvexElem->SetTransform(ConvexTrans);

			TArray<FVector>& Vertices = ConvexElem->VertexData;
			for (int32 VertIndex = 0; VertIndex < Vertices.Num(); ++VertIndex)
			{
				Vertices[VertIndex] *= ScaleMultiplier3D;
			}

			ConvexElem->UpdateElemBox();
		}

		// @todo Deal with non-vector properties by just applying the max value for the time being
		const float ScaleMultiplier = ScaleMultiplier3D.GetMax();

		for (int32 i = 0; i < AggGeom.SphereElems.Num(); i++)
		{
			FKSphereElem* SphereElem = &(AggGeom.SphereElems[i]);

			SphereElem->Center *= ScaleMultiplier3D;
			SphereElem->Radius *= ScaleMultiplier;
		}

		for (int32 i = 0; i < AggGeom.BoxElems.Num(); i++)
		{
			FKBoxElem* BoxElem = &(AggGeom.BoxElems[i]);

			BoxElem->Center *= ScaleMultiplier3D;
			BoxElem->X *= ScaleMultiplier3D.X;
			BoxElem->Y *= ScaleMultiplier3D.Y;
			BoxElem->Z *= ScaleMultiplier3D.Z;
		}

		for (int32 i = 0; i < AggGeom.SphylElems.Num(); i++)
		{
			FKSphylElem* SphylElem = &(AggGeom.SphylElems[i]);

			SphylElem->Center *= ScaleMultiplier3D;
			SphylElem->Radius *= ScaleMultiplier;
			SphylElem->Length *= ScaleMultiplier;
		}

		BuildScale3D = BuildScale;
	}
}

void UBodySetup::InvalidatePhysicsData()
{
	ClearPhysicsMeshes();
	BodySetupGuid = FGuid::NewGuid(); // change the guid
	if (!bSharedCookedData)
	{
		CookedFormatData.FlushData();
	}
}
#endif // WITH_EDITOR


void UBodySetup::BeginDestroy()
{
	Super::BeginDestroy();

	AggGeom.FreeRenderInfo();
}	

void UBodySetup::FinishDestroy()
{
	ClearPhysicsMeshes();
	Super::FinishDestroy();
}




void UBodySetup::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);


	// Load GUID (or create one for older versions)
	Ar << BodySetupGuid;

	// If we loaded a ZERO Guid, fix that
	if(Ar.IsLoading() && !BodySetupGuid.IsValid())
	{
		MarkPackageDirty();
		UE_LOG(LogPhysics, Log, TEXT("FIX GUID FOR: %s"), *GetPathName());
		BodySetupGuid = FGuid::NewGuid();
	}

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogPhysics, Fatal, TEXT("This platform requires cooked packages, and physX data was not cooked into %s."), *GetFullName());
	}

	if (bCooked)
	{
		if (Ar.IsCooking())
		{
			// Make sure to reset bHasCookedCollision data to true before calling GetCookedData for cooking
			bHasCookedCollisionData = true;
			FName Format = Ar.CookingTarget()->GetPhysicsFormat(this);
			bHasCookedCollisionData = GetCookedData(Format) != NULL; // Get the data from the DDC or build it

			TArray<FName> ActualFormatsToSave;
			ActualFormatsToSave.Add(Format);

			Ar << bHasCookedCollisionData;
			CookedFormatData.Serialize(Ar, this, &ActualFormatsToSave, !bSharedCookedData);
		}
		else
		{
			if (Ar.UE4Ver() >= VER_UE4_STORE_HASCOOKEDDATA_FOR_BODYSETUP)
			{
				Ar << bHasCookedCollisionData;
			}
			CookedFormatData.Serialize(Ar, this);
		}
	}

	if ( Ar.IsLoading() )
	{
		AggGeom.Serialize( Ar );
	}
}

void UBodySetup::PostLoad()
{
	Super::PostLoad();

	// Our owner needs to be post-loaded before us else they may not have loaded
	// their data yet.
	UObject* Outer = GetOuter();
	if (Outer)
	{
		Outer->ConditionalPostLoad();
	}

	if ( GetLinkerUE4Version() < VER_UE4_BUILD_SCALE_VECTOR )
	{
		BuildScale3D = FVector( BuildScale_DEPRECATED );
	}

	DefaultInstance.FixupData(this);

	if ( GetLinkerUE4Version() < VER_UE4_REFACTOR_PHYSICS_BLENDING )
	{
		if ( bAlwaysFullAnimWeight_DEPRECATED )
		{
			PhysicsType = PhysType_Simulated;
		}
		else if ( DefaultInstance.bSimulatePhysics == false )
		{
			PhysicsType = PhysType_Kinematic;
		}
		else
		{
			PhysicsType = PhysType_Default;
		}
	}

	if ( GetLinkerUE4Version() < VER_UE4_BODYSETUP_COLLISION_CONVERSION )
	{
		if ( DefaultInstance.GetCollisionEnabled() == ECollisionEnabled::NoCollision )
		{
			CollisionReponse = EBodyCollisionResponse::BodyCollision_Disabled;
		}
	}

	// Compress to whatever formats the active target platforms want
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			GetCookedData(Platforms[Index]->GetPhysicsFormat(this));
		}
	}

	// make sure that we load the physX data while the linker's loader is still open
	CreatePhysicsMeshes();

	// fix up invalid transform to use identity
	// this can be here because BodySetup isn't blueprintable
	if ( GetLinkerUE4Version() < VER_UE4_FIXUP_BODYSETUP_INVALID_CONVEX_TRANSFORM )
	{
		for (int32 i=0; i<AggGeom.ConvexElems.Num(); ++i)
		{
			if ( AggGeom.ConvexElems[i].GetTransform().IsValid() == false )
			{
				AggGeom.ConvexElems[i].SetTransform(FTransform::Identity);
			}
		}
	}
}

void UBodySetup::UpdateTriMeshVertices(const TArray<FVector> & NewPositions)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateTriMeshVertices);
#if WITH_PHYSX
	if (TriMesh)
	{
		
		PxVec3 * PNewPositions = TriMesh->getVerticesForModification();
		for (int32 i = 0; i < NewPositions.Num(); ++i)
		{
			PNewPositions[i] = U2PVector(NewPositions[i]);
		}

		TriMesh->refitBVH();
	}
#endif
}

FByteBulkData* UBodySetup::GetCookedData(FName Format)
{
	if (IsTemplate())
	{
		return NULL;
	}

	IInterface_CollisionDataProvider* CDP = Cast<IInterface_CollisionDataProvider>(GetOuter());

	// If there is nothing to cook or if we are reading data from a cooked package for an asset with no collision, 
	// we want to return here
	if ((AggGeom.ConvexElems.Num() == 0 && CDP == NULL) || !bHasCookedCollisionData)
	{
		return NULL;
	}

	FFormatContainer* UseCookedData = CookedFormatDataOverride ? CookedFormatDataOverride : &CookedFormatData;

	bool bContainedData = UseCookedData->Contains(Format);
	FByteBulkData* Result = &UseCookedData->GetFormat(Format);
#if WITH_PHYSX
	if (!bContainedData)
	{
		if (FPlatformProperties::RequiresCookedData())
		{
			UE_LOG(LogPhysics, Error, TEXT("Attempt to build physics data for %s when we are unable to. This platform requires cooked packages."), *GetPathName());
		}

		if (AggGeom.ConvexElems.Num() == 0 && (CDP == NULL || CDP->ContainsPhysicsTriMeshData(bMeshCollideAll) == false))
		{
			return NULL;
		}

#if WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR
		TArray<uint8> OutData;
		FDerivedDataPhysXCooker* DerivedPhysXData = new FDerivedDataPhysXCooker(Format, this);
		if (DerivedPhysXData->CanBuild())
		{
		#if WITH_EDITOR
			GetDerivedDataCacheRef().GetSynchronous(DerivedPhysXData, OutData);
		#elif WITH_RUNTIME_PHYSICS_COOKING
			DerivedPhysXData->Build(OutData);
		#endif
			if (OutData.Num())
			{
				Result->Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(Result->Realloc(OutData.Num()), OutData.GetData(), OutData.Num());
				Result->Unlock();
			}
		}
		else
#endif
		{
			UE_LOG(LogPhysics, Warning, TEXT("Attempt to build physics data for %s when we are unable to."), *GetPathName());
		}
	}
#endif // WITH_PHYSX
	check(Result);
	return Result->GetBulkDataSize() > 0 ? Result : NULL; // we don't return empty bulk data...but we save it to avoid thrashing the DDC
}

void UBodySetup::PostInitProperties()
{
	Super::PostInitProperties();

	if(!IsTemplate())
	{
		BodySetupGuid = FGuid::NewGuid();
	}
}

#if WITH_EDITOR
void UBodySetup::PostEditUndo()
{
	Super::PostEditUndo();

	// If we have any convex elems, ensure they are recreated whenever anything is modified!
	if(AggGeom.ConvexElems.Num() > 0)
	{
		InvalidatePhysicsData();
		CreatePhysicsMeshes();
	}
}
#endif // WITH_EDITOR

SIZE_T UBodySetup::GetResourceSize( EResourceSizeMode::Type Mode )
{
	SIZE_T ResourceSize = 0;

#if WITH_PHYSX
	// Count PhysX trimesh mem usage
	if (TriMesh != NULL)
	{
		ResourceSize += GetPhysxObjectSize(TriMesh, NULL);
	}

	if (TriMeshNegX != NULL)
	{
		ResourceSize += GetPhysxObjectSize(TriMeshNegX, NULL);
	}

	// Count PhysX convex mem usage
	for(int ConvIdx=0; ConvIdx<AggGeom.ConvexElems.Num(); ConvIdx++)
	{
		FKConvexElem& ConvexElem = AggGeom.ConvexElems[ConvIdx];

		if(ConvexElem.ConvexMesh != NULL)
		{
			ResourceSize += GetPhysxObjectSize(ConvexElem.ConvexMesh, NULL);
		}

		if(ConvexElem.ConvexMeshNegX != NULL)
		{
			ResourceSize += GetPhysxObjectSize(ConvexElem.ConvexMeshNegX, NULL);
		}
	}

#endif // WITH_PHYSX

	if (CookedFormatData.Contains(FPlatformProperties::GetPhysicsFormat()))
	{
		const FByteBulkData& FmtData = CookedFormatData.GetFormat(FPlatformProperties::GetPhysicsFormat());
		ResourceSize += FmtData.GetElementSize() * FmtData.GetElementCount();
	}
	
	return ResourceSize;
}

void FKAggregateGeom::Serialize( const FArchive& Ar )
{
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_REFACTOR_PHYSICS_TRANSFORMS )
	{
		for ( auto SphereElemIt = SphereElems.CreateIterator(); SphereElemIt; ++SphereElemIt )
		{
			SphereElemIt->Serialize( Ar );
		}

		for ( auto BoxElemIt = BoxElems.CreateIterator(); BoxElemIt; ++BoxElemIt )
		{
			BoxElemIt->Serialize( Ar );
		}

		for ( auto SphylElemIt = SphylElems.CreateIterator(); SphylElemIt; ++SphylElemIt )
		{
			SphylElemIt->Serialize( Ar );
		}
	}
}

float FKAggregateGeom::GetVolume(const FVector& Scale) const
{
	float Volume = 0.0f;

	for ( auto SphereElemIt = SphereElems.CreateConstIterator(); SphereElemIt; ++SphereElemIt )
	{
		Volume += SphereElemIt->GetVolume(Scale);
	}

	for ( auto BoxElemIt = BoxElems.CreateConstIterator(); BoxElemIt; ++BoxElemIt )
	{
		Volume += BoxElemIt->GetVolume(Scale);
	}

	for ( auto SphylElemIt = SphylElems.CreateConstIterator(); SphylElemIt; ++SphylElemIt )
	{
		Volume += SphylElemIt->GetVolume(Scale);
	}

	for ( auto ConvexElemIt = ConvexElems.CreateConstIterator(); ConvexElemIt; ++ConvexElemIt )
	{
		Volume += ConvexElemIt->GetVolume(Scale);
	}

	return Volume;
}

int32 FKAggregateGeom::GetElementCount(int32 Type) const
{
	switch (Type)
	{
	case KPT_Box:
		return BoxElems.Num();
	case KPT_Convex:
		return ConvexElems.Num();
	case KPT_Sphyl:
		return SphylElems.Num();
	case KPT_Sphere:
		return SphereElems.Num();
	default:
		return 0;
	}
}

void FKConvexElem::ScaleElem(FVector DeltaSize, float MinSize)
{
	FTransform ScaledTransform = GetTransform();
	ScaledTransform.SetScale3D(ScaledTransform.GetScale3D() + DeltaSize);
	SetTransform(ScaledTransform);
}

// References: 
// http://amp.ece.cmu.edu/Publication/Cha/icip01_Cha.pdf
// http://stackoverflow.com/questions/1406029/how-to-calculate-the-volume-of-a-3d-mesh-object-the-surface-of-which-is-made-up
float SignedVolumeOfTriangle(const FVector& p1, const FVector& p2, const FVector& p3) 
{
	return FVector::DotProduct(p1, FVector::CrossProduct(p2, p3)) / 6.0f;
}

float FKConvexElem::GetVolume(const FVector& Scale) const
{
	float Volume = 0.0f;

#if WITH_PHYSX
	if (ConvexMesh != NULL)
	{
		// Preparation for convex mesh scaling implemented in another changelist
		FTransform ScaleTransform = FTransform(FQuat::Identity, FVector::ZeroVector, Scale);

		int32 NumPolys = ConvexMesh->getNbPolygons();
		PxHullPolygon PolyData;

		const PxVec3* Vertices = ConvexMesh->getVertices();
		const PxU8* Indices = ConvexMesh->getIndexBuffer();

		for (int32 PolyIdx = 0; PolyIdx < NumPolys; ++PolyIdx)
		{
			if (ConvexMesh->getPolygonData(PolyIdx, PolyData))
			{
				for (int32 VertIdx = 2; VertIdx < PolyData.mNbVerts; ++ VertIdx)
				{
					// Grab triangle indices that we hit
					int32 I0 = Indices[PolyData.mIndexBase + 0];
					int32 I1 = Indices[PolyData.mIndexBase + (VertIdx - 1)];
					int32 I2 = Indices[PolyData.mIndexBase + VertIdx];


					Volume += SignedVolumeOfTriangle(ScaleTransform.TransformPosition(P2UVector(Vertices[I0])), 
						ScaleTransform.TransformPosition(P2UVector(Vertices[I1])), 
						ScaleTransform.TransformPosition(P2UVector(Vertices[I2])));
				}
			}
		}
	}
#endif // WITH_PHYSX

	return Volume;
}

void FKSphereElem::Serialize( const FArchive& Ar )
{
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_REFACTOR_PHYSICS_TRANSFORMS )
	{
		Center = TM_DEPRECATED.GetOrigin();
	}
}

void FKSphereElem::ScaleElem(FVector DeltaSize, float MinSize)
{
	// Find element with largest magnitude, btu preserve sign.
	float DeltaRadius = DeltaSize.X;
	if (FMath::Abs(DeltaSize.Y) > FMath::Abs(DeltaRadius))
		DeltaRadius = DeltaSize.Y;
	else if (FMath::Abs(DeltaSize.Z) > FMath::Abs(DeltaRadius))
		DeltaRadius = DeltaSize.Z;

	Radius = FMath::Max(Radius + DeltaRadius, MinSize);
}

void FKBoxElem::Serialize( const FArchive& Ar )
{
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_REFACTOR_PHYSICS_TRANSFORMS )
	{
		Center = TM_DEPRECATED.GetOrigin();
		Orientation = TM_DEPRECATED.ToQuat();
	}
}

void FKBoxElem::ScaleElem(FVector DeltaSize, float MinSize)
{
	// Sizes are lengths, so we double the delta to get similar increase in size.
	X = FMath::Max(X + 2 * DeltaSize.X, MinSize);
	Y = FMath::Max(Y + 2 * DeltaSize.Y, MinSize);
	Z = FMath::Max(Z + 2 * DeltaSize.Z, MinSize);
}

void FKSphylElem::Serialize( const FArchive& Ar )
{
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_REFACTOR_PHYSICS_TRANSFORMS )
	{
		Center = TM_DEPRECATED.GetOrigin();
		Orientation = TM_DEPRECATED.ToQuat();
	}
}

void FKSphylElem::ScaleElem(FVector DeltaSize, float MinSize)
{
	float DeltaRadius = DeltaSize.X;
	if (FMath::Abs(DeltaSize.Y) > FMath::Abs(DeltaRadius))
	{
		DeltaRadius = DeltaSize.Y;
	}

	float DeltaHeight = DeltaSize.Z;
	float radius = FMath::Max(Radius + DeltaRadius, MinSize);
	float length = Length + DeltaHeight;

	length += Radius - radius;
	length = FMath::Max(0.f, length);

	Radius = radius;
	Length = length;
}

class UPhysicalMaterial* UBodySetup::GetPhysMaterial() const
{
	UPhysicalMaterial* PhysMat = PhysMaterial;

	if (PhysMat == NULL && GEngine != NULL)
	{
		PhysMat = GEngine->DefaultPhysMaterial;
	}
	return PhysMat;
}

float UBodySetup::CalculateMass(const UPrimitiveComponent* Component) const
{
	FVector ComponentScale(1.0f, 1.0f, 1.0f);
	const FBodyInstance* BodyInstance = NULL;
	float MassScale = DefaultInstance.MassScale;

	const UPrimitiveComponent* OuterComp = Component != NULL ? Component : Cast<UPrimitiveComponent>(GetOuter());
	if (OuterComp)
	{
		ComponentScale = OuterComp->GetComponentScale();

		BodyInstance = &OuterComp->BodyInstance;

		const USkinnedMeshComponent* SkinnedMeshComp = Cast<const USkinnedMeshComponent>(OuterComp);
		if (SkinnedMeshComp != NULL)
		{
			const FBodyInstance* Body = SkinnedMeshComp->GetBodyInstance(BoneName);

			if (Body != NULL)
			{
				BodyInstance = Body;
			}
		}
	}
	else
	{
		BodyInstance = &DefaultInstance;
	}

	UPhysicalMaterial* PhysMat = BodyInstance->GetSimplePhysicalMaterial();
	MassScale = BodyInstance->MassScale;

	// physical material - nothing can weigh less than hydrogen (0.09 kg/m^3)
	float DensityKGPerCubicUU = 1.0f;
	float RaiseMassToPower = 0.75f;
	if (PhysMat)
	{
		DensityKGPerCubicUU = FMath::Max(0.00009f, PhysMat->Density * 0.001f);
		RaiseMassToPower = PhysMat->RaiseMassToPower;
	}

	// Then scale mass to avoid big differences between big and small objects.
	const float BasicVolume = GetVolume(ComponentScale);
	//@TODO: Some static meshes are triggering this - disabling until content can be analyzed - ensureMsgf(BasicVolume >= 0.0f, TEXT("UBodySetup::CalculateMass(%s) - The volume of the aggregate geometry is negative"), *Component->GetReadableName());

	const float BasicMass = FMath::Max<float>(BasicVolume, 0.0f) * DensityKGPerCubicUU;

	const float UsePow = FMath::Clamp<float>(RaiseMassToPower, KINDA_SMALL_NUMBER, 1.f);
	const float RealMass = FMath::Pow(BasicMass, UsePow);

	return RealMass * MassScale;
}

float UBodySetup::GetVolume(const FVector& Scale) const
{
	return AggGeom.GetVolume(Scale);
}
