// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "Collision.h"

#include "MessageLog.h"

#if WITH_PHYSX
	#include "PhysXSupport.h"
	#include "../Collision/PhysXCollision.h"
	#include "../Collision/CollisionConversions.h"
#endif // WITH_PHYSX

#include "PhysDerivedData.h"
#include "PhysicsSerializer.h"

#define LOCTEXT_NAMESPACE "BodyInstance"

#if WITH_BOX2D
#include "../PhysicsEngine2D/Box2DIntegration.h"
#include "PhysicsEngine/BodySetup2D.h"
#include "PhysicsEngine/AggregateGeometry2D.h"
#endif	//WITH_BOX2D
#include "Components/ModelComponent.h"
#include "Components/BrushComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"

DEFINE_STAT(STAT_InitBody);
DEFINE_STAT(STAT_InitBodyDebug);
DEFINE_STAT(STAT_InitBodySceneInteraction);
DEFINE_STAT(STAT_InitBodyPostAdd);
DEFINE_STAT(STAT_UpdatePhysFilter);
DEFINE_STAT(STAT_UpdatePhysFilterPhysX);

DEFINE_STAT(STAT_InitBodies);
DEFINE_STAT(STAT_BulkSceneAdd);
DEFINE_STAT(STAT_StaticInitBodies);

////////////////////////////////////////////////////////////////////////////
// FCollisionResponse
////////////////////////////////////////////////////////////////////////////

FCollisionResponse::FCollisionResponse()
{

}

FCollisionResponse::FCollisionResponse(ECollisionResponse DefaultResponse)
{
	SetAllChannels(DefaultResponse);
}

/** Set the response of a particular channel in the structure. */
void FCollisionResponse::SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	ECollisionResponse DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(Channel);
	if (DefaultResponse == NewResponse)
	{
		RemoveReponseFromArray(Channel);
	}
	else
	{
		AddReponseToArray(Channel, NewResponse);
	}
#endif

	ResponseToChannels.SetResponse(Channel, NewResponse);
}

/** Set all channels to the specified response */
void FCollisionResponse::SetAllChannels(ECollisionResponse NewResponse)
{
	ResponseToChannels.SetAllChannels(NewResponse);
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	UpdateArrayFromResponseContainer();
#endif
}

void FCollisionResponse::ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)
{
	ResponseToChannels.ReplaceChannels(OldResponse, NewResponse);
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	UpdateArrayFromResponseContainer();
#endif
}

/** Returns the response set on the specified channel */
ECollisionResponse FCollisionResponse::GetResponse(ECollisionChannel Channel) const
{
	return ResponseToChannels.GetResponse(Channel);
}

/** Set all channels from ChannelResponse Array **/
void FCollisionResponse::SetCollisionResponseContainer(const FCollisionResponseContainer& InResponseToChannels)
{
	ResponseToChannels = InResponseToChannels;
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	// this is only valid case that has to be updated
	UpdateArrayFromResponseContainer();
#endif
}

void FCollisionResponse::SetResponsesArray(const TArray<FResponseChannel>& InChannelResponses)
{
#if DO_GUARD_SLOW
	// verify if the name is overlapping, if so, ensure, do not remove in debug becuase it will cause inconsistent bug between debug/release
	int32 const ResponseNum = InChannelResponses.Num();
	for (int32 I=0; I<ResponseNum; ++I)
	{
		for (int32 J=I+1; J<ResponseNum; ++J)
		{
			if (InChannelResponses[I].Channel == InChannelResponses[J].Channel)
			{
				UE_LOG(LogCollision, Warning, TEXT("Collision Channel : Redundant name exists"));
			}
		}
	}
#endif

	ResponseArray = InChannelResponses;
	UpdateResponseContainerFromArray();
}

#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
bool FCollisionResponse::RemoveReponseFromArray(ECollisionChannel Channel)
{
	// this is expensive operation, I'd love to remove names but this operation is supposed to do
	// so only allow it in editor
	// without editor, this does not have to match 
	// We'd need to save name just in case that name is gone or not
	FName ChannelName = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Channel);
	for (auto Iter=ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		if (ChannelName == (*Iter).Channel)
		{
			ResponseArray.RemoveAt(Iter.GetIndex());
			return true;
		}
	}
	return false;
}

bool FCollisionResponse::AddReponseToArray(ECollisionChannel Channel, ECollisionResponse Response)
{
	// this is expensive operation, I'd love to remove names but this operation is supposed to do
	// so only allow it in editor
	// without editor, this does not have to match 
	FName ChannelName = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Channel);
	for (auto Iter=ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		if (ChannelName == (*Iter).Channel)
		{
			(*Iter).Response = Response;
			return true;
		}
	}

	// if not add to the list
	ResponseArray.Add(FResponseChannel(ChannelName, Response));
	return true;
}

void FCollisionResponse::UpdateArrayFromResponseContainer()
{
	ResponseArray.Empty();

	const FCollisionResponseContainer& DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer();

	for(int32 i=0; i<ARRAY_COUNT(ResponseToChannels.EnumArray); i++)
	{
		// if not same as default
		if ( ResponseToChannels.EnumArray[i] != DefaultResponse.EnumArray[i] )
		{
			FName ChannelName = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(i);
			if (ChannelName != NAME_None)
			{
				FResponseChannel NewResponse;
				NewResponse.Channel = ChannelName;
				NewResponse.Response = (ECollisionResponse)ResponseToChannels.EnumArray[i];
				ResponseArray.Add(NewResponse);
			}
		}
	}
}

#endif // WITH_EDITOR

void FCollisionResponse::UpdateResponseContainerFromArray()
{
	ResponseToChannels = FCollisionResponseContainer::GetDefaultResponseContainer();

	for (auto Iter = ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		FResponseChannel& Response = *Iter;

		int32 EnumIndex = UCollisionProfile::Get()->ReturnContainerIndexFromChannelName(Response.Channel);
		if ( EnumIndex != INDEX_NONE )
		{
			ResponseToChannels.SetResponse((ECollisionChannel)EnumIndex, Response.Response);
		}
		else
		{
			// otherwise remove
			ResponseArray.RemoveAt(Iter.GetIndex());
			--Iter;
		}
	}
}
////////////////////////////////////////////////////////////////////////////

FBodyInstance::FBodyInstance()
: InstanceBodyIndex(INDEX_NONE)
, InstanceBoneIndex(INDEX_NONE)
, Scale3D(1.0f)
, SceneIndexSync(0)
, SceneIndexAsync(0)
, CollisionProfileName(UCollisionProfile::CustomCollisionProfileName)
, CollisionEnabled(ECollisionEnabled::QueryAndPhysics)
, ObjectType(ECC_WorldStatic)
, bUseCCD(false)
, bNotifyRigidBodyCollision(false)
, bSimulatePhysics(false)
, bOverrideMass(false)
, MassInKg(100.f)
, LinearDamping(0.01)
, AngularDamping(0.0)
, bEnableGravity(true)
, bAutoWeld(false)
, bWelded(false)
, bStartAwake(true)
, bUpdateMassWhenScaleChanges(false)
, bLockTranslation(true)
, bLockRotation(true)
, bLockXTranslation(false)
, bLockYTranslation(false)
, bLockZTranslation(false)
, bLockXRotation(false)
, bLockYRotation(false)
, bLockZRotation(false)
, DOFMode(0)
, CustomDOFPlaneNormal(FVector::ZeroVector)
, COMNudge(ForceInit)
, MassScale(1.f)
, MaxAngularVelocity(400.f)
, DOFConstraint(NULL)
, WeldParent(NULL)
, bUseAsyncScene(false)
, bOverrideMaxDepenetrationVelocity(false)
, MaxDepenetrationVelocity(0.f)
, bOverrideWalkableSlopeOnInstance(false)
, PhysMaterialOverride(NULL)
, SleepFamily(ESleepFamily::Normal)
, PhysicsBlendWeight(0.f)
, PositionSolverIterationCount(8)
, VelocitySolverIterationCount(1)
#if WITH_PHYSX
, RigidActorSync(NULL)
, RigidActorAsync(NULL)
, BodyAggregate(NULL)
, RigidActorSyncId(PX_SERIAL_OBJECT_ID_INVALID)
, RigidActorAsyncId(PX_SERIAL_OBJECT_ID_INVALID)
, InitialLinearVelocity(0.0f)
, bWokenExternally(false)
, PhysxUserData(this)
, CurrentSceneState(BodyInstanceSceneState::NotAdded)
#endif	//WITH_PHYSX
#if WITH_BOX2D
, BodyInstancePtr(nullptr)
#endif
{
}

FArchive& operator<<(FArchive& Ar,FBodyInstance& BodyInst)
{
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		Ar << BodyInst.OwnerComponent;
		Ar << BodyInst.PhysMaterialOverride;
	}

	return Ar;
}


/** Util for finding the parent bodyinstance of a specified body, using skeleton hierarchy */
FBodyInstance* FindParentBodyInstance(FName BodyName, USkeletalMeshComponent* SkelMeshComp)
{
	FName TestBoneName = BodyName;
	while(true)
	{
		TestBoneName = SkelMeshComp->GetParentBone(TestBoneName);
		// Bail out if parent bone not found
		if(TestBoneName == NAME_None)
		{
			return NULL;
		}

		// See if we have a body for the parent bone
		FBodyInstance* BI = SkelMeshComp->GetBodyInstance(TestBoneName);
		if(BI != NULL)
		{
			// We do - return it
			return BI;
		}

		// Don't repeat if we are already at the root!
		if(SkelMeshComp->GetBoneIndex(TestBoneName) == 0)
		{
			return NULL;
		}
	}

	return NULL;
}

FPhysScene* GetPhysicsScene(const FBodyInstance* BodyInstance)
{
	UPrimitiveComponent* OwnerComponentInst = BodyInstance->OwnerComponent.Get();
	return OwnerComponentInst ? OwnerComponentInst->GetWorld()->GetPhysicsScene() : nullptr;
}


#if WITH_PHYSX
//Determine that the shape is associated with this subbody (or root body)
bool ShapeBoundToBody(const PxShape * PShape, const FBodyInstance* SubBody)
{
	FBodyInstance* BI = FPhysxUserData::Get<FBodyInstance>(PShape->userData);
	return (SubBody->WeldParent == NULL && BI == NULL) || (BI == SubBody && BI->WeldParent != NULL);
}

int32 FBodyInstance::GetAllShapes_AssumesLocked(TArray<PxShape*>& OutShapes) const
{
	int32 NumSyncShapes = 0;
	OutShapes.Empty();
	// grab shapes from sync actor
	if(RigidActorSync)
	{
		NumSyncShapes = RigidActorSync->getNbShapes();
		OutShapes.AddUninitialized(NumSyncShapes);
		RigidActorSync->getShapes(OutShapes.GetData(), NumSyncShapes);
	}

	// grab shapes from async actor
	if( RigidActorAsync != NULL && !HasSharedShapes())
	{
		const int32 NumAsyncShapes = RigidActorAsync->getNbShapes();
		OutShapes.AddUninitialized(NumAsyncShapes);
		RigidActorAsync->getShapes(OutShapes.GetData() + NumSyncShapes, NumAsyncShapes);
	}

	return NumSyncShapes;
}

#endif

void FBodyInstance::UpdateTriMeshVertices(const TArray<FVector> & NewPositions)
{
#if WITH_PHYSX
	if (BodySetup.IsValid())
	{
		ExecuteOnPhysicsReadWrite([&]
		{
			BodySetup->UpdateTriMeshVertices(NewPositions);

			//after updating the vertices we must call setGeometry again to update any shapes referencing the mesh

			TArray<PxShape *> PShapes;
			const int32 SyncShapeCount = GetAllShapes_AssumesLocked(PShapes);
			PxTriangleMeshGeometry PTriangleMeshGeometry;
			for (int32 ShapeIdx = 0; ShapeIdx < PShapes.Num(); ShapeIdx++)
			{
				PxShape* PShape = PShapes[ShapeIdx];
				if (PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
				{
					PShape->getTriangleMeshGeometry(PTriangleMeshGeometry);
					PShape->setGeometry(PTriangleMeshGeometry);
				}
			}
		});
	}
#endif
}

DEFINE_STAT(STAT_UpdatePhysMats);
DEFINE_STAT(STAT_UpdatePhysMatsSceneInteraction);

void FBodyInstance::UpdatePhysicalMaterials()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePhysMats);
	UPhysicalMaterial* SimplePhysMat = GetSimplePhysicalMaterial();
	check(SimplePhysMat != NULL);
	TArray<UPhysicalMaterial*> ComplexPhysMats = GetComplexPhysicalMaterials();

#if WITH_PHYSX
	PxMaterial* PSimpleMat = SimplePhysMat->GetPhysXMaterial();
	check(PSimpleMat != NULL);
	ExecuteOnPhysicsReadWrite([&]()
	{
		ApplyMaterialToInstanceShapes_AssumesLocked(PSimpleMat, ComplexPhysMats);
	});
#endif

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		for (b2Fixture* Fixture = BodyInstancePtr->GetFixtureList(); Fixture; Fixture = Fixture->GetNext())
		{
			Fixture->SetFriction(SimplePhysMat->Friction);
			Fixture->SetRestitution(SimplePhysMat->Restitution);

			//@TODO: BOX2D: Determine if it's feasible to add support for FrictionCombineMode to Box2D
		}
	}
#endif
}

void FBodyInstance::InvalidateCollisionProfileName()
{
	CollisionProfileName = UCollisionProfile::CustomCollisionProfileName;
}

ECollisionResponse FBodyInstance::GetResponseToChannel(ECollisionChannel Channel) const
{
	return CollisionResponses.GetResponse(Channel);
}

void FBodyInstance::SetResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	ResponseToChannels_DEPRECATED.SetResponse(Channel, NewResponse);
	CollisionResponses.SetResponse(Channel, NewResponse);
	UpdatePhysicsFilterData();
}

const FCollisionResponseContainer& FBodyInstance::GetResponseToChannels() const
{
	return CollisionResponses.GetResponseContainer();
}

void FBodyInstance::SetResponseToAllChannels(ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	ResponseToChannels_DEPRECATED.SetAllChannels(NewResponse);
	CollisionResponses.SetAllChannels(NewResponse);
	UpdatePhysicsFilterData();
}
	
void FBodyInstance::ReplaceResponseToChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	ResponseToChannels_DEPRECATED.ReplaceChannels(OldResponse, NewResponse);
	CollisionResponses.ReplaceChannels(OldResponse, NewResponse);
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetResponseToChannels(const FCollisionResponseContainer& NewReponses)
{
	InvalidateCollisionProfileName();
	CollisionResponses.SetCollisionResponseContainer(NewReponses);
	UpdatePhysicsFilterData();
}
	
void FBodyInstance::SetObjectType(ECollisionChannel Channel)
{
	InvalidateCollisionProfileName();
	ObjectType = Channel;
	UpdatePhysicsFilterData();
}

ECollisionChannel FBodyInstance::GetObjectType() const
{
	return ObjectType;
}

void FBodyInstance::SetCollisionProfileName(FName InCollisionProfileName)
{
	if (CollisionProfileName != InCollisionProfileName)
	{
		CollisionProfileName = InCollisionProfileName;
		// now Load ProfileData
		LoadProfileData(false);
	}
}

FName FBodyInstance::GetCollisionProfileName() const
{
	return CollisionProfileName;
}

bool FBodyInstance::DoesUseCollisionProfile() const
{
	return IsValidCollisionProfileName(CollisionProfileName);
}

void FBodyInstance::SetMassScale(float InMassScale)
{
	MassScale = InMassScale;
	UpdateMassProperties();
}

void FBodyInstance::SetCollisionEnabled(ECollisionEnabled::Type NewType, bool bUpdatePhysicsFilterData)
{
	if (CollisionEnabled != NewType)
	{
		InvalidateCollisionProfileName();
		CollisionEnabled = NewType;
		
		// Only update physics filter data if required
		if (bUpdatePhysicsFilterData)
		{
			UpdatePhysicsFilterData();
		}
	}
}



EDOFMode::Type FBodyInstance::ResolveDOFMode(EDOFMode::Type DOFMode)
{
	EDOFMode::Type ResultDOF = DOFMode;
	if (DOFMode == EDOFMode::Default)
	{
		ESettingsDOF::Type SettingDefaultPlane = UPhysicsSettings::Get()->DefaultDegreesOfFreedom;
		if (SettingDefaultPlane == ESettingsDOF::XYPlane) ResultDOF = EDOFMode::XYPlane;
		if (SettingDefaultPlane == ESettingsDOF::XZPlane) ResultDOF = EDOFMode::XZPlane;
		if (SettingDefaultPlane == ESettingsDOF::YZPlane) ResultDOF = EDOFMode::YZPlane;
		if (SettingDefaultPlane == ESettingsDOF::Full3D) ResultDOF  = EDOFMode::SixDOF;
	}

	return ResultDOF;
}

FVector FBodyInstance::GetLockedAxis() const
{
	EDOFMode::Type MyDOFMode = ResolveDOFMode(DOFMode);

	switch (MyDOFMode)
	{
	case EDOFMode::None: return FVector::ZeroVector;
	case EDOFMode::YZPlane: return FVector(1, 0, 0);
	case EDOFMode::XZPlane: return FVector(0, 1, 0);
	case EDOFMode::XYPlane: return FVector(0, 0, 1);
	case EDOFMode::CustomPlane: return CustomDOFPlaneNormal;
	case EDOFMode::SixDOF: return FVector::ZeroVector;
	default:	check(0);	//unsupported locked axis type
	}

	return FVector::ZeroVector;
}

void FBodyInstance::SetDOFLock(EDOFMode::Type NewAxisMode)
{
	DOFMode = NewAxisMode;

	CreateDOFLock();
}

void FBodyInstance::CreateDOFLock()
{
	if (DOFConstraint)
	{
		DOFConstraint->TermConstraint();
		FConstraintInstance::Free(DOFConstraint);
		DOFConstraint = NULL;
	}

	const FVector LockedAxis = GetLockedAxis();
	const EDOFMode::Type DOF = ResolveDOFMode(DOFMode);

	if (IsDynamic() == false || (LockedAxis.IsNearlyZero() && DOF != EDOFMode::SixDOF))
	{
		return;
	}

	//if we're using SixDOF make sure we have at least one constraint
	if (DOF == EDOFMode::SixDOF && !bLockXTranslation && !bLockYTranslation && !bLockZTranslation && !bLockXRotation && !bLockYRotation && !bLockZRotation)
	{
		return;
	}

	DOFConstraint = FConstraintInstance::Alloc();
	{
		DOFConstraint->bSwingLimitSoft = false;
		DOFConstraint->bTwistLimitSoft = false;
		DOFConstraint->bLinearLimitSoft = false;

		const FTransform TM = GetUnrealWorldTransform();
		FVector Normal = FVector(1, 0, 0);
		FVector Sec = FVector(0, 1, 0);


		if(DOF != EDOFMode::SixDOF)
		{
			DOFConstraint->AngularSwing1Motion = (bLockRotation || DOFMode != EDOFMode::CustomPlane) ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free;
			DOFConstraint->AngularSwing2Motion = (bLockRotation || DOFMode != EDOFMode::CustomPlane) ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free;
			DOFConstraint->AngularTwistMotion = EAngularConstraintMotion::ACM_Free;

			DOFConstraint->LinearXMotion = (bLockTranslation || DOFMode != EDOFMode::CustomPlane) ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free;
			DOFConstraint->LinearYMotion = ELinearConstraintMotion::LCM_Free;
			DOFConstraint->LinearZMotion = ELinearConstraintMotion::LCM_Free;

			Normal = LockedAxis.GetSafeNormal();
			FVector Garbage;
			Normal.FindBestAxisVectors(Garbage, Sec);
		}else
		{
			DOFConstraint->AngularTwistMotion = bLockXRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free;
			DOFConstraint->AngularSwing2Motion = bLockYRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free;
			DOFConstraint->AngularSwing1Motion = bLockZRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free;

			DOFConstraint->LinearXMotion = bLockXTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free;
			DOFConstraint->LinearYMotion = bLockYTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free;
			DOFConstraint->LinearZMotion = bLockZTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free;
		}

		DOFConstraint->PriAxis1 = TM.InverseTransformVectorNoScale(Normal);
		DOFConstraint->SecAxis1 = TM.InverseTransformVectorNoScale(Sec);

		DOFConstraint->PriAxis2 = Normal;
		DOFConstraint->SecAxis2 = Sec;
		DOFConstraint->Pos2 = TM.GetLocation();

		// Create constraint instance based on DOF
		DOFConstraint->InitConstraint(OwnerComponent.Get(), this, nullptr, 1.f);
	}
}

ECollisionEnabled::Type FBodyInstance::GetCollisionEnabled() const
{
	// Check actor override
	const UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;
	if ((Owner != NULL) && !Owner->GetActorEnableCollision())
	{
		return ECollisionEnabled::NoCollision;
	}
	else
	{
		return CollisionEnabled;
	}
}

#if WITH_PHYSX
PxShape* ClonePhysXShape_AssumesLocked(PxShape* PShape)
{
	//NOTE: this code is copied directly from ExtSimpleFactory.cpp because physx does not currently have an API for copying a shape or incrementing a ref count.
	//This is hard to maintain so once they provide us with it we can get rid of this

	PxU16 PMaterialCount = PShape->getNbMaterials();

	TArray<PxMaterial*, TInlineAllocator<64>> PMaterials;

	PMaterials.AddZeroed(PMaterialCount);
	PShape->getMaterials(&PMaterials[0], PMaterialCount);

	PxShape* PNewShape = GPhysXSDK->createShape(PShape->getGeometry().any(), &PMaterials[0], PMaterialCount, false, PShape->getFlags());
	PNewShape->setLocalPose(PShape->getLocalPose());
	PNewShape->setContactOffset(PShape->getContactOffset());
	PNewShape->setRestOffset(PShape->getRestOffset());
	PNewShape->setSimulationFilterData(PShape->getSimulationFilterData());
	PNewShape->setQueryFilterData(PShape->getQueryFilterData());

	return PNewShape;
}


template<typename Lambda>
void ExecuteOnPxShapeWrite(FBodyInstance* BodyInstance, PxShape* PShape, Lambda Func)
{
	const bool bSharedShapes = BodyInstance->HasSharedShapes();
	if (bSharedShapes)
	{
		//we must now create a new shape because calling detach will lose our ref count (there's no way to increment it)
		//shape sharing is only done on static actors so this code should never execute outside the editor
		PxShape* PNewShape = ClonePhysXShape_AssumesLocked(PShape);
		BodyInstance->RigidActorSync->detachShape(*PShape, false);
		BodyInstance->RigidActorAsync->detachShape(*PShape, false);
		PShape = PNewShape;
	}

	Func(PShape);

	if (bSharedShapes)
	{
		BodyInstance->RigidActorSync->attachShape(*PShape);
		BodyInstance->RigidActorAsync->attachShape(*PShape);
		PShape->release();	//we must have created a new shape so release our reference to it (held by actors)
	}
}


void FBodyInstance::UpdatePhysicsShapeFilterData(uint32 SkelMeshCompID, bool bUseComplexAsSimple, bool bUseSimpleAsComplex, bool bPhysicsStatic, TEnumAsByte<ECollisionEnabled::Type> * CollisionEnabledOverride, FCollisionResponseContainer * ResponseOverride, bool * bNotifyOverride)
{
	ExecuteOnPhysicsReadWrite([&]
	{
		if (PxRigidActor* PActor = GetPxRigidActor_AssumesLocked())
		{

		// Iterate over all shapes and assign filterdata
			TArray<PxShape*> AllShapes;
			const int32 NumSyncShapes = GetAllShapes_AssumesLocked(AllShapes);

		bool bUpdateMassProperties = false;
		for (int32 ShapeIdx = 0; ShapeIdx < AllShapes.Num(); ShapeIdx++)
		{
			PxShape* PShape = AllShapes[ShapeIdx];
			FBodyInstance* BI = FPhysxUserData::Get<FBodyInstance>(PShape->userData);
			BI = BI ? BI : this;

			const TEnumAsByte<ECollisionEnabled::Type> & UseCollisionEnabled = CollisionEnabledOverride ? *CollisionEnabledOverride : (TEnumAsByte<ECollisionEnabled::Type>)BI->GetCollisionEnabled();
			const FCollisionResponseContainer& UseResponse = ResponseOverride ? *ResponseOverride : BI->CollisionResponses.GetResponseContainer();
			bool bUseNotify = bNotifyOverride ? *bNotifyOverride : BI->bNotifyRigidBodyCollision;


			UPrimitiveComponent* OwnerPrimitiveComponent = BI->OwnerComponent.Get();
			int32 CompID = (OwnerPrimitiveComponent != nullptr) ? OwnerPrimitiveComponent->GetUniqueID() : 0;

			// Create the filterdata structs
			PxFilterData PSimFilterData;
			PxFilterData PSimpleQueryData;
			PxFilterData PComplexQueryData;
			if (UseCollisionEnabled != ECollisionEnabled::NoCollision)
			{
				CreateShapeFilterData(BI->ObjectType, CompID, UseResponse, SkelMeshCompID, InstanceBodyIndex, PSimpleQueryData, PSimFilterData, bUseCCD && !bPhysicsStatic, bUseNotify, bPhysicsStatic);	//InstanceBodyIndex and CCD are determined by root body in case of welding
				PComplexQueryData = PSimpleQueryData;

				// Build filterdata variations for complex and simple
				PSimpleQueryData.word3 |= EPDF_SimpleCollision;
				if (bUseSimpleAsComplex)
				{
					PSimpleQueryData.word3 |= EPDF_ComplexCollision;
				}

				PComplexQueryData.word3 |= EPDF_ComplexCollision;
				if (bUseComplexAsSimple)
				{
					PComplexQueryData.word3 |= EPDF_SimpleCollision;
				}
			}

			PShape->setSimulationFilterData(PSimFilterData);

			// If query collision is enabled..
			if ((UseCollisionEnabled == ECollisionEnabled::QueryAndPhysics) || (UseCollisionEnabled == ECollisionEnabled::QueryOnly))
			{
				// Only perform scene queries in the synchronous scene for static shapes
				if (bPhysicsStatic)
				{
					bool bIsSyncShape = (ShapeIdx < NumSyncShapes);
					PShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bIsSyncShape);
				}
				// If non-static, always enable scene queries
				else
				{
					PShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
				}

				// See if we want physics collision
				bool bSimCollision = (UseCollisionEnabled == ECollisionEnabled::QueryAndPhysics);

				// Triangle mesh is 'complex' geom
				if (PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
				{
					PShape->setQueryFilterData(PComplexQueryData);

					// on dynamic objects and objects which don't use complex as simple, tri mesh not used for sim
					if (!bSimCollision || !bUseComplexAsSimple)
					{
						PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
					}
					else
					{
						PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
					}

					if (OwnerPrimitiveComponent == NULL || !OwnerPrimitiveComponent->IsA(UModelComponent::StaticClass()))
					{
						PShape->setFlag(PxShapeFlag::eVISUALIZATION, false); // dont draw the tri mesh, we can see it anyway, and its slow
					}
				}
				// Everything else is 'simple'
				else
				{
					PShape->setQueryFilterData(PSimpleQueryData);

					// See if we currently have sim collision
					bool bCurrentSimCollision = (PShape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE);
					// Enable sim collision
					if (bSimCollision && !bCurrentSimCollision)
					{
						bUpdateMassProperties = true;
						PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
					}
					// Disable sim collision
					else if (!bSimCollision && bCurrentSimCollision)
					{
						bUpdateMassProperties = true;
						PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
					}

					// enable swept bounds for CCD for this shape
						PxRigidBody* PBody = PActor->is<PxRigidBody>();
					if (bSimCollision && !bPhysicsStatic && bUseCCD && PBody)
					{
						PBody->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
					}
					else if (PBody)
					{

						PBody->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, false);
					}
				}
			}
			// No collision enabled
			else
			{
				PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
				PShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
			}
		}

		if (bUpdateMassProperties)
		{
			UpdateMassProperties();
		}
	}
	});
}
#endif

/** Update the filter data on the physics shapes, based on the owning component flags. */
void FBodyInstance::UpdatePhysicsFilterData(bool bForceSimpleAsComplex)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePhysFilter);

	// Do nothing if no physics actor
	if (!IsValidBodyInstance())
	{
		return;
	}

	// this can happen in landscape height field collision component
	if (!BodySetup.IsValid())
	{
		return;
	}

	// Figure out if we are static
	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;
	const bool bPhysicsStatic = !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry();

	// Grab collision setting from body instance
	TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled = GetCollisionEnabled(); // this checks actor override
	bool bUseNotifyRBCollision = bNotifyRigidBodyCollision;
	FCollisionResponseContainer UseResponse = CollisionResponses.GetResponseContainer();

	bool bUseCollisionEnabledOverride = false;
	bool bResponseOverride = false;
	bool bNotifyOverride = false;

	// Get skelmeshcomp ID
	uint32 SkelMeshCompID = 0;
	if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(OwnerComponentInst))
	{
		SkelMeshCompID = SkelMeshComp->GetUniqueID();

		// In skeletal case, collision enable/disable/movement should be overriden by mesh component
		// being in the physics asset, and not having collision is a waste and it can cause a bug where disconnected bodies
		if (Owner)
		{
			if (Owner->GetActorEnableCollision())	//we only override if actor has collision enabled
			{
				UseCollisionEnabled = SkelMeshComp->BodyInstance.CollisionEnabled;
			}
			else
			{	//actor has collision disabled so disable regardless of component setting
				UseCollisionEnabled = ECollisionEnabled::NoCollision;
			}
		}
		

		ObjectType = SkelMeshComp->GetCollisionObjectType();
		bUseCollisionEnabledOverride = true;

		if (BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Enabled)
		{
			UseResponse.SetAllChannels(ECR_Block);
		}
		else if (BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
		{
			UseResponse.SetAllChannels(ECR_Ignore);
		}

		UseResponse = FCollisionResponseContainer::CreateMinContainer(UseResponse, SkelMeshComp->BodyInstance.CollisionResponses.GetResponseContainer());
		bUseNotifyRBCollision = bUseNotifyRBCollision && SkelMeshComp->BodyInstance.bNotifyRigidBodyCollision;
		bResponseOverride = true;
		bNotifyOverride = true;
	}

#if WITH_EDITOR
	// if no collision, but if world wants to enable trace collision for components, allow it
	if ((UseCollisionEnabled == ECollisionEnabled::NoCollision) && Owner && (Cast<AVolume>(Owner) == nullptr))
	{
		UWorld* World = Owner->GetWorld();
		UPrimitiveComponent* PrimComp = OwnerComponentInst;
		if (World && World->bEnableTraceCollision && 
			(Cast<UStaticMeshComponent>(PrimComp) || Cast<USkeletalMeshComponent>(PrimComp) || Cast<UBrushComponent>(PrimComp)))
		{
			//UE_LOG(LogPhysics, Warning, TEXT("Enabling collision %s : %s"), *GetNameSafe(Owner), *GetNameSafe(OwnerComponent.Get()));
			// clear all other channel just in case other people using those channels to do something
			UseResponse.SetAllChannels(ECR_Ignore);
			UseCollisionEnabled = ECollisionEnabled::QueryOnly;
			bResponseOverride = true;
			bUseCollisionEnabledOverride = true;
		}
	}
#endif

	const bool bUseComplexAsSimple = !bForceSimpleAsComplex && (BodySetup.Get()->CollisionTraceFlag == CTF_UseComplexAsSimple);
	const bool bUseSimpleAsComplex = bForceSimpleAsComplex || (BodySetup.Get()->CollisionTraceFlag == CTF_UseSimpleAsComplex);

#if WITH_PHYSX
	TEnumAsByte<ECollisionEnabled::Type> * CollisionEnabledOverride = bUseCollisionEnabledOverride ? &UseCollisionEnabled : NULL;
	FCollisionResponseContainer * ResponseOverride = bResponseOverride ? &UseResponse : NULL;
	bool * bNotifyOverridePtr = bNotifyOverride ? &bUseNotifyRBCollision : NULL;
	UpdatePhysicsShapeFilterData(SkelMeshCompID, bUseComplexAsSimple, bUseSimpleAsComplex, bPhysicsStatic, CollisionEnabledOverride, ResponseOverride, bNotifyOverridePtr);
#endif

#if WITH_BOX2D
	if (BodyInstancePtr != nullptr)
	{
		if (UseCollisionEnabled != ECollisionEnabled::NoCollision)
		{
			// Create the simulation/query filter data
			FPhysicsFilterBuilder FilterBuilder(ObjectType, UseResponse);
 			FilterBuilder.ConditionalSetFlags(EPDF_CCD, bUseCCD && !bPhysicsStatic);
			FilterBuilder.ConditionalSetFlags(EPDF_ContactNotify, bUseNotifyRBCollision);
 			FilterBuilder.ConditionalSetFlags(EPDF_StaticShape, bPhysicsStatic);

			b2Filter BoxSimFilterData;
			FilterBuilder.GetCombinedData(/*out*/ BoxSimFilterData.BlockingChannels, /*out*/ BoxSimFilterData.TouchingChannels, /*out*/ BoxSimFilterData.ObjectTypeAndFlags);
			BoxSimFilterData.UniqueComponentID = SkelMeshCompID;
			BoxSimFilterData.BodyIndex = InstanceBodyIndex;

			// Update the body data
			const bool bSimCollision = (UseCollisionEnabled == ECollisionEnabled::QueryAndPhysics);
			BodyInstancePtr->SetBullet(bSimCollision && !bPhysicsStatic && bUseCCD);
			BodyInstancePtr->SetActive(true);

			// Copy the filter data to each fixture in the body
			for (b2Fixture* Fixture = BodyInstancePtr->GetFixtureList(); Fixture; Fixture = Fixture->GetNext())
			{
				Fixture->SetFilterData(BoxSimFilterData);
				Fixture->SetSensor(UseCollisionEnabled == ECollisionEnabled::QueryOnly);
			}
		}
		else
		{
			// No collision
			BodyInstancePtr->SetActive(false);
		}
	}
#endif
}

#if UE_WITH_PHYSICS


template <bool bCompileStatic>
struct FInitBodiesHelper
{
	FInitBodiesHelper(const TArray<FBodyInstance*>& InBodies, const TArray<FTransform>& InTransforms, class UBodySetup* InBodySetup, class UPrimitiveComponent* InPrimitiveComp, class FPhysScene* InInRBScene, FBodyInstance::PhysXAggregateType InInAggregate = NULL, UPhysicsSerializer* InPhysicsSerializer = nullptr)
	: Bodies(InBodies)
	, Transforms(InTransforms)
	, BodySetup(InBodySetup)
	, PrimitiveComp(InPrimitiveComp)
	, PhysScene(InInRBScene)
	, InAggregate(InInAggregate)
	, PhysicsSerializer(InPhysicsSerializer)
	, DebugName(TEXT(""))
	, InstanceBlendWeight(-1.f)
	, bInstanceSimulatePhysics(false)
	, bComponentAwake(false)
	, SkelMeshComp(nullptr)
	, InitialLinVel(EForceInit::ForceInitToZero)
	{
		//Compute all the needed constants

		PhysXName = GetDebugDebugName(PrimitiveComp, BodySetup, DebugName);

		bStatic = bCompileStatic || PrimitiveComp == nullptr || PrimitiveComp->Mobility != EComponentMobility::Movable;
		SkelMeshComp = bCompileStatic ? nullptr : GetSkeletalMeshComponentAndProperties(PrimitiveComp, BodySetup, InstanceBlendWeight, bInstanceSimulatePhysics, bComponentAwake);

		const AActor* OwningActor = PrimitiveComp ? PrimitiveComp->GetOwner() : nullptr;
		InitialLinVel = GetInitialLinearVelocity(OwningActor, bComponentAwake);

#if WITH_PHYSX
		PSyncScene = PhysScene->GetPhysXScene(PST_Sync);
		PAsyncScene = PhysScene->HasAsyncScene() ? PhysScene->GetPhysXScene(PST_Async) : nullptr;
#endif

	}

	void InitBodies() const
	{
#if WITH_PHYSX
		InitBodies_PhysX();
#endif

#if WITH_BOX2D
		InitBodies_Box2D();
#endif
	}

	//The arguments passed into InitBodies
	const TArray<FBodyInstance*>& Bodies;
	const TArray<FTransform>& Transforms;
	class UBodySetup* BodySetup;
	class UPrimitiveComponent* PrimitiveComp;
	class FPhysScene* PhysScene;
	FBodyInstance::PhysXAggregateType InAggregate;
	UPhysicsSerializer* PhysicsSerializer;

	FString DebugName;
	TSharedPtr<TArray<ANSICHAR>> PhysXName;

	//The constants shared between PhysX and Box2D
	bool bStatic;
	float InstanceBlendWeight;
	bool bInstanceSimulatePhysics;
	bool bComponentAwake;

	const USkeletalMeshComponent* SkelMeshComp;
	FVector InitialLinVel;

#if WITH_PHYSX

	PxScene* PSyncScene;
	PxScene* PAsyncScene;

	bool GetBinaryData_PhysX_AssumesLocked(FBodyInstance* Instance) const
	{
		Instance->RigidActorSync = PhysicsSerializer->GetRigidActor(Instance->RigidActorSyncId);
		Instance->RigidActorAsync = PhysicsSerializer->GetRigidActor(Instance->RigidActorAsyncId);

		return Instance->RigidActorSync || Instance->RigidActorAsync;
	}


	physx::PxRigidActor* CreateActor_PhysX_AssumesLocked(FBodyInstance* Instance, const PxTransform& PTransform) const
	{
		physx::PxRigidDynamic* PNewDynamic = nullptr;

		if (bCompileStatic || bStatic)
		{
			Instance->RigidActorSync = GPhysXSDK->createRigidStatic(PTransform);
			if (PAsyncScene)
			{
				Instance->RigidActorAsync = GPhysXSDK->createRigidStatic(PTransform);
			}
		}
		else
		{
			PNewDynamic = GPhysXSDK->createRigidDynamic(PTransform);
			if (Instance->UseAsyncScene(PhysScene))
			{
				Instance->RigidActorAsync = PNewDynamic;
			}
			else
			{
				Instance->RigidActorSync = PNewDynamic;
			}

			if(!Instance->ShouldInstanceSimulatingPhysics())
			{
				PNewDynamic->setRigidDynamicFlag(PxRigidDynamicFlag::eKINEMATIC, true);
			}
		}

		return PNewDynamic;
	}

	bool CreateShapes_PhysX_AssumesLocked(FBodyInstance* Instance, physx::PxRigidActor* PNewDynamic) const
	{
		UPhysicalMaterial* SimplePhysMat = Instance->GetSimplePhysicalMaterial();
		TArray<UPhysicalMaterial*> ComplexPhysMats = Instance->GetComplexPhysicalMaterials();
		PxMaterial* PSimpleMat = SimplePhysMat->GetPhysXMaterial();

		FShapeData ShapeData;
		Instance->GetFilterData_AssumesLocked(ShapeData);
		Instance->GetShapeFlags_AssumesLocked(ShapeData, Instance->CollisionEnabled, BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple);

		if (!bCompileStatic && PNewDynamic)
		{
			if (!Instance->ShouldInstanceSimulatingPhysics())
			{
				ShapeData.SyncBodyFlags |= PxRigidBodyFlag::eKINEMATIC;
			}
			ShapeData.SyncBodyFlags |= PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES;
		}

		bool bInitFail = false;

		const bool bShapeSharing = Instance->HasSharedShapes(); //If we have a static actor we can reuse the shapes between sync and async scene
		TArray<PxShape*> PSharedShapes;
		if (Instance->RigidActorSync)
		{
			BodySetup->AddShapesToRigidActor_AssumesLocked(Instance, Instance->RigidActorSync, PST_Sync, Instance->Scale3D, PSimpleMat, ComplexPhysMats, ShapeData, FTransform::Identity, bShapeSharing ? &PSharedShapes : nullptr, bShapeSharing);
			bInitFail |= Instance->RigidActorSync->getNbShapes() == 0;
			Instance->RigidActorSync->userData = &Instance->PhysxUserData;
			Instance->RigidActorSync->setName(Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr);

			check(FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorSync->userData) == Instance && FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorSync->userData)->OwnerComponent != NULL);

		}

		if (Instance->RigidActorAsync)
		{
			check(PAsyncScene);
			if (bShapeSharing)
			{
				for (PxShape* PShape : PSharedShapes)
				{
					Instance->RigidActorAsync->attachShape(*PShape);
				}
			}
			else
			{
				BodySetup->AddShapesToRigidActor_AssumesLocked(Instance, Instance->RigidActorAsync, PST_Async, Instance->Scale3D, PSimpleMat, ComplexPhysMats, ShapeData);
			}

			bInitFail |= Instance->RigidActorAsync->getNbShapes() == 0;
			Instance->RigidActorAsync->userData = &Instance->PhysxUserData;
			Instance->RigidActorAsync->setName(Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr);

			check(FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorAsync->userData) == Instance && FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorAsync->userData)->OwnerComponent != NULL);
		}

		return bInitFail;
	}

	bool CreateShapesAndActors_PhysX(TArray<PxActor*>& PSyncActors, TArray<PxActor*>& PAsyncActors, TArray<PxActor*>& PDynamicActors, const bool bCanDefer, bool& bDynamicsUseAsyncScene) const
	{
		const int32 NumBodies = Bodies.Num();
		PSyncActors.Reserve(NumBodies);

		if (PAsyncScene)
		{
			// Only allocate this if we can have async bodies
			PAsyncActors.Reserve(NumBodies);
		}

		// Ensure we have the AggGeom inside the body setup so we can calculate the number of shapes
		BodySetup->CreatePhysicsMeshes();
		for (int32 BodyIdx = 0; BodyIdx < NumBodies; ++BodyIdx)
		{
			FBodyInstance* Instance = Bodies[BodyIdx];
			const FTransform& Transform = Transforms[BodyIdx];

			FBodyInstance::ValidateTransform(Transform, DebugName, BodySetup);

			Instance->OwnerComponent = PrimitiveComp;
			Instance->BodySetup = BodySetup;
			Instance->Scale3D = Transform.GetScale3D();
			Instance->CharDebugName = PhysXName;
			Instance->bHasSharedShapes = bStatic && PhysScene->HasAsyncScene() && UPhysicsSettings::Get()->bEnableShapeSharing;

			// Handle autowelding here to avoid extra work
			if (!bCompileStatic && Instance->bAutoWeld)
			{
				UPrimitiveComponent * ParentPrimComponent = PrimitiveComp ? Cast<UPrimitiveComponent>(PrimitiveComp->AttachParent) : NULL;

				//if we have a parent we will now do the weld and exit any further initialization
				if (ParentPrimComponent && PrimitiveComp->GetWorld() && PrimitiveComp->GetWorld()->IsGameWorld())
				{
					if (PrimitiveComp->WeldToImplementation(ParentPrimComponent, PrimitiveComp->AttachSocketName, false))	//welded new simulated body so initialization is done
					{
						return false;
					}
				}
			}

			// Don't process if we've already got a body
			if (Instance->GetPxRigidActor_AssumesLocked())
			{
				Instance->OwnerComponent = nullptr;
				Instance->BodySetup = nullptr;

				continue;
			}

			// Set sim parameters for bodies from skeletal mesh components
			if (!bCompileStatic && SkelMeshComp)
			{
				Instance->bSimulatePhysics = bInstanceSimulatePhysics;
				if (InstanceBlendWeight != -1.0f)
				{
					Instance->PhysicsBlendWeight = InstanceBlendWeight;
				}
			}

			Instance->PhysxUserData = FPhysxUserData(Instance);

			static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());

			physx::PxRigidActor* PNewDynamic = nullptr;
			bool bFoundBinaryData = false;
			if (PhysicsSerializer)
			{
				bFoundBinaryData = GetBinaryData_PhysX_AssumesLocked(Instance);
			}

			if (!bFoundBinaryData)
			{
				PNewDynamic = CreateActor_PhysX_AssumesLocked(Instance, U2PTransform(Transform));
				const bool bInitFail = CreateShapes_PhysX_AssumesLocked(Instance, PNewDynamic);

				if (bInitFail)
				{
					UE_LOG(LogPhysics, Log, TEXT("Init Instance %d of Primitive Component %s failed"), BodyIdx, *PrimitiveComp->GetName());
					if (Instance->RigidActorSync)
					{
						Instance->RigidActorSync->release();
						Instance->RigidActorSync = nullptr;
					}

					if (Instance->RigidActorAsync)
					{
						Instance->RigidActorAsync->release();
						Instance->RigidActorAsync = nullptr;
					}

					Instance->OwnerComponent = nullptr;
					Instance->BodySetup = nullptr;

					continue;
				}
			}

			if (Instance->RigidActorSync)
			{
				Instance->RigidActorSync->userData = &Instance->PhysxUserData;
				Instance->RigidActorSync->setName(Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr);
				check(FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorSync->userData) == Instance && FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorSync->userData)->OwnerComponent != NULL);
			}

			if (Instance->RigidActorAsync)
			{
				Instance->RigidActorAsync->userData = &Instance->PhysxUserData;
				Instance->RigidActorAsync->setName(Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr);
				check(FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorAsync->userData) == Instance && FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorAsync->userData)->OwnerComponent != NULL);
			}

			//handle special stuff only dynamic actors care about
			if (!bCompileStatic && PNewDynamic)
			{
				// turn off gravity if desired
				if (!Instance->bEnableGravity)
				{
					PNewDynamic->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
				}

				bDynamicsUseAsyncScene = Instance->UseAsyncScene(PhysScene);
			}

			if (bCompileStatic || bCanDefer)
			{
				if (!bCompileStatic && PNewDynamic)
				{
					PhysScene->DeferAddActor(Instance, PNewDynamic, Instance->UseAsyncScene(PhysScene) ? PST_Async : PST_Sync);
				}
				else
				{
					if (Instance->RigidActorSync)
					{
						PhysScene->DeferAddActor(Instance, Instance->RigidActorSync, PST_Sync);
					}
					if (Instance->RigidActorAsync)
					{
						PhysScene->DeferAddActor(Instance, Instance->RigidActorAsync, PST_Async);
					}
				}
			}
			else
			{
				if (Instance->RigidActorSync)
				{
					PSyncActors.Add(Instance->RigidActorSync);
				}

				if (Instance->RigidActorAsync)
				{
					PAsyncActors.Add(Instance->RigidActorAsync);
				}

				if (PNewDynamic)
				{
					PDynamicActors.Add(PNewDynamic);
				}

				// Set the instance to added as we'll add it to the scene in a moment.
				// This makes sure if it ends up in one of the deferred queues later it 
				// functions correctly
				Instance->CurrentSceneState = BodyInstanceSceneState::Added;
			}

			Instance->SceneIndexSync = PhysScene->PhysXSceneIndex[PST_Sync];
			Instance->SceneIndexAsync = PAsyncScene ? PhysScene->PhysXSceneIndex[PST_Async] : 0;
			Instance->InitialLinearVelocity = InitialLinVel;
			Instance->bWokenExternally = bComponentAwake;
		}

		return true;
	}

	void AddActorsToScene_PhysX_AssumesLocked(TArray<PxActor*>& PSyncActors, TArray<PxActor*>& PAsyncActors, TArray<PxActor*>& PDynamicActors, PxScene* PDyanmicScene) const
	{
		SCOPE_CYCLE_COUNTER(STAT_BulkSceneAdd);


		// Use the aggregate if it exists, has enough slots and is in the correct scene (or no scene)
		if (InAggregate && PDynamicActors.Num() > 0 && (InAggregate->getMaxNbActors() - InAggregate->getNbActors()) >= (uint32)PDynamicActors.Num())
		{
			if (InAggregate->getScene() == nullptr || InAggregate->getScene() == PDyanmicScene)
			{
				for (PxActor* Actor : PDynamicActors)
				{
					InAggregate->addActor(*Actor);
				}
			}
		}
		else
		{

			for (PxActor* Actor : PSyncActors)
			{
				PSyncScene->addActor(*Actor);
			}

			if (PAsyncScene)
			{
				for (PxActor* Actor : PAsyncActors)
				{
					PAsyncScene->addActor(*Actor);
				}
			}
		}

		// Set up dynamic instance data
		if (!bCompileStatic && !bStatic)
		{
			SCOPE_CYCLE_COUNTER(STAT_InitBodyPostAdd);
			for (int32 BodyIdx = 0, NumBodies = Bodies.Num(); BodyIdx < NumBodies; ++BodyIdx)
			{
				FBodyInstance* Instance = Bodies[BodyIdx];
				Instance->InitDynamicProperties_AssumesLocked();
			}
		}
	}

	void InitBodies_PhysX() const
	{
		TArray<PxActor*> PSyncActors;
		TArray<PxActor*> PAsyncActors;
		TArray<PxActor*> PDynamicActors;

		// Only static objects qualify for deferred addition
		const bool bCanDefer = bCompileStatic;
		bool bDynamicsUseAsync = false;
		if (CreateShapesAndActors_PhysX(PSyncActors, PAsyncActors, PDynamicActors, bCanDefer, bDynamicsUseAsync) == false)
		{
			return;
		}

		if (!bCompileStatic && !bCanDefer)
		{
			const bool bAddingToSyncScene = (PSyncActors.Num() || (PDynamicActors.Num() && !bDynamicsUseAsync)) && PSyncScene;
			const bool bAddingToAsyncScene = (PAsyncActors.Num() || (PDynamicActors.Num() && bDynamicsUseAsync)) && PAsyncScene;

			SCOPED_SCENE_WRITE_LOCK(bAddingToSyncScene ? PSyncScene : nullptr);
			SCOPED_SCENE_WRITE_LOCK(bAddingToAsyncScene ? PAsyncScene : nullptr);

			AddActorsToScene_PhysX_AssumesLocked(PSyncActors, PAsyncActors, PDynamicActors, bDynamicsUseAsync ? PAsyncScene : PSyncScene);
		}
	}
#endif

#if WITH_BOX2D

	void InitBodies_Box2D() const
	{
		const int32 NumBodies = Bodies.Num();
		bool bLocalComponentAwake = bComponentAwake;
		if (UBodySetup2D* BodySetup2D = Cast<UBodySetup2D>(BodySetup))
		{
			if (b2World* BoxWorld = FPhysicsIntegration2D::FindAssociatedWorld(PrimitiveComp->GetWorld()))
			{
				for (int32 BodyIdx = 0; BodyIdx < NumBodies; ++BodyIdx)
				{
					FBodyInstance* Instance = Bodies[BodyIdx];
					const FTransform& Transform = Transforms[BodyIdx];

					const b2Vec2 Scale2D = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Instance->Scale3D);
					Instance->OwnerComponent = PrimitiveComp;

					// Create the body definition
					b2BodyDef BodyDefinition;
					if (bStatic)
					{
						BodyDefinition.type = b2_staticBody;
					}
					else
					{
						BodyDefinition.type = Instance->ShouldInstanceSimulatingPhysics() ? b2_dynamicBody : b2_kinematicBody;
					}

					if (SkelMeshComp)
					{
						bLocalComponentAwake = Instance->bStartAwake && SkelMeshComp->BodyInstance.bStartAwake;
					}

					if (Instance->bStartAwake || bLocalComponentAwake)
					{
						BodyDefinition.awake = true;
					}
					else
					{
						BodyDefinition.awake = false;
					}

					if (Instance->ShouldInstanceSimulatingPhysics())
					{
						BodyDefinition.linearVelocity = FPhysicsIntegration2D::ConvertUnrealVectorToBox(InitialLinVel);
					}

					// Create the body
					Instance->BodyInstancePtr = BoxWorld->CreateBody(&BodyDefinition);
					Instance->BodyInstancePtr->SetUserData(Instance);

					// Circles
					for (const FCircleElement2D& Circle : BodySetup2D->AggGeom2D.CircleElements)
					{
						b2CircleShape CircleShape;
						CircleShape.m_radius = Circle.Radius * Instance->Scale3D.Size() / UnrealUnitsPerMeter;
						CircleShape.m_p.x = Circle.Center.X * Scale2D.x;
						CircleShape.m_p.y = Circle.Center.Y * Scale2D.y;

						b2FixtureDef FixtureDef;
						FixtureDef.shape = &CircleShape;

						Instance->BodyInstancePtr->CreateFixture(&FixtureDef);
					}

					// Boxes
					for (const FBoxElement2D& Box : BodySetup2D->AggGeom2D.BoxElements)
					{
						const b2Vec2 HalfBoxSize(Box.Width * 0.5f * Scale2D.x, Box.Height * 0.5f * Scale2D.y);
						const b2Vec2 BoxCenter(Box.Center.X * Scale2D.x, Box.Center.Y * Scale2D.y);

						b2PolygonShape DynamicBox;
						DynamicBox.SetAsBox(HalfBoxSize.x, HalfBoxSize.y, BoxCenter, FMath::DegreesToRadians(Box.Angle));

						b2FixtureDef FixtureDef;
						FixtureDef.shape = &DynamicBox;

						Instance->BodyInstancePtr->CreateFixture(&FixtureDef);
					}

					// Convex hulls
					for (const FConvexElement2D& Convex : BodySetup2D->AggGeom2D.ConvexElements)
					{
						const int32 NumVerts = Convex.VertexData.Num();

						if (NumVerts <= b2_maxPolygonVertices)
						{
							TArray<b2Vec2, TInlineAllocator<b2_maxPolygonVertices>> Verts;

							for (int32 VertexIndex = 0; VertexIndex < Convex.VertexData.Num(); ++VertexIndex)
							{
								const FVector2D SourceVert = Convex.VertexData[VertexIndex];
								new (Verts)b2Vec2(SourceVert.X * Scale2D.x, SourceVert.Y * Scale2D.y);
							}

							b2PolygonShape ConvexPoly;
							ConvexPoly.Set(Verts.GetData(), Verts.Num());

							b2FixtureDef FixtureDef;
							FixtureDef.shape = &ConvexPoly;

							Instance->BodyInstancePtr->CreateFixture(&FixtureDef);
						}
						else
						{
							UE_LOG(LogPhysics, Warning, TEXT("Too many vertices in a 2D convex body")); //@TODO: Create a better error message that indicates the asset
						}
					}

					// Make sure it contained at least one shape
					if (Instance->BodyInstancePtr->GetFixtureList() == nullptr)
					{
						if (DebugName.Len())
						{
							UE_LOG(LogPhysics, Log, TEXT("InitBody: failed - no shapes: %s"), *DebugName);
						}

						Instance->BodyInstancePtr->SetUserData(nullptr);
						BoxWorld->DestroyBody(Instance->BodyInstancePtr);
						Instance->BodyInstancePtr = nullptr;

						//clear Owner and Setup info as well to properly clean up the BodyInstance.
						Instance->OwnerComponent = NULL;
						Instance->BodySetup = NULL;

						return;
					}
					else
					{
						// Position the body
						Instance->SetBodyTransform(Transform, /*bTeleport=*/ true);
					}

					// Apply correct physical materials to shape we created.
					Instance->UpdatePhysicalMaterials();

					// Set the filter data on the shapes (call this after setting BodyData because it uses that pointer)
					Instance->UpdatePhysicsFilterData();

					if (!bStatic)
					{
						// Compute mass (call this after setting BodyData because it uses that pointer)
						Instance->UpdateMassProperties();

						// Update damping
						Instance->UpdateDampingProperties();

						Instance->SetMaxAngularVelocity(Instance->MaxAngularVelocity, false);

						Instance->SetMaxDepenetrationVelocity(Instance->bOverrideMaxDepenetrationVelocity ? Instance->MaxDepenetrationVelocity : UPhysicsSettings::Get()->MaxDepenetrationVelocity);

						//@TODO: BOX2D: Determine if sleep threshold and solver settings can be configured per-body or not
#if 0
						// Set the parameters for determining when to put the object to sleep.
						float SleepEnergyThresh = PNewDynamic->getSleepThreshold();
						if (SleepFamily == ESleepFamily::Sensitive)
						{
							SleepEnergyThresh /= 20.f;
						}
						PNewDynamic->setSleepThreshold(SleepEnergyThresh);
						// set solver iteration count 
						int32 PositionIterCount = FMath::Clamp(PositionSolverIterationCount, 1, 255);
						int32 VelocityIterCount = FMath::Clamp(VelocitySolverIterationCount, 1, 255);
						PNewDynamic->setSolverIterationCounts(PositionIterCount, VelocityIterCount);
#endif
					}
				}
			}
			return;
		}
	}

#endif

};

void FBodyInstance::InitBody(class UBodySetup* Setup, const FTransform& Transform, class UPrimitiveComponent* PrimComp, class FPhysScene* InRBScene, PhysXAggregateType InAggregate /*= NULL*/)
{
	SCOPE_CYCLE_COUNTER(STAT_InitBody);
	check(Setup);
	check(InRBScene);
	
	static TArray<FBodyInstance*> Bodies;
	static TArray<FTransform> Transforms;

	if(Bodies.Num() == 1 && Transforms.Num() == 1)
	{
		Bodies[0] = this;
		Transforms[0] = Transform;

	}
	else
	{
		Bodies.Add(this);
		Transforms.Add(Transform);
	}
	
	check(Bodies.Num() == Transforms.Num() == 1);

	FInitBodiesHelper<false> InitBodiesHelper(Bodies, Transforms, Setup, PrimComp, InRBScene, InAggregate);
	InitBodiesHelper.InitBodies();
}

TSharedPtr<TArray<ANSICHAR>> GetDebugDebugName(const UPrimitiveComponent* PrimitiveComp, const UBodySetup* BodySetup, FString& DebugName)
{
	// Setup names
	// Make the debug name for this geometry...
	DebugName = (TEXT(""));
	TSharedPtr<TArray<ANSICHAR>> PhysXName;

#if (WITH_EDITORONLY_DATA || UE_BUILD_DEBUG || LOOKING_FOR_PERF_ISSUES) && !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && !NO_LOGGING
	if (PrimitiveComp)
	{
		DebugName += FString::Printf(TEXT("Component: %s "), *PrimitiveComp->GetReadableName());
	}

	if (BodySetup->BoneName != NAME_None)
	{
		DebugName += FString::Printf(TEXT("Bone: %s "), *BodySetup->BoneName.ToString());
	}

	// Convert to char* for PhysX
	PhysXName = MakeShareable(new TArray<ANSICHAR>(StringToArray<ANSICHAR>(*DebugName, DebugName.Len() + 1)));
#endif

	return PhysXName;
}

const USkeletalMeshComponent* GetSkeletalMeshComponentAndProperties(const UPrimitiveComponent* PrimitiveComp, const UBodySetup* BodySetup, float& InstanceBlendWeight, bool& bInstanceSimulatePhysics, bool& bComponentAwake)
{
	const USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(PrimitiveComp);
	if (SkelMeshComp)
	{
		if ((BodySetup->PhysicsType == PhysType_Simulated) || (BodySetup->PhysicsType == PhysType_Default))
		{
			bool bEnableSim = (SkelMeshComp && IsRunningDedicatedServer()) ? SkelMeshComp->bEnablePhysicsOnDedicatedServer : true;
			bEnableSim &= ((BodySetup->PhysicsType == PhysType_Simulated) || (SkelMeshComp->BodyInstance.bSimulatePhysics));	//if unfixed enable. If default look at parent

			if (bEnableSim)
			{
				// set simulate to true if using physics
				bInstanceSimulatePhysics = true;
				if (BodySetup->PhysicsType == PhysType_Simulated)
				{
					InstanceBlendWeight = 1.f;
				}
			}
			else
			{
				bInstanceSimulatePhysics = false;
				if (BodySetup->PhysicsType == PhysType_Simulated)
				{
					InstanceBlendWeight = 0.f;
				}
			}
		}

		bComponentAwake = SkelMeshComp->BodyInstance.bStartAwake;
	}

	return SkelMeshComp;
}

FVector GetInitialLinearVelocity(const AActor* OwningActor, bool& bComponentAwake)
{
	FVector InitialLinVel(EForceInit::ForceInitToZero);
	if (OwningActor)
	{
		InitialLinVel = OwningActor->GetVelocity();

		if (InitialLinVel.Size() > KINDA_SMALL_NUMBER)
		{
			bComponentAwake = true;
		}
	}

	return InitialLinVel;
}


#endif // UE_WITH_PHYSICS

#if WITH_PHYSX
TArray<int32> FBodyInstance::AddCollisionNotifyInfo(const FBodyInstance* Body0, const FBodyInstance* Body1, const physx::PxContactPair * Pairs, uint32 NumPairs, TArray<FCollisionNotifyInfo> & PendingNotifyInfos)
{
	TArray<int32> PairNotifyMapping;
	PairNotifyMapping.Empty(NumPairs);

	TMap<const FBodyInstance *, TMap<const FBodyInstance *, int32> > BodyPairNotifyMap;
	for (uint32 PairIdx = 0; PairIdx < NumPairs; ++PairIdx)
	{
		const PxContactPair* Pair = Pairs + PairIdx;
		// Get the two shapes that are involved in the collision
		const PxShape* Shape0 = Pair->shapes[0];
		check(Shape0);
		const PxShape* Shape1 = Pair->shapes[1];
		check(Shape1);

		PairNotifyMapping.Add(-1);	//start as -1 because we can have collisions that we don't want to actually record collision

		const FBodyInstance* SubBody0 = FPhysxUserData::Get<FBodyInstance>(Shape0->userData);
		const FBodyInstance* SubBody1 = FPhysxUserData::Get<FBodyInstance>(Shape1->userData);

		if (SubBody0 == NULL) { SubBody0 = Body0; }
		if (SubBody1 == NULL) { SubBody1 = Body1; }
		
		if (SubBody0->bNotifyRigidBodyCollision || SubBody1->bNotifyRigidBodyCollision)
		{
			TMap<const FBodyInstance *, int32> & SubBodyNotifyMap = BodyPairNotifyMap.FindOrAdd(SubBody0);
			int32* NotifyInfoIndex = SubBodyNotifyMap.Find(SubBody1);

			if (NotifyInfoIndex == NULL)
			{
				FCollisionNotifyInfo * NotifyInfo = new (PendingNotifyInfos) FCollisionNotifyInfo;
				NotifyInfo->bCallEvent0 = (SubBody0->bNotifyRigidBodyCollision);
				NotifyInfo->Info0.SetFrom(SubBody0);
				NotifyInfo->bCallEvent1 = (SubBody1->bNotifyRigidBodyCollision);
				NotifyInfo->Info1.SetFrom(SubBody1);

				NotifyInfoIndex = &SubBodyNotifyMap.Add(SubBody0, PendingNotifyInfos.Num() - 1);
			}

			PairNotifyMapping[PairIdx] = *NotifyInfoIndex;
		}
	}

	return PairNotifyMapping;
}

//helper function for TermBody to avoid code duplication between scenes
void TermBodyHelper(int32& SceneIndex, PxRigidActor*& PRigidActor, FBodyInstance* BodyInstance)
{
	if (SceneIndex)
	{
		PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);

		if (PScene)
		{
			// Enable scene lock
			SCOPED_SCENE_WRITE_LOCK(PScene);

			if (PRigidActor)
			{


				// Let FPhysScene know
				FPhysScene* PhysScene = FPhysxUserData::Get<FPhysScene>(PScene->userData);
				if (PhysScene)
				{
					PhysScene->TermBody_AssumesLocked(BodyInstance);
				}

				PRigidActor->release();
				PRigidActor = NULL;	//we must do this within the lock because we use it in the sub-stepping thread to determine that RigidActor is still valid
			}
		}

		SceneIndex = 0;
	}
#if WITH_APEX
	else
	{
		if (PRigidActor)
		{
			//When DestructibleMesh is used we create fake BodyInstances. In this case the RigidActor is set, but InitBody is never called.
			//The RigidActor is released by the destructible component, but it's still up to us to NULL out the pointer
			checkSlow(!PRigidActor->userData || FPhysxUserData::Get<FDestructibleChunkInfo>(PRigidActor->userData));	//Make sure we are really a destructible. Note you CAN get a case when userData is NULL, for example when trying to attach a destructible to another.
			PRigidActor = NULL;
		}
	}
#endif

	checkSlow(PRigidActor == NULL);
	checkSlow(SceneIndex == 0);
}

#endif

DEFINE_STAT(STAT_TermBody);
/**
 *	Clean up the physics engine info for this instance.
 */
void FBodyInstance::TermBody()
{
	SCOPE_CYCLE_COUNTER(STAT_TermBody);
#if WITH_BOX2D
	if (BodyInstancePtr != NULL)
	{
		if (UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get())
		{
			if (b2World* World = FPhysicsIntegration2D::FindAssociatedWorld(OwnerComponentInst->GetWorld()))
			{
				World->DestroyBody(BodyInstancePtr);
			}
			else
			{
				BodyInstancePtr->SetUserData(nullptr);
			}
		}
		BodyInstancePtr = nullptr;
	}
#endif

#if WITH_PHYSX
	// Release sync actor
	TermBodyHelper(SceneIndexSync, RigidActorSync, this);
	// Release async actor
	TermBodyHelper(SceneIndexAsync, RigidActorAsync, this);

	// releasing BodyAggregate, it shouldn't contain RigidActor now, because it's released above
	if(BodyAggregate)
	{
		check(!BodyAggregate->getNbActors());
		BodyAggregate->release();
		BodyAggregate = NULL;
	}
#endif

	// @TODO UE4: Release spring body here

	BodySetup = NULL;
	OwnerComponent = NULL;

	if (DOFConstraint)
	{
		DOFConstraint->TermConstraint();
		FConstraintInstance::Free(DOFConstraint);
			DOFConstraint = NULL;
	}
	

	
}

bool FBodyInstance::Weld(FBodyInstance* TheirBody, const FTransform& TheirTM)
{
	check(TheirBody);
	if (TheirBody->BodySetup.IsValid() == false)	//attach actor can be called before body has been initialized. In this case just return false
	{
		return false;
	}

	//@TODO: BOX2D: Implement Weld

#if WITH_PHYSX
	TArray<PxShape *> PNewShapes;

	FTransform MyTM = GetUnrealWorldTransform();
	MyTM.SetScale3D(Scale3D);	//physx doesn't store 3d so set it here

	FTransform RelativeTM = TheirTM.GetRelativeTransform(MyTM);

	PxScene* PSyncScene = RigidActorSync ? RigidActorSync->getScene() : NULL;
	PxScene* PAsyncScene = RigidActorAsync ? RigidActorAsync->getScene() : NULL;

	ExecuteOnPhysicsReadWrite([&]
	{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePhysMats);

	UPhysicalMaterial* SimplePhysMat = GetSimplePhysicalMaterial();
	TArray<UPhysicalMaterial*> ComplexPhysMats = GetComplexPhysicalMaterials();
	PxMaterial* PSimpleMat = SimplePhysMat->GetPhysXMaterial();

	FShapeData ShapeData;
		GetFilterData_AssumesLocked(ShapeData);
		GetShapeFlags_AssumesLocked(ShapeData, CollisionEnabled, BodySetup->CollisionTraceFlag == CTF_UseComplexAsSimple);

	//child body gets placed into the same scenes as parent body
	if (PxRigidActor* MyBody = RigidActorSync)
	{
			TheirBody->BodySetup->AddShapesToRigidActor_AssumesLocked(this, MyBody, PST_Sync, Scale3D, PSimpleMat, ComplexPhysMats, ShapeData, RelativeTM, &PNewShapes);
	}

	if (PxRigidActor* MyBody = RigidActorAsync)
	{
			TheirBody->BodySetup->AddShapesToRigidActor_AssumesLocked(this, MyBody, PST_Sync, Scale3D, PSimpleMat, ComplexPhysMats, ShapeData, RelativeTM, &PNewShapes);
	}


	for (int32 ShapeIdx = 0; ShapeIdx < PNewShapes.Num(); ++ShapeIdx)
	{
		PxShape* PShape = PNewShapes[ShapeIdx];
		//FBodyInstance *& BI = ShapeToBodyMap.FindOrAdd(PShape);
		//BI = TheirBody;

		PShape->userData = &TheirBody->PhysxUserData;
	}

	PostShapeChange();

	//remove their body from scenes
	TermBodyHelper(TheirBody->SceneIndexSync, TheirBody->RigidActorSync, TheirBody);
	TermBodyHelper(TheirBody->SceneIndexAsync, TheirBody->RigidActorAsync, TheirBody);
	});
	

	return true;
#endif

	return false;
}

void FBodyInstance::UnWeld(FBodyInstance* TheirBI)
{
	//@TODO: BOX2D: Implement Weld

#if WITH_PHYSX

	bool bShapesChanged = false;
	bool bNeedsNotification = false;

	ExecuteOnPhysicsReadWrite([&]
	{
		TArray<physx::PxShape *> PShapes;
		const int32 NumSyncShapes = GetAllShapes_AssumesLocked(PShapes);

	for (int32 ShapeIdx = 0; ShapeIdx < NumSyncShapes; ++ShapeIdx)
	{
		PxShape* PShape = PShapes[ShapeIdx];
		if (FBodyInstance* BI = FPhysxUserData::Get<FBodyInstance>(PShape->userData))
		{
			bNeedsNotification |= BI->bNotifyRigidBodyCollision;

			if (TheirBI == BI)
			{
				PShape->userData = NULL;
				RigidActorSync->detachShape(*PShape);
				bShapesChanged = true;
			}
		}
	}

	for (int32 ShapeIdx = NumSyncShapes; ShapeIdx <PShapes.Num(); ++ShapeIdx)
	{
		PxShape* PShape = PShapes[ShapeIdx];
		if (FBodyInstance* BI = FPhysxUserData::Get<FBodyInstance>(PShape->userData))
		{
			bNeedsNotification |= BI->bNotifyRigidBodyCollision;

			if (TheirBI == BI)
			{
				PShape->userData = NULL;
				RigidActorAsync->detachShape(*PShape);
				bShapesChanged = true;
			}
		}
	}
	});

	if (bShapesChanged)
	{
		PostShapeChange();
	}
#endif
}

void FBodyInstance::PostShapeChange()
{
	// Apply correct physical materials to shape we created.
	UpdatePhysicalMaterials();

	// Set the filter data on the shapes (call this after setting BodyData because it uses that pointer)
	UpdatePhysicsFilterData(true);

	UpdateMassProperties();
	// Update damping
	UpdateDampingProperties();
}

float AdjustForSmallThreshold(float NewVal, float OldVal)
{
	float Threshold = 0.1f;
	float Delta = NewVal - OldVal;
	if (Delta < 0 && FMath::Abs(NewVal) < Threshold)	//getting smaller and passed threshold so flip sign
	{
		return -Threshold;
	}
	else if (Delta > 0 && FMath::Abs(NewVal) < Threshold)	//getting bigger and passed small threshold so flip sign
	{
		return Threshold;
	}

	return NewVal;
}

//Non uniform scaling depends on the primitive that has the least non uniform scaling capability. So for example, a capsule's x and y axes scale are locked.
//So if a capsule exists in this body we must use locked x and y scaling for all shapes.
namespace EScaleMode
{
	enum Type
	{
		Free,
		LockedXY,
		LockedXYZ
	};
}

//computes the relative scaling vectors based on scale mode used
void ComputeScalingVectors(EScaleMode::Type ScaleMode, const FVector& NewScale3D, const FVector& OldScale3D, FVector& RelativeScale3D, FVector& RelativeScale3DAbs, FVector& OutScale3D)
{
	FVector NewScale3DAbs = NewScale3D.GetAbs();
	FVector OldScale3DAbs = OldScale3D.GetAbs();

	switch (ScaleMode)
	{
	case EScaleMode::Free:
	{
		OutScale3D = NewScale3D;
		RelativeScale3DAbs.X = NewScale3DAbs.X / OldScale3DAbs.X;
		RelativeScale3DAbs.Y = NewScale3DAbs.Y / OldScale3DAbs.Y;
		RelativeScale3DAbs.Z = NewScale3DAbs.Z / OldScale3DAbs.Z;

		RelativeScale3D.X = NewScale3D.X / OldScale3D.X;
		RelativeScale3D.Y = NewScale3D.Y / OldScale3D.Y;
		RelativeScale3D.Z = NewScale3D.Z / OldScale3D.Z;
		break;
	}
	case EScaleMode::LockedXY:
	{
		float XYScaleAbs = FMath::Max(NewScale3DAbs.X, NewScale3DAbs.Y);
		float XYScale = FMath::Max(NewScale3D.X, NewScale3D.Y) < 0.f ? -XYScaleAbs : XYScaleAbs;	//if both xy are negative we should make the xy scale negative

		float OldXYScaleAbs = FMath::Max(OldScale3DAbs.X, OldScale3DAbs.Y);
		float OldScaleXY = FMath::Max(OldScale3D.X, OldScale3D.Y) < 0.f ? -OldXYScaleAbs : OldXYScaleAbs;

		float RelativeScaleAbs = XYScaleAbs / OldXYScaleAbs;
		float RelativeScale = XYScale / OldScaleXY;

		RelativeScale3DAbs.X = RelativeScale3DAbs.Y = RelativeScaleAbs;
		RelativeScale3DAbs.Z = NewScale3DAbs.Z / OldScale3DAbs.Z;

		RelativeScale3D.X = RelativeScale3D.Y = RelativeScale;
		RelativeScale3D.Z = NewScale3D.Z / OldScale3D.Z;
		
		OutScale3D = NewScale3D;
		OutScale3D.X = OutScale3D.Y = XYScale;

		break;
	}
	case EScaleMode::LockedXYZ:
	{
		float UniformScaleAbs = NewScale3DAbs.GetMin();	//uniform scale uses the smallest magnitude
		float UniformScale = FMath::Max3(NewScale3D.X, NewScale3D.Y, NewScale3D.Z) < 0.f ? -UniformScaleAbs : UniformScaleAbs;	//if all three values are negative we should make uniform scale negative

		float OldUniformScaleAbs = OldScale3D.GetAbs().GetMin();
		float OldUniformScale = FMath::Max3(OldScale3D.X, OldScale3D.Y, OldScale3D.Z) < 0.f ? -OldUniformScaleAbs : OldUniformScaleAbs;

		float RelativeScale = UniformScale / OldUniformScale;
		float RelativeScaleAbs = UniformScaleAbs / OldUniformScaleAbs;

		RelativeScale3DAbs = FVector(RelativeScaleAbs);
		RelativeScale3D = FVector(RelativeScale);

		OutScale3D = FVector(UniformScale);
		break;
	}
	default:
	{
		check(false);	//invalid scale mode
	}
	}
}

bool FBodyInstance::UpdateBodyScale(const FVector& InScale3D)
{
	FVector InScale3DAdjusted = InScale3D;

	if (!IsValidBodyInstance())
	{
		//UE_LOG(LogPhysics, Log, TEXT("Body hasn't been initialized. Call InitBody to initialize."));
		return false;
	}

	// if same, return
	if (Scale3D.Equals(InScale3D))
	{
		return false;
	}

	bool bSuccess = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ensure ( !Scale3D.ContainsNaN() && !InScale3D.ContainsNaN() );
#endif
	FVector OldScale3D = Scale3D;
	
	//we never want to hit a scale of 0
	//But we still want to be able to cross from positive to negative
	InScale3DAdjusted.X = AdjustForSmallThreshold(InScale3D.X, OldScale3D.X);
	InScale3DAdjusted.Y = AdjustForSmallThreshold(InScale3D.Y, OldScale3D.Y);
	InScale3DAdjusted.Z = AdjustForSmallThreshold(InScale3D.Z, OldScale3D.Z);
	
	//Make sure OldScale3D is not too small or NaNs can happen
	OldScale3D.X = OldScale3D.X < 0.1f ? 0.1f : OldScale3D.X;
	OldScale3D.Y = OldScale3D.Y < 0.1f ? 0.1f : OldScale3D.Y;
	OldScale3D.Z = OldScale3D.Z < 0.1f ? 0.1f : OldScale3D.Z;

	// Determine the scaling mode
	EScaleMode::Type ScaleMode = EScaleMode::Free;
	FVector UpdatedScale3D;
#if WITH_PHYSX
	//Get all shapes
	ExecuteOnPhysicsReadWrite([&]
	{
		TArray<PxShape *> PShapes;
		GetAllShapes_AssumesLocked(PShapes);

		for (int32 ShapeIdx = 0; ShapeIdx < PShapes.Num(); ++ShapeIdx)
		{
			PxShape* PShape = PShapes[ShapeIdx];
			PxGeometryType::Enum GeomType = PShape->getGeometryType();

			if (GeomType == PxGeometryType::eSPHERE)
			{
				ScaleMode = EScaleMode::LockedXYZ;	//sphere is most restrictive so we can stop
				break;
			}
			else if (GeomType == PxGeometryType::eCAPSULE)
			{
				ScaleMode = EScaleMode::LockedXY;
			}
		}
#endif
#if WITH_BOX2D
		if (BodyInstancePtr)
		{
			//@TODO: BOX2D: UpdateBodyScale is not implemented yet
		}
#endif

		FVector RelativeScale3D;
		FVector RelativeScale3DAbs;
		ComputeScalingVectors(ScaleMode, InScale3DAdjusted, OldScale3D, RelativeScale3D, RelativeScale3DAbs, UpdatedScale3D);

		// Apply scaling
#if WITH_PHYSX
		//we need to allocate all of these here because PhysX insists on using the stack. This is wasteful, but reduces a lot of code duplication
		PxSphereGeometry PSphereGeom;
		PxBoxGeometry PBoxGeom;
		PxCapsuleGeometry PCapsuleGeom;
		PxConvexMeshGeometry PConvexGeom;
		PxTriangleMeshGeometry PTriMeshGeom;

		for (int32 ShapeIdx = 0; ShapeIdx < PShapes.Num(); ShapeIdx++)
		{
			bool bInvalid = false;	//we only mark invalid if actually found geom and it's invalid scale
			PxGeometry* UpdatedGeometry = NULL;
			PxShape* PShape = PShapes[ShapeIdx];

			PxTransform PLocalPose = PShape->getLocalPose();
			PLocalPose.q.normalize();
			PxGeometryType::Enum GeomType = PShape->getGeometryType();

			switch (GeomType)
			{
				case PxGeometryType::eSPHERE:
				{
					ensure(ScaleMode == EScaleMode::LockedXYZ);

					PShape->getSphereGeometry(PSphereGeom);

					PSphereGeom.radius *= RelativeScale3DAbs.X;
					PLocalPose.p *= RelativeScale3D.X;

					if (PSphereGeom.isValid())
					{
						UpdatedGeometry = &PSphereGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}
					break;
				}
				case PxGeometryType::eBOX:
				{
					PShape->getBoxGeometry(PBoxGeom);

					PBoxGeom.halfExtents.x *= RelativeScale3DAbs.X;
					PBoxGeom.halfExtents.y *= RelativeScale3DAbs.Y;
					PBoxGeom.halfExtents.z *= RelativeScale3DAbs.Z;
					PLocalPose.p.x *= RelativeScale3D.X;
					PLocalPose.p.y *= RelativeScale3D.Y;
					PLocalPose.p.z *= RelativeScale3D.Z;

					if (PBoxGeom.isValid())
					{
						UpdatedGeometry = &PBoxGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}
					break;
				}
				case PxGeometryType::eCAPSULE:
				{
					ensure(ScaleMode == EScaleMode::LockedXY || ScaleMode == EScaleMode::LockedXYZ);

					PShape->getCapsuleGeometry(PCapsuleGeom);

					PCapsuleGeom.halfHeight *= RelativeScale3DAbs.Z;
					PCapsuleGeom.radius *= RelativeScale3DAbs.X;

					PLocalPose.p.x *= RelativeScale3D.X;
					PLocalPose.p.y *= RelativeScale3D.Y;
					PLocalPose.p.z *= RelativeScale3D.Z;

					if (PCapsuleGeom.isValid())
					{
						UpdatedGeometry = &PCapsuleGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}

					break;
				}
				case PxGeometryType::eCONVEXMESH:
				{
					PShape->getConvexMeshGeometry(PConvexGeom);

					// find which convex elems it is
					// it would be nice to know if the order of PShapes array index is in the order of createShape
					// Create convex shapes
					if (BodySetup.IsValid())
					{
						for (int32 i = 0; i < BodySetup->AggGeom.ConvexElems.Num(); i++)
						{
							FKConvexElem* ConvexElem = &(BodySetup->AggGeom.ConvexElems[i]);

							// found it
							if (ConvexElem->ConvexMesh == PConvexGeom.convexMesh)
							{
								// Please note that this one we don't inverse old scale, but just set new one (but we still follow scale mode restriction)
								FVector NewScale3D = RelativeScale3D * OldScale3D;
								FVector Scale3DAbs(FMath::Abs(NewScale3D.X), FMath::Abs(NewScale3D.Y), FMath::Abs(NewScale3D.Z)); // magnitude of scale (sign removed)

								PxTransform PNewLocalPose;
								bool bUseNegX = CalcMeshNegScaleCompensation(NewScale3D, PNewLocalPose);

								PxTransform PElementTransform = U2PTransform(ConvexElem->GetTransform());
								PNewLocalPose.q *= PElementTransform.q;
								PNewLocalPose.p += PElementTransform.p;

								PConvexGeom.convexMesh = bUseNegX ? ConvexElem->ConvexMeshNegX : ConvexElem->ConvexMesh;
								PConvexGeom.scale.scale = U2PVector(Scale3DAbs);

								if (PConvexGeom.isValid())
								{
									UpdatedGeometry = &PConvexGeom;
									bSuccess = true;
								}
								else
								{
									bInvalid = true;
								}
								break;
							}
						}
					}

					break;
				}
				case PxGeometryType::eTRIANGLEMESH:
				{
					PShape->getTriangleMeshGeometry(PTriMeshGeom);

					// Create tri-mesh shape
					if (BodySetup.IsValid() && (BodySetup->TriMesh != NULL || BodySetup->TriMeshNegX != NULL))
					{
						// Please note that this one we don't inverse old scale, but just set new one (but still adjust for scale mode)
						FVector NewScale3D = RelativeScale3D * OldScale3D;
						FVector Scale3DAbs(FMath::Abs(NewScale3D.X), FMath::Abs(NewScale3D.Y), FMath::Abs(NewScale3D.Z)); // magnitude of scale (sign removed)

						PxTransform PNewLocalPose;
						bool bUseNegX = CalcMeshNegScaleCompensation(NewScale3D, PNewLocalPose);

						// Only case where TriMeshNegX should be null is BSP, which should not require negX version
						if (bUseNegX && BodySetup->TriMeshNegX == NULL)
						{
							UE_LOG(LogPhysics, Warning, TEXT("FBodyInstance::UpdateBodyScale: Want to use NegX but it doesn't exist! %s"), *BodySetup->GetPathName());
						}

						PxTriangleMesh* UseTriMesh = bUseNegX ? BodySetup->TriMeshNegX : BodySetup->TriMesh;
						if (UseTriMesh != NULL)
						{
							PTriMeshGeom.triangleMesh = bUseNegX ? BodySetup->TriMeshNegX : BodySetup->TriMesh;
							PTriMeshGeom.scale.scale = U2PVector(Scale3DAbs);

							if (PTriMeshGeom.isValid())
							{
								UpdatedGeometry = &PTriMeshGeom;
								bSuccess = true;

							}
							else
							{
								bInvalid = true;
							}
						}
					}
					break;
				}
				case PxGeometryType::eHEIGHTFIELD:
				{
					// HeightField is only used by Landscape, which does different code path from other primitives
					break;
				}
				default:
				{
						   UE_LOG(LogPhysics, Error, TEXT("Unknown geom type."));
				}
			}// end switch

			if (UpdatedGeometry)
			{
				ExecuteOnPxShapeWrite(this, PShape, [&](PxShape* PGivenShape)
				{
					PGivenShape->setLocalPose(PLocalPose);
					PGivenShape->setGeometry(*UpdatedGeometry);
				});
			}
			else if (bInvalid)
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("PhysicsInvalidScale", "Scale ''{0}'' is not valid on object '{1}'."), FText::FromString(InScale3DAdjusted.ToString()), FText::FromString(GetBodyDebugName())));
			}
		}
	});
	
#endif

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		//@TODO: BOX2D: UpdateBodyScale is not implemented yet
	}
#endif

	// if success, overwrite old Scale3D, otherwise, just don't do it. It will have invalid scale next time
	if (bSuccess)
	{
		Scale3D = UpdatedScale3D;

		// update mass if required
		if (bUpdateMassWhenScaleChanges)
		{
			UpdateMassProperties();
		}
	}

	return bSuccess;
}

void FBodyInstance::UpdateInstanceSimulatePhysics()
{
	// In skeletal case, we need both our bone and skelcomponent flag to be true.
	// This might be 'and'ing us with ourself, but thats fine.
	const bool bUseSimulate = IsInstanceSimulatingPhysics();
	bool bInitialized = false;
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		bInitialized = true;
		// If we want it fixed, and it is currently not kinematic
		bool bNewKinematic = (bUseSimulate == false);
		{
			PRigidDynamic->setRigidDynamicFlag(PxRigidDynamicFlag::eKINEMATIC, bNewKinematic);
			PRigidDynamic->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, !bNewKinematic && bUseCCD);

			//if wake when level starts is true, calling this function automatically wakes body up
			if (bSimulatePhysics && bStartAwake)
			{
				PRigidDynamic->wakeUp();
			}
		}
		
	});
#endif

#if WITH_BOX2D
	if (BodyInstancePtr != NULL)
	{
		bInitialized = true;
		BodyInstancePtr->SetType(bUseSimulate ? b2_dynamicBody : b2_kinematicBody);
	}
#endif

	//In the original physx only implementation this was wrapped in a PRigidDynamic != NULL check.
	//We use bInitialized to check rigid actor has been created in either engine because if we haven't even initialized yet, we don't want to undo our settings
	if (bInitialized)
	{
		if (bUseSimulate)
		{
			PhysicsBlendWeight = 1.f;
		}
		else
		{
			PhysicsBlendWeight = 0.f;
		}

		bSimulatePhysics = bUseSimulate;
	}
}

bool FBodyInstance::IsNonKinematic() const
{
	return bSimulatePhysics;
}

bool FBodyInstance::IsDynamic() const
{
	bool bIsDynamic = false;
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadOnly(this, [&](const PxRigidDynamic* PRigidDynamic)
	{
		bIsDynamic = true;
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr != nullptr)
	{
		bIsDynamic = BodyInstancePtr->GetType() != b2_staticBody;
	}
#endif // WITH_BOX2D

	return bIsDynamic;
}

void FBodyInstance::SetInstanceSimulatePhysics(bool bSimulate, bool bMaintainPhysicsBlending)
{
	if (bSimulate)
	{
		UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (OwnerComponentInst)
		{
			if (!IsValidBodyInstance())
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimPhysNoBody", "Trying to simulate physics on ''{0}'' but no physics body."),
					FText::FromString(GetPathNameSafe(OwnerComponentInst))));
			}
			else if (!IsDynamic())
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimPhysStatic", "Trying to simulate physics on ''{0}'' but it is static."),
					FText::FromString(GetPathNameSafe(OwnerComponentInst))));
			}
		}
#endif

		// If we are enabling simulation, and we are the root body of our component, we detach the component 
		if (OwnerComponentInst && OwnerComponentInst->IsRegistered() && OwnerComponentInst->GetBodyInstance() == this)
		{
			if (OwnerComponentInst->AttachParent)
			{
				OwnerComponentInst->DetachFromParent(true);
			}
			
			if (bSimulatePhysics == false)	//if we're switching from kinematic to simulated
			{
				//we must make sure to update children that are welded
				TArray<FBodyInstance*> ChildrenBodies;
				TArray<FName> ChildrenLabels;
				OwnerComponentInst->GetWeldedBodies(ChildrenBodies, ChildrenLabels);

				for (int32 ChildIdx = 0; ChildIdx < ChildrenBodies.Num(); ++ChildIdx)
				{
					FBodyInstance* ChildBI = ChildrenBodies[ChildIdx];
					if (ChildBI != this)
					{
						Weld(ChildBI, ChildBI->OwnerComponent->GetSocketTransform(ChildrenLabels[ChildIdx]));
						ChildBI->WeldParent = this;
					}
				}
			}
		}
	}

	bSimulatePhysics = bSimulate;
	if ( !bMaintainPhysicsBlending )
	{
		if (bSimulatePhysics)
		{
			PhysicsBlendWeight = 1.f;
		}
		else
		{
			PhysicsBlendWeight = 0.f;
		}
	}

	UpdateInstanceSimulatePhysics();
}

bool FBodyInstance::IsInstanceSimulatingPhysics()
{
	// if I'm simulating or owner is simulating
	return ShouldInstanceSimulatingPhysics() && IsValidBodyInstance();
}

bool FBodyInstance::ShouldInstanceSimulatingPhysics()
{
	return bSimulatePhysics;
}

bool FBodyInstance::IsValidBodyInstance() const
{
#if WITH_PHYSX
	if (PxRigidActor* PActor = GetPxRigidActor_AssumesLocked())
	{
		return true;
	}
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr != nullptr)
	{
		return true;
	}
#endif

	return false;
}

template <bool NeedsLock>
FTransform GetUnrealWorldTransformImp(const FBodyInstance* BodyInstance)
{
	FTransform WorldTM = FTransform::Identity;
#if WITH_PHYSX
	FPhysXSupport<NeedsLock>::ExecuteOnPxRigidActorReadOnly(BodyInstance, [&](const PxRigidActor* PActor)
	{
		PxTransform PTM = PActor->getGlobalPose();
		WorldTM = P2UTransform(PTM);
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstance->BodyInstancePtr != NULL)
	{
		const b2Vec2 Pos2D = BodyInstance->BodyInstancePtr->GetPosition();
		const float RotationInRadians = BodyInstance->BodyInstancePtr->GetAngle();

		const FVector Translation3D(FPhysicsIntegration2D::ConvertBoxVectorToUnreal(Pos2D));
		const FRotator Rotation3D(FMath::RadiansToDegrees(RotationInRadians), 0.0f, 0.0f); //@TODO: BOX2D: Should be moved to FPhysicsIntegration2D

		WorldTM = FTransform(Rotation3D, Translation3D, BodyInstance->Scale3D);
	}
#endif

	return WorldTM;
}

FTransform FBodyInstance::GetUnrealWorldTransform() const
{
	return GetUnrealWorldTransformImp<true>(this);
}


FTransform FBodyInstance::GetUnrealWorldTransform_AssumesLocked() const
{
	return GetUnrealWorldTransformImp<false>(this);
}

void FBodyInstance::SetBodyTransform(const FTransform& NewTransform, bool bTeleport)
{
	SCOPE_CYCLE_COUNTER(STAT_SetBodyTransform);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	extern bool GShouldLogOutAFrameOfSetBodyTransform;
	if (GShouldLogOutAFrameOfSetBodyTransform == true)
	{
		UE_LOG(LogPhysics, Log, TEXT("SetBodyTransform: %s"), *GetBodyDebugName());
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// Catch NaNs and elegantly bail out.

	if( !ensureMsgf(!NewTransform.ContainsNaN(), TEXT("SetBodyTransform contains NaN (%s)\n%s"), (OwnerComponent.Get() ? *OwnerComponent->GetPathName() : TEXT("NONE")), *NewTransform.ToString()) )
	{
		return;
	}

#if WITH_PHYSX
	if (PxRigidActor* RigidActor = GetPxRigidActor_AssumesLocked())
	{
		const PxTransform PNewPose = U2PTransform(NewTransform);

		if (!PNewPose.isValid())
		{
			UE_LOG(LogPhysics, Warning, TEXT("FBodyInstance::SetBodyTransform: Trying to set new transform with bad data [p=(%f,%f,%f) q=(%f,%f,%f,%f)]"), PNewPose.p.x, PNewPose.p.y, PNewPose.p.z, PNewPose.q.x, PNewPose.q.y, PNewPose.q.z, PNewPose.q.w);
			return;
		}

		ExecuteOnPhysicsReadWrite([&]
		{
			// SIMULATED & KINEMATIC
			if (PxRigidDynamic* PRigidDynamic = GetPxRigidDynamic_AssumesLocked())
			{
				// If kinematic and not teleporting, set kinematic target
				if (!IsRigidBodyNonKinematic_AssumesLocked(PRigidDynamic) && !bTeleport)
				{
					if(FPhysScene* PhysScene = GetPhysicsScene(this))
					{
						PhysScene->SetKinematicTarget_AssumesLocked(this, NewTransform, true);
					}
				}
				// Otherwise, set global pose
				else
				{
					PRigidDynamic->setGlobalPose(PNewPose);
				}
			}
			// STATIC
			else
			{
				UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
				const bool bIsGame = !GIsEditor || (OwnerComponentInst != NULL && OwnerComponentInst->GetWorld()->IsGameWorld());
				// Do NOT move static actors in-game, give a warning but let it happen
				if (bIsGame)
				{
					const FString ComponentPathName = (OwnerComponentInst != NULL) ? OwnerComponentInst->GetPathName() : TEXT("NONE");
					UE_LOG(LogPhysics, Warning, TEXT("MoveFixedBody: Trying to move component'%s' with a non-Movable Mobility."), *ComponentPathName);
				}
				// In EDITOR, go ahead and move it with no warning, we are editing the level
				RigidActor->setGlobalPose(PNewPose);
			}
		});
	}
#endif  // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr != NULL)
	{
		const FVector NewLocation = NewTransform.GetLocation();
		const b2Vec2 NewLocation2D(FPhysicsIntegration2D::ConvertUnrealVectorToBox(NewLocation));

		//@TODO: BOX2D: SetBodyTransform: What about scale?
		const FRotator NewRotation3D(NewTransform.GetRotation());
		const float NewAngle = FMath::DegreesToRadians(NewRotation3D.Pitch);

		BodyInstancePtr->SetTransform(NewLocation2D, NewAngle);
	}
#endif
}

template <bool NeedsLock>
FVector GetUnrealWorldVelocityImp(const FBodyInstance* BodyInstance)
{
	FVector LinVel(EForceInit::ForceInitToZero);

#if WITH_PHYSX
	FPhysXSupport<NeedsLock>::ExecuteOnPxRigidBodyReadOnly(BodyInstance, [&](const PxRigidBody* PRigidBody)
	{
		LinVel = P2UVector(PRigidBody->getLinearVelocity());
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstance->BodyInstancePtr != nullptr)
	{
		LinVel = FPhysicsIntegration2D::ConvertBoxVectorToUnreal(BodyInstance->BodyInstancePtr->GetLinearVelocity());
	}
#endif

	return LinVel;
}

FVector FBodyInstance::GetUnrealWorldVelocity() const
{
	return GetUnrealWorldVelocityImp<true>(this);
}

FVector FBodyInstance::GetUnrealWorldVelocity_AssumesLocked() const
{
	return GetUnrealWorldVelocityImp<false>(this);
}

template <bool NeedsLock>
FVector GetUnrealWorldAngularVelocityImp(const FBodyInstance* BodyInstance)
{
	FVector AngVel(EForceInit::ForceInitToZero);

#if WITH_PHYSX
	FPhysXSupport<NeedsLock>::ExecuteOnPxRigidBodyReadOnly(BodyInstance, [&](const PxRigidBody* PRigidBody)
	{
		AngVel = FVector::RadiansToDegrees(P2UVector(PRigidBody->getAngularVelocity()));
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstance->BodyInstancePtr != nullptr)
	{
		AngVel = FPhysicsIntegration2D::ConvertBoxAngularVelocityToUnreal(BodyInstance->BodyInstancePtr->GetAngularVelocity());
	}
#endif

	return AngVel;
}

/** Note: returns angular velocity in degrees per second. */
FVector FBodyInstance::GetUnrealWorldAngularVelocity() const
{
	return GetUnrealWorldAngularVelocityImp<true>(this);
}

/** Note: returns angular velocity in degrees per second. */
FVector FBodyInstance::GetUnrealWorldAngularVelocity_AssumesLocked() const
{
	return GetUnrealWorldAngularVelocityImp<false>(this);
}

template <bool NeedsLock>
FVector GetUnrealWorldVelocityAtPointImp(const FBodyInstance* BodyInstance, const FVector& Point)
{
	FVector LinVel(EForceInit::ForceInitToZero);

#if WITH_PHYSX
	FPhysXSupport<NeedsLock>::ExecuteOnPxRigidBodyReadOnly(BodyInstance, [&](const PxRigidBody* PRigidBody)
	{
		PxVec3 PPoint = U2PVector(Point);
		LinVel = P2UVector(PxRigidBodyExt::getVelocityAtPos(*PRigidBody, PPoint));
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstance->BodyInstancePtr != nullptr)
	{
		const b2Vec2 BoxPoint = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Point);
		LinVel = FPhysicsIntegration2D::ConvertBoxVectorToUnreal(BodyInstance->BodyInstancePtr->GetLinearVelocityFromWorldPoint(BoxPoint));
	}
#endif

	return LinVel;
}


FVector FBodyInstance::GetUnrealWorldVelocityAtPoint(const FVector& Point) const
{
	return GetUnrealWorldVelocityAtPointImp<true>(this, Point);
}


FVector FBodyInstance::GetUnrealWorldVelocityAtPoint_AssumesLocked(const FVector& Point) const
{
	return GetUnrealWorldVelocityAtPointImp<false>(this, Point);
}

FVector FBodyInstance::GetCOMPosition() const
{
	FVector WorldCOM = FVector::ZeroVector;
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadOnly(this, [&](const PxRigidBody* PRigidBody)
	{
		PxTransform PLocalCOM = PRigidBody->getCMassLocalPose();
		PxVec3 PWorldCOM = PRigidBody->getGlobalPose().transform(PLocalCOM.p);
		WorldCOM = P2UVector(PWorldCOM);
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr != nullptr)
	{
		WorldCOM = FPhysicsIntegration2D::ConvertBoxVectorToUnreal(BodyInstancePtr->GetWorldCenter());
	}
#endif

	return WorldCOM;
}

float FBodyInstance::GetBodyMass() const
{
	float Retval = 0.f;

#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadOnly(this, [&] (const PxRigidBody* PRigidBody)
	{
		Retval = PRigidBody->getMass();
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr != nullptr)
	{
		Retval = BodyInstancePtr->GetMass();
	}
#endif

	return Retval;
}


FVector FBodyInstance::GetBodyInertiaTensor() const
{
	FVector Retval = FVector::ZeroVector;

#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadOnly(this, [&] (const PxRigidBody* PRigidBody)
	{
		Retval = P2UVector(PRigidBody->getMassSpaceInertiaTensor());
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr != nullptr)
	{
		Retval = FVector(BodyInstancePtr->GetInertia(), 0.f, 0.f);
	}
#endif

	return Retval;
}

FBox FBodyInstance::GetBodyBounds() const
{
	FBox Bounds;

#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadOnly(this, [&](const PxRigidBody* PRigidBody)
	{
		PxBounds3 PBounds = PRigidBody->getWorldBounds();

		Bounds.Min = P2UVector(PBounds.minimum);
		Bounds.Max = P2UVector(PBounds.maximum);
	});
#endif // WITH_PHYSX

	//@TODO: BOX2D: Implement GetBodyBounds

	return Bounds;
}

void FBodyInstance::DrawCOMPosition(FPrimitiveDrawInterface* PDI, float COMRenderSize, const FColor& COMRenderColor)
{
	if (IsValidBodyInstance())
	{
		DrawWireStar(PDI, GetCOMPosition(), COMRenderSize, COMRenderColor, SDPG_World);
	}
}

/** Utility for copying properties from one BodyInstance to another. */
void FBodyInstance::CopyBodyInstancePropertiesFrom(const FBodyInstance* FromInst)
{
	// No copying of runtime instances (strictly defaults off BodySetup)
	check(FromInst);
	check(FromInst->OwnerComponent.Get() == NULL);
	check(FromInst->BodySetup.Get() == NULL);
#if WITH_PHYSX
	check(!FromInst->RigidActorSync);
	check(!FromInst->RigidActorAsync);
	check(!FromInst->BodyAggregate);
#endif //WITH_PHYSX
	check(FromInst->SceneIndexSync == 0);
	check(FromInst->SceneIndexAsync == 0);

	//check(!OwnerComponent);
#if WITH_PHYSX
	check(!RigidActorSync);
	check(!RigidActorAsync);
	check(!BodyAggregate);
#endif //WITH_PHYSX
	//check(SceneIndex == 0);

#if WITH_BOX2D
	check(!FromInst->BodyInstancePtr);
	check(!BodyInstancePtr);
#endif

	*this = *FromInst;
}

void FBodyInstance::ExecuteOnPhysicsReadOnly(TFunctionRef<void()> Func) const
{
	//If we are doing a read operation on a static actor there is really no reason to lock both scenes since the data should be the same (as far as queries etc... go)
	//Because of this our read operations are typically on a dynamic or the sync actor

#if WITH_PHYSX
	const int32 SceneIndex = RigidActorSync ? SceneIndexSync : SceneIndexAsync;
	SCOPED_SCENE_READ_LOCK(GetPhysXSceneFromIndex(SceneIndex));
	Func();
#endif
}

void FBodyInstance::ExecuteOnPhysicsReadWrite(TFunctionRef<void()> Func) const
{
	//If we are doing a write operation we will need to modify both actors to keep them in sync (or it's dynamic).
	//Because of that write operations on static actors are more expensive and require both locks.

#if WITH_PHYSX
	if(RigidActorSync)
	{
		SCENE_LOCK_WRITE(GetPhysXSceneFromIndex(SceneIndexSync));
	}
	
	if (RigidActorAsync)
	{
		SCENE_LOCK_WRITE(GetPhysXSceneFromIndex(SceneIndexAsync));
	}

	Func();

	if (RigidActorSync)
	{
		SCENE_UNLOCK_WRITE(GetPhysXSceneFromIndex(SceneIndexSync));
	}

	if (RigidActorAsync)
	{
		SCENE_UNLOCK_WRITE(GetPhysXSceneFromIndex(SceneIndexAsync));
	}
#endif
}


#if WITH_PHYSX
int32 FBodyInstance::GetSceneIndex(int32 SceneType /* = -1 */) const
{
	if(SceneType < 0)
	{
		return RigidActorSync ? SceneIndexSync : SceneIndexAsync;
	}
	else if(SceneType < PST_MAX)
	{
		return SceneType == PST_Sync ? SceneIndexSync : SceneIndexAsync;
	}

	return -1;
}

PxRigidActor* FBodyInstance::GetPxRigidActor_AssumesLocked(int32 SceneType) const
{
	// Negative scene type means to return whichever is not NULL, preferring the sync scene.
	if( SceneType < 0 )
	{
		return RigidActorSync != NULL ? RigidActorSync : RigidActorAsync;
	}
	else
	// Otherwise return the specified actor
	if( SceneType < PST_MAX )
	{
		return SceneType == PST_Sync ? RigidActorSync : RigidActorAsync;
	}
	return NULL;
}

PxRigidDynamic* FBodyInstance::GetPxRigidDynamic_AssumesLocked() const
{
	// The logic below works because dynamic actors are non-NULL in only one scene.
	// If this assumption changes, the logic needs to change.
	PxRigidActor* RigidActor = RigidActorSync ? RigidActorSync : RigidActorAsync;
	return RigidActor ? RigidActor->isRigidDynamic() : nullptr;
}

PxRigidBody* FBodyInstance::GetPxRigidBody_AssumesLocked() const
{
	// The logic below works because dynamic actors are non-NULL in only one scene.
	// If this assumption changes, the logic needs to change.
	PxRigidActor* RigidActor = RigidActorSync ? RigidActorSync : RigidActorAsync;
	return RigidActor ? RigidActor->isRigidBody() : nullptr;
}
#endif // WITH_PHYSX


const FWalkableSlopeOverride& FBodyInstance::GetWalkableSlopeOverride() const
{
	if (bOverrideWalkableSlopeOnInstance || !BodySetup.IsValid())
	{
		return WalkableSlopeOverride;
	}
	else
	{
		return BodySetup->WalkableSlopeOverride;
	}
}

void FBodyInstance::SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride)
{
	bOverrideWalkableSlopeOnInstance = true;
	WalkableSlopeOverride = NewOverride;
}


/** 
*	Changes the current PhysMaterialOverride for this body. 
*	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc, it will only change its 
*	surface properties like friction and the damping.
*/
void FBodyInstance::SetPhysMaterialOverride( UPhysicalMaterial* NewPhysMaterial )
{
	// Save ref to PhysicalMaterial
	PhysMaterialOverride = NewPhysMaterial;

	// Go through the chain of physical materials and update the NxActor
	UpdatePhysicalMaterials();
}

UPhysicalMaterial* FBodyInstance::GetSimplePhysicalMaterial() const
{
	return GetSimplePhysicalMaterial(this, OwnerComponent, BodySetup);
}

UPhysicalMaterial* FBodyInstance::GetSimplePhysicalMaterial(const FBodyInstance* BodyInstance, TWeakObjectPtr<UPrimitiveComponent> OwnerComp, TWeakObjectPtr<UBodySetup> BodySetupPtr)
{
	check(GEngine->DefaultPhysMaterial != NULL);

	// Find the PhysicalMaterial we need to apply to the physics bodies.
	// (LOW priority) Engine Mat, Material PhysMat, BodySetup Mat, Component Override, Body Override (HIGH priority)

	UPhysicalMaterial* ReturnPhysMaterial = NULL;

	// BodyInstance override
	if (BodyInstance->PhysMaterialOverride != NULL)
	{
		ReturnPhysMaterial = BodyInstance->PhysMaterialOverride;
		check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
	}
	// Component override
	else if (OwnerComp.IsValid() && OwnerComp->BodyInstance.PhysMaterialOverride != NULL)
	{
		ReturnPhysMaterial = OwnerComp->BodyInstance.PhysMaterialOverride;
		check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
	}
	// BodySetup
	else if (BodySetupPtr.IsValid() && BodySetupPtr->PhysMaterial != NULL)
	{
		ReturnPhysMaterial = BodySetupPtr->PhysMaterial;
		check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
	}
	else
	{
		// See if the Material has a PhysicalMaterial
		UMeshComponent* MeshComp = Cast<UMeshComponent>(OwnerComp.Get());
		UPhysicalMaterial* PhysMatFromMaterial = NULL;
		if (MeshComp != NULL)
		{
			UMaterialInterface* Material = MeshComp->GetMaterial(0);
			if(Material != NULL)
			{
				PhysMatFromMaterial = Material->GetPhysicalMaterial();
			}
		}

		if( PhysMatFromMaterial != NULL )
		{
			ReturnPhysMaterial = PhysMatFromMaterial;
			check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
		}
		// fallback is default physical material
		else
		{
			ReturnPhysMaterial = GEngine->DefaultPhysMaterial;
			check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
		}
	}

	return ReturnPhysMaterial;
}

TArray<UPhysicalMaterial*> FBodyInstance::GetComplexPhysicalMaterials() const
{
	TArray<UPhysicalMaterial*> PhysMaterials;
	GetComplexPhysicalMaterials(PhysMaterials);
	return PhysMaterials;
}

void FBodyInstance::GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*>& PhysMaterials) const
{
	GetComplexPhysicalMaterials(this, OwnerComponent, PhysMaterials);
}


void FBodyInstance::GetComplexPhysicalMaterials(const FBodyInstance*, TWeakObjectPtr<UPrimitiveComponent> OwnerComp, TArray<UPhysicalMaterial*>& OutPhysicalMaterials)
{
	//NOTE: Binary serialization of physics objects depends on this logic being the same between runs.
	//      If you make any changes to the returned object update the GUID of the physx binary serializer (FDerivedDataPhysXBinarySerializer)

	check(GEngine->DefaultPhysMaterial != NULL);
	// See if the Material has a PhysicalMaterial
	UPrimitiveComponent* PrimComp = OwnerComp.Get();
	if (PrimComp)
	{
		const int32 NumMaterials = PrimComp->GetNumMaterials();
		OutPhysicalMaterials.SetNum(NumMaterials);

		for (int32 MatIdx = 0; MatIdx < NumMaterials; MatIdx++)
		{
			UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;
			UMaterialInterface* Material = PrimComp->GetMaterial(MatIdx);
			if (Material)
			{
				PhysMat = Material->GetPhysicalMaterial();
			}

			check(PhysMat != NULL);
			OutPhysicalMaterials[MatIdx] = PhysMat;
		}
	}
}

#if WITH_PHYSX
/** Util for finding the number of 'collision sim' shapes on this PxRigidActor */
int32 GetNumSimShapes_AssumesLocked(const PxRigidBody* PRigidBody)
{
	TArray<PxShape*, TInlineAllocator<16>> PShapes;
	PShapes.AddUninitialized(PRigidBody->getNbShapes());
	int32 NumShapes = PRigidBody->getShapes(PShapes.GetData(), PShapes.Num());

	int32 NumSimShapes = 0;

	for(int32 ShapeIdx=0; ShapeIdx<NumShapes; ShapeIdx++)
	{
		PxShape* PShape = PShapes[ShapeIdx];
		if(PShape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE)
		{
			NumSimShapes++;
		}
	}

	return NumSimShapes;
}
#endif // WITH_PHYSX

float KgPerM3ToKgPerCm3(float KgPerM3)
{
	//1m = 100cm => 1m^3 = (100cm)^3 = 1000000cm^3
	//kg/m^3 = kg/1000000cm^3
	const float M3ToCm3Inv = 1.f / (100.f * 100.f * 100.f);
	return KgPerM3 * M3ToCm3Inv;
}

float gPerCm3ToKgPerCm3(float gPerCm3)
{
	//1000g = 1kg
	//kg/cm^3 = 1000g/cm^3 => g/cm^3 = kg/1000 cm^3
	const float gToKG = 1.f / 1000.f;
	return gPerCm3 * gToKG;
}

void FBodyInstance::UpdateMassProperties()
{
	UPhysicalMaterial* PhysMat = GetSimplePhysicalMaterial();
	check(PhysMat);

#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&] (PxRigidBody* PRigidBody)
	{
		if(GetNumSimShapes_AssumesLocked(PRigidBody) > 0)
	{
		// physical material - nothing can weigh less than hydrogen (0.09 kg/m^3)
		float DensityKGPerCubicUU = FMath::Max(KgPerM3ToKgPerCm3(0.09f), gPerCm3ToKgPerCm3(PhysMat->Density));
		PxRigidBodyExt::updateMassAndInertia(*PRigidBody, DensityKGPerCubicUU);

		//grab OldMass so we can apply new mass while maintaining inertia tensor
		float OldMass = PRigidBody->getMass();
		float NewMass = 0.f;

		if (bOverrideMass == false)
		{
			float UsePow = FMath::Clamp<float>(PhysMat->RaiseMassToPower, KINDA_SMALL_NUMBER, 1.f);
			NewMass = FMath::Pow(OldMass, UsePow);

			// Apply user-defined mass scaling.
			NewMass *= FMath::Clamp<float>(MassScale, 0.01f, 100.0f);

			MassInKg = NewMass;	//update the override mass to be the default mass
		}
		else
		{
			NewMass = FMath::Max(MassInKg, 0.001f);	//min weight of 1g
		}

			check(NewMass > 0.f);

			float MassRatio = NewMass / OldMass;
		PxVec3 InertiaTensor = PRigidBody->getMassSpaceInertiaTensor();

		PRigidBody->setMassSpaceInertiaTensor(InertiaTensor * MassRatio);
		PRigidBody->setMass(NewMass);

		// Apply the COMNudge
		if (!COMNudge.IsZero())
		{
			PxVec3 PCOMNudge = U2PVector(COMNudge);
			PxTransform PCOMTransform = PRigidBody->getCMassLocalPose();
			PCOMTransform.p += PCOMNudge;
			PRigidBody->setCMassLocalPose(PCOMTransform);
		}
	}
	});
#endif

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		//@TODO: BOX2D: Implement COMNudge, Unreal 'funky' mass algorithm, etc... for UpdateMassProperties (if we don't update the formula, we need to update the displayed mass in the details panel)

		float MassScaledDensity = 0.f;
		if (bOverrideMass == false)
		{
			// Unreal material density is in g/cm^3, and Box2D density is in kg/m^2
			// physical material - nothing can weigh less than hydrogen (0.09 kg/m^3)
			float DensityKGPerCubicCM = FMath::Max(KgPerM3ToKgPerCm3(0.09f), gPerCm3ToKgPerCm3(PhysMat->Density));
			const float DensityKGPerCubicM = DensityKGPerCubicCM * 1000.0f;
			const float DensityKGPerSquareM = DensityKGPerCubicM * 0.1f; //@TODO: BOX2D: Should there be a thickness property for mass calculations?
			MassScaledDensity = DensityKGPerSquareM * FMath::Clamp<float>(MassScale, 0.01f, 100.0f);
		}
		else
		{
			MassScaledDensity = FMath::Max(MassInKg, 0.001f);	//min weight of 1g	//TODO: this is actually wrong because we're assuming mass and density are the same thing, but good enough for now
		}

		check(MassScaledDensity > 0.f);

		// Apply the density
		for (b2Fixture* Fixture = BodyInstancePtr->GetFixtureList(); Fixture; Fixture = Fixture->GetNext())
		{
			Fixture->SetDensity(MassScaledDensity);
		}

		// Recalculate the body mass / COM / etc... based on the updated density
		BodyInstancePtr->ResetMassData();
	}
#endif
}

void FBodyInstance::UpdateDampingProperties()
{
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		PRigidDynamic->setLinearDamping(LinearDamping);
		PRigidDynamic->setAngularDamping(AngularDamping);
	});
#endif

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		BodyInstancePtr->SetLinearDamping(LinearDamping);
		BodyInstancePtr->SetAngularDamping(AngularDamping);
	}
#endif
}

bool FBodyInstance::IsInstanceAwake() const
{
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadOnly(this, [&](const PxRigidDynamic* PRigidDynamic)
	{
		return !PRigidDynamic->isSleeping();
	});
#endif

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		return BodyInstancePtr->IsAwake();
	}
#endif

	return false;
}

void FBodyInstance::WakeInstance()
{
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidDynamic))
	{
		PRigidDynamic->wakeUp();
	}
	});
#endif

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		BodyInstancePtr->SetAwake(true);
	}
#endif
}

void FBodyInstance::PutInstanceToSleep()
{
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidDynamic))
	{
		PRigidDynamic->putToSleep();
	}
	});
#endif //WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		BodyInstancePtr->SetAwake(false);
	}
#endif
}

void FBodyInstance::SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		PxVec3 PNewVel = U2PVector(NewVel);

		if (bAddToCurrent)
		{
			const PxVec3 POldVel = PRigidBody->getLinearVelocity();
			PNewVel += POldVel;
		}

		PRigidBody->setLinearVelocity(PNewVel);
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		b2Vec2 BoxNewVel = FPhysicsIntegration2D::ConvertUnrealVectorToBox(NewVel);

		if (bAddToCurrent)
		{
			const b2Vec2 BoxOldVel = BodyInstancePtr->GetLinearVelocity();
			BoxNewVel += BoxOldVel;
		}

		BodyInstancePtr->SetLinearVelocity(BoxNewVel);
	}
#endif
}

/** Note NewAngVel is in degrees per second */
void FBodyInstance::SetAngularVelocity(const FVector& NewAngVel, bool bAddToCurrent)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		PxVec3 PNewAngVel = U2PVector( FVector::DegreesToRadians(NewAngVel) );

		if (bAddToCurrent)
		{
			const PxVec3 POldAngVel = PRigidBody->getAngularVelocity();
			PNewAngVel += POldAngVel;
		}

		PRigidBody->setAngularVelocity(PNewAngVel);
	});
#endif //WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		float BoxNewAngVel = FPhysicsIntegration2D::ConvertUnrealAngularVelocityToBox(NewAngVel);

		if (bAddToCurrent)
		{
			const float BoxOldAngVel = BodyInstancePtr->GetAngularVelocity();
			BoxNewAngVel += BoxOldAngVel;
		}

		BodyInstancePtr->SetAngularVelocity(BoxNewAngVel);
	}
#endif
}

void FBodyInstance::SetMaxAngularVelocity(float NewMaxAngVel, bool bAddToCurrent)
{
#if WITH_PHYSX
	const bool bIsDynamic = ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		float RadNewMaxAngVel = FMath::DegreesToRadians(NewMaxAngVel);
		
		if (bAddToCurrent)
		{
			float RadOldMaxAngVel = PRigidDynamic->getMaxAngularVelocity();
			RadNewMaxAngVel += RadOldMaxAngVel;

			//doing this part so our UI stays in degrees and not lose precision from the conversion
			float OldMaxAngVel = FMath::RadiansToDegrees(RadOldMaxAngVel);
			NewMaxAngVel += OldMaxAngVel;
		}

		PRigidDynamic->setMaxAngularVelocity(RadNewMaxAngVel);

		MaxAngularVelocity = NewMaxAngVel;
	});

	if(!bIsDynamic)
	{
		MaxAngularVelocity = NewMaxAngVel;	//doesn't really matter since we are not dynamic, but makes sense that we update this anyway
	}
#endif

	//@TODO: BOX2D: Implement SetMaxAngularVelocity
}

void FBodyInstance::SetMaxDepenetrationVelocity(float MaxVelocity)
{
	MaxDepenetrationVelocity = MaxVelocity;
#if WITH_PHYSX
	//0 from both instance and project settings means 
	float UseMaxVelocity = MaxVelocity == 0.f ? PX_MAX_F32 : MaxVelocity;
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		PRigidBody->setMaxDepenetrationVelocity(UseMaxVelocity);
	});
#endif

}


void FBodyInstance::AddCustomPhysics(FCalculateCustomPhysics& CalculateCustomPhysics)
{
	
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadOnly(this, [&](const PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddCustomPhysics_AssumesLocked(this, CalculateCustomPhysics);
			}
		}
	});
	
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		// Since Box2D does not have any substepping, might as well apply custom forces now
		CalculateCustomPhysics.ExecuteIfBound(0.0f, this);
	}
#endif
}

void FBodyInstance::AddForce(const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddForce_AssumesLocked(this, Force, bAllowSubstepping, bAccelChange);
			}

		}
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		if (!Force.IsNearlyZero())
		{
			const b2Vec2 Force2D = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Force);
			BodyInstancePtr->ApplyForceToCenter(bAccelChange ? (BodyInstancePtr->GetMass() * Force2D) : Force2D, /*wake=*/ true);
		}
	}
#endif
}

void FBodyInstance::AddForceAtPosition(const FVector& Force, const FVector& Position, bool bAllowSubstepping)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddForceAtPosition_AssumesLocked(this, Force, Position, bAllowSubstepping);
			}
		}
	});
	
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		if (!Force.IsNearlyZero())
		{
			const b2Vec2 Force2D = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Force);
			const b2Vec2 Position2D = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Position);
			BodyInstancePtr->ApplyForce(Force2D, Position2D, /*wake=*/ true);
		}
	}
#endif
}

void FBodyInstance::AddTorque(const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddTorque_AssumesLocked(this, Torque, bAllowSubstepping, bAccelChange);
			}
		}
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		const float Torque1D = FPhysicsIntegration2D::ConvertUnrealTorqueToBox(Torque) * (bAccelChange ? BodyInstancePtr->GetInertia() : 1.f);
		if (!FMath::IsNearlyZero(Torque1D))
		{
			BodyInstancePtr->ApplyTorque(Torque1D, /*wake=*/ true);
		}
	}
#endif
}

void FBodyInstance::AddAngularImpulse(const FVector& AngularImpulse, bool bVelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
	{
		PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
		PRigidBody->addTorque(U2PVector(AngularImpulse), Mode, true);
	}
	});
	
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		if (!AngularImpulse.IsNearlyZero())
		{
			const float AngularImpulse2D = FPhysicsIntegration2D::ConvertUnrealTorqueToBox(AngularImpulse);
			BodyInstancePtr->ApplyAngularImpulse(AngularImpulse2D, /*wake=*/ true);
		}
	}
#endif
}

void FBodyInstance::AddImpulse(const FVector& Impulse, bool bVelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
	{
		PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
		PRigidBody->addForce(U2PVector(Impulse), Mode, true);
	}
	});
	
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		if (!Impulse.IsNearlyZero())
		{
			const b2Vec2 Impulse2D = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Impulse);
			BodyInstancePtr->ApplyLinearImpulse(Impulse2D, BodyInstancePtr->GetWorldCenter(), /*wake=*/ true);
		}
	}
#endif
}

void FBodyInstance::AddImpulseAtPosition(const FVector& Impulse, const FVector& Position)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
	{
		PxForceMode::Enum Mode = PxForceMode::eIMPULSE; // does not support eVELOCITY_CHANGE
		PxRigidBodyExt::addForceAtPos(*PRigidBody, U2PVector(Impulse), U2PVector(Position), Mode, true);
	}
	});
	
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		if (!Impulse.IsNearlyZero())
		{
			const b2Vec2 Impulse2D = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Impulse);
			const b2Vec2 Position2D = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Position);
			BodyInstancePtr->ApplyLinearImpulse(Impulse2D, Position2D, /*wake=*/ true);
		}
	}
#endif
}

void FBodyInstance::SetInstanceNotifyRBCollision(bool bNewNotifyCollision)
{
	bNotifyRigidBodyCollision = bNewNotifyCollision;
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetEnableGravity(bool bInGravityEnabled)
{
	if (bEnableGravity != bInGravityEnabled)
	{
		bEnableGravity = bInGravityEnabled;

#if WITH_PHYSX
		ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
		{
			PRigidBody->setActorFlag( PxActorFlag::eDISABLE_GRAVITY, !bEnableGravity );
		});
#endif // WITH_PHYSX

#if WITH_BOX2D
		if (BodyInstancePtr)
		{
			BodyInstancePtr->SetGravityScale(bEnableGravity ? 1.0f : 0.0f);
		}
#endif

		if (bEnableGravity)
		{
			WakeInstance();
		}
	}
}


#if WITH_BOX2D
void AddRadialImpulseToBox2DBodyInstance(class b2Body* BodyInstancePtr, const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bImpulse, bool bMassIndependent)
{
	const b2Vec2 CenterOfMass2D = BodyInstancePtr->GetWorldCenter();
	const b2Vec2 Origin2D = FPhysicsIntegration2D::ConvertUnrealVectorToBox(Origin);

	// If the center of mass is outside of the blast radius, do nothing.
	b2Vec2 Delta2D = CenterOfMass2D - Origin2D;
	const float DistanceFromBlast = Delta2D.Normalize() * UnrealUnitsPerMeter;
	if (DistanceFromBlast > Radius)
	{
		return;
	}
	
	// If using linear falloff, scale with distance.
	const float EffectiveStrength = (Falloff == RIF_Linear) ? (Strength * (1.0f - (DistanceFromBlast / Radius))) : Strength;

	b2Vec2 ForceOrImpulse2D = Delta2D;
	ForceOrImpulse2D *= EffectiveStrength;

	if (bMassIndependent)
	{
		const float Mass = BodyInstancePtr->GetMass();
		if (!FMath::IsNearlyZero(Mass))
		{
			ForceOrImpulse2D *= Mass;
		}
	}

	if (bImpulse)
	{
		BodyInstancePtr->ApplyLinearImpulse(ForceOrImpulse2D, CenterOfMass2D, /*wake=*/ true);
	}
	else
	{
		BodyInstancePtr->ApplyForceToCenter(ForceOrImpulse2D, /*wake=*/ true);
	}
}
#endif



void FBodyInstance::AddRadialImpulseToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
	{
			AddRadialImpulseToPxRigidBody_AssumesLocked(*PRigidBody, Origin, Radius, Strength, Falloff, bVelChange);
	}
	});
	
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		AddRadialImpulseToBox2DBodyInstance(BodyInstancePtr, Origin, Radius, Strength, static_cast<ERadialImpulseFalloff>(Falloff), /*bImpulse=*/ true, /*bMassIndependent=*/ bVelChange);
	}
#endif
}

void FBodyInstance::AddRadialForceToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (IsRigidBodyNonKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddRadialForceToBody_AssumesLocked(this, Origin, Radius, Strength, Falloff, bAccelChange, bAllowSubstepping);
			}

		}
	});
#endif // WITH_PHYSX

#if WITH_BOX2D
	if (BodyInstancePtr)
	{
		AddRadialImpulseToBox2DBodyInstance(BodyInstancePtr, Origin, Radius, Strength, static_cast<ERadialImpulseFalloff>(Falloff), /*bImpulse=*/ false, /*bMassIndependent=*/ bAccelChange);
	}
#endif
}

FString FBodyInstance::GetBodyDebugName() const
{
	FString DebugName;

	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	if (OwnerComponentInst != NULL)
	{
		DebugName = OwnerComponentInst->GetPathName();
		if (const UObject* StatObject = OwnerComponentInst->AdditionalStatObject())
		{
			DebugName += TEXT(" ");
			StatObject->AppendName(DebugName);
		}
	}

	if ((BodySetup != NULL) && (BodySetup->BoneName != NAME_None))
	{
		DebugName += FString(TEXT(" Bone: ")) + BodySetup->BoneName.ToString();
	}

	return DebugName;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// COLLISION

bool FBodyInstance::LineTrace(struct FHitResult& OutHit, const FVector& Start, const FVector& End, bool bTraceComplex, bool bReturnPhysicalMaterial) const
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_RaycastAny);

	OutHit.TraceStart = Start;
	OutHit.TraceEnd = End;

	bool bHitSomething = false;

	const FVector Delta = End - Start;
	const float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
#if WITH_PHYSX
		ExecuteOnPhysicsReadOnly([&]
		{
			const PxRigidActor* RigidBody = WeldParent ? WeldParent->GetPxRigidActor_AssumesLocked() : GetPxRigidActor_AssumesLocked();
		if ((RigidBody != NULL) && (RigidBody->getNbShapes() != 0))
		{
			// Create filter data used to filter collisions, should always return eTOUCH for LineTraceComponent		
			PxSceneQueryFlags POutputFlags = PxSceneQueryFlag::eIMPACT | PxSceneQueryFlag::eNORMAL | PxSceneQueryFlag::eDISTANCE;

			PxRaycastHit BestHit;
			float BestHitDistance = BIG_NUMBER;

			// Get all the shapes from the actor
			TArray<PxShape*, TInlineAllocator<16>> PShapes;
			PShapes.AddUninitialized(RigidBody->getNbShapes());
			int32 NumShapes = RigidBody->getShapes(PShapes.GetData(), PShapes.Num());

			// Iterate over each shape
			for (int32 ShapeIdx = 0; ShapeIdx < PShapes.Num(); ShapeIdx++)
			{
				PxShape* PShape = PShapes[ShapeIdx];
				check(PShape);

				if (ShapeBoundToBody(PShape, this) == false) { continue;  }

				const PxU32 HitBufferSize = 1;
				PxRaycastHit PHits[HitBufferSize];

				// Filter so we trace against the right kind of collision
				PxFilterData ShapeFilter = PShape->getQueryFilterData();
				const bool bShapeIsComplex = (ShapeFilter.word3 & EPDF_ComplexCollision) != 0;
				const bool bShapeIsSimple = (ShapeFilter.word3 & EPDF_SimpleCollision) != 0;
				if ((bTraceComplex && bShapeIsComplex) || (!bTraceComplex && bShapeIsSimple))
				{
					const int32 ArraySize = ARRAY_COUNT(PHits);
					// only care about one hit - not closest hit			
					const PxI32 NumHits = PxGeometryQuery::raycast(U2PVector(Start), U2PVector(Delta / DeltaMag), PShape->getGeometry().any(), PxShapeExt::getGlobalPose(*PShape, *RigidBody), DeltaMag, POutputFlags, ArraySize, PHits, true);


					if (ensure(NumHits <= ArraySize))
					{
						for (int HitIndex = 0; HitIndex < NumHits; HitIndex++)
						{
							if (PHits[HitIndex].distance < BestHitDistance)
							{
								BestHitDistance = PHits[HitIndex].distance;
								BestHit = PHits[HitIndex];

								// we don't get Shape information when we access via PShape, so I filled it up
								BestHit.shape = PShape;
								BestHit.actor = HasSharedShapes() ? RigidActorSync : PShape->getActor();	//for shared shapes there is no actor, but since it's shared just return the sync actor
							}
						}
					}
				}
			}

			if (BestHitDistance < BIG_NUMBER)
			{
				// we just like to make sure if the hit is made, set to test touch
				PxFilterData QueryFilter;
				QueryFilter.word2 = 0xFFFFF;

				PxTransform PStartTM(U2PVector(Start));
				const UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
				ConvertQueryImpactHit(OwnerComponentInst ? OwnerComponentInst->GetWorld() : nullptr, BestHit, OutHit, DeltaMag, QueryFilter, Start, End, NULL, PStartTM, false, bReturnPhysicalMaterial);
				bHitSomething = true;
			}
		}
		});
		
#endif //WITH_PHYSX

#if WITH_BOX2D
		if (BodyInstancePtr != nullptr)
		{
			//@TODO: BOX2D: Implement FBodyInstance::LineTrace
		}
#endif
	}

	return bHitSomething;
}

bool FBodyInstance::Sweep(struct FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionShape& CollisionShape, bool bTraceComplex) const
{
	if (CollisionShape.IsNearlyZero())
	{
		return LineTrace(OutHit, Start, End, bTraceComplex);
	}
	else
	{
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;

		bool bSweepHit = false;
#if WITH_PHYSX
		ExecuteOnPhysicsReadOnly([&]
		{
			const PxRigidActor* RigidBody = WeldParent ? WeldParent->GetPxRigidActor_AssumesLocked() : GetPxRigidActor_AssumesLocked();

		if ((RigidBody != NULL) && (RigidBody->getNbShapes() != 0) && (OwnerComponent != NULL))
		{
			FPhysXShapeAdaptor ShapeAdaptor(FQuat::Identity, CollisionShape);
			bSweepHit = InternalSweepPhysX(OutHit, Start, End, CollisionShape, bTraceComplex, RigidBody, &ShapeAdaptor.GetGeometry());
		}
		});
		
#endif //WITH_PHYSX

#if WITH_BOX2D
		if (BodyInstancePtr != nullptr)
		{
			//@TODO: BOX2D: Implement FBodyInstance::Sweep
		}
#endif

		return bSweepHit;
	}
}

#if WITH_PHYSX
bool FBodyInstance::InternalSweepPhysX(struct FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionShape& Shape, bool bTraceComplex, const PxRigidActor* RigidBody, const PxGeometry* Geometry) const
{
	const FVector Delta = End - Start;
	const float DeltaMag = Delta.Size();
	if(DeltaMag > KINDA_SMALL_NUMBER)
	{
		PxSceneQueryFlags POutputFlags = PxSceneQueryFlag::eIMPACT | PxSceneQueryFlag::eNORMAL | PxSceneQueryFlag::eDISTANCE | PxSceneQueryFlag::eMTD;
		
		PxQuat PGeomRot = PxQuat::createIdentity();

		UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
		PxTransform PStartTM(U2PVector(Start), PGeomRot);
		PxTransform PCompTM(U2PTransform(OwnerComponentInst->ComponentToWorld));

		PxVec3 PDir = U2PVector(Delta/DeltaMag);

		PxSweepHit PHit;

		// Get all the shapes from the actor
		TArray<PxShape*, TInlineAllocator<16>> PShapes;
		PShapes.AddUninitialized(RigidBody->getNbShapes());
		int32 NumShapes = RigidBody->getShapes(PShapes.GetData(), PShapes.Num());

		// Iterate over each shape
		for(int32 ShapeIdx=0; ShapeIdx<PShapes.Num(); ShapeIdx++)
		{
			PxShape* PShape = PShapes[ShapeIdx];
			check(PShape);

			if (ShapeBoundToBody(PShape, this) == false){ continue; }

			// Filter so we trace against the right kind of collision
			PxFilterData ShapeFilter = PShape->getQueryFilterData();
			const bool bShapeIsComplex = (ShapeFilter.word3 & EPDF_ComplexCollision) != 0;
			const bool bShapeIsSimple = (ShapeFilter.word3 & EPDF_SimpleCollision) != 0;
			if((bTraceComplex && bShapeIsComplex) || (!bTraceComplex && bShapeIsSimple))
			{
				GeometryFromShapeStorage GeomStorage;
				PxGeometry* PGeom = GetGeometryFromShape(GeomStorage, PShape, true);
				PxTransform PGlobalPose = PCompTM.transform(PShape->getLocalPose());

				if (PGeom)
				{
					if(PxGeometryQuery::sweep(PDir, DeltaMag, *Geometry, PStartTM, *PGeom, PGlobalPose, PHit, POutputFlags))
					{
						// we just like to make sure if the hit is made
						PxFilterData QueryFilter;
						QueryFilter.word2 = 0xFFFFF;

						// we don't get Shape information when we access via PShape, so I filled it up
						PHit.shape = PShape;
						PHit.actor = HasSharedShapes() ? RigidActorSync : PShape->getActor();	//in the case of shared shapes getActor will return null. Since the shape is shared we just return the sync actor
						PxTransform PStartTransform(U2PVector(Start));
						ConvertQueryImpactHit(OwnerComponentInst->GetWorld(), PHit, OutHit, DeltaMag, QueryFilter, Start, End, NULL, PStartTransform, false, false);
						return true;
					}
				}
			}
		}
	}
	return false;
}
#endif //WITH_PHYSX

float FBodyInstance::GetDistanceToBody(const FVector& Point, FVector& OutPointOnBody) const
{
	OutPointOnBody = Point;
	float ReturnDistance = -1.f;
	float MinDistanceSqr = BIG_NUMBER;
	bool bFoundValidBody = false;
	bool bEarlyOut = true;

#if WITH_PHYSX
	ExecuteOnPxRigidActorReadOnly(this, [&](const PxRigidActor* RigidActor)
	{
		if (RigidActor->getNbShapes() == 0 || OwnerComponent == NULL)
		{
			return;
		}

		bEarlyOut = false;

		// Get all the shapes from the actor
		TArray<PxShape*, TInlineAllocator<16>> PShapes;
		PShapes.AddUninitialized(RigidActor->getNbShapes());
		int32 NumShapes = RigidActor->getShapes(PShapes.GetData(), PShapes.Num());

		const PxVec3 PPoint = U2PVector(Point);

		// Iterate over each shape
		for (int32 ShapeIdx = 0; ShapeIdx < PShapes.Num(); ShapeIdx++)
		{
			PxShape* PShape = PShapes[ShapeIdx];
			check(PShape);
			PxGeometry& PGeom = PShape->getGeometry().any();
			PxTransform PGlobalPose = PxShapeExt::getGlobalPose(*PShape, *RigidActor);
			PxGeometryType::Enum GeomType = PShape->getGeometryType();

			if (GeomType == PxGeometryType::eTRIANGLEMESH)
			{
				// Type unsupported for this function, but some other shapes will probably work. 
				continue;
			}
			bFoundValidBody = true;

			PxVec3 PClosestPoint;
			float SqrDistance = PxGeometryQuery::pointDistance(PPoint, PGeom, PGlobalPose, &PClosestPoint);
			// distance has valid data and smaller than mindistance
				if (SqrDistance > 0.f && MinDistanceSqr > SqrDistance)
			{
				MinDistanceSqr = SqrDistance;
				OutPointOnBody = P2UVector(PClosestPoint);
			}
				else if (SqrDistance == 0.f)
			{
				MinDistanceSqr = 0.f;
				break;
			}
		}	
	});
#endif //WITH_PHYSX

	if (!bFoundValidBody && !bEarlyOut)
	{
		UE_LOG(LogPhysics, Warning, TEXT("GetDistanceToBody: Component (%s) has no simple collision and cannot be queried for closest point."), OwnerComponent.Get() ? *OwnerComponent->GetPathName() : TEXT("NONE"));
	}

	return (bFoundValidBody ? FMath::Sqrt(MinDistanceSqr) : -1.f);

	//@TODO: BOX2D: Implement DistanceToBody
}

bool FBodyInstance::OverlapTestForBodies(const FVector& Pos, const FQuat& Rot, const TArray<FBodyInstance*>& Bodies) const
{
	bool bHaveOverlap = false;
#if WITH_PHYSX

	ExecuteOnPxRigidActorReadOnly(this, [&] (const PxRigidActor* TargetRigidActor)
	{
		// calculate the test global pose of the rigid body
		PxTransform PTestGlobalPose = U2PTransform(FTransform(Rot, Pos));

		// Get all the shapes from the actor
		TArray<PxShape*, TInlineAllocator<16>> PTargetShapes;
		PTargetShapes.AddUninitialized(TargetRigidActor->getNbShapes());
		int32 NumTargetShapes = TargetRigidActor->getShapes(PTargetShapes.GetData(), PTargetShapes.Num());

		for (int32 TargetShapeIdx = 0; TargetShapeIdx < PTargetShapes.Num(); ++TargetShapeIdx)
		{
			const PxShape * PTargetShape = PTargetShapes[TargetShapeIdx];

			// Calc shape global pose
			PxTransform PShapeGlobalPose = PTestGlobalPose.transform(PTargetShape->getLocalPose());

			GET_GEOMETRY_FROM_SHAPE(PGeom, PTargetShape);

			if (PGeom)
			{
				for (const FBodyInstance* BodyInstance : Bodies)
				{
					bHaveOverlap = BodyInstance->OverlapPhysX_AssumesLocked(*PGeom, PShapeGlobalPose);
					if (bHaveOverlap)
					{
						return;
					}
				}
			}
		}
	});
#endif //WITH_PHYSX
	return bHaveOverlap;
}

bool FBodyInstance::OverlapTest(const FVector& Position, const FQuat& Rotation, const struct FCollisionShape& CollisionShape, FMTDResult* OutMTD) const
{
	bool bHasOverlap = false;

#if WITH_PHYSX
	ExecuteOnPxRigidActorReadOnly(this, [&] (const PxRigidActor* )
	{
	FPhysXShapeAdaptor ShapeAdaptor(Rotation, CollisionShape);
		bHasOverlap = OverlapPhysX_AssumesLocked(ShapeAdaptor.GetGeometry(), ShapeAdaptor.GetGeomPose(Position), OutMTD);
	});
#endif

#if WITH_BOX2D
	if (!bHasOverlap && (BodyInstancePtr != nullptr))
	{
		//@TODO: BOX2D: Implement FBodyInstance::OverlapTest
	}
#endif

	return bHasOverlap;
}

FTransform RootSpaceToWeldedSpace(const FBodyInstance* BI, const FTransform& RootTM)
{
	if (BI->WeldParent)
	{
		UPrimitiveComponent* BIOwnerComponentInst = BI->OwnerComponent.Get();
		if (BIOwnerComponentInst)
		{
			FTransform RootToWelded = BIOwnerComponentInst->GetRelativeTransform().Inverse();
			RootToWelded.ScaleTranslation(BI->Scale3D);

			return RootToWelded * RootTM;
		}
	}

	return RootTM;
}

bool FBodyInstance::OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FQuat& Quat, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
	if ( (IsValidBodyInstance() || (WeldParent && WeldParent->IsValidBodyInstance())) == false )
	{
		UE_LOG(LogCollision, Log, TEXT("FBodyInstance::OverlapMulti : (%s) No physics data"), *GetBodyDebugName());
		return false;
	}

	SCOPE_CYCLE_COUNTER(STAT_Collision_GeomOverlapMultiple);
	bool bHaveBlockingHit = false;

	// Determine how to convert the local space of this body instance to the test space
	const FTransform ComponentSpaceToTestSpace(Quat, Pos);

	FTransform BodyInstanceSpaceToTestSpace;
	if (pWorldToComponent)
	{
		const FTransform RootTM = WeldParent ? WeldParent->GetUnrealWorldTransform() : GetUnrealWorldTransform();
		const FTransform ShapeSpaceToComponentSpace = RootTM * (*pWorldToComponent);
		BodyInstanceSpaceToTestSpace = ShapeSpaceToComponentSpace * ComponentSpaceToTestSpace;
	}
	else
	{
		BodyInstanceSpaceToTestSpace = ComponentSpaceToTestSpace;
	}

	//We want to test using global position. However, the global position of the body will be in terms of the root body which we are welded to. So we must undo the relative transform so that our shapes are centered
	//Global = Parent * Relative => Global * RelativeInverse = Parent
	if (WeldParent)
	{
		BodyInstanceSpaceToTestSpace = RootSpaceToWeldedSpace(this, BodyInstanceSpaceToTestSpace);
	}

#if WITH_PHYSX
	PxRigidActor* PRigidActor = WeldParent ? WeldParent->GetPxRigidActor_AssumesLocked() : GetPxRigidActor_AssumesLocked();
	if (PRigidActor)
	{
		const PxTransform PBodyInstanceSpaceToTestSpace = U2PTransform(BodyInstanceSpaceToTestSpace);

		ExecuteOnPhysicsReadOnly([&]
		{
		// Get all the shapes from the actor
			TArray<PxShape*, TInlineAllocator<16>> PShapes;
			PShapes.AddUninitialized(PRigidActor->getNbShapes());
			int32 NumShapes = PRigidActor->getShapes(PShapes.GetData(), PShapes.Num());

			// Iterate over each shape
			TArray<struct FOverlapResult> TempOverlaps;
			for (int32 ShapeIdx = 0; ShapeIdx < PShapes.Num(); ShapeIdx++)
			{
				PxShape* PShape = PShapes[ShapeIdx];
				check(PShape);

				if (ShapeBoundToBody(PShape, this) == false)
				{
					continue;
				}

				// Calc shape global pose
				const PxTransform PLocalPose = PShape->getLocalPose();
				const PxTransform PShapeGlobalPose = PBodyInstanceSpaceToTestSpace.transform(PLocalPose);

				GET_GEOMETRY_FROM_SHAPE(PGeom, PShape);

				if (PGeom != NULL)
				{
					TempOverlaps.Reset();
					if (GeomOverlapMulti_PhysX(World, *PGeom, PShapeGlobalPose, TempOverlaps, TestChannel, Params, ResponseParams, ObjectQueryParams))
					{
						bHaveBlockingHit = true;
					}
					InOutOverlaps.Append(TempOverlaps);
				}
			}
		});
	}
#endif

#if WITH_BOX2D
	if (BodyInstancePtr != nullptr)
	{
		//@TODO: BOX2D: Implement FBodyInstance::OverlapMulti
	}
#endif

	return bHaveBlockingHit;
}


#if WITH_PHYSX
bool FBodyInstance::OverlapPhysX_AssumesLocked(const PxGeometry& PGeom, const PxTransform& ShapePose, FMTDResult* OutMTD) const
{
	const PxRigidActor* RigidBody = WeldParent ? WeldParent->GetPxRigidActor_AssumesLocked() : GetPxRigidActor_AssumesLocked();

	if (RigidBody==NULL || RigidBody->getNbShapes()==0)
	{
		return false;
	}

	// Get all the shapes from the actor
	TArray<PxShape*, TInlineAllocator<16>> PShapes;
	PShapes.AddUninitialized(RigidBody->getNbShapes());
	int32 NumShapes = RigidBody->getShapes(PShapes.GetData(), PShapes.Num());

	// Iterate over each shape
	for(int32 ShapeIdx=0; ShapeIdx<PShapes.Num(); ++ShapeIdx)
	{
		const PxShape* PShape = PShapes[ShapeIdx];
		check(PShape);

		if (ShapeBoundToBody(PShape, this) == true)
		{
			PxVec3 POutDirection;
			float OutDistance;
			if (PxGeometryQuery::computePenetration(POutDirection, OutDistance, PGeom, ShapePose, PShape->getGeometry().any(), PxShapeExt::getGlobalPose(*PShape, *RigidBody)))
			{
				if(OutMTD)
				{
					OutMTD->Direction = P2UVector(POutDirection);
					OutMTD->Distance = OutDistance;
				}
					
				return true;
			}			
		}
	}
	return false;
}
#endif //WITH_PHYSX


bool FBodyInstance::IsValidCollisionProfileName(FName InCollisionProfileName)
{
	return (InCollisionProfileName != NAME_None) && (InCollisionProfileName != UCollisionProfile::CustomCollisionProfileName);
}

void FBodyInstance::LoadProfileData(bool bVerifyProfile)
{
	if ( bVerifyProfile )
	{
		// if collision profile name exists, 
		// check with current settings
		// if same, then keep the profile name
		// if not same, that means it has been modified from default
		// leave it as it is, and clear profile name
		if ( IsValidCollisionProfileName(CollisionProfileName) )
		{
			FCollisionResponseTemplate Template;
			if ( UCollisionProfile::Get()->GetProfileTemplate(CollisionProfileName, Template) ) 
			{
				// this function is only used for old code that did require verification of using profile or not
				// so that means it will have valid ResponsetoChannels value, so this is okay to access. 
				if (Template.IsEqual(CollisionEnabled, ObjectType, CollisionResponses.GetResponseContainer()) == false)
				{
					InvalidateCollisionProfileName(); 
				}
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("COLLISION PROFILE [%s] is not found"), *CollisionProfileName.ToString());
				// if not nothing to do
				InvalidateCollisionProfileName(); 
			}
		}

	}
	else
	{
		if ( IsValidCollisionProfileName(CollisionProfileName) )
		{
			if ( UCollisionProfile::Get()->ReadConfig(CollisionProfileName, *this) == false)
			{
				// clear the name
				InvalidateCollisionProfileName();
			}
		}

		// no profile, so it just needs to update container from array data
		if ( DoesUseCollisionProfile() == false )
		{
			CollisionResponses.UpdateResponseContainerFromArray();
		}
	}
}

SIZE_T FBodyInstance::GetBodyInstanceResourceSize(EResourceSizeMode::Type Mode) const
{
	SIZE_T ResSize = 0;

#if WITH_PHYSX
	// Get size of PhysX data, skipping contents of 'shared data'
	if (RigidActorSync != NULL)
	{
		ResSize += GetPhysxObjectSize(RigidActorSync, FPhysxSharedData::Get().GetCollection());
	}

	if (RigidActorAsync != NULL)
	{
		ResSize += GetPhysxObjectSize(RigidActorAsync, FPhysxSharedData::Get().GetCollection());
	}
#endif

	return ResSize;
}

void FBodyInstance::FixupData(class UObject* Loader)
{
	check (Loader);

	int32 const UE4Version = Loader->GetLinkerUE4Version();

	if (UE4Version < VER_UE4_ADD_CUSTOMPROFILENAME_CHANGE)
	{
		if (CollisionProfileName == NAME_None)
		{
			CollisionProfileName = UCollisionProfile::CustomCollisionProfileName;
		}
	}

	if (UE4Version < VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL)
	{
		CollisionResponses.SetCollisionResponseContainer(ResponseToChannels_DEPRECATED);
	}

	// Load profile. If older version, please verify profile name first
	bool bNeedToVerifyProfile = (UE4Version < VER_UE4_COLLISION_PROFILE_SETTING) || 
		// or shape component needs to convert since we added profile
		(UE4Version < VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL && Loader->IsA(UShapeComponent::StaticClass()));
	LoadProfileData(bNeedToVerifyProfile);

	// if profile isn't set, then fix up channel responses
	if( CollisionProfileName == UCollisionProfile::CustomCollisionProfileName ) 
	{
		if (UE4Version >= VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL)
		{
			CollisionResponses.UpdateResponseContainerFromArray();
		}
	}
}

bool FBodyInstance::UseAsyncScene() const
{
	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	UWorld* World = OwnerComponentInst ? OwnerComponentInst->GetWorld() : nullptr;
	return UseAsyncScene(World ? World->GetPhysicsScene() : nullptr);
}

bool FBodyInstance::UseAsyncScene(const FPhysScene* PhysScene) const
{
	bool bHasAsyncScene = true;
	if (PhysScene)
	{
		bHasAsyncScene = PhysScene->HasAsyncScene();
	}

	return bUseAsyncScene && bHasAsyncScene;
}


void FBodyInstance::SetUseAsyncScene(bool bNewUseAsyncScene)
{
#if WITH_BOX2D
	// Invalid to call this if the body has been created
	check(BodyInstancePtr == nullptr);
#endif // WITH_BOX2D

	// Set flag
	bUseAsyncScene = bNewUseAsyncScene;
}

void FBodyInstance::ApplyMaterialToShape_AssumesLocked(PxShape* PShape, PxMaterial* PSimpleMat, const TArray<UPhysicalMaterial*>& ComplexPhysMats, const bool bSharedShape)
{
	if(!bSharedShape && !PShape->isExclusive())	//user says the shape is exclusive, but physx says it's shared
	{
		UE_LOG(LogPhysics, Warning, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : Trying to change the physical material of a shared shape. If this is your intention pass bSharedShape = true"));
	}

	// If a triangle mesh, need to get array of materials...
	if(PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
	{
		TArray<PxMaterial*, TInlineAllocator<16>> PComplexMats;
		PComplexMats.AddUninitialized(ComplexPhysMats.Num());
		for(int MatIdx = 0; MatIdx < ComplexPhysMats.Num(); MatIdx++)
		{
			check(ComplexPhysMats[MatIdx] != NULL);
			PComplexMats[MatIdx] = ComplexPhysMats[MatIdx]->GetPhysXMaterial();
			check(PComplexMats[MatIdx] != NULL);
		}

		if(PComplexMats.Num())
		{
			PShape->setMaterials(PComplexMats.GetData(), PComplexMats.Num());
		}
		else
		{
			UE_LOG(LogPhysics, Warning, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : PComplexMats is empty - falling back on simple physical material."));
			PShape->setMaterials(&PSimpleMat, 1);
		}

	}
	// Simple shape, 
	else
	{
		PShape->setMaterials(&PSimpleMat, 1);
	}
}

void FBodyInstance::ApplyMaterialToInstanceShapes_AssumesLocked(PxMaterial* PSimpleMat, TArray<UPhysicalMaterial*>& ComplexPhysMats)
{
	TArray<PxShape*> AllShapes;
	GetAllShapes_AssumesLocked(AllShapes);

	for(int32 ShapeIdx = 0; ShapeIdx < AllShapes.Num(); ShapeIdx++)
	{
		PxShape* PShape = AllShapes[ShapeIdx];

		ExecuteOnPxShapeWrite(this, PShape, [&](PxShape* PNewShape)
		{
			ApplyMaterialToShape_AssumesLocked(PNewShape, PSimpleMat, ComplexPhysMats, HasSharedShapes());
		});		
	}
}

bool FBodyInstance::ValidateTransform(const FTransform &Transform, const FString& DebugName, const UBodySetup* Setup)
{
	if(Transform.GetScale3D().IsNearlyZero())
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Scale3D is (nearly) zero: %s"), *DebugName);
		return false;
	}

	// Check we support mirroring/non-mirroring
	const float TransformDet = Transform.GetDeterminant();
	if(TransformDet < 0.f && !Setup->bGenerateMirroredCollision)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Body is mirrored but bGenerateMirroredCollision == false: %s"), *DebugName);
		return false;
	}

	if(TransformDet > 0.f && !Setup->bGenerateNonMirroredCollision)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Body is not mirrored but bGenerateNonMirroredCollision == false: %s"), *DebugName);
		return false;
	}

	if(Transform.ContainsNaN())
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Bad transform - %s %s\n%s"), *DebugName, *Setup->BoneName.ToString(), *Transform.ToString());
		return false;
	}

	return true;
}

#if WITH_PHYSX
void FBodyInstance::InitDynamicProperties_AssumesLocked()
{
	if(PxRigidDynamic* RigidActor = GetPxRigidDynamic_AssumesLocked())
	{
		UpdateMassProperties();
		UpdateDampingProperties();
		SetMaxAngularVelocity(MaxAngularVelocity, false);
		SetMaxDepenetrationVelocity(bOverrideMaxDepenetrationVelocity ? MaxDepenetrationVelocity : UPhysicsSettings::Get()->MaxDepenetrationVelocity);

		if(ShouldInstanceSimulatingPhysics())
		{
			RigidActor->setLinearVelocity(U2PVector(InitialLinearVelocity));
		}

		float SleepEnergyThresh = RigidActor->getSleepThreshold();
		if (SleepFamily == ESleepFamily::Sensitive)
		{
			SleepEnergyThresh /= 20.f;
		}
		RigidActor->setSleepThreshold(SleepEnergyThresh);
		// set solver iteration count 
		int32 PositionIterCount = FMath::Clamp(PositionSolverIterationCount, 1, 255);
		int32 VelocityIterCount = FMath::Clamp(VelocitySolverIterationCount, 1, 255);
		RigidActor->setSolverIterationCounts(PositionIterCount, VelocityIterCount);

		if(RigidActor->getScene())
		{
			CreateDOFLock();
			if(IsRigidBodyNonKinematic_AssumesLocked(RigidActor))
			{
				if(bStartAwake || bWokenExternally)
				{
					RigidActor->wakeUp();
				}
				else
				{
					RigidActor->putToSleep();
				}
			}
		}
	}
}

void FBodyInstance::GetFilterData_AssumesLocked(FShapeData& ShapeData, bool bForceSimpleAsComplex)//PxFilterData& SimData, PxFilterData& QueryDataSimple, PxFilterData& QueryDataComplex, TEnumAsByte<ECollisionEnabled::Type>& UseCollisionEnabled, bool bForceSimpleAsComplex)
{
	// Do nothing if no physics actor
	if(!IsValidBodyInstance())
	{
		return;
	}

	// this can happen in landscape height field collision component
	if(!BodySetup.IsValid())
	{
		return;
	}

	// Figure out if we are static
	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;
	const bool bPhysicsStatic = !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry();

	// Grab collision setting from body instance
	ShapeData.CollisionEnabled = GetCollisionEnabled(); // this checks actor override
	bool bUseNotifyRBCollision = bNotifyRigidBodyCollision;
	FCollisionResponseContainer UseResponse = CollisionResponses.GetResponseContainer();

	bool bUseCollisionEnabledOverride = false;
	bool bResponseOverride = false;
	bool bNotifyOverride = false;

	// Get skelmeshcomp ID
	uint32 SkelMeshCompID = 0;
	if(USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(OwnerComponentInst))
	{
		SkelMeshCompID = SkelMeshComp->GetUniqueID();

		// In skeletal case, collision enable/disable/movement should be overriden by mesh component
		// being in the physics asset, and not having collision is a waste and it can cause a bug where disconnected bodies
		if(Owner)
		{
			if(Owner->GetActorEnableCollision())	//we only override if actor has collision enabled
			{
				ShapeData.CollisionEnabled = SkelMeshComp->BodyInstance.CollisionEnabled;
			}
			else
			{	//actor has collision disabled so disable regardless of component setting
				ShapeData.CollisionEnabled = ECollisionEnabled::NoCollision;
			}
		}


		ObjectType = SkelMeshComp->GetCollisionObjectType();
		bUseCollisionEnabledOverride = true;

		if(BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Enabled)
		{
			UseResponse.SetAllChannels(ECR_Block);
		}
		else if(BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
		{
			UseResponse.SetAllChannels(ECR_Ignore);
		}

		UseResponse = FCollisionResponseContainer::CreateMinContainer(UseResponse, SkelMeshComp->BodyInstance.CollisionResponses.GetResponseContainer());
		bUseNotifyRBCollision = bUseNotifyRBCollision && SkelMeshComp->BodyInstance.bNotifyRigidBodyCollision;
		bResponseOverride = true;
		bNotifyOverride = true;
	}

#if WITH_EDITOR
	// if no collision, but if world wants to enable trace collision for components, allow it
	if((ShapeData.CollisionEnabled == ECollisionEnabled::NoCollision) && Owner && (Owner->IsA(AVolume::StaticClass()) == false))
	{
		UWorld* World = Owner->GetWorld();
		UPrimitiveComponent* PrimComp = OwnerComponentInst;
		if(World && World->bEnableTraceCollision &&
		   (PrimComp->IsA(UStaticMeshComponent::StaticClass()) || PrimComp->IsA(USkeletalMeshComponent::StaticClass()) || PrimComp->IsA(UBrushComponent::StaticClass())))
		{
			//UE_LOG(LogPhysics, Warning, TEXT("Enabling collision %s : %s"), *GetNameSafe(Owner), *GetNameSafe(OwnerComponent.Get()));
			// clear all other channel just in case other people using those channels to do something
			UseResponse.SetAllChannels(ECR_Ignore);
			ShapeData.CollisionEnabled = ECollisionEnabled::QueryOnly;
			bResponseOverride = true;
			bUseCollisionEnabledOverride = true;
		}
	}
#endif

	const bool bUseComplexAsSimple = !bForceSimpleAsComplex && (BodySetup.Get()->CollisionTraceFlag == CTF_UseComplexAsSimple);
	const bool bUseSimpleAsComplex = bForceSimpleAsComplex || (BodySetup.Get()->CollisionTraceFlag == CTF_UseSimpleAsComplex);

	if(GetPxRigidActor_AssumesLocked())
	{
		if(ShapeData.CollisionEnabled != ECollisionEnabled::NoCollision)
		{
			bool * bNotifyOverridePtr = bNotifyOverride ? &bUseNotifyRBCollision : NULL;

			PxFilterData PSimFilterData;
			PxFilterData PSimpleQueryData;
			PxFilterData PComplexQueryData;
			int32 CompID = (OwnerComponentInst != nullptr) ? OwnerComponentInst->GetUniqueID() : 0;
			CreateShapeFilterData(ObjectType, CompID, UseResponse, SkelMeshCompID, InstanceBodyIndex, PSimpleQueryData, PSimFilterData, bUseCCD && !bPhysicsStatic, bUseNotifyRBCollision, bPhysicsStatic);	//CCD is determined by root body in case of welding
			PComplexQueryData = PSimpleQueryData;
			
			// Set output sim data
			ShapeData.FilterData.SimFilter = PSimFilterData;

			// Build filterdata variations for complex and simple
			PSimpleQueryData.word3 |= EPDF_SimpleCollision;
			if(bUseSimpleAsComplex)
			{
				PSimpleQueryData.word3 |= EPDF_ComplexCollision;
			}

			PComplexQueryData.word3 |= EPDF_ComplexCollision;
			if(bUseComplexAsSimple)
			{
				PComplexQueryData.word3 |= EPDF_SimpleCollision;
			}

			ShapeData.FilterData.QuerySimpleFilter = PSimpleQueryData;
			ShapeData.FilterData.QueryComplexFilter = PComplexQueryData;
		}
	}
}

void FBodyInstance::InitStaticBodies(TArray<FBodyInstance*>& Bodies, TArray<FTransform>& Transforms, class UBodySetup* BodySetup, class UPrimitiveComponent* PrimitiveComp, class FPhysScene* InRBScene, UPhysicsSerializer* PhysicsSerializer)
{
	SCOPE_CYCLE_COUNTER(STAT_StaticInitBodies);

	check(BodySetup);
	check(InRBScene);
	check(Bodies.Num() > 0);

	FInitBodiesHelper<true> InitBodiesHelper(Bodies, Transforms, BodySetup, PrimitiveComp, InRBScene, nullptr, PhysicsSerializer);
	InitBodiesHelper.InitBodies();
}

void FBodyInstance::SetShapeFlags_AssumesLocked(TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled, PxShape* PInShape, EPhysicsSceneType SceneType, const bool bUseComplexAsSimple)
{
	ExecuteOnPxShapeWrite(this, PInShape, [&](PxShape* PShape)
	{
		// If query collision is enabled..
		bool bUpdateMassProperties = false;
		if (UseCollisionEnabled != ECollisionEnabled::NoCollision)
		{
			UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
			AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;
			const bool bPhysicsStatic = !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry();

			// Only perform scene queries in the synchronous scene for static shapes
			if (bPhysicsStatic)
			{
				PShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, SceneType == PST_Sync);
			}
			// If non-static, always enable scene queries
			else
			{
				PShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
			}

			// See if we want physics collision
			bool bSimCollision = (UseCollisionEnabled == ECollisionEnabled::QueryAndPhysics);

			// Triangle mesh is 'complex' geom
			if (PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
			{
				// on dynamic objects and objects which don't use complex as simple, tri mesh not used for sim
				if (!bSimCollision || !bUseComplexAsSimple)
				{
					PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
				}
				else
				{
					PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
				}

				if (OwnerComponentInst == NULL || !OwnerComponentInst->IsA(UModelComponent::StaticClass()))
				{
					PShape->setFlag(PxShapeFlag::eVISUALIZATION, false); // dont draw the tri mesh, we can see it anyway, and its slow
				}
			}
			// Everything else is 'simple'
			else
			{
				// See if we currently have sim collision
				bool bCurrentSimCollision = (PShape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE);
				// Enable sim collision
				if (bSimCollision && !bCurrentSimCollision)
				{
					bUpdateMassProperties = true;
					PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
				}
				// Disable sim collision
				else if (!bSimCollision && bCurrentSimCollision)
				{
					bUpdateMassProperties = true;
					PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
				}

				// enable swept bounds for CCD for this shape
				PxRigidBody* PBody = GetPxRigidActor_AssumesLocked()->is<PxRigidBody>();
				if (bSimCollision && !bPhysicsStatic && bUseCCD && PBody)
				{
					PBody->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
				}
				else if (PBody)
				{

					PBody->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, false);
				}
			}
		}
		// No collision enabled
		else
		{
			PShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			PShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
		}

		if (bUpdateMassProperties)
		{
			UpdateMassProperties();
		}
	});
}

void FBodyInstance::GetShapeFlags_AssumesLocked(FShapeData& ShapeData, TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled, const bool bUseComplexAsSimple /*= false*/)
{
	// If query collision is enabled..
	ShapeData.SyncShapeFlags = PxShapeFlags(0);
	ShapeData.AsyncShapeFlags = PxShapeFlags(0);
	ShapeData.SimpleShapeFlags = PxShapeFlags(0);
	ShapeData.ComplexShapeFlags = PxShapeFlags(0);
	ShapeData.SyncBodyFlags = PxRigidBodyFlags(0);

	// Default flags
	ShapeData.SyncShapeFlags |= PxShapeFlag::eSCENE_QUERY_SHAPE;
	ShapeData.AsyncShapeFlags |= PxShapeFlag::eSCENE_QUERY_SHAPE;
	ShapeData.SimpleShapeFlags |= PxShapeFlag::eVISUALIZATION;
	ShapeData.ComplexShapeFlags |= PxShapeFlag::eVISUALIZATION;

	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;

	if(ShapeData.CollisionEnabled != ECollisionEnabled::NoCollision)
	{
		const bool bPhysicsStatic = !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry();

		// Only perform scene queries in the synchronous scene for static shapes
		if(bPhysicsStatic)
		{
			ShapeData.AsyncShapeFlags.clear(PxShapeFlag::eSCENE_QUERY_SHAPE);
		}

		// See if we want physics collision
		bool bSimCollision = (UseCollisionEnabled == ECollisionEnabled::QueryAndPhysics);

		// on dynamic objects and objects which don't use complex as simple, tri mesh not used for sim
		if(bSimCollision && bUseComplexAsSimple)
		{
			ShapeData.ComplexShapeFlags |= PxShapeFlag::eSIMULATION_SHAPE;
		}
		
		if(OwnerComponentInst == NULL || !OwnerComponentInst->IsA(UModelComponent::StaticClass()))
		{
			ShapeData.ComplexShapeFlags.clear(PxShapeFlag::eVISUALIZATION);
		}

		// Enable sim collision
		if(bSimCollision)
		{
			ShapeData.SimpleShapeFlags |= PxShapeFlag::eSIMULATION_SHAPE;
		}

		// enable swept bounds for CCD for this shape
		PxRigidBody* PBody = GetPxRigidActor_AssumesLocked()->is<PxRigidBody>();
		if(bSimCollision && !bPhysicsStatic && bUseCCD && PBody)
		{
			ShapeData.SyncBodyFlags |= PxRigidBodyFlag::eENABLE_CCD;
		}
	}
	// No collision enabled
	else
	{
		ShapeData.SyncShapeFlags.clear(PxShapeFlag::eSCENE_QUERY_SHAPE);
		ShapeData.SyncShapeFlags.clear(PxShapeFlag::eSIMULATION_SHAPE);
		ShapeData.AsyncShapeFlags.clear(PxShapeFlag::eSCENE_QUERY_SHAPE);
		ShapeData.AsyncShapeFlags.clear(PxShapeFlag::eSIMULATION_SHAPE);
	}
}

#endif

////////////////////////////////////////////////////////////////////////////
// FBodyInstanceEditorHelpers

#if WITH_EDITOR

void FBodyInstanceEditorHelpers::EnsureConsistentMobilitySimulationSettingsOnPostEditChange(UPrimitiveComponent* Component, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (UProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		// Automatically change collision profile based on mobility and physics settings (if it is currently one of the default profiles)
		const bool bMobilityChanged = PropertyName == GET_MEMBER_NAME_CHECKED(USceneComponent, Mobility);
		const bool bSimulatePhysicsChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FBodyInstance, bSimulatePhysics);

		if (bMobilityChanged || bSimulatePhysicsChanged)
		{
			// If we enabled physics simulation, but we are not marked movable, do that for them
			if (bSimulatePhysicsChanged && Component->BodyInstance.bSimulatePhysics && (Component->Mobility != EComponentMobility::Movable))
			{
				Component->SetMobility(EComponentMobility::Movable);
			}
			// If we made the component no longer movable, but simulation was enabled, disable that for them
			else if (bMobilityChanged && (Component->Mobility != EComponentMobility::Movable) && Component->BodyInstance.bSimulatePhysics)
			{
				Component->BodyInstance.bSimulatePhysics = false;
			}

			// If the collision profile is one of the 'default' ones for a StaticMeshActor, make sure it is the correct one
			// If user has changed it to something else, don't touch it
			const FName CurrentProfileName = Component->BodyInstance.GetCollisionProfileName();
			if ((CurrentProfileName == UCollisionProfile::BlockAll_ProfileName) ||
				(CurrentProfileName == UCollisionProfile::BlockAllDynamic_ProfileName) ||
				(CurrentProfileName == UCollisionProfile::PhysicsActor_ProfileName))
			{
				if (Component->Mobility == EComponentMobility::Movable)
				{
					if (Component->BodyInstance.bSimulatePhysics)
					{
						Component->BodyInstance.SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
					}
					else
					{
						Component->BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
					}
				}
				else
				{
					Component->BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				}
			}
		}
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
