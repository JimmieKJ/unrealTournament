// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BodySetup.cpp
=============================================================================*/ 

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "TargetPlatform.h"
#include "Animation/AnimStats.h"

#if WITH_PHYSX
	#include "PhysXSupport.h"
#endif // WITH_PHYSX


#include "PhysDerivedData.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "CookStats.h"

#if ENABLE_COOK_STATS
namespace PhysXBodySetupCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("PhysX.Usage"), TEXT("BodySetup"));
	});
}
#endif

#if WITH_PHYSX
	// Quaternion that converts Sphyls from UE space to PhysX space (negate Y, swap X & Z)
	// This is equivalent to a 180 degree rotation around the normalized (1, 0, 1) axis
	const physx::PxQuat U2PSphylBasis( PI, PxVec3( 1.0f / FMath::Sqrt( 2.0f ), 0.0f, 1.0f / FMath::Sqrt( 2.0f ) ) );
#endif // WITH_PHYSX

// CVars
static TAutoConsoleVariable<float> CVarContactOffsetFactor(
	TEXT("p.ContactOffsetFactor"),
	-1.f,
	TEXT("Multiplied by min dimension of object to calculate how close objects get before generating contacts. < 0 implies use project settings. Default: 0.01"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarMaxContactOffset(
	TEXT("p.MaxContactOffset"),
	-1.f,
	TEXT("Max value of contact offset, which controls how close objects get before generating contacts. < 0 implies use project settings. Default: 1.0"),
	ECVF_Default);


SIZE_T FBodySetupUVInfo::GetResourceSize()
{
	SIZE_T Size = 0;
	Size += IndexBuffer.GetAllocatedSize();
	Size += VertPositions.GetAllocatedSize();

	for (int32 ChannelIdx = 0; ChannelIdx < VertUVs.Num(); ChannelIdx++)
	{
		Size += VertUVs[ChannelIdx].GetAllocatedSize();
	}

	Size += VertUVs.GetAllocatedSize();
	return Size;
}


DEFINE_LOG_CATEGORY(LogPhysics);
UBodySetup::UBodySetup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bConsiderForBounds = true;
	bMeshCollideAll = false;
	CollisionTraceFlag = CTF_UseDefault;
	bHasCookedCollisionData = true;
	bNeverNeedsCookedCollisionData = false;
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
	CollisionTraceFlag = FromSetup->CollisionTraceFlag;
}

void UBodySetup::AddCollisionFrom(const FKAggregateGeom& FromAggGeom)
{
	// Add shapes from static mesh
	AggGeom.SphereElems.Append(FromAggGeom.SphereElems);
	AggGeom.BoxElems.Append(FromAggGeom.BoxElems);
	AggGeom.SphylElems.Append(FromAggGeom.SphylElems);

	// Remember how many convex we already have
	int32 FirstNewConvexIdx = AggGeom.ConvexElems.Num();
	// copy convex
	AggGeom.ConvexElems.Append(FromAggGeom.ConvexElems);
	// clear pointers on convex elements
	for (int32 i = FirstNewConvexIdx; i < AggGeom.ConvexElems.Num(); i++)
	{
		FKConvexElem& ConvexElem = AggGeom.ConvexElems[i];
		ConvexElem.ConvexMesh = NULL;
		ConvexElem.ConvexMeshNegX = NULL;
	}
}

void UBodySetup::AddCollisionFrom(class UBodySetup* FromSetup)
{
	AddCollisionFrom(FromSetup->AggGeom);
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

	// If we don't have any convex/trimesh data we can skip this whole function
	if (bNeverNeedsCookedCollisionData)
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

		FPhysXFormatDataReader CookedDataReader(*FormatData, &UVInfo);

		if (GetCollisionTraceFlag() != CTF_UseComplexAsSimple)
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

		if (GetCollisionTraceFlag() != CTF_UseComplexAsSimple)
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

		for(PxTriangleMesh* TriMesh : CookedDataReader.TriMeshes)
		{
			check(TriMesh);
			TriMeshes.Add(TriMesh);
			FPhysxSharedData::Get().Add(TriMesh);
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

	for(int32 ElementIndex = 0; ElementIndex < TriMeshes.Num(); ++ElementIndex)
	{
		GPhysXPendingKillTriMesh.Add(TriMeshes[ElementIndex]);
		FPhysxSharedData::Get().Remove(TriMeshes[ElementIndex]);
		TriMeshes[ElementIndex] = NULL;
	}
	TriMeshes.Empty();

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

void SetupNonUniformHelper(FVector Scale3D, float& MinScale, float& MinScaleAbs, FVector& Scale3DAbs)
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

void GetContactOffsetParams(float& ContactOffsetFactor, float& MinContactOffset, float& MaxContactOffset)
{
	// Get contact offset params
	ContactOffsetFactor = CVarContactOffsetFactor.GetValueOnGameThread();
	MaxContactOffset = CVarMaxContactOffset.GetValueOnGameThread();

	ContactOffsetFactor = ContactOffsetFactor < 0.f ? UPhysicsSettings::Get()->ContactOffsetMultiplier : ContactOffsetFactor;
	MaxContactOffset = MaxContactOffset < 0.f ? UPhysicsSettings::Get()->MaxContactOffset : MaxContactOffset;

	MinContactOffset = UPhysicsSettings::Get()->MinContactOffset;
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
		SetupNonUniformHelper(Scale3D, MinScale, MinScaleAbs, ShapeScale3DAbs);
		{
			float MinScaleRelative;
			float MinScaleAbsRelative;
			FVector Scale3DAbsRelative;
			FVector Scale3DRelative = RelativeTM.GetScale3D();

			SetupNonUniformHelper(Scale3DRelative, MinScaleRelative, MinScaleAbsRelative, Scale3DAbsRelative);

			MinScaleAbs *= MinScaleAbsRelative;
			ShapeScale3DAbs.X *= Scale3DAbsRelative.X;
			ShapeScale3DAbs.Y *= Scale3DAbsRelative.Y;
			ShapeScale3DAbs.Z *= Scale3DAbsRelative.Z;

			ShapeScale3D = Scale3D;
			ShapeScale3D.X *= Scale3DAbsRelative.X;
			ShapeScale3D.Y *= Scale3DAbsRelative.Y;
			ShapeScale3D.Z *= Scale3DAbsRelative.Z;
		}

		GetContactOffsetParams(ContactOffsetFactor, MinContactOffset, MaxContactOffset);
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
	FVector ShapeScale3DAbs;
	FVector ShapeScale3D;

	float ContactOffsetFactor;
	float MinContactOffset;
	float MaxContactOffset;

public:
	void AddSpheresToRigidActor_AssumesLocked() const
	{
		for (int32 i = 0; i < BodySetup->AggGeom.SphereElems.Num(); i++)
		{
			const FKSphereElem& SphereElem = BodySetup->AggGeom.SphereElems[i];
			const FKSphereElem ScaledSphereElem = SphereElem.GetFinalScaled(Scale3D, RelativeTM);

			PxSphereGeometry PSphereGeom;
			PSphereGeom.radius = ScaledSphereElem.Radius;

			if (ensure(PSphereGeom.isValid()))
			{
				PxTransform PLocalPose(U2PVector(ScaledSphereElem.Center));
				ensure(PLocalPose.isValid());
				{
					const float ContactOffset = FMath::Clamp(ContactOffsetFactor * PSphereGeom.radius, MinContactOffset, MaxContactOffset);
					PxShape* PShape = AttachShape_AssumesLocked(PSphereGeom, PLocalPose, ContactOffset, SphereElem.GetUserData());
				}
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("AddSpheresToRigidActor: [%s] ScaledSphereElem[%d] invalid"), *GetPathNameSafe(BodySetup->GetOuter()), i);
			}
		}
	}

	void AddBoxesToRigidActor_AssumesLocked() const
	{
		for (int32 i = 0; i < BodySetup->AggGeom.BoxElems.Num(); i++)
		{
			const FKBoxElem& BoxElem = BodySetup->AggGeom.BoxElems[i];
			const FKBoxElem ScaledBoxElem = BoxElem.GetFinalScaled(Scale3D, RelativeTM);
			const FTransform& BoxTransform = ScaledBoxElem.GetTransform();
			
			PxBoxGeometry PBoxGeom;
			PBoxGeom.halfExtents.x = ScaledBoxElem.X * 0.5f;
			PBoxGeom.halfExtents.y = ScaledBoxElem.Y * 0.5f;
			PBoxGeom.halfExtents.z = ScaledBoxElem.Z * 0.5f;

			if (PBoxGeom.isValid() && BoxTransform.IsValid())
			{
				PxTransform PLocalPose(U2PTransform(BoxTransform));
				ensure(PLocalPose.isValid());
				{
					const float ContactOffset = FMath::Clamp(ContactOffsetFactor * PBoxGeom.halfExtents.minElement(), MinContactOffset, MaxContactOffset);
					AttachShape_AssumesLocked(PBoxGeom, PLocalPose, ContactOffset, BoxElem.GetUserData());
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
		float ScaleRadius = FMath::Max(ShapeScale3DAbs.X, ShapeScale3DAbs.Y);
		float ScaleLength = ShapeScale3DAbs.Z;

		for (int32 i = 0; i < BodySetup->AggGeom.SphylElems.Num(); i++)
		{
			const FKSphylElem& SphylElem = BodySetup->AggGeom.SphylElems[i];
			const FKSphylElem ScaledSphylElem = SphylElem.GetFinalScaled(Scale3D, RelativeTM);

			PxCapsuleGeometry PCapsuleGeom;
			PCapsuleGeom.halfHeight = ScaledSphylElem.Length * 0.5f;
			PCapsuleGeom.radius = ScaledSphylElem.Radius;

			if (PCapsuleGeom.isValid())
			{
				// The stored sphyl transform assumes the sphyl axis is down Z. In PhysX, it points down X, so we twiddle the matrix a bit here (swap X and Z and negate Y).
				PxTransform PLocalPose(U2PVector(ScaledSphylElem.Center), U2PQuat(ScaledSphylElem.Orientation) * U2PSphylBasis);

				ensure(PLocalPose.isValid());
				{
					const float ContactOffset = FMath::Clamp(ContactOffsetFactor * PCapsuleGeom.radius, MinContactOffset, MaxContactOffset);
					AttachShape_AssumesLocked(PCapsuleGeom, PLocalPose, ContactOffset, SphylElem.GetUserData());
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
		for (int32 i = 0; i < BodySetup->AggGeom.ConvexElems.Num(); i++)
		{
			const FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems[i];

			PxTransform PLocalPose;
			bool bUseNegX = CalcMeshNegScaleCompensation(Scale3D, PLocalPose);

			PxConvexMesh* UseConvexMesh = bUseNegX ? ConvexElem.ConvexMeshNegX : ConvexElem.ConvexMesh;
			if (UseConvexMesh)
			{
				PxConvexMeshGeometry PConvexGeom;
				PConvexGeom.convexMesh = UseConvexMesh;
				PConvexGeom.scale.scale = U2PVector(ShapeScale3DAbs * ConvexElem.GetTransform().GetScale3D().GetAbs());	//scale shape about the origin
				FTransform ConvexTransform = ConvexElem.GetTransform();
				if (ConvexTransform.GetScale3D().X < 0 || ConvexTransform.GetScale3D().Y < 0 || ConvexTransform.GetScale3D().Z < 0)
				{
					UE_LOG(LogPhysics, Warning, TEXT("AddConvexElemsToRigidActor: [%s] ConvexElem[%d] has negative scale. Not currently supported"), *GetPathNameSafe(BodySetup->GetOuter()), i);
				}
				if (ConvexTransform.IsValid())
				{
					//Scale the position independent of shape scale. This is because physx transforms have no concept of scale
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
							const float ContactOffset = FMath::Clamp(ContactOffsetFactor * PBoundsExtents.minElement(), MinContactOffset, MaxContactOffset);
							AttachShape_AssumesLocked(PConvexGeom, PLocalPose, ContactOffset, ConvexElem.GetUserData());
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
		for(PxTriangleMesh* TriMesh : BodySetup->TriMeshes)
		{
		
			PxTriangleMeshGeometry PTriMeshGeom;
			PTriMeshGeom.triangleMesh = TriMesh;
			PTriMeshGeom.scale.scale = U2PVector(ShapeScale3D); //scale shape about the origin
			if (BodySetup->bDoubleSidedGeometry)
			{
				PTriMeshGeom.meshFlags |= PxMeshGeometryFlag::eDOUBLE_SIDED;
			}

			if (PTriMeshGeom.isValid())
			{
				//Scale the position independent of shape scale. This is because physx transforms have no concept of scale
				PxTransform PElementTransform = U2PTransform(RelativeTM);
				PElementTransform.p.x *= Scale3D.X;
				PElementTransform.p.y *= Scale3D.Y;
				PElementTransform.p.z *= Scale3D.Z;

				// Create without 'sim shape' flag, problematic if it's kinematic, and it gets set later anyway.
				if (!AttachShape_AssumesLocked(PTriMeshGeom, PElementTransform, MaxContactOffset, nullptr, PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eVISUALIZATION))
				{
					UE_LOG(LogPhysics, Log, TEXT("Can't create new mesh shape in AddShapesToRigidActor"));
				}
			}
			else
			{
				UE_LOG(LogPhysics, Log, TEXT("AddTriMeshToRigidActor: TriMesh invalid"));
			}
		}
	}

private:

	PxShape* AttachShape_AssumesLocked(const PxGeometry& PGeom, const PxTransform& PLocalPose, const float ContactOffset, const FPhysxUserData* ShapeElemUserData, PxShapeFlags PShapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE) const
	{
		const PxMaterial* PMaterial = GetDefaultPhysMaterial(); 
		PxShape* PNewShape = bShapeSharing ? GPhysXSDK->createShape(PGeom, *PMaterial, /*isExclusive =*/ false, PShapeFlags) : PDestActor->createShape(PGeom, *PMaterial, PShapeFlags);

		if (PNewShape)
		{
			PNewShape->userData = (void*) ShapeElemUserData;
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

	// if almost zero, set min scale
	// @todo fixme
	if (Scale3D.IsNearlyZero())
	{
		// set min scale
		Scale3D = FVector(0.1f);
	}

	FAddShapesHelper AddShapesHelper(this, OwningInstance, PDestActor, SceneType, Scale3D, SimpleMaterial, ComplexMaterials, ShapeData, RelativeTM, NewShapes, bShapeSharing);

	// Create shapes for simple collision if we do not want to use the complex collision mesh 
	// for simple queries as well
	if (GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple)
	{
		AddShapesHelper.AddSpheresToRigidActor_AssumesLocked();
		AddShapesHelper.AddBoxesToRigidActor_AssumesLocked();
		AddShapesHelper.AddSphylsToRigidActor_AssumesLocked();
		AddShapesHelper.AddConvexElemsToRigidActor_AssumesLocked();
	}

	// Create tri-mesh shape, when we are not using simple collision shapes for 
	// complex queries as well
	if (GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseSimpleAsComplex)
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

#if !WITH_RUNTIME_PHYSICS_COOKING
	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogPhysics, Fatal, TEXT("This platform requires cooked packages, and physX data was not cooked into %s."), *GetFullName());
	}
#endif //!WITH_RUNTIME_PHYSICS_COOKING

	if (bCooked)
	{
#if WITH_EDITOR
		if (Ar.IsCooking())
		{
			// Make sure to reset bHasCookedCollision data to true before calling GetCookedData for cooking
			bHasCookedCollisionData = true;
			FName Format = Ar.CookingTarget()->GetPhysicsFormat(this);
			bool bUseRuntimeOnlyCookedData = !bSharedCookedData;	//For shared cook data we do not optimize for runtime only flags. This is only used by per poly skeletal mesh component at the moment. Might want to add support in future
			bHasCookedCollisionData = GetCookedData(Format, bUseRuntimeOnlyCookedData) != NULL; // Get the data from the DDC or build it

			TArray<FName> ActualFormatsToSave;
			ActualFormatsToSave.Add(Format);

			Ar << bHasCookedCollisionData;

			FFormatContainer* UseCookedFormatData = bUseRuntimeOnlyCookedData ? &CookedFormatDataRuntimeOnlyOptimization : &CookedFormatData;
			UseCookedFormatData->Serialize(Ar, this, &ActualFormatsToSave, !bSharedCookedData);
		}
		else
#endif
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
	if (TriMeshes.Num())
	{
		check(TriMeshes[0] != nullptr);
		PxU32 PNumVerts = TriMeshes[0]->getNbVertices(); // Get num of verts we expect
		PxVec3 * PNewPositions = TriMeshes[0]->getVerticesForModification();	//we only update the first trimesh. We assume this per poly case is not updating welded trimeshes

		int32 NumToCopy = FMath::Min<int32>(PNumVerts, NewPositions.Num()); // Make sure we don't write off end of array provided
		for (int32 i = 0; i < NumToCopy; ++i)
		{
			PNewPositions[i] = U2PVector(NewPositions[i]);
		}

		TriMeshes[0]->refitBVH();
	}
#endif
}

template <bool bPositionAndNormal>
float GetClosestPointAndNormalImpl(const UBodySetup* BodySetup, const FVector& WorldPosition, const FTransform& LocalToWorld, FVector* ClosestWorldPosition, FVector* FeatureNormal)
{
	float ClosestDist = FLT_MAX;
	FVector TmpPosition, TmpNormal;

	//Note that this function is optimized for BodySetup with few elements. This is more common. If we want to optimize the case with many elements we should really return the element during the distance check to avoid pointless iteration
	for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
	{
		
		if(bPositionAndNormal)
		{
			const float Dist = SphereElem.GetClosestPointAndNormal(WorldPosition, LocalToWorld, TmpPosition, TmpNormal);

			if(Dist < ClosestDist)
			{
				*ClosestWorldPosition = TmpPosition;
				*FeatureNormal = TmpNormal;
				ClosestDist = Dist;
			}
		}
		else
		{
			const float Dist = SphereElem.GetShortestDistanceToPoint(WorldPosition, LocalToWorld);
			ClosestDist = Dist < ClosestDist ? Dist : ClosestDist;
		}
	}

	for (const FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
	{
		if (bPositionAndNormal)
		{
			const float Dist = SphylElem.GetClosestPointAndNormal(WorldPosition, LocalToWorld, TmpPosition, TmpNormal);

			if (Dist < ClosestDist)
			{
				*ClosestWorldPosition = TmpPosition;
				*FeatureNormal = TmpNormal;
				ClosestDist = Dist;
			}
		}
		else
		{
			const float Dist = SphylElem.GetShortestDistanceToPoint(WorldPosition, LocalToWorld);
			ClosestDist = Dist < ClosestDist ? Dist : ClosestDist;
		}
	}

	for (const FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
	{
		if (bPositionAndNormal)
		{
			const float Dist = BoxElem.GetClosestPointAndNormal(WorldPosition, LocalToWorld, TmpPosition, TmpNormal);

			if (Dist < ClosestDist)
			{
				*ClosestWorldPosition = TmpPosition;
				*FeatureNormal = TmpNormal;
				ClosestDist = Dist;
			}
		}
		else
		{
			const float Dist =  BoxElem.GetShortestDistanceToPoint(WorldPosition, LocalToWorld);
			ClosestDist = Dist < ClosestDist ? Dist : ClosestDist;
		}
	}

	if (ClosestDist == FLT_MAX)
	{
		UE_LOG(LogPhysics, Warning, TEXT("GetClosestPointAndNormalImpl ClosestDist for BodySetup %s is coming back as FLT_MAX. WorldPosition = %s, LocalToWorld = %s"), *BodySetup->GetFullName(), *WorldPosition.ToString(), *LocalToWorld.ToHumanReadableString());
	}

	return ClosestDist;
}

float UBodySetup::GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& LocalToWorld) const
{
	return GetClosestPointAndNormalImpl<false>(this, WorldPosition, LocalToWorld, nullptr, nullptr);
}

float UBodySetup::GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& LocalToWorld, FVector& ClosestWorldPosition, FVector& FeatureNormal) const
{
	return GetClosestPointAndNormalImpl<true>(this, WorldPosition, LocalToWorld, &ClosestWorldPosition, &FeatureNormal);
}

#if WITH_EDITOR
void UBodySetup::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	GetCookedData(TargetPlatform->GetPhysicsFormat(this), true);
}

void UBodySetup::ClearCachedCookedPlatformData( const ITargetPlatform* TargetPlatform )
{
	CookedFormatDataRuntimeOnlyOptimization.FlushData();
}
#endif

int32 UBodySetup::GetRuntimeOnlyCookOptimizationFlags() const
{
	int32 RuntimeCookFlags = 0;
#if WITH_PHYSX && (WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR)
	if(UPhysicsSettings::Get()->bSuppressFaceRemapTable)
	{
		RuntimeCookFlags |= ERuntimePhysxCookOptimizationFlags::SuppressFaceRemapTable;
	}
#endif

	return RuntimeCookFlags;
}

bool UBodySetup::CalcUVAtLocation(const FVector& BodySpaceLocation, int32 FaceIndex, int32 UVChannel, FVector2D& UV) const
{
	bool bSuccess = false;

	if (UVInfo.VertUVs.IsValidIndex(UVChannel) && UVInfo.IndexBuffer.IsValidIndex(FaceIndex * 3 + 2))
	{
		int32 Index0 = UVInfo.IndexBuffer[FaceIndex * 3 + 0];
		int32 Index1 = UVInfo.IndexBuffer[FaceIndex * 3 + 1];
		int32 Index2 = UVInfo.IndexBuffer[FaceIndex * 3 + 2];

		FVector Pos0 = UVInfo.VertPositions[Index0];
		FVector Pos1 = UVInfo.VertPositions[Index1];
		FVector Pos2 = UVInfo.VertPositions[Index2];

		FVector2D UV0 = UVInfo.VertUVs[UVChannel][Index0];
		FVector2D UV1 = UVInfo.VertUVs[UVChannel][Index1];
		FVector2D UV2 = UVInfo.VertUVs[UVChannel][Index2];

		// Transform hit location from world to local space.
		// Find barycentric coords
		FVector BaryCoords = FMath::ComputeBaryCentric2D(BodySpaceLocation, Pos0, Pos1, Pos2);
		// Use to blend UVs
		UV = (BaryCoords.X * UV0) + (BaryCoords.Y * UV1) + (BaryCoords.Z * UV2);

		bSuccess = true;
	}

	return bSuccess;
}


FByteBulkData* UBodySetup::GetCookedData(FName Format, bool bRuntimeOnlyOptimizedVersion)
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

#if WITH_EDITOR
	//We don't support runtime cook optimization for per poly skeletal mesh. This is an edge case we may want to support (only helps memory savings)
	FFormatContainer* UseCookedData = CookedFormatDataOverride ? CookedFormatDataOverride : (bRuntimeOnlyOptimizedVersion ? &CookedFormatDataRuntimeOnlyOptimization : &CookedFormatData);
#else
	FFormatContainer* UseCookedData = CookedFormatDataOverride ? CookedFormatDataOverride : &CookedFormatData;
#endif

	bool bContainedData = UseCookedData->Contains(Format);
	FByteBulkData* Result = &UseCookedData->GetFormat(Format);
#if WITH_PHYSX
	if (!bContainedData)
	{
#if !defined(WITH_RUNTIME_PHYSICS_COOKING) || !WITH_RUNTIME_PHYSICS_COOKING
		if (FPlatformProperties::RequiresCookedData())
		{
			UE_LOG(LogPhysics, Error, TEXT("Attempt to build physics data for %s when we are unable to. This platform requires cooked packages."), *GetPathName());
		}
#endif

		if (AggGeom.ConvexElems.Num() == 0 && (CDP == NULL || CDP->ContainsPhysicsTriMeshData(bMeshCollideAll) == false))
		{
			return NULL;
		}

#if WITH_EDITOR
		const bool bEligibleForRuntimeOptimization = UseCookedData == &CookedFormatDataRuntimeOnlyOptimization;
#else
		const bool bEligibleForRuntimeOptimization = CookedFormatDataOverride == nullptr;	//We don't support runtime cook optimization for per poly skeletal mesh. This is an edge case we may want to support (only helps memory savings)
#endif

#if WITH_RUNTIME_PHYSICS_COOKING || WITH_EDITOR
		TArray<uint8> OutData;

		const int32 CookingFlags = bEligibleForRuntimeOptimization ? GetRuntimeOnlyCookOptimizationFlags() : 0;
		FDerivedDataPhysXCooker* DerivedPhysXData = new FDerivedDataPhysXCooker(Format, CookingFlags, this);
		if (DerivedPhysXData->CanBuild())
		{
		#if WITH_EDITOR
			COOK_STAT(auto Timer = PhysXBodySetupCookStats::UsageStats.TimeSyncWork());
			bool bDataWasBuilt = false;
			bool DDCHit = GetDerivedDataCacheRef().GetSynchronous(DerivedPhysXData, OutData, &bDataWasBuilt);
		#elif WITH_RUNTIME_PHYSICS_COOKING
			DerivedPhysXData->Build(OutData);
		#endif
			COOK_STAT(Timer.AddHitOrMiss(!DDCHit || bDataWasBuilt ? FCookStats::CallStats::EHitOrMiss::Miss : FCookStats::CallStats::EHitOrMiss::Hit, OutData.Num()));
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

void UBodySetup::CopyBodySetupProperty(const UBodySetup* Other)
{
	BoneName = Other->BoneName;
	PhysicsType = Other->PhysicsType;
	bConsiderForBounds = Other->bConsiderForBounds;
	bMeshCollideAll = Other->bMeshCollideAll;
	bDoubleSidedGeometry = Other->bDoubleSidedGeometry;
	bGenerateNonMirroredCollision = Other->bGenerateNonMirroredCollision;
	bSharedCookedData = Other->bSharedCookedData;
	bGenerateMirroredCollision = Other->bGenerateMirroredCollision;
	PhysMaterial = Other->PhysMaterial;
	CollisionReponse = Other->CollisionReponse;
	CollisionTraceFlag = Other->CollisionTraceFlag;
	DefaultInstance = Other->DefaultInstance;
	WalkableSlopeOverride = Other->WalkableSlopeOverride;
	BuildScale3D = Other->BuildScale3D;
}

#endif // WITH_EDITOR

SIZE_T UBodySetup::GetResourceSize( EResourceSizeMode::Type Mode )
{
	SIZE_T ResourceSize = Super::GetResourceSize(Mode);

#if WITH_PHYSX
	// Count PhysX trimesh mem usage
	for(PxTriangleMesh* TriMesh : TriMeshes)
	{
		ResourceSize += GetPhysxObjectSize(TriMesh, NULL);
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
	
	// Count any UV info
	ResourceSize += UVInfo.GetResourceSize();

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

float FKSphereElem::GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& LocalToWorldTM) const
{
	FKSphereElem ScaledSphere = GetFinalScaled(LocalToWorldTM.GetScale3D(), FTransform::Identity);

	const FVector Dir = LocalToWorldTM.TransformPositionNoScale(ScaledSphere.Center) - WorldPosition;
	const float DistToCenter = Dir.Size();
	const float DistToEdge = DistToCenter - ScaledSphere.Radius;
	
	return DistToEdge > SMALL_NUMBER ? DistToEdge : 0.f;
}

float FKSphereElem::GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& LocalToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const
{
	FKSphereElem ScaledSphere = GetFinalScaled(LocalToWorldTM.GetScale3D(), FTransform::Identity);

	const FVector Dir = LocalToWorldTM.TransformPositionNoScale(ScaledSphere.Center) - WorldPosition;
	const float DistToCenter = Dir.Size();
	const float DistToEdge = FMath::Max(DistToCenter - ScaledSphere.Radius, 0.f);

	if(DistToCenter > SMALL_NUMBER)
	{
		Normal = -Dir.GetUnsafeNormal();
	}
	else
	{
		Normal = FVector::ZeroVector;
	}
	
	ClosestWorldPosition = WorldPosition - Normal*DistToEdge;

	return DistToEdge;
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

FKSphereElem FKSphereElem::GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
{
	float MinScale, MinScaleAbs;
	FVector Scale3DAbs;

	SetupNonUniformHelper(Scale3D * RelativeTM.GetScale3D(), MinScale, MinScaleAbs, Scale3DAbs);

	FKSphereElem ScaledSphere = *this;
	ScaledSphere.Radius *= MinScaleAbs;

	ScaledSphere.Center = RelativeTM.TransformPosition(Center) * Scale3D;


	return ScaledSphere;
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


FKBoxElem FKBoxElem::GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
{
	float MinScale, MinScaleAbs;
	FVector Scale3DAbs;

	SetupNonUniformHelper(Scale3D * RelativeTM.GetScale3D(), MinScale, MinScaleAbs, Scale3DAbs);

	FKBoxElem ScaledBox = *this;
	ScaledBox.X *= Scale3DAbs.X;
	ScaledBox.Y *= Scale3DAbs.Y;
	ScaledBox.Z *= Scale3DAbs.Z;

	FTransform BoxTransform = GetTransform() * RelativeTM;
	BoxTransform.ScaleTranslation(Scale3D);
	ScaledBox.SetTransform(BoxTransform);

	return ScaledBox;
}

float FKBoxElem::GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BoneToWorldTM) const
{
	const FKBoxElem& ScaledBox = GetFinalScaled(BoneToWorldTM.GetScale3D(), FTransform::Identity);
	const FTransform LocalToWorldTM = GetTransform() * BoneToWorldTM;
	const FVector LocalPosition = LocalToWorldTM.InverseTransformPositionNoScale(WorldPosition);
	const FVector LocalPositionAbs = LocalPosition.GetAbs();

	const FVector HalfPoint(ScaledBox.X*0.5f, ScaledBox.Y*0.5f, ScaledBox.Z*0.5f);
	const FVector Delta = LocalPositionAbs - HalfPoint;
	const FVector Errors = FVector(FMath::Max(Delta.X, 0.f), FMath::Max(Delta.Y, 0.f), FMath::Max(Delta.Z, 0.f));
	const float Error = Errors.Size();

	return Error > SMALL_NUMBER ? Error : 0.f;
}

float FKBoxElem::GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BoneToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const
{
	const FKBoxElem& ScaledBox = GetFinalScaled(BoneToWorldTM.GetScale3D(), FTransform::Identity);
	const FTransform LocalToWorldTM = GetTransform() * BoneToWorldTM;
	const FVector LocalPosition = LocalToWorldTM.InverseTransformPositionNoScale(WorldPosition);

	const float HalfX = ScaledBox.X * 0.5f;
	const float HalfY = ScaledBox.Y * 0.5f;
	const float HalfZ = ScaledBox.Z * 0.5f;
	
	const FVector ClosestLocalPosition(FMath::Clamp(LocalPosition.X, -HalfX, HalfX), FMath::Clamp(LocalPosition.Y, -HalfY, HalfY), FMath::Clamp(LocalPosition.Z, -HalfZ, HalfZ));
	ClosestWorldPosition = LocalToWorldTM.TransformPositionNoScale(ClosestLocalPosition);

	const FVector LocalDelta = LocalPosition - ClosestLocalPosition;
	float Error = LocalDelta.Size();
	
	bool bIsOutside = Error > SMALL_NUMBER;
	
	const FVector LocalNormal = bIsOutside ? LocalDelta.GetUnsafeNormal() : FVector::ZeroVector;

	ClosestWorldPosition = LocalToWorldTM.TransformPositionNoScale(ClosestLocalPosition);
	Normal = LocalToWorldTM.TransformVectorNoScale(LocalNormal);
	
	return bIsOutside ? Error : 0.f;
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

FKSphylElem FKSphylElem::GetFinalScaled(const FVector& Scale3D, const FTransform& RelativeTM) const
{
	FKSphylElem ScaledSphylElem = *this;

	float MinScale, MinScaleAbs;
	FVector Scale3DAbs;

	SetupNonUniformHelper(Scale3D * RelativeTM.GetScale3D(), MinScale, MinScaleAbs, Scale3DAbs);

	float ScaleRadius = FMath::Max(Scale3DAbs.X, Scale3DAbs.Y);
	float ScaleLength = Scale3DAbs.Z;
	
	// this is a bit confusing since radius and height is scaled
	// first apply the scale first 
	ScaledSphylElem.Radius = FMath::Max(Radius * ScaleRadius, 0.1f);
	ScaledSphylElem.Length = Length + Radius * 2.f;
	float HalfLength = FMath::Max(ScaledSphylElem.Length * ScaleLength * 0.5f, 0.1f);
	ScaledSphylElem.Radius = FMath::Clamp(ScaledSphylElem.Radius, 0.1f, HalfLength);	//radius is capped by half length
	ScaledSphylElem.Length = FMath::Max(0.1f, (HalfLength - ScaledSphylElem.Radius) * 2.f);

	FVector LocalOrigin = RelativeTM.TransformPosition(Center) * Scale3D;
	ScaledSphylElem.Center = LocalOrigin;
	
	return ScaledSphylElem;
}

float FKSphylElem::GetShortestDistanceToPoint(const FVector& WorldPosition, const FTransform& BoneToWorldTM) const
{
	const FKSphylElem ScaledSphyl = GetFinalScaled(BoneToWorldTM.GetScale3D(), FTransform::Identity);

	const FTransform LocalToWorldTM = GetTransform() * BoneToWorldTM;
	const FVector ErrorScale = LocalToWorldTM.GetScale3D();
	const FVector LocalPosition = LocalToWorldTM.InverseTransformPositionNoScale(WorldPosition);
	const FVector LocalPositionAbs = LocalPosition.GetAbs();
	
	
	const FVector Target(LocalPositionAbs.X, LocalPositionAbs.Y, FMath::Max(LocalPositionAbs.Z - ScaledSphyl.Length * 0.5f, 0.f));	//If we are above half length find closest point to cap, otherwise to cylinder
	const float Error = FMath::Max(Target.Size() - ScaledSphyl.Radius, 0.f);

	return Error > SMALL_NUMBER ? Error : 0.f;
}

float FKSphylElem::GetClosestPointAndNormal(const FVector& WorldPosition, const FTransform& BoneToWorldTM, FVector& ClosestWorldPosition, FVector& Normal) const
{
	const FKSphylElem ScaledSphyl = GetFinalScaled(BoneToWorldTM.GetScale3D(), FTransform::Identity);

	const FTransform LocalToWorldTM = GetTransform() * BoneToWorldTM;
	const FVector ErrorScale = LocalToWorldTM.GetScale3D();
	const FVector LocalPosition = LocalToWorldTM.InverseTransformPositionNoScale(WorldPosition);
	
	const float HalfLength = 0.5f * ScaledSphyl.Length;
	const float TargetZ = FMath::Clamp(LocalPosition.Z, -HalfLength, HalfLength);	//We want to move to a sphere somewhere along the capsule axis

	const FVector WorldSphere = LocalToWorldTM.TransformPositionNoScale(FVector(0.f, 0.f, TargetZ));
	const FVector Dir = WorldSphere - WorldPosition;
	const float DistToCenter = Dir.Size();
	const float DistToEdge = FMath::Max(DistToCenter - ScaledSphyl.Radius, 0.f);

	bool bIsOutside = DistToCenter > SMALL_NUMBER;
	if (bIsOutside)
	{
		Normal = -Dir.GetUnsafeNormal();
	}
	else
	{
		Normal = FVector::ZeroVector;
	}

	ClosestWorldPosition = WorldPosition - Normal*DistToEdge;

	return bIsOutside ? DistToEdge : 0.f;
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
	const FBodyInstance* BodyInstance = &DefaultInstance;
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

	if(BodyInstance->bOverrideMass)
	{
		return BodyInstance->GetMassOverride();
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

TEnumAsByte<enum ECollisionTraceFlag> UBodySetup::GetCollisionTraceFlag() const
{
	TEnumAsByte<enum ECollisionTraceFlag> DefaultFlag = UPhysicsSettings::Get()->DefaultShapeComplexity;
	return CollisionTraceFlag == ECollisionTraceFlag::CTF_UseDefault ? DefaultFlag : CollisionTraceFlag;
}